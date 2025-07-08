#pragma once

#include "core/types.hpp"
#include "async/future.hpp"
#include <exception>

namespace http_framework::async {
    template<typename T>
    class Promise {
    public:
        Promise() : promise_(std::make_unique<std::promise<T>>()) {}
        
        Promise(const Promise&) = delete;
        Promise(Promise&&) = default;
        Promise& operator=(const Promise&) = delete;
        Promise& operator=(Promise&&) = default;
        ~Promise() = default;
        
        Future<T> get_future() {
            if (!promise_) {
                throw std::runtime_error("Promise has already provided a future");
            }
            return Future<T>(promise_->get_future());
        }
        
        void set_value(const T& value) {
            if (!promise_) {
                throw std::runtime_error("Promise is not valid");
            }
            try {
                promise_->set_value(value);
                promise_.reset();
            } catch (const std::future_error& e) {
                if (e.code() == std::future_errc::promise_already_satisfied) {
                    throw std::runtime_error("Promise already satisfied");
                }
                throw;
            }
        }
        
        void set_value(T&& value) {
            if (!promise_) {
                throw std::runtime_error("Promise is not valid");
            }
            try {
                promise_->set_value(std::move(value));
                promise_.reset();
            } catch (const std::future_error& e) {
                if (e.code() == std::future_errc::promise_already_satisfied) {
                    throw std::runtime_error("Promise already satisfied");
                }
                throw;
            }
        }
        
        void set_exception(std::exception_ptr ex) {
            if (!promise_) {
                throw std::runtime_error("Promise is not valid");
            }
            try {
                promise_->set_exception(ex);
                promise_.reset();
            } catch (const std::future_error& e) {
                if (e.code() == std::future_errc::promise_already_satisfied) {
                    throw std::runtime_error("Promise already satisfied");
                }
                throw;
            }
        }
        
        template<typename Exception>
        void set_exception(Exception&& ex) {
            set_exception(std::make_exception_ptr(std::forward<Exception>(ex)));
        }
        
        void set_value_at_thread_exit(const T& value) {
            if (!promise_) {
                throw std::runtime_error("Promise is not valid");
            }
            promise_->set_value_at_thread_exit(value);
        }
        
        void set_value_at_thread_exit(T&& value) {
            if (!promise_) {
                throw std::runtime_error("Promise is not valid");
            }
            promise_->set_value_at_thread_exit(std::move(value));
        }
        
        void set_exception_at_thread_exit(std::exception_ptr ex) {
            if (!promise_) {
                throw std::runtime_error("Promise is not valid");
            }
            promise_->set_exception_at_thread_exit(ex);
        }
        
        template<typename Exception>
        void set_exception_at_thread_exit(Exception&& ex) {
            set_exception_at_thread_exit(std::make_exception_ptr(std::forward<Exception>(ex)));
        }
        
        bool valid() const noexcept {
            return promise_ != nullptr;
        }
        
        void reset() {
            promise_ = std::make_unique<std::promise<T>>();
        }
        
        template<typename F>
        void fulfill_with(F&& func) {
            try {
                if constexpr (std::is_void_v<std::invoke_result_t<F>>) {
                    func();
                    if constexpr (std::is_void_v<T>) {
                        set_value();
                    } else {
                        throw std::runtime_error("Cannot set void value to non-void promise");
                    }
                } else {
                    set_value(func());
                }
            } catch (...) {
                set_exception(std::current_exception());
            }
        }
        
        template<typename F, typename... Args>
        void fulfill_with_args(F&& func, Args&&... args) {
            fulfill_with([&] { return func(std::forward<Args>(args)...); });
        }
        
        static std::pair<Promise<T>, Future<T>> create_pair() {
            Promise<T> promise;
            Future<T> future = promise.get_future();
            return std::make_pair(std::move(promise), std::move(future));
        }
        
    private:
        unique_ptr<std::promise<T>> promise_;
    };
    
    template<>
    class Promise<void> {
    public:
        Promise() : promise_(std::make_unique<std::promise<void>>()) {}
        
        Promise(const Promise&) = delete;
        Promise(Promise&&) = default;
        Promise& operator=(const Promise&) = delete;
        Promise& operator=(Promise&&) = default;
        ~Promise() = default;
        
        Future<void> get_future() {
            if (!promise_) {
                throw std::runtime_error("Promise has already provided a future");
            }
            return Future<void>(promise_->get_future());
        }
        
        void set_value() {
            if (!promise_) {
                throw std::runtime_error("Promise is not valid");
            }
            try {
                promise_->set_value();
                promise_.reset();
            } catch (const std::future_error& e) {
                if (e.code() == std::future_errc::promise_already_satisfied) {
                    throw std::runtime_error("Promise already satisfied");
                }
                throw;
            }
        }
        
        void set_exception(std::exception_ptr ex) {
            if (!promise_) {
                throw std::runtime_error("Promise is not valid");
            }
            try {
                promise_->set_exception(ex);
                promise_.reset();
            } catch (const std::future_error& e) {
                if (e.code() == std::future_errc::promise_already_satisfied) {
                    throw std::runtime_error("Promise already satisfied");
                }
                throw;
            }
        }
        
        template<typename Exception>
        void set_exception(Exception&& ex) {
            set_exception(std::make_exception_ptr(std::forward<Exception>(ex)));
        }
        
        void set_value_at_thread_exit() {
            if (!promise_) {
                throw std::runtime_error("Promise is not valid");
            }
            promise_->set_value_at_thread_exit();
        }
        
        void set_exception_at_thread_exit(std::exception_ptr ex) {
            if (!promise_) {
                throw std::runtime_error("Promise is not valid");
            }
            promise_->set_exception_at_thread_exit(ex);
        }
        
        template<typename Exception>
        void set_exception_at_thread_exit(Exception&& ex) {
            set_exception_at_thread_exit(std::make_exception_ptr(std::forward<Exception>(ex)));
        }
        
        bool valid() const noexcept {
            return promise_ != nullptr;
        }
        
        void reset() {
            promise_ = std::make_unique<std::promise<void>>();
        }
        
        template<typename F>
        void fulfill_with(F&& func) {
            try {
                func();
                set_value();
            } catch (...) {
                set_exception(std::current_exception());
            }
        }
        
        template<typename F, typename... Args>
        void fulfill_with_args(F&& func, Args&&... args) {
            fulfill_with([&] { func(std::forward<Args>(args)...); });
        }
        
        static std::pair<Promise<void>, Future<void>> create_pair() {
            Promise<void> promise;
            Future<void> future = promise.get_future();
            return std::make_pair(std::move(promise), std::move(future));
        }
        
    private:
        unique_ptr<std::promise<void>> promise_;
    };
    
    template<typename T>
    std::pair<Promise<T>, Future<T>> make_promise_future_pair() {
        return Promise<T>::create_pair();
    }
    
    template<typename T, typename F>
    Future<T> async_execute(F&& func) {
        auto [promise, future] = make_promise_future_pair<T>();
        
        std::thread([promise = std::move(promise), func = std::forward<F>(func)]() mutable {
            promise.fulfill_with(std::move(func));
        }).detach();
        
        return future;
    }
    
    template<typename F>
    auto async_execute(F&& func) -> Future<std::invoke_result_t<F>> {
        using ReturnType = std::invoke_result_t<F>;
        return async_execute<ReturnType>(std::forward<F>(func));
    }
    
    template<typename T, typename F, typename... Args>
    Future<T> async_execute_with_args(F&& func, Args&&... args) {
        auto [promise, future] = make_promise_future_pair<T>();
        
        std::thread([promise = std::move(promise), func = std::forward<F>(func), 
                    args = std::make_tuple(std::forward<Args>(args)...)]() mutable {
            std::apply([&](auto&&... a) {
                promise.fulfill_with_args(std::move(func), std::forward<decltype(a)>(a)...);
            }, std::move(args));
        }).detach();
        
        return future;
    }
    
    template<typename F, typename... Args>
    auto async_execute_with_args(F&& func, Args&&... args) -> Future<std::invoke_result_t<F, Args...>> {
        using ReturnType = std::invoke_result_t<F, Args...>;
        return async_execute_with_args<ReturnType>(std::forward<F>(func), std::forward<Args>(args)...);
    }
} 