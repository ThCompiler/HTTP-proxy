#include "tcp_base_socket.hpp"

using namespace bstcp;



#include <iostream>

BaseSocket::~BaseSocket() {
    disconnect();
}

status BaseSocket::init(uint32_t host, uint16_t port, uint8_t type) {
    if (_status == status::connected) {
        disconnect();
    }

    if ((uint8_t)type & (uint8_t)SocketType::server_socket) {
        return _init_as_server(host, port, type);
    }
    if ((uint8_t)type & (uint8_t)SocketType::server_socket) {
        return _init_as_client(host, port, type);
    }
    return status::err_socket_type;
}

status BaseSocket::_init_as_client(uint32_t host, uint16_t port, uint8_t type) {
#ifndef _WIN32
    int _type = SOCK_STREAM;
    if ((uint8_t)type & (uint8_t)SocketType::nonblocking_socket) {
        _type |= SOCK_NONBLOCK;
    }
#endif

#ifdef _WIN32
    if(WSAStartup(MAKEWORD(2, 2), &w_data) != 0) {}
    if((_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_IP)) == INVALID_SOCKET) {
        return _status = status::err_socket_init;
    }
#else
    if ((_socket = socket(AF_INET, _type, IPPROTO_IP)) < 0) {
        return _status = status::err_socket_init;
    }
#endif

#ifdef _WIN32
    if (unsigned long mode = 0; (uint8_t)type & (uint8_t)SocketType::nonblocking_socket
        && ioctlsocket(serv_socket, FIONBIO, &mode) == SOCKET_ERROR) {
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

    return _status = status::connected;
}

status BaseSocket::accept(const BaseSocket& server_socket) {
    if (_status == status::connected) {
        disconnect();
    }

    sock_len_t addrlen = sizeof(socket_addr_in);
#ifdef _WIN32
    if ((_socket = accept(server_socket._socket, (struct sockaddr *) &_address,
                           &addrlen)) == 0) {
#else
    if ((_socket = accept4(server_socket._socket, (struct sockaddr *) &_address,
                           &addrlen,
                          0)) < 0) {
#endif
        return _status = status::disconnected;
    }
    return _status = status::connected;
}

status BaseSocket::_init_as_server(uint32_t, uint16_t port, uint8_t type) {
    socket_addr_in address;
#ifdef _WIN32
    address.sin_addr.S_un.S_addr = INADDR_ANY;
#else
    address.sin_addr.s_addr = INADDR_ANY;
#endif
    address.sin_port = htons(port);
    address.sin_family = AF_INET;


#ifdef _WIN32
    if ((_serv_socket = socket(AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET) {
        return _status = ServerStatus::err_socket_init;
    }

    if (unsigned long mode = 0; ioctlsocket(serv_socket, FIONBIO, &mode) == SOCKET_ERROR) {
        return _status = ServerStatus::err_socket_init;
    }

     if (flag = true; setsockopt(_serv_socket, SOL_SOCKET, SO_REUSEADDR, (char *) &flag, sizeof(flag)) == -1)  {
        return _status = ServerStatus::err_socket_bind;
    }

    if (bind(_serv_socket, (struct sockaddr *) &address, sizeof(address)) == SOCKET_ERROR) {
        return _status = ServerStatus::err_socket_bind;
    }

    if (listen(_serv_socket, SOMAXCONN) == SOCKET_ERROR) {
        return _status = ServerStatus::err_socket_listening;
    }
#else
    int _type = SOCK_STREAM;
    if ((uint8_t)type & (uint8_t)SocketType::nonblocking_socket) {
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

bool BaseSocket::recv_from(void *buffer, size_t size) {
    if (_status != SocketStatus::connected)  {
        return false;
    }

#ifdef _WIN32
    ssize_t answ = recv(_socket, reinterpret_cast<char *>(buffer), size, 0);
#else
    ssize_t answ = recv(_socket, reinterpret_cast<char *>(buffer), size, 0);
#endif

    if (!answ || !size) {
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
    return address.sin_addr.S_un.S_addr;
#else
    return _address.sin_addr.s_addr;
#endif
}

uint16_t BaseSocket::get_port() const {
    return _address.sin_port;
}

BaseSocket &BaseSocket::operator=(BaseSocket &&sok) noexcept {
    _status = sok._status;
    _socket = sok._socket;
    _address = sok._address;
#ifdef _WIN32
    sok._socket = INVALID_SOCKET;
#else
    sok._socket = -1;
#endif
    sok._status = status::disconnected;
    sok._address = socket_addr_in();
    return *this;
}

BaseSocket::BaseSocket(BaseSocket &&sok) noexcept
        : _status(sok._status)
          , _socket(sok._socket)
          , _address(sok._address) {
#ifdef _WIN32
    sok._socket = INVALID_SOCKET;
#else
    sok._socket = -1;
#endif
    sok._status = status::disconnected;
    sok._address = socket_addr_in();
}
