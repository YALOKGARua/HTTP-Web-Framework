#include <gtest/gtest.h>
#include "http/router.hpp"
#include "http/request.hpp"
#include "http/response.hpp"

using namespace http_framework;

class RouterTest : public ::testing::Test {
protected:
    void SetUp() override {
        router = std::make_unique<Router>();
    }

    std::unique_ptr<Router> router;
};

TEST_F(RouterTest, BasicRouteRegistration) {
    bool handler_called = false;
    
    router->get("/test", [&handler_called](const Request& req, Response& res) {
        handler_called = true;
        res.set_status_code(200);
    });
    
    EXPECT_TRUE(router->has_route(HttpMethod::GET, "/test"));
}

TEST_F(RouterTest, RouteMatching) {
    router->get("/users/{id}", [](const Request& req, Response& res) {
        auto id = req.get_path_param("id");
        if (id) {
            res.set_json_body("{\"id\": " + *id + "}");
        }
        res.set_status_code(200);
    });
    
    EXPECT_TRUE(router->has_route(HttpMethod::GET, "/users/123"));
    EXPECT_FALSE(router->has_route(HttpMethod::POST, "/users/123"));
}

TEST_F(RouterTest, MiddlewareExecution) {
    bool middleware_called = false;
    bool handler_called = false;
    
    router->use([&middleware_called](const Request& req, Response& res, std::function<void()> next) {
        middleware_called = true;
        next();
    });
    
    router->get("/test", [&handler_called](const Request& req, Response& res) {
        handler_called = true;
        res.set_status_code(200);
    });
    
    EXPECT_TRUE(middleware_called);
    EXPECT_TRUE(handler_called);
} 