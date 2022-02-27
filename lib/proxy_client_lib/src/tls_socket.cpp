#include "tls_socket.hpp"
#include "ssl_cert.hpp"

#include <iostream>

extern "C" {
#include "openss_utilits.c"
}

namespace proxy {
SSLSocket::~SSLSocket() {
    TcpSocket::~TcpSocket();
}

status SSLSocket::init(TcpSocket &&base_socket, bool client, std::string domain) {
    if (base_socket._status != bstcp::status::connected) {
        return _ssl_status = base_socket._status;
    }

    _status     = base_socket.get_status();
    _type       = (uint16_t) base_socket.get_status();
    _socket     = base_socket.get_socket();
    _address    = base_socket.get_address();

#ifdef _WIN32
    base_socket._socket = INVALID_SOCKET;
#else
    base_socket._socket = -1;
#endif
    base_socket._status = status::disconnected;
    base_socket._address = socket_addr_in();
    base_socket._type = (uint16_t) SocketType::unset_type;

    if (!SSLCert::is_inited()) {
        return _ssl_status = bstcp::status::err_socket_init;
    }

    if (client) {
        _cert = SSLCert::get_client_cert();
    } else {
        _cert = SSLCert::get_server_cert(domain);
    }

    if (_cert == nullptr) {
        SSLCert::free_cert(_cert);
        return _ssl_status = bstcp::status::err_socket_init;
    }

    ERR_clear_error();

    _ssl_socket = SSL_new(_cert);
    if (_ssl_socket == nullptr) {
        _cert = nullptr;
        SSLCert::free_cert(_cert);
        return _ssl_status = bstcp::status::err_socket_bind;
    }

    SSL_set_fd(_ssl_socket, (int) _socket);

    int status = 0;
    if (client) {
        status = SSL_connect(_ssl_socket);
    } else {
        status = SSL_accept(_ssl_socket);
        auto ret = SSL_get_error(_ssl_socket, status);
        while (ret == SSL_ERROR_WANT_READ || ret == SSL_ERROR_WANT_WRITE) {
            ret = SSL_get_error(_ssl_socket, status);
            ERR_clear_error();
            status = SSL_accept(_ssl_socket);
        }
    }

    if (status != 1) {
        ERR_print_errors_fp(stdout);
        ERR_clear_error();
        SSLCert::free_cert(_cert);
        return _ssl_status = bstcp::status::err_socket_connect;
    }

    return _ssl_status = bstcp::status::connected;
}


bool SSLSocket::recv_from(void *buffer, int size) {
    if (_ssl_status != SocketStatus::connected) {
        return false;
    }

    auto answ = SSL_read(_ssl_socket, buffer, size);
    if (answ <= 0) {
        ERR_clear_error();
        return false;
    }

    return true;
}

bool SSLSocket::send_to(const void *buffer, int size) const {
    if (_ssl_status != SocketStatus::connected) {
        return false;
    }

    if (SSL_write(_ssl_socket, buffer, size) < 0) {
        ERR_clear_error();
        return false;
    }

    return true;
}

status SSLSocket::disconnect() {
    _clear_ssl();
    return TcpSocket::disconnect();
}

SSLSocket &SSLSocket::operator=(SSLSocket &&sok) noexcept {
    _status     = sok._status;
    _socket     = sok._socket;
    _type       = sok._type;
    _address    = sok._address;
    _ssl_socket = sok._ssl_socket;
    _ssl_status = sok._ssl_status;
    _cert       = sok._cert;


#ifdef _WIN32
    sok._socket     = INVALID_SOCKET;
#else
    sok._socket     = -1;
#endif
    sok._status     = status::disconnected;
    sok._address    = socket_addr_in();
    sok._type       = (uint16_t) SocketType::unset_type;
    sok._cert       = nullptr;
    sok._ssl_status = status::disconnected;
    sok._ssl_socket = nullptr;
    return *this;
}

SSLSocket::SSLSocket(SSLSocket &&sok) noexcept
        : _ssl_socket(nullptr)
          , _cert(nullptr)
          , _ssl_status(status::disconnected) {
    *this = std::move(sok);
}


TcpSocket SSLSocket::release() {
    auto tmp = TcpSocket();

    tmp._status     = _status;
    tmp._socket     = _socket;
    tmp._address    = _address;
    tmp._type       = _type;

    _clear_ssl();
    return tmp;
}

void SSLSocket::_clear_ssl() {
    if (_ssl_status != status::connected) {
        return;
    }
    SSLCert::free_cert(_cert);
    _cert = nullptr;
    _ssl_status = status::disconnected;
}


}