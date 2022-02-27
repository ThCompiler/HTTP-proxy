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

    nj::json& operator[](const char* name);

    nj::json& json();

    const nj::json& operator[](const char* name) const;

    [[nodiscard]] nj::json get_json() const;

  private:
    nj::json _request;
};

}