#include <chrono>
#include <cstring>
#include <mutex>
#include <utility>

using namespace bstcp;

SOCKET_TEMPLATE
TcpServer<Socket, T>::TcpServer(uint16_t port,
                                KeepAliveConfig ka_conf,
                                _con_handler_function_t connect_hndl,
                                _con_handler_function_t disconnect_hndl,
                                size_t thread_count
)
        : _port(port)
          , _thread_pool()
          , _ka_conf(ka_conf)
          , _connect_hndl(std::move(connect_hndl))
          , _disconnect_hndl(std::move(disconnect_hndl)) {
    _thread_pool.set_max_threads(thread_count);
}

SOCKET_TEMPLATE
TcpServer<Socket, T>::~TcpServer() {
    if (_status == ServerStatus::up) {
        stop();
    }
}

SOCKET_TEMPLATE
uint16_t TcpServer<Socket, T>::get_port() const {
    return _port;
}

SOCKET_TEMPLATE
uint16_t TcpServer<Socket, T>::set_port(uint16_t port) {
    _port = port;
    start();
    return port;
}

SOCKET_TEMPLATE
typename bstcp::TcpServer<Socket, T>::ServerStatus
TcpServer<Socket, T>::start() {
    if (_status == ServerStatus::up) {
        stop();
    }

    auto sts = _serv_socket.init(localhost, _port,
                                 (uint16_t) SocketType::nonblocking_socket
                                 | (uint16_t) SocketType::server_socket);
    switch (sts) {
        case SocketStatus::connected:
            break;
        case SocketStatus::err_socket_bind:
            return _status = ServerStatus::err_socket_bind;
        case SocketStatus::err_socket_init:
            return _status = ServerStatus::err_socket_init;
        case SocketStatus::err_socket_listening:
            return _status = ServerStatus::err_socket_listening;
        case SocketStatus::err_socket_connect:
            return _status = ServerStatus::close;
        case SocketStatus::disconnected:
            return _status = ServerStatus::close;
        case SocketStatus::err_socket_type:
            return _status = ServerStatus::close;
        default:
            return _status = ServerStatus::close;
    }

    _status = ServerStatus::up;
    _thread_pool.add([this] { _handling_accept_loop(); });
    _thread_pool.add([this] { _waiting_recv_loop(); });

    return _status;
}

SOCKET_TEMPLATE
void TcpServer<Socket, T>::stop() {
    _thread_pool.wait();
    _status = ServerStatus::close;

    _serv_socket.disconnect();

    _client_list.clear();
}

SOCKET_TEMPLATE
void TcpServer<Socket, T>::joinLoop() {
    _thread_pool.join();
}

SOCKET_TEMPLATE
bool TcpServer<Socket, T>::connect_to(uint32_t host, uint16_t port,
                                      const _con_handler_function_t &connect_hndl) {
    BaseSocket client_socket;
    auto sts = client_socket.init(host, port,
                                  (uint16_t) SocketType::nonblocking_socket
                                  | (uint16_t) SocketType::client_socket);
    if (sts != status::connected) {
        return false;
    }


    if (!_enable_keep_alive(client_socket.get_socket())) {
        client_socket.disconnect();
        return false;
    }

    std::unique_ptr<Client> client(new Client(std::move(client_socket)));
    connect_hndl(client);
    _client_mutex.lock();
    _client_list.emplace_back(std::move(client));
    _client_mutex.unlock();
    return true;
}

SOCKET_TEMPLATE
void TcpServer<Socket, T>::send_to(const void *buffer, int size) {
    for (std::unique_ptr<Client> &client: _client_list) {
        client->send_to(buffer, size);
    }
}

SOCKET_TEMPLATE
bool TcpServer<Socket, T>::send_to_by(uint32_t host, uint16_t port,
                                      const void *buffer,
                                      const size_t size) {
    bool data_is_sended = false;

    for (std::unique_ptr<Client> &client: _client_list)
        if (client->get_host() == host &&
            client->get_port() == port) {
            client->send_to(buffer, size);
            data_is_sended = true;
        }

    return data_is_sended;
}

SOCKET_TEMPLATE
bool TcpServer<Socket, T>::disconnect_by(uint32_t host, uint16_t port) {
    bool client_is_disconnected = false;
    for (std::unique_ptr<Client> &client: _client_list)
        if (client->get_host() == host &&
            client->get_port() == port) {
            client->disconnect();
            client_is_disconnected = true;
        }
    return client_is_disconnected;
}

SOCKET_TEMPLATE
void TcpServer<Socket, T>::disconnect_all() {
    for (std::unique_ptr<Client> &client: _client_list)
        client->disconnect();
}

SOCKET_TEMPLATE
void TcpServer<Socket, T>::_handling_accept_loop() {
    Socket client_socket;
    if (client_socket.accept(_serv_socket) == status::connected
        && _status == ServerStatus::up) {

        // Enable keep alive for client
        if (_enable_keep_alive(client_socket.get_socket())) {
            std::unique_ptr<Client> client(
                    new Client(std::move(client_socket)));
            _connect_hndl(reinterpret_cast<std::unique_ptr<ISocket> &>(client));
            _client_mutex.lock();
            _client_list.emplace_back(std::move(client));
            _client_mutex.unlock();
        }
    }

    if (_status == ServerStatus::up) {
        _thread_pool.add([this]() { _handling_accept_loop(); });
    }
}

SOCKET_TEMPLATE
void TcpServer<Socket, T>::_waiting_recv_loop() {
    {
        std::lock_guard lock(_client_mutex);
        for (auto it = _client_list.begin(); it != _client_list.end(); it++) {
            auto &client = *it;
            if (client) {
                if (!client->_in_use) {
                    client->_in_use = true;
                    if (client->get_status() == SocketStatus::disconnected) {
                        client->_access_mtx.lock();
                        Client *pointer = client.release();
                        client = nullptr;
                        _disconnect_hndl(
                                reinterpret_cast<std::unique_ptr<ISocket> &>(pointer));
                        _client_list.erase(it);
                        pointer->_access_mtx.unlock();
                        delete pointer;
                        break;
                    } else {
                        _thread_pool.add(
                                [this, &client] {
                                    client->_access_mtx.lock();
                                    if (client->get_status() !=
                                        SocketStatus::disconnected) {
                                        client->handle_request();
                                    }
                                    client->_in_use = false;
                                    client->_access_mtx.unlock();
                                });
                    }
                }
            }
        }
    }

    if (_status == ServerStatus::up) {
        _thread_pool.add([this]() { _waiting_recv_loop(); });
    }
}

SOCKET_TEMPLATE
bool TcpServer<Socket, T>::_enable_keep_alive(socket_t socket) {
    int flag = 1;
#ifdef _WIN32
    tcp_keepalive ka {1, _ka_conf.ka_idle * 1000, _ka_conf.ka_intvl * 1000};
    if (setsockopt (socket, SOL_SOCKET, SO_KEEPALIVE, (const char *) &flag, sizeof(flag)) != 0) {
        return false;
    }
    unsigned long numBytesReturned = 0;
    if(WSAIoctl(socket, SIO_KEEPALIVE_VALS, &ka, sizeof (ka), nullptr, 0, &numBytesReturned, 0, nullptr) != 0)  {
        return false;
    }
#else //POSIX
    if (setsockopt(socket, SOL_SOCKET, SO_KEEPALIVE, &flag, sizeof(flag)) ==
        -1)
        return false;
    if (setsockopt(socket, IPPROTO_TCP, TCP_KEEPIDLE, &_ka_conf.ka_idle,
                   sizeof(_ka_conf.ka_idle)) == -1)
        return false;
    if (setsockopt(socket, IPPROTO_TCP, TCP_KEEPINTVL, &_ka_conf.ka_intvl,
                   sizeof(_ka_conf.ka_intvl)) == -1)
        return false;
    if (setsockopt(socket, IPPROTO_TCP, TCP_KEEPCNT, &_ka_conf.ka_cnt,
                   sizeof(_ka_conf.ka_cnt)) == -1)
        return false;
#endif
    return true;
}

SOCKET_TEMPLATE
typename TcpServer<Socket, T>::ServerStatus
TcpServer<Socket, T>::get_status() const {
    return _status;
}

SOCKET_TEMPLATE
prll::Parallel &TcpServer<Socket, T>::get_thread_pool() {
    return _thread_pool;
}
