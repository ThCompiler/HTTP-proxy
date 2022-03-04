#include "request_parser.hpp"

#include <map>

static const char * METHOD = "method";
static const char * URL = "url";
static const char * VERSION = "version";
static const char * BODY = "body";
static const char * HEADERS = "headers";
static const char * COOKIE = "cookies";

// trim from start (in place)
static inline void ltrim(std::string &s) {
    s.erase(s.begin(), std::find_if(s.begin(), s.end(),
                                    std::not1(std::ptr_fun<int, int>(std::isspace))));
}

// trim from end (in place)
static inline void rtrim(std::string &s) {
    s.erase(std::find_if(s.rbegin(), s.rend(),
                         std::not1(std::ptr_fun<int, int>(std::isspace))).base(), s.end());
}

// trim from both ends (in place)
static inline std::string trim(std::string s) {
    ltrim(s);
    rtrim(s);
    return s;
}

std::size_t replace_all(std::string& inout, std::string_view what, std::string_view with) {
    std::size_t count{};
    for (std::string::size_type pos{};
         std::string::npos != (pos = inout.find(what.data(), pos, what.length()));
         pos += with.length(), ++count) {
        inout.replace(pos, what.length(), with.data(), with.length());
    }
    return count;
}


bool parse_first_line(nj::json& request, const std::string &text) {
    auto end = text.find(' ');
    if (end == std::string::npos) {
        return false;
    }
    request[METHOD] = trim(text.substr(0, end));
    auto next_end = text.find(' ', end + 1);
    if (next_end == std::string::npos) {
        return false;
    }
    request[URL] = trim(text.substr(end, next_end - end));
    if (next_end == text.size()) {
        return false;
    }

    request[VERSION] = trim(text.substr(next_end, text.size() - next_end));
    return true;
}

std::map<std::string, std::string> parse_cookies(const std::string &text) {
    size_t end = 0;
    size_t next_end = text.find(';');
    std::map<std::string, std::string> res;
    while(next_end != std::string::npos) {
        auto cookie = trim(text.substr(end, next_end - end));
        auto end_value = cookie.find('=');
        if (end_value == std::string::npos) {
            return {};
        }

        auto name = trim(cookie.substr(0, end_value));
        auto value = trim(cookie.substr(end_value + 1));
        res[name] = value;
        next_end = text.find(';', end + 1);
    }

    auto cookie = trim(text.substr(end + 1, text.size() - end));
    auto end_value = cookie.find('=');
    auto name = trim(cookie.substr(0, end_value));
    auto value = trim(cookie.substr(end_value + 1));
    res[name] = value;

    return res;
}

bool parse_headers(nj::json& request, const std::string &text) {
    size_t end = 0;
    size_t next_end = text.find(": ");
    nj::json tmp;
    request[COOKIE] = nj::json();
    while(next_end != std::string::npos) {
        auto name = trim(text.substr(end, next_end - end));
        end = text.find('\n', next_end + 1);
        if (end == std::string::npos) {
            return false;
        }
        auto tm = trim(text.substr(next_end + 1, end - next_end - 1));
        next_end = text.find(": ", end + 1);

        auto lower_name = name;
        std::transform(lower_name.begin(), lower_name.end(), lower_name.begin(),
                       [](unsigned char c){ return std::tolower(c); });
        if (lower_name == "cookie") {
            auto cookies = parse_cookies(tm);
            if (cookies.empty()) {
                return false;
            }
            request[COOKIE] = nj::json(cookies);
            continue;
        }

        tmp[name] = tm;

    }
    request[HEADERS] = tmp;
    return true;
}

bool http::Request::parse(const std::string &request) {
    auto request_ = request;
    replace_all(request_, "\r", "");
    auto end_first_line = request_.find('\n');
    if (end_first_line == std::string::npos) {
        return false;
    }
    auto first_line = trim(request_.substr(0, end_first_line));
    request_ = request_.substr(end_first_line + 1, request_.size() - end_first_line - 1);

    auto end_headers = request_.find("\n\n");
    if (end_headers == std::string::npos) {
        return false;
    }
    auto headers = trim(request_.substr(0, end_headers)) + "\n";
    request_ = trim(request_.substr(end_headers + 2, request_.size() - end_headers - 2));

    auto body = std::move(request_);

    if (!parse_first_line(_request, first_line)) {
        _request.clear();
        return false;
    }

    if (!parse_headers(_request, headers)) {
        _request.clear();
        return false;
    }

    _request[BODY] = body;
    std::string serialized_string = _request.dump();
    return true;
}

std::string http::Request::string() const {
    std::string res;
    if (_request.contains(METHOD) && _request.contains(URL) && _request.contains(VERSION)) {
        res = _request[METHOD].get<std::string>() + " " + _request[URL].get<std::string>()
              + " " + _request[VERSION].get<std::string>() + "\n";
    }

    if (_request.contains(HEADERS)) {
        for (auto&[key, value]: _request[HEADERS].items()) {
            res += key + ": " + value.get<std::string>() + "\n";
        }
    }

    if (_request.contains(COOKIE)) {
        if (!_request[COOKIE].empty()) {
            res += "Cookie: ";
        }

        for (auto&[key, value]: _request[COOKIE].items()) {
            std::string tmp = _request[HEADERS].dump();
            res += key + "=" + value.get<std::string>() + ";";
        }

        if (!_request[COOKIE].empty()) {
            res += "\n";
        }
    }

    if (_request.contains(BODY)) {
        res += "\n" + _request[BODY].get<std::string>();
    }
    return res;
}


nj::json http::Request::get_json() const {
    return _request;
}

nj::json& http::Request::json() {
    return _request;
}

http::Request::Request(const std::string &request) {
    parse(request);
}

std::string http::Request::get_header(const std::string &name) const {
    if (_request.contains(HEADERS) && _request.is_string()) {
        return _get_param(_request[HEADERS], name);
    }
    return "";
}

std::string http::Request::get_url() const {
    return _get_param(_request, URL);
}

std::string http::Request::get_method() const {
    return _get_param(_request, METHOD);
}

std::string http::Request::get_version() const {
    return _get_param(_request, VERSION);
}

std::string http::Request::get_body() const {
    return _get_param(_request, BODY);
}

bool http::Request::set_header(const std::string &name, const std::string &value) {
    if (_request.contains(HEADERS) && _request.is_string()) {
        return _set_param(_request[HEADERS], name, value);
    }
    return false;
}

bool http::Request::set_url(const std::string &value) {
    return _set_param(_request, URL, value);
}

bool http::Request::set_method(const std::string &value) {
    return _set_param(_request, METHOD, value);
}

bool http::Request::set_version(const std::string &value) {
    return _set_param(_request, VERSION, value);
}

bool http::Request::set_body(const std::string &value) {
    return _set_param(_request, BODY, value);
}

bool http::Request::delete_header(const std::string &name) {
    if (_request.contains(HEADERS) && _request[HEADERS].is_string()) {
        auto& headers = _request[HEADERS];
        auto res = headers.find(name);
        if (res != headers.end()) {
            headers.erase(res);
            return true;
        }
    }
    return false;
}

std::string http::Request::_get_param(const nj::json& json, const std::string &name) {
    if (json.contains(name) && json[name].is_string()) {
        return json[name].get<std::string>();
    }
    return "";
}

bool http::Request::_set_param(nj::json &json, const std::string &name, const std::string& value) {
    json[name] = value;
    return true;
}

std::map<std::string, std::string> http::Request::get_cookies() const {
    if (_request.contains(COOKIE)) {
        std::map<std::string, std::string> res;
        for (auto& [key, value] : _request[HEADERS].items()) {
            std::string tmp = _request[HEADERS].dump();
            res[key] = value.get<std::string>();
        }
        return res;
    }
    return {};
}
