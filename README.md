# <div align="center">HTTP Web Framework</div>

<div align="center">
  
  ![Logo](https://via.placeholder.com/150x150.png?text=HTTP+Framework)
  
  [![Release](https://img.shields.io/github/v/release/YALOKGARua/HTTP-Web-Framework?style=for-the-badge&logo=github)](https://github.com/YALOKGARua/HTTP-Web-Framework/releases)
  [![License](https://img.shields.io/github/license/YALOKGARua/HTTP-Web-Framework?style=for-the-badge)](LICENSE)
  [![CI](https://img.shields.io/github/actions/workflow/status/YALOKGARua/HTTP-Web-Framework/ci.yml?branch=main&style=for-the-badge&logo=github-actions&logoColor=white&label=CI)](https://github.com/YALOKGARua/HTTP-Web-Framework/actions)
  [![Platform](https://img.shields.io/badge/platform-Windows%20%7C%20Linux%20%7C%20macOS-blue?style=for-the-badge&logo=windows&logoColor=white)](https://github.com/YALOKGARua/HTTP-Web-Framework)
  
  <h3>Современный высокопроизводительный веб-фреймворк на C++20</h3>
  <h4>Создавайте масштабируемые веб-приложения и микросервисы с использованием новейших возможностей C++</h4>
  
</div>

<hr>

## 📋 Содержание

- [🚀 Возможности](#-возможности)
- [🖥️ Скриншоты](#️-скриншоты)
- [📦 Установка](#-установка)
- [🎯 Быстрый старт](#-быстрый-старт)
- [📚 Документация](#-документация)
- [🧪 Тестирование](#-тестирование)
- [🔧 Создание релизов](#-создание-релизов)
- [📈 Мониторинг и трейсинг](#-мониторинг-и-трейсинг)
- [🌟 Примеры](#-примеры)
- [🤝 Вклад в проект](#-вклад-в-проект)
- [📄 Лицензия](#-лицензия)
- [🏆 Статус сборки](#-статус-сборки)

<hr>

## 🚀 Возможности

<table>
  <tr>
    <td width="33%">
      <h3>Основные функции</h3>
      <ul>
        <li><b>Современный C++20</b>: Использует concepts, coroutines, ranges</li>
        <li><b>Высокая производительность</b>: Асинхронный I/O с пулом потоков</li>
        <li><b>WebSocket поддержка</b>: Полнодуплексная связь</li>
        <li><b>GraphQL</b>: Встроенная поддержка схем и запросов</li>
        <li><b>gRPC</b>: Интеграция для микросервисной архитектуры</li>
        <li><b>Event Sourcing</b>: Паттерн хранения событий</li>
      </ul>
    </td>
    <td width="33%">
      <h3>Архитектурные паттерны</h3>
      <ul>
        <li><b>Circuit Breaker</b>: Паттерн отказоустойчивости</li>
        <li><b>Middleware System</b>: Гибкая система промежуточного ПО</li>
        <li><b>Dependency Injection</b>: Встроенный контейнер зависимостей</li>
        <li><b>Command/Query Separation</b>: CQRS паттерн</li>
        <li><b>Real-time Streaming</b>: Server-Sent Events (SSE)</li>
      </ul>
    </td>
    <td width="33%">
      <h3>Производительность и надежность</h3>
      <ul>
        <li><b>Распределенный трейсинг</b>: Мониторинг микросервисов</li>
        <li><b>Сжатие</b>: Встроенная поддержка gzip/deflate</li>
        <li><b>SSL/TLS</b>: Безопасные соединения с OpenSSL</li>
        <li><b>Rate Limiting</b>: Ограничение скорости запросов</li>
        <li><b>Connection Pooling</b>: Пулы соединений для БД</li>
      </ul>
    </td>
  </tr>
</table>

<hr>

## 🖥️ Скриншоты

<div align="center">
  <img src="https://via.placeholder.com/800x400.png?text=HTTP+Framework+Demo" alt="Demo Screenshot" width="80%">
</div>

<div align="center">
  <table>
    <tr>
      <td><img src="https://via.placeholder.com/400x200.png?text=WebSocket+Demo" alt="WebSocket Demo"></td>
      <td><img src="https://via.placeholder.com/400x200.png?text=GraphQL+Demo" alt="GraphQL Demo"></td>
    </tr>
    <tr>
      <td><img src="https://via.placeholder.com/400x200.png?text=Middleware+System" alt="Middleware System"></td>
      <td><img src="https://via.placeholder.com/400x200.png?text=Performance+Metrics" alt="Performance Metrics"></td>
    </tr>
  </table>
</div>

<hr>

## 📦 Установка

### Системные требования

<table>
  <tr>
    <td><b>Компилятор:</b></td>
    <td>C++20 совместимый (GCC 10+, Clang 10+, MSVC 2019+)</td>
  </tr>
  <tr>
    <td><b>Сборка:</b></td>
    <td>CMake 3.16+</td>
  </tr>
  <tr>
    <td><b>Безопасность:</b></td>
    <td>OpenSSL (для HTTPS)</td>
  </tr>
</table>

### Готовые бинарные файлы

<div align="center">
  <table>
    <tr>
      <th>Платформа</th>
      <th>Скачать</th>
      <th>Статус</th>
    </tr>
    <tr>
      <td>
        <img src="https://img.shields.io/badge/Windows-0078D6?style=for-the-badge&logo=windows&logoColor=white" alt="Windows">
      </td>
      <td>
        <a href="https://github.com/YALOKGARua/HTTP-Web-Framework/releases/latest/download/http-framework-windows-v1.0.4.zip">
          <img src="https://img.shields.io/badge/Скачать_EXE-5cb85c?style=for-the-badge&logo=windows&logoColor=white" alt="Download Windows">
        </a>
      </td>
      <td>✅ Исполняемый файл</td>
    </tr>
    <tr>
      <td>
        <img src="https://img.shields.io/badge/Linux-FCC624?style=for-the-badge&logo=linux&logoColor=black" alt="Linux">
      </td>
      <td>
        <a href="https://github.com/YALOKGARua/HTTP-Web-Framework/releases/latest/download/http-framework-linux-v1.0.4.tar.gz">
          <img src="https://img.shields.io/badge/Скачать_TAR-5cb85c?style=for-the-badge&logo=linux&logoColor=black" alt="Download Linux">
        </a>
      </td>
      <td>⚠️ Заглушка</td>
    </tr>
    <tr>
      <td>
        <img src="https://img.shields.io/badge/macOS-000000?style=for-the-badge&logo=apple&logoColor=white" alt="macOS">
      </td>
      <td>
        <a href="https://github.com/YALOKGARua/HTTP-Web-Framework/releases/latest/download/http-framework-macos-v1.0.4.tar.gz">
          <img src="https://img.shields.io/badge/Скачать_TAR-5cb85c?style=for-the-badge&logo=apple&logoColor=white" alt="Download macOS">
        </a>
      </td>
      <td>⚠️ Заглушка</td>
    </tr>
  </table>
</div>

### Демонстрационный сервер

<div align="center">
  <img src="https://via.placeholder.com/600x300.png?text=Demo+Server+Screenshot" alt="Demo Server Screenshot" width="60%">
</div>

Для Windows доступен готовый демонстрационный сервер:

1. Распакуйте архив
2. Запустите `http-framework.exe`
3. Выберите опцию "1" для запуска сервера или "2" для просмотра информации о версии

### Сборка из исходников

```bash
# Клонирование репозитория
git clone https://github.com/YALOKGARua/HTTP-Web-Framework.git
cd HTTP-Web-Framework

# Сборка основной библиотеки
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --parallel

# Сборка демонстрационного сервера
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release --target http_framework_demo
```

### Docker

```bash
# Сборка образа
docker build -t http-framework .

# Запуск контейнера
docker run -p 8080:8080 http-framework
```

<hr>

## 🎯 Быстрый старт

<div align="center">
  <img src="https://via.placeholder.com/800x200.png?text=Quick+Start+Diagram" alt="Quick Start Diagram" width="80%">
</div>

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
    
    // Простой GET-запрос
    router->get("/", [](const Request& req, Response& res) {
        res.set_json_body(R"({"message": "Hello, World!"})");
    });
    
    // Запрос с параметрами пути
    router->get("/api/users/{id}", [](const Request& req, Response& res) {
        auto id = req.get_path_param("id");
        res.set_json_body("{\"user_id\": " + *id + "}");
    });
    
    // POST-запрос
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

<hr>

## 📚 Документация

### Основные компоненты

<table>
  <tr>
    <td width="20%"><b>Server</b></td>
    <td>Главный HTTP сервер, обрабатывающий входящие соединения</td>
  </tr>
  <tr>
    <td><b>Router</b></td>
    <td>Маршрутизация URL и управление обработчиками запросов</td>
  </tr>
  <tr>
    <td><b>Request</b></td>
    <td>Представление HTTP запроса с доступом к заголовкам, параметрам и телу</td>
  </tr>
  <tr>
    <td><b>Response</b></td>
    <td>Построитель HTTP ответа с поддержкой различных форматов данных</td>
  </tr>
  <tr>
    <td><b>Middleware</b></td>
    <td>Конвейер обработки запросов/ответов для расширения функциональности</td>
  </tr>
</table>

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

<hr>

## 🧪 Тестирование

```bash
# Запуск всех тестов
cd build
ctest --output-on-failure

# Запуск конкретной группы тестов
ctest -R "router_tests" --output-on-failure
```

<hr>

## 🔧 Создание релизов

### Автоматически через GitHub Actions

```bash
# Создание тега для автоматического релиза
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

<hr>

## 📈 Мониторинг и трейсинг

<div align="center">
  <img src="https://via.placeholder.com/800x300.png?text=Distributed+Tracing+Visualization" alt="Tracing Visualization" width="80%">
</div>

Фреймворк включает встроенную поддержку распределенного трейсинга:

```cpp
auto tracer = tracing::Tracer::create("my-service");
auto span = tracer->start_span("database_query");
span->set_attribute("query", "SELECT * FROM users");
```

<hr>

## 🌟 Примеры

<table>
  <tr>
    <td width="50%">
      <h3>Микросервис</h3>
      <a href="examples/modern_microservice.cpp">examples/modern_microservice.cpp</a>
      <p>Полнофункциональный микросервис с поддержкой REST API, аутентификации и базы данных</p>
    </td>
    <td width="50%">
      <h3>GraphQL + gRPC</h3>
      <p>Интеграция GraphQL и gRPC для создания эффективных API с сильной типизацией</p>
    </td>
  </tr>
  <tr>
    <td>
      <h3>Event Sourcing</h3>
      <p>Архитектура на основе событий для надежного хранения и обработки данных</p>
    </td>
    <td>
      <h3>Circuit Breaker</h3>
      <p>Паттерн отказоустойчивости для предотвращения каскадных сбоев в распределенных системах</p>
    </td>
  </tr>
</table>

<hr>

## 🤝 Вклад в проект

<div align="center">
  <img src="https://via.placeholder.com/800x200.png?text=Contribution+Workflow" alt="Contribution Workflow" width="80%">
</div>

1. Форкните репозиторий
2. Создайте ветку (`git checkout -b feature/amazing-feature`)
3. Зафиксируйте изменения (`git commit -m 'Add amazing feature'`)
4. Отправьте в ветку (`git push origin feature/amazing-feature`)
5. Откройте Pull Request

<hr>

## 📄 Лицензия

Проект лицензирован под [MIT License](LICENSE).

<hr>

## 🏆 Статус сборки

<table>
  <tr>
    <td><b>Main</b></td>
    <td>
      <a href="https://github.com/YALOKGARua/HTTP-Web-Framework/actions">
        <img src="https://github.com/YALOKGARua/HTTP-Web-Framework/workflows/Continuous%20Integration/badge.svg?branch=main" alt="CI Status">
      </a>
    </td>
  </tr>
  <tr>
    <td><b>Releases</b></td>
    <td>
      <a href="https://github.com/YALOKGARua/HTTP-Web-Framework/actions">
        <img src="https://github.com/YALOKGARua/HTTP-Web-Framework/workflows/Release%20Build/badge.svg" alt="Release Status">
      </a>
    </td>
  </tr>
</table>

<hr>

<div align="center">
  <p>Создано с ❤️ командой HTTP Web Framework</p>
  <p>
    <a href="https://github.com/YALOKGARua">
      <img src="https://img.shields.io/badge/GitHub-YALOKGARua-181717?style=for-the-badge&logo=github" alt="GitHub">
    </a>
  </p>
</div> 