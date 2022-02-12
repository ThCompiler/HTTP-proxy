#include "tcp_server.hpp"

using namespace bstcp;

#ifdef _WIN32
#define WIN(exp) exp
#define NIX(exp)

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
#endif


#include <iostream>

tcp_data_t TcpServer::Client::loadData() {
    if (_status != SocketStatus::connected)  {
        return {};
    }

    tcp_data_t buffer;
    uint32_t size;
    int err;

    // Read data length in non-blocking mode
    // MSG_DONTWAIT - Unix non-blocking read
    WIN(if (u_long t = true; SOCKET_ERROR ==
                             ioctlsocket(socket, FIONBIO, &t))
            return DataBuffer();) // Windows non-blocking mode on
    ssize_t answ = recv(_socket, (char *) &size, sizeof(size), NIX(MSG_DONTWAIT)
                    WIN(0));
    WIN(if (u_long t = false; SOCKET_ERROR ==
                              ioctlsocket(socket, FIONBIO, &t))
            return DataBuffer();) // Windows non-blocking mode off

    // Disconnect
    if (!answ) {
        disconnect();
        return {};
    } else if (answ == -1) {
        // Error handle (f*ckin OS-dependence!)
        WIN(
                err = convertError();
                if (!err) {
                    SockLen_t len = sizeof(err);
                    getsockopt(socket, SOL_SOCKET, SO_ERROR,
                               WIN((char * )) & err, &len);
                }
        )NIX(
                sock_len_t len = sizeof(err);
                getsockopt(_socket, SOL_SOCKET, SO_ERROR, WIN((char * )) & err,
                           &len);
                if (!err) err = errno;
        )


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
                std::cerr << "Unhandled error!\n"
                          << "Code: " << err << " Err: " << std::strerror(err)
                          << '\n';
                return {};
        }
    }

    if (!size) {
        return {};
    }

    buffer.resize(size);
    recv(_socket, buffer.data(), buffer.size(), 0);
    return buffer;
}


BaseSocket::status TcpServer::Client::disconnect() {
    _status = status::disconnected;
    if (_socket == WIN(INVALID_SOCKET)NIX(-1)) {
        return _status;
    }

    shutdown(_socket, SD_BOTH);
    WIN(closesocket)NIX(close)(_socket);
    _socket = WIN(INVALID_SOCKET)NIX(-1);

    return _status;
}

bool TcpServer::Client::sendData(const void *buffer, const size_t size) const {
    if (_status != SocketStatus::connected) {
        return false;
    }

    void *send_buffer = malloc(size + sizeof(uint32_t));
    memcpy(reinterpret_cast<char *>(send_buffer) + sizeof(uint32_t), buffer,
           size);
    *reinterpret_cast<uint32_t *>(send_buffer) = size;

    if (send(_socket, reinterpret_cast<char *>(send_buffer), size + sizeof(int),
             0) < 0) {
        return false;
    }

    free(send_buffer);
    return true;
}

TcpServer::Client::Client(socket_t socket, socket_addr_in address)
        : _socket(socket)
          , _address(address) {}


TcpServer::Client::~Client() {
    if (_socket == WIN(INVALID_SOCKET)NIX(-1)) {
        return;
    }
    shutdown(_socket, SD_BOTH);
    WIN(closesocket(socket);)
    NIX(close(_socket);)
}

uint32_t TcpServer::Client::getHost() const {
    return NIX(_address.sin_addr.s_addr) WIN(address.sin_addr.S_un.S_addr);

}

uint16_t TcpServer::Client::getPort() const {
    return _address.sin_port;
}