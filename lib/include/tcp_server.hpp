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

#include "tcp_lib.hpp"

/// Sipmple TCP
namespace bstcp {

struct KeepAliveConfig {
    ka_prop_t ka_idle = 120;
    ka_prop_t ka_intvl = 3;
    ka_prop_t ka_cnt = 5;
};

struct TcpServer {
    struct Client;

    class ClientList;

    enum class Status : uint8_t {
        up                      = 0,
        err_socket_init         = 1,
        err_socket_bind         = 2,
        err_scoket_keep_alive   = 3,
        err_socket_listening    = 4,
        close                   = 5
    };

    typedef std::function<void(tcp_data_t, Client &)>   _handler_function_t;
    typedef std::function<void(Client &)>               _con_handler_function_t;

    static constexpr auto _default_data_handler = [](const tcp_data_t &,
                                                     Client &) {};
    static constexpr auto _default_connsection_handler = [](Client &) {};

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
    void setHandler(_handler_function_t hdlr);

    ThreadPool &getThreadPool() {
        return _thread_pool;
    }

    // Server properties getters
    [[nodiscard]] uint16_t getPort() const;

    uint16_t setPort(uint16_t port);

    [[nodiscard]] Status getStatus() const {
        return _status;
    }

    // Server status manip
    Status start();

    void stop();

    void joinLoop();

    // Server client management
    bool connectTo(uint32_t host, uint16_t port,
                   const _con_handler_function_t& connect_hndl);

    void sendData(const void *buffer, size_t size);

    bool
    sendDataBy(uint32_t host, uint16_t port, const void *buffer, size_t size);

    bool disconnectBy(uint32_t host, uint16_t port);

    void disconnectAll();

  private:
    Status          _status                     = Status::close;
    socket_t        _serv_socket;
    uint16_t        _port;
    std::mutex      _client_mutex;
    ThreadPool      _thread_pool;
    KeepAliveConfig _ka_conf;

    _handler_function_t     _handler            = _default_data_handler;
    _con_handler_function_t _connect_hndl       = _default_connsection_handler;
    _con_handler_function_t _disconnect_hndl    = _default_connsection_handler;

    typedef std::list<std::unique_ptr<Client>>::iterator _ClientIterator;

    std::list<std::unique_ptr<Client>> _client_list;

    bool enableKeepAlive(socket_t socket);

    void handlingAcceptLoop();

    void waitingDataLoop();

};

struct TcpServer::Client : public BaseSocket {
  public:
    Client(socket_t socket, socket_addr_in address);

    virtual ~Client() override;

    virtual uint32_t getHost() const override;

    virtual uint16_t getPort() const override;

    virtual status getStatus() const override { return _status; }

    virtual status disconnect() override;

    virtual tcp_data_t loadData() override;

    virtual bool sendData(const void *buffer, const size_t size) const override;

    virtual SocketType
    getType() const override { return SocketType::server_socket; }

  private:
    friend struct TcpServer;

    status          _status         = status::connected;
    socket_t        _socket;
    std::mutex      _access_mtx;
    socket_addr_in  _address;
};

}
