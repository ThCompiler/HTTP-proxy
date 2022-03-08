//
// Created by vet_v on 07/03/22.
//

#include "pq_repository.hpp"

namespace repository {

    PQStoreRequest::PQStoreRequest(const std::string &connection_string)
        : _rep(connection_string) {}


    http::Request PQStoreRequest::get_by_id(size_t id) {
        auto conn = _rep.conn();

        pqxx::work w(*conn);

        auto res = w.exec_params1("SELECT request FROM history WHERE id = $1", id);
        auto rs = res["request"].as<std::string>();

        w.commit();
        _rep.free_conn(conn);
        http::Request answ;
        answ.read_json_from_string(rs);
        return answ;
    }

    http::Request PQStoreRequest::get_by_host(const std::string &host) {
        auto conn = _rep.conn();

        pqxx::work w(*conn);

        auto res = w.exec_params1("SELECT request FROM history WHERE host = $1 LIMIT 1", host);
        auto rs = res["request"].as<std::string>();

        w.commit();
        _rep.free_conn(conn);
        http::Request answ;
        answ.read_json_from_string(rs);
        return answ;
    }

    size_t PQStoreRequest::add(const http::Request &req, const std::string& host) {
        auto conn = _rep.conn();

        pqxx::work w(*conn);
        auto tmp = req.get_json().dump();
        auto res = w.exec_params1("INSERT INTO history (request, host) VALUES($1, $2) RETURNING id", tmp, host);
        auto rs = res[0].as<size_t>();

        w.commit();
        _rep.free_conn(conn);
        return rs;
    }
}

