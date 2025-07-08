#pragma once

#include "core/types.hpp"

namespace http_framework::http {
    class Request;
    class Response;
    
    class Middleware {
    public:
        virtual ~Middleware() = default;
        
        virtual void handle(const Request& request, Response& response, function<void()> next) = 0;
        virtual async::Task<void> handle_async(const Request& request, Response& response, function<async::Task<void>()> next);
        
        virtual string name() const = 0;
        virtual int priority() const { return 0; }
        virtual bool is_async() const { return false; }
        
        virtual void configure(const hash_map<string, variant<string, int, double, bool>>& config) {}
        virtual hash_map<string, variant<string, int, double, bool>> get_config() const { return {}; }
        
        virtual bool should_execute(const Request& request) const { return true; }
        virtual void on_error(const Request& request, Response& response, const std::exception& error) {}
        
    protected:
        void skip_remaining_middlewares(Response& response);
        bool is_middleware_execution_stopped(const Response& response) const;
    };
    
    class MiddlewareChain {
    public:
        MiddlewareChain() = default;
        MiddlewareChain(const MiddlewareChain&) = delete;
        MiddlewareChain(MiddlewareChain&&) = default;
        MiddlewareChain& operator=(const MiddlewareChain&) = delete;
        MiddlewareChain& operator=(MiddlewareChain&&) = default;
        ~MiddlewareChain() = default;
        
        void add(shared_ptr<Middleware> middleware);
        void add(function<void(const Request&, Response&, function<void()>)> handler);
        void add_async(function<async::Task<void>(const Request&, Response&, function<async::Task<void>()>)> handler);
        
        void insert(size_type index, shared_ptr<Middleware> middleware);
        void remove(string_view name);
        void remove_at(size_type index);
        void clear();
        
        bool execute(const Request& request, Response& response, function<void()> final_handler);
        async::Task<bool> execute_async(const Request& request, Response& response, function<async::Task<void>()> final_handler);
        
        size_type size() const noexcept { return middlewares_.size(); }
        bool empty() const noexcept { return middlewares_.empty(); }
        
        shared_ptr<Middleware> get(string_view name) const;
        shared_ptr<Middleware> get_at(size_type index) const;
        
        vector<shared_ptr<Middleware>> get_all() const { return middlewares_; }
        vector<string> get_names() const;
        
        void sort_by_priority();
        void reverse();
        
    private:
        vector<shared_ptr<Middleware>> middlewares_;
        
        void execute_next(size_type index, const Request& request, Response& response, function<void()> final_handler);
        async::Task<void> execute_next_async(size_type index, const Request& request, Response& response, 
                                           function<async::Task<void>()> final_handler);
    };
    
    template<typename Handler>
    class LambdaMiddleware : public Middleware {
    public:
        explicit LambdaMiddleware(Handler handler, string name = "lambda")
            : handler_(std::move(handler)), name_(std::move(name)) {}
        
        void handle(const Request& request, Response& response, function<void()> next) override {
            if constexpr (std::is_invocable_v<Handler, const Request&, Response&, function<void()>>) {
                handler_(request, response, std::move(next));
            } else {
                handler_(request, response);
                next();
            }
        }
        
        string name() const override { return name_; }
        
    private:
        Handler handler_;
        string name_;
    };
    
    template<typename AsyncHandler>
    class AsyncLambdaMiddleware : public Middleware {
    public:
        explicit AsyncLambdaMiddleware(AsyncHandler handler, string name = "async_lambda")
            : handler_(std::move(handler)), name_(std::move(name)) {}
        
        void handle(const Request& request, Response& response, function<void()> next) override {
            auto task = handle_async(request, response, [next = std::move(next)]() -> async::Task<void> {
                next();
                co_return;
            });
            task.wait();
        }
        
        async::Task<void> handle_async(const Request& request, Response& response, 
                                     function<async::Task<void>()> next) override {
            if constexpr (std::is_invocable_v<AsyncHandler, const Request&, Response&, function<async::Task<void>()>>) {
                co_await handler_(request, response, std::move(next));
            } else {
                co_await handler_(request, response);
                co_await next();
            }
        }
        
        string name() const override { return name_; }
        bool is_async() const override { return true; }
        
    private:
        AsyncHandler handler_;
        string name_;
    };
    
    class ConditionalMiddleware : public Middleware {
    public:
        ConditionalMiddleware(shared_ptr<Middleware> middleware, function<bool(const Request&)> condition)
            : middleware_(std::move(middleware)), condition_(std::move(condition)) {}
        
        void handle(const Request& request, Response& response, function<void()> next) override {
            if (condition_(request)) {
                middleware_->handle(request, response, std::move(next));
            } else {
                next();
            }
        }
        
        async::Task<void> handle_async(const Request& request, Response& response, 
                                     function<async::Task<void>()> next) override {
            if (condition_(request)) {
                co_await middleware_->handle_async(request, response, std::move(next));
            } else {
                co_await next();
            }
        }
        
        string name() const override { return "conditional(" + middleware_->name() + ")"; }
        bool is_async() const override { return middleware_->is_async(); }
        
    private:
        shared_ptr<Middleware> middleware_;
        function<bool(const Request&)> condition_;
    };
    
    template<typename Handler>
    shared_ptr<Middleware> make_middleware(Handler&& handler, string name = "lambda") {
        return std::make_shared<LambdaMiddleware<std::decay_t<Handler>>>(
            std::forward<Handler>(handler), std::move(name));
    }
    
    template<typename AsyncHandler>
    shared_ptr<Middleware> make_async_middleware(AsyncHandler&& handler, string name = "async_lambda") {
        return std::make_shared<AsyncLambdaMiddleware<std::decay_t<AsyncHandler>>>(
            std::forward<AsyncHandler>(handler), std::move(name));
    }
    
    shared_ptr<Middleware> make_conditional_middleware(shared_ptr<Middleware> middleware, 
                                                     function<bool(const Request&)> condition);
} 