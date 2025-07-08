#pragma once

#include "core/types.hpp"
#include "async/future.hpp"
#include "async/task.hpp"

namespace http_framework::core {
    class ThreadPool {
    public:
        explicit ThreadPool(size_type num_threads = std::thread::hardware_concurrency());
        ThreadPool(const ThreadPool&) = delete;
        ThreadPool(ThreadPool&&) = delete;
        ThreadPool& operator=(const ThreadPool&) = delete;
        ThreadPool& operator=(ThreadPool&&) = delete;
        ~ThreadPool();
        
        template<typename F, typename... Args>
        auto submit(F&& func, Args&&... args) -> async::Future<std::invoke_result_t<F, Args...>>;
        
        template<typename F>
        auto submit_task(F&& func) -> async::Task<std::invoke_result_t<F>>;
        
        void submit_detached(function<void()> task);
        
        template<typename Iterator, typename F>
        auto parallel_for(Iterator begin, Iterator end, F&& func) -> async::Future<void>;
        
        template<typename T, typename F>
        auto parallel_transform(const vector<T>& input, F&& func) -> async::Future<vector<std::invoke_result_t<F, T>>>;
        
        template<typename F, typename... Args>
        void submit_delayed(duration_t delay, F&& func, Args&&... args);
        
        template<typename F, typename... Args>
        void submit_periodic(duration_t interval, F&& func, Args&&... args);
        
        void resize(size_type new_size);
        void shutdown();
        void shutdown_now();
        bool is_shutdown() const noexcept { return shutdown_requested_; }
        
        size_type size() const noexcept { return workers_.size(); }
        size_type active_threads() const noexcept { return active_threads_; }
        size_type pending_tasks() const;
        size_type completed_tasks() const noexcept { return completed_tasks_; }
        
        void wait_for_all();
        bool wait_for_all(duration_t timeout);
        
        void set_thread_priority(int priority);
        void set_thread_affinity(const vector<int>& cpu_cores);
        
        void enable_work_stealing(bool enable = true);
        void set_max_queue_size(size_type max_size);
        
        hash_map<string, variant<string, int64_t, double, bool>> get_statistics() const;
        
        void pause();
        void resume();
        bool is_paused() const noexcept { return paused_; }
        
        void set_exception_handler(function<void(const std::exception&)> handler);
        
    private:
        struct WorkerThread {
            std::thread thread;
            queue<function<void()>> local_queue;
            mutable mutex queue_mutex;
            condition_variable condition;
            atomic<bool> running{true};
            atomic<size_type> processed_tasks{0};
            thread_id_t thread_id;
            
            WorkerThread() : thread_id(std::this_thread::get_id()) {}
        };
        
        vector<unique_ptr<WorkerThread>> workers_;
        queue<function<void()>> global_queue_;
        mutable mutex global_queue_mutex_;
        condition_variable global_condition_;
        
        atomic<bool> shutdown_requested_{false};
        atomic<bool> paused_{false};
        atomic<size_type> active_threads_{0};
        atomic<size_type> completed_tasks_{0};
        atomic<size_type> pending_tasks_{0};
        
        bool work_stealing_enabled_{true};
        optional<size_type> max_queue_size_;
        optional<int> thread_priority_;
        vector<int> thread_affinity_;
        
        optional<function<void(const std::exception&)>> exception_handler_;
        
        void worker_loop(WorkerThread* worker);
        bool try_steal_work(WorkerThread* worker);
        bool try_execute_task(WorkerThread* worker);
        
        void schedule_on_thread(size_type thread_index, function<void()> task);
        size_type select_thread() const;
        
        void apply_thread_settings(std::thread& thread);
        void handle_worker_exception(const std::exception& e);
        
        bool enqueue_task(function<void()> task);
        optional<function<void()>> dequeue_task();
        optional<function<void()>> steal_task_from(WorkerThread* worker);
        
        void notify_workers();
        void wait_for_worker_shutdown();
        
        void update_statistics();
        void cleanup_resources();
    };
    
    template<typename F, typename... Args>
    auto ThreadPool::submit(F&& func, Args&&... args) -> async::Future<std::invoke_result_t<F, Args...>> {
        using ReturnType = std::invoke_result_t<F, Args...>;
        
        if (shutdown_requested_) {
            throw std::runtime_error("ThreadPool is shutting down");
        }
        
        auto task = std::make_shared<std::packaged_task<ReturnType()>>(
            std::bind(std::forward<F>(func), std::forward<Args>(args)...)
        );
        
        auto future = async::Future<ReturnType>(task->get_future());
        
        auto wrapper = [task]() {
            try {
                (*task)();
            } catch (...) {
                // Exception will be captured by the packaged_task
            }
        };
        
        if (!enqueue_task(std::move(wrapper))) {
            throw std::runtime_error("Failed to enqueue task");
        }
        
        return future;
    }
    
    template<typename F>
    auto ThreadPool::submit_task(F&& func) -> async::Task<std::invoke_result_t<F>> {
        using ReturnType = std::invoke_result_t<F>;
        
        auto future = submit(std::forward<F>(func));
        
        if constexpr (std::is_void_v<ReturnType>) {
            co_await future;
        } else {
            co_return co_await future;
        }
    }
    
    template<typename Iterator, typename F>
    auto ThreadPool::parallel_for(Iterator begin, Iterator end, F&& func) -> async::Future<void> {
        if (begin == end) {
            return async::make_ready_future();
        }
        
        auto distance = std::distance(begin, end);
        auto chunk_size = std::max(1L, distance / static_cast<long>(size()));
        
        vector<async::Future<void>> futures;
        auto current = begin;
        
        while (current != end) {
            auto chunk_end = current;
            std::advance(chunk_end, std::min(chunk_size, std::distance(current, end)));
            
            auto chunk_begin = current;
            futures.push_back(submit([chunk_begin, chunk_end, func]() {
                for (auto it = chunk_begin; it != chunk_end; ++it) {
                    func(*it);
                }
            }));
            
            current = chunk_end;
        }
        
        return async::when_all(futures.begin(), futures.end());
    }
    
    template<typename T, typename F>
    auto ThreadPool::parallel_transform(const vector<T>& input, F&& func) -> async::Future<vector<std::invoke_result_t<F, T>>> {
        using ReturnType = std::invoke_result_t<F, T>;
        
        if (input.empty()) {
            return async::make_ready_future(vector<ReturnType>{});
        }
        
        auto chunk_size = std::max(1UL, input.size() / size());
        vector<async::Future<vector<ReturnType>>> futures;
        
        for (size_type i = 0; i < input.size(); i += chunk_size) {
            auto end_idx = std::min(i + chunk_size, input.size());
            
            futures.push_back(submit([&input, i, end_idx, func]() -> vector<ReturnType> {
                vector<ReturnType> result;
                result.reserve(end_idx - i);
                
                for (size_type j = i; j < end_idx; ++j) {
                    if constexpr (std::is_void_v<ReturnType>) {
                        func(input[j]);
                    } else {
                        result.push_back(func(input[j]));
                    }
                }
                
                return result;
            }));
        }
        
        return async::when_all(futures.begin(), futures.end()).then([](auto&& results) {
            vector<ReturnType> combined;
            for (auto&& chunk : results) {
                auto chunk_result = chunk.get();
                combined.insert(combined.end(), 
                              std::make_move_iterator(chunk_result.begin()),
                              std::make_move_iterator(chunk_result.end()));
            }
            return combined;
        });
    }
    
    template<typename F, typename... Args>
    void ThreadPool::submit_delayed(duration_t delay, F&& func, Args&&... args) {
        auto task = std::bind(std::forward<F>(func), std::forward<Args>(args)...);
        
        submit([task = std::move(task), delay]() {
            std::this_thread::sleep_for(delay);
            task();
        });
    }
    
    template<typename F, typename... Args>
    void ThreadPool::submit_periodic(duration_t interval, F&& func, Args&&... args) {
        auto task = std::bind(std::forward<F>(func), std::forward<Args>(args)...);
        
        submit([this, task = std::move(task), interval]() {
            while (!shutdown_requested_ && !paused_) {
                task();
                std::this_thread::sleep_for(interval);
            }
        });
    }
} 