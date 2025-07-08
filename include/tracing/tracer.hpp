#pragma once

#include "core/types.hpp"
#include <random>

namespace http_framework::tracing {
    struct TraceId {
        array<byte_t, 16> data;
        
        TraceId();
        explicit TraceId(string_view hex_string);
        string to_string() const;
        bool is_valid() const noexcept;
        
        static TraceId generate();
        static TraceId invalid();
    };
    
    struct SpanId {
        array<byte_t, 8> data;
        
        SpanId();
        explicit SpanId(string_view hex_string);
        string to_string() const;
        bool is_valid() const noexcept;
        
        static SpanId generate();
        static SpanId invalid();
    };
    
    enum class SpanKind : std::uint8_t {
        INTERNAL = 0,
        SERVER = 1,
        CLIENT = 2,
        PRODUCER = 3,
        CONSUMER = 4
    };
    
    enum class AttributeType : std::uint8_t {
        STRING = 0,
        BOOL = 1,
        INT64 = 2,
        DOUBLE = 3,
        STRING_ARRAY = 4,
        BOOL_ARRAY = 5,
        INT64_ARRAY = 6,
        DOUBLE_ARRAY = 7
    };
    
    struct Attribute {
        AttributeType type;
        variant<string, bool, int64_t, double, vector<string>, vector<bool>, vector<int64_t>, vector<double>> value;
        
        Attribute() = default;
        
        template<typename T>
        explicit Attribute(T&& val);
        
        string to_string() const;
    };
    
    struct SpanContext {
        TraceId trace_id;
        SpanId span_id;
        bool is_remote{false};
        bool is_sampled{true};
        hash_map<string, string> trace_state;
        
        SpanContext() = default;
        SpanContext(TraceId tid, SpanId sid, bool sampled = true);
        
        bool is_valid() const noexcept;
        string to_trace_parent() const;
        static optional<SpanContext> from_trace_parent(string_view trace_parent);
    };
    
    class Span {
    public:
        Span(string_view name, SpanKind kind = SpanKind::INTERNAL);
        Span(string_view name, const SpanContext& parent_context, SpanKind kind = SpanKind::INTERNAL);
        Span(const Span&) = delete;
        Span(Span&&) = default;
        Span& operator=(const Span&) = delete;
        Span& operator=(Span&&) = default;
        ~Span();
        
        const SpanContext& context() const noexcept { return context_; }
        const string& name() const noexcept { return name_; }
        SpanKind kind() const noexcept { return kind_; }
        timestamp_t start_time() const noexcept { return start_time_; }
        optional<timestamp_t> end_time() const noexcept { return end_time_; }
        bool is_recording() const noexcept { return recording_; }
        bool is_ended() const noexcept { return end_time_.has_value(); }
        
        Span& set_attribute(string_view key, string_view value);
        Span& set_attribute(string_view key, bool value);
        Span& set_attribute(string_view key, int64_t value);
        Span& set_attribute(string_view key, double value);
        Span& set_attribute(string_view key, const vector<string>& value);
        
        template<typename T>
        Span& set_attribute(string_view key, T&& value);
        
        Span& add_event(string_view name);
        Span& add_event(string_view name, const hash_map<string, Attribute>& attributes);
        Span& add_event(string_view name, timestamp_t timestamp);
        Span& add_event(string_view name, timestamp_t timestamp, const hash_map<string, Attribute>& attributes);
        
        Span& set_status(bool ok, string_view description = "");
        Span& record_exception(const std::exception& exception);
        
        void end();
        void end(timestamp_t end_timestamp);
        
        const hash_map<string, Attribute>& attributes() const noexcept { return attributes_; }
        
        struct Event {
            string name;
            timestamp_t timestamp;
            hash_map<string, Attribute> attributes;
        };
        
        const vector<Event>& events() const noexcept { return events_; }
        
        struct Status {
            bool ok{true};
            string description;
        };
        
        const Status& status() const noexcept { return status_; }
        
    private:
        SpanContext context_;
        string name_;
        SpanKind kind_;
        timestamp_t start_time_;
        optional<timestamp_t> end_time_;
        bool recording_{true};
        hash_map<string, Attribute> attributes_;
        vector<Event> events_;
        Status status_;
    };
    
    class SpanProcessor {
    public:
        virtual ~SpanProcessor() = default;
        virtual void on_start(const Span& span) = 0;
        virtual void on_end(const Span& span) = 0;
        virtual void force_flush(duration_t timeout = std::chrono::seconds(30)) = 0;
        virtual void shutdown(duration_t timeout = std::chrono::seconds(30)) = 0;
    };
    
    class BatchSpanProcessor : public SpanProcessor {
    public:
        struct Config {
            size_type max_queue_size{2048};
            duration_t schedule_delay{std::chrono::milliseconds(500)};
            size_type max_export_batch_size{512};
            duration_t export_timeout{std::chrono::seconds(30)};
        };
        
        explicit BatchSpanProcessor(shared_ptr<SpanExporter> exporter, Config config = {});
        ~BatchSpanProcessor() override;
        
        void on_start(const Span& span) override;
        void on_end(const Span& span) override;
        void force_flush(duration_t timeout = std::chrono::seconds(30)) override;
        void shutdown(duration_t timeout = std::chrono::seconds(30)) override;
        
    private:
        shared_ptr<SpanExporter> exporter_;
        Config config_;
        queue<Span> span_queue_;
        mutex queue_mutex_;
        condition_variable queue_condition_;
        unique_ptr<std::thread> worker_thread_;
        atomic<bool> shutdown_requested_{false};
        
        void worker_loop();
        void export_batch(vector<Span>& spans);
    };
    
    class SpanExporter {
    public:
        enum class ExportResult {
            SUCCESS,
            FAILURE,
            TIMEOUT
        };
        
        virtual ~SpanExporter() = default;
        virtual ExportResult export_spans(const vector<Span>& spans) = 0;
        virtual void shutdown(duration_t timeout = std::chrono::seconds(30)) = 0;
    };
    
    class OtlpSpanExporter : public SpanExporter {
    public:
        struct Config {
            string endpoint{"http://localhost:4317"};
            hash_map<string, string> headers;
            duration_t timeout{std::chrono::seconds(10)};
            bool use_grpc{true};
        };
        
        explicit OtlpSpanExporter(Config config = {});
        ~OtlpSpanExporter() override;
        
        ExportResult export_spans(const vector<Span>& spans) override;
        void shutdown(duration_t timeout = std::chrono::seconds(30)) override;
        
    private:
        Config config_;
        unique_ptr<class HttpClient> http_client_;
        
        string serialize_spans(const vector<Span>& spans);
        ExportResult send_request(const string& payload);
    };
    
    class TracerProvider {
    public:
        struct Config {
            string service_name{"unknown_service"};
            string service_version{"unknown_version"};
            hash_map<string, Attribute> resource_attributes;
            double sampling_ratio{1.0};
        };
        
        explicit TracerProvider(Config config = {});
        ~TracerProvider();
        
        shared_ptr<class Tracer> get_tracer(string_view name, string_view version = "");
        void add_span_processor(unique_ptr<SpanProcessor> processor);
        void force_flush(duration_t timeout = std::chrono::seconds(30));
        void shutdown(duration_t timeout = std::chrono::seconds(30));
        
        static TracerProvider& instance();
        static void set_global_provider(unique_ptr<TracerProvider> provider);
        
    private:
        Config config_;
        vector<unique_ptr<SpanProcessor>> processors_;
        hash_map<string, shared_ptr<class Tracer>> tracers_;
        mutable mutex tracers_mutex_;
        
        void process_span_start(const Span& span);
        void process_span_end(const Span& span);
        
        friend class Tracer;
        friend class Span;
    };
    
    class Tracer {
    public:
        Tracer(string_view name, string_view version, TracerProvider& provider);
        
        unique_ptr<Span> start_span(string_view name, SpanKind kind = SpanKind::INTERNAL);
        unique_ptr<Span> start_span(string_view name, const SpanContext& parent_context, SpanKind kind = SpanKind::INTERNAL);
        
        template<typename F>
        auto with_span(string_view name, F&& func) -> std::invoke_result_t<F, Span&>;
        
        template<typename F>
        auto with_span(string_view name, SpanKind kind, F&& func) -> std::invoke_result_t<F, Span&>;
        
        const string& name() const noexcept { return name_; }
        const string& version() const noexcept { return version_; }
        
    private:
        string name_;
        string version_;
        TracerProvider& provider_;
    };
    
    class SpanScope {
    public:
        explicit SpanScope(unique_ptr<Span> span);
        SpanScope(const SpanScope&) = delete;
        SpanScope(SpanScope&&) = default;
        SpanScope& operator=(const SpanScope&) = delete;
        SpanScope& operator=(SpanScope&&) = default;
        ~SpanScope();
        
        Span& span() const noexcept { return *span_; }
        Span* operator->() const noexcept { return span_.get(); }
        Span& operator*() const noexcept { return *span_; }
        
    private:
        unique_ptr<Span> span_;
    };
    
    thread_local extern optional<SpanContext> current_span_context;
    
    SpanScope start_active_span(string_view name, SpanKind kind = SpanKind::INTERNAL);
    SpanScope start_active_span(string_view name, const SpanContext& parent_context, SpanKind kind = SpanKind::INTERNAL);
    optional<SpanContext> get_current_span_context();
    void set_current_span_context(const SpanContext& context);
    void clear_current_span_context();
    
    template<typename F>
    auto with_active_span(string_view name, F&& func) -> std::invoke_result_t<F, Span&>;
    
    template<typename F>
    auto with_active_span(string_view name, SpanKind kind, F&& func) -> std::invoke_result_t<F, Span&>;
    
    namespace http {
        hash_map<string, string> extract_trace_headers(const http_framework::http::Request& request);
        void inject_trace_headers(http_framework::http::Response& response, const SpanContext& context);
        void inject_trace_headers(hash_map<string, string>& headers, const SpanContext& context);
        
        SpanScope start_server_span(const http_framework::http::Request& request);
        void end_server_span(Span& span, const http_framework::http::Response& response);
        
        unique_ptr<Span> start_client_span(string_view method, string_view url);
        void end_client_span(Span& span, status_code_t status_code);
    }
    
    template<typename T>
    Attribute::Attribute(T&& val) {
        if constexpr (std::is_same_v<std::decay_t<T>, string>) {
            type = AttributeType::STRING;
            value = std::forward<T>(val);
        } else if constexpr (std::is_same_v<std::decay_t<T>, bool>) {
            type = AttributeType::BOOL;
            value = std::forward<T>(val);
        } else if constexpr (std::is_integral_v<std::decay_t<T>>) {
            type = AttributeType::INT64;
            value = static_cast<int64_t>(val);
        } else if constexpr (std::is_floating_point_v<std::decay_t<T>>) {
            type = AttributeType::DOUBLE;
            value = static_cast<double>(val);
        }
    }
    
    template<typename T>
    Span& Span::set_attribute(string_view key, T&& value) {
        if (recording_) {
            attributes_[string(key)] = Attribute(std::forward<T>(value));
        }
        return *this;
    }
    
    template<typename F>
    auto Tracer::with_span(string_view name, F&& func) -> std::invoke_result_t<F, Span&> {
        auto span = start_span(name);
        try {
            if constexpr (std::is_void_v<std::invoke_result_t<F, Span&>>) {
                func(*span);
                span->end();
            } else {
                auto result = func(*span);
                span->end();
                return result;
            }
        } catch (...) {
            span->set_status(false, "Exception occurred");
            span->end();
            throw;
        }
    }
    
    template<typename F>
    auto Tracer::with_span(string_view name, SpanKind kind, F&& func) -> std::invoke_result_t<F, Span&> {
        auto span = start_span(name, kind);
        try {
            if constexpr (std::is_void_v<std::invoke_result_t<F, Span&>>) {
                func(*span);
                span->end();
            } else {
                auto result = func(*span);
                span->end();
                return result;
            }
        } catch (...) {
            span->set_status(false, "Exception occurred");
            span->end();
            throw;
        }
    }
    
    template<typename F>
    auto with_active_span(string_view name, F&& func) -> std::invoke_result_t<F, Span&> {
        auto span_scope = start_active_span(name);
        try {
            if constexpr (std::is_void_v<std::invoke_result_t<F, Span&>>) {
                func(span_scope.span());
            } else {
                return func(span_scope.span());
            }
        } catch (...) {
            span_scope->set_status(false, "Exception occurred");
            throw;
        }
    }
    
    template<typename F>
    auto with_active_span(string_view name, SpanKind kind, F&& func) -> std::invoke_result_t<F, Span&> {
        auto span_scope = start_active_span(name, kind);
        try {
            if constexpr (std::is_void_v<std::invoke_result_t<F, Span&>>) {
                func(span_scope.span());
            } else {
                return func(span_scope.span());
            }
        } catch (...) {
            span_scope->set_status(false, "Exception occurred");
            throw;
        }
    }
} 