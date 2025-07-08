#pragma once

#include "core/types.hpp"
#include "core/thread_pool.hpp"
#include "async/future.hpp"
#include "async/task.hpp"
#include "grpc/service.hpp"
#include "grpc/context.hpp"

namespace http_framework::grpc {
    enum class StatusCode : std::uint32_t {
        OK = 0,
        CANCELLED = 1,
        UNKNOWN = 2,
        INVALID_ARGUMENT = 3,
        DEADLINE_EXCEEDED = 4,
        NOT_FOUND = 5,
        ALREADY_EXISTS = 6,
        PERMISSION_DENIED = 7,
        RESOURCE_EXHAUSTED = 8,
        FAILED_PRECONDITION = 9,
        ABORTED = 10,
        OUT_OF_RANGE = 11,
        UNIMPLEMENTED = 12,
        INTERNAL = 13,
        UNAVAILABLE = 14,
        DATA_LOSS = 15,
        UNAUTHENTICATED = 16
    };
    
    struct Status {
        StatusCode code{StatusCode::OK};
        string message;
        hash_map<string, string> details;
        
        Status() = default;
        Status(StatusCode c, string_view msg = "") : code(c), message(msg) {}
        
        bool ok() const noexcept { return code == StatusCode::OK; }
        string to_string() const;
        
        static Status ok() { return Status{StatusCode::OK}; }
        static Status cancelled(string_view message = "Cancelled") { return Status{StatusCode::CANCELLED, message}; }
        static Status invalid_argument(string_view message = "Invalid argument") { return Status{StatusCode::INVALID_ARGUMENT, message}; }
        static Status deadline_exceeded(string_view message = "Deadline exceeded") { return Status{StatusCode::DEADLINE_EXCEEDED, message}; }
        static Status not_found(string_view message = "Not found") { return Status{StatusCode::NOT_FOUND, message}; }
        static Status internal_error(string_view message = "Internal error") { return Status{StatusCode::INTERNAL, message}; }
        static Status unimplemented(string_view message = "Unimplemented") { return Status{StatusCode::UNIMPLEMENTED, message}; }
    };
    
    template<typename T>
    struct ServerResult {
        Status status;
        optional<T> value;
        
        ServerResult() = default;
        ServerResult(Status s) : status(std::move(s)) {}
        ServerResult(T val) : status(Status::ok()), value(std::move(val)) {}
        ServerResult(Status s, T val) : status(std::move(s)), value(std::move(val)) {}
        
        bool ok() const noexcept { return status.ok(); }
        const T& operator*() const { return value.value(); }
        T& operator*() { return value.value(); }
        const T* operator->() const { return &value.value(); }
        T* operator->() { return &value.value(); }
    };
    
    template<>
    struct ServerResult<void> {
        Status status;
        
        ServerResult() = default;
        ServerResult(Status s) : status(std::move(s)) {}
        
        bool ok() const noexcept { return status.ok(); }
        
        static ServerResult ok() { return ServerResult{Status::ok()}; }
    };
    
    class Metadata {
    public:
        Metadata() = default;
        Metadata(std::initializer_list<std::pair<string, string>> init);
        
        void set(string_view key, string_view value);
        void add(string_view key, string_view value);
        void remove(string_view key);
        void clear();
        
        bool has(string_view key) const;
        optional<string> get(string_view key) const;
        vector<string> get_all(string_view key) const;
        
        size_type size() const noexcept { return data_.size(); }
        bool empty() const noexcept { return data_.empty(); }
        
        auto begin() const { return data_.begin(); }
        auto end() const { return data_.end(); }
        
        string to_string() const;
        
    private:
        vector<pair<string, string>> data_;
    };
    
    class ServerContext {
    public:
        ServerContext() = default;
        ServerContext(const ServerContext&) = delete;
        ServerContext(ServerContext&&) = default;
        ServerContext& operator=(const ServerContext&) = delete;
        ServerContext& operator=(ServerContext&&) = default;
        
        const Metadata& client_metadata() const noexcept { return client_metadata_; }
        Metadata& initial_metadata() noexcept { return initial_metadata_; }
        Metadata& trailing_metadata() noexcept { return trailing_metadata_; }
        
        void cancel() { cancelled_ = true; }
        bool is_cancelled() const noexcept { return cancelled_; }
        
        void set_deadline(timestamp_t deadline) { deadline_ = deadline; }
        optional<timestamp_t> deadline() const noexcept { return deadline_; }
        bool is_deadline_exceeded() const;
        
        const string& peer() const noexcept { return peer_; }
        void set_peer(string_view p) { peer_ = p; }
        
        const string& method() const noexcept { return method_; }
        void set_method(string_view m) { method_ = m; }
        
        template<typename T>
        void set_user_data(T&& data) { user_data_ = std::forward<T>(data); }
        
        template<typename T>
        optional<T> get_user_data() const {
            if (std::holds_alternative<T>(user_data_)) {
                return std::get<T>(user_data_);
            }
            return std::nullopt;
        }
        
        void add_initial_metadata(string_view key, string_view value);
        void add_trailing_metadata(string_view key, string_view value);
        
        duration_t elapsed_time() const;
        
    private:
        Metadata client_metadata_;
        Metadata initial_metadata_;
        Metadata trailing_metadata_;
        atomic<bool> cancelled_{false};
        optional<timestamp_t> deadline_;
        string peer_;
        string method_;
        timestamp_t start_time_{std::chrono::steady_clock::now()};
        variant<std::monostate, string, int64_t, double, bool> user_data_;
        
        friend class GrpcServer;
    };
    
    template<typename Request, typename Response>
    using UnaryHandler = function<async::Future<ServerResult<Response>>(const Request&, ServerContext&)>;
    
    template<typename Request, typename Response>
    using ServerStreamingHandler = function<async::Future<Status>(const Request&, ServerContext&, function<async::Future<bool>(const Response&)>)>;
    
    template<typename Request, typename Response>
    using ClientStreamingHandler = function<async::Future<ServerResult<Response>>(function<async::Future<optional<Request>>()>, ServerContext&)>;
    
    template<typename Request, typename Response>
    using BidirectionalStreamingHandler = function<async::Future<Status>(function<async::Future<optional<Request>>()>, ServerContext&, function<async::Future<bool>(const Response&)>)>;
    
    class ServiceBuilder {
    public:
        explicit ServiceBuilder(string_view service_name);
        
        template<typename Request, typename Response>
        ServiceBuilder& add_unary_method(string_view method_name, UnaryHandler<Request, Response> handler);
        
        template<typename Request, typename Response>
        ServiceBuilder& add_server_streaming_method(string_view method_name, ServerStreamingHandler<Request, Response> handler);
        
        template<typename Request, typename Response>
        ServiceBuilder& add_client_streaming_method(string_view method_name, ClientStreamingHandler<Request, Response> handler);
        
        template<typename Request, typename Response>
        ServiceBuilder& add_bidirectional_streaming_method(string_view method_name, BidirectionalStreamingHandler<Request, Response> handler);
        
        ServiceBuilder& add_interceptor(shared_ptr<class ServerInterceptor> interceptor);
        ServiceBuilder& set_max_receive_message_size(size_type size);
        ServiceBuilder& set_max_send_message_size(size_type size);
        
        unique_ptr<class GrpcService> build();
        
    private:
        string service_name_;
        vector<unique_ptr<class MethodHandler>> methods_;
        vector<shared_ptr<class ServerInterceptor>> interceptors_;
        optional<size_type> max_receive_message_size_;
        optional<size_type> max_send_message_size_;
    };
    
    class MethodHandler {
    public:
        virtual ~MethodHandler() = default;
        virtual async::Future<void> handle(const buffer_t& request_data, ServerContext& context, function<async::Future<void>(const buffer_t&, Status)> response_callback) = 0;
        virtual const string& method_name() const = 0;
        virtual bool is_streaming() const = 0;
    };
    
    template<typename Request, typename Response>
    class UnaryMethodHandler : public MethodHandler {
    public:
        UnaryMethodHandler(string_view name, UnaryHandler<Request, Response> handler);
        
        async::Future<void> handle(const buffer_t& request_data, ServerContext& context, function<async::Future<void>(const buffer_t&, Status)> response_callback) override;
        const string& method_name() const override { return method_name_; }
        bool is_streaming() const override { return false; }
        
    private:
        string method_name_;
        UnaryHandler<Request, Response> handler_;
    };
    
    class ServerInterceptor {
    public:
        virtual ~ServerInterceptor() = default;
        virtual async::Future<Status> intercept(ServerContext& context, function<async::Future<Status>()> next) = 0;
    };
    
    class AuthenticationInterceptor : public ServerInterceptor {
    public:
        using AuthValidator = function<async::Future<bool>(string_view token, ServerContext& context)>;
        
        explicit AuthenticationInterceptor(AuthValidator validator);
        async::Future<Status> intercept(ServerContext& context, function<async::Future<Status>()> next) override;
        
    private:
        AuthValidator validator_;
    };
    
    class LoggingInterceptor : public ServerInterceptor {
    public:
        LoggingInterceptor() = default;
        async::Future<Status> intercept(ServerContext& context, function<async::Future<Status>()> next) override;
    };
    
    class RateLimitInterceptor : public ServerInterceptor {
    public:
        explicit RateLimitInterceptor(size_type requests_per_minute);
        async::Future<Status> intercept(ServerContext& context, function<async::Future<Status>()> next) override;
        
    private:
        size_type requests_per_minute_;
        hash_map<string, std::pair<size_type, timestamp_t>> rate_counters_;
        mutable mutex rate_mutex_;
    };
    
    class GrpcService {
    public:
        explicit GrpcService(string_view name);
        
        void add_method(unique_ptr<MethodHandler> method);
        void add_interceptor(shared_ptr<ServerInterceptor> interceptor);
        
        async::Future<void> handle_request(string_view method_name, const buffer_t& request_data, ServerContext& context, function<async::Future<void>(const buffer_t&, Status)> response_callback);
        
        const string& name() const noexcept { return name_; }
        bool has_method(string_view method_name) const;
        vector<string> get_method_names() const;
        
    private:
        string name_;
        hash_map<string, unique_ptr<MethodHandler>> methods_;
        vector<shared_ptr<ServerInterceptor>> interceptors_;
        
        async::Future<Status> execute_interceptors(ServerContext& context, function<async::Future<Status>()> final_handler);
    };
    
    class GrpcServer {
    public:
        struct Config {
            string host{"0.0.0.0"};
            port_t port{50051};
            size_type max_connections{1000};
            size_type thread_pool_size{std::thread::hardware_concurrency()};
            duration_t keep_alive_time{std::chrono::seconds(30)};
            duration_t keep_alive_timeout{std::chrono::seconds(5)};
            size_type max_receive_message_size{4 * 1024 * 1024};
            size_type max_send_message_size{4 * 1024 * 1024};
            bool enable_reflection{true};
            bool enable_health_check{true};
            optional<string> ssl_cert_file;
            optional<string> ssl_key_file;
            bool require_client_auth{false};
        };
        
        explicit GrpcServer(Config config = {});
        ~GrpcServer();
        
        void add_service(shared_ptr<GrpcService> service);
        void remove_service(string_view service_name);
        
        void add_global_interceptor(shared_ptr<ServerInterceptor> interceptor);
        
        bool start();
        void stop();
        void wait_for_shutdown();
        bool is_running() const noexcept { return running_; }
        
        void enable_health_service();
        void set_service_health(string_view service_name, bool healthy);
        
        void enable_reflection_service();
        void disable_reflection_service();
        
        void set_ssl_credentials(string_view cert_file, string_view key_file, bool require_client_auth = false);
        void disable_ssl();
        
        size_type active_connections() const;
        size_type total_requests() const noexcept { return total_requests_; }
        hash_map<string, size_type> get_method_stats() const;
        
        void set_error_handler(function<void(const std::exception&)> handler);
        void set_completion_queue_size(size_type size);
        
    private:
        Config config_;
        atomic<bool> running_{false};
        atomic<bool> shutdown_requested_{false};
        
        hash_map<string, shared_ptr<GrpcService>> services_;
        vector<shared_ptr<ServerInterceptor>> global_interceptors_;
        
        unique_ptr<core::ThreadPool> thread_pool_;
        unique_ptr<class GrpcTransport> transport_;
        unique_ptr<class CompletionQueue> completion_queue_;
        
        atomic<size_type> total_requests_{0};
        hash_map<string, atomic<size_type>> method_counters_;
        mutable shared_mutex services_mutex_;
        
        optional<function<void(const std::exception&)>> error_handler_;
        
        void accept_connections();
        async::Future<void> handle_connection(unique_ptr<class GrpcConnection> connection);
        async::Future<void> process_request(unique_ptr<class GrpcConnection> connection);
        
        void setup_health_service();
        void setup_reflection_service();
        void setup_ssl();
        
        void handle_error(const std::exception& e);
        void update_method_stats(string_view service_name, string_view method_name);
        
        shared_ptr<GrpcService> find_service(string_view service_name);
    };
    
    template<typename T>
    concept ProtobufMessage = requires(T& msg, const T& const_msg) {
        { msg.SerializeToString() } -> std::convertible_to<string>;
        { msg.ParseFromString(std::declval<string_view>()) } -> std::convertible_to<bool>;
        { msg.ByteSizeLong() } -> std::convertible_to<size_type>;
        { const_msg.IsInitialized() } -> std::convertible_to<bool>;
    };
    
    template<ProtobufMessage T>
    buffer_t serialize_message(const T& message) {
        string serialized = message.SerializeToString();
        return buffer_t(serialized.begin(), serialized.end());
    }
    
    template<ProtobufMessage T>
    optional<T> deserialize_message(const buffer_t& data) {
        T message;
        string str_data(data.begin(), data.end());
        if (message.ParseFromString(str_data) && message.IsInitialized()) {
            return message;
        }
        return std::nullopt;
    }
    
    template<typename Request, typename Response>
    ServiceBuilder& ServiceBuilder::add_unary_method(string_view method_name, UnaryHandler<Request, Response> handler) {
        static_assert(ProtobufMessage<Request>, "Request type must be a protobuf message");
        static_assert(ProtobufMessage<Response>, "Response type must be a protobuf message");
        
        methods_.push_back(std::make_unique<UnaryMethodHandler<Request, Response>>(method_name, std::move(handler)));
        return *this;
    }
    
    template<typename Request, typename Response>
    UnaryMethodHandler<Request, Response>::UnaryMethodHandler(string_view name, UnaryHandler<Request, Response> handler)
        : method_name_(name), handler_(std::move(handler)) {}
    
    template<typename Request, typename Response>
    async::Future<void> UnaryMethodHandler<Request, Response>::handle(const buffer_t& request_data, ServerContext& context, function<async::Future<void>(const buffer_t&, Status)> response_callback) {
        auto request = deserialize_message<Request>(request_data);
        if (!request) {
            co_await response_callback({}, Status::invalid_argument("Failed to parse request"));
            co_return;
        }
        
        try {
            auto result = co_await handler_(*request, context);
            if (result.ok()) {
                auto response_data = serialize_message(*result.value);
                co_await response_callback(response_data, Status::ok());
            } else {
                co_await response_callback({}, result.status);
            }
        } catch (const std::exception& e) {
            co_await response_callback({}, Status::internal_error(e.what()));
        }
    }
    
    class GrpcClientStub {
    public:
        struct Config {
            string endpoint;
            duration_t timeout{std::chrono::seconds(30)};
            size_type max_receive_message_size{4 * 1024 * 1024};
            size_type max_send_message_size{4 * 1024 * 1024};
            optional<string> ssl_ca_file;
            bool verify_ssl{true};
            Metadata default_metadata;
        };
        
        explicit GrpcClientStub(Config config);
        ~GrpcClientStub();
        
        template<typename Request, typename Response>
        async::Future<ServerResult<Response>> unary_call(string_view service_name, string_view method_name, const Request& request, const Metadata& metadata = {});
        
        template<typename Request, typename Response>
        class StreamingCall {
        public:
            async::Future<bool> write(const Request& request);
            async::Future<optional<Response>> read();
            async::Future<Status> finish();
            void cancel();
        };
        
        template<typename Request, typename Response>
        async::Future<unique_ptr<StreamingCall<Request, Response>>> bidirectional_streaming_call(string_view service_name, string_view method_name, const Metadata& metadata = {});
        
        bool is_connected() const;
        void disconnect();
        
    private:
        Config config_;
        unique_ptr<class GrpcChannel> channel_;
    };
} 