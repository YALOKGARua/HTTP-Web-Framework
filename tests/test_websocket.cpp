#include <gtest/gtest.h>
#include "websocket/websocket.hpp"
#include "websocket/frame.hpp"

using namespace http_framework::websocket;

class WebSocketTest : public ::testing::Test {
protected:
    void SetUp() override {
        websocket = std::make_unique<WebSocket>();
    }

    std::unique_ptr<WebSocket> websocket;
};

TEST_F(WebSocketTest, AcceptKeyCalculation) {
    std::string client_key = "dGhlIHNhbXBsZSBub25jZQ==";
    std::string expected_accept = "s3pPLMBiTxaQ9kYGzzhZRbK+xOo=";
    
    std::string calculated_accept = WebSocket::calculate_accept_key(client_key);
    EXPECT_EQ(calculated_accept, expected_accept);
}

TEST_F(WebSocketTest, FrameParsing) {
    Frame frame;
    frame.set_fin(true);
    frame.set_opcode(Frame::Opcode::TEXT);
    frame.set_payload("Hello WebSocket");
    
    EXPECT_TRUE(frame.is_fin());
    EXPECT_EQ(frame.opcode(), Frame::Opcode::TEXT);
    EXPECT_EQ(frame.payload_as_string(), "Hello WebSocket");
}

TEST_F(WebSocketTest, MaskedFrameHandling) {
    Frame frame;
    frame.set_fin(true);
    frame.set_opcode(Frame::Opcode::TEXT);
    frame.set_masked(true);
    frame.set_mask_key({0x12, 0x34, 0x56, 0x78});
    frame.set_payload("Test");
    
    EXPECT_TRUE(frame.is_masked());
    EXPECT_EQ(frame.mask_key().size(), 4);
    EXPECT_EQ(frame.payload_as_string(), "Test");
}

TEST_F(WebSocketTest, ConnectionUpgrade) {
    auto is_valid_upgrade = [](const std::string& upgrade, const std::string& connection) {
        return upgrade == "websocket" && connection.find("Upgrade") != std::string::npos;
    };
    
    EXPECT_TRUE(is_valid_upgrade("websocket", "Upgrade"));
    EXPECT_TRUE(is_valid_upgrade("websocket", "keep-alive, Upgrade"));
    EXPECT_FALSE(is_valid_upgrade("http", "keep-alive"));
} 