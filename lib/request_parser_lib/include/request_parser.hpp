#pragma once

#include "json/json.hpp"

namespace nj = nlohmann;

namespace http {

class Request {
  public:

    Request() = default;
    explicit Request(const std::string &request);

    bool parse(const std::string &request);

    [[nodiscard]] std::string string() const;

    [[nodiscard]] std::string get_header(const std::string& name) const;
    [[nodiscard]] std::string get_url() const;
    [[nodiscard]] std::string get_method() const;
    [[nodiscard]] std::string get_version() const;
    [[nodiscard]] std::string get_body() const;
    [[nodiscard]] std::map<std::string, std::string> get_cookies() const;

    bool set_header(const std::string& name, const std::string& value);
    bool set_url(const std::string& value);
    bool set_method(const std::string& value);
    bool set_version(const std::string& value);
    bool set_body(const std::string& value);
    bool delete_header(const std::string& name);

    nj::json& json();

    [[nodiscard]] nj::json get_json() const;

  private:

    [[nodiscard]] static std::string _get_param(const nj::json& json, const std::string& name);
    [[nodiscard]] static bool _set_param(nj::json& json, const std::string& name, const std::string& value);
    nj::json _request;
};

}