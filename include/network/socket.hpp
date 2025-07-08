#pragma once

#include "core/types.hpp"

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <netdb.h>
#endif

namespace http_framework::network {
    
enum class SocketType {
    TCP,
    UDP,
    UNIX_SOCKET
};

enum class ProtocolFamily {
    IPv4,
    IPv6,
    UNSPEC
};

class Socket {
public:
    Socket() = default;
    explicit Socket(socket_t fd);
    Socket(SocketType type, ProtocolFamily family = ProtocolFamily::IPv4);
    Socket(const Socket&) = delete;
    Socket(Socket&& other) noexcept;
    Socket& operator=(const Socket&) = delete;
    Socket& operator=(Socket&& other) noexcept;
    ~Socket();
    
    bool create(SocketType type, ProtocolFamily family = ProtocolFamily::IPv4);
    bool bind(const Endpoint& endpoint);
    bool listen(int backlog = 128);
    unique_ptr<Socket> accept(Endpoint& client_endpoint);
    bool connect(const Endpoint& endpoint);
    
    ssize_type send(byte_span data, int flags = 0);
    ssize_type receive(mutable_byte_span buffer, int flags = 0);
    
    ssize_type send_to(byte_span data, const Endpoint& endpoint, int flags = 0);
    ssize_type receive_from(mutable_byte_span buffer, Endpoint& endpoint, int flags = 0);
    
    async::Task<ssize_type> send_async(byte_span data, int flags = 0);
    async::Task<ssize_type> receive_async(mutable_byte_span buffer, int flags = 0);
    
    async::Task<unique_ptr<Socket>> accept_async(Endpoint& client_endpoint);
    async::Task<bool> connect_async(const Endpoint& endpoint);
    
    bool set_blocking(bool blocking);
    bool is_blocking() const;
    
    bool set_reuse_address(bool reuse);
    bool set_reuse_port(bool reuse);
    bool set_tcp_nodelay(bool nodelay);
    bool set_tcp_keepalive(bool keepalive);
    bool set_tcp_keepalive_params(int idle_time, int interval, int probe_count);
    
    bool set_send_buffer_size(size_type size);
    bool set_receive_buffer_size(size_type size);
    size_type get_send_buffer_size() const;
    size_type get_receive_buffer_size() const;
    
    bool set_send_timeout(duration_t timeout);
    bool set_receive_timeout(duration_t timeout);
    duration_t get_send_timeout() const;
    duration_t get_receive_timeout() const;
    
    bool set_linger(bool enable, int timeout_seconds = 0);
    bool set_broadcast(bool enable);
    bool set_multicast_loopback(bool enable);
    bool set_multicast_ttl(int ttl);
    
    bool join_multicast_group(const IpAddress& group_address, const IpAddress& interface_address = {});
    bool leave_multicast_group(const IpAddress& group_address, const IpAddress& interface_address = {});
    
    void close();
    bool is_valid() const noexcept { return fd_ != INVALID_SOCKET_VALUE; }
    bool is_connected() const;
    
    socket_t native_handle() const noexcept { return fd_; }
    SocketType type() const noexcept { return type_; }
    ProtocolFamily family() const noexcept { return family_; }
    
    Endpoint local_endpoint() const;
    Endpoint remote_endpoint() const;
    
    bool has_pending_data() const;
    size_type pending_data_size() const;
    
    bool wait_for_read(duration_t timeout = duration_t::max()) const;
    bool wait_for_write(duration_t timeout = duration_t::max()) const;
    bool wait_for_connect(duration_t timeout = duration_t::max()) const;
    
    enum class SelectResult {
        READY,
        TIMEOUT,
        ERROR
    };
    
    SelectResult select_read(duration_t timeout = duration_t::max()) const;
    SelectResult select_write(duration_t timeout = duration_t::max()) const;
    SelectResult select_error(duration_t timeout = duration_t::max()) const;
    
    bool shutdown_send();
    bool shutdown_receive();
    bool shutdown_both();
    
    int get_error() const;
    string get_error_string() const;
    string get_last_error_string() const;
    
    static bool initialize_networking();
    static void cleanup_networking();
    static string ip_address_to_string(const IpAddress& address);
    static IpAddress string_to_ip_address(string_view address_str);
    static vector<IpAddress> resolve_hostname(string_view hostname, ProtocolFamily family = ProtocolFamily::UNSPEC);
    static string get_hostname();
    static vector<IpAddress> get_local_addresses(ProtocolFamily family = ProtocolFamily::UNSPEC);
    
    hash_map<string, variant<string, int64_t, double, bool>> get_socket_info() const;
    
    bool enable_ssl(string_view cert_path = "", string_view key_path = "");
    bool disable_ssl();
    bool is_ssl_enabled() const noexcept { return ssl_enabled_; }
    
    ssize_type ssl_send(byte_span data);
    ssize_type ssl_receive(mutable_byte_span buffer);
    bool ssl_handshake();
    bool ssl_shutdown();
    
    string get_peer_certificate() const;
    vector<string> get_peer_certificate_chain() const;
    bool verify_peer_certificate() const;
    
private:
    socket_t fd_{INVALID_SOCKET_VALUE};
    SocketType type_{SocketType::TCP};
    ProtocolFamily family_{ProtocolFamily::IPv4};
    bool blocking_{true};
    bool ssl_enabled_{false};
    void* ssl_context_{nullptr};
    void* ssl_connection_{nullptr};
    
#ifdef _WIN32
    static constexpr socket_t INVALID_SOCKET_VALUE = INVALID_SOCKET;
#else
    static constexpr socket_t INVALID_SOCKET_VALUE = -1;
#endif
    
    static atomic<bool> networking_initialized_;
    
    int get_socket_domain() const;
    int get_socket_type() const;
    int get_socket_protocol() const;
    
    bool set_socket_option(int level, int option, const void* value, socklen_t value_len);
    bool get_socket_option(int level, int option, void* value, socklen_t* value_len) const;
    
    template<typename T>
    bool set_socket_option(int level, int option, const T& value) {
        return set_socket_option(level, option, &value, sizeof(value));
    }
    
    template<typename T>
    bool get_socket_option(int level, int option, T& value) const {
        socklen_t len = sizeof(value);
        return get_socket_option(level, option, &value, &len);
    }
    
    static sockaddr_storage endpoint_to_sockaddr(const Endpoint& endpoint);
    static Endpoint sockaddr_to_endpoint(const sockaddr_storage& addr, socklen_t addr_len);
    
    void cleanup_ssl();
    bool initialize_ssl_context();
    bool load_ssl_certificates(string_view cert_path, string_view key_path);
    
    static void handle_socket_error(const string& operation);
    static int get_last_socket_error();
    static string socket_error_to_string(int error_code);
};

class SocketAddress {
public:
    SocketAddress() = default;
    explicit SocketAddress(const Endpoint& endpoint);
    SocketAddress(const IpAddress& address, port_t port);
    SocketAddress(string_view address_str, port_t port);
    
    const sockaddr* data() const noexcept { return reinterpret_cast<const sockaddr*>(&storage_); }
    sockaddr* data() noexcept { return reinterpret_cast<sockaddr*>(&storage_); }
    socklen_t size() const noexcept { return size_; }
    
    Endpoint to_endpoint() const;
    string to_string() const;
    
    bool is_ipv4() const noexcept;
    bool is_ipv6() const noexcept;
    bool is_loopback() const;
    bool is_multicast() const;
    bool is_broadcast() const;
    
private:
    sockaddr_storage storage_{};
    socklen_t size_{0};
    
    void set_from_endpoint(const Endpoint& endpoint);
};

class SocketSelector {
public:
    SocketSelector() = default;
    SocketSelector(const SocketSelector&) = delete;
    SocketSelector(SocketSelector&&) = default;
    SocketSelector& operator=(const SocketSelector&) = delete;
    SocketSelector& operator=(SocketSelector&&) = default;
    ~SocketSelector() = default;
    
    void add_read_socket(const Socket& socket);
    void add_write_socket(const Socket& socket);
    void add_error_socket(const Socket& socket);
    
    void remove_read_socket(const Socket& socket);
    void remove_write_socket(const Socket& socket);
    void remove_error_socket(const Socket& socket);
    
    void clear_read_sockets();
    void clear_write_sockets();
    void clear_error_sockets();
    void clear_all_sockets();
    
    int select(duration_t timeout = duration_t::max());
    
    bool is_ready_for_read(const Socket& socket) const;
    bool is_ready_for_write(const Socket& socket) const;
    bool has_error(const Socket& socket) const;
    
    vector<socket_t> get_ready_read_sockets() const;
    vector<socket_t> get_ready_write_sockets() const;
    vector<socket_t> get_error_sockets() const;
    
    size_type read_socket_count() const { return read_sockets_.size(); }
    size_type write_socket_count() const { return write_sockets_.size(); }
    size_type error_socket_count() const { return error_sockets_.size(); }
    
private:
    vector<socket_t> read_sockets_;
    vector<socket_t> write_sockets_;
    vector<socket_t> error_sockets_;
    
    vector<socket_t> ready_read_sockets_;
    vector<socket_t> ready_write_sockets_;
    vector<socket_t> ready_error_sockets_;
    
    void prepare_fd_sets(fd_set& read_set, fd_set& write_set, fd_set& error_set, socket_t& max_fd) const;
    void process_fd_sets(const fd_set& read_set, const fd_set& write_set, const fd_set& error_set);
};

}  // namespace http_framework::network 