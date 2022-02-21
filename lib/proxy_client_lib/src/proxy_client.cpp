#include "proxy_client.hpp"
#include "tls_socket.hpp"

#include <regex>
#include <iostream>


const auto https_method = "CONNECT";
const auto https = "https";
const auto http = "http";
const std::string https_answer = "HTTP/1.0 200 Connection established\n\n";


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

static std::string get_method(const std::string &request) {
    auto end = request.find(' ');
    if (end == std::string::npos) {
        return "";
    }
    return request.substr(0, end);
}

namespace proxy {

    struct request_t {
        int             port;
        std::string     hostname;
        std::string     method;
        std::string     url;
        std::string&    data;
        std::string     protocol;

        request_t(std::string &&url_, std::string &data_)
                : url(url_), data(data_) {
            method = get_method(data);
            protocol = method == https_method ? https : get_protocol(url);
            hostname = get_hostname(url);
            port = get_port(hostname);
            data = std::regex_replace(data, url_r, url, std::regex_constants::format_first_only);
        }
    };

}

static std::string read_from_socket(bstcp::ISocket &socket) {
    tcp_data_t buffer(1024);
    bool status = socket.recv_from(buffer.data(), 1024);
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
        status = socket.recv_from(buffer.data(), 1024);
    }

    auto end = res.find('\0');
    res = res.substr(0, end);
    return res;
}

static std::string init_client_socket(request_t& request, TcpSocket& socket) {
    socket_addr_in adr;
    if (bstcp::hostname_to_ip(request.hostname.c_str(), &adr) == -1) {
        return "HTTP/1.1 523 Origin Is Unreachable \n Can't resolve hostname " + request.hostname +  "\n\n";
    }

#ifdef _WIN32
    if (socket.init((uint32_t) adr.sin_addr.S_un.S_addr, request.port,
#else
            if (socket.init((uint32_t) adr.sin_addr.s_addr, request.port,
#endif
                (uint16_t) SocketType::nonblocking_socket
                | (uint16_t) SocketType::client_socket) != SocketStatus::connected) {
        return "HTTP/1.1 503 Service Unavailable \n Can't connect to host \n\n";
    }
    return "";
}

std::string ProxyClient::_parse_request(std::string &data) {
    std::smatch m;
    if (regex_search(data, m, url_r)) {
        auto url = m[0].str();

        const std::regex proxy_header(R"((\r\n|\n)(Proxy-Connection[A-z: .-]+))");
        data = std::regex_replace(data, proxy_header, "", std::regex_constants::format_first_only);

        request_t request(std::move(url), data);

        std::cout << "Connect to client" + request.protocol + "://" + request.hostname + ":"
        << request.port << std::endl;

        if (request.protocol == http) {
            return _http_request(request);
        }

        if (request.protocol == https && request.method == https_method) {
            return _https_request(request);
        }

        return "HTTP/1.1 404 Not found \n Unknown protocol " + request.protocol + "\n\n";
    }
    return {};
}

std::string ProxyClient::_https_request(request_t& request) {
    send_to(https_answer.data(), (int)https_answer.size());

    TcpSocket to;
    auto res = init_client_socket(request, to);
    if (!res.empty()) {
        return res;
    }

    SSLSocket ssl_socket;
    ssl_socket.init(std::move(to));

    auto message = read_from_socket(*this);

    ssl_socket.send_to(message.data(), message.size());
    message.clear();

    return read_from_socket(ssl_socket);
}

std::string ProxyClient::_http_request(request_t& request) {
    TcpSocket to;
    auto res = init_client_socket(request, to);
    if (!res.empty()) {
        return res;
    }

    to.send_to(request.data.data(), request.data.size());
    request.data.clear();

    if (!to.is_allow_to_read(2000)) {
        return "HTTP/1.1 408 Request Timeout  \n 2s time out \n\n";
    }

    return read_from_socket(to);
}


void ProxyClient::handle_request() {
    std::string data = read_from_socket(*this);
    if (data == "") {
        return;
    }

    std::cout << "Client " << " send data [ " << data.size()
              << " bytes ]: \n" << (char *) data.data() << '\n';
    auto res = _parse_request(data);
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