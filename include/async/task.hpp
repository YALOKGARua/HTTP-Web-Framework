#pragma once

#include "core/types.hpp"
#include "async/future.hpp"
#include <coroutine>
#include <exception>

namespace http_framework::async {
    template<typename T = void>
    class Task {
    public:
        struct promise_type {
            Task get_return_object() {
                return Task{std::coroutine_handle<promise_type>::from_promise(*this)};
            }
            
            std::suspend_never initial_suspend() noexcept { return {}; }
            std::suspend_never final_suspend() noexcept { return {}; }
            
            void unhandled_exception() {
                exception_ = std::current_exception();
            }
            
            template<typename U>
            void return_value(U&& value) requires(!std::is_void_v<T>) {
                value_ = std::forward<U>(value);
            }
            
            void return_void() requires(std::is_void_v<T>) {}
            
            template<typename U>
            auto await_transform(U&& awaitable) {
                return std::forward<U>(awaitable);
            }
            
            template<typename U>
            auto await_transform(Future<U>&& future) {
                struct Awaiter {
                    Future<U> future_;
                    
                    bool await_ready() const noexcept {
                        return future_.is_ready();
                    }
                    
                    void await_suspend(std::coroutine_handle<> handle) {
                        std::thread([future = std::move(future_), handle]() mutable {
                            try {
                                future.wait();
                                handle.resume();
                            } catch (...) {
                                handle.resume();
                            }
                        }).detach();
                    }
                    
                    U await_resume() {
                        if constexpr (std::is_void_v<U>) {
                            future_.get();
                        } else {
                            return future_.get();
                        }
                    }
                };
                
                return Awaiter{std::move(future)};
            }
            
            template<typename U>
            auto await_transform(Task<U>&& task) {
                struct Awaiter {
                    Task<U> task_;
                    
                    bool await_ready() const noexcept {
                        return task_.is_ready();
                    }
                    
                    void await_suspend(std::coroutine_handle<> handle) {
                        task_.then([handle](auto&&...) mutable {
                            handle.resume();
                        });
                    }
                    
                    U await_resume() {
                        if constexpr (std::is_void_v<U>) {
                            task_.get();
                        } else {
                            return task_.get();
                        }
                    }
                };
                
                return Awaiter{std::move(task)};
            }
            
            optional<T> value_;
            std::exception_ptr exception_;
        };
        
        using handle_type = std::coroutine_handle<promise_type>;
        
        Task() = default;
        
        explicit Task(handle_type handle) : handle_(handle) {}
        
        Task(const Task&) = delete;
        Task& operator=(const Task&) = delete;
        
        Task(Task&& other) noexcept : handle_(std::exchange(other.handle_, {})) {}
        
        Task& operator=(Task&& other) noexcept {
            if (this != &other) {
                if (handle_) {
                    handle_.destroy();
                }
                handle_ = std::exchange(other.handle_, {});
            }
            return *this;
        }
        
        ~Task() {
            if (handle_) {
                handle_.destroy();
            }
        }
        
        bool is_ready() const noexcept {
            return handle_ && handle_.done();
        }
        
        bool is_valid() const noexcept {
            return handle_ != nullptr;
        }
        
        void wait() {
            if (!handle_) return;
            
            while (!handle_.done()) {
                std::this_thread::yield();
            }
        }
        
        template<typename Rep, typename Period>
        bool wait_for(const std::chrono::duration<Rep, Period>& timeout) {
            if (!handle_) return true;
            
            auto start = std::chrono::steady_clock::now();
            while (!handle_.done()) {
                if (std::chrono::steady_clock::now() - start > timeout) {
                    return false;
                }
                std::this_thread::yield();
            }
            return true;
        }
        
        template<typename Clock, typename Duration>
        bool wait_until(const std::chrono::time_point<Clock, Duration>& abs_time) {
            if (!handle_) return true;
            
            while (!handle_.done()) {
                if (std::chrono::steady_clock::now() > abs_time) {
                    return false;
                }
                std::this_thread::yield();
            }
            return true;
        }
        
        T get() {
            wait();
            
            if (!handle_) {
                throw std::runtime_error("Task is not valid");
            }
            
            auto& promise = handle_.promise();
            
            if (promise.exception_) {
                std::rethrow_exception(promise.exception_);
            }
            
            if constexpr (std::is_void_v<T>) {
                return;
            } else {
                if (!promise.value_) {
                    throw std::runtime_error("Task has no value");
                }
                return std::move(*promise.value_);
            }
        }
        
        template<typename F>
        auto then(F&& func) -> Task<std::invoke_result_t<F, T>> {
            using ReturnType = std::invoke_result_t<F, T>;
            
            if constexpr (std::is_void_v<T>) {
                co_await *this;
                if constexpr (std::is_void_v<ReturnType>) {
                    func();
                } else {
                    co_return func();
                }
            } else {
                auto result = co_await *this;
                if constexpr (std::is_void_v<ReturnType>) {
                    func(std::move(result));
                } else {
                    co_return func(std::move(result));
                }
            }
        }
        
        template<typename F>
        auto catch_exception(F&& func) -> Task<T> {
            try {
                if constexpr (std::is_void_v<T>) {
                    co_await *this;
                } else {
                    co_return co_await *this;
                }
            } catch (...) {
                if constexpr (std::is_void_v<T>) {
                    func(std::current_exception());
                } else {
                    co_return func(std::current_exception());
                }
            }
        }
        
        template<typename F>
        auto finally(F&& func) -> Task<T> {
            try {
                if constexpr (std::is_void_v<T>) {
                    co_await *this;
                    func();
                } else {
                    auto result = co_await *this;
                    func();
                    co_return result;
                }
            } catch (...) {
                func();
                throw;
            }
        }
        
        Future<T> to_future() {
            auto [promise, future] = Promise<T>::create_pair();
            
            std::thread([task = std::move(*this), promise = std::move(promise)]() mutable {
                try {
                    if constexpr (std::is_void_v<T>) {
                        task.get();
                        promise.set_value();
                    } else {
                        promise.set_value(task.get());
                    }
                } catch (...) {
                    promise.set_exception(std::current_exception());
                }
            }).detach();
            
            return future;
        }
        
        void detach() {
            if (handle_) {
                std::thread([handle = std::exchange(handle_, {})]() mutable {
                    Task task{handle};
                    try {
                        task.wait();
                    } catch (...) {
                    }
                }).detach();
            }
        }
        
        static Task<T> from_value(T value) requires(!std::is_void_v<T>) {
            co_return value;
        }
        
        static Task<void> from_void() requires(std::is_void_v<T>) {
            co_return;
        }
        
        static Task<T> from_exception(std::exception_ptr ex) {
            std::rethrow_exception(ex);
            co_return;
        }
        
        template<typename Exception>
        static Task<T> from_exception(Exception&& ex) {
            throw std::forward<Exception>(ex);
            co_return;
        }
        
        template<typename... Tasks>
        static Task<std::tuple<typename Tasks::value_type...>> when_all(Tasks&&... tasks) {
            co_return std::make_tuple((co_await tasks)...);
        }
        
        template<typename... Tasks>
        static Task<variant<typename Tasks::value_type...>> when_any(Tasks&&... tasks) {
            return when_any_impl(std::forward<Tasks>(tasks)...);
        }
        
        bool await_ready() const noexcept {
            return is_ready();
        }
        
        void await_suspend(std::coroutine_handle<> continuation) {
            if (handle_) {
                std::thread([handle = handle_, continuation]() mutable {
                    Task task{handle};
                    task.wait();
                    continuation.resume();
                }).detach();
            } else {
                continuation.resume();
            }
        }
        
        T await_resume() {
            return get();
        }
        
    private:
        handle_type handle_;
        
        template<typename... Tasks>
        static Task<variant<typename Tasks::value_type...>> when_any_impl(Tasks&&... tasks) {
            struct AnyAwaiter {
                std::tuple<Tasks...> tasks_;
                atomic<bool> completed_{false};
                variant<typename Tasks::value_type...> result_;
                std::exception_ptr exception_;
                
                bool await_ready() const noexcept {
                    return std::apply([](auto&&... t) {
                        return (t.is_ready() || ...);
                    }, tasks_);
                }
                
                void await_suspend(std::coroutine_handle<> handle) {
                    std::apply([&](auto&&... t) {
                        auto process_task = [&](auto&& task, auto index) {
                            std::thread([&task, &completed = completed_, &result = result_, 
                                       &exception = exception_, handle, index]() mutable {
                                try {
                                    if constexpr (std::is_void_v<typename std::decay_t<decltype(task)>::value_type>) {
                                        task.wait();
                                        if (!completed.exchange(true)) {
                                            result = index;
                                            handle.resume();
                                        }
                                    } else {
                                        auto value = task.get();
                                        if (!completed.exchange(true)) {
                                            result = std::move(value);
                                            handle.resume();
                                        }
                                    }
                                } catch (...) {
                                    if (!completed.exchange(true)) {
                                        exception = std::current_exception();
                                        handle.resume();
                                    }
                                }
                            }).detach();
                        };
                        
                        size_t index = 0;
                        (process_task(t, std::integral_constant<size_t, index++>{}), ...);
                    }, tasks_);
                }
                
                variant<typename Tasks::value_type...> await_resume() {
                    if (exception_) {
                        std::rethrow_exception(exception_);
                    }
                    return std::move(result_);
                }
            };
            
            co_return co_await AnyAwaiter{std::make_tuple(std::forward<Tasks>(tasks)...)};
        }
    };
    
    template<typename F>
    auto make_task(F&& func) -> Task<std::invoke_result_t<F>> {
        if constexpr (std::is_void_v<std::invoke_result_t<F>>) {
            func();
            co_return;
        } else {
            co_return func();
        }
    }
    
    template<typename F, typename... Args>
    auto make_task_with_args(F&& func, Args&&... args) -> Task<std::invoke_result_t<F, Args...>> {
        if constexpr (std::is_void_v<std::invoke_result_t<F, Args...>>) {
            func(std::forward<Args>(args)...);
            co_return;
        } else {
            co_return func(std::forward<Args>(args)...);
        }
    }
    
    template<typename T>
    Task<T> make_ready_task(T value) requires(!std::is_void_v<T>) {
        co_return value;
    }
    
    inline Task<void> make_ready_task() {
        co_return;
    }
    
    template<typename T, typename Exception>
    Task<T> make_exceptional_task(Exception&& ex) {
        throw std::forward<Exception>(ex);
        co_return;
    }
    
    template<typename Rep, typename Period>
    Task<void> sleep_for(const std::chrono::duration<Rep, Period>& duration) {
        struct SleepAwaiter {
            std::chrono::duration<Rep, Period> duration_;
            
            bool await_ready() const noexcept { return duration_.count() <= 0; }
            
            void await_suspend(std::coroutine_handle<> handle) {
                std::thread([duration = duration_, handle]() {
                    std::this_thread::sleep_for(duration);
                    handle.resume();
                }).detach();
            }
            
            void await_resume() const noexcept {}
        };
        
        co_await SleepAwaiter{duration};
    }
    
    template<typename Clock, typename Duration>
    Task<void> sleep_until(const std::chrono::time_point<Clock, Duration>& abs_time) {
        auto now = Clock::now();
        if (abs_time <= now) {
            co_return;
        }
        co_await sleep_for(abs_time - now);
    }
} 