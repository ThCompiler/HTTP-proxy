#pragma once

#include "tcp_socket.hpp"

#include <openssl/ssl.h>
#include <openssl/err.h>

namespace proxy {

class SSLSocket : public TcpSocket {
  public:

    SSLSocket()
            : TcpSocket()
              , _ssl_socket(nullptr)
              , _cert(nullptr)
              , _ssl_status(bstcp::status::disconnected) {}

    SSLSocket(const SSLSocket &) = delete;

    SSLSocket operator=(const SSLSocket &) = delete;

    SSLSocket(SSLSocket &&sok) noexcept;

    SSLSocket &operator=(SSLSocket &&sok) noexcept;

    bstcp::status init(TcpSocket &&base_socket, bool client = true);

    ~SSLSocket() override;

    bstcp::status disconnect() override;

    bool recv_from(void *buffer, int size) override;

    bool send_to(const void *buffer, int size) const override;

    TcpSocket release();

  private:
    static void     _init_ssl_lib();
    static SSL_CTX *_init_ssl(bool client = true);

    void _clear_ssl();

    using TcpSocket::accept;

    static bool is_init;

    SSL *           _ssl_socket{nullptr};
    SSL_CTX *       _cert{nullptr};
    bstcp::status   _ssl_status;
};

}