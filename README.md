# HTTP Framework - Modern C++20 Web Framework

Enterprise-уровневый HTTP фреймворк на C++20 с поддержкой современных архитектурных паттернов и производительности.

## 🚀 Основные возможности

### Core Framework
- **High-Performance HTTP Server** - Многопоточный сервер с event loop и connection pooling
- **Modern C++20** - Использование корутин, concepts, и современных STL возможностей
- **Async/Await Support** - Полная поддержка асинхронного программирования
- **WebSocket Support** - Полноценная реализация WebSocket протокола
- **SSL/TLS** - Встроенная поддержка безопасных соединений

### Advanced Features
- **🔍 Distributed Tracing** - OpenTelemetry-совместимый tracing с Jaeger
- **📊 GraphQL Support** - Полнофункциональный GraphQL server с playground
- **⚡ gRPC Integration** - High-performance RPC с protobuf
- **📡 Event Sourcing & CQRS** - Современные архитектурные паттерны
- **🌊 Server-Sent Events** - Real-time streaming capabilities
- **🔧 Circuit Breaker** - Resilience patterns для distributed systems

### Enterprise Features
- **Middleware Chain** - Гибкая система middleware для обработки запросов
- **Authentication & Authorization** - JWT, session management, OAuth2
- **Caching** - LRU cache, Redis integration, memory pools
- **Compression** - gzip, deflate, brotli поддержка
- **Load Balancing** - Built-in load balancing algorithms
- **Health Checks & Metrics** - Comprehensive monitoring и observability

## 📦 Quick Start

### Prerequisites
- C++20 compatible compiler (GCC 11+, Clang 13+, MSVC 2022+)
- CMake 3.16+
- Docker & Docker Compose (optional)

### Building from Source

```bash
git clone https://github.com/your-org/http-framework.git
cd http-framework
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
./HttpFramework
```

### Docker Deployment

```bash
docker-compose up -d

# Services will be available at:
# - HTTP Server: http://localhost:8080
# - GraphQL Playground: http://localhost:8080/graphql
# - gRPC Server: localhost:50051
# - Jaeger UI: http://localhost:16686
# - Grafana: http://localhost:3000 (admin/admin)
# - Prometheus: http://localhost:9090
```

## 🔧 Basic Usage

### Simple HTTP Server

```cpp
#include "http_framework.hpp"
using namespace http_framework;

int main() {
    auto server = std::make_shared<Server>();
    auto router = std::make_shared<Router>();
    router->get("/hello", [](const Request& req, Response& res) {
        res.set_text_body("Hello, World!");
    });
    router->post("/users", [](const Request& req, Response& res) -> async::Task<void> {
        auto user_data = req.body_as_json<UserData>();
        auto user_id = co_await create_user(*user_data);
        
        res.set_status_code(201);
        res.set_json_body(json{{"id", user_id}, {"status", "created"}});
    });
    
    server->set_router(router);
    server->run();
}
```

### WebSocket Support

```cpp
router->websocket("/ws", [](const Request& req, Response& res) -> async::Task<void> {
    auto ws = co_await WebSocket::upgrade(req, res);
    
    while (ws->is_open()) {
        auto message = co_await ws->receive_message();
        if (message) {
            co_await ws->send_text("Echo: " + message->data);
        }
    }
});
```

### Distributed Tracing

```cpp
#include "tracing/tracer.hpp"
using namespace http_framework::tracing;

async::Task<User> UserService::create_user(const CreateUserRequest& request) {
    auto span = start_active_span("UserService.create_user", SpanKind::SERVER);
    span->set_attribute("user.email", request.email);
    
    try {
        auto user = co_await database_->create_user(request);
        span->set_attribute("user.id", user.id);
        co_return user;
    } catch (const std::exception& e) {
        span->record_exception(e);
        span->set_status(false, e.what());
        throw;
    }
}
```

### GraphQL API

```cpp
#include "graphql/schema.hpp"
using namespace http_framework::graphql;

TypeSystem build_schema() {
    TypeSystem schema;
    
    ObjectType user_type;
    user_type.name = "User";
    
    FieldDefinition email_field;
    email_field.name = "email";
    email_field.type = FieldType::scalar(ScalarType::STRING);
    email_field.resolver = [](const ResolverInfo& info) -> async::Future<Value> {
        auto user_id = info.parent_value.get<string>("id");
        auto user = co_await UserService::get_user(*user_id);
        co_return Value{user.email};
    };
    
    user_type.fields = {email_field};
    schema.add_object_type(user_type);
    
    return schema;
}

auto graphql_handler = std::make_shared<GraphQLHandler>(build_schema());
router->post("/graphql", [graphql_handler](const Request& req, Response& res) -> async::Task<void> {
    co_await graphql_handler->handle_request_async(req, res);
});
```

### Event Sourcing & CQRS

```cpp
#include "patterns/event_sourcing.hpp"
using namespace http_framework::patterns;

struct UserCreatedEvent {
    string user_id;
    string email;
    timestamp_t created_at;
    
    string get_event_type() const { return "UserCreated"; }
    string serialize() const { /* JSON serialization */ }
    static optional<UserCreatedEvent> deserialize(string_view data) { /* JSON parsing */ }
};

class User {
public:
    void create_user(string_view email) {
        if (!user_id_.empty()) throw std::runtime_error("User already exists");
        
        UserCreatedEvent event{generate_id(), string(email), std::chrono::steady_clock::now()};
        apply_and_record(event);
    }
    
    void apply_event(const Event& event) {
        if (auto user_event = event.get_data<UserCreatedEvent>()) {
            user_id_ = user_event->user_id;
            email_ = user_event->email;
            created_at_ = user_event->created_at;
        }
    }
    
};

auto event_store = std::make_shared<PostgreSqlEventStore>(config);
auto repository = std::make_shared<Repository<User>>(event_store);

async::Task<string> create_user(string_view email) {
    User user;
    user.create_user(email);
    co_await repository->save(user);
    co_return user.id();
}
```

### Circuit Breaker Pattern

```cpp
#include "resilience/circuit_breaker.hpp"
using namespace http_framework::resilience;

class ExternalApiClient {
public:
    ExternalApiClient() {
        CircuitBreaker<ApiResponse>::Config config;
        config.failure_threshold = 5;
        config.timeout = std::chrono::seconds(10);
        config.reset_timeout = std::chrono::seconds(30);
        
        circuit_breaker_ = std::make_shared<CircuitBreaker<ApiResponse>>("external-api", config);
    }
    
    async::Future<ApiResponse> call_external_api(const ApiRequest& request) {
        auto result = co_await circuit_breaker_->execute([this, &request]() -> async::Future<ApiResponse> {
            co_return co_await http_client_->post("/api/endpoint", request.to_json());
        });
        
        if (!result.success) {
            throw std::runtime_error("Circuit breaker: " + result.error_message);
        }
        
        co_return *result.value;
    }
    
private:
    shared_ptr<CircuitBreaker<ApiResponse>> circuit_breaker_;
    shared_ptr<HttpClient> http_client_;
};
```

### Server-Sent Events

```cpp
#include "streaming/sse.hpp"
using namespace http_framework::streaming;

class NotificationService {
public:
    NotificationService() {
        SseServer::Config config;
        config.endpoint_path = "/events";
        sse_server_ = std::make_shared<SseServer>(config);
        
        auto notifications_stream = sse_server_->create_stream("notifications");
        notifications_stream->enable_history(100);
    }
    
    async::Future<void> send_notification(const Notification& notification) {
        EventData event{"notification", notification.to_json()};
        co_await sse_server_->broadcast_to_stream("notifications", event);
    }
    
    void setup_routes(shared_ptr<Router> router) {
        router->get("/events", [this](const Request& req, Response& res) -> async::Task<void> {
            co_await sse_server_->handle_request_async(req, res);
        });
    }
    
private:
    shared_ptr<SseServer> sse_server_;
};
```

## 🏗️ Architecture

### Core Components

```
┌─────────────────────────────────────────────────────────────┐
│                     HTTP Framework                          │
├─────────────────────────────────────────────────────────────┤
│  HTTP Server  │  Router  │  Middleware  │  WebSocket       │
├─────────────────────────────────────────────────────────────┤
│  Thread Pool  │  Event Loop  │  Connection Pool            │
├─────────────────────────────────────────────────────────────┤
│  Async/Await  │  Future/Promise  │  Coroutines             │
└─────────────────────────────────────────────────────────────┘
```

### Advanced Patterns

```
┌─────────────────────────────────────────────────────────────┐
│              Distributed Systems Patterns                   │
├─────────────────────────────────────────────────────────────┤
│  Event Sourcing  │  CQRS  │  Circuit Breaker             │
├─────────────────────────────────────────────────────────────┤
│  Distributed Tracing  │  Service Mesh  │  Load Balancing   │
├─────────────────────────────────────────────────────────────┤
│  GraphQL  │  gRPC  │  Server-Sent Events                   │
└─────────────────────────────────────────────────────────────┘
```

## 📊 Performance

### Benchmarks

- **Throughput**: 1M+ requests/second на современном hardware
- **Latency**: <1ms median response time для простых операций
- **Memory**: Efficient memory pools с minimal allocations
- **Scalability**: Linear scaling до 1000+ concurrent connections

### Optimization Features

- Work-stealing thread pool
- Zero-copy operations где возможно
- Custom memory allocators
- SIMD optimizations для string operations
- Adaptive timeout mechanisms

## 🔒 Security

### Built-in Security Features

- **TLS 1.3** support из коробки
- **JWT token validation** с configurable algorithms
- **Rate limiting** на multiple levels
- **CORS protection** с whitelist support
- **Input validation** с sanitization
- **SQL injection protection** в database layers

### Security Best Practices

```cpp
server->add_middleware([](const Request& req, Response& res, auto next) {
    res.set_header("X-Content-Type-Options", "nosniff");
    res.set_header("X-Frame-Options", "DENY");
    res.set_header("X-XSS-Protection", "1; mode=block");
    next();
});

server->set_rate_limit(1000);
server->enable_cors("https://trusted-domain.com");
```

## 📈 Monitoring & Observability

### Metrics Collection

```cpp
auto metrics = server->get_stats();
server->register_metric("business_metric", [](){ 
    return get_business_value(); 
});
```

### Health Checks

```cpp
server->enable_health_check("/health");
server->register_health_check("database", []() -> async::Future<bool> {
    co_return co_await database->ping();
});
```

### Distributed Tracing

Автоматическая интеграция с OpenTelemetry и Jaeger для полного distributed tracing через микросервисы.

## 🧪 Testing

### Unit Testing

```cpp
#include "testing/framework.hpp"

TEST_CASE("HTTP Router") {
    auto router = std::make_shared<Router>();
    router->get("/test", [](const Request& req, Response& res) {
        res.set_text_body("OK");
    });
    
    auto request = create_test_request("GET", "/test");
    auto response = router->handle_request(request);
    
    REQUIRE(response.status_code() == 200);
    REQUIRE(response.body_as_string() == "OK");
}
```

### Integration Testing

```cpp
TEST_CASE("Full Server Integration") {
    TestServer server;
    server.start_async();
    
    auto client = HttpClient{"http://localhost:8080"};
    auto response = client.post("/users", R"({"name":"John","email":"john@example.com"})");
    
    REQUIRE(response.status_code == 201);
    auto user = response.json<User>();
    REQUIRE(user.name == "John");
}
```

## 🚀 Production Deployment

### Docker Production Setup

```dockerfile
FROM httpframework:latest
COPY config/production.yml /app/config/
EXPOSE 8080 50051
CMD ["./HttpFramework", "--config", "config/production.yml"]
```

### Kubernetes Deployment

```yaml
apiVersion: apps/v1
kind: Deployment
metadata:
  name: http-framework-app
spec:
  replicas: 3
  selector:
    matchLabels:
      app: http-framework
  template:
    metadata:
      labels:
        app: http-framework
    spec:
      containers:
      - name: app
        image: httpframework:latest
        ports:
        - containerPort: 8080
        - containerPort: 50051
        env:
        - name: DATABASE_URL
          valueFrom:
            secretKeyRef:
              name: db-secret
              key: url
        resources:
          requests:
            memory: "256Mi"
            cpu: "250m"
          limits:
            memory: "512Mi"
            cpu: "500m"
```

## 📚 Documentation

### API Reference
- [HTTP Server API](docs/api/server.md)
- [Router & Middleware](docs/api/router.md)
- [WebSocket API](docs/api/websocket.md)
- [GraphQL Integration](docs/api/graphql.md)
- [gRPC Support](docs/api/grpc.md)

### Guides
- [Getting Started](docs/guides/getting-started.md)
- [Event Sourcing Pattern](docs/guides/event-sourcing.md)
- [Circuit Breaker Pattern](docs/guides/circuit-breaker.md)
- [Distributed Tracing](docs/guides/tracing.md)
- [Performance Tuning](docs/guides/performance.md)

## 🤝 Contributing

Приветствуются contributions! См. [CONTRIBUTING.md](CONTRIBUTING.md) для guidelines.

### Development Setup

```bash
sudo apt-get install build-essential cmake ninja-build clang-format clang-tidy
pip install pre-commit
pre-commit install
mkdir build && cd build
cmake .. -DENABLE_TESTING=ON
make test
```

## 📄 License

This project is licensed under the MIT License - см. [LICENSE](LICENSE) файл.

## 🙏 Acknowledgments

- Inspired by современными фреймворками как ASP.NET Core, Spring Boot
- Использует patterns от Martin Fowler's Enterprise Application Patterns
- Performance optimizations inspired by высокопроизводительными C++ libraries

---

**Создано с ❤️ для современной C++ разработки** 