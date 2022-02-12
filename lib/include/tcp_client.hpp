#pragma once
#include <cstdint>
#include <cstddef>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <Windows.h>
typedef SOCKET socket_t;
#else
typedef int socket_t;
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstdio>
#include <cstdlib>
#endif

#include "tcp_lib.hpp"
#include <memory.h>

/// Sipmple TCP
namespace bstcp {

class TcpClient : public BaseSocket {
  public:
    typedef std::function<void(tcp_data_t)> handler_function_t;
    TcpClient() noexcept;
    TcpClient(ThreadPool* thread_pool) noexcept;
    ~TcpClient() override;

    status connectTo(uint32_t host, uint16_t port) noexcept;
    status disconnect() noexcept override;

    [[nodiscard]] uint32_t getHost() const override;
    [[nodiscard]] uint16_t getPort() const override;
    [[nodiscard]] status getStatus() const override {
        return _status;
    }

    tcp_data_t loadData() override;

    tcp_data_t loadDataSync();
    void setHandler(const handler_function_t& handler);
    void joinHandler();

    bool sendData(const void* buffer, size_t size) const override;
    [[nodiscard]] SocketType getType() const override {
        return SocketType::client_socket;
    }

  private:

    enum class _thread_manager_type : bool {
        single_thread = false,
        thread_pool = true
    };

    union Thread {
        std::thread* thread;
        ThreadPool* thread_pool;

        Thread()
            : thread(nullptr) {}
        explicit Thread(ThreadPool* thread_pool)
            : thread_pool(thread_pool) {}

        ~Thread() = default;
    };

    status                          _status = status::disconnected;
    Thread                          _threads;
    socket_t                        _client_socket;
    std::mutex                      _handle_mutex;
    socket_addr_in                  _address;
    _thread_manager_type            _thread_management_type;
    std::function<void(tcp_data_t)> _handler_func = [](const tcp_data_t&){};

    void _handle_single_thread();
    void _handle_thread_pool();
};

}

