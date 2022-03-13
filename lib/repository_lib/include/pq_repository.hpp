#pragma once

#include <pqxx/pqxx>

#include "pq_pool.hpp"
#include "request_parser_lib.hpp"

namespace repository {
    struct request_t {
        bool            is_valid;
        bool            is_https;
        size_t          id;
        size_t          port;
        std::string     host;
        http::Request   request;
    };

    class PQStoreRequest {
    public:

        explicit PQStoreRequest(const std::string& connection_string);

        request_t get_by_id(size_t id);
        request_t get_by_host(const std::string& host);

        std::vector<request_t> get_list(size_t limit);

        size_t add(const request_t& req);

    private:
        PQPool _rep;
    };
}

namespace rp = repository;