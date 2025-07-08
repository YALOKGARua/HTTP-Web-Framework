#include "http_framework.hpp"
#include "tracing/tracer.hpp"
#include "graphql/schema.hpp"
#include "grpc/server.hpp"
#include "patterns/event_sourcing.hpp"
#include "streaming/sse.hpp"
#include "resilience/circuit_breaker.hpp"

using namespace http_framework;
using namespace http_framework::tracing;
using namespace http_framework::graphql;
using namespace http_framework::grpc;
using namespace http_framework::patterns;
using namespace http_framework::streaming;
using namespace http_framework::resilience;

struct UserCreatedEvent {
    string user_id;
    string email;
    string name;
    timestamp_t created_at;
    
    string get_event_type() const { return "UserCreated"; }
    string serialize() const {
        return R"({"user_id":")" + user_id + R"(","email":")" + email + 
               R"(","name":")" + name + R"("})";
    }
    
    static optional<UserCreatedEvent> deserialize(string_view data) {
        UserCreatedEvent event;
        return event;
    }
};

struct CreateUserCommand {
    string email;
    string name;
    
    string get_command_type() const { return "CreateUser"; }
    AggregateId aggregate_id() const { return generate_user_id(); }
    bool validate() const { return !email.empty() && !name.empty(); }
    
private:
    string generate_user_id() const {
        static size_t counter = 0;
        return "user_" + std::to_string(++counter);
    }
};

class User {
public:
    User() = default;
    explicit User(string_view user_id) : user_id_(user_id) {}
    
    AggregateId id() const { return user_id_; }
    StreamVersion version() const { return version_; }
    
    void create_user(string_view email, string_view name) {
        if (!user_id_.empty()) {
            throw std::runtime_error("User already exists");
        }
        
        UserCreatedEvent event{user_id_, string(email), string(name), std::chrono::steady_clock::now()};
        apply_and_record(event);
    }
    
    void apply_event(const Event& event) {
        if (event.event_type() == "UserCreated") {
            auto user_event = event.get_data<UserCreatedEvent>();
            if (user_event) {
                user_id_ = user_event->user_id;
                email_ = user_event->email;
                name_ = user_event->name;
                created_at_ = user_event->created_at;
                version_++;
            }
        }
    }
    
    vector<Event> get_uncommitted_events() const { return uncommitted_events_; }
    void mark_events_as_committed() { uncommitted_events_.clear(); }
    
    const string& email() const { return email_; }
    const string& name() const { return name_; }
    timestamp_t created_at() const { return created_at_; }
    
private:
    string user_id_;
    string email_;
    string name_;
    timestamp_t created_at_;
    StreamVersion version_{0};
    vector<Event> uncommitted_events_;
    
    template<typename T>
    void apply_and_record(const T& event_data) {
        EventMetadata metadata{
            generate_event_id(),
            event_data.get_event_type(),
            "user-" + user_id_,
            version_ + 1
        };
        
        Event event{event_data, metadata};
        apply_event(event);
        uncommitted_events_.push_back(event);
    }
    
    string generate_event_id() const {
        static size_t counter = 0;
        return "event_" + std::to_string(++counter);
    }
};

class UserService {
public:
    UserService(shared_ptr<Repository<User>> repository, 
                shared_ptr<CommandBus> command_bus,
                shared_ptr<EventBus> event_bus,
                shared_ptr<CircuitBreaker<User>> circuit_breaker)
        : repository_(std::move(repository)),
          command_bus_(std::move(command_bus)),
          event_bus_(std::move(event_bus)),
          circuit_breaker_(std::move(circuit_breaker)) {}
    
    async::Future<string> create_user(string_view email, string_view name) {
        auto span = start_active_span("UserService.create_user", SpanKind::SERVER);
        span->set_attribute("user.email", email);
        span->set_attribute("user.name", name);
        
        try {
            CreateUserCommand command{string(email), string(name)};
            
            auto result = co_await circuit_breaker_->execute([this, command]() -> async::Future<string> {
                auto command_result = co_await command_bus_->send(command);
                if (!command_result.success) {
                    throw std::runtime_error(command_result.error_message);
                }
                
                User user{command.aggregate_id()};
                user.create_user(command.email, command.name);
                
                co_await repository_->save(user);
                
                for (const auto& event : command_result.events) {
                    co_await event_bus_->publish(event);
                }
                
                co_return user.id();
            });
            
            if (!result.success) {
                span->set_status(false, result.error_message);
                throw std::runtime_error(result.error_message);
            }
            
            span->set_attribute("user.id", *result.value);
            co_return *result.value;
            
        } catch (const std::exception& e) {
            span->record_exception(e);
            throw;
        }
    }
    
    async::Future<optional<User>> get_user(string_view user_id) {
        auto span = start_active_span("UserService.get_user", SpanKind::SERVER);
        span->set_attribute("user.id", user_id);
        
        auto result = co_await circuit_breaker_->execute([this, user_id]() -> async::Future<optional<User>> {
            co_return co_await repository_->get_by_id(string(user_id));
        });
        
        if (!result.success) {
            span->set_status(false, result.error_message);
            co_return std::nullopt;
        }
        
        co_return *result.value;
    }
    
private:
    shared_ptr<Repository<User>> repository_;
    shared_ptr<CommandBus> command_bus_;
    shared_ptr<EventBus> event_bus_;
    shared_ptr<CircuitBreaker<User>> circuit_breaker_;
};

class ModernMicroservice {
public:
    ModernMicroservice() {
        setup_tracing();
        setup_event_sourcing();
        setup_circuit_breaker();
        setup_http_server();
        setup_graphql();
        setup_grpc_server();
        setup_sse_streaming();
    }
    
    void run() {
        auto logger = Logger::get_instance();
        logger->info("Starting modern microservice...");
        
        auto server_future = std::async(std::launch::async, [this]() {
            http_server_->run();
        });
        
        auto grpc_future = std::async(std::launch::async, [this]() {
            grpc_server_->start();
            grpc_server_->wait_for_shutdown();
        });
        
        logger->info("Microservice started successfully");
        logger->info("HTTP Server: http://localhost:8080");
        logger->info("GraphQL Playground: http://localhost:8080/graphql");
        logger->info("SSE Endpoint: http://localhost:8080/events");
        logger->info("gRPC Server: localhost:50051");
        
        server_future.wait();
        grpc_future.wait();
    }
    
private:
    shared_ptr<Server> http_server_;
    shared_ptr<Router> router_;
    shared_ptr<GrpcServer> grpc_server_;
    shared_ptr<SseServer> sse_server_;
    shared_ptr<EventStore> event_store_;
    shared_ptr<Repository<User>> user_repository_;
    shared_ptr<UserService> user_service_;
    shared_ptr<CircuitBreaker<User>> circuit_breaker_;
    
    void setup_tracing() {
        TracerProvider::Config config;
        config.service_name = "modern-microservice";
        config.service_version = "1.0.0";
        
        auto provider = std::make_unique<TracerProvider>(config);
        auto exporter = std::make_unique<OtlpSpanExporter>();
        auto processor = std::make_unique<BatchSpanProcessor>(std::move(exporter));
        
        provider->add_span_processor(std::move(processor));
        TracerProvider::set_global_provider(std::move(provider));
    }
    
    void setup_event_sourcing() {
        event_store_ = std::make_shared<InMemoryEventStore>();
        user_repository_ = std::make_shared<Repository<User>>(event_store_);
        
        auto command_bus = std::make_shared<CommandBus>();
        auto event_bus = std::make_shared<EventBus>();
        
        event_bus->subscribe<UserCreatedEvent>([this](const UserCreatedEvent& event) -> async::Task<void> {
            auto sse_event = EventData{"user.created", event.serialize()};
            co_await sse_server_->broadcast_to_all(sse_event);
        });
        
        user_service_ = std::make_shared<UserService>(
            user_repository_, command_bus, event_bus, circuit_breaker_
        );
    }
    
    void setup_circuit_breaker() {
        CircuitBreaker<User>::Config config;
        config.failure_threshold = 5;
        config.timeout = std::chrono::seconds(10);
        config.reset_timeout = std::chrono::seconds(30);
        
        circuit_breaker_ = std::make_shared<CircuitBreaker<User>>("user-service", config);
        
        circuit_breaker_->set_state_change_callback([](CircuitState old_state, CircuitState new_state) {
            auto logger = Logger::get_instance();
            logger->warn("Circuit breaker state changed: {} -> {}", 
                        static_cast<int>(old_state), static_cast<int>(new_state));
        });
    }
    
    void setup_http_server() {
        Server::Config config;
        config.port = 8080;
        config.thread_pool_size = std::thread::hardware_concurrency();
        config.enable_compression = true;
        
        http_server_ = std::make_shared<Server>(config);
        router_ = std::make_shared<Router>();
        
        router_->get("/health", [](const Request& req, Response& res) {
            res.set_json_body(R"({"status":"healthy","service":"modern-microservice"})");
        });
        
        router_->post("/users", [this](const Request& req, Response& res) -> async::Task<void> {
            try {
                auto body = req.body_as_string();
                auto email = extract_field(body, "email");
                auto name = extract_field(body, "name");
                
                auto user_id = co_await user_service_->create_user(email, name);
                
                res.set_status_code(201);
                res.set_json_body(R"({"id":")" + user_id + R"(","status":"created"})");
            } catch (const std::exception& e) {
                res.set_status_code(400);
                res.set_json_body(R"({"error":")" + string(e.what()) + R"("})");
            }
        });
        
        router_->get("/users/{id}", [this](const Request& req, Response& res) -> async::Task<void> {
            auto user_id = req.get_path_param("id");
            if (!user_id) {
                res.set_status_code(400);
                res.set_json_body(R"({"error":"Missing user ID"})");
                co_return;
            }
            
            auto user = co_await user_service_->get_user(*user_id);
            if (!user) {
                res.set_status_code(404);
                res.set_json_body(R"({"error":"User not found"})");
                co_return;
            }
            
            string json = R"({"id":")" + user->id() + R"(","email":")" + user->email() + 
                         R"(","name":")" + user->name() + R"("})";
            res.set_json_body(json);
        });
        
        router_->get("/metrics", [this](const Request& req, Response& res) {
            auto metrics = circuit_breaker_->metrics();
            res.set_json_body(metrics.to_json());
        });
        
        http_server_->set_router(router_);
        http_server_->enable_cors();
        http_server_->enable_request_logging();
    }
    
    void setup_graphql() {
        TypeSystem schema;
        
        ObjectType user_type;
        user_type.name = "User";
        user_type.description = "A user in the system";
        
        FieldDefinition id_field;
        id_field.name = "id";
        id_field.type = FieldType::scalar(ScalarType::ID);
        id_field.resolver = [](const ResolverInfo& info) -> async::Future<Value> {
            co_return Value{string("user_123")};
        };
        
        FieldDefinition email_field;
        email_field.name = "email";
        email_field.type = FieldType::scalar(ScalarType::STRING);
        email_field.resolver = [](const ResolverInfo& info) -> async::Future<Value> {
            co_return Value{string("user@example.com")};
        };
        
        user_type.fields = {id_field, email_field};
        schema.add_object_type(user_type);
        
        ObjectType query_type;
        query_type.name = "Query";
        
        FieldDefinition user_query;
        user_query.name = "user";
        user_query.type = FieldType::object("User");
        user_query.arguments = {{"id", FieldType::scalar(ScalarType::ID)}};
        user_query.resolver = [this](const ResolverInfo& info) -> async::Future<Value> {
            hash_map<string, Value> user_data;
            user_data["id"] = Value{string("user_123")};
            user_data["email"] = Value{string("user@example.com")};
            co_return Value{user_data};
        };
        
        query_type.fields = {user_query};
        schema.add_object_type(query_type);
        schema.set_query_type("Query");
        
        auto graphql_handler = std::make_shared<GraphQLHandler>(std::move(schema));
        router_->post("/graphql", [graphql_handler](const Request& req, Response& res) -> async::Task<void> {
            co_await graphql_handler->handle_request_async(req, res);
        });
        
        router_->get("/graphql", [graphql_handler](const Request& req, Response& res) {
            graphql_handler->handle_request(req, res);
        });
    }
    
    void setup_grpc_server() {
        GrpcServer::Config config;
        config.port = 50051;
        config.enable_reflection = true;
        
        grpc_server_ = std::make_shared<GrpcServer>(config);
        
        auto user_service_grpc = ServiceBuilder{"UserService"}
            .add_unary_method<CreateUserRequest, CreateUserResponse>(
                "CreateUser",
                [this](const CreateUserRequest& req, ServerContext& ctx) -> async::Future<ServerResult<CreateUserResponse>> {
                    try {
                        auto user_id = co_await user_service_->create_user(req.email(), req.name());
                        
                        CreateUserResponse response;
                        response.set_user_id(user_id);
                        response.set_success(true);
                        
                        co_return ServerResult<CreateUserResponse>::success(response);
                    } catch (const std::exception& e) {
                        co_return ServerResult<CreateUserResponse>::failure(
                            Status::internal_error(e.what())
                        );
                    }
                }
            )
            .build();
        
        grpc_server_->add_service(user_service_grpc);
    }
    
    void setup_sse_streaming() {
        SseServer::Config config;
        config.endpoint_path = "/events";
        config.enable_cors = true;
        
        sse_server_ = std::make_shared<SseServer>(config);
        
        auto notifications_stream = sse_server_->create_stream("notifications");
        notifications_stream->enable_history(50);
        
        router_->get("/events", [this](const Request& req, Response& res) -> async::Task<void> {
            co_await sse_server_->handle_request_async(req, res);
        });
        
        std::thread([this]() {
            while (true) {
                std::this_thread::sleep_for(std::chrono::seconds(30));
                
                EventData heartbeat{"heartbeat", R"({"timestamp":")" + 
                    std::to_string(std::chrono::duration_cast<std::chrono::seconds>(
                        std::chrono::steady_clock::now().time_since_epoch()).count()) + R"("})"};
                
                sse_server_->broadcast_to_all(heartbeat);
            }
        }).detach();
    }
    
    string extract_field(string_view json, string_view field) {
        size_t start = json.find("\"" + string(field) + "\"");
        if (start == string::npos) return "";
        
        start = json.find(":", start);
        if (start == string::npos) return "";
        
        start = json.find("\"", start);
        if (start == string::npos) return "";
        start++;
        
        size_t end = json.find("\"", start);
        if (end == string::npos) return "";
        
        return string(json.substr(start, end - start));
    }
};

struct CreateUserRequest {
    string email() const { return email_; }
    string name() const { return name_; }
    void set_email(const string& e) { email_ = e; }
    void set_name(const string& n) { name_ = n; }
    
    string SerializeToString() const { return email_ + ":" + name_; }
    bool ParseFromString(string_view data) {
        auto pos = data.find(':');
        if (pos != string::npos) {
            email_ = data.substr(0, pos);
            name_ = data.substr(pos + 1);
            return true;
        }
        return false;
    }
    size_t ByteSizeLong() const { return email_.size() + name_.size() + 1; }
    bool IsInitialized() const { return !email_.empty() && !name_.empty(); }
    
private:
    string email_;
    string name_;
};

struct CreateUserResponse {
    string user_id() const { return user_id_; }
    bool success() const { return success_; }
    void set_user_id(const string& id) { user_id_ = id; }
    void set_success(bool s) { success_ = s; }
    
    string SerializeToString() const { return user_id_ + ":" + (success_ ? "1" : "0"); }
    bool ParseFromString(string_view data) {
        auto pos = data.find(':');
        if (pos != string::npos) {
            user_id_ = data.substr(0, pos);
            success_ = data.substr(pos + 1) == "1";
            return true;
        }
        return false;
    }
    size_t ByteSizeLong() const { return user_id_.size() + 2; }
    bool IsInitialized() const { return !user_id_.empty(); }
    
private:
    string user_id_;
    bool success_{false};
};

int main() {
    try {
        ModernMicroservice service;
        service.run();
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
} 