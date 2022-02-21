#include "tls_socket.hpp"

#include <iostream>


namespace proxy {
SSLSocket::~SSLSocket() {
    TcpSocket::~TcpSocket();
}

status SSLSocket::init(TcpSocket &&base_socket, bool client) {
    if (!SSLSocket::is_init) {
        SSLSocket::is_init = true;
        SSLSocket::_init_ssl_lib();
    }

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

    _cert = _init_ssl(client);
    if (_cert == nullptr) {
        return _ssl_status = bstcp::status::err_socket_init;
    }

    _ssl_socket = SSL_new(_cert);
    if (_ssl_socket == nullptr) {
        SSL_CTX_free(_cert);
        _cert = nullptr;
        return _ssl_status = bstcp::status::err_socket_bind;
    }

    SSL_set_fd(_ssl_socket, (int) _socket);

    int status = 0;
    if (client) {
        status = SSL_connect(_ssl_socket);
    } else {
        status = SSL_accept(_ssl_socket);
    }
    if (status != 1) {
        auto ret = SSL_get_error(_ssl_socket, status);
        if (ret == SSL_ERROR_WANT_READ || ret == SSL_ERROR_WANT_WRITE) {
            return _ssl_status = bstcp::status::connected;
        }
        ERR_print_errors_fp(stdout);
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
        return false;
    }

    return true;
}

bool SSLSocket::send_to(const void *buffer, int size) const {
    if (_ssl_status != SocketStatus::connected) {
        return false;
    }

    if (SSL_write(_ssl_socket, buffer, size) < 0) {
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

int password_cb(char *buf, int size, int, void *password) {
    strncpy(buf, (char *)(password), size);
    buf[size - 1] = '\0';
    return (int)strlen(buf);
}

EVP_PKEY *generate_pk() {
    EVP_PKEY *pkey = nullptr;
    EVP_PKEY_CTX *pctx = EVP_PKEY_CTX_new_id(EVP_PKEY_RSA, nullptr);
    EVP_PKEY_keygen_init(pctx);
    EVP_PKEY_CTX_set_rsa_keygen_bits(pctx, 2048);
    EVP_PKEY_keygen(pctx, &pkey);
    return pkey;
}

X509 *generate_cert(EVP_PKEY *pkey) {
    X509 *x509 = X509_new();
    X509_set_version(x509, 2);
    ASN1_INTEGER_set(X509_get_serialNumber(x509), 0);
    X509_gmtime_adj(X509_get_notBefore(x509), 0);
    X509_gmtime_adj(X509_get_notAfter(x509), (long)60*60*24*365);
    X509_set_pubkey(x509, pkey);

    X509_NAME *name = X509_get_subject_name(x509);
    X509_NAME_add_entry_by_txt(name, "C", MBSTRING_ASC, (const unsigned char*)"CH", -1, -1, 0);
    X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC, (const unsigned char*)"http-proxy", -1, -1, 0);
    X509_set_issuer_name(x509, name);
    X509_sign(x509, pkey, EVP_sha3_512());
    return x509;
}

SSL_CTX *SSLSocket::_init_ssl(bool client) {
    SSL_CTX *ctx = nullptr;
    if (client) {
        const SSL_METHOD *method = TLS_client_method(); /* Create new client-method instance */
        ctx = SSL_CTX_new(method);

        if (ctx == nullptr) {
            ERR_print_errors_fp(stderr);
        }

    } else {
        const SSL_METHOD *method = TLS_server_method(); /* Create new client-method instance */
        ctx = SSL_CTX_new(method);

        if (ctx == nullptr) {
            ERR_print_errors_fp(stderr);
            return ctx;
        }

        EVP_PKEY *pkey = generate_pk();
        X509 *x509 = generate_cert(pkey);

        SSL_CTX_use_certificate(ctx, x509);
        SSL_CTX_set_default_passwd_cb(ctx, password_cb);
        SSL_CTX_use_PrivateKey(ctx, pkey);

        SSL_CTX_set_verify(ctx, SSL_VERIFY_NONE, nullptr);
    }

    return ctx;
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
    _ssl_status = status::disconnected;
    SSL_free(_ssl_socket);
    SSL_CTX_free(_cert);
}

void SSLSocket::_init_ssl_lib() {
    SSL_load_error_strings();
    ERR_load_crypto_strings();

    OpenSSL_add_all_algorithms();
    SSL_library_init();
}

}