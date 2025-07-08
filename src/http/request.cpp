#include "http/request.hpp"
#include "utils/logger.hpp"
#include <algorithm>
#include <regex>
#include <sstream>
#include <cctype>

namespace http_framework::http {
    
void Request::add_header(string_view name, string_view value) {
    headers_.emplace_back(normalize_header_name(name), string(value));
}

void Request::set_header(string_view name, string_view value) {
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

void Request::remove_header(string_view name) {
    auto normalized_name = normalize_header_name(name);
    headers_.erase(
        std::remove_if(headers_.begin(), headers_.end(),
            [&normalized_name](const HttpHeader& header) {
                return header.name == normalized_name;
            }),
        headers_.end());
}

bool Request::has_header(string_view name) const {
    auto normalized_name = normalize_header_name(name);
    return std::any_of(headers_.begin(), headers_.end(),
        [&normalized_name](const HttpHeader& header) {
            return header.name == normalized_name;
        });
}

optional<string> Request::get_header(string_view name) const {
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

void Request::add_cookie(const HttpCookie& cookie) {
    cookies_.push_back(cookie);
}

void Request::set_cookie(string_view name, string_view value) {
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

void Request::remove_cookie(string_view name) {
    cookies_.erase(
        std::remove_if(cookies_.begin(), cookies_.end(),
            [name](const HttpCookie& cookie) {
                return cookie.name == name;
            }),
        cookies_.end());
}

bool Request::has_cookie(string_view name) const {
    return std::any_of(cookies_.begin(), cookies_.end(),
        [name](const HttpCookie& cookie) {
            return cookie.name == name;
        });
}

optional<string> Request::get_cookie(string_view name) const {
    auto it = std::find_if(cookies_.begin(), cookies_.end(),
        [name](const HttpCookie& cookie) {
            return cookie.name == name;
        });
    
    if (it != cookies_.end()) {
        return it->value;
    }
    return std::nullopt;
}

void Request::set_query_param(string_view name, string_view value) {
    query_params_[string(name)] = string(value);
}

void Request::remove_query_param(string_view name) {
    query_params_.erase(string(name));
}

bool Request::has_query_param(string_view name) const {
    return query_params_.find(string(name)) != query_params_.end();
}

optional<string> Request::get_query_param(string_view name) const {
    auto it = query_params_.find(string(name));
    if (it != query_params_.end()) {
        return it->second;
    }
    return std::nullopt;
}

void Request::set_path_param(string_view name, string_view value) {
    path_params_[string(name)] = string(value);
}

bool Request::has_path_param(string_view name) const {
    return path_params_.find(string(name)) != path_params_.end();
}

optional<string> Request::get_path_param(string_view name) const {
    auto it = path_params_.find(string(name));
    if (it != path_params_.end()) {
        return it->second;
    }
    return std::nullopt;
}

string Request::body_as_string() const {
    return string(body_.begin(), body_.end());
}

bool Request::is_websocket_upgrade() const {
    auto upgrade = get_header("Upgrade");
    auto connection = get_header("Connection");
    auto websocket_key = get_header("Sec-WebSocket-Key");
    auto websocket_version = get_header("Sec-WebSocket-Version");
    
    return upgrade && connection && websocket_key && websocket_version &&
           upgrade->find("websocket") != string::npos &&
           connection->find("Upgrade") != string::npos;
}

bool Request::is_chunked() const {
    auto transfer_encoding = get_header("Transfer-Encoding");
    return transfer_encoding && transfer_encoding->find("chunked") != string::npos;
}

bool Request::is_keep_alive() const {
    auto connection = get_header("Connection");
    if (connection) {
        return connection->find("keep-alive") != string::npos;
    }
    return version_ == HttpVersion::HTTP_1_1;
}

optional<size_type> Request::content_length() const {
    auto content_length_header = get_header("Content-Length");
    if (content_length_header) {
        try {
            return std::stoull(*content_length_header);
        } catch (const std::exception&) {
            return std::nullopt;
        }
    }
    return std::nullopt;
}

optional<string> Request::content_type() const {
    return get_header("Content-Type");
}

optional<string> Request::authorization() const {
    return get_header("Authorization");
}

optional<string> Request::user_agent() const {
    return get_header("User-Agent");
}

optional<string> Request::referer() const {
    return get_header("Referer");
}

optional<string> Request::host() const {
    return get_header("Host");
}

optional<string> Request::origin() const {
    return get_header("Origin");
}

string Request::get_path() const {
    if (cached_path_) {
        return *cached_path_;
    }
    
    auto question_pos = uri_.find('?');
    auto fragment_pos = uri_.find('#');
    auto end_pos = std::min(question_pos, fragment_pos);
    
    if (end_pos == string::npos) {
        cached_path_ = uri_;
    } else {
        cached_path_ = uri_.substr(0, end_pos);
    }
    
    return *cached_path_;
}

string Request::get_query_string() const {
    if (cached_query_string_) {
        return *cached_query_string_;
    }
    
    auto question_pos = uri_.find('?');
    if (question_pos == string::npos) {
        cached_query_string_ = "";
        return *cached_query_string_;
    }
    
    auto fragment_pos = uri_.find('#', question_pos);
    if (fragment_pos == string::npos) {
        cached_query_string_ = uri_.substr(question_pos + 1);
    } else {
        cached_query_string_ = uri_.substr(question_pos + 1, fragment_pos - question_pos - 1);
    }
    
    return *cached_query_string_;
}

string Request::get_fragment() const {
    if (cached_fragment_) {
        return *cached_fragment_;
    }
    
    auto fragment_pos = uri_.find('#');
    if (fragment_pos == string::npos) {
        cached_fragment_ = "";
    } else {
        cached_fragment_ = uri_.substr(fragment_pos + 1);
    }
    
    return *cached_fragment_;
}

bool Request::matches_path(const std::regex& pattern) const {
    return std::regex_match(get_path(), pattern);
}

hash_map<string, string> Request::extract_path_variables(const std::regex& pattern, const vector<string>& var_names) const {
    hash_map<string, string> variables;
    std::smatch matches;
    
    if (std::regex_match(get_path(), matches, pattern)) {
        for (size_type i = 1; i < matches.size() && i - 1 < var_names.size(); ++i) {
            variables[var_names[i - 1]] = matches[i].str();
        }
    }
    
    return variables;
}

void Request::parse_uri() {
    cached_path_.reset();
    cached_query_string_.reset();
    cached_fragment_.reset();
    
    get_path();
    get_query_string();
    get_fragment();
}

void Request::parse_query_string() {
    query_params_.clear();
    auto query_string = get_query_string();
    if (query_string.empty()) {
        return;
    }
    
    query_params_ = parse_query_string(query_string);
}

void Request::parse_cookies() {
    cookies_.clear();
    auto cookie_header = get_header("Cookie");
    if (cookie_header) {
        cookies_ = parse_cookie_header(*cookie_header);
    }
}

void Request::parse_form_data() {
    auto content_type_header = content_type();
    if (!content_type_header || content_type_header->find("application/x-www-form-urlencoded") == string::npos) {
        return;
    }
    
    auto form_data = body_as_string();
    auto parsed_params = parse_query_string(form_data);
    
    for (const auto& [key, value] : parsed_params) {
        query_params_[key] = value;
    }
}

void Request::parse_multipart_data() {
    auto content_type_header = content_type();
    if (!content_type_header || content_type_header->find("multipart/form-data") == string::npos) {
        return;
    }
}

bool Request::has_attribute(string_view name) const {
    return attributes_.find(string(name)) != attributes_.end();
}

void Request::remove_attribute(string_view name) {
    attributes_.erase(string(name));
}

string Request::to_string() const {
    std::ostringstream oss;
    
    oss << static_cast<int>(method_) << " " << uri_ << " HTTP/";
    if (version_ == HttpVersion::HTTP_1_0) {
        oss << "1.0";
    } else if (version_ == HttpVersion::HTTP_1_1) {
        oss << "1.1";
    } else if (version_ == HttpVersion::HTTP_2_0) {
        oss << "2.0";
    }
    oss << "\r\n";
    
    for (const auto& header : headers_) {
        oss << header.name << ": " << header.value << "\r\n";
    }
    
    if (!body_.empty()) {
        oss << "Content-Length: " << body_.size() << "\r\n";
    }
    
    oss << "\r\n";
    
    if (!body_.empty()) {
        oss << body_as_string();
    }
    
    return oss.str();
}

Request Request::from_string(string_view data) {
    Request request;
    
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
        return request;
    }
    
    std::istringstream request_line(lines[0]);
    string method_str, uri, version_str;
    request_line >> method_str >> uri >> version_str;
    
    if (method_str == "GET") {
        request.set_method(HttpMethod::GET);
    } else if (method_str == "POST") {
        request.set_method(HttpMethod::POST);
    } else if (method_str == "PUT") {
        request.set_method(HttpMethod::PUT);
    } else if (method_str == "DELETE") {
        request.set_method(HttpMethod::DELETE);
    } else if (method_str == "PATCH") {
        request.set_method(HttpMethod::PATCH);
    } else if (method_str == "HEAD") {
        request.set_method(HttpMethod::HEAD);
    } else if (method_str == "OPTIONS") {
        request.set_method(HttpMethod::OPTIONS);
    }
    
    request.set_uri(uri);
    
    if (version_str == "HTTP/1.0") {
        request.set_version(HttpVersion::HTTP_1_0);
    } else if (version_str == "HTTP/1.1") {
        request.set_version(HttpVersion::HTTP_1_1);
    }
    
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
            
            while (!value.empty() && std::isspace(value[0])) {
                value.erase(0, 1);
            }
            while (!value.empty() && std::isspace(value.back())) {
                value.pop_back();
            }
            
            request.add_header(name, value);
        }
    }
    
    if (header_end + 1 < lines.size()) {
        string body;
        for (size_type i = header_end + 1; i < lines.size(); ++i) {
            if (i > header_end + 1) {
                body += "\n";
            }
            body += lines[i];
        }
        
        buffer_t body_buffer(body.begin(), body.end());
        request.set_body(std::move(body_buffer));
    }
    
    request.parse_uri();
    request.parse_query_string();
    request.parse_cookies();
    
    return request;
}

void Request::reset() {
    method_ = HttpMethod::GET;
    uri_.clear();
    version_ = HttpVersion::HTTP_1_1;
    headers_.clear();
    cookies_.clear();
    body_.clear();
    query_params_.clear();
    path_params_.clear();
    attributes_.clear();
    timestamp_ = std::chrono::steady_clock::now();
    request_id_ = 0;
    
    cached_path_.reset();
    cached_query_string_.reset();
    cached_fragment_.reset();
}

bool Request::is_valid() const {
    return !uri_.empty();
}

string Request::normalize_header_name(string_view name) const {
    string normalized(name);
    std::transform(normalized.begin(), normalized.end(), normalized.begin(),
        [](char c) { return std::tolower(c); });
    return normalized;
}

string Request::url_decode(string_view encoded) const {
    string decoded;
    decoded.reserve(encoded.size());
    
    for (size_type i = 0; i < encoded.size(); ++i) {
        if (encoded[i] == '%' && i + 2 < encoded.size()) {
            auto hex_str = string(encoded.substr(i + 1, 2));
            try {
                char decoded_char = static_cast<char>(std::stoi(hex_str, nullptr, 16));
                decoded += decoded_char;
                i += 2;
            } catch (const std::exception&) {
                decoded += encoded[i];
            }
        } else if (encoded[i] == '+') {
            decoded += ' ';
        } else {
            decoded += encoded[i];
        }
    }
    
    return decoded;
}

hash_map<string, string> Request::parse_query_string(string_view query) const {
    hash_map<string, string> params;
    
    if (query.empty()) {
        return params;
    }
    
    size_type start = 0;
    while (start < query.size()) {
        auto amp_pos = query.find('&', start);
        if (amp_pos == string_view::npos) {
            amp_pos = query.size();
        }
        
        auto param = query.substr(start, amp_pos - start);
        auto eq_pos = param.find('=');
        
        if (eq_pos == string_view::npos) {
            auto key = url_decode(param);
            params[key] = "";
        } else {
            auto key = url_decode(param.substr(0, eq_pos));
            auto value = url_decode(param.substr(eq_pos + 1));
            params[key] = value;
        }
        
        start = amp_pos + 1;
    }
    
    return params;
}

vector<HttpCookie> Request::parse_cookie_header(string_view cookie_header) const {
    vector<HttpCookie> cookies;
    
    size_type start = 0;
    while (start < cookie_header.size()) {
        auto semicolon_pos = cookie_header.find(';', start);
        if (semicolon_pos == string_view::npos) {
            semicolon_pos = cookie_header.size();
        }
        
        auto cookie_str = cookie_header.substr(start, semicolon_pos - start);
        
        while (!cookie_str.empty() && std::isspace(cookie_str[0])) {
            cookie_str.remove_prefix(1);
        }
        while (!cookie_str.empty() && std::isspace(cookie_str.back())) {
            cookie_str.remove_suffix(1);
        }
        
        auto eq_pos = cookie_str.find('=');
        if (eq_pos != string_view::npos) {
            auto name = cookie_str.substr(0, eq_pos);
            auto value = cookie_str.substr(eq_pos + 1);
            
            while (!name.empty() && std::isspace(name.back())) {
                name.remove_suffix(1);
            }
            while (!value.empty() && std::isspace(value[0])) {
                value.remove_prefix(1);
            }
            
            cookies.emplace_back(string(name), string(value));
        }
        
        start = semicolon_pos + 1;
    }
    
    return cookies;
}

}  // namespace http_framework::http 