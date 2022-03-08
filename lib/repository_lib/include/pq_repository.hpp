#pragma once

#include <pqxx/pqxx>

#include "pq_pool.hpp"
#include "request_parser_lib.hpp"

namespace repository {
    class PQStoreRequest {
    public:

        explicit PQStoreRequest(const std::string& connection_string);

        http::Request get_by_id(size_t id);
        http::Request get_by_host(const std::string& host);

        size_t add(const http::Request& req, const std::string& host);

    private:
        PQPool _rep;
    };
}

namespace rp = repository;