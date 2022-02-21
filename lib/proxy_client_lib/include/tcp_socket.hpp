#pragma once

#include "tcp_server_lib.hpp"

namespace proxy {

class SSLSocket;

class TcpSocket : public bstcp::BaseSocket {
  public:

    TcpSocket()
            : BaseSocket() {}

    TcpSocket(const TcpSocket &) = delete;

    TcpSocket operator=(const TcpSocket &) = delete;

    TcpSocket(TcpSocket &&sok) noexcept
            : BaseSocket(std::move(sok)) {}

    TcpSocket &operator=(TcpSocket &&) noexcept = default;

    ~TcpSocket() override {
        BaseSocket::~BaseSocket();
    }

  private:
    friend class SSLSocket;
};

}