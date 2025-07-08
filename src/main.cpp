#include "http_framework.hpp"
#include <iostream>
#include <signal.h>

using namespace http_framework;

class UserController {
public:
    void index(const Request& req, Response& res) {
        res.set_json_body(R"({"users": [{"id": 1, "name": "John"}, {"id": 2, "name": "Jane"}]})");
    }
    
    void show(const Request& req, Response& res) {
        auto id = req.get_path_param("id");
        if (id) {
            res.set_json_body("{\"id\": " + *id + ", \"name\": \"User " + *id + "\"}");
        } else {
            res.send_error(400, "Invalid user ID");
        }
    }
    
    void create(const Request& req, Response& res) {
        auto body = req.body_as_string();
        res.set_status_code(201);
        res.set_json_body("{\"message\": \"User created\", \"data\": " + body + "}");
    }
    
    void update(const Request& req, Response& res) {
        auto id = req.get_path_param("id");
        auto body = req.body_as_string();
        if (id) {
            res.set_json_body("{\"message\": \"User " + *id + " updated\", \"data\": " + body + "}");
        } else {
            res.send_error(400, "Invalid user ID");
        }
    }
    
    void destroy(const Request& req, Response& res) {
        auto id = req.get_path_param("id");
        if (id) {
            res.set_status_code(204);
        } else {
            res.send_error(400, "Invalid user ID");
        }
    }
};

class ProductController {
public:
    void list_products(const Request& req, Response& res) {
        auto category = req.get_query_param("category");
        auto limit = req.get_query_param("limit");
        
        string response = R"({"products": [)";
        response += R"({"id": 1, "name": "Laptop", "category": "electronics"},)";
        response += R"({"id": 2, "name": "Book", "category": "education"})";
        response += "]}";
        
        res.set_json_body(response);
    }
    
    void get_product(const Request& req, Response& res) {
        auto id = req.get_path_param("id");
        if (id) {
            res.set_json_body("{\"id\": " + *id + ", \"name\": \"Product " + *id + "\"}");
        } else {
            res.send_error(404, "Product not found");
        }
    }
};

async::Task<void> async_handler(const Request& req, Response& res) {
    co_await async::sleep_for(std::chrono::milliseconds(100));
    res.set_json_body(R"({"message": "Async response", "timestamp": ")" + 
                     std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(
                         std::chrono::system_clock::now().time_since_epoch()).count()) + "\"}");
}

void websocket_handler(const Request& req, Response& res) {
    if (req.is_websocket_upgrade()) {
        res.set_status_code(101);
        res.set_header("Upgrade", "websocket");
        res.set_header("Connection", "Upgrade");
        
        auto websocket_key = req.get_header("Sec-WebSocket-Key");
        if (websocket_key) {
            auto accept_key = websocket::WebSocket::calculate_accept_key(*websocket_key);
            res.set_header("Sec-WebSocket-Accept", accept_key);
        }
    }
}

void cors_middleware(const Request& req, Response& res, function<void()> next) {
    res.set_header("Access-Control-Allow-Origin", "*");
    res.set_header("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
    res.set_header("Access-Control-Allow-Headers", "Content-Type, Authorization");
    
    if (req.method() == HttpMethod::OPTIONS) {
        res.set_status_code(200);
        return;
    }
    
    next();
}

void auth_middleware(const Request& req, Response& res, function<void()> next) {
    auto auth_header = req.get_header("Authorization");
    if (!auth_header || !auth_header->starts_with("Bearer ")) {
        res.send_error(401, "Unauthorized");
        return;
    }
    
    next();
}

void logging_middleware(const Request& req, Response& res, function<void()> next) {
    auto start_time = std::chrono::steady_clock::now();
    
    std::cout << "[" << std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count() 
              << "] " << "Request: " << static_cast<int>(req.method()) 
              << " " << req.uri() << std::endl;
    
    next();
    
    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    
    std::cout << "[" << std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count() 
              << "] " << "Response: " << res.status_code() 
              << " (" << duration.count() << "ms)" << std::endl;
}

void rate_limit_middleware(const Request& req, Response& res, function<void()> next) {
    static hash_map<string, std::pair<int, timestamp_t>> rate_limits;
    static mutex rate_limit_mutex;
    
    lock_guard lock(rate_limit_mutex);
    
    auto client_ip = req.remote_endpoint().to_string();
    auto now = std::chrono::steady_clock::now();
    
    auto it = rate_limits.find(client_ip);
    if (it != rate_limits.end()) {
        auto& [count, last_request] = it->second;
        auto time_diff = std::chrono::duration_cast<std::chrono::minutes>(now - last_request);
        
        if (time_diff.count() < 1) {
            if (count >= 100) {
                res.send_error(429, "Too Many Requests");
                return;
            }
            count++;
        } else {
            count = 1;
            last_request = now;
        }
    } else {
        rate_limits[client_ip] = {1, now};
    }
    
    next();
}

volatile sig_atomic_t shutdown_requested = 0;

void signal_handler(int signum) {
    shutdown_requested = 1;
    std::cout << "\nShutdown requested (signal " << signum << ")" << std::endl;
}

int main() {
    try {
        signal(SIGINT, signal_handler);
        signal(SIGTERM, signal_handler);
        
        std::cout << "Starting HTTP Framework Server..." << std::endl;
        
        auto logger = std::make_unique<utils::Logger>();
        logger->set_level(LogLevel::INFO);
        logger->enable_console_output(true);
        utils::Logger::set_global_logger(std::move(logger));
        
        core::Server::Config config;
        config.host = "0.0.0.0";
        config.port = 8080;
        config.max_connections = 1000;
        config.thread_pool_size = std::thread::hardware_concurrency();
        config.enable_compression = true;
        config.enable_websocket = true;
        config.server_name = "HttpFramework/1.0";
        
        Server server(config);
        
        auto router = std::make_shared<Router>();
        
        router->get("/", [](const Request& req, Response& res) {
            res.set_html_body(R"(
<!DOCTYPE html>
<html>
<head>
    <title>HTTP Framework</title>
    <style>
        body { font-family: Arial, sans-serif; margin: 40px; }
        .container { max-width: 800px; margin: 0 auto; }
        .endpoint { background: #f5f5f5; padding: 10px; margin: 10px 0; border-radius: 5px; }
        .method { font-weight: bold; color: #007acc; }
    </style>
</head>
<body>
    <div class="container">
        <h1>HTTP Framework Demo Server</h1>
        <p>Welcome to the HTTP Framework demonstration server!</p>
        
        <h2>Available Endpoints:</h2>
        
        <div class="endpoint">
            <span class="method">GET</span> /users - List all users
        </div>
        
        <div class="endpoint">
            <span class="method">GET</span> /users/{id} - Get user by ID
        </div>
        
        <div class="endpoint">
            <span class="method">POST</span> /users - Create new user
        </div>
        
        <div class="endpoint">
            <span class="method">PUT</span> /users/{id} - Update user
        </div>
        
        <div class="endpoint">
            <span class="method">DELETE</span> /users/{id} - Delete user
        </div>
        
        <div class="endpoint">
            <span class="method">GET</span> /products - List products
        </div>
        
        <div class="endpoint">
            <span class="method">GET</span> /products/{id} - Get product by ID
        </div>
        
        <div class="endpoint">
            <span class="method">GET</span> /async - Async endpoint demo
        </div>
        
        <div class="endpoint">
            <span class="method">GET</span> /websocket - WebSocket upgrade endpoint
        </div>
        
        <div class="endpoint">
            <span class="method">GET</span> /protected - Protected endpoint (requires auth)
        </div>
        
        <h2>Features:</h2>
        <ul>
            <li>RESTful routing</li>
            <li>Middleware support</li>
            <li>Async/await support</li>
            <li>WebSocket support</li>
            <li>CORS handling</li>
            <li>Rate limiting</li>
            <li>Request logging</li>
            <li>JSON responses</li>
            <li>Static file serving</li>
            <li>Error handling</li>
        </ul>
    </div>
</body>
</html>
            )");
        });
        
        router->middleware(http::make_middleware(cors_middleware, "cors"));
        router->middleware(http::make_middleware(logging_middleware, "logging"));
        router->middleware(http::make_middleware(rate_limit_middleware, "rate_limit"));
        
        UserController user_controller;
        router->controller("/users", &user_controller);
        
        ProductController product_controller;
        router->get("/products", [&](const Request& req, Response& res) {
            product_controller.list_products(req, res);
        });
        router->get("/products/{id}", [&](const Request& req, Response& res) {
            product_controller.get_product(req, res);
        });
        
        router->get_async("/async", async_handler);
        
        router->websocket("/websocket", websocket_handler);
        
        router->group("/api/v1", [&](Router& api_router) {
            api_router.middleware(http::make_middleware(auth_middleware, "auth"));
            
            api_router.get("/protected", [](const Request& req, Response& res) {
                res.set_json_body(R"({"message": "Access granted to protected resource"})");
            });
            
            api_router.get("/stats", [&](const Request& req, Response& res) {
                auto stats = server.get_stats();
                string json = "{";
                bool first = true;
                for (const auto& [key, value] : stats) {
                    if (!first) json += ",";
                    json += "\"" + key + "\":";
                    std::visit([&](const auto& v) {
                        if constexpr (std::is_same_v<std::decay_t<decltype(v)>, string>) {
                            json += "\"" + v + "\"";
                        } else {
                            json += std::to_string(v);
                        }
                    }, value);
                    first = false;
                }
                json += "}";
                res.set_json_body(json);
            });
        });
        
        router->get("/health", [](const Request& req, Response& res) {
            res.set_json_body(R"({"status": "healthy", "timestamp": ")" + 
                             std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(
                                 std::chrono::system_clock::now().time_since_epoch()).count()) + "\"}");
        });
        
        router->fallback([](const Request& req, Response& res) {
            res.send_error(404, "Endpoint not found");
        });
        
        router->error_handler([](const Request& req, Response& res, const std::exception& e) {
            std::cerr << "Error handling request: " << e.what() << std::endl;
            res.send_error(500, "Internal server error");
        });
        
        server.set_router(router);
        
        server.enable_cors("*");
        server.enable_request_logging(true);
        server.enable_health_check("/health");
        server.enable_metrics("/metrics");
        server.set_static_file_handler("/static", "./public");
        
        server.register_startup_handler([]() {
            std::cout << "Server startup completed!" << std::endl;
        });
        
        server.register_shutdown_handler([]() {
            std::cout << "Server shutdown completed!" << std::endl;
        });
        
        server.enable_graceful_shutdown(std::chrono::seconds(10));
        
        if (!server.start()) {
            std::cerr << "Failed to start server" << std::endl;
            return 1;
        }
        
        std::cout << "Server running on http://" << config.host << ":" << config.port << std::endl;
        std::cout << "Press Ctrl+C to stop the server" << std::endl;
        
        while (!shutdown_requested && server.is_running()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        
        std::cout << "Stopping server..." << std::endl;
        server.stop();
        
        std::cout << "Server stopped. Final statistics:" << std::endl;
        std::cout << "Total requests: " << server.total_requests() << std::endl;
        std::cout << "Failed requests: " << server.failed_requests() << std::endl;
        std::cout << "Uptime: " << std::chrono::duration_cast<std::chrono::seconds>(server.uptime()).count() << "s" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
} 