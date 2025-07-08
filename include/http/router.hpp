#pragma once

#include "core/types.hpp"
#include "http/request.hpp"
#include "http/response.hpp"
#include "http/middleware.hpp"
#include <regex>

namespace http_framework::http {
    class Router;
    
    using HandlerFunction = function<void(const Request&, Response&)>;
    using AsyncHandlerFunction = function<async::Task<void>(const Request&, Response&)>;
    using ErrorHandlerFunction = function<void(const Request&, Response&, const std::exception&)>;
    using NotFoundHandlerFunction = function<void(const Request&, Response&)>;
    
    struct Route {
        HttpMethod method;
        std::regex pattern;
        vector<string> parameter_names;
        HandlerFunction handler;
        optional<AsyncHandlerFunction> async_handler;
        vector<shared_ptr<Middleware>> middlewares;
        string name;
        hash_map<string, string> constraints;
        bool is_websocket{false};
        int priority{0};
        
        Route() = default;
        Route(HttpMethod m, const std::regex& p, HandlerFunction h)
            : method(m), pattern(p), handler(std::move(h)) {}
        Route(HttpMethod m, const std::regex& p, AsyncHandlerFunction h)
            : method(m), pattern(p), async_handler(std::move(h)) {}
        
        bool matches(const Request& request) const;
        hash_map<string, string> extract_parameters(const Request& request) const;
        bool satisfies_constraints(const hash_map<string, string>& params) const;
    };
    
    struct RouteGroup {
        string prefix;
        vector<shared_ptr<Middleware>> middlewares;
        vector<Route> routes;
        vector<unique_ptr<RouteGroup>> subgroups;
        hash_map<string, string> default_constraints;
        
        RouteGroup() = default;
        RouteGroup(string_view p) : prefix(p) {}
    };
    
    class Router {
    public:
        Router() = default;
        Router(const Router&) = delete;
        Router(Router&&) = default;
        Router& operator=(const Router&) = delete;
        Router& operator=(Router&&) = default;
        ~Router() = default;
        
        Router& get(string_view pattern, HandlerFunction handler);
        Router& post(string_view pattern, HandlerFunction handler);
        Router& put(string_view pattern, HandlerFunction handler);
        Router& patch(string_view pattern, HandlerFunction handler);
        Router& delete_(string_view pattern, HandlerFunction handler);
        Router& head(string_view pattern, HandlerFunction handler);
        Router& options(string_view pattern, HandlerFunction handler);
        Router& trace(string_view pattern, HandlerFunction handler);
        Router& connect(string_view pattern, HandlerFunction handler);
        
        Router& get_async(string_view pattern, AsyncHandlerFunction handler);
        Router& post_async(string_view pattern, AsyncHandlerFunction handler);
        Router& put_async(string_view pattern, AsyncHandlerFunction handler);
        Router& patch_async(string_view pattern, AsyncHandlerFunction handler);
        Router& delete_async(string_view pattern, AsyncHandlerFunction handler);
        Router& head_async(string_view pattern, AsyncHandlerFunction handler);
        Router& options_async(string_view pattern, AsyncHandlerFunction handler);
        Router& trace_async(string_view pattern, AsyncHandlerFunction handler);
        Router& connect_async(string_view pattern, AsyncHandlerFunction handler);
        
        Router& route(HttpMethod method, string_view pattern, HandlerFunction handler);
        Router& route_async(HttpMethod method, string_view pattern, AsyncHandlerFunction handler);
        
        Router& websocket(string_view pattern, HandlerFunction handler);
        Router& websocket_async(string_view pattern, AsyncHandlerFunction handler);
        
        Router& middleware(shared_ptr<Middleware> middleware);
        Router& middleware(function<void(const Request&, Response&, function<void()>)> handler);
        
        Router& group(string_view prefix, function<void(Router&)> config);
        Router& subdomain(string_view subdomain, function<void(Router&)> config);
        
        Router& name(string_view route_name);
        Router& where(string_view parameter, string_view constraint);
        Router& where(const hash_map<string, string>& constraints);
        Router& priority(int p);
        
        Router& namespace_(string_view ns);
        Router& prefix(string_view prefix);
        Router& domain(string_view domain);
        
        Router& fallback(HandlerFunction handler);
        Router& fallback_async(AsyncHandlerFunction handler);
        
        Router& error_handler(ErrorHandlerFunction handler);
        Router& not_found_handler(NotFoundHandlerFunction handler);
        
        Router& resource(string_view name, string_view controller);
        Router& resources(string_view name, string_view controller);
        
        Router& redirect(string_view from, string_view to, status_code_t code = 302);
        Router& permanent_redirect(string_view from, string_view to);
        Router& view(string_view pattern, string_view template_name);
        
        bool handle_request(const Request& request, Response& response) const;
        async::Task<bool> handle_request_async(const Request& request, Response& response) const;
        
        optional<Route> find_route(const Request& request) const;
        vector<Route> get_routes_for_method(HttpMethod method) const;
        vector<Route> get_all_routes() const;
        
        string generate_url(string_view route_name, const hash_map<string, string>& parameters = {}) const;
        bool has_route(string_view route_name) const;
        
        void clear_routes();
        void clear_middlewares();
        void clear_groups();
        void clear_all();
        
        size_type route_count() const;
        size_type middleware_count() const;
        size_type group_count() const;
        
        void print_routes() const;
        string routes_to_string() const;
        
        Router& bind(string_view key, const variant<string, int, double, bool>& value);
        optional<variant<string, int, double, bool>> get_binding(string_view key) const;
        
        Router& cache_routes(bool enable = true);
        Router& optimize_routes();
        Router& compile_patterns();
        
        template<typename T>
        Router& controller(string_view pattern, T* controller);
        
        template<typename T>
        Router& resource_controller(string_view name, T* controller);
        
    private:
        vector<Route> routes_;
        vector<shared_ptr<Middleware>> global_middlewares_;
        vector<unique_ptr<RouteGroup>> groups_;
        optional<ErrorHandlerFunction> error_handler_;
        optional<NotFoundHandlerFunction> not_found_handler_;
        optional<HandlerFunction> fallback_handler_;
        optional<AsyncHandlerFunction> fallback_async_handler_;
        hash_map<string, Route*> named_routes_;
        hash_map<string, variant<string, int, double, bool>> bindings_;
        
        string current_prefix_;
        string current_namespace_;
        string current_domain_;
        vector<shared_ptr<Middleware>> current_middlewares_;
        hash_map<string, string> current_constraints_;
        int current_priority_{0};
        string current_name_;
        
        bool routes_cached_{false};
        bool patterns_compiled_{false};
        
        std::regex compile_pattern(string_view pattern, vector<string>& parameter_names) const;
        string pattern_to_regex(string_view pattern) const;
        vector<string> extract_parameter_names(string_view pattern) const;
        bool is_valid_parameter_name(string_view name) const;
        
        void add_route(Route route);
        void apply_current_settings(Route& route) const;
        void register_named_route(const Route& route);
        
        Route* find_mutable_route(const Request& request);
        bool execute_middlewares(const vector<shared_ptr<Middleware>>& middlewares, 
                                const Request& request, Response& response) const;
        
        async::Task<bool> execute_middlewares_async(const vector<shared_ptr<Middleware>>& middlewares,
                                                   const Request& request, Response& response) const;
        
        void handle_error(const Request& request, Response& response, const std::exception& e) const;
        void handle_not_found(const Request& request, Response& response) const;
        
        string build_full_pattern(string_view pattern) const;
        void validate_route_configuration(const Route& route) const;
        
        static string escape_regex_special_chars(string_view str);
        static bool is_regex_pattern(string_view pattern);
        static HttpMethod string_to_method(string_view method);
        static string method_to_string(HttpMethod method);
    };
    
    template<typename T>
    Router& Router::controller(string_view pattern, T* controller) {
        static_assert(std::is_class_v<T>, "Controller must be a class type");
        
        if constexpr (requires { controller->index(); }) {
            get(string(pattern), [controller](const Request& req, Response& res) {
                controller->index(req, res);
            });
        }
        
        if constexpr (requires { controller->show(); }) {
            get(string(pattern) + "/{id}", [controller](const Request& req, Response& res) {
                controller->show(req, res);
            });
        }
        
        if constexpr (requires { controller->create(); }) {
            post(string(pattern), [controller](const Request& req, Response& res) {
                controller->create(req, res);
            });
        }
        
        if constexpr (requires { controller->update(); }) {
            put(string(pattern) + "/{id}", [controller](const Request& req, Response& res) {
                controller->update(req, res);
            });
        }
        
        if constexpr (requires { controller->destroy(); }) {
            delete_(string(pattern) + "/{id}", [controller](const Request& req, Response& res) {
                controller->destroy(req, res);
            });
        }
        
        return *this;
    }
    
    template<typename T>
    Router& Router::resource_controller(string_view name, T* controller) {
        return controller(name, controller);
    }
} 