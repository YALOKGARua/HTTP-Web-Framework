#include <gtest/gtest.h>
#include "http/request.hpp"

using namespace http_framework;

class RequestTest : public ::testing::Test {
protected:
    void SetUp() override {
        request = std::make_unique<Request>();
    }

    std::unique_ptr<Request> request;
};

TEST_F(RequestTest, HeaderParsing) {
    request->set_header("Content-Type", "application/json");
    request->set_header("Authorization", "Bearer token123");
    
    auto content_type = request->get_header("Content-Type");
    auto auth = request->get_header("Authorization");
    
    ASSERT_TRUE(content_type.has_value());
    ASSERT_TRUE(auth.has_value());
    EXPECT_EQ(*content_type, "application/json");
    EXPECT_EQ(*auth, "Bearer token123");
}

TEST_F(RequestTest, QueryParameterParsing) {
    request->set_uri("/api/users?page=1&limit=10&sort=name");
    
    auto page = request->get_query_param("page");
    auto limit = request->get_query_param("limit");
    auto sort = request->get_query_param("sort");
    auto missing = request->get_query_param("missing");
    
    ASSERT_TRUE(page.has_value());
    ASSERT_TRUE(limit.has_value());
    ASSERT_TRUE(sort.has_value());
    EXPECT_FALSE(missing.has_value());
    
    EXPECT_EQ(*page, "1");
    EXPECT_EQ(*limit, "10");
    EXPECT_EQ(*sort, "name");
}

TEST_F(RequestTest, PathParameterExtraction) {
    request->set_uri("/api/users/123/posts/456");
    request->set_route_pattern("/api/users/{user_id}/posts/{post_id}");
    
    auto user_id = request->get_path_param("user_id");
    auto post_id = request->get_path_param("post_id");
    
    ASSERT_TRUE(user_id.has_value());
    ASSERT_TRUE(post_id.has_value());
    EXPECT_EQ(*user_id, "123");
    EXPECT_EQ(*post_id, "456");
}

TEST_F(RequestTest, BodyHandling) {
    std::string json_body = "{\"name\": \"John\", \"age\": 30}";
    request->set_body(json_body);
    
    EXPECT_EQ(request->body_as_string(), json_body);
    EXPECT_EQ(request->body().size(), json_body.size());
} 