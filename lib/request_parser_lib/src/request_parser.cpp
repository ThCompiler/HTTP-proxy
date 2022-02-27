#include "request_parser.hpp"

static const char * METHOD = "method";
static const char * URL = "url";
static const char * VERSION = "version";
static const char * BODY = "body";
static const char * HEADERS = "headers";


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

bool parse_headers(nj::json& request, const std::string &text) {
    size_t end = 0;
    size_t next_end = text.find(": ");
    nj::json tmp;
    while(next_end != std::string::npos) {
        auto name = trim(text.substr(end, next_end - end));
        end = text.find('\n', next_end + 1);
        if (end == std::string::npos) {
            return false;
        }
        auto tm = trim(text.substr(next_end + 1, end - next_end - 1));
        tmp[name] = tm;
        next_end = text.find(": ", end + 1);
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
    std::string res = _request[METHOD].get<std::string>() + " " + _request[URL].get<std::string>()
            + " " + _request[VERSION].get<std::string>() + "\n";
    for (auto& [key, value] : _request[HEADERS].items()) {
        std::string tmp = _request[HEADERS].dump();
        res += key + " : " + value.get<std::string>() + "\n";
    }
    res += "\n" + _request[BODY].get<std::string>();
    return res;
}

nj::json &http::Request::operator[](const char *name) {
    return _request[name];
}

const nj::json &http::Request::operator[](const char *name) const {
    return _request[name];
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
