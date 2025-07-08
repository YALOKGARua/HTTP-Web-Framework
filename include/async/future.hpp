#pragma once

#include "core/types.hpp"
#include <exception>

namespace http_framework::async {
    enum class FutureStatus {
        READY,
        TIMEOUT,
        DEFERRED
    };
    
    template<typename T>
    class Future {
    public:
        Future() = default;
        Future(const Future&) = delete;
        Future(Future&&) = default;
        Future& operator=(const Future&) = delete;
        Future& operator=(Future&&) = default;
        ~Future() = default;
        
        explicit Future(std::future<T> future) : future_(std::move(future)) {}
        
        T get() {
            if (future_.valid()) {
                return future_.get();
            }
            throw std::runtime_error("Future is not valid");
        }
        
        bool valid() const noexcept {
            return future_.valid();
        }
        
        void wait() const {
            if (future_.valid()) {
                future_.wait();
            }
        }
        
        template<typename Rep, typename Period>
        FutureStatus wait_for(const std::chrono::duration<Rep, Period>& timeout) const {
            if (!future_.valid()) {
                return FutureStatus::DEFERRED;
            }
            
            auto status = future_.wait_for(timeout);
            switch (status) {
                case std::future_status::ready:
                    return FutureStatus::READY;
                case std::future_status::timeout:
                    return FutureStatus::TIMEOUT;
                case std::future_status::deferred:
                    return FutureStatus::DEFERRED;
            }
            return FutureStatus::TIMEOUT;
        }
        
        template<typename Clock, typename Duration>
        FutureStatus wait_until(const std::chrono::time_point<Clock, Duration>& abs_time) const {
            if (!future_.valid()) {
                return FutureStatus::DEFERRED;
            }
            
            auto status = future_.wait_until(abs_time);
            switch (status) {
                case std::future_status::ready:
                    return FutureStatus::READY;
                case std::future_status::timeout:
                    return FutureStatus::TIMEOUT;
                case std::future_status::deferred:
                    return FutureStatus::DEFERRED;
            }
            return FutureStatus::TIMEOUT;
        }
        
        bool is_ready() const {
            return wait_for(std::chrono::seconds(0)) == FutureStatus::READY;
        }
        
        template<typename F>
        auto then(F&& func) -> Future<std::invoke_result_t<F, T>>;
        
        template<typename F>
        auto catch_exception(F&& func) -> Future<T>;
        
        template<typename F>
        auto finally(F&& func) -> Future<T>;
        
        Future<T> timeout(duration_t timeout_duration);
        
        static Future<T> make_ready(T value);
        static Future<T> make_exception(std::exception_ptr ex);
        
        template<typename... Futures>
        static auto all(Futures&&... futures) -> Future<std::tuple<typename Futures::value_type...>>;
        
        template<typename... Futures>
        static auto any(Futures&&... futures) -> Future<variant<typename Futures::value_type...>>;
        
        template<typename Iterator>
        static auto all_of(Iterator begin, Iterator end) -> Future<vector<T>>;
        
        template<typename Iterator>
        static auto any_of(Iterator begin, Iterator end) -> Future<T>;
        
        shared_ptr<std::shared_future<T>> share();
        
    private:
        mutable std::future<T> future_;
        
        template<typename U>
        friend class Promise;
    };
    
    template<>
    class Future<void> {
    public:
        Future() = default;
        Future(const Future&) = delete;
        Future(Future&&) = default;
        Future& operator=(const Future&) = delete;
        Future& operator=(Future&&) = default;
        ~Future() = default;
        
        explicit Future(std::future<void> future) : future_(std::move(future)) {}
        
        void get() {
            if (future_.valid()) {
                future_.get();
            } else {
                throw std::runtime_error("Future is not valid");
            }
        }
        
        bool valid() const noexcept {
            return future_.valid();
        }
        
        void wait() const {
            if (future_.valid()) {
                future_.wait();
            }
        }
        
        template<typename Rep, typename Period>
        FutureStatus wait_for(const std::chrono::duration<Rep, Period>& timeout) const {
            if (!future_.valid()) {
                return FutureStatus::DEFERRED;
            }
            
            auto status = future_.wait_for(timeout);
            switch (status) {
                case std::future_status::ready:
                    return FutureStatus::READY;
                case std::future_status::timeout:
                    return FutureStatus::TIMEOUT;
                case std::future_status::deferred:
                    return FutureStatus::DEFERRED;
            }
            return FutureStatus::TIMEOUT;
        }
        
        template<typename Clock, typename Duration>
        FutureStatus wait_until(const std::chrono::time_point<Clock, Duration>& abs_time) const {
            if (!future_.valid()) {
                return FutureStatus::DEFERRED;
            }
            
            auto status = future_.wait_until(abs_time);
            switch (status) {
                case std::future_status::ready:
                    return FutureStatus::READY;
                case std::future_status::timeout:
                    return FutureStatus::TIMEOUT;
                case std::future_status::deferred:
                    return FutureStatus::DEFERRED;
            }
            return FutureStatus::TIMEOUT;
        }
        
        bool is_ready() const {
            return wait_for(std::chrono::seconds(0)) == FutureStatus::READY;
        }
        
        template<typename F>
        auto then(F&& func) -> Future<std::invoke_result_t<F>>;
        
        template<typename F>
        auto catch_exception(F&& func) -> Future<void>;
        
        template<typename F>
        auto finally(F&& func) -> Future<void>;
        
        Future<void> timeout(duration_t timeout_duration);
        
        static Future<void> make_ready();
        static Future<void> make_exception(std::exception_ptr ex);
        
        shared_ptr<std::shared_future<void>> share();
        
    private:
        mutable std::future<void> future_;
        
        template<typename U>
        friend class Promise;
    };
    
    template<typename T>
    template<typename F>
    auto Future<T>::then(F&& func) -> Future<std::invoke_result_t<F, T>> {
        using ReturnType = std::invoke_result_t<F, T>;
        
        auto promise = std::make_shared<std::promise<ReturnType>>();
        auto future = promise->get_future();
        
        std::thread([future = std::move(future_), func = std::forward<F>(func), promise]() mutable {
            try {
                if constexpr (std::is_void_v<ReturnType>) {
                    func(future.get());
                    promise->set_value();
                } else {
                    promise->set_value(func(future.get()));
                }
            } catch (...) {
                promise->set_exception(std::current_exception());
            }
        }).detach();
        
        return Future<ReturnType>(std::move(future));
    }
    
    template<typename T>
    template<typename F>
    auto Future<T>::catch_exception(F&& func) -> Future<T> {
        auto promise = std::make_shared<std::promise<T>>();
        auto future = promise->get_future();
        
        std::thread([future = std::move(future_), func = std::forward<F>(func), promise]() mutable {
            try {
                if constexpr (std::is_void_v<T>) {
                    future.get();
                    promise->set_value();
                } else {
                    promise->set_value(future.get());
                }
            } catch (...) {
                try {
                    if constexpr (std::is_void_v<T>) {
                        func(std::current_exception());
                        promise->set_value();
                    } else {
                        promise->set_value(func(std::current_exception()));
                    }
                } catch (...) {
                    promise->set_exception(std::current_exception());
                }
            }
        }).detach();
        
        return Future<T>(std::move(future));
    }
    
    template<typename... Futures>
    auto when_all(Futures&&... futures) -> Future<std::tuple<typename std::decay_t<Futures>::value_type...>> {
        return Future<typename std::decay_t<Futures>::value_type...>::all(std::forward<Futures>(futures)...);
    }
    
    template<typename... Futures>
    auto when_any(Futures&&... futures) -> Future<variant<typename std::decay_t<Futures>::value_type...>> {
        return Future<typename std::decay_t<Futures>::value_type...>::any(std::forward<Futures>(futures)...);
    }
    
    template<typename T>
    Future<T> make_ready_future(T value) {
        return Future<T>::make_ready(std::move(value));
    }
    
    inline Future<void> make_ready_future() {
        return Future<void>::make_ready();
    }
    
    template<typename T>
    Future<T> make_exceptional_future(std::exception_ptr ex) {
        return Future<T>::make_exception(ex);
    }
    
    template<typename T, typename Exception>
    Future<T> make_exceptional_future(Exception&& ex) {
        return Future<T>::make_exception(std::make_exception_ptr(std::forward<Exception>(ex)));
    }
} 