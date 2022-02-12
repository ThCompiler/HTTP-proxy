#include "tcp_client.hpp"
#include <cstdio>
#include <cstring>
#include <iostream>
#include <utility>

using namespace bstcp;

#ifdef _WIN32
#define WIN(exp) exp
#define NIX(exp)
#define WINIX(win_exp, nix_exp) win_exp


inline int convertError() {
    switch (WSAGetLastError()) {
    case 0:
        return 0;
    case WSAEINTR:
        return EINTR;
    case WSAEINVAL:
        return EINVAL;
    case WSA_INVALID_HANDLE:
        return EBADF;
    case WSA_NOT_ENOUGH_MEMORY:
        return ENOMEM;
    case WSA_INVALID_PARAMETER:
        return EINVAL;
    case WSAENAMETOOLONG:
        return ENAMETOOLONG;
    case WSAENOTEMPTY:
        return ENOTEMPTY;
    case WSAEWOULDBLOCK:
        return EAGAIN;
    case WSAEINPROGRESS:
        return EINPROGRESS;
    case WSAEALREADY:
        return EALREADY;
    case WSAENOTSOCK:
        return ENOTSOCK;
    case WSAEDESTADDRREQ:
        return EDESTADDRREQ;
    case WSAEMSGSIZE:
        return EMSGSIZE;
    case WSAEPROTOTYPE:
        return EPROTOTYPE;
    case WSAENOPROTOOPT:
        return ENOPROTOOPT;
    case WSAEPROTONOSUPPORT:
        return EPROTONOSUPPORT;
    case WSAEOPNOTSUPP:
        return EOPNOTSUPP;
    case WSAEAFNOSUPPORT:
        return EAFNOSUPPORT;
    case WSAEADDRINUSE:
        return EADDRINUSE;
    case WSAEADDRNOTAVAIL:
        return EADDRNOTAVAIL;
    case WSAENETDOWN:
        return ENETDOWN;
    case WSAENETUNREACH:
        return ENETUNREACH;
    case WSAENETRESET:
        return ENETRESET;
    case WSAECONNABORTED:
        return ECONNABORTED;
    case WSAECONNRESET:
        return ECONNRESET;
    case WSAENOBUFS:
        return ENOBUFS;
    case WSAEISCONN:
        return EISCONN;
    case WSAENOTCONN:
        return ENOTCONN;
    case WSAETIMEDOUT:
        return ETIMEDOUT;
    case WSAECONNREFUSED:
        return ECONNREFUSED;
    case WSAELOOP:
        return ELOOP;
    case WSAEHOSTUNREACH:
        return EHOSTUNREACH;
    default:
        return EIO;
    }
}

#else
#define WIN(exp)
#define NIX(exp) exp
#define WINIX(win_exp, nix_exp) nix_exp
#endif


void TcpClient::_handle_single_thread() {
    try {
        while (_status == status::connected) {
            if (tcp_data_t data = loadData(); !data.empty()) {
                std::lock_guard lock(_handle_mutex);
                _handler_func(std::move(data));
            } else if (_status != status::connected) return;
        }
    } catch (std::exception &except) {
        std::cerr << except.what() << std::endl;
        return;
    }
}

void TcpClient::_handle_thread_pool() {
    try {
        if (tcp_data_t data = loadData(); !data.empty()) {
            std::lock_guard lock(_handle_mutex);
            _handler_func(std::move(data));
        }
        if (_status == status::connected)
            _threads.thread_pool->addJob([this] { _handle_thread_pool(); });
    } catch (std::exception &except) {
        std::cerr << except.what() << std::endl;
        return;
    } catch (...) {
        std::cerr << "Unhandled exception!" << std::endl;
        return;
    }
}

TcpClient::TcpClient() noexcept
        : _status(status::disconnected) {}

TcpClient::TcpClient(ThreadPool *thread_pool) noexcept
        : _status(status::disconnected)
          , _threads(thread_pool)
          , _thread_management_type(_thread_manager_type::thread_pool) {}

TcpClient::~TcpClient() {
    disconnect();
    WIN(WSACleanup();)

    switch (_thread_management_type) {
        case _thread_manager_type::single_thread:
            if (_threads.thread) _threads.thread->join();
            delete _threads.thread;
            break;
        case _thread_manager_type::thread_pool:
            break;
    }
}

TcpClient::status TcpClient::connectTo(uint32_t host, uint16_t port) noexcept {
#ifdef _WIN32
    if(WSAStartup(MAKEWORD(2, 2), &w_data) != 0) {}
    if((_client_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_IP)) == INVALID_SOCKET) {
        return _status = status::err_socket_init;
    }
#else
    if ((_client_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_IP)) < 0) {
        return _status = status::err_socket_init;
    }
#endif

    new(&_address) socket_addr_in;
    _address.sin_family = AF_INET;
    _address.sin_addr.s_addr = host;
    _address.sin_port = htons(port);

#ifdef _WIN32
    _address.sin_addr.S_un.S_addr = host;
#endif

#ifdef _WIN32
    if(connect(_client_socket, (sockaddr *)&_address, sizeof(_address)) == SOCKET_ERROR) {
        closesocket(client_socket);
#else
    if (connect(_client_socket, (sockaddr *) &_address, sizeof(_address)) !=
        0) {
        close(_client_socket);
#endif
        return _status = status::err_socket_connect;
    }

    return _status = status::connected;
}

TcpClient::status TcpClient::disconnect() noexcept {
    if (_status != status::connected)
        return _status;
    _status = status::disconnected;
    switch (_thread_management_type) {
        case _thread_manager_type::single_thread:
            if (_threads.thread) _threads.thread->join();
            delete _threads.thread;
            break;
        case _thread_manager_type::thread_pool:
            break;
    }
    shutdown(_client_socket, SD_BOTH);
#ifdef _WIN32
        closesocket(_client_socket);
#else
        close(_client_socket);
#endif
    return _status;
}

tcp_data_t TcpClient::loadData() {
    tcp_data_t buffer;
    uint32_t size;
    int err;

#ifdef _WIN32
    if ((u_long t = true; SOCKET_ERROR == ioctlsocket(_client_socket, FIONBIO, &t)) {
        return {}; // Windows non-blocking mode off
    }

    int answ = recv(_client_socket, (char *) &size, sizeof(size), 0);

    if (u_long t = false; SOCKET_ERROR == ioctlsocket(_client_socket, FIONBIO, &t)) {
        return {}; // Windows non-blocking mode off
    }
#else
    int answ = recv(_client_socket, (char *) &size, sizeof(size), MSG_DONTWAIT);
#endif

    // Disconnect
    if (!answ) {
        disconnect();
        return {};
    } else if (answ == -1) {
        // Error handle (f*ckin OS-dependence!)
#ifdef _WIN32
        err = convertError();
        if (!err) {
            SockLen_t len = sizeof(err);
            getsockopt(_client_socket, SOL_SOCKET, SO_ERROR,
                       WIN((char * )) & err, &len);
        }
#else
        sock_len_t len = sizeof(err);
        getsockopt(_client_socket, SOL_SOCKET, SO_ERROR,
                   WIN((char * )) & err, &len);
        if (!err) err = errno;
#endif

        switch (err) {
            case 0:
                break;
                // Keep alive timeout
            case ETIMEDOUT:
            case ECONNRESET:
            case EPIPE:
                disconnect();
                [[fallthrough]];
                // No data
            case EAGAIN:
                return {};
            default:
                disconnect();
                return {};
        }
    }

    if (!size) {
        return {};
    }
    buffer.resize(size);
    recv(_client_socket, (char *) buffer.data(), buffer.size(), 0);
    return buffer;
}

tcp_data_t TcpClient::loadDataSync() {
    tcp_data_t data;
    uint32_t size = 0;
    ssize_t answ = recv(_client_socket, reinterpret_cast<char *>(&size),
                    sizeof(size), 0);
    if (size && answ == sizeof(size)) {
        data.resize(size);
        recv(_client_socket, reinterpret_cast<char *>(data.data()), data.size(),
             0);
    }
    return data;
}

void TcpClient::setHandler(const TcpClient::handler_function_t& handler) {
    {
        std::lock_guard lock(_handle_mutex);
        _handler_func = handler;
    }

    switch (_thread_management_type) {
        case _thread_manager_type::single_thread:
            if (_threads.thread) {
                return;
            }
            _threads.thread = new std::thread(&TcpClient::_handle_single_thread,
                                             this);
            break;
        case _thread_manager_type::thread_pool:
            _threads.thread_pool->addJob([this] { _handle_thread_pool(); });
            break;
    }
}

void TcpClient::joinHandler() {
    switch (_thread_management_type) {
        case _thread_manager_type::single_thread:
            if (_threads.thread) {
                _threads.thread->join();
            }
            break;
        case _thread_manager_type::thread_pool:
            _threads.thread_pool->join();
            break;
    }
}

bool TcpClient::sendData(const void *buffer, const size_t size) const {
    void *send_buffer = malloc(size + sizeof(size_t));
    memcpy(reinterpret_cast<char *>(send_buffer) + sizeof(size_t), buffer, size);
    *reinterpret_cast<size_t *>(send_buffer) = size;

    if (send(_client_socket, reinterpret_cast<char *>(send_buffer),
             size + sizeof(size_t), 0) < 0) {
        return false;
    }

    free(send_buffer);
    return true;
}

uint32_t TcpClient::getHost() const {
#ifdef _WIN32
    return address.sin_addr.S_un.S_addr;
#else
    return _address.sin_addr.s_addr;
#endif
}

uint16_t TcpClient::getPort() const {
        return _address.sin_port;
}