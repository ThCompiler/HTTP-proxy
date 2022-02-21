#pragma once

#include <ostream>

#include "tcp_server_lib.hpp"
#include "tcp_socket.hpp"

namespace proxy {

struct request_t;

struct ProxyClient : public bstcp::IServerClient {
  public:
    ProxyClient() = delete;

    explicit ProxyClient(TcpSocket&& socket)
            : _socket(std::move(socket)) {}

    ProxyClient(const ProxyClient&) = delete;
    ProxyClient operator=(const ProxyClient&) = delete;

    ProxyClient(ProxyClient&& clt) noexcept
            : _socket(std::move(clt._socket)) {}

    ProxyClient& operator=(const ProxyClient&&) = delete;

    ~ProxyClient() override = default;

    void handle_request() override;

    [[nodiscard]] uint32_t get_host() const override;

    [[nodiscard]] uint16_t get_port() const override;

    [[nodiscard]] bstcp::SocketStatus get_status() const override;

    bstcp::SocketStatus disconnect() final;

    bool recv_from(void *buffer, int size) override;

    bool send_to(const void *buffer, int size) const override;

    [[nodiscard]] SocketType get_type() const override;

    socket_t get_socket();

    [[nodiscard]] socket_addr_in get_address() const;

  private:

    std::string _parse_request(std::string &data);

    std::string _http_request(request_t& request);

    std::string _https_request(request_t& request);

    TcpSocket   _socket;
};

}