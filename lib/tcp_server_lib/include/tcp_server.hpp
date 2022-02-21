#pragma once

#include <functional>
#include <list>

#include <thread>
#include <mutex>
#include <shared_mutex>
#include <type_traits>
#include <concepts>

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
#include "parallel.hpp"

namespace bstcp {

struct KeepAliveConfig {
    ka_prop_t ka_idle = 120;
    ka_prop_t ka_intvl = 3;
    ka_prop_t ka_cnt = 5;
};

class IServerClient: public ISocket {
public:
    virtual void handle_request() = 0;

    ~IServerClient() override = default;
};

template<typename T>
concept socket_type =   (std::is_base_of_v<ISocket, T>
                        || std::is_convertible_v<T, ISocket>)
                        && std::is_move_assignable_v<T>;

template<typename T, class Socket = BaseSocket>
concept server_client = std::is_constructible<T, Socket&&>::value
                        && std::is_destructible_v<T>
                        && (std::is_base_of_v<IServerClient, T>
                        || std::is_convertible_v<T, IServerClient>)
                        && std::is_move_constructible_v<T>;

template<socket_type Socket, server_client<Socket> T>
class TcpServer {
public:
    struct Client : public T {
    public:
        Client() = delete;

        explicit Client(Socket&& socket)
                : T(std::move(socket))
                , _in_use(false) {}

        Client(const Client&) = delete;
        Client operator=(const Client&) = delete;

        Client(Client&& clt) noexcept
                : T(std::move(clt))
                , _in_use(false)
                , _access_mtx() {}

        Client& operator=(const Client&&) = delete;

        ~Client() override = default;

    private:
        friend class TcpServer;

        bool        _in_use;
        std::mutex  _access_mtx;
    };

    enum class ServerStatus : uint8_t {
        up                      = 0,
        err_socket_init         = 1,
        err_socket_bind         = 2,
        err_scoket_keep_alive   = 3,
        err_socket_listening    = 4,
        close                   = 5
    };

    typedef std::function<void(uniq_ptr<ISocket> &)>   _con_handler_function_t;

    static constexpr auto _default_connsection_handler  = [](uniq_ptr<ISocket> &) {};

  public:
    explicit TcpServer(uint16_t port,
                       KeepAliveConfig ka_conf = {},
                       _con_handler_function_t connect_hndl = _default_connsection_handler,
                       _con_handler_function_t disconnect_hndl = _default_connsection_handler,
                       size_t thread_count = std::thread::hardware_concurrency()
    );

    ~TcpServer();

    prll::Parallel &get_thread_pool();

    // Server properties getters
    [[nodiscard]] uint16_t get_port() const;

    uint16_t set_port(uint16_t port);

    [[nodiscard]] ServerStatus get_status() const;

    // Server status manip
    ServerStatus start();

    void stop();

    void joinLoop();

    // Server client management
    bool connect_to(uint32_t host, uint16_t port,
                   const _con_handler_function_t& connect_hndl);

    void send_to(const void *buffer, int size);

    bool send_to_by(uint32_t host, uint16_t port, const void *buffer, size_t size);

    bool disconnect_by(uint32_t host, uint16_t port);

    void disconnect_all();

  private:
    Socket          _serv_socket;
    uint16_t        _port;
    std::mutex      _client_mutex;
    ServerStatus    _status  = ServerStatus::close;
    prll::Parallel  _thread_pool;
    KeepAliveConfig _ka_conf;

    _con_handler_function_t _connect_hndl       = _default_connsection_handler;
    _con_handler_function_t _disconnect_hndl    = _default_connsection_handler;

    std::list<std::unique_ptr<Client>> _client_list;

    bool _enable_keep_alive(socket_t socket);

    void _handling_accept_loop();

    void _waiting_recv_loop();
};


template<server_client<BaseSocket> T>
using BaseTcpServer = TcpServer<BaseSocket, T>;

}

#include "tcp_server.inl"