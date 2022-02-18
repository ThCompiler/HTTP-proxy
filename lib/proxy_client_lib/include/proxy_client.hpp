#pragma once

#include <ostream>

#include "tcp_server_lib.hpp"

namespace proxy {

struct ProxyClient : public bstcp::IServerClient {
public:
    ProxyClient() = delete;

    explicit ProxyClient(bstcp::BaseSocket&& socket)
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

    bool send_to(const void *buffer, size_t size) const override;

    [[nodiscard]] SocketType get_type() const override;

    socket_t get_socket();

    [[nodiscard]] socket_addr_in get_address() const;

    private:

    bstcp::BaseSocket   _socket;
};

}