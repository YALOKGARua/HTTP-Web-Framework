#pragma once

#include "core/types.hpp"
#include "core/connection.hpp"
#include "websocket/frame.hpp"

namespace http_framework::websocket {
    class WebSocket {
    public:
        enum class State {
            CONNECTING,
            OPEN,
            CLOSING,
            CLOSED
        };
        
        enum class CloseCode : uint16_t {
            NORMAL_CLOSURE = 1000,
            GOING_AWAY = 1001,
            PROTOCOL_ERROR = 1002,
            UNSUPPORTED_DATA = 1003,
            NO_STATUS_RECEIVED = 1005,
            ABNORMAL_CLOSURE = 1006,
            INVALID_FRAME_PAYLOAD_DATA = 1007,
            POLICY_VIOLATION = 1008,
            MESSAGE_TOO_BIG = 1009,
            MANDATORY_EXTENSION = 1010,
            INTERNAL_SERVER_ERROR = 1011,
            SERVICE_RESTART = 1012,
            TRY_AGAIN_LATER = 1013,
            BAD_GATEWAY = 1014,
            TLS_HANDSHAKE = 1015
        };
        
        using MessageHandler = function<void(const buffer_t&, WebSocketOpcode)>;
        using CloseHandler = function<void(CloseCode, string_view)>;
        using ErrorHandler = function<void(const std::exception&)>;
        using PingHandler = function<void(const buffer_t&)>;
        using PongHandler = function<void(const buffer_t&)>;
        
        WebSocket(shared_ptr<core::Connection> connection, bool is_server = true);
        WebSocket(const WebSocket&) = delete;
        WebSocket(WebSocket&&) = default;
        WebSocket& operator=(const WebSocket&) = delete;
        WebSocket& operator=(WebSocket&&) = default;
        ~WebSocket();
        
        State state() const noexcept { return state_; }
        bool is_open() const noexcept { return state_ == State::OPEN; }
        bool is_closing() const noexcept { return state_ == State::CLOSING; }
        bool is_closed() const noexcept { return state_ == State::CLOSED; }
        bool is_server() const noexcept { return is_server_; }
        
        const string& protocol() const noexcept { return protocol_; }
        const vector<string>& extensions() const noexcept { return extensions_; }
        
        bool send_text(string_view message);
        bool send_binary(const buffer_t& data);
        bool send_ping(const buffer_t& payload = {});
        bool send_pong(const buffer_t& payload = {});
        bool send_close(CloseCode code = CloseCode::NORMAL_CLOSURE, string_view reason = "");
        
        async::Task<bool> send_text_async(string_view message);
        async::Task<bool> send_binary_async(const buffer_t& data);
        async::Task<bool> send_ping_async(const buffer_t& payload = {});
        async::Task<bool> send_pong_async(const buffer_t& payload = {});
        async::Task<bool> send_close_async(CloseCode code = CloseCode::NORMAL_CLOSURE, string_view reason = "");
        
        bool receive_message(buffer_t& data, WebSocketOpcode& opcode);
        async::Task<bool> receive_message_async(buffer_t& data, WebSocketOpcode& opcode);
        
        void start_message_loop();
        void stop_message_loop();
        bool is_message_loop_running() const noexcept { return message_loop_running_; }
        
        void set_message_handler(MessageHandler handler) { message_handler_ = std::move(handler); }
        void set_close_handler(CloseHandler handler) { close_handler_ = std::move(handler); }
        void set_error_handler(ErrorHandler handler) { error_handler_ = std::move(handler); }
        void set_ping_handler(PingHandler handler) { ping_handler_ = std::move(handler); }
        void set_pong_handler(PongHandler handler) { pong_handler_ = std::move(handler); }
        
        void close(CloseCode code = CloseCode::NORMAL_CLOSURE, string_view reason = "");
        void force_close();
        
        void set_max_message_size(size_type max_size) { max_message_size_ = max_size; }
        size_type max_message_size() const noexcept { return max_message_size_; }
        
        void enable_compression(bool enable = true) { compression_enabled_ = enable; }
        bool is_compression_enabled() const noexcept { return compression_enabled_; }
        
        void set_ping_interval(duration_t interval) { ping_interval_ = interval; }
        duration_t ping_interval() const noexcept { return ping_interval_; }
        
        void set_pong_timeout(duration_t timeout) { pong_timeout_ = timeout; }
        duration_t pong_timeout() const noexcept { return pong_timeout_; }
        
        void enable_auto_ping(bool enable = true);
        void disable_auto_ping() { enable_auto_ping(false); }
        bool is_auto_ping_enabled() const noexcept { return auto_ping_enabled_; }
        
        size_type messages_sent() const noexcept { return messages_sent_; }
        size_type messages_received() const noexcept { return messages_received_; }
        size_type bytes_sent() const noexcept { return bytes_sent_; }
        size_type bytes_received() const noexcept { return bytes_received_; }
        
        timestamp_t created_at() const noexcept { return created_at_; }
        timestamp_t last_ping_sent() const noexcept { return last_ping_sent_; }
        timestamp_t last_pong_received() const noexcept { return last_pong_received_; }
        
        hash_map<string, variant<string, int64_t, double, bool>> get_statistics() const;
        
        template<typename T>
        void set_user_data(T&& data) { user_data_ = std::forward<T>(data); }
        
        template<typename T>
        optional<T> get_user_data() const {
            if (std::holds_alternative<T>(user_data_)) {
                return std::get<T>(user_data_);
            }
            return std::nullopt;
        }
        
        void set_attribute(string_view name, variant<string, int64_t, double, bool> value);
        optional<variant<string, int64_t, double, bool>> get_attribute(string_view name) const;
        bool has_attribute(string_view name) const;
        void remove_attribute(string_view name);
        void clear_attributes();
        
        static string generate_websocket_key();
        static string calculate_accept_key(string_view websocket_key);
        static bool validate_websocket_key(string_view websocket_key);
        
        string to_string() const;
        
    private:
        shared_ptr<core::Connection> connection_;
        State state_{State::CONNECTING};
        bool is_server_;
        
        string protocol_;
        vector<string> extensions_;
        
        MessageHandler message_handler_;
        CloseHandler close_handler_;
        ErrorHandler error_handler_;
        PingHandler ping_handler_;
        PongHandler pong_handler_;
        
        atomic<bool> message_loop_running_{false};
        unique_ptr<std::thread> message_loop_thread_;
        
        size_type max_message_size_{1024 * 1024};
        bool compression_enabled_{false};
        
        duration_t ping_interval_{std::chrono::seconds(30)};
        duration_t pong_timeout_{std::chrono::seconds(10)};
        bool auto_ping_enabled_{false};
        unique_ptr<std::thread> ping_thread_;
        atomic<bool> ping_thread_running_{false};
        
        atomic<size_type> messages_sent_{0};
        atomic<size_type> messages_received_{0};
        atomic<size_type> bytes_sent_{0};
        atomic<size_type> bytes_received_{0};
        
        timestamp_t created_at_;
        atomic<timestamp_t> last_ping_sent_;
        atomic<timestamp_t> last_pong_received_;
        
        variant<std::monostate, string, int64_t, double, bool> user_data_;
        hash_map<string, variant<string, int64_t, double, bool>> attributes_;
        
        mutable mutex websocket_mutex_;
        
        buffer_t fragmented_message_;
        WebSocketOpcode fragmented_opcode_{WebSocketOpcode::TEXT};
        bool expecting_continuation_{false};
        
        CloseCode close_code_{CloseCode::NO_STATUS_RECEIVED};
        string close_reason_;
        
        void message_loop();
        void ping_loop();
        
        bool send_frame(const Frame& frame);
        async::Task<bool> send_frame_async(const Frame& frame);
        
        bool receive_frame(Frame& frame);
        async::Task<bool> receive_frame_async(Frame& frame);
        
        bool process_frame(const Frame& frame);
        void handle_control_frame(const Frame& frame);
        void handle_data_frame(const Frame& frame);
        
        void handle_close_frame(const Frame& frame);
        void handle_ping_frame(const Frame& frame);
        void handle_pong_frame(const Frame& frame);
        
        bool validate_frame(const Frame& frame) const;
        bool is_control_frame(WebSocketOpcode opcode) const;
        
        buffer_t compress_payload(const buffer_t& payload);
        buffer_t decompress_payload(const buffer_t& payload);
        
        void update_state(State new_state);
        void handle_error(const std::exception& e);
        
        void send_automatic_pong(const buffer_t& ping_payload);
        void check_connection_health();
        
        string format_close_frame_payload(CloseCode code, string_view reason);
        std::pair<CloseCode, string> parse_close_frame_payload(const buffer_t& payload);
        
        void cleanup_resources();
        void log_websocket_event(string_view event, const hash_map<string, string>& data = {});
    };
    
    enum class HandshakeResult {
        SUCCESS,
        INVALID_METHOD,
        MISSING_UPGRADE_HEADER,
        MISSING_CONNECTION_HEADER,
        MISSING_WEBSOCKET_VERSION,
        UNSUPPORTED_VERSION,
        MISSING_WEBSOCKET_KEY,
        INVALID_WEBSOCKET_KEY,
        PROTOCOL_NOT_SUPPORTED,
        EXTENSION_NOT_SUPPORTED,
        INTERNAL_ERROR
    };
    
    class WebSocketHandshake {
    public:
        static HandshakeResult perform_server_handshake(const http::Request& request, http::Response& response, 
                                                       const vector<string>& supported_protocols = {},
                                                       const vector<string>& supported_extensions = {});
        
        static HandshakeResult perform_client_handshake(http::Request& request, const http::Response& response,
                                                       string_view protocol = "",
                                                       const vector<string>& extensions = {});
        
        static bool is_websocket_request(const http::Request& request);
        static bool is_websocket_response(const http::Response& response);
        
        static string select_protocol(const vector<string>& client_protocols, 
                                    const vector<string>& server_protocols);
        
        static vector<string> select_extensions(const vector<string>& client_extensions,
                                              const vector<string>& server_extensions);
        
        static string format_protocols(const vector<string>& protocols);
        static vector<string> parse_protocols(string_view protocols_header);
        
        static string format_extensions(const vector<string>& extensions);
        static vector<string> parse_extensions(string_view extensions_header);
        
    private:
        static bool validate_websocket_version(string_view version);
        static bool validate_upgrade_header(string_view upgrade);
        static bool validate_connection_header(string_view connection);
        
        static string generate_websocket_accept(string_view websocket_key);
        static bool verify_websocket_accept(string_view websocket_key, string_view websocket_accept);
        
        static const string WEBSOCKET_MAGIC_STRING;
        static const string SUPPORTED_VERSION;
    };
} 