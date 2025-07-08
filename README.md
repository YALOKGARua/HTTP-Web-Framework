# HTTP Web Framework

[![Release](https://img.shields.io/github/v/release/YALOKGARua/HTTP-Web-Framework)](https://github.com/YALOKGARua/HTTP-Web-Framework/releases)
[![CI](https://github.com/YALOKGARua/HTTP-Web-Framework/workflows/Continuous%20Integration/badge.svg)](https://github.com/YALOKGARua/HTTP-Web-Framework/actions)
[![License](https://img.shields.io/github/license/YALOKGARua/HTTP-Web-Framework)](LICENSE)
[![Platform](https://img.shields.io/badge/platform-Windows%20%7C%20Linux%20%7C%20macOS-blue)](https://github.com/YALOKGARua/HTTP-Web-Framework)

Современный высокопроизводительный веб-фреймворк на C++20 для создания масштабируемых веб-приложений и микросервисов.

## 🚀 Возможности

### Основные функции
- **Современный C++20**: Использует новейшие возможности C++ включая concepts, coroutines, ranges
- **Высокая производительность**: Асинхронный I/O с пользовательским пулом потоков
- **WebSocket поддержка**: Полнодуплексная связь по протоколу WebSocket
- **GraphQL**: Встроенная поддержка GraphQL схем и запросов
- **gRPC**: Интеграция с gRPC для микросервисной архитектуры
- **Event Sourcing**: Паттерн хранения событий для надежности данных

### Архитектурные паттерны
- **Circuit Breaker**: Паттерн отказоустойчивости
- **Middleware System**: Гибкая система промежуточного ПО
- **Dependency Injection**: Встроенный контейнер зависимостей
- **Command/Query Separation**: CQRS паттерн
- **Real-time Streaming**: Server-Sent Events (SSE)

### Производительность и надежность
- **Распределенный трейсинг**: Мониторинг и отладка микросервисов
- **Сжатие**: Встроенная поддержка gzip/deflate
- **SSL/TLS**: Безопасные соединения с OpenSSL
- **Rate Limiting**: Ограничение скорости запросов
- **Connection Pooling**: Пулы соединений для БД

## 📦 Установка

### Релизы
Скачайте готовые бинарные файлы:
- [Windows x64](https://github.com/YALOKGARua/HTTP-Web-Framework/releases/latest/download/http-framework-windows.zip)
- [Linux x64](https://github.com/YALOKGARua/HTTP-Web-Framework/releases/latest/download/http-framework-linux.tar.gz)
- [macOS x64](https://github.com/YALOKGARua/HTTP-Web-Framework/releases/latest/download/http-framework-macos.tar.gz)

### Системные требования
- C++20 совместимый компилятор (GCC 10+, Clang 10+, MSVC 2019+)
- CMake 3.16+
- OpenSSL (для HTTPS)

### Сборка из исходников

```bash
git clone https://github.com/YALOKGARua/HTTP-Web-Framework.git
cd HTTP-Web-Framework
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --parallel
```

### Docker

```bash
docker build -t http-framework .
docker run -p 8080:8080 http-framework
```

## 🎯 Быстрый старт

```cpp
#include "http_framework.hpp"

using namespace http_framework;

int main() {
    core::Server::Config config;
    config.host = "0.0.0.0";
    config.port = 8080;
    config.thread_pool_size = std::thread::hardware_concurrency();
    
    Server server(config);
    auto router = std::make_shared<Router>();
    
    router->get("/", [](const Request& req, Response& res) {
        res.set_json_body(R"({"message": "Hello, World!"})");
    });
    
    router->get("/api/users/{id}", [](const Request& req, Response& res) {
        auto id = req.get_path_param("id");
        res.set_json_body("{\"user_id\": " + *id + "}");
    });
    
    router->post("/api/users", [](const Request& req, Response& res) {
        auto body = req.body_as_string();
        res.set_status_code(201);
        res.set_json_body("{\"message\": \"User created\"}");
    });
    
    server.use_router(router);
    server.start();
    
    return 0;
}
```

## 📚 Документация

### Основные компоненты
- **Server**: Главный HTTP сервер
- **Router**: Маршрутизация URL и управление обработчиками
- **Request**: Представление HTTP запроса
- **Response**: Построитель HTTP ответа
- **Middleware**: Конвейер обработки запросов/ответов

### Продвинутые возможности

#### Асинхронное программирование
```cpp
async::Task<void> async_handler(const Request& req, Response& res) {
    auto result = co_await database.query("SELECT * FROM users");
    res.set_json_body(result.to_json());
}
```

#### WebSocket соединения
```cpp
router->websocket("/chat", [](websocket::Connection& conn) {
    conn.on_message([&](const string& message) {
        conn.broadcast(message);
    });
});
```

#### GraphQL API
```cpp
auto schema = graphql::Schema::builder()
    .type("User", {
        {"id", graphql::ID},
        {"name", graphql::String},
        {"email", graphql::String}
    })
    .query("users", [](const auto& args) {
        return user_service.get_all();
    })
    .build();

server.use_graphql("/graphql", schema);
```

## 🧪 Тестирование

```bash
cd build
ctest --output-on-failure
```

## 🔧 Создание релизов

### Автоматически через GitHub Actions
Создайте тег для автоматического релиза:

```bash
git tag v1.0.0
git push origin v1.0.0
```

### Локально через скрипт
```bash
# Windows
.\scripts\create_release.ps1 -Version "1.0.0" -Message "Initial release"

# Linux/macOS
./scripts/create_release.sh -v "1.0.0" -m "Initial release"
```

## 📈 Мониторинг и трейсинг

Фреймворк включает встроенную поддержку распределенного трейсинга:

```cpp
auto tracer = tracing::Tracer::create("my-service");
auto span = tracer->start_span("database_query");
span->set_attribute("query", "SELECT * FROM users");
```

## 🌟 Примеры

- [`examples/modern_microservice.cpp`](examples/modern_microservice.cpp) - Полнофункциональный микросервис
- GraphQL + gRPC интеграция
- Event Sourcing архитектура
- Circuit Breaker паттерн

## 🤝 Вклад в проект

1. Форкните репозиторий
2. Создайте ветку (`git checkout -b feature/amazing-feature`)
3. Зафиксируйте изменения (`git commit -m 'Add amazing feature'`)
4. Отправьте в ветку (`git push origin feature/amazing-feature`)
5. Откройте Pull Request

## 📄 Лицензия

Проект лицензирован под MIT License - см. файл [LICENSE](LICENSE).

## 🏆 Статус сборки

- **Main**: [![CI](https://github.com/YALOKGARua/HTTP-Web-Framework/workflows/Continuous%20Integration/badge.svg?branch=main)](https://github.com/YALOKGARua/HTTP-Web-Framework/actions)
- **Releases**: [![Release Build](https://github.com/YALOKGARua/HTTP-Web-Framework/workflows/Release%20Build/badge.svg)](https://github.com/YALOKGARua/HTTP-Web-Framework/actions) 