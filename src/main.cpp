#include "tcp_server.hpp"

#include <iostream>
#include <regex>
#include <getopt.h>

using namespace bstcp;

//Parse ip to std::string
std::string getHostStr(const uniq_ptr<TcpServer::Client> &client) {
    uint32_t ip = client->get_host();
    return std::string() + std::to_string(int(reinterpret_cast<unsigned char *>(&ip)[0])) + '.' +
           std::to_string(int(reinterpret_cast<unsigned char *>(&ip)[1])) + '.' +
           std::to_string(int(reinterpret_cast<unsigned char *>(&ip)[2])) + '.' +
           std::to_string(int(reinterpret_cast<unsigned char *>(&ip)[3])) + ':' +
           std::to_string(client->get_port());
}

int getPort(std::string &url) {
    auto end = url.find_last_of(':');
    if (end == std::string::npos) {
        return 80;
    }
    auto port = url.substr(end + 1, url.size() - end);
    url = url.substr(0, end);
    return strtol(port.c_str(), nullptr, 10);
}

std::string getProtocol(std::string &url) {
    auto end = url.find('/');
    if (end == std::string::npos) {
        return "http";
    }
    auto protocol = url.substr(0, end - 1);
    url = url.substr(end + 2, url.size() - end);
    return protocol;
}

std::string getHostname(std::string &url) {
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

const std::regex url_r(
        R"((((ftp|http|https):\/\/)|(\/)|(..\/))(\w+:{0,1}\w*@)?(\S+)(:[0-9]+)?(\/|\/([\w#!:.?+=&%@!\-\/]))?)");
const std::regex proxy(R"((\r\n|\n)(Proxy-Connection[A-z: .-]+))");

std::string send_requst(std::string url, std::string &data) {
    auto protocol = getProtocol(url);
    auto hostname = getHostname(url);
    auto port = getPort(hostname);

    if (protocol != "http") {
        return "HTTP/1.1 404 Not found \n Unknown protocol " + protocol + "\n\n";
    }

    data = std::regex_replace(data, url_r, url, std::regex_constants::format_first_only);
    data = std::regex_replace(data, proxy, "", std::regex_constants::format_first_only);

    std::cout << "Connect to client " << protocol + "://" + hostname + ":" << port << std::endl;

    BaseSocket to;
    socket_addr_in adr;
    if (hostname_to_ip(hostname.c_str(), &adr) == -1) {
        return "";
    }

#ifdef _WIN32
    if (to.init((uint32_t) adr.sin_addr.S_un.S_addr, port,
#else
    if (to.init((uint32_t) adr.sin_addr.s_addr, port,
#endif
                (uint16_t) SocketType::nonblocking_socket
                | (uint16_t) SocketType::client_socket) != SocketStatus::connected) {
        return "HTTP/1.1 500 Error \n Can't connect to host \n\n";
    }
    to.send_to(data.data(), data.size());
    data.clear();

    if (!to.is_allow_to_read(2000)) {
        return "HTTP/1.1 524 A Timeout Occurred  \n 2s time out \n\n";
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

std::string sendRequest(std::string &data) {
    auto end = data.find('\0');
    data = data.substr(0, end);


    std::smatch m;
    if (regex_search(data, m, url_r)) {
        return send_requst(m[0].str(), data);
    }
    return {};
}


int main(int argc, char *argv[]) {
    int opt;

    int http_port = 8081;
    while ((opt = getopt(argc, argv, "p:")) != -1) {
        if (opt == 'p') {
            http_port = strtol(optarg, nullptr, 10);
        }
    }

    TcpServer server(http_port,
                     {1, 1, 1}, // Keep alive{idle:1s, interval: 1s, pk_count: 1}

                     [](uniq_ptr<TcpServer::Client> &client) { // Data handler
                         tcp_data_t buffer(1024);
                         auto status = client->recv_from(buffer.data(), 1024);
                         if (!status) return;
                         std::string data;
                         while (status) {
                             data.insert(
                                     data.end(),
                                     std::make_move_iterator(buffer.begin()),
                                     std::make_move_iterator(buffer.end())
                             );
                             status = client->recv_from(buffer.data(), 1024);
                         }
                         auto res = sendRequest(data);
                         std::cout << "Client " << getHostStr(client) << " send data [ " << data.size()
                                   << " bytes ]: \n" << (char *) data.data() << '\n';
                         client->send_to(res.data(), res.size());
                         client->disconnect();
                     },

                     [](uniq_ptr<TcpServer::Client> &client) { // Connect handler
                         std::cout << "Client " << getHostStr(client) << " connected\n";
                     },

                     [](uniq_ptr<TcpServer::Client> &client) { // Disconnect handler
                         std::cout << "Client " << getHostStr(client) << " disconnected\n";
                     },

                     std::thread::hardware_concurrency() // Thread pool size
    );

    try {
        //Start server
        if (server.start() == TcpServer::ServerStatus::up) {
            std::cout << "Server listen on port: " << server.get_port() << std::endl
                      << "Server handling thread pool size: " << server.get_thread_pool().getThreadCount() << std::endl;
            server.joinLoop();
            return EXIT_SUCCESS;
        } else {
            std::cout << "Server start error! Error code:" << int(server.get_status()) << std::endl;
            return EXIT_FAILURE;
        }

    } catch (std::exception &except) {
        std::cerr << except.what();
        return EXIT_FAILURE;
    }
}