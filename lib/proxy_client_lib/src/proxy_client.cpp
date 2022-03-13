#include "proxy_client.hpp"
#include "tls_socket.hpp"

#include "request_parser_lib.hpp"

#include <regex>
#include <iostream>

const auto https_method = "CONNECT";
const auto HTTPS = "https";
const auto HTTP = "http";
const auto https_answer = "HTTP/1.0 200 Connection established\n\n";

const size_t client_chank_size = 1024;
const size_t server_chank_size = 20000;

using namespace proxy;

static const std::regex url_r(
        R"((((https|http)?:\/\/)||())(?:www\.)?([-a-zA-Z0-9@:%._\+~#=]{2,256}\.[a-z]{2,6}\b)+(\/[\/\d\w\.-]*)*(?:[\?])*([-a-zA-Z0-9@:%._\/+~#=]+)*)");

static int get_port(std::string &url) {
    auto end = url.find_last_of(':');
    if (end == std::string::npos) {
        return 80;
    }
    auto port = url.substr(end + 1, url.size() - end);
    url = url.substr(0, end);
    return strtol(port.c_str(), nullptr, 10);
}

static std::string get_protocol(std::string &url) {
    auto end = url.find('/');
    if (end == std::string::npos) {
        return "http";
    }
    auto protocol = url.substr(0, end - 1);
    url = url.substr(end + 2, url.size() - end);
    return protocol;
}

static std::string get_hostname(std::string &url) {
    auto end = url.find('/');
    if (end == std::string::npos) {
        auto hostname = url;
        url = "/";
        return hostname;
    }
    auto hostname = url.substr(0, end);
    url = url.substr(end, url.size() - end);
    return hostname;
}

namespace proxy {

    struct request_t {
        int port{};
        std::string hostname;
        std::string method;
        std::string url;
        http::Request data;
        std::string protocol;
    };

    std::unique_ptr<rp::PQStoreRequest> ProxyClient::_rep = nullptr;

    void ProxyClient::set_repository(const std::string &conn_string) {
        _rep = std::make_unique<rp::PQStoreRequest>(conn_string);
    }
}

std::string ProxyClient::_read_from_socket(bstcp::ISocket &socket, size_t chank_size) {
    tcp_data_t buffer(chank_size);

    if(!socket.is_allow_to_read(1000)) {
        break;
    }


    bool status = socket.recv_from(buffer.data(), (int)chank_size);
    if (!status) {
        return "";
    }
    std::string res;

    while (status) {
        res.insert(
                res.end(),
                std::make_move_iterator(buffer.begin()),
                std::make_move_iterator(buffer.end())
        );
        auto end = res.find('\0');
        if (end != std::string::npos) {
            res = res.substr(0, end);
        }

        if(!socket.is_allow_to_read(1000)) {
            break;
        }

        buffer = tcp_data_t(chank_size);
        status = socket.recv_from(buffer.data(), (int)chank_size);
    }

    return res;
}

bool ProxyClient::_send_to_socket(bstcp::ISocket &socket, const std::string& data, size_t chank_size) {
    bool res = true;
    for (size_t i = 0; (i < data.size()) && res; i += chank_size) {
        res = socket.send_to(data.data() + i, (int)std::min(chank_size, data.size() - i));
    }
    return res;
}

std::string ProxyClient::_init_client_socket(const std::string& host, size_t port, TcpSocket &socket) {
    socket_addr_in adr;
    if (bstcp::hostname_to_ip(host.c_str(), &adr) == -1) {
        return "HTTP/1.1 523 Origin Is Unreachable \n Can't resolve hostname " +
                host + "\n\n";
    }

#ifdef _WIN32
    if (socket.init((uint32_t) adr.sin_addr.S_un.S_addr, request.port,
#else
    if (socket.init((uint32_t) adr.sin_addr.s_addr, port,
#endif
                    (uint16_t) SocketType::blocking_socket
                    | (uint16_t) SocketType::client_socket) !=
        SocketStatus::connected) {
        return "HTTP/1.1 503 Service Unavailable \n Can't connect to host \n\n";
    }
    return "";
}

std::string ProxyClient::_parse_request(std::string &data) {
    http::Request tmp(data);
    if (tmp.get_header("Proxy-Connection").empty() && tmp.get_method() != https_method) {
        return _parse_not_proxy_request(tmp);
    }
    return _parse_proxy_request(tmp);
}


std::string ProxyClient::_parse_proxy_request(http::Request& data) {
    data.delete_header("Proxy-Connection");

    auto url = data.get_url();
    request_t req;
    req.method = data.get_method();
    req.protocol = req.method  == https_method ? HTTPS : get_protocol(url);
    req.hostname = get_hostname(url);
    req.port = ::get_port(req.hostname);
    req.url = std::move(url);
    data.set_url(req.url);

    req.data = std::move(data);

    std::cout << "Connect to client" + req.protocol + "://" +
                 req.hostname + ":" << req.port << std::endl;

    if (req.protocol == HTTP) {
        return _http_request(req);
    }

    if (req.protocol == HTTPS && req.method == https_method) {
        return _https_request(req);
    }

    return "HTTP/1.1 404 Not found \n Unknown protocol " + req.protocol + "\n\n";
}


std::string ProxyClient::_https_request(request_t &request) {
    send_to(https_answer, (int)std::string(https_answer).size());

    SSLSocket client_socket;
    if (client_socket.init(std::move(_socket), false, request.hostname) != bstcp::status::connected) {
        return "HTTP/1.1 525 SSL Handshake Failed \n Can't connect to client by tls \n\n";
    }

    auto message = _read_from_socket(client_socket, client_chank_size);

    if (message.empty()) {
        return "HTTP/1.1 400 Bad request \n Empty message from client \n\n";
    }

    try {
        ProxyClient::_rep->add(rp::request_t{
                .is_valid = true,
                .is_https = true,
                .id = 0,
                .port = (size_t)request.port,
                .host = request.hostname,
                .request = http::Request(message)
        });
    } catch(std::exception& e) {
        std::cerr << e.what() << "\n";
    }

    TcpSocket to;
    auto res = _init_client_socket(request.hostname, request.port, to);
    if (!res.empty()) {
        return res;
    }

    SSLSocket ssl_socket;
    if (ssl_socket.init(std::move(to)) != bstcp::status::connected) {
        return "HTTP/1.1 525 SSL Handshake Failed \n Can't connect to server by tls \n\n";
    }

    ssl_socket.send_to(message.data(), message.size());
    message.clear();

    auto answ = _read_from_socket(ssl_socket, server_chank_size);
    _send_to_socket(client_socket, answ, client_chank_size);
    _socket = client_socket.release();
    return "";
}

std::string ProxyClient::_http_request(request_t &request) {
    try {
        ProxyClient::_rep->add(rp::request_t{
                .is_valid = true,
                .is_https = false,
                .id = 0,
                .port = (size_t)request.port,
                .host = request.hostname,
                .request = request.data
        });
    } catch(std::exception& e) {
        std::cerr << e.what() << "\n";
    }

    TcpSocket to;
    auto res = _init_client_socket(request.hostname, request.port, to);
    if (!res.empty()) {
        return res;
    }

    _send_to_socket(to, request.data.string(), client_chank_size);

    if (!to.is_allow_to_read(2000)) {
        return "HTTP/1.1 408 Request Timeout  \n 2s time out \n\n";
    }

    auto answ = _read_from_socket(to, server_chank_size);
    _send_to_socket(*this, answ, client_chank_size);
    return "";
}


void ProxyClient::handle_request() {
    std::string data = _read_from_socket(*this, client_chank_size);
    if (data.empty()) {
        return;
    }

    std::cout << "Client " << " send data [ " << data.size()
              << " bytes ]: \n" << (char *) data.data() << '\n';
    auto res = _parse_request(data);

    if (!res.empty()) {
        _send_to_socket(*this, res, client_chank_size);
    }
    disconnect();
}

uint32_t ProxyClient::get_host() const {
    return _socket.get_host();
}

uint16_t ProxyClient::get_port() const {
    return _socket.get_port();
}

bstcp::SocketStatus ProxyClient::get_status() const {
    return _socket.get_status();
}

bstcp::SocketStatus ProxyClient::disconnect() {
    return _socket.disconnect();
}

bool ProxyClient::recv_from(void *buffer, int size) {
    return _socket.recv_from(buffer, size);
}

bool ProxyClient::send_to(const void *buffer, int size) const {
    return _socket.send_to(buffer, size);
}

SocketType ProxyClient::get_type() const {
    return SocketType::client_socket;
}

socket_t ProxyClient::get_socket() {
    return _socket.get_socket();
}

socket_addr_in ProxyClient::get_address() const {
    return _socket.get_address();
}

bool ProxyClient::is_allow_to_write(long timeout) const {
    return _socket.is_allow_to_write(timeout);
}

bool ProxyClient::is_allow_to_rwrite(long timeout) const {
    return _socket.is_allow_to_rwrite(timeout);
}

bool ProxyClient::is_allow_to_read(long timeout) const {
    return _socket.is_allow_to_read(timeout);
}


