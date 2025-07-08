#pragma once

#include "core/types.hpp"
#include "async/future.hpp"
#include "async/task.hpp"
#include <concepts>
#include <random>

namespace http_framework::resilience {
    enum class CircuitState : std::uint8_t {
        CLOSED = 0,
        OPEN = 1,
        HALF_OPEN = 2
    };
    
    enum class FailureType : std::uint8_t {
        TIMEOUT = 0,
        EXCEPTION = 1,
        PREDICATE_FAILED = 2,
        CIRCUIT_OPEN = 3
    };
    
    struct CircuitBreakerException : public std::runtime_error {
        FailureType failure_type;
        CircuitState circuit_state;
        
        CircuitBreakerException(FailureType type, CircuitState state, string_view message)
            : std::runtime_error(string(message)), failure_type(type), circuit_state(state) {}
    };
    
    struct ExecutionResult {
        bool success{false};
        optional<FailureType> failure_type;
        duration_t execution_time{duration_t::zero()};
        string error_message;
        
        ExecutionResult() = default;
        ExecutionResult(bool s) : success(s) {}
        ExecutionResult(FailureType type, string_view msg) : failure_type(type), error_message(msg) {}
        
        static ExecutionResult success() { return ExecutionResult{true}; }
        static ExecutionResult failure(FailureType type, string_view message = "") {
            return ExecutionResult{type, message};
        }
    };
    
    template<typename T>
    struct CallResult {
        bool success{false};
        optional<T> value;
        optional<FailureType> failure_type;
        duration_t execution_time{duration_t::zero()};
        string error_message;
        
        CallResult() = default;
        CallResult(T val) : success(true), value(std::move(val)) {}
        CallResult(FailureType type, string_view msg) : failure_type(type), error_message(msg) {}
        
        bool has_value() const noexcept { return success && value.has_value(); }
        const T& operator*() const { return value.value(); }
        T& operator*() { return value.value(); }
        const T* operator->() const { return &value.value(); }
        T* operator->() { return &value.value(); }
        
        static CallResult success(T value) { return CallResult{std::move(value)}; }
        static CallResult failure(FailureType type, string_view message = "") {
            return CallResult{type, message};
        }
    };
    
    template<>
    struct CallResult<void> {
        bool success{false};
        optional<FailureType> failure_type;
        duration_t execution_time{duration_t::zero()};
        string error_message;
        
        CallResult() = default;
        CallResult(bool s) : success(s) {}
        CallResult(FailureType type, string_view msg) : failure_type(type), error_message(msg) {}
        
        static CallResult success() { return CallResult{true}; }
        static CallResult failure(FailureType type, string_view message = "") {
            return CallResult{type, message};
        }
    };
    
    struct CircuitBreakerMetrics {
        CircuitState current_state{CircuitState::CLOSED};
        timestamp_t state_changed_at;
        duration_t time_in_current_state{duration_t::zero()};
        
        size_type total_requests{0};
        size_type successful_requests{0};
        size_type failed_requests{0};
        size_type timeout_requests{0};
        size_type circuit_open_requests{0};
        
        double success_rate{1.0};
        double failure_rate{0.0};
        
        duration_t average_execution_time{duration_t::zero()};
        duration_t max_execution_time{duration_t::zero()};
        duration_t min_execution_time{duration_t::max()};
        
        size_type consecutive_failures{0};
        size_type consecutive_successes{0};
        
        timestamp_t last_failure_time;
        timestamp_t last_success_time;
        
        deque<std::pair<timestamp_t, bool>> recent_calls;
        
        void reset() {
            total_requests = successful_requests = failed_requests = 0;
            timeout_requests = circuit_open_requests = 0;
            success_rate = 1.0;
            failure_rate = 0.0;
            average_execution_time = max_execution_time = duration_t::zero();
            min_execution_time = duration_t::max();
            consecutive_failures = consecutive_successes = 0;
            recent_calls.clear();
        }
        
        string to_json() const;
    };
    
    template<typename T>
    concept CircuitBreakerPredicate = requires(const T& predicate) {
        { predicate(std::declval<const std::exception&>()) } -> std::convertible_to<bool>;
    };
    
    class DefaultFailurePredicate {
    public:
        bool operator()(const std::exception& e) const {
            return true; // All exceptions are considered failures
        }
    };
    
    template<typename T>
    class CircuitBreaker {
    public:
        struct Config {
            size_type failure_threshold{5};
            duration_t timeout{std::chrono::seconds(30)};
            duration_t reset_timeout{std::chrono::seconds(60)};
            size_type success_threshold{3};
            size_type sampling_duration_seconds{60};
            size_type minimum_throughput{10};
            double failure_rate_threshold{0.5};
            bool enable_timeout{true};
            bool enable_metrics{true};
            bool enable_half_open_retry{true};
            size_type max_concurrent_calls{100};
        };
        
        template<CircuitBreakerPredicate P = DefaultFailurePredicate>
        explicit CircuitBreaker(string_view name, Config config = {}, P predicate = {});
        
        ~CircuitBreaker();
        
        template<typename F>
        async::Future<CallResult<std::invoke_result_t<F>>> execute(F&& func);
        
        template<typename F, typename Fallback>
        async::Future<CallResult<std::invoke_result_t<F>>> execute_with_fallback(F&& func, Fallback&& fallback);
        
        async::Future<CallResult<T>> execute_async(function<async::Future<T>()> func);
        async::Future<CallResult<void>> execute_async(function<async::Future<void>()> func);
        
        CircuitState state() const noexcept { return current_state_; }
        const string& name() const noexcept { return name_; }
        
        void force_open() { transition_to(CircuitState::OPEN); }
        void force_close() { transition_to(CircuitState::CLOSED); }
        void force_half_open() { transition_to(CircuitState::HALF_OPEN); }
        
        const CircuitBreakerMetrics& metrics() const noexcept { return metrics_; }
        void reset_metrics() { metrics_.reset(); }
        
        bool is_call_permitted() const;
        size_type current_concurrent_calls() const noexcept { return concurrent_calls_; }
        
        void set_state_change_callback(function<void(CircuitState, CircuitState)> callback);
        void set_metrics_callback(function<void(const CircuitBreakerMetrics&)> callback);
        
        template<typename F>
        void add_health_check(F&& health_check, duration_t interval = std::chrono::seconds(30));
        
        void remove_health_check();
        
        void enable_adaptive_timeout(bool enable = true) { adaptive_timeout_enabled_ = enable; }
        void enable_jitter(bool enable = true) { jitter_enabled_ = enable; }
        void enable_bulkhead(bool enable = true) { bulkhead_enabled_ = enable; }
        
    private:
        string name_;
        Config config_;
        function<bool(const std::exception&)> failure_predicate_;
        
        atomic<CircuitState> current_state_{CircuitState::CLOSED};
        atomic<timestamp_t> state_changed_at_;
        atomic<size_type> consecutive_failures_{0};
        atomic<size_type> consecutive_successes_{0};
        atomic<size_type> concurrent_calls_{0};
        
        CircuitBreakerMetrics metrics_;
        mutable shared_mutex metrics_mutex_;
        
        optional<function<void(CircuitState, CircuitState)>> state_change_callback_;
        optional<function<void(const CircuitBreakerMetrics&)>> metrics_callback_;
        
        optional<function<async::Future<bool>()>> health_check_;
        unique_ptr<std::thread> health_check_thread_;
        atomic<bool> health_check_running_{false};
        duration_t health_check_interval_{std::chrono::seconds(30)};
        
        bool adaptive_timeout_enabled_{false};
        bool jitter_enabled_{false};
        bool bulkhead_enabled_{false};
        
        std::mt19937 rng_{std::random_device{}()};
        
        void transition_to(CircuitState new_state);
        void record_success(duration_t execution_time);
        void record_failure(FailureType failure_type, duration_t execution_time);
        void update_metrics();
        bool should_attempt_reset() const;
        bool should_trip_circuit() const;
        
        duration_t get_adaptive_timeout() const;
        duration_t add_jitter(duration_t base_duration) const;
        
        void start_health_check();
        void stop_health_check();
        void health_check_loop();
        
        template<typename F>
        async::Future<CallResult<std::invoke_result_t<F>>> execute_internal(F&& func);
        
        class SemaphoreGuard {
        public:
            explicit SemaphoreGuard(atomic<size_type>& counter, size_type max_value);
            ~SemaphoreGuard();
            bool acquired() const noexcept { return acquired_; }
            
        private:
            atomic<size_type>& counter_;
            bool acquired_{false};
        };
    };
    
    class CircuitBreakerRegistry {
    public:
        static CircuitBreakerRegistry& instance();
        
        template<typename T>
        shared_ptr<CircuitBreaker<T>> get_or_create(string_view name, typename CircuitBreaker<T>::Config config = {});
        
        template<typename T>
        shared_ptr<CircuitBreaker<T>> get(string_view name);
        
        void remove(string_view name);
        void clear();
        
        vector<string> get_circuit_names() const;
        hash_map<string, CircuitBreakerMetrics> get_all_metrics() const;
        
        void set_global_config(function<void(string_view, auto&)> config_modifier);
        
    private:
        hash_map<string, shared_ptr<void>> circuits_;
        mutable shared_mutex circuits_mutex_;
        optional<function<void(string_view, auto&)>> global_config_modifier_;
        
        CircuitBreakerRegistry() = default;
    };
    
    template<typename T>
    class BulkheadCircuitBreaker : public CircuitBreaker<T> {
    public:
        struct BulkheadConfig {
            size_type max_concurrent_calls{10};
            size_type max_wait_duration_ms{1000};
            bool enable_queue{true};
            size_type queue_capacity{50};
        };
        
        BulkheadCircuitBreaker(string_view name, 
                              typename CircuitBreaker<T>::Config circuit_config = {},
                              BulkheadConfig bulkhead_config = {});
        
        template<typename F>
        async::Future<CallResult<std::invoke_result_t<F>>> execute(F&& func);
        
        size_type active_calls() const noexcept { return active_calls_; }
        size_type queued_calls() const noexcept { return call_queue_.size(); }
        
    private:
        BulkheadConfig bulkhead_config_;
        atomic<size_type> active_calls_{0};
        queue<function<async::Future<void>()>> call_queue_;
        mutable mutex queue_mutex_;
        condition_variable queue_condition_;
        
        bool try_acquire_permit();
        void release_permit();
        async::Future<bool> wait_for_permit();
    };
    
    template<typename T>
    class RetryCircuitBreaker : public CircuitBreaker<T> {
    public:
        struct RetryConfig {
            size_type max_attempts{3};
            duration_t initial_delay{std::chrono::milliseconds(100)};
            double backoff_multiplier{2.0};
            duration_t max_delay{std::chrono::seconds(30)};
            bool enable_jitter{true};
            function<bool(const std::exception&)> retry_predicate;
        };
        
        RetryCircuitBreaker(string_view name,
                           typename CircuitBreaker<T>::Config circuit_config = {},
                           RetryConfig retry_config = {});
        
        template<typename F>
        async::Future<CallResult<std::invoke_result_t<F>>> execute_with_retry(F&& func);
        
    private:
        RetryConfig retry_config_;
        std::mt19937 rng_{std::random_device{}()};
        
        duration_t calculate_delay(size_type attempt) const;
        bool should_retry(const std::exception& e, size_type attempt) const;
    };
    
    class CircuitBreakerMonitor {
    public:
        explicit CircuitBreakerMonitor(duration_t check_interval = std::chrono::seconds(10));
        ~CircuitBreakerMonitor();
        
        void start();
        void stop();
        
        void add_circuit(string_view name);
        void remove_circuit(string_view name);
        
        void set_alert_callback(function<void(string_view, const CircuitBreakerMetrics&)> callback);
        void set_health_degradation_threshold(double threshold) { health_threshold_ = threshold; }
        
        hash_map<string, CircuitBreakerMetrics> get_all_metrics() const;
        string generate_health_report() const;
        
    private:
        duration_t check_interval_;
        vector<string> monitored_circuits_;
        mutable shared_mutex circuits_mutex_;
        
        unique_ptr<std::thread> monitor_thread_;
        atomic<bool> monitoring_{false};
        
        optional<function<void(string_view, const CircuitBreakerMetrics&)>> alert_callback_;
        double health_threshold_{0.8};
        
        void monitoring_loop();
        void check_circuit_health(string_view circuit_name);
        bool is_circuit_healthy(const CircuitBreakerMetrics& metrics) const;
    };
    
    namespace policies {
        template<typename T>
        class CompositePolicy {
        public:
            CompositePolicy& with_circuit_breaker(typename CircuitBreaker<T>::Config config = {});
            CompositePolicy& with_retry(typename RetryCircuitBreaker<T>::RetryConfig config = {});
            CompositePolicy& with_timeout(duration_t timeout);
            CompositePolicy& with_bulkhead(typename BulkheadCircuitBreaker<T>::BulkheadConfig config = {});
            
            template<typename F>
            async::Future<CallResult<std::invoke_result_t<F>>> execute(F&& func);
            
        private:
            vector<function<async::Future<CallResult<T>>(function<async::Future<CallResult<T>>>())>> policies_;
        };
        
        template<typename T>
        auto create_policy(string_view name) -> CompositePolicy<T> {
            return CompositePolicy<T>{};
        }
    }
    
    // Implementation details
    template<typename T>
    template<CircuitBreakerPredicate P>
    CircuitBreaker<T>::CircuitBreaker(string_view name, Config config, P predicate)
        : name_(name), config_(std::move(config)), failure_predicate_(std::move(predicate)),
          state_changed_at_(std::chrono::steady_clock::now()) {
        metrics_.state_changed_at = state_changed_at_.load();
    }
    
    template<typename T>
    CircuitBreaker<T>::~CircuitBreaker() {
        stop_health_check();
    }
    
    template<typename T>
    template<typename F>
    async::Future<CallResult<std::invoke_result_t<F>>> CircuitBreaker<T>::execute(F&& func) {
        return execute_internal(std::forward<F>(func));
    }
    
    template<typename T>
    template<typename F>
    async::Future<CallResult<std::invoke_result_t<F>>> CircuitBreaker<T>::execute_internal(F&& func) {
        using ReturnType = std::invoke_result_t<F>;
        
        if (!is_call_permitted()) {
            record_failure(FailureType::CIRCUIT_OPEN, duration_t::zero());
            co_return CallResult<ReturnType>::failure(FailureType::CIRCUIT_OPEN, "Circuit breaker is open");
        }
        
        if (bulkhead_enabled_) {
            SemaphoreGuard guard(concurrent_calls_, config_.max_concurrent_calls);
            if (!guard.acquired()) {
                record_failure(FailureType::PREDICATE_FAILED, duration_t::zero());
                co_return CallResult<ReturnType>::failure(FailureType::PREDICATE_FAILED, "Max concurrent calls exceeded");
            }
        }
        
        auto start_time = std::chrono::steady_clock::now();
        auto timeout_duration = adaptive_timeout_enabled_ ? get_adaptive_timeout() : config_.timeout;
        
        if (jitter_enabled_) {
            timeout_duration = add_jitter(timeout_duration);
        }
        
        try {
            if constexpr (std::is_void_v<ReturnType>) {
                if (config_.enable_timeout) {
                    auto timeout_future = async::Future<void>::timeout(timeout_duration);
                    auto func_future = async::Future<void>::from_function(std::forward<F>(func));
                    
                    auto result_future = async::when_any(std::move(func_future), std::move(timeout_future));
                    co_await result_future;
                } else {
                    func();
                }
                
                auto execution_time = std::chrono::steady_clock::now() - start_time;
                record_success(execution_time);
                co_return CallResult<ReturnType>::success();
            } else {
                ReturnType result;
                if (config_.enable_timeout) {
                    auto timeout_future = async::Future<ReturnType>::timeout(timeout_duration);
                    auto func_future = async::Future<ReturnType>::from_function(std::forward<F>(func));
                    
                    auto result_future = async::when_any(std::move(func_future), std::move(timeout_future));
                    result = co_await result_future;
                } else {
                    result = func();
                }
                
                auto execution_time = std::chrono::steady_clock::now() - start_time;
                record_success(execution_time);
                co_return CallResult<ReturnType>::success(std::move(result));
            }
        } catch (const std::exception& e) {
            auto execution_time = std::chrono::steady_clock::now() - start_time;
            
            if (execution_time >= timeout_duration) {
                record_failure(FailureType::TIMEOUT, execution_time);
                co_return CallResult<ReturnType>::failure(FailureType::TIMEOUT, "Operation timed out");
            } else if (failure_predicate_(e)) {
                record_failure(FailureType::EXCEPTION, execution_time);
                co_return CallResult<ReturnType>::failure(FailureType::EXCEPTION, e.what());
            } else {
                record_success(execution_time);
                throw; // Re-throw if not considered a failure
            }
        }
    }
    
    template<typename T>
    bool CircuitBreaker<T>::is_call_permitted() const {
        auto state = current_state_.load();
        
        switch (state) {
            case CircuitState::CLOSED:
                return true;
            case CircuitState::OPEN:
                return should_attempt_reset();
            case CircuitState::HALF_OPEN:
                return consecutive_successes_.load() < config_.success_threshold;
            default:
                return false;
        }
    }
} 