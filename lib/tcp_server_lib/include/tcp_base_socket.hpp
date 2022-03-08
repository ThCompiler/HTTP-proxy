#pragma once

#include "tcp_utilits.hpp"

namespace bstcp {

class BaseSocket : public ISocket {
  public:

#ifdef _WIN32
    BaseSocket()
        : _status(status::disconnected)
        , _type((uint16_t)SocketType::unset_type)
        , _socket(INVALID_SOCKET)
        , _address() {}
#else
    BaseSocket()
        : _status(status::disconnected)
        , _type((uint16_t)SocketType::unset_type)
          , _socket(-1)
          , _address() {}
#endif

    BaseSocket(const BaseSocket&) = delete;
    BaseSocket operator=(const BaseSocket&) = delete;

    BaseSocket(BaseSocket&& sok) noexcept;

    BaseSocket& operator=(BaseSocket&& sok)  noexcept;

    status init(uint32_t host, uint16_t port, uint16_t type);

    status accept(const BaseSocket& server_socket);

    ~BaseSocket() override;

    [[nodiscard]] uint32_t get_host() const override;

    [[nodiscard]] uint16_t get_port() const override;

    [[nodiscard]] status get_status() const override;

    status disconnect() override;

    bool recv_from(void *buffer, int size) override;

    bool send_to(const void *buffer, int size) const override;

    [[nodiscard]] SocketType get_type() const override;

    socket_t get_socket();

    [[nodiscard]] socket_addr_in get_address() const;

    [[nodiscard]] bool is_allow_to_read(long timeout) const override;

    [[nodiscard]] bool is_allow_to_write(long timeout) const override;

    [[nodiscard]] bool is_allow_to_rwrite(long timeout) const override;

  private:

    status _init_as_client(uint32_t host, uint16_t port, uint16_t type);

    status _init_as_server(uint32_t host, uint16_t port, uint16_t type);

  protected:

    status          _status;
    uint16_t        _type;
    socket_t        _socket;
    socket_addr_in  _address;

};

}