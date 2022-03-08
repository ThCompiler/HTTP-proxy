#include "pq_pool.hpp"

#include <utility>
#include <iostream>
#include <exception>

static const size_t pool_size = 20;

namespace repository {

    void PQPool::_create_pool() {
        std::lock_guard<std::mutex> lck(_mutex);
        try {
            for (size_t i = 0; i < pool_size; ++i) {
                _pool.emplace_back(std::make_shared<pqxx::connection>(_connection_string));
            }
        } catch (std::exception& e) {
            std::cerr << e.what() << "\n";
        }
    }

    PQPool::PQPool(std::string connection_string)
        : _is_moved(false)
        , _connection_string(std::move(connection_string))
        , _condition() {
        _create_pool();
    }

    std::shared_ptr<pqxx::connection> PQPool::conn() {
        if (_is_moved) {
            return nullptr;
        }

        std::unique_lock<std::mutex> lck(_mutex);
        _condition.wait(lck, [this] {
            return !_pool.empty();
        });

        auto conn_ = _pool.back();
        _pool.pop_back();

        return conn_;
    }

    void PQPool::free_conn(const std::shared_ptr<pqxx::connection>& conn) {
        if (_is_moved) {
            return;
        }

        {
            std::lock_guard<std::mutex> lck(_mutex);
            _pool.push_back(conn);
        }
        _condition.notify_one();
    }

    PQPool::PQPool(PQPool &&pool) noexcept
        : _is_moved(false)
        , _mutex()
        , _connection_string(std::move(pool._connection_string))
        , _condition() {
        pool._is_moved = true;
        std::lock_guard<std::mutex> lck(_mutex);
        _pool = std::move(pool._pool);
    }

}
