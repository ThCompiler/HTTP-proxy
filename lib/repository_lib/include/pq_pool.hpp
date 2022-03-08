#pragma once

#include <pqxx/connection>
#include <vector>
#include <condition_variable>

namespace repository {
    class PQPool {
    public:
        explicit PQPool(std::string connection_string);

        PQPool(PQPool&& pool) noexcept;

        std::shared_ptr<pqxx::connection> conn();

        void free_conn(const std::shared_ptr<pqxx::connection>& conn);

    private:

        void _create_pool();

        bool _is_moved;

        std::mutex                                      _mutex;
        std::string                                     _connection_string;
        std::condition_variable                         _condition;
        std::vector<std::shared_ptr<pqxx::connection>>  _pool;
    };
}