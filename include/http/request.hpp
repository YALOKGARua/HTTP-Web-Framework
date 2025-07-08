#pragma once

#include "core/types.hpp"
#include <regex>
#include <sstream>

namespace http_framework::http {
    class Request {
    public:
        Request() = default;
        Request(const Request&) = default;
        Request(Request&&) = default;
        Request& operator=(const Request&) = default;
        Request& operator=(Request&&) = default;
        ~Request() = default;
        
        void set_method(HttpMethod method) noexcept { method_ = method; }
        void set_uri(string_view uri) { uri_ = uri; }
        void set_version(HttpVersion version) noexcept { version_ = version; }
        void set_body(buffer_t body) { body_ = std::move(body); }
        void set_remote_endpoint(const Endpoint& endpoint) { remote_endpoint_ = endpoint; }
        
        HttpMethod method() const noexcept { return method_; }
        const string& uri() const noexcept { return uri_; }
        HttpVersion version() const noexcept { return version_; }
        const buffer_t& body() const noexcept { return body_; }
        const Endpoint& remote_endpoint() const noexcept { return remote_endpoint_; }
        
        void add_header(string_view name, string_view value);
        void set_header(string_view name, string_view value);
        void remove_header(string_view name);
        bool has_header(string_view name) const;
        optional<string> get_header(string_view name) const;
        const vector<HttpHeader>& headers() const noexcept { return headers_; }
        
        void add_cookie(const HttpCookie& cookie);
        void set_cookie(string_view name, string_view value);
        void remove_cookie(string_view name);
        bool has_cookie(string_view name) const;
        optional<string> get_cookie(string_view name) const;
        const vector<HttpCookie>& cookies() const noexcept { return cookies_; }
        
        void set_query_param(string_view name, string_view value);
        void remove_query_param(string_view name);
        bool has_query_param(string_view name) const;
        optional<string> get_query_param(string_view name) const;
        const hash_map<string, string>& query_params() const noexcept { return query_params_; }
        
        void set_path_param(string_view name, string_view value);
        bool has_path_param(string_view name) const;
        optional<string> get_path_param(string_view name) const;
        const hash_map<string, string>& path_params() const noexcept { return path_params_; }
        
        string body_as_string() const;
        template<typename T> optional<T> body_as_json() const;
        
        bool is_websocket_upgrade() const;
        bool is_chunked() const;
        bool is_keep_alive() const;
        optional<size_type> content_length() const;
        optional<string> content_type() const;
        optional<string> authorization() const;
        optional<string> user_agent() const;
        optional<string> referer() const;
        optional<string> host() const;
        optional<string> origin() const;
        
        string get_path() const;
        string get_query_string() const;
        string get_fragment() const;
        
        bool matches_path(const std::regex& pattern) const;
        hash_map<string, string> extract_path_variables(const std::regex& pattern, const vector<string>& var_names) const;
        
        void parse_uri();
        void parse_query_string();
        void parse_cookies();
        void parse_form_data();
        void parse_multipart_data();
        
        timestamp_t timestamp() const noexcept { return timestamp_; }
        void set_timestamp(timestamp_t ts) noexcept { timestamp_ = ts; }
        
        size_type request_id() const noexcept { return request_id_; }
        void set_request_id(size_type id) noexcept { request_id_ = id; }
        
        template<typename T>
        void set_attribute(string_view name, T&& value);
        
        template<typename T>
        optional<T> get_attribute(string_view name) const;
        
        bool has_attribute(string_view name) const;
        void remove_attribute(string_view name);
        
        string to_string() const;
        static Request from_string(string_view data);
        
        void reset();
        bool is_valid() const;
        
    private:
        HttpMethod method_{HttpMethod::GET};
        string uri_;
        HttpVersion version_{HttpVersion::HTTP_1_1};
        vector<HttpHeader> headers_;
        vector<HttpCookie> cookies_;
        buffer_t body_;
        Endpoint remote_endpoint_;
        hash_map<string, string> query_params_;
        hash_map<string, string> path_params_;
        hash_map<string, variant<string, int64_t, double, bool>> attributes_;
        timestamp_t timestamp_{std::chrono::steady_clock::now()};
        size_type request_id_{0};
        
        mutable optional<string> cached_path_;
        mutable optional<string> cached_query_string_;
        mutable optional<string> cached_fragment_;
        
        string normalize_header_name(string_view name) const;
        string url_decode(string_view encoded) const;
        hash_map<string, string> parse_query_string(string_view query) const;
        vector<HttpCookie> parse_cookie_header(string_view cookie_header) const;
    };
    
    template<typename T>
    void Request::set_attribute(string_view name, T&& value) {
        attributes_[string(name)] = std::forward<T>(value);
    }
    
    template<typename T>
    optional<T> Request::get_attribute(string_view name) const {
        auto it = attributes_.find(string(name));
        if (it == attributes_.end()) {
            return std::nullopt;
        }
        
        if (std::holds_alternative<T>(it->second)) {
            return std::get<T>(it->second);
        }
        
        return std::nullopt;
    }
} 