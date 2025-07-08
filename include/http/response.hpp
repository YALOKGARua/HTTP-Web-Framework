#pragma once

#include "core/types.hpp"
#include <fstream>

namespace http_framework::http {
    class Response {
    public:
        Response() = default;
        Response(const Response&) = default;
        Response(Response&&) = default;
        Response& operator=(const Response&) = default;
        Response& operator=(Response&&) = default;
        ~Response() = default;
        
        void set_status_code(status_code_t code) noexcept { status_code_ = code; }
        void set_status_message(string_view message) { status_message_ = message; }
        void set_version(HttpVersion version) noexcept { version_ = version; }
        void set_body(buffer_t body) { body_ = std::move(body); chunked_ = false; }
        void set_body(string_view body);
        
        status_code_t status_code() const noexcept { return status_code_; }
        const string& status_message() const noexcept { return status_message_; }
        HttpVersion version() const noexcept { return version_; }
        const buffer_t& body() const noexcept { return body_; }
        size_type body_size() const noexcept { return body_.size(); }
        
        void add_header(string_view name, string_view value);
        void set_header(string_view name, string_view value);
        void remove_header(string_view name);
        bool has_header(string_view name) const;
        optional<string> get_header(string_view name) const;
        const vector<HttpHeader>& headers() const noexcept { return headers_; }
        
        void add_cookie(const HttpCookie& cookie);
        void set_cookie(string_view name, string_view value);
        void set_cookie(string_view name, string_view value, duration_t max_age);
        void set_secure_cookie(string_view name, string_view value, duration_t max_age);
        void remove_cookie(string_view name);
        void clear_cookies();
        const vector<HttpCookie>& cookies() const noexcept { return cookies_; }
        
        void set_content_type(string_view content_type);
        void set_content_length(size_type length);
        void set_cache_control(string_view cache_control);
        void set_etag(string_view etag);
        void set_last_modified(timestamp_t timestamp);
        void set_location(string_view location);
        void set_cors_headers(string_view origin = "*");
        
        void enable_compression(CompressionType type);
        void disable_compression();
        bool is_compression_enabled() const noexcept { return compression_type_ != CompressionType::NONE; }
        CompressionType compression_type() const noexcept { return compression_type_; }
        
        void enable_chunked_encoding();
        void disable_chunked_encoding();
        bool is_chunked() const noexcept { return chunked_; }
        
        void append_body_chunk(string_view chunk);
        void append_body_chunk(const buffer_t& chunk);
        void finish_chunked_body();
        
        string body_as_string() const;
        
        template<typename T>
        void set_json_body(const T& data);
        
        void set_html_body(string_view html);
        void set_text_body(string_view text);
        void set_xml_body(string_view xml);
        void set_css_body(string_view css);
        void set_javascript_body(string_view js);
        
        bool send_file(string_view file_path);
        bool send_file_range(string_view file_path, size_type start, size_type end);
        void send_error(status_code_t code, string_view message = "");
        
        void redirect(string_view url, status_code_t code = 302);
        void permanent_redirect(string_view url);
        void temporary_redirect(string_view url);
        
        void set_download_headers(string_view filename);
        void set_no_cache_headers();
        void set_cors_preflight_headers();
        
        string to_string() const;
        buffer_t to_buffer() const;
        static Response from_string(string_view data);
        
        void reset();
        bool is_valid() const;
        bool is_complete() const;
        
        timestamp_t timestamp() const noexcept { return timestamp_; }
        void set_timestamp(timestamp_t ts) noexcept { timestamp_ = ts; }
        
        template<typename T>
        void set_attribute(string_view name, T&& value);
        
        template<typename T>
        optional<T> get_attribute(string_view name) const;
        
        bool has_attribute(string_view name) const;
        void remove_attribute(string_view name);
        
        static Response ok(string_view body = "");
        static Response created(string_view body = "");
        static Response no_content();
        static Response bad_request(string_view message = "Bad Request");
        static Response unauthorized(string_view message = "Unauthorized");
        static Response forbidden(string_view message = "Forbidden");
        static Response not_found(string_view message = "Not Found");
        static Response method_not_allowed(string_view message = "Method Not Allowed");
        static Response internal_server_error(string_view message = "Internal Server Error");
        static Response service_unavailable(string_view message = "Service Unavailable");
        
    private:
        status_code_t status_code_{200};
        string status_message_{"OK"};
        HttpVersion version_{HttpVersion::HTTP_1_1};
        vector<HttpHeader> headers_;
        vector<HttpCookie> cookies_;
        buffer_t body_;
        CompressionType compression_type_{CompressionType::NONE};
        bool chunked_{false};
        bool headers_sent_{false};
        timestamp_t timestamp_{std::chrono::steady_clock::now()};
        hash_map<string, variant<string, int64_t, double, bool>> attributes_;
        
        string normalize_header_name(string_view name) const;
        string get_status_message(status_code_t code) const;
        buffer_t compress_body(const buffer_t& data, CompressionType type) const;
        string format_timestamp(timestamp_t ts) const;
        void update_content_length();
        void add_default_headers();
    };
    
    template<typename T>
    void Response::set_attribute(string_view name, T&& value) {
        attributes_[string(name)] = std::forward<T>(value);
    }
    
    template<typename T>
    optional<T> Response::get_attribute(string_view name) const {
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