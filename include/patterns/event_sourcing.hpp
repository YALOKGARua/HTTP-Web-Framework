#pragma once

#include "core/types.hpp"
#include "async/future.hpp"
#include "async/task.hpp"
#include <concepts>
#include <compare>

namespace http_framework::patterns {
    using EventId = string;
    using StreamId = string;
    using AggregateId = string;
    using CommandId = string;
    using EventVersion = std::uint64_t;
    using StreamVersion = std::uint64_t;
    
    template<typename T>
    concept EventData = requires(const T& event) {
        { event.get_event_type() } -> std::convertible_to<string>;
        { event.serialize() } -> std::convertible_to<string>;
        { T::deserialize(std::declval<string_view>()) } -> std::convertible_to<optional<T>>;
    };
    
    template<typename T>
    concept CommandData = requires(const T& cmd) {
        { cmd.get_command_type() } -> std::convertible_to<string>;
        { cmd.aggregate_id() } -> std::convertible_to<AggregateId>;
        { cmd.validate() } -> std::convertible_to<bool>;
    };
    
    template<typename T>
    concept AggregateRoot = requires(T& agg, const T& const_agg) {
        { const_agg.id() } -> std::convertible_to<AggregateId>;
        { const_agg.version() } -> std::convertible_to<StreamVersion>;
        { agg.apply_event(std::declval<const Event&>()) } -> std::same_as<void>;
        { const_agg.get_uncommitted_events() } -> std::convertible_to<vector<Event>>;
        { agg.mark_events_as_committed() } -> std::same_as<void>;
    };
    
    struct EventMetadata {
        EventId event_id;
        string event_type;
        StreamId stream_id;
        StreamVersion stream_version;
        timestamp_t occurred_at;
        string correlation_id;
        string causation_id;
        hash_map<string, string> custom_metadata;
        
        EventMetadata() = default;
        EventMetadata(EventId id, string_view type, StreamId sid, StreamVersion version)
            : event_id(std::move(id)), event_type(type), stream_id(std::move(sid)), 
              stream_version(version), occurred_at(std::chrono::steady_clock::now()) {}
        
        string to_json() const;
        static optional<EventMetadata> from_json(string_view json);
    };
    
    class Event {
    public:
        template<EventData T>
        Event(T data, EventMetadata metadata);
        
        Event(const Event&) = default;
        Event(Event&&) = default;
        Event& operator=(const Event&) = default;
        Event& operator=(Event&&) = default;
        
        const EventMetadata& metadata() const noexcept { return metadata_; }
        const string& data() const noexcept { return data_; }
        const string& event_type() const noexcept { return metadata_.event_type; }
        
        template<EventData T>
        optional<T> get_data() const;
        
        string to_json() const;
        static optional<Event> from_json(string_view json);
        
        auto operator<=>(const Event& other) const {
            if (metadata_.stream_id != other.metadata_.stream_id) {
                return metadata_.stream_id <=> other.metadata_.stream_id;
            }
            return metadata_.stream_version <=> other.metadata_.stream_version;
        }
        
        bool operator==(const Event& other) const {
            return metadata_.event_id == other.metadata_.event_id;
        }
        
    private:
        EventMetadata metadata_;
        string data_;
    };
    
    struct StreamSlice {
        StreamId stream_id;
        StreamVersion from_version;
        StreamVersion to_version;
        vector<Event> events;
        bool is_end_of_stream{false};
        
        StreamSlice() = default;
        StreamSlice(StreamId id, StreamVersion from, StreamVersion to)
            : stream_id(std::move(id)), from_version(from), to_version(to) {}
    };
    
    struct WriteResult {
        StreamId stream_id;
        StreamVersion next_expected_version;
        vector<EventId> written_event_ids;
        
        WriteResult() = default;
        WriteResult(StreamId id, StreamVersion version)
            : stream_id(std::move(id)), next_expected_version(version) {}
    };
    
    enum class ExpectedVersion : std::int64_t {
        ANY = -2,
        NO_STREAM = -1,
        EMPTY_STREAM = 0
    };
    
    class EventStore {
    public:
        virtual ~EventStore() = default;
        
        virtual async::Future<WriteResult> append_to_stream(
            const StreamId& stream_id,
            ExpectedVersion expected_version,
            const vector<Event>& events) = 0;
        
        virtual async::Future<StreamSlice> read_stream_events_forward(
            const StreamId& stream_id,
            StreamVersion start_version,
            size_type count) = 0;
        
        virtual async::Future<StreamSlice> read_stream_events_backward(
            const StreamId& stream_id,
            StreamVersion start_version,
            size_type count) = 0;
        
        virtual async::Future<bool> stream_exists(const StreamId& stream_id) = 0;
        virtual async::Future<StreamVersion> get_stream_version(const StreamId& stream_id) = 0;
        virtual async::Future<void> delete_stream(const StreamId& stream_id, ExpectedVersion expected_version) = 0;
        
        virtual async::Future<vector<Event>> read_all_events_forward(
            optional<timestamp_t> from_timestamp = std::nullopt,
            size_type count = 1000) = 0;
        
        virtual async::Future<vector<Event>> read_events_by_type(
            string_view event_type,
            optional<timestamp_t> from_timestamp = std::nullopt,
            size_type count = 1000) = 0;
        
        virtual async::Future<void> create_projection(
            string_view projection_name,
            string_view query,
            bool emit_enabled = true) = 0;
        
        virtual async::Future<void> start_subscription(
            string_view subscription_name,
            string_view stream_name,
            function<async::Task<void>(const Event&)> event_handler) = 0;
        
        virtual async::Future<void> stop_subscription(string_view subscription_name) = 0;
    };
    
    class InMemoryEventStore : public EventStore {
    public:
        InMemoryEventStore() = default;
        
        async::Future<WriteResult> append_to_stream(
            const StreamId& stream_id,
            ExpectedVersion expected_version,
            const vector<Event>& events) override;
        
        async::Future<StreamSlice> read_stream_events_forward(
            const StreamId& stream_id,
            StreamVersion start_version,
            size_type count) override;
        
        async::Future<StreamSlice> read_stream_events_backward(
            const StreamId& stream_id,
            StreamVersion start_version,
            size_type count) override;
        
        async::Future<bool> stream_exists(const StreamId& stream_id) override;
        async::Future<StreamVersion> get_stream_version(const StreamId& stream_id) override;
        async::Future<void> delete_stream(const StreamId& stream_id, ExpectedVersion expected_version) override;
        
        async::Future<vector<Event>> read_all_events_forward(
            optional<timestamp_t> from_timestamp,
            size_type count) override;
        
        async::Future<vector<Event>> read_events_by_type(
            string_view event_type,
            optional<timestamp_t> from_timestamp,
            size_type count) override;
        
        async::Future<void> create_projection(
            string_view projection_name,
            string_view query,
            bool emit_enabled) override;
        
        async::Future<void> start_subscription(
            string_view subscription_name,
            string_view stream_name,
            function<async::Task<void>(const Event&)> event_handler) override;
        
        async::Future<void> stop_subscription(string_view subscription_name) override;
        
    private:
        hash_map<StreamId, vector<Event>> streams_;
        vector<Event> all_events_;
        mutable shared_mutex streams_mutex_;
        
        hash_map<string, function<async::Task<void>(const Event&)>> subscriptions_;
        mutable mutex subscriptions_mutex_;
        
        void notify_subscribers(const Event& event);
        bool check_expected_version(const StreamId& stream_id, ExpectedVersion expected_version);
    };
    
    class PostgreSqlEventStore : public EventStore {
    public:
        struct Config {
            string connection_string;
            string events_table{"events"};
            string streams_table{"streams"};
            string snapshots_table{"snapshots"};
            size_type connection_pool_size{10};
            duration_t connection_timeout{std::chrono::seconds(30)};
        };
        
        explicit PostgreSqlEventStore(Config config);
        ~PostgreSqlEventStore();
        
        async::Future<WriteResult> append_to_stream(
            const StreamId& stream_id,
            ExpectedVersion expected_version,
            const vector<Event>& events) override;
        
        async::Future<StreamSlice> read_stream_events_forward(
            const StreamId& stream_id,
            StreamVersion start_version,
            size_type count) override;
        
        async::Future<StreamSlice> read_stream_events_backward(
            const StreamId& stream_id,
            StreamVersion start_version,
            size_type count) override;
        
        async::Future<bool> stream_exists(const StreamId& stream_id) override;
        async::Future<StreamVersion> get_stream_version(const StreamId& stream_id) override;
        async::Future<void> delete_stream(const StreamId& stream_id, ExpectedVersion expected_version) override;
        
        async::Future<vector<Event>> read_all_events_forward(
            optional<timestamp_t> from_timestamp,
            size_type count) override;
        
        async::Future<vector<Event>> read_events_by_type(
            string_view event_type,
            optional<timestamp_t> from_timestamp,
            size_type count) override;
        
        async::Future<void> create_projection(
            string_view projection_name,
            string_view query,
            bool emit_enabled) override;
        
        async::Future<void> start_subscription(
            string_view subscription_name,
            string_view stream_name,
            function<async::Task<void>(const Event&)> event_handler) override;
        
        async::Future<void> stop_subscription(string_view subscription_name) override;
        
        async::Future<void> save_snapshot(const AggregateId& aggregate_id, StreamVersion version, const string& snapshot_data);
        async::Future<optional<pair<StreamVersion, string>>> load_snapshot(const AggregateId& aggregate_id);
        
        async::Future<void> initialize_database();
        
    private:
        Config config_;
        unique_ptr<class DatabaseConnectionPool> connection_pool_;
        hash_map<string, function<async::Task<void>(const Event&)>> subscriptions_;
        mutable shared_mutex subscriptions_mutex_;
        
        async::Future<unique_ptr<class DatabaseConnection>> get_connection();
        void return_connection(unique_ptr<class DatabaseConnection> connection);
    };
    
    template<AggregateRoot T>
    class Repository {
    public:
        explicit Repository(shared_ptr<EventStore> event_store);
        
        async::Future<optional<T>> get_by_id(const AggregateId& id);
        async::Future<void> save(T& aggregate, ExpectedVersion expected_version = ExpectedVersion::ANY);
        async::Future<bool> exists(const AggregateId& id);
        
        void enable_snapshots(size_type snapshot_frequency = 10);
        void disable_snapshots();
        
    private:
        shared_ptr<EventStore> event_store_;
        bool snapshots_enabled_{false};
        size_type snapshot_frequency_{10};
        
        async::Future<T> build_aggregate_from_events(const AggregateId& id, const vector<Event>& events);
        async::Future<optional<T>> try_load_from_snapshot(const AggregateId& id);
        async::Future<void> maybe_save_snapshot(const T& aggregate);
        
        StreamId get_stream_id(const AggregateId& aggregate_id);
    };
    
    template<CommandData T>
    struct CommandResult {
        bool success{false};
        optional<T> command;
        vector<Event> events;
        string error_message;
        
        CommandResult() = default;
        CommandResult(T cmd, vector<Event> evts) : success(true), command(std::move(cmd)), events(std::move(evts)) {}
        CommandResult(string_view error) : error_message(error) {}
    };
    
    template<CommandData TCommand, AggregateRoot TAggregate>
    using CommandHandler = function<async::Future<CommandResult<TCommand>>(const TCommand&, optional<TAggregate>)>;
    
    class CommandBus {
    public:
        CommandBus() = default;
        
        template<CommandData TCommand, AggregateRoot TAggregate>
        void register_handler(CommandHandler<TCommand, TAggregate> handler);
        
        template<CommandData T>
        async::Future<CommandResult<T>> send(const T& command);
        
        void add_middleware(function<async::Future<bool>(const string& command_type, const string& aggregate_id)> middleware);
        
    private:
        hash_map<string, function<async::Future<variant<>>(const string&)>> handlers_;
        vector<function<async::Future<bool>(const string&, const string&)>> middlewares_;
        
        async::Future<bool> execute_middlewares(const string& command_type, const string& aggregate_id);
    };
    
    template<typename TEvent>
    using EventHandler = function<async::Task<void>(const TEvent&)>;
    
    class EventBus {
    public:
        EventBus() = default;
        
        template<EventData T>
        void subscribe(EventHandler<T> handler);
        
        template<EventData T>
        void unsubscribe();
        
        async::Task<void> publish(const Event& event);
        async::Task<void> publish_batch(const vector<Event>& events);
        
        void add_middleware(function<async::Task<bool>(const Event&)> middleware);
        
    private:
        hash_map<string, vector<function<async::Task<void>(const Event&)>>> handlers_;
        vector<function<async::Task<bool>(const Event&)>> middlewares_;
        mutable shared_mutex handlers_mutex_;
        
        async::Task<bool> execute_middlewares(const Event& event);
    };
    
    class Saga {
    public:
        virtual ~Saga() = default;
        virtual string saga_type() const = 0;
        virtual string saga_id() const = 0;
        virtual bool is_complete() const = 0;
        virtual async::Task<void> handle(const Event& event) = 0;
        virtual vector<string> get_handled_event_types() const = 0;
    };
    
    class SagaManager {
    public:
        explicit SagaManager(shared_ptr<EventStore> event_store);
        
        template<typename TSaga>
        void register_saga();
        
        async::Task<void> handle_event(const Event& event);
        async::Task<void> start_saga(unique_ptr<Saga> saga);
        async::Task<void> complete_saga(const string& saga_id);
        
        async::Future<vector<unique_ptr<Saga>>> get_active_sagas(const string& saga_type = "");
        
    private:
        shared_ptr<EventStore> event_store_;
        hash_map<string, function<unique_ptr<Saga>()>> saga_factories_;
        hash_map<string, unique_ptr<Saga>> active_sagas_;
        hash_map<string, vector<string>> event_to_saga_mapping_;
        mutable shared_mutex sagas_mutex_;
        
        async::Task<void> persist_saga_state(const Saga& saga);
        async::Task<void> load_saga_state(Saga& saga);
    };
    
    class ProjectionBuilder {
    public:
        ProjectionBuilder() = default;
        
        template<EventData T>
        ProjectionBuilder& when(function<async::Task<void>(const T&)> handler);
        
        ProjectionBuilder& from_stream(const StreamId& stream_id);
        ProjectionBuilder& from_category(string_view category);
        ProjectionBuilder& from_all();
        
        ProjectionBuilder& include_deleted();
        ProjectionBuilder& include_system_events();
        
        unique_ptr<class Projection> build();
        
    private:
        hash_map<string, function<async::Task<void>(const Event&)>> event_handlers_;
        optional<StreamId> stream_id_;
        optional<string> category_;
        bool from_all_{false};
        bool include_deleted_{false};
        bool include_system_events_{false};
    };
    
    class ReadModel {
    public:
        virtual ~ReadModel() = default;
        virtual async::Task<void> apply(const Event& event) = 0;
        virtual async::Task<void> reset() = 0;
        virtual string get_name() const = 0;
    };
    
    template<typename T>
    class InMemoryReadModel : public ReadModel {
    public:
        explicit InMemoryReadModel(string_view name) : name_(name) {}
        
        void add_item(string_view key, T item) {
            unique_lock lock(data_mutex_);
            data_[string(key)] = std::move(item);
        }
        
        void remove_item(string_view key) {
            unique_lock lock(data_mutex_);
            data_.erase(string(key));
        }
        
        optional<T> get_item(string_view key) const {
            shared_lock lock(data_mutex_);
            auto it = data_.find(string(key));
            return it != data_.end() ? std::make_optional(it->second) : std::nullopt;
        }
        
        vector<T> get_all() const {
            shared_lock lock(data_mutex_);
            vector<T> result;
            result.reserve(data_.size());
            for (const auto& [key, value] : data_) {
                result.push_back(value);
            }
            return result;
        }
        
        async::Task<void> reset() override {
            unique_lock lock(data_mutex_);
            data_.clear();
        }
        
        string get_name() const override { return name_; }
        
    protected:
        hash_map<string, T> data_;
        mutable shared_mutex data_mutex_;
        string name_;
    };
    
    template<EventData T>
    Event::Event(T data, EventMetadata metadata) : metadata_(std::move(metadata)) {
        data_ = data.serialize();
    }
    
    template<EventData T>
    optional<T> Event::get_data() const {
        return T::deserialize(data_);
    }
    
    template<AggregateRoot T>
    Repository<T>::Repository(shared_ptr<EventStore> event_store) : event_store_(std::move(event_store)) {}
    
    template<AggregateRoot T>
    async::Future<optional<T>> Repository<T>::get_by_id(const AggregateId& id) {
        if (snapshots_enabled_) {
            auto snapshot_result = co_await try_load_from_snapshot(id);
            if (snapshot_result) {
                co_return snapshot_result;
            }
        }
        
        auto stream_slice = co_await event_store_->read_stream_events_forward(get_stream_id(id), 0, 1000);
        if (stream_slice.events.empty()) {
            co_return std::nullopt;
        }
        
        co_return co_await build_aggregate_from_events(id, stream_slice.events);
    }
    
    template<AggregateRoot T>
    async::Future<void> Repository<T>::save(T& aggregate, ExpectedVersion expected_version) {
        auto uncommitted_events = aggregate.get_uncommitted_events();
        if (uncommitted_events.empty()) {
            co_return;
        }
        
        auto write_result = co_await event_store_->append_to_stream(
            get_stream_id(aggregate.id()),
            expected_version,
            uncommitted_events
        );
        
        aggregate.mark_events_as_committed();
        
        if (snapshots_enabled_) {
            co_await maybe_save_snapshot(aggregate);
        }
    }
    
    template<CommandData TCommand, AggregateRoot TAggregate>
    void CommandBus::register_handler(CommandHandler<TCommand, TAggregate> handler) {
        string command_type = TCommand{}.get_command_type();
        handlers_[command_type] = [handler = std::move(handler)](const string& serialized_command) -> async::Future<variant<>> {
            auto command = TCommand::deserialize(serialized_command);
            if (!command) {
                co_return variant<>{};
            }
            
            // This would typically load the aggregate from repository
            optional<TAggregate> aggregate = std::nullopt;
            auto result = co_await handler(*command, aggregate);
            co_return variant<>{};
        };
    }
    
    template<EventData T>
    void EventBus::subscribe(EventHandler<T> handler) {
        unique_lock lock(handlers_mutex_);
        string event_type = T{}.get_event_type();
        handlers_[event_type].push_back([handler = std::move(handler)](const Event& event) -> async::Task<void> {
            auto event_data = event.get_data<T>();
            if (event_data) {
                co_await handler(*event_data);
            }
        });
    }
} 