#pragma once

#include <functional>
#include <list>

#include <thread>
#include <mutex>
#include <shared_mutex>

#ifdef _WIN32 // Windows NT

#include <WinSock2.h>
#include <mstcpip.h>

#else // *nix

#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstdio>
#include <cstdlib>

#endif

#include "tcp_base_socket.hpp"


namespace bstcp {

struct KeepAliveConfig {
    ka_prop_t ka_idle = 120;
    ka_prop_t ka_intvl = 3;
    ka_prop_t ka_cnt = 5;
};

struct TcpServer {
    struct Client;

    enum class ServerStatus : uint8_t {
        up                      = 0,
        err_socket_init         = 1,
        err_socket_bind         = 2,
        err_scoket_keep_alive   = 3,
        err_socket_listening    = 4,
        close                   = 5
    };

    typedef std::function<void(uniq_ptr<Client> &)>   _handler_function_t;
    typedef std::function<void(uniq_ptr<Client> &)>   _con_handler_function_t;

    static constexpr auto _default_data_handler = [](uniq_ptr<Client> &) {};
    static constexpr auto _default_connsection_handler = [](uniq_ptr<Client> &) {};

  public:
    explicit TcpServer(uint16_t port,
                       KeepAliveConfig ka_conf = {},
                       _handler_function_t handler = _default_data_handler,
                       _con_handler_function_t connect_hndl = _default_connsection_handler,
                       _con_handler_function_t disconnect_hndl = _default_connsection_handler,
                       uint thread_count = std::thread::hardware_concurrency()
    );

    ~TcpServer();

    //! Set client handler
    void set_handler(_handler_function_t hdlr);

    ThreadPool &get_thread_pool() {
        return _thread_pool;
    }

    // Server properties getters
    [[nodiscard]] uint16_t get_port() const;

    uint16_t setPort(uint16_t port);

    [[nodiscard]] ServerStatus get_status() const {
        return _status;
    }

    // Server status manip
    ServerStatus start();

    void stop();

    void joinLoop();

    // Server client management
    bool connect_to(uint32_t host, uint16_t port,
                   const _con_handler_function_t& connect_hndl);

    void send_to(const void *buffer, size_t size);

    bool send_to_by(uint32_t host, uint16_t port, const void *buffer, size_t size);

    bool disconnect_by(uint32_t host, uint16_t port);

    void disconnect_all();

  private:
    ServerStatus    _status  = ServerStatus::close;
    BaseSocket      _serv_socket;
    uint16_t        _port;
    std::mutex      _client_mutex;
    ThreadPool      _thread_pool;
    KeepAliveConfig _ka_conf;

    _handler_function_t     _handler            = _default_data_handler;
    _con_handler_function_t _connect_hndl       = _default_connsection_handler;
    _con_handler_function_t _disconnect_hndl    = _default_connsection_handler;

    typedef std::list<std::shared_ptr<Client>>::iterator ClientIterator;

    std::list<std::shared_ptr<Client>> _client_list;

    bool _enable_keep_alive(socket_t socket);

    void _handling_accept_loop();

    void _waiting_recv_loop();

};

struct TcpServer::Client : public BaseSocket {
  public:
    Client() = delete;

    explicit Client(BaseSocket&& socket)
        : BaseSocket(std::move(socket)) {}

    Client(const Client&) = delete;
    Client operator=(const Client&) = delete;

    Client(Client&& clt) noexcept
        : BaseSocket(std::move(clt))
        , _access_mtx() {
    };

    Client& operator=(const Client&&) = delete;

    ~Client() override = default;

  private:
    friend struct TcpServer;

    std::mutex  _access_mtx;
};

}
