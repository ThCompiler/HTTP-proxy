#include "proxy_client.hpp"
#include "tls_socket.hpp"

#include "request_parser_lib.hpp"

#include <regex>
#include <iostream>


const auto https_method = "CONNECT";
const auto HTTPS = "https";
const auto HTTP = "http";
const std::string https_answer = "HTTP/1.0 200 Connection established\n\n";

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

/*
static size_t get_content_len(std::string &url) {
    auto end = url.find('/');
    if (end == std::string::npos) {
        return 0;
    }
    auto protocol = url.substr(0, end - 1);
    url = url.substr(end + 2, url.size() - end);
    return 1;
}*/

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

static std::string get_method(const std::string &request) {
    auto end = request.find(' ');
    if (end == std::string::npos) {
        return "";
    }
    return request.substr(0, end);
}

namespace proxy {

struct request_t {
    int port;
    std::string hostname;
    std::string method;
    std::string url;
    std::string &data;
    std::string protocol;

    request_t(std::string &&url_, std::string &data_)
            : url(url_), data(data_) {
        method = get_method(data);
        protocol = method == https_method ? HTTPS : get_protocol(url);
        hostname = get_hostname(url);
        port = get_port(hostname);
        data = std::regex_replace(data, url_r, url,
                                  std::regex_constants::format_first_only);
    }
};

}

static std::string read_from_socket(bstcp::ISocket &socket, size_t chank_size) {
    tcp_data_t buffer(chank_size);

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

static bool send_to_socket(bstcp::ISocket &socket, const std::string& data, size_t chank_size) {
    bool res = true;
    for (size_t i = 0; (i < data.size()) && res; i += chank_size) {
        res = socket.send_to(data.data() + i, (int)std::min(chank_size, data.size() - i));
    }
    return res;
}

static std::string init_client_socket(request_t &request, TcpSocket &socket) {
    socket_addr_in adr;
    if (bstcp::hostname_to_ip(request.hostname.c_str(), &adr) == -1) {
        return "HTTP/1.1 523 Origin Is Unreachable \n Can't resolve hostname " +
               request.hostname + "\n\n";
    }

#ifdef _WIN32
    if (socket.init((uint32_t) adr.sin_addr.S_un.S_addr, request.port,
#else
    if (socket.init((uint32_t) adr.sin_addr.s_addr, request.port,
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
    auto proxy = tmp.json()["headers"].find("Proxy-Connection");
    if (proxy != tmp.json()["headers"].end()) {
        tmp.json()["headers"].erase(proxy);
    }
    std::string  rs = tmp.string();
    std::smatch m;
    if (regex_search(data, m, url_r)) {
        auto url = m[0].str();

        const std::regex proxy_header(
                R"((\r\n|\n)(Proxy-Connection[A-z: .-]+))");
        data = std::regex_replace(data, proxy_header, "",
                                  std::regex_constants::format_first_only);

        request_t request(std::move(url), data);

        std::cout << "Connect to client" + request.protocol + "://" +
                     request.hostname + ":"
                  << request.port << std::endl;

        if (request.protocol == HTTP) {
            return _http_request(request);
        }

        if (request.protocol == HTTPS && request.method == https_method) {
            return _https_request(request);
        }

        return "HTTP/1.1 404 Not found \n Unknown protocol " +
               request.protocol + "\n\n";
    }
    return {};
}

std::string ProxyClient::_https_request(request_t &request) {
    send_to(https_answer.data(), (int) https_answer.size());

    SSLSocket client_socket;
    if (client_socket.init(std::move(_socket), false, request.hostname) != bstcp::status::connected) {
        return "HTTP/1.1 525 SSL Handshake Failed \n Can't connect to client by tls \n\n";
    }

    auto message = read_from_socket(client_socket, client_chank_size);

    if (message.empty()) {
        return "HTTP/1.1 400 Bad request \n Empty message from client \n\n";
    }

    TcpSocket to;
    auto res = init_client_socket(request, to);
    if (!res.empty()) {
        return res;
    }

    SSLSocket ssl_socket;
    if (ssl_socket.init(std::move(to)) != bstcp::status::connected) {
        return "HTTP/1.1 525 SSL Handshake Failed \n Can't connect to server by tls \n\n";
    }

    ssl_socket.send_to(message.data(), message.size());
    message.clear();

    auto answ = read_from_socket(ssl_socket, server_chank_size);
    send_to_socket(client_socket, answ, client_chank_size);
    _socket = client_socket.release();
    return "";
}

std::string ProxyClient::_http_request(request_t &request) {
    TcpSocket to;
    auto res = init_client_socket(request, to);
    if (!res.empty()) {
        return res;
    }

    send_to_socket(to, request.data, client_chank_size);
    request.data.clear();

    if (!to.is_allow_to_read(2000)) {
        return "HTTP/1.1 408 Request Timeout  \n 2s time out \n\n";
    }

    auto answ = read_from_socket(to, server_chank_size);
    send_to_socket(*this, answ, client_chank_size);
    return "";
}


void ProxyClient::handle_request() {
    std::string data = read_from_socket(*this, client_chank_size);
    if (data.empty()) {
        return;
    }

    std::cout << "Client " << " send data [ " << data.size()
              << " bytes ]: \n" << (char *) data.data() << '\n';
    auto res = _parse_request(data);

    if (!res.empty()) {
        send_to_socket(*this, res, client_chank_size);
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
