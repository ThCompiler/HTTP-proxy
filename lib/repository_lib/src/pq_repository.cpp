//
// Created by vet_v on 07/03/22.
//

#include "pq_repository.hpp"

namespace repository {

    PQStoreRequest::PQStoreRequest(const std::string &connection_string)
        : _rep(connection_string) {}


    request_t PQStoreRequest::get_by_id(size_t id) {
        auto conn = _rep.conn();

        pqxx::work w(*conn);

        auto res = w.exec_params1("SELECT request, host, is_https, port FROM history WHERE id = $1", id);
        if (res.empty()) {
            w.commit();
            _rep.free_conn(conn);
            return {.is_valid = false,
                    .is_https = false,
                    .id = 0,
                    .port = 0,
                    .host = "",
                    .request = http::Request()};
        }
        auto rs = res["request"].as<std::string>();
        request_t answ;
        answ.is_valid = true;
        answ.host = res["host"].as<std::string>();
        answ.port = res["port"].as<size_t>();
        answ.is_https = res["is_https"].as<bool>();
        w.commit();

        _rep.free_conn(conn);
        answ.request.read_json_from_string(rs);
        return answ;
    }

    request_t PQStoreRequest::get_by_host(const std::string &host) {
        auto conn = _rep.conn();

        pqxx::work w(*conn);

        auto res = w.exec_params1("SELECT id, request, is_https, port FROM history WHERE host = $1 LIMIT 1", host);
        if (res.empty()) {
            w.commit();
            _rep.free_conn(conn);
            return {.is_valid = false,
                    .is_https = false,
                    .id = 0,
                    .port = 0,
                    .host = "",
                    .request = http::Request()};
        }
        auto rs = res["request"].as<std::string>();
        request_t answ;
        answ.is_valid = true;
        answ.id = res["id"].as<size_t>();
        answ.is_https = res["is_https"].as<bool>();
        answ.port = res["port"].as<size_t>();

        w.commit();
        _rep.free_conn(conn);

        answ.host = host;
        answ.request.read_json_from_string(rs);
        return answ;
    }

    std::vector<request_t> PQStoreRequest::get_list(size_t limit) {
        auto conn = _rep.conn();

        pqxx::work w(*conn);

        auto res = w.exec_params("SELECT id, request, is_https, host, port FROM history LIMIT $1", limit);
        if (res.empty()) {
            w.commit();
            _rep.free_conn(conn);
            return {};
        }
        w.commit();
        _rep.free_conn(conn);

        std::vector<request_t> selected;
        for (auto rs : res) {
            request_t answ;

            auto tmp = rs["request"].as<std::string>();
            answ.request.read_json_from_string(tmp);
            answ.id = rs["id"].as<size_t>();
            answ.is_https = rs["is_https"].as<bool>();
            answ.host = rs["host"].as<std::string>();
            answ.port = rs["port"].as<size_t>();
            answ.is_valid = true;

            selected.push_back(answ);
        }

        return selected;
    }

    size_t PQStoreRequest::add(const request_t &req) {
        auto conn = _rep.conn();

        pqxx::work w(*conn);
        auto tmp = req.request.get_json().dump();
        auto res = w.exec_params1("INSERT INTO history (request, is_https, host, port) VALUES($1, $2, $3, $4) RETURNING id",
                                  tmp, req.is_https, req.host, req.port);
        auto rs = res[0].as<size_t>();

        w.commit();
        _rep.free_conn(conn);
        return rs;
    }
}

