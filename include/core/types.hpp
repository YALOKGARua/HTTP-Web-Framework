#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <functional>
#include <variant>
#include <optional>
#include <chrono>
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <future>
#include <queue>
#include <deque>
#include <array>
#include <span>
#include <concepts>
#include <coroutine>

namespace http_framework {
    using byte_t = std::uint8_t;
    using socket_t = int;
    using port_t = std::uint16_t;
    using status_code_t = std::uint16_t;
    using timestamp_t = std::chrono::time_point<std::chrono::steady_clock>;
    using duration_t = std::chrono::milliseconds;
    using thread_id_t = std::thread::id;
    using size_type = std::size_t;
    using ssize_type = std::ptrdiff_t;
    
    using string_view = std::string_view;
    using string = std::string;
    using buffer_t = std::vector<byte_t>;
    using byte_span = std::span<const byte_t>;
    using mutable_byte_span = std::span<byte_t>;
    
    template<typename T>
    using unique_ptr = std::unique_ptr<T>;
    
    template<typename T>
    using shared_ptr = std::shared_ptr<T>;
    
    template<typename T>
    using weak_ptr = std::weak_ptr<T>;
    
    template<typename T>
    using optional = std::optional<T>;
    
    template<typename... Args>
    using variant = std::variant<Args...>;
    
    template<typename K, typename V>
    using hash_map = std::unordered_map<K, V>;
    
    template<typename T>
    using vector = std::vector<T>;
    
    template<typename T>
    using deque = std::deque<T>;
    
    template<typename T>
    using queue = std::queue<T>;
    
    template<typename T, size_type N>
    using array = std::array<T, N>;
    
    template<typename Signature>
    using function = std::function<Signature>;
    
    template<typename T>
    using future = std::future<T>;
    
    template<typename T>
    using promise = std::promise<T>;
    
    using mutex = std::mutex;
    using shared_mutex = std::shared_mutex;
    using lock_guard = std::lock_guard<mutex>;
    using unique_lock = std::unique_lock<mutex>;
    using shared_lock = std::shared_lock<shared_mutex>;
    using condition_variable = std::condition_variable;
    
    template<typename T>
    using atomic = std::atomic<T>;
    
    enum class LogLevel : std::uint8_t {
        TRACE = 0,
        DEBUG = 1,
        INFO = 2,
        WARN = 3,
        ERROR = 4,
        FATAL = 5
    };
    
    enum class HttpMethod : std::uint8_t {
        GET = 0,
        POST = 1,
        PUT = 2,
        DELETE = 3,
        PATCH = 4,
        HEAD = 5,
        OPTIONS = 6,
        TRACE = 7,
        CONNECT = 8
    };
    
    enum class HttpVersion : std::uint8_t {
        HTTP_1_0 = 10,
        HTTP_1_1 = 11,
        HTTP_2_0 = 20,
        HTTP_3_0 = 30
    };
    
    enum class ConnectionState : std::uint8_t {
        CONNECTING = 0,
        CONNECTED = 1,
        READING = 2,
        WRITING = 3,
        CLOSING = 4,
        CLOSED = 5,
        ERROR = 6
    };
    
    enum class WebSocketOpcode : std::uint8_t {
        CONTINUATION = 0x0,
        TEXT = 0x1,
        BINARY = 0x2,
        CLOSE = 0x8,
        PING = 0x9,
        PONG = 0xA
    };
    
    enum class CompressionType : std::uint8_t {
        NONE = 0,
        GZIP = 1,
        DEFLATE = 2,
        BROTLI = 3
    };
    
    enum class CachePolicy : std::uint8_t {
        NO_CACHE = 0,
        CACHE = 1,
        CACHE_REVALIDATE = 2,
        IMMUTABLE = 3
    };
    
    struct HttpHeader {
        string name;
        string value;
        
        HttpHeader() = default;
        HttpHeader(string_view n, string_view v) : name(n), value(v) {}
    };
    
    struct HttpCookie {
        string name;
        string value;
        optional<string> domain;
        optional<string> path;
        optional<timestamp_t> expires;
        optional<duration_t> max_age;
        bool secure = false;
        bool http_only = false;
        optional<string> same_site;
        
        HttpCookie() = default;
        HttpCookie(string_view n, string_view v) : name(n), value(v) {}
    };
    
    struct IpAddress {
        variant<std::array<byte_t, 4>, std::array<byte_t, 16>> address;
        bool is_ipv6() const noexcept { return std::holds_alternative<std::array<byte_t, 16>>(address); }
        string to_string() const;
    };
    
    struct Endpoint {
        IpAddress address;
        port_t port;
        
        Endpoint() = default;
        Endpoint(IpAddress addr, port_t p) : address(std::move(addr)), port(p) {}
        string to_string() const;
    };
    
    template<typename T>
    concept Serializable = requires(const T& t) {
        { t.serialize() } -> std::convertible_to<string>;
    };
    
    template<typename T>
    concept Deserializable = requires(T& t, string_view data) {
        { t.deserialize(data) } -> std::convertible_to<bool>;
    };
    
    template<typename T>
    concept Hashable = requires(const T& t) {
        { std::hash<T>{}(t) } -> std::convertible_to<size_type>;
    };
    
    template<typename T>
    concept Comparable = requires(const T& a, const T& b) {
        { a == b } -> std::convertible_to<bool>;
        { a != b } -> std::convertible_to<bool>;
        { a < b } -> std::convertible_to<bool>;
    };
} 