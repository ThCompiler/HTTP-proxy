#include "tcp_base_socket.hpp"

using namespace bstcp;

#include <iostream>

BaseSocket::~BaseSocket() {
    disconnect();
}

status BaseSocket::init(uint32_t host, uint16_t port, uint16_t type) {
    if (_status == status::connected) {
        disconnect();
    }

    if (type & (uint16_t)SocketType::server_socket) {
        return _init_as_server(host, port, type);
    }
    if (type & (uint16_t)SocketType::client_socket) {
        return _init_as_client(host, port, type);
    }
    return status::err_socket_type;
}

status BaseSocket::_init_as_client(uint32_t host, uint16_t port, uint16_t type) {
#ifdef _WIN32
    if((_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_IP)) == INVALID_SOCKET) {
        return _status = status::err_socket_init;
    }
#else
    if ((_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_IP)) < 0) {
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
    if(connect(_socket, (sockaddr *)&_address, sizeof(_address)) == SOCKET_ERROR) {
        closesocket(_socket);
#else
    if (connect(_socket, (sockaddr *) &_address, sizeof(_address)) != 0) {
        close(_socket);
#endif
        return _status = status::err_socket_connect;
    }

#ifdef _WIN32
    if (unsigned long mode = 1; type & (uint16_t)SocketType::nonblocking_socket
                                && ioctlsocket(_socket, FIONBIO, &mode) == SOCKET_ERROR) {
        closesocket(_socket);
        return _status = status::err_socket_init;
    }
#else
    int flags = fcntl(_socket, F_GETFL, 0);
    if (flags == -1) {
        close(_socket);
        return _status = status::err_socket_init;
    }
    flags = (type & (uint16_t)SocketType::nonblocking_socket) ? (flags | O_NONBLOCK) : (flags & ~O_NONBLOCK);
    if (fcntl(_socket, F_SETFL, flags) != 0) {
        close(_socket);
        return _status = status::err_socket_init;
    }
#endif

    return _status = status::connected;
}

status BaseSocket::accept(const BaseSocket& server_socket) {
    if (_status == status::connected) {
        disconnect();
    }

    sock_len_t addrlen = sizeof(socket_addr_in);
#ifdef _WIN32
    if ((_socket = ::accept(server_socket._socket, (struct sockaddr *) &_address,
                           &addrlen)) == 0) {
#else
    if ((_socket = accept4(server_socket._socket, (struct sockaddr *) &_address,
                           &addrlen, O_NONBLOCK)) < 0) {
#endif
        return _status = status::disconnected;
    }

#ifdef _WIN32
    if (unsigned long mode = 1;  ioctlsocket(_socket, FIONBIO, &mode) == SOCKET_ERROR) {
        return _status = status::err_socket_init;
    }
#endif


    return _status = status::connected;
}

status BaseSocket::_init_as_server(uint32_t, uint16_t port, uint16_t type) {
    socket_addr_in address;
#ifdef _WIN32
    address.sin_addr.S_un.S_addr = INADDR_ANY;
#else
    address.sin_addr.s_addr = INADDR_ANY;
#endif
    address.sin_port = htons(port);
    address.sin_family = AF_INET;


#ifdef _WIN32
    if ((_socket = socket(AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET) {
        return _status = status::err_socket_init;
    }

    if (unsigned long mode = 1; type & (uint16_t)SocketType::nonblocking_socket
        && ioctlsocket(_socket, FIONBIO, &mode) == SOCKET_ERROR) {
        return _status = status::err_socket_init;
    }

     if (bool flag = true; setsockopt(_socket, SOL_SOCKET, SO_REUSEADDR, (char *) &flag, sizeof(flag)) == -1)  {
        return _status = status::err_socket_bind;
    }

    if (bind(_socket, (struct sockaddr *) &address, sizeof(address)) == SOCKET_ERROR) {
        return _status = status::err_socket_bind;
    }

    if (listen(_socket, SOMAXCONN) == SOCKET_ERROR) {
        return _status = status::err_socket_listening;
    }
#else
    int _type = SOCK_STREAM;
    if (type & (uint16_t)SocketType::nonblocking_socket) {
        _type |= SOCK_NONBLOCK;
    }

    if ((_socket = socket(AF_INET, _type, 0)) == -1) {
        return _status = status::err_socket_init;
    }

    // Set nonblocking accept
    //  NIX( // not needed becouse socket created with flag SOCK_NONBLOCK
    //  if(fcntl(serv_socket, F_SETFL, fcntl(serv_socket, F_GETFL, 0) | O_NONBLOCK) < 0) {
    //    return _status = status::err_socket_init;
    //  })

    // Bind address to socket
    if (int flag = true; setsockopt(_socket, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag)) == -1)  {
        return _status = status::err_socket_bind;
    }

    if (bind(_socket, (struct sockaddr *) &address, sizeof(address)) < 0) {
        return _status = status::err_socket_bind;
    }

    if (listen(_socket, SOMAXCONN) < 0) {
        return _status = status::err_socket_listening;
    }
#endif

    return _status = status::connected;
}

bool BaseSocket::recv_from(void *buffer, int size) {
    if (_status != SocketStatus::connected)  {
        return false;
    }

#ifdef _WIN32
    ssize_t answ = recv(_socket, reinterpret_cast<char *>(buffer), size, 0);
#else
    ssize_t answ = recv(_socket, reinterpret_cast<char *>(buffer), size, 0);
#endif

    if (answ <= 0 || !size) {
        return false;
    }

    return true;
}

bool BaseSocket::send_to(const void *buffer, size_t size) const {
    if (_status != SocketStatus::connected) {
        return false;
    }

    if (send(_socket, reinterpret_cast<const char *>(buffer), size, 0) < 0) {
        return false;
    }

    return true;
}

status BaseSocket::disconnect() {
    _status = status::disconnected;
#ifdef _WIN32
    if (_socket == INVALID_SOCKET) {
#else
    if (_socket == -1) {
#endif
        return _status;
    }

    shutdown(_socket, SD_BOTH);

#ifdef _WIN32
    closesocket(_socket);
    _socket = INVALID_SOCKET;
#else
    close(_socket);
    _socket = -1;
#endif

    return _status;
}

uint32_t BaseSocket::get_host() const {
#ifdef _WIN32
    return _address.sin_addr.S_un.S_addr;
#else
    return _address.sin_addr.s_addr;
#endif
}

uint16_t BaseSocket::get_port() const {
    return _address.sin_port;
}

BaseSocket &BaseSocket::operator=(BaseSocket &&sok) noexcept {
    _status     = sok._status;
    _socket     = sok._socket;
    _type       = sok._type;
    _address    = sok._address;

#ifdef _WIN32
    sok._socket     = INVALID_SOCKET;
#else
    sok._socket     = -1;
#endif
    sok._status     = status::disconnected;
    sok._address    = socket_addr_in();
    sok._type       = (uint16_t)SocketType::unset_type;
    return *this;
}

BaseSocket::BaseSocket(BaseSocket &&sok) noexcept
        : _status   (sok._status)
          , _type   (sok._type)
          , _socket (sok._socket)
          , _address(sok._address) {
#ifdef _WIN32
    sok._socket     = INVALID_SOCKET;
#else
    sok._socket     = -1;
#endif
    sok._status     = status::disconnected;
    sok._address    = socket_addr_in();
    sok._type       = (uint16_t)SocketType::unset_type;
}

bool BaseSocket::is_allow_to_read(long timeout) const {
    if (_status == status::disconnected) {
        return false;
    }

    fd_set rfds;
    struct timeval tv{};
    FD_ZERO(&rfds);
    FD_SET(_socket, &rfds);

    tv.tv_sec = timeout / 1000;
    tv.tv_usec = timeout % 1000;
    int selres = select(_socket + 1, &rfds, nullptr, nullptr, &tv);
    switch (selres){
        case -1:
            return false;
        case 0:
            return false;
        default:
            break;
    }

    if (FD_ISSET(_socket, &rfds)){
        return true;
    } else {
        return false;
    }
}

bool BaseSocket::is_allow_to_write(long timeout) const {
    if (_status == status::disconnected) {
        return false;
    }

    fd_set wfds;
    struct timeval tv{};
    FD_ZERO(&wfds);
    FD_SET(_socket, &wfds);

    tv.tv_sec = timeout / 1000;
    tv.tv_usec = timeout % 1000;
    int selres = select(_socket + 1, nullptr, &wfds, nullptr, &tv);
    switch (selres){
        case -1:
            return false;
        case 0:
            return false;
        default:
            break;
    }

    if (FD_ISSET(_socket, &wfds)){
        return true;
    } else {
        return false;
    }
}

bool BaseSocket::is_allow_to_rwrite(long timeout) const {
    if (_status == status::disconnected) {
        return false;
    }

    fd_set rfds, wfds;
    struct timeval tv{};
    FD_ZERO(&wfds);
    FD_SET(_socket, &wfds);
    FD_ZERO(&rfds);
    FD_SET(_socket, &rfds);

    tv.tv_sec = timeout / 1000;
    tv.tv_usec = timeout % 1000;
    int selres = select(_socket + 1,  &rfds, &wfds, nullptr, &tv);
    switch (selres){
        case -1:
            return false;
        case 0:
            return false;
        default:
            break;
    }

    if (FD_ISSET(_socket, &rfds) && FD_ISSET(_socket, &wfds)){
        return true;
    } else {
        return false;
    }
}

socket_t BaseSocket::get_socket() {
    return _socket;
}

socket_addr_in BaseSocket::get_address() const {
    return _address;
}

status BaseSocket::get_status() const {
    return _status;
}

SocketType BaseSocket::get_type() const {
    return (SocketType)_type;
}
