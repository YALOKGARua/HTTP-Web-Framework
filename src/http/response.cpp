#include "http/response.hpp"
#include "utils/logger.hpp"
#include <algorithm>
#include <sstream>
#include <fstream>
#include <iomanip>
#include <ctime>

namespace http_framework::http {

void Response::set_body(string_view body) {
    body_.assign(body.begin(), body.end());
    chunked_ = false;
    update_content_length();
}

void Response::add_header(string_view name, string_view value) {
    headers_.emplace_back(normalize_header_name(name), string(value));
}

void Response::set_header(string_view name, string_view value) {
    auto normalized_name = normalize_header_name(name);
    auto it = std::find_if(headers_.begin(), headers_.end(),
        [&normalized_name](const HttpHeader& header) {
            return header.name == normalized_name;
        });
    
    if (it != headers_.end()) {
        it->value = value;
    } else {
        headers_.emplace_back(normalized_name, string(value));
    }
}

void Response::remove_header(string_view name) {
    auto normalized_name = normalize_header_name(name);
    headers_.erase(
        std::remove_if(headers_.begin(), headers_.end(),
            [&normalized_name](const HttpHeader& header) {
                return header.name == normalized_name;
            }),
        headers_.end());
}

bool Response::has_header(string_view name) const {
    auto normalized_name = normalize_header_name(name);
    return std::any_of(headers_.begin(), headers_.end(),
        [&normalized_name](const HttpHeader& header) {
            return header.name == normalized_name;
        });
}

optional<string> Response::get_header(string_view name) const {
    auto normalized_name = normalize_header_name(name);
    auto it = std::find_if(headers_.begin(), headers_.end(),
        [&normalized_name](const HttpHeader& header) {
            return header.name == normalized_name;
        });
    
    if (it != headers_.end()) {
        return it->value;
    }
    return std::nullopt;
}

void Response::add_cookie(const HttpCookie& cookie) {
    cookies_.push_back(cookie);
}

void Response::set_cookie(string_view name, string_view value) {
    auto it = std::find_if(cookies_.begin(), cookies_.end(),
        [name](const HttpCookie& cookie) {
            return cookie.name == name;
        });
    
    if (it != cookies_.end()) {
        it->value = value;
    } else {
        cookies_.emplace_back(string(name), string(value));
    }
}

void Response::set_cookie(string_view name, string_view value, duration_t max_age) {
    HttpCookie cookie(name, value);
    cookie.max_age = max_age;
    
    auto it = std::find_if(cookies_.begin(), cookies_.end(),
        [name](const HttpCookie& cookie) {
            return cookie.name == name;
        });
    
    if (it != cookies_.end()) {
        *it = cookie;
    } else {
        cookies_.push_back(cookie);
    }
}

void Response::set_secure_cookie(string_view name, string_view value, duration_t max_age) {
    HttpCookie cookie(name, value);
    cookie.max_age = max_age;
    cookie.secure = true;
    cookie.http_only = true;
    
    auto it = std::find_if(cookies_.begin(), cookies_.end(),
        [name](const HttpCookie& cookie) {
            return cookie.name == name;
        });
    
    if (it != cookies_.end()) {
        *it = cookie;
    } else {
        cookies_.push_back(cookie);
    }
}

void Response::remove_cookie(string_view name) {
    cookies_.erase(
        std::remove_if(cookies_.begin(), cookies_.end(),
            [name](const HttpCookie& cookie) {
                return cookie.name == name;
            }),
        cookies_.end());
}

void Response::clear_cookies() {
    cookies_.clear();
}

void Response::set_content_type(string_view content_type) {
    set_header("Content-Type", content_type);
}

void Response::set_content_length(size_type length) {
    set_header("Content-Length", std::to_string(length));
}

void Response::set_cache_control(string_view cache_control) {
    set_header("Cache-Control", cache_control);
}

void Response::set_etag(string_view etag) {
    set_header("ETag", etag);
}

void Response::set_last_modified(timestamp_t timestamp) {
    set_header("Last-Modified", format_timestamp(timestamp));
}

void Response::set_location(string_view location) {
    set_header("Location", location);
}

void Response::set_cors_headers(string_view origin) {
    set_header("Access-Control-Allow-Origin", origin);
    set_header("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
    set_header("Access-Control-Allow-Headers", "Content-Type, Authorization");
}

void Response::enable_compression(CompressionType type) {
    compression_type_ = type;
    
    switch (type) {
        case CompressionType::GZIP:
            set_header("Content-Encoding", "gzip");
            break;
        case CompressionType::DEFLATE:
            set_header("Content-Encoding", "deflate");
            break;
        case CompressionType::BROTLI:
            set_header("Content-Encoding", "br");
            break;
        default:
            remove_header("Content-Encoding");
            break;
    }
}

void Response::disable_compression() {
    compression_type_ = CompressionType::NONE;
    remove_header("Content-Encoding");
}

void Response::enable_chunked_encoding() {
    chunked_ = true;
    set_header("Transfer-Encoding", "chunked");
    remove_header("Content-Length");
}

void Response::disable_chunked_encoding() {
    chunked_ = false;
    remove_header("Transfer-Encoding");
    update_content_length();
}

void Response::append_body_chunk(string_view chunk) {
    if (!chunked_) {
        enable_chunked_encoding();
    }
    
    body_.insert(body_.end(), chunk.begin(), chunk.end());
}

void Response::append_body_chunk(const buffer_t& chunk) {
    if (!chunked_) {
        enable_chunked_encoding();
    }
    
    body_.insert(body_.end(), chunk.begin(), chunk.end());
}

void Response::finish_chunked_body() {
    if (chunked_) {
        // Add final chunk marker (would be handled in serialization)
    }
}

string Response::body_as_string() const {
    return string(body_.begin(), body_.end());
}

void Response::set_html_body(string_view html) {
    set_body(html);
    set_content_type("text/html; charset=utf-8");
}

void Response::set_text_body(string_view text) {
    set_body(text);
    set_content_type("text/plain; charset=utf-8");
}

void Response::set_xml_body(string_view xml) {
    set_body(xml);
    set_content_type("application/xml; charset=utf-8");
}

void Response::set_css_body(string_view css) {
    set_body(css);
    set_content_type("text/css; charset=utf-8");
}

void Response::set_javascript_body(string_view js) {
    set_body(js);
    set_content_type("application/javascript; charset=utf-8");
}

bool Response::send_file(string_view file_path) {
    std::ifstream file(string(file_path), std::ios::binary);
    if (!file.is_open()) {
        return false;
    }
    
    file.seekg(0, std::ios::end);
    auto file_size = file.tellg();
    file.seekg(0, std::ios::beg);
    
    body_.resize(file_size);
    file.read(reinterpret_cast<char*>(body_.data()), file_size);
    file.close();
    
    // Set appropriate content type based on file extension
    auto ext_pos = file_path.rfind('.');
    if (ext_pos != string_view::npos) {
        auto extension = file_path.substr(ext_pos + 1);
        
        if (extension == "html" || extension == "htm") {
            set_content_type("text/html");
        } else if (extension == "css") {
            set_content_type("text/css");
        } else if (extension == "js") {
            set_content_type("application/javascript");
        } else if (extension == "json") {
            set_content_type("application/json");
        } else if (extension == "xml") {
            set_content_type("application/xml");
        } else if (extension == "png") {
            set_content_type("image/png");
        } else if (extension == "jpg" || extension == "jpeg") {
            set_content_type("image/jpeg");
        } else if (extension == "gif") {
            set_content_type("image/gif");
        } else if (extension == "svg") {
            set_content_type("image/svg+xml");
        } else if (extension == "pdf") {
            set_content_type("application/pdf");
        } else {
            set_content_type("application/octet-stream");
        }
    }
    
    update_content_length();
    return true;
}

bool Response::send_file_range(string_view file_path, size_type start, size_type end) {
    std::ifstream file(string(file_path), std::ios::binary);
    if (!file.is_open()) {
        return false;
    }
    
    file.seekg(0, std::ios::end);
    auto file_size = static_cast<size_type>(file.tellg());
    
    if (start >= file_size || end >= file_size || start > end) {
        return false;
    }
    
    file.seekg(start, std::ios::beg);
    auto range_size = end - start + 1;
    
    body_.resize(range_size);
    file.read(reinterpret_cast<char*>(body_.data()), range_size);
    file.close();
    
    set_status_code(206); // Partial Content
    set_header("Content-Range", "bytes " + std::to_string(start) + "-" + 
               std::to_string(end) + "/" + std::to_string(file_size));
    set_header("Accept-Ranges", "bytes");
    
    update_content_length();
    return true;
}

void Response::send_error(status_code_t code, string_view message) {
    set_status_code(code);
    set_status_message(message.empty() ? get_status_message(code) : string(message));
    
    string error_html = "<!DOCTYPE html><html><head><title>Error " + 
                       std::to_string(code) + "</title></head><body>";
    error_html += "<h1>Error " + std::to_string(code) + "</h1>";
    error_html += "<p>" + status_message_ + "</p>";
    error_html += "</body></html>";
    
    set_html_body(error_html);
}

void Response::redirect(string_view url, status_code_t code) {
    set_status_code(code);
    set_location(url);
    
    string redirect_html = "<!DOCTYPE html><html><head><title>Redirect</title></head><body>";
    redirect_html += "<p>Redirecting to <a href=\"" + string(url) + "\">" + string(url) + "</a></p>";
    redirect_html += "</body></html>";
    
    set_html_body(redirect_html);
}

void Response::permanent_redirect(string_view url) {
    redirect(url, 301);
}

void Response::temporary_redirect(string_view url) {
    redirect(url, 302);
}

void Response::set_download_headers(string_view filename) {
    set_header("Content-Disposition", "attachment; filename=\"" + string(filename) + "\"");
    set_content_type("application/octet-stream");
}

void Response::set_no_cache_headers() {
    set_cache_control("no-cache, no-store, must-revalidate");
    set_header("Pragma", "no-cache");
    set_header("Expires", "0");
}

void Response::set_cors_preflight_headers() {
    set_cors_headers();
    set_header("Access-Control-Max-Age", "86400");
}

string Response::to_string() const {
    std::ostringstream oss;
    
    // Status line
    oss << "HTTP/";
    if (version_ == HttpVersion::HTTP_1_0) {
        oss << "1.0";
    } else if (version_ == HttpVersion::HTTP_1_1) {
        oss << "1.1";
    } else if (version_ == HttpVersion::HTTP_2_0) {
        oss << "2.0";
    }
    oss << " " << status_code_ << " " << status_message_ << "\r\n";
    
    // Headers
    for (const auto& header : headers_) {
        oss << header.name << ": " << header.value << "\r\n";
    }
    
    // Cookies
    for (const auto& cookie : cookies_) {
        oss << "Set-Cookie: " << cookie.name << "=" << cookie.value;
        
        if (cookie.domain) {
            oss << "; Domain=" << *cookie.domain;
        }
        if (cookie.path) {
            oss << "; Path=" << *cookie.path;
        }
        if (cookie.max_age) {
            oss << "; Max-Age=" << cookie.max_age->count();
        }
        if (cookie.secure) {
            oss << "; Secure";
        }
        if (cookie.http_only) {
            oss << "; HttpOnly";
        }
        if (cookie.same_site) {
            oss << "; SameSite=" << *cookie.same_site;
        }
        
        oss << "\r\n";
    }
    
    oss << "\r\n";
    
    // Body
    if (!body_.empty()) {
        oss << body_as_string();
    }
    
    return oss.str();
}

buffer_t Response::to_buffer() const {
    auto response_str = to_string();
    return buffer_t(response_str.begin(), response_str.end());
}

Response Response::from_string(string_view data) {
    Response response;
    
    auto lines = vector<string>();
    std::istringstream stream(string(data));
    string line;
    
    while (std::getline(stream, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        lines.push_back(line);
    }
    
    if (lines.empty()) {
        return response;
    }
    
    // Parse status line
    std::istringstream status_line(lines[0]);
    string version_str, status_code_str, status_message;
    status_line >> version_str >> status_code_str;
    std::getline(status_line, status_message);
    
    if (!status_message.empty() && status_message[0] == ' ') {
        status_message.erase(0, 1);
    }
    
    response.set_status_code(static_cast<status_code_t>(std::stoi(status_code_str)));
    response.set_status_message(status_message);
    
    if (version_str == "HTTP/1.0") {
        response.set_version(HttpVersion::HTTP_1_0);
    } else if (version_str == "HTTP/1.1") {
        response.set_version(HttpVersion::HTTP_1_1);
    }
    
    // Parse headers
    size_type header_end = 1;
    for (size_type i = 1; i < lines.size(); ++i) {
        if (lines[i].empty()) {
            header_end = i;
            break;
        }
        
        auto colon_pos = lines[i].find(':');
        if (colon_pos != string::npos) {
            auto name = lines[i].substr(0, colon_pos);
            auto value = lines[i].substr(colon_pos + 1);
            
            // Trim whitespace
            while (!value.empty() && std::isspace(value[0])) {
                value.erase(0, 1);
            }
            while (!value.empty() && std::isspace(value.back())) {
                value.pop_back();
            }
            
            response.add_header(name, value);
        }
    }
    
    // Parse body
    if (header_end + 1 < lines.size()) {
        string body;
        for (size_type i = header_end + 1; i < lines.size(); ++i) {
            if (i > header_end + 1) {
                body += "\n";
            }
            body += lines[i];
        }
        
        response.set_body(body);
    }
    
    return response;
}

void Response::reset() {
    status_code_ = 200;
    status_message_ = "OK";
    version_ = HttpVersion::HTTP_1_1;
    headers_.clear();
    cookies_.clear();
    body_.clear();
    compression_type_ = CompressionType::NONE;
    chunked_ = false;
    headers_sent_ = false;
    timestamp_ = std::chrono::steady_clock::now();
    attributes_.clear();
}

bool Response::is_valid() const {
    return status_code_ >= 100 && status_code_ < 600;
}

bool Response::is_complete() const {
    return !body_.empty() || status_code_ == 204 || status_code_ == 304;
}

Response Response::ok(string_view body) {
    Response response;
    response.set_status_code(200);
    response.set_status_message("OK");
    if (!body.empty()) {
        response.set_body(body);
    }
    return response;
}

Response Response::created(string_view body) {
    Response response;
    response.set_status_code(201);
    response.set_status_message("Created");
    if (!body.empty()) {
        response.set_body(body);
    }
    return response;
}

Response Response::no_content() {
    Response response;
    response.set_status_code(204);
    response.set_status_message("No Content");
    return response;
}

Response Response::bad_request(string_view message) {
    Response response;
    response.send_error(400, message);
    return response;
}

Response Response::unauthorized(string_view message) {
    Response response;
    response.send_error(401, message);
    return response;
}

Response Response::forbidden(string_view message) {
    Response response;
    response.send_error(403, message);
    return response;
}

Response Response::not_found(string_view message) {
    Response response;
    response.send_error(404, message);
    return response;
}

Response Response::method_not_allowed(string_view message) {
    Response response;
    response.send_error(405, message);
    return response;
}

Response Response::internal_server_error(string_view message) {
    Response response;
    response.send_error(500, message);
    return response;
}

Response Response::service_unavailable(string_view message) {
    Response response;
    response.send_error(503, message);
    return response;
}

string Response::normalize_header_name(string_view name) const {
    string normalized(name);
    bool capitalize_next = true;
    
    for (char& c : normalized) {
        if (c == '-') {
            capitalize_next = true;
        } else if (capitalize_next) {
            c = std::toupper(c);
            capitalize_next = false;
        } else {
            c = std::tolower(c);
        }
    }
    
    return normalized;
}

string Response::get_status_message(status_code_t code) const {
    switch (code) {
        case 100: return "Continue";
        case 101: return "Switching Protocols";
        case 200: return "OK";
        case 201: return "Created";
        case 202: return "Accepted";
        case 204: return "No Content";
        case 206: return "Partial Content";
        case 300: return "Multiple Choices";
        case 301: return "Moved Permanently";
        case 302: return "Found";
        case 304: return "Not Modified";
        case 400: return "Bad Request";
        case 401: return "Unauthorized";
        case 403: return "Forbidden";
        case 404: return "Not Found";
        case 405: return "Method Not Allowed";
        case 409: return "Conflict";
        case 413: return "Payload Too Large";
        case 414: return "URI Too Long";
        case 415: return "Unsupported Media Type";
        case 429: return "Too Many Requests";
        case 500: return "Internal Server Error";
        case 501: return "Not Implemented";
        case 502: return "Bad Gateway";
        case 503: return "Service Unavailable";
        case 504: return "Gateway Timeout";
        default: return "Unknown Status";
    }
}

buffer_t Response::compress_body(const buffer_t& data, CompressionType type) const {
    // Compression implementation would go here
    // For now, return the data as-is
    return data;
}

string Response::format_timestamp(timestamp_t ts) const {
    auto time_t_val = std::chrono::system_clock::to_time_t(
        std::chrono::system_clock::now() + 
        std::chrono::duration_cast<std::chrono::system_clock::duration>(
            ts.time_since_epoch()));
    
    std::ostringstream oss;
    oss << std::put_time(std::gmtime(&time_t_val), "%a, %d %b %Y %H:%M:%S GMT");
    return oss.str();
}

void Response::update_content_length() {
    if (!chunked_ && !body_.empty()) {
        set_content_length(body_.size());
    }
}

void Response::add_default_headers() {
    if (!has_header("Server")) {
        set_header("Server", "HttpFramework/1.0");
    }
    
    if (!has_header("Date")) {
        set_header("Date", format_timestamp(std::chrono::steady_clock::now()));
    }
    
    if (!has_header("Connection")) {
        set_header("Connection", "keep-alive");
    }
}

}  // namespace http_framework::http 