#include "tcp_server.hpp"
#include <chrono>
#include <cstring>
#include <mutex>
#include <utility>

using namespace bstcp;


#ifdef _WIN32
#define WIN(exp) exp
#define NIX(exp)

#else
#define WIN(exp)
#define NIX(exp) exp
#endif

TcpServer::TcpServer(uint16_t port,
                     KeepAliveConfig ka_conf,
                     _handler_function_t handler,
                     _con_handler_function_t connect_hndl,
                     _con_handler_function_t disconnect_hndl,
                     uint thread_count
)
        : _port(port)
          , _thread_pool(thread_count)
          , _ka_conf(ka_conf)
          , _handler(std::move(handler))
          , _connect_hndl(std::move(connect_hndl))
          , _disconnect_hndl(std::move(disconnect_hndl)) {}

TcpServer::~TcpServer() {
    if (_status == Status::up)
        stop();
}

void TcpServer::setHandler(_handler_function_t hdlr) {
    _handler = std::move(hdlr);
}

uint16_t TcpServer::getPort() const {
    return _port;
}

uint16_t TcpServer::setPort(uint16_t port) {
    _port = port;
    start();
    return port;
}

bstcp::TcpServer::Status TcpServer::start() {
    int flag;
    if (_status == Status::up) stop();

    socket_addr_in address;
    address.sin_addr
            WIN(.S_un.S_addr)NIX(.s_addr) = INADDR_ANY;
    address.sin_port = htons(_port);
    address.sin_family = AF_INET;


    if ((_serv_socket = socket(AF_INET, SOCK_STREAM NIX(| SOCK_NONBLOCK),
                               0)) WIN(== INVALID_SOCKET) NIX(== -1))
        return _status = Status::err_socket_init;

    // Set nonblocking accept
//  NIX( // not needed becouse socket created with flag SOCK_NONBLOCK
//  if(fcntl(serv_socket, F_SETFL, fcntl(serv_socket, F_GETFL, 0) | O_NONBLOCK) < 0) {
//    return _status = status::err_socket_init;
//  })
    WIN(
            if (unsigned long mode = 0;
                    ioctlsocket(serv_socket, FIONBIO, &mode) == SOCKET_ERROR) {
                return _status = status::err_socket_init;
            })


    // Bind address to socket
    if (flag = true;
            (setsockopt(_serv_socket, SOL_SOCKET, SO_REUSEADDR, WIN((char * ))
                        &flag, sizeof(flag)) == -1) ||
            (bind(_serv_socket, (struct sockaddr *) &address,
                  sizeof(address)) WIN(== SOCKET_ERROR) NIX(< 0)))
        return _status = Status::err_socket_bind;

    if (listen(_serv_socket, SOMAXCONN) WIN(== SOCKET_ERROR) NIX(< 0))
        return _status = Status::err_socket_listening;
    _status = Status::up;
    _thread_pool.addJob([this] { handlingAcceptLoop(); });
    _thread_pool.addJob([this] { waitingDataLoop(); });
    return _status;
}

void TcpServer::stop() {
    _thread_pool.dropUnstartedJobs();
    _status = Status::close;
    WIN(closesocket)NIX(close)(_serv_socket);
    _client_list.clear();
}

void TcpServer::joinLoop() {
    _thread_pool.join();
}

bool TcpServer::connectTo(uint32_t host, uint16_t port,
                          const _con_handler_function_t &connect_hndl) {
    socket_t client_socket;
    socket_addr_in address;
    if ((client_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_IP)) WIN(
            == INVALID_SOCKET) NIX(< 0))
        return false;

    new(&address) socket_addr_in;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = host;
    WIN(address.sin_addr.S_un.S_addr = host;)
    NIX(address.sin_addr.s_addr = host;)

    address.sin_port = htons(port);

    if (connect(client_socket, (sockaddr *) &address, sizeof(address))
        WIN(== SOCKET_ERROR) NIX(!= 0)) {
        WIN(closesocket(client_socket);)NIX(close(client_socket);)
        return false;
    }

    if (!enableKeepAlive(client_socket)) {
        shutdown(client_socket, 0);
        WIN(closesocket)NIX(close)(client_socket);
    }

    std::unique_ptr<Client> client(new Client(client_socket, address));
    connect_hndl(*client);
    _client_mutex.lock();
    _client_list.emplace_back(std::move(client));
    _client_mutex.unlock();
    return true;
}

void TcpServer::sendData(const void *buffer, const size_t size) {
    for (std::unique_ptr<Client> &client: _client_list)
        client->sendData(buffer, size);
}

bool TcpServer::sendDataBy(uint32_t host, uint16_t port, const void *buffer,
                           const size_t size) {
    bool data_is_sended = false;
    for (std::unique_ptr<Client> &client: _client_list)
        if (client->getHost() == host &&
            client->getPort() == port) {
            client->sendData(buffer, size);
            data_is_sended = true;
        }
    return data_is_sended;
}

bool TcpServer::disconnectBy(uint32_t host, uint16_t port) {
    bool client_is_disconnected = false;
    for (std::unique_ptr<Client> &client: _client_list)
        if (client->getHost() == host &&
            client->getPort() == port) {
            client->disconnect();
            client_is_disconnected = true;
        }
    return client_is_disconnected;
}

void TcpServer::disconnectAll() {
    for (std::unique_ptr<Client> &client: _client_list)
        client->disconnect();
}

void TcpServer::handlingAcceptLoop() {
    sock_len_t addrlen = sizeof(socket_addr_in);
    socket_addr_in client_addr;
    if (socket_t client_socket =
                WIN(accept(_serv_socket, (struct sockaddr *) &client_addr,
                           &addrlen))
                NIX(accept4(_serv_socket, (struct sockaddr *) &client_addr,
                            &addrlen, SOCK_NONBLOCK));
            client_socket WIN(!= 0) NIX(>= 0) && _status == Status::up) {

        // Enable keep alive for client
        if (enableKeepAlive(client_socket)) {
            std::unique_ptr<Client> client(
                    new Client(client_socket, client_addr));
            _connect_hndl(*client);
            _client_mutex.lock();
            _client_list.emplace_back(std::move(client));
            _client_mutex.unlock();
        } else {
            shutdown(client_socket, 0);
            WIN(closesocket)NIX(close)(client_socket);
        }
    }

    if (_status == Status::up)
        _thread_pool.addJob([this]() { handlingAcceptLoop(); });
}

void TcpServer::waitingDataLoop() {
    {
        std::lock_guard lock(_client_mutex);
        for (auto it = _client_list.begin(); it != _client_list.end(); it++) {
            auto &client = *it;
            if (client) {
                if (tcp_data_t data = client->loadData(); !data.empty()) {
                    _thread_pool.addJob(
                            [this, _data = std::move(data), &client] {
                                client->_access_mtx.lock();
                                _handler(_data, *client);
                                client->_access_mtx.unlock();
                            });
                } else if (client->_status == SocketStatus::disconnected) {
                    _thread_pool.addJob([this, &client, it] {
                        client->_access_mtx.lock();
                        Client *pointer = client.release();
                        client = nullptr;
                        pointer->_access_mtx.unlock();
                        _disconnect_hndl(*pointer);
                        _client_list.erase(it);
                        delete pointer;
                    });
                }
            }
        }
    }

    if (_status == Status::up) {
        _thread_pool.addJob([this]() { waitingDataLoop(); });
    }
}

bool TcpServer::enableKeepAlive(socket_t socket) {
    int flag = 1;
#ifdef _WIN32
    tcp_keepalive ka {1, ka_conf.ka_idle * 1000, ka_conf.ka_intvl * 1000};
  if (setsockopt (socket, SOL_SOCKET, SO_KEEPALIVE, (const char *) &flag, sizeof(flag)) != 0) return false;
  unsigned long numBytesReturned = 0;
  if(WSAIoctl(socket, SIO_KEEPALIVE_VALS, &ka, sizeof (ka), nullptr, 0, &numBytesReturned, 0, nullptr) != 0) return false;
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