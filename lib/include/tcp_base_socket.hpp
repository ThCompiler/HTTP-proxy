#pragma once

#include "tcp_lib.hpp"

namespace bstcp {

class BaseSocket : public ISocket {
  public:

#ifdef _WIN32
    BaseSocket()
        : _status(status::disconnected)
        , _socket(INVALID_SOCKET)
        , _address() {}
#else
    BaseSocket()
        : _status(status::disconnected)
          , _socket(-1)
          , _address() {}
#endif

    BaseSocket(const BaseSocket&) = delete;
    BaseSocket operator=(const BaseSocket&) = delete;

    BaseSocket(BaseSocket&& sok) noexcept;

    BaseSocket& operator=(BaseSocket&& sok)  noexcept;

    status init(uint32_t host, uint16_t port, uint8_t type);

    status accept(const BaseSocket& server_socket);

    ~BaseSocket() override;

    [[nodiscard]] uint32_t get_host() const override;

    [[nodiscard]] uint16_t get_port() const override;

    [[nodiscard]] status get_status() const override {
        return _status;
    }

    status disconnect() final;

    bool recv_from(void *buffer, int size) override;

    bool send_to(const void *buffer, size_t size) const override;

    [[nodiscard]] SocketType get_type() const override {
        return SocketType::server_socket;
    }

    socket_t get_socket() {
        return _socket;
    }

    [[nodiscard]] socket_addr_in get_address() const {
        return _address;
    }


  private:

    status _init_as_client(uint32_t host, uint16_t port, uint8_t type);

    status _init_as_server(uint32_t host, uint16_t port, uint8_t type);

    status          _status = status::connected;
    socket_t        _socket;
    socket_addr_in  _address;
};

}