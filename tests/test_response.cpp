#include <gtest/gtest.h>
#include "http/response.hpp"

using namespace http_framework;

class ResponseTest : public ::testing::Test {
protected:
    void SetUp() override {
        response = std::make_unique<Response>();
    }

    std::unique_ptr<Response> response;
};

TEST_F(ResponseTest, StatusCodeHandling) {
    response->set_status_code(404);
    EXPECT_EQ(response->status_code(), 404);
    
    response->set_status_code(200);
    EXPECT_EQ(response->status_code(), 200);
}

TEST_F(ResponseTest, HeaderManagement) {
    response->set_header("Content-Type", "application/json");
    response->set_header("Cache-Control", "no-cache");
    
    auto content_type = response->get_header("Content-Type");
    auto cache_control = response->get_header("Cache-Control");
    
    ASSERT_TRUE(content_type.has_value());
    ASSERT_TRUE(cache_control.has_value());
    EXPECT_EQ(*content_type, "application/json");
    EXPECT_EQ(*cache_control, "no-cache");
}

TEST_F(ResponseTest, JsonBodySetting) {
    std::string json = "{\"message\": \"success\", \"data\": [1, 2, 3]}";
    response->set_json_body(json);
    
    auto content_type = response->get_header("Content-Type");
    ASSERT_TRUE(content_type.has_value());
    EXPECT_EQ(*content_type, "application/json");
    EXPECT_EQ(response->body_as_string(), json);
}

TEST_F(ResponseTest, HtmlBodySetting) {
    std::string html = "<html><body><h1>Hello World</h1></body></html>";
    response->set_html_body(html);
    
    auto content_type = response->get_header("Content-Type");
    ASSERT_TRUE(content_type.has_value());
    EXPECT_EQ(*content_type, "text/html");
    EXPECT_EQ(response->body_as_string(), html);
}

TEST_F(ResponseTest, ErrorResponse) {
    response->send_error(500, "Internal Server Error");
    
    EXPECT_EQ(response->status_code(), 500);
    auto content_type = response->get_header("Content-Type");
    ASSERT_TRUE(content_type.has_value());
    EXPECT_EQ(*content_type, "application/json");
} 