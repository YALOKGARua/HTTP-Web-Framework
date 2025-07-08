#pragma once

#include "core/types.hpp"
#include "core/connection.hpp"
#include "http/request.hpp"
#include "http/response.hpp"
#include "async/future.hpp"
#include "async/task.hpp"
#include <concepts>

namespace http_framework::streaming {
    using EventId = string;
    using EventType = string;
    using StreamId = string;
    using ClientId = string;
    
    enum class ConnectionState : std::uint8_t {
        CONNECTING = 0,
        CONNECTED = 1,
        RECONNECTING = 2,
        DISCONNECTED = 3,
        ERROR = 4
    };
    
    struct EventData {
        optional<EventId> id;
        optional<EventType> event;
        string data;
        optional<duration_t> retry;
        hash_map<string, string> custom_fields;
        
        EventData() = default;
        explicit EventData(string_view d) : data(d) {}
        EventData(EventType e, string_view d) : event(std::move(e)), data(d) {}
        EventData(EventId i, EventType e, string_view d) : id(std::move(i)), event(std::move(e)), data(d) {}
        
        string to_sse_format() const;
        static optional<EventData> from_sse_format(string_view sse_data);
    };
    
    class SseClient {
    public:
        struct Config {
            string user_agent{"HttpFramework-SSE/1.0"};
            duration_t connection_timeout{std::chrono::seconds(30)};
            duration_t retry_interval{std::chrono::seconds(3)};
            size_type max_retry_attempts{10};
            size_type max_message_size{1024 * 1024};
            bool auto_reconnect{true};
            hash_map<string, string> headers;
        };
        
        explicit SseClient(Config config = {});
        ~SseClient();
        
        async::Future<bool> connect(string_view url);
        void disconnect();
        bool is_connected() const noexcept { return state_ == ConnectionState::CONNECTED; }
        ConnectionState state() const noexcept { return state_; }
        
        void set_message_handler(function<async::Task<void>(const EventData&)> handler);
        void set_error_handler(function<void(const std::exception&)> handler);
        void set_connection_handler(function<void(ConnectionState)> handler);
        
        void set_last_event_id(string_view event_id) { last_event_id_ = event_id; }
        const string& last_event_id() const noexcept { return last_event_id_; }
        
        void start_listening();
        void stop_listening();
        bool is_listening() const noexcept { return listening_; }
        
        size_type messages_received() const noexcept { return messages_received_; }
        size_type reconnection_attempts() const noexcept { return reconnection_attempts_; }
        timestamp_t connected_at() const noexcept { return connected_at_; }
        
    private:
        Config config_;
        atomic<ConnectionState> state_{ConnectionState::DISCONNECTED};
        atomic<bool> listening_{false};
        atomic<bool> should_reconnect_{true};
        
        string url_;
        string last_event_id_;
        unique_ptr<std::thread> listener_thread_;
        unique_ptr<class HttpClient> http_client_;
        
        optional<function<async::Task<void>(const EventData&)>> message_handler_;
        optional<function<void(const std::exception&)>> error_handler_;
        optional<function<void(ConnectionState)>> connection_handler_;
        
        atomic<size_type> messages_received_{0};
        atomic<size_type> reconnection_attempts_{0};
        timestamp_t connected_at_;
        
        void listen_loop();
        async::Task<void> handle_message_line(string_view line);
        async::Task<void> process_complete_message(const EventData& event);
        void handle_error(const std::exception& e);
        void update_state(ConnectionState new_state);
        bool should_retry() const;
        duration_t get_retry_delay() const;
    };
    
    class SseConnection {
    public:
        SseConnection(ClientId client_id, shared_ptr<core::Connection> connection);
        ~SseConnection();
        
        const ClientId& client_id() const noexcept { return client_id_; }
        bool is_connected() const noexcept { return connected_; }
        timestamp_t connected_at() const noexcept { return connected_at_; }
        timestamp_t last_activity() const noexcept { return last_activity_; }
        
        async::Future<bool> send_event(const EventData& event);
        async::Future<bool> send_comment(string_view comment);
        async::Future<bool> send_keepalive();
        
        void set_compression(bool enable) { compression_enabled_ = enable; }
        bool is_compression_enabled() const noexcept { return compression_enabled_; }
        
        void close();
        
        size_type events_sent() const noexcept { return events_sent_; }
        size_type bytes_sent() const noexcept { return bytes_sent_; }
        
        void set_user_data(const variant<string, int64_t, double, bool>& data) { user_data_ = data; }
        const variant<string, int64_t, double, bool>& user_data() const noexcept { return user_data_; }
        
        template<typename T>
        void set_attribute(string_view name, T&& value);
        
        template<typename T>
        optional<T> get_attribute(string_view name) const;
        
        hash_map<string, variant<string, int64_t, double, bool>> get_statistics() const;
        
    private:
        ClientId client_id_;
        shared_ptr<core::Connection> connection_;
        atomic<bool> connected_{true};
        timestamp_t connected_at_;
        atomic<timestamp_t> last_activity_;
        
        atomic<size_type> events_sent_{0};
        atomic<size_type> bytes_sent_{0};
        
        bool compression_enabled_{false};
        variant<string, int64_t, double, bool> user_data_;
        hash_map<string, variant<string, int64_t, double, bool>> attributes_;
        mutable shared_mutex attributes_mutex_;
        
        async::Future<bool> write_data(string_view data);
        string compress_data(string_view data) const;
        void update_activity();
    };
    
    template<typename T>
    concept StreamFilter = requires(const T& filter, const EventData& event, const SseConnection& connection) {
        { filter.should_send(event, connection) } -> std::convertible_to<bool>;
    };
    
    class BasicStreamFilter {
    public:
        BasicStreamFilter() = default;
        
        BasicStreamFilter& add_event_type(string_view event_type);
        BasicStreamFilter& remove_event_type(string_view event_type);
        BasicStreamFilter& add_client_attribute_filter(string_view attribute, string_view value);
        BasicStreamFilter& set_rate_limit(size_type events_per_second);
        
        bool should_send(const EventData& event, const SseConnection& connection) const;
        
    private:
        vector<string> allowed_event_types_;
        hash_map<string, string> required_attributes_;
        optional<size_type> rate_limit_;
        mutable hash_map<ClientId, std::pair<size_type, timestamp_t>> rate_counters_;
        mutable mutex rate_mutex_;
    };
    
    class SseStream {
    public:
        explicit SseStream(StreamId stream_id);
        ~SseStream();
        
        const StreamId& stream_id() const noexcept { return stream_id_; }
        
        void add_client(shared_ptr<SseConnection> connection);
        void remove_client(const ClientId& client_id);
        bool has_client(const ClientId& client_id) const;
        size_type client_count() const;
        vector<ClientId> get_client_ids() const;
        
        async::Future<size_type> broadcast(const EventData& event);
        async::Future<bool> send_to_client(const ClientId& client_id, const EventData& event);
        async::Future<size_type> send_keepalive();
        
        void set_filter(shared_ptr<BasicStreamFilter> filter) { filter_ = std::move(filter); }
        void clear_filter() { filter_.reset(); }
        
        void enable_history(size_type max_events = 100);
        void disable_history();
        vector<EventData> get_history(size_type count = 0) const;
        
        void enable_persistence(string_view storage_path);
        void disable_persistence();
        
        hash_map<string, variant<string, int64_t, double, bool>> get_statistics() const;
        
        void set_compression(bool enable) { compression_enabled_ = enable; }
        void set_max_clients(size_type max_clients) { max_clients_ = max_clients; }
        void set_idle_timeout(duration_t timeout) { idle_timeout_ = timeout; }
        
    private:
        StreamId stream_id_;
        hash_map<ClientId, shared_ptr<SseConnection>> clients_;
        mutable shared_mutex clients_mutex_;
        
        shared_ptr<BasicStreamFilter> filter_;
        
        bool history_enabled_{false};
        size_type max_history_events_{100};
        deque<EventData> event_history_;
        mutable shared_mutex history_mutex_;
        
        bool persistence_enabled_{false};
        string storage_path_;
        
        bool compression_enabled_{true};
        optional<size_type> max_clients_;
        optional<duration_t> idle_timeout_;
        
        timestamp_t created_at_;
        atomic<size_type> total_events_sent_{0};
        atomic<size_type> total_clients_served_{0};
        
        void cleanup_disconnected_clients();
        void add_to_history(const EventData& event);
        async::Task<void> persist_event(const EventData& event);
        async::Task<vector<EventData>> load_persisted_events();
        bool should_send_to_client(const EventData& event, const SseConnection& connection) const;
    };
    
    class SseServer {
    public:
        struct Config {
            string endpoint_path{"/events"};
            duration_t heartbeat_interval{std::chrono::seconds(30)};
            duration_t client_timeout{std::chrono::minutes(5)};
            size_type max_clients_per_stream{1000};
            size_type max_total_clients{10000};
            size_type max_event_size{64 * 1024};
            bool enable_cors{true};
            string cors_origin{"*"};
            bool require_authentication{false};
            vector<string> allowed_origins;
        };
        
        explicit SseServer(Config config = {});
        ~SseServer();
        
        void handle_request(const http::Request& request, http::Response& response);
        async::Task<void> handle_request_async(const http::Request& request, http::Response& response);
        
        shared_ptr<SseStream> create_stream(const StreamId& stream_id);
        shared_ptr<SseStream> get_stream(const StreamId& stream_id);
        void remove_stream(const StreamId& stream_id);
        bool has_stream(const StreamId& stream_id) const;
        vector<StreamId> get_stream_ids() const;
        
        async::Future<size_type> broadcast_to_all(const EventData& event);
        async::Future<size_type> broadcast_to_stream(const StreamId& stream_id, const EventData& event);
        async::Future<bool> send_to_client(const ClientId& client_id, const EventData& event);
        
        void start_heartbeat();
        void stop_heartbeat();
        
        void set_authentication_handler(function<async::Future<bool>(const http::Request&)> handler);
        void set_client_filter(function<bool(const http::Request&)> filter);
        void set_stream_selector(function<StreamId(const http::Request&)> selector);
        
        size_type total_clients() const;
        size_type total_streams() const;
        hash_map<string, variant<string, int64_t, double, bool>> get_statistics() const;
        
        void enable_metrics_endpoint(string_view path = "/sse-metrics");
        void disable_metrics_endpoint();
        
    private:
        Config config_;
        hash_map<StreamId, shared_ptr<SseStream>> streams_;
        hash_map<ClientId, std::pair<StreamId, shared_ptr<SseConnection>>> client_registry_;
        mutable shared_mutex streams_mutex_;
        mutable shared_mutex clients_mutex_;
        
        optional<function<async::Future<bool>(const http::Request&)>> auth_handler_;
        optional<function<bool(const http::Request&)>> client_filter_;
        optional<function<StreamId(const http::Request&)>> stream_selector_;
        
        unique_ptr<std::thread> heartbeat_thread_;
        atomic<bool> heartbeat_running_{false};
        
        bool metrics_enabled_{false};
        string metrics_endpoint_;
        
        atomic<size_type> total_connections_{0};
        atomic<size_type> total_events_sent_{0};
        timestamp_t server_start_time_;
        
        async::Task<bool> authenticate_client(const http::Request& request);
        bool is_origin_allowed(string_view origin) const;
        ClientId generate_client_id() const;
        StreamId determine_stream_id(const http::Request& request);
        
        void setup_sse_response(http::Response& response);
        void add_cors_headers(http::Response& response, string_view origin = "*");
        
        async::Task<void> handle_client_connection(const http::Request& request, http::Response& response);
        void cleanup_inactive_clients();
        void heartbeat_loop();
        
        void handle_metrics_request(const http::Request& request, http::Response& response);
        string generate_metrics_json() const;
    };
    
    class EventChannel {
    public:
        explicit EventChannel(string_view name);
        
        void subscribe(function<async::Task<void>(const EventData&)> handler);
        void unsubscribe();
        
        async::Future<void> publish(const EventData& event);
        async::Future<void> publish_async(const EventData& event);
        
        const string& name() const noexcept { return name_; }
        size_type subscriber_count() const;
        
        void enable_buffering(size_type buffer_size = 100);
        void disable_buffering();
        void flush_buffer();
        
    private:
        string name_;
        vector<function<async::Task<void>(const EventData&)>> subscribers_;
        mutable shared_mutex subscribers_mutex_;
        
        bool buffering_enabled_{false};
        size_type buffer_size_{100};
        deque<EventData> event_buffer_;
        mutable mutex buffer_mutex_;
        
        async::Task<void> notify_subscribers(const EventData& event);
        void add_to_buffer(const EventData& event);
    };
    
    class EventChannelManager {
    public:
        static EventChannelManager& instance();
        
        shared_ptr<EventChannel> get_channel(string_view name);
        void remove_channel(string_view name);
        vector<string> get_channel_names() const;
        
        async::Future<void> publish_to_channel(string_view channel_name, const EventData& event);
        void subscribe_to_channel(string_view channel_name, function<async::Task<void>(const EventData&)> handler);
        
    private:
        hash_map<string, shared_ptr<EventChannel>> channels_;
        mutable shared_mutex channels_mutex_;
        
        EventChannelManager() = default;
    };
    
    template<typename T>
    void SseConnection::set_attribute(string_view name, T&& value) {
        unique_lock lock(attributes_mutex_);
        attributes_[string(name)] = std::forward<T>(value);
    }
    
    template<typename T>
    optional<T> SseConnection::get_attribute(string_view name) const {
        shared_lock lock(attributes_mutex_);
        auto it = attributes_.find(string(name));
        if (it != attributes_.end() && std::holds_alternative<T>(it->second)) {
            return std::get<T>(it->second);
        }
        return std::nullopt;
    }
    
    namespace helpers {
        inline EventData json_event(const string& json_data, optional<EventType> event_type = std::nullopt) {
            return EventData{event_type.value_or("json"), json_data};
        }
        
        inline EventData text_event(string_view text_data, optional<EventType> event_type = std::nullopt) {
            return EventData{event_type.value_or("message"), string(text_data)};
        }
        
        inline EventData heartbeat_event() {
            return EventData{"heartbeat", "ping"};
        }
        
        inline EventData error_event(string_view error_message) {
            return EventData{"error", string(error_message)};
        }
        
        template<typename T>
        EventData structured_event(const T& data, string_view event_type = "data") {
            // This would typically use a JSON serializer
            return EventData{string(event_type), ""}; // Placeholder
        }
    }
} 