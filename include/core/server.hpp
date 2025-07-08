#pragma once

#include "core/types.hpp"
#include "core/connection.hpp"
#include "core/thread_pool.hpp"
#include "core/event_loop.hpp"
#include "http/router.hpp"
#include "http/middleware.hpp"
#include "utils/config.hpp"
#include "utils/logger.hpp"
#include "network/socket.hpp"

namespace http_framework::core {
    class Server {
    public:
        struct Config {
            string host{"0.0.0.0"};
            port_t port{8080};
            size_type max_connections{1000};
            size_type thread_pool_size{std::thread::hardware_concurrency()};
            duration_t connection_timeout{std::chrono::seconds(30)};
            duration_t keep_alive_timeout{std::chrono::seconds(5)};
            size_type max_request_size{1024 * 1024};
            size_type max_header_size{8192};
            bool reuse_address{true};
            bool tcp_no_delay{true};
            bool enable_keep_alive{true};
            bool enable_compression{true};
            bool enable_websocket{true};
            string server_name{"HttpFramework/1.0"};
            LogLevel log_level{LogLevel::INFO};
            optional<string> ssl_cert_path;
            optional<string> ssl_key_path;
            bool enable_ssl{false};
        };
        
        Server();
        explicit Server(Config config);
        Server(const Server&) = delete;
        Server(Server&&) = default;
        Server& operator=(const Server&) = delete;
        Server& operator=(Server&&) = default;
        ~Server();
        
        void configure(const Config& config);
        void configure(string_view config_file);
        const Config& get_config() const noexcept { return config_; }
        
        void set_router(shared_ptr<http::Router> router);
        shared_ptr<http::Router> get_router() const { return router_; }
        
        void add_middleware(shared_ptr<http::Middleware> middleware);
        void add_middleware(function<void(const http::Request&, http::Response&, function<void()>)> handler);
        void clear_middlewares();
        
        void set_error_handler(function<void(const http::Request&, http::Response&, const std::exception&)> handler);
        void set_not_found_handler(function<void(const http::Request&, http::Response&)> handler);
        
        bool start();
        bool start(string_view host, port_t port);
        void stop();
        bool is_running() const noexcept { return running_; }
        
        void run();
        void run_async();
        void wait_for_shutdown();
        
        size_type active_connections() const;
        size_type total_requests() const noexcept { return total_requests_; }
        size_type failed_requests() const noexcept { return failed_requests_; }
        duration_t uptime() const;
        
        void enable_graceful_shutdown(duration_t timeout = std::chrono::seconds(30));
        void disable_graceful_shutdown();
        
        void set_signal_handlers();
        void handle_signal(int signal);
        
        bool bind_socket();
        bool listen_socket();
        void accept_connections();
        void handle_connection(shared_ptr<Connection> connection);
        
        async::Task<void> handle_connection_async(shared_ptr<Connection> connection);
        
        void process_request(shared_ptr<Connection> connection, const http::Request& request);
        async::Task<void> process_request_async(shared_ptr<Connection> connection, const http::Request& request);
        
        void send_response(shared_ptr<Connection> connection, const http::Response& response);
        async::Task<void> send_response_async(shared_ptr<Connection> connection, const http::Response& response);
        
        void handle_websocket_upgrade(shared_ptr<Connection> connection, const http::Request& request);
        async::Task<void> handle_websocket_upgrade_async(shared_ptr<Connection> connection, const http::Request& request);
        
        void cleanup_connections();
        void close_connection(shared_ptr<Connection> connection);
        
        void set_custom_header(string_view name, string_view value);
        void remove_custom_header(string_view name);
        void clear_custom_headers();
        
        void enable_cors(string_view origin = "*");
        void disable_cors();
        
        void set_rate_limit(size_type requests_per_minute);
        void disable_rate_limit();
        
        void enable_request_logging(bool enable = true);
        void enable_access_logging(string_view log_file);
        void disable_access_logging();
        
        void set_static_file_handler(string_view path, string_view root_dir);
        void remove_static_file_handler(string_view path);
        void clear_static_file_handlers();
        
        void enable_health_check(string_view endpoint = "/health");
        void disable_health_check();
        
        void enable_metrics(string_view endpoint = "/metrics");
        void disable_metrics();
        
        string get_server_info() const;
        hash_map<string, variant<string, int64_t, double, bool>> get_stats() const;
        
        void register_shutdown_handler(function<void()> handler);
        void register_startup_handler(function<void()> handler);
        
        template<typename T>
        void register_service(shared_ptr<T> service);
        
        template<typename T>
        shared_ptr<T> get_service() const;
        
        void schedule_task(function<void()> task, duration_t delay = duration_t::zero());
        void schedule_periodic_task(function<void()> task, duration_t interval);
        void cancel_scheduled_tasks();
        
        bool load_ssl_certificates(string_view cert_path, string_view key_path);
        void enable_ssl_verification(bool enable = true);
        void set_ssl_protocols(const vector<string>& protocols);
        
        void set_connection_idle_timeout(duration_t timeout);
        void set_request_timeout(duration_t timeout);
        void set_response_timeout(duration_t timeout);
        
        void set_max_concurrent_requests(size_type max_requests);
        void set_max_request_body_size(size_type max_size);
        void set_max_header_count(size_type max_headers);
        
        void enable_request_tracing(bool enable = true);
        void enable_performance_monitoring(bool enable = true);
        
        void add_virtual_host(string_view host, shared_ptr<http::Router> router);
        void remove_virtual_host(string_view host);
        void clear_virtual_hosts();
        
    private:
        Config config_;
        atomic<bool> running_{false};
        atomic<bool> shutdown_requested_{false};
        
        unique_ptr<network::Socket> listen_socket_;
        unique_ptr<ThreadPool> thread_pool_;
        unique_ptr<EventLoop> event_loop_;
        shared_ptr<http::Router> router_;
        http::MiddlewareChain middleware_chain_;
        
        vector<shared_ptr<Connection>> connections_;
        mutable shared_mutex connections_mutex_;
        
        atomic<size_type> total_requests_{0};
        atomic<size_type> failed_requests_{0};
        timestamp_t start_time_;
        
        optional<function<void(const http::Request&, http::Response&, const std::exception&)>> error_handler_;
        optional<function<void(const http::Request&, http::Response&)>> not_found_handler_;
        
        vector<function<void()>> startup_handlers_;
        vector<function<void()>> shutdown_handlers_;
        
        hash_map<string, variant<string, int64_t, double, bool>> custom_headers_;
        hash_map<string, shared_ptr<http::Router>> virtual_hosts_;
        hash_map<string, string> static_file_handlers_;
        hash_map<std::type_index, shared_ptr<void>> services_;
        
        bool graceful_shutdown_enabled_{false};
        duration_t graceful_shutdown_timeout_{std::chrono::seconds(30)};
        
        bool cors_enabled_{false};
        string cors_origin_{"*"};
        
        optional<size_type> rate_limit_;
        hash_map<string, std::pair<size_type, timestamp_t>> rate_limit_counters_;
        mutable mutex rate_limit_mutex_;
        
        bool request_logging_enabled_{false};
        bool access_logging_enabled_{false};
        optional<string> access_log_file_;
        
        optional<string> health_check_endpoint_;
        optional<string> metrics_endpoint_;
        
        bool ssl_enabled_{false};
        bool ssl_verification_enabled_{true};
        vector<string> ssl_protocols_;
        
        duration_t connection_idle_timeout_{std::chrono::seconds(30)};
        duration_t request_timeout_{std::chrono::seconds(10)};
        duration_t response_timeout_{std::chrono::seconds(10)};
        
        size_type max_concurrent_requests_{1000};
        size_type max_request_body_size_{1024 * 1024};
        size_type max_header_count_{100};
        
        bool request_tracing_enabled_{false};
        bool performance_monitoring_enabled_{false};
        
        atomic<size_type> active_request_count_{0};
        
        void initialize_components();
        void cleanup_components();
        
        bool validate_config() const;
        void apply_socket_options();
        
        void handle_startup();
        void handle_shutdown();
        
        bool is_rate_limited(const string& client_ip);
        void update_rate_limit_counter(const string& client_ip);
        void cleanup_rate_limit_counters();
        
        void log_request(const http::Request& request, const http::Response& response, duration_t processing_time);
        void log_access(const http::Request& request, const http::Response& response);
        
        void handle_health_check(const http::Request& request, http::Response& response);
        void handle_metrics(const http::Request& request, http::Response& response);
        
        shared_ptr<http::Router> get_router_for_host(string_view host) const;
        
        void monitor_connections();
        void cleanup_idle_connections();
        
        string generate_request_id() const;
        void trace_request(const http::Request& request, string_view event);
        
        void collect_performance_metrics(const http::Request& request, const http::Response& response, duration_t processing_time);
        
        static void signal_handler(int signal);
        static Server* instance_;
    };
    
    template<typename T>
    void Server::register_service(shared_ptr<T> service) {
        services_[std::type_index(typeid(T))] = std::static_pointer_cast<void>(service);
    }
    
    template<typename T>
    shared_ptr<T> Server::get_service() const {
        auto it = services_.find(std::type_index(typeid(T)));
        if (it != services_.end()) {
            return std::static_pointer_cast<T>(it->second);
        }
        return nullptr;
    }
} 