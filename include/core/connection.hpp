#pragma once

#include "core/types.hpp"
#include "network/socket.hpp"
#include "http/request.hpp"
#include "http/response.hpp"

namespace http_framework::core {
    class Connection {
    public:
        Connection(unique_ptr<network::Socket> socket, const Endpoint& remote_endpoint);
        Connection(const Connection&) = delete;
        Connection(Connection&&) = default;
        Connection& operator=(const Connection&) = delete;
        Connection& operator=(Connection&&) = default;
        ~Connection();
        
        ConnectionState state() const noexcept { return state_; }
        const Endpoint& remote_endpoint() const noexcept { return remote_endpoint_; }
        const Endpoint& local_endpoint() const noexcept { return local_endpoint_; }
        
        bool is_connected() const noexcept { return state_ == ConnectionState::CONNECTED; }
        bool is_secure() const noexcept { return is_secure_; }
        bool is_websocket() const noexcept { return is_websocket_; }
        bool is_keep_alive() const noexcept { return keep_alive_; }
        
        timestamp_t created_at() const noexcept { return created_at_; }
        timestamp_t last_activity() const noexcept { return last_activity_; }
        duration_t idle_time() const;
        
        size_type connection_id() const noexcept { return connection_id_; }
        size_type request_count() const noexcept { return request_count_; }
        size_type bytes_sent() const noexcept { return bytes_sent_; }
        size_type bytes_received() const noexcept { return bytes_received_; }
        
        bool read_request(http::Request& request);
        async::Task<bool> read_request_async(http::Request& request);
        
        bool write_response(const http::Response& response);
        async::Task<bool> write_response_async(const http::Response& response);
        
        ssize_type read_data(mutable_byte_span buffer);
        async::Task<ssize_type> read_data_async(mutable_byte_span buffer);
        
        ssize_type write_data(byte_span data);
        async::Task<ssize_type> write_data_async(byte_span data);
        
        bool read_line(string& line);
        async::Task<bool> read_line_async(string& line);
        
        bool read_until(string& data, string_view delimiter);
        async::Task<bool> read_until_async(string& data, string_view delimiter);
        
        void set_timeout(duration_t timeout);
        void set_read_timeout(duration_t timeout);
        void set_write_timeout(duration_t timeout);
        
        void enable_keep_alive(bool enable = true);
        void set_keep_alive_timeout(duration_t timeout);
        
        void upgrade_to_websocket(string_view protocol = "");
        void downgrade_from_websocket();
        
        bool send_websocket_frame(const buffer_t& data, WebSocketOpcode opcode = WebSocketOpcode::TEXT);
        async::Task<bool> send_websocket_frame_async(const buffer_t& data, WebSocketOpcode opcode = WebSocketOpcode::TEXT);
        
        bool receive_websocket_frame(buffer_t& data, WebSocketOpcode& opcode);
        async::Task<bool> receive_websocket_frame_async(buffer_t& data, WebSocketOpcode& opcode);
        
        void send_websocket_ping(const buffer_t& payload = {});
        void send_websocket_pong(const buffer_t& payload = {});
        void send_websocket_close(uint16_t code = 1000, string_view reason = "");
        
        void close();
        void close_gracefully();
        void force_close();
        
        bool is_readable() const;
        bool is_writable() const;
        bool has_error() const;
        
        void set_blocking(bool blocking);
        bool is_blocking() const;
        
        void enable_tcp_nodelay(bool enable = true);
        void enable_tcp_keepalive(bool enable = true);
        void set_send_buffer_size(size_type size);
        void set_receive_buffer_size(size_type size);
        
        string get_peer_certificate() const;
        vector<string> get_peer_certificate_chain() const;
        bool verify_peer_certificate() const;
        
        void set_compression(CompressionType type);
        CompressionType compression_type() const noexcept { return compression_type_; }
        
        template<typename T>
        void set_user_data(T&& data);
        
        template<typename T>
        optional<T> get_user_data() const;
        
        bool has_user_data() const;
        void clear_user_data();
        
        void set_attribute(string_view name, variant<string, int64_t, double, bool> value);
        optional<variant<string, int64_t, double, bool>> get_attribute(string_view name) const;
        bool has_attribute(string_view name) const;
        void remove_attribute(string_view name);
        void clear_attributes();
        
        string to_string() const;
        hash_map<string, variant<string, int64_t, double, bool>> get_info() const;
        
        void start_health_monitoring();
        void stop_health_monitoring();
        bool is_healthy() const;
        
        void enable_bandwidth_limiting(size_type bytes_per_second);
        void disable_bandwidth_limiting();
        
        void enable_request_tracing(bool enable = true);
        void trace_event(string_view event, const hash_map<string, string>& data = {});
        
        void reset_connection();
        void prepare_for_reuse();
        
    private:
        unique_ptr<network::Socket> socket_;
        Endpoint remote_endpoint_;
        Endpoint local_endpoint_;
        ConnectionState state_{ConnectionState::CONNECTING};
        
        size_type connection_id_;
        timestamp_t created_at_;
        atomic<timestamp_t> last_activity_;
        
        atomic<size_type> request_count_{0};
        atomic<size_type> bytes_sent_{0};
        atomic<size_type> bytes_received_{0};
        
        bool is_secure_{false};
        bool is_websocket_{false};
        bool keep_alive_{true};
        duration_t keep_alive_timeout_{std::chrono::seconds(5)};
        
        duration_t read_timeout_{std::chrono::seconds(30)};
        duration_t write_timeout_{std::chrono::seconds(30)};
        
        buffer_t read_buffer_;
        buffer_t write_buffer_;
        size_type read_buffer_offset_{0};
        size_type write_buffer_offset_{0};
        
        CompressionType compression_type_{CompressionType::NONE};
        optional<string> websocket_protocol_;
        
        variant<std::monostate, string, int64_t, double, bool> user_data_;
        hash_map<string, variant<string, int64_t, double, bool>> attributes_;
        
        bool health_monitoring_enabled_{false};
        atomic<bool> is_healthy_{true};
        
        optional<size_type> bandwidth_limit_;
        timestamp_t last_bandwidth_check_;
        size_type bandwidth_consumed_{0};
        
        bool request_tracing_enabled_{false};
        vector<std::pair<timestamp_t, string>> trace_events_;
        
        mutable mutex connection_mutex_;
        
        static atomic<size_type> next_connection_id_;
        
        void update_last_activity();
        void update_state(ConnectionState new_state);
        
        bool parse_http_request_line(const string& line, http::Request& request);
        bool parse_http_headers(vector<string>& lines, http::Request& request);
        bool read_http_body(http::Request& request, size_type content_length);
        
        string format_http_response(const http::Response& response);
        buffer_t serialize_response(const http::Response& response);
        
        bool check_bandwidth_limit(size_type bytes);
        void apply_bandwidth_limiting(size_type bytes);
        
        bool perform_websocket_handshake(const http::Request& request);
        string generate_websocket_accept_key(string_view websocket_key) const;
        
        bool compress_data(const buffer_t& input, buffer_t& output);
        bool decompress_data(const buffer_t& input, buffer_t& output);
        
        void log_connection_event(string_view event);
        void collect_connection_metrics();
        
        ssize_type raw_read(mutable_byte_span buffer);
        ssize_type raw_write(byte_span data);
        
        async::Task<ssize_type> raw_read_async(mutable_byte_span buffer);
        async::Task<ssize_type> raw_write_async(byte_span data);
        
        void handle_connection_error(const std::exception& e);
        void cleanup_connection_resources();
    };
    
    template<typename T>
    void Connection::set_user_data(T&& data) {
        lock_guard lock(connection_mutex_);
        user_data_ = std::forward<T>(data);
    }
    
    template<typename T>
    optional<T> Connection::get_user_data() const {
        lock_guard lock(connection_mutex_);
        if (std::holds_alternative<T>(user_data_)) {
            return std::get<T>(user_data_);
        }
        return std::nullopt;
    }
} 