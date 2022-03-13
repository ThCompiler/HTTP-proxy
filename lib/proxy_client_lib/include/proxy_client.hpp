#pragma once

#include <ostream>

#include "tcp_server_lib.hpp"
#include "tcp_socket.hpp"
#include "repository_lib.hpp"

namespace proxy {

struct request_t;

class ProxyClient : public bstcp::IServerClient {
  public:
    ProxyClient() = delete;

    explicit ProxyClient(TcpSocket &&socket)
            : _socket(std::move(socket)) {}

    ProxyClient(const ProxyClient &) = delete;

    ProxyClient operator=(const ProxyClient &) = delete;

    ProxyClient(ProxyClient &&clt) noexcept
            : _socket(std::move(clt._socket)) {}

    ProxyClient &operator=(const ProxyClient &&) = delete;

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

    [[nodiscard]] bool is_allow_to_read(long timeout) const override;

    [[nodiscard]] bool is_allow_to_write(long timeout) const override;

    [[nodiscard]] bool is_allow_to_rwrite(long timeout) const override;

    static void set_repository(const std::string& conn_string);

  private:
    static std::string _read_from_socket(bstcp::ISocket &socket, size_t chank_size);

    static bool _send_to_socket(bstcp::ISocket &socket, const std::string& data, size_t chank_size);

    static std::string _init_client_socket(const std::string& host, size_t port, TcpSocket &socket);

    static std::string _parse_not_proxy_request(http::Request& req);

    static std::string _get_list(http::Request& req);

    static std::string _resend_request(rp::request_t& req);

    static std::string _repeat_request(http::Request& req);

    static std::string _search_vulnerability(http::Request& req);

    std::string _parse_proxy_request(http::Request& req);

    std::string _parse_request(std::string &data);

    std::string _http_request(request_t &request);

    std::string _https_request(request_t &request);

    TcpSocket _socket;

    static std::unique_ptr<rp::PQStoreRequest> _rep;
};

}