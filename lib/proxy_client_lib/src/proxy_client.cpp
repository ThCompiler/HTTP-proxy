#include "proxy_client.hpp"

#include <regex>
#include <iostream>

using namespace proxy;

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

static const std::regex url_r(
        R"((((http|https):\/\/)|(\/)|(..\/))(\w+:{0,1}\w*@)?(\S+)(:[0-9]+)?(\/|\/([\w#!:.?+=&%@!\-\/]))?)");
static const std::regex proxy_header(R"((\r\n|\n)(Proxy-Connection[A-z: .-]+))");

std::string send_requst(std::string url, std::string &data) {
    auto protocol = get_protocol(url);
    auto hostname = get_hostname(url);
    auto port = get_port(hostname);

    if (protocol != "http") {
        return "HTTP/1.1 404 Not found \n Unknown protocol " + protocol + "\n\n";
    }

    data = std::regex_replace(data, url_r, url, std::regex_constants::format_first_only);
    data = std::regex_replace(data, proxy_header, "", std::regex_constants::format_first_only);

    std::cout << "Connect to client " << protocol + "://" + hostname + ":" << port << std::endl;

    BaseSocket to;
    socket_addr_in adr;
    if (bstcp::hostname_to_ip(hostname.c_str(), &adr) == -1) {
        return "HTTP/1.1 523 Origin Is Unreachable \n Can't resolve hostname " + hostname +  "\n\n";
    }

#ifdef _WIN32
    if (to.init((uint32_t) adr.sin_addr.S_un.S_addr, port,
#else
            if (to.init((uint32_t) adr.sin_addr.s_addr, port,
#endif
                (uint16_t) SocketType::nonblocking_socket
                | (uint16_t) SocketType::client_socket) != SocketStatus::connected) {
        return "HTTP/1.1 503 Service Unavailable \n Can't connect to host \n\n";
    }
    to.send_to(data.data(), data.size());
    data.clear();

    if (!to.is_allow_to_read(2000)) {
        return "HTTP/1.1 408 Request Timeout  \n 2s time out \n\n";
    }

    tcp_data_t buffer(1024);
    bool status = to.recv_from(buffer.data(), 1024);
    std::string res;
    while (status) {
        res.insert(
                res.end(),
                std::make_move_iterator(buffer.begin()),
                std::make_move_iterator(buffer.end())
        );
        status = to.recv_from(buffer.data(), 1024);
    }

    auto end = res.find('\0');
    res = res.substr(0, end);

    return res;
}

std::string parseRequest(std::string &data) {
    auto end = data.find('\0');
    data = data.substr(0, end);


    std::smatch m;
    if (regex_search(data, m, url_r)) {
        return send_requst(m[0].str(), data);
    }
    return {};
}

void ProxyClient::handle_request() {
    tcp_data_t buffer(1024);
    auto status = recv_from(buffer.data(), 1024);
    if (!status) return;
    std::string data;
    while (status) {
        data.insert(
                data.end(),
                std::make_move_iterator(buffer.begin()),
                std::make_move_iterator(buffer.end())
        );
        status = recv_from(buffer.data(), 1024);
    }
    std::cout << "Client " << " send data [ " << data.size()
              << " bytes ]: \n" << (char *) data.data() << '\n';
    auto res = parseRequest(data);
    send_to(res.data(), res.size());
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

bool ProxyClient::send_to(const void *buffer, size_t size) const {
    return _socket.send_to(buffer, size);
}

SocketType ProxyClient::get_type() const {
    return SocketType::server_socket;
}

socket_t ProxyClient::get_socket() {
    return _socket.get_socket();
}

socket_addr_in ProxyClient::get_address() const {
    return _socket.get_address();
}
