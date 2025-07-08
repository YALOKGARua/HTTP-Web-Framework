#pragma once

#include "core/types.hpp"
#include <fstream>
#include <sstream>

namespace http_framework::utils {
    class Logger {
    public:
        struct Config {
            LogLevel level{LogLevel::INFO};
            bool console_output{true};
            bool file_output{false};
            optional<string> file_path;
            bool auto_flush{true};
            bool thread_safe{true};
            bool include_timestamp{true};
            bool include_thread_id{true};
            bool include_location{false};
            string timestamp_format{"%Y-%m-%d %H:%M:%S"};
            size_type max_file_size{10 * 1024 * 1024};
            size_type max_backup_files{5};
            bool colored_output{true};
        };
        
        Logger();
        explicit Logger(const Config& config);
        Logger(const Logger&) = delete;
        Logger(Logger&&) = default;
        Logger& operator=(const Logger&) = delete;
        Logger& operator=(Logger&&) = default;
        ~Logger();
        
        void configure(const Config& config);
        const Config& get_config() const noexcept { return config_; }
        
        void set_level(LogLevel level) { config_.level = level; }
        LogLevel level() const noexcept { return config_.level; }
        
        void trace(string_view message);
        void debug(string_view message);
        void info(string_view message);
        void warn(string_view message);
        void error(string_view message);
        void fatal(string_view message);
        
        template<typename... Args>
        void trace(string_view format, Args&&... args);
        
        template<typename... Args>
        void debug(string_view format, Args&&... args);
        
        template<typename... Args>
        void info(string_view format, Args&&... args);
        
        template<typename... Args>
        void warn(string_view format, Args&&... args);
        
        template<typename... Args>
        void error(string_view format, Args&&... args);
        
        template<typename... Args>
        void fatal(string_view format, Args&&... args);
        
        void log(LogLevel level, string_view message);
        void log(LogLevel level, string_view message, const hash_map<string, string>& context);
        
        template<typename... Args>
        void log(LogLevel level, string_view format, Args&&... args);
        
        void log_with_location(LogLevel level, string_view message, string_view file, int line, string_view function);
        
        template<typename... Args>
        void log_with_location(LogLevel level, string_view format, string_view file, int line, string_view function, Args&&... args);
        
        bool should_log(LogLevel level) const noexcept { return level >= config_.level; }
        
        void flush();
        void close();
        
        void enable_console_output(bool enable = true) { config_.console_output = enable; }
        void enable_file_output(bool enable = true) { config_.file_output = enable; }
        void set_file_path(string_view path);
        
        void enable_auto_flush(bool enable = true) { config_.auto_flush = enable; }
        void enable_thread_safety(bool enable = true) { config_.thread_safe = enable; }
        void enable_colored_output(bool enable = true) { config_.colored_output = enable; }
        
        void add_context(string_view key, string_view value);
        void remove_context(string_view key);
        void clear_context();
        
        size_type message_count() const noexcept { return message_count_; }
        size_type error_count() const noexcept { return error_count_; }
        size_type bytes_written() const noexcept { return bytes_written_; }
        
        void rotate_log_file();
        void set_max_file_size(size_type max_size) { config_.max_file_size = max_size; }
        void set_max_backup_files(size_type max_files) { config_.max_backup_files = max_files; }
        
        hash_map<string, variant<string, int64_t, double, bool>> get_statistics() const;
        
        static Logger& get_global_logger();
        static void set_global_logger(unique_ptr<Logger> logger);
        
        static string level_to_string(LogLevel level);
        static LogLevel string_to_level(string_view level_str);
        
    private:
        Config config_;
        unique_ptr<std::ofstream> file_stream_;
        hash_map<string, string> context_;
        mutable mutex logger_mutex_;
        
        atomic<size_type> message_count_{0};
        atomic<size_type> error_count_{0};
        atomic<size_type> bytes_written_{0};
        atomic<size_type> current_file_size_{0};
        
        static unique_ptr<Logger> global_logger_;
        static mutex global_logger_mutex_;
        
        void write_log_entry(LogLevel level, string_view message, const optional<hash_map<string, string>>& extra_context = std::nullopt);
        void write_to_console(LogLevel level, const string& formatted_message);
        void write_to_file(const string& formatted_message);
        
        string format_message(LogLevel level, string_view message, const optional<hash_map<string, string>>& extra_context) const;
        string format_timestamp() const;
        string format_thread_id() const;
        string get_color_code(LogLevel level) const;
        string get_reset_color_code() const;
        
        void check_file_rotation();
        void perform_file_rotation();
        void create_backup_filename(size_type backup_number, string& backup_path) const;
        
        bool open_file_stream();
        void close_file_stream();
        
        template<typename... Args>
        string format_string(string_view format, Args&&... args) const;
        
        void handle_logging_error(const std::exception& e);
        void update_statistics(LogLevel level, size_type message_size);
    };
    
    class LoggerScope {
    public:
        LoggerScope(Logger& logger, string_view scope_name);
        LoggerScope(string_view scope_name);
        ~LoggerScope();
        
        void add_context(string_view key, string_view value);
        
    private:
        Logger& logger_;
        string scope_name_;
        hash_map<string, string> original_context_;
    };
    
    class PerformanceLogger {
    public:
        PerformanceLogger(Logger& logger, string_view operation_name, LogLevel level = LogLevel::DEBUG);
        PerformanceLogger(string_view operation_name, LogLevel level = LogLevel::DEBUG);
        ~PerformanceLogger();
        
        void checkpoint(string_view checkpoint_name);
        duration_t elapsed() const;
        
    private:
        Logger& logger_;
        string operation_name_;
        LogLevel level_;
        timestamp_t start_time_;
        vector<std::pair<string, timestamp_t>> checkpoints_;
    };
    
    template<typename... Args>
    void Logger::trace(string_view format, Args&&... args) {
        if (should_log(LogLevel::TRACE)) {
            log(LogLevel::TRACE, format_string(format, std::forward<Args>(args)...));
        }
    }
    
    template<typename... Args>
    void Logger::debug(string_view format, Args&&... args) {
        if (should_log(LogLevel::DEBUG)) {
            log(LogLevel::DEBUG, format_string(format, std::forward<Args>(args)...));
        }
    }
    
    template<typename... Args>
    void Logger::info(string_view format, Args&&... args) {
        if (should_log(LogLevel::INFO)) {
            log(LogLevel::INFO, format_string(format, std::forward<Args>(args)...));
        }
    }
    
    template<typename... Args>
    void Logger::warn(string_view format, Args&&... args) {
        if (should_log(LogLevel::WARN)) {
            log(LogLevel::WARN, format_string(format, std::forward<Args>(args)...));
        }
    }
    
    template<typename... Args>
    void Logger::error(string_view format, Args&&... args) {
        if (should_log(LogLevel::ERROR)) {
            log(LogLevel::ERROR, format_string(format, std::forward<Args>(args)...));
        }
    }
    
    template<typename... Args>
    void Logger::fatal(string_view format, Args&&... args) {
        if (should_log(LogLevel::FATAL)) {
            log(LogLevel::FATAL, format_string(format, std::forward<Args>(args)...));
        }
    }
    
    template<typename... Args>
    void Logger::log(LogLevel level, string_view format, Args&&... args) {
        if (should_log(level)) {
            log(level, format_string(format, std::forward<Args>(args)...));
        }
    }
    
    template<typename... Args>
    void Logger::log_with_location(LogLevel level, string_view format, string_view file, int line, string_view function, Args&&... args) {
        if (should_log(level)) {
            hash_map<string, string> location_context = {
                {"file", string(file)},
                {"line", std::to_string(line)},
                {"function", string(function)}
            };
            log(level, format_string(format, std::forward<Args>(args)...), location_context);
        }
    }
    
    template<typename... Args>
    string Logger::format_string(string_view format, Args&&... args) const {
        std::ostringstream oss;
        size_type pos = 0;
        size_type arg_index = 0;
        
        auto format_arg = [&oss](auto&& arg) {
            oss << arg;
        };
        
        while (pos < format.size()) {
            auto brace_pos = format.find("{}", pos);
            if (brace_pos == string_view::npos) {
                oss << format.substr(pos);
                break;
            }
            
            oss << format.substr(pos, brace_pos - pos);
            
            if (arg_index < sizeof...(args)) {
                auto apply_format = [&](auto&& first_arg, auto&&... rest_args) {
                    if (arg_index == 0) {
                        format_arg(first_arg);
                    } else {
                        apply_format(rest_args...);
                        --arg_index;
                    }
                };
                
                if constexpr (sizeof...(args) > 0) {
                    apply_format(args...);
                }
                ++arg_index;
            }
            
            pos = brace_pos + 2;
        }
        
        return oss.str();
    }
}

#define LOG_TRACE(logger, ...) (logger).log_with_location(http_framework::LogLevel::TRACE, __VA_ARGS__, __FILE__, __LINE__, __FUNCTION__)
#define LOG_DEBUG(logger, ...) (logger).log_with_location(http_framework::LogLevel::DEBUG, __VA_ARGS__, __FILE__, __LINE__, __FUNCTION__)
#define LOG_INFO(logger, ...) (logger).log_with_location(http_framework::LogLevel::INFO, __VA_ARGS__, __FILE__, __LINE__, __FUNCTION__)
#define LOG_WARN(logger, ...) (logger).log_with_location(http_framework::LogLevel::WARN, __VA_ARGS__, __FILE__, __LINE__, __FUNCTION__)
#define LOG_ERROR(logger, ...) (logger).log_with_location(http_framework::LogLevel::ERROR, __VA_ARGS__, __FILE__, __LINE__, __FUNCTION__)
#define LOG_FATAL(logger, ...) (logger).log_with_location(http_framework::LogLevel::FATAL, __VA_ARGS__, __FILE__, __LINE__, __FUNCTION__)

#define GLOBAL_LOG_TRACE(...) LOG_TRACE(http_framework::utils::Logger::get_global_logger(), __VA_ARGS__)
#define GLOBAL_LOG_DEBUG(...) LOG_DEBUG(http_framework::utils::Logger::get_global_logger(), __VA_ARGS__)
#define GLOBAL_LOG_INFO(...) LOG_INFO(http_framework::utils::Logger::get_global_logger(), __VA_ARGS__)
#define GLOBAL_LOG_WARN(...) LOG_WARN(http_framework::utils::Logger::get_global_logger(), __VA_ARGS__)
#define GLOBAL_LOG_ERROR(...) LOG_ERROR(http_framework::utils::Logger::get_global_logger(), __VA_ARGS__)
#define GLOBAL_LOG_FATAL(...) LOG_FATAL(http_framework::utils::Logger::get_global_logger(), __VA_ARGS__)

#define SCOPED_LOGGER(name) http_framework::utils::LoggerScope __scoped_logger_##__LINE__(name)
#define PERFORMANCE_LOG(name) http_framework::utils::PerformanceLogger __perf_logger_##__LINE__(name) 