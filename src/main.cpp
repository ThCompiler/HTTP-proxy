#include "tcp_server.hpp"

#include <iostream>
#include <regex>

using namespace bstcp;

//Parse ip to std::string
std::string getHostStr(const uniq_ptr<TcpServer::Client>& client) {
    uint32_t ip = client->get_host();
    return std::string() + std::to_string(int(reinterpret_cast<char*>(&ip)[0])) + '.' +
           std::to_string(int(reinterpret_cast<char*>(&ip)[1])) + '.' +
           std::to_string(int(reinterpret_cast<char*>(&ip)[2])) + '.' +
           std::to_string(int(reinterpret_cast<char*>(&ip)[3])) + ':' +
           std::to_string( client->get_port());
}

int getPort(std::string& url) {
    auto end = url.find_last_of(':');
    if (end == std::string::npos) {
        return 80;
    }
    auto port = url.substr(end + 1, url.size() - end);
    url = url.substr(0, end);
    return strtol(port.c_str(), nullptr, 10);
}

std::string getProtocol(std::string& url) {
    auto end = url.find('/');
    if (end == std::string::npos) {
        return "http";
    }
    auto protocol = url.substr(0, end - 1);
    url = url.substr(end + 2, url.size() - end);
    return protocol;
}

std::string getHostname(std::string& url) {
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

const std::regex url_r(R"((((ftp|http|https):\/\/)|(\/)|(..\/))(\w+:{0,1}\w*@)?(\S+)(:[0-9]+)?(\/|\/([\w#!:.?+=&%@!\-\/]))?)");
const std::regex proxy(R"((\r\n|\n)(Proxy-Connection[A-z: .-]+))");

std::string send_requst(std::string url, std::string& data) {
    auto protocol = getProtocol(url);
    auto hostname = getHostname(url);
    auto port = getPort(hostname);
    std::cout << port;
    data = std::regex_replace(data, url_r, url, std::regex_constants::format_first_only);
    data = std::regex_replace(data, proxy, "", std::regex_constants::format_first_only);

    BaseSocket to;
    socket_addr_in adr;
    if (hostname_to_ip(hostname.c_str(), &adr) == -1) {
        return "";
    }
    url = std::to_string(int(reinterpret_cast<char*>(&adr.sin_addr.S_un.S_addr)[0])) + '.' +
                 std::to_string(int(reinterpret_cast<char*>(&adr.sin_addr.S_un.S_addr)[1])) + '.' +
                 std::to_string(int(reinterpret_cast<char*>(&adr.sin_addr.S_un.S_addr)[2])) + '.' +
                 std::to_string(int(reinterpret_cast<char*>(&adr.sin_addr.S_un.S_addr)[3])) + ':';
    if (to.init((uint32_t)adr.sin_addr.S_un.S_addr, port,
                (uint8_t) SocketType::nonblocking_socket | (uint8_t) SocketType::client_socket) != SocketStatus::connected ) {
        return  "Can't connect to host";
    }

    to.send_to(data.data(), data.size());

    tcp_data_t buffer(1024);
    bool status = false;
    while (!status)  {
       status = to.recv_from(buffer.data(), 1024);
    }

    std::string res;
    while (status) {
        data.insert(
                res.end(),
                std::make_move_iterator(buffer.begin()),
                std::make_move_iterator(buffer.end())
        );
        status = to.recv_from(buffer.data(), 1024);
    }

    auto end = data.find('\0');
    data = data.substr(0, end);

    return data;
}

std::string sendRequest(std::string& data) {
    auto end = data.find('\0');
    data = data.substr(0, end);


    std::smatch m;
    if (regex_search(data, m, url_r)) {
        send_requst(m[0].str(), data);
    }
    return {};
}

int main() {
    using namespace std::chrono_literals;
    TcpServer server(8081,
                     {1, 1, 1}, // Keep alive{idle:1s, interval: 1s, pk_count: 1}

                     [](uniq_ptr<TcpServer::Client>& client){ // Data handler
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
                         std::cout << "Client "<< getHostStr(client) <<" send data [ " << data.size() << " bytes ]: " << (char*)data.data() << '\n';
                         client->send_to(res.data(), res.size());
                         client->disconnect();
                     },

                     [](uniq_ptr<TcpServer::Client>& client) { // Connect handler
                         std::cout << "Client " << getHostStr(client) << " connected\n";
                     },

                     [](uniq_ptr<TcpServer::Client>& client) { // Disconnect handler
                         std::cout << "Client " << getHostStr(client) << " disconnected\n";
                     },

                     std::thread::hardware_concurrency() // Thread pool size
    );

    try {
        //Start server
        if(server.start() == TcpServer::ServerStatus::up) {
            std::cout<<"Server listen on port: " << server.get_port() << std::endl
                     <<"Server handling thread pool size: " << server.get_thread_pool().getThreadCount() << std::endl;
            server.joinLoop();
            return EXIT_SUCCESS;
        } else {
            std::cout<<"Server start error! Error code:"<< int(server.get_status()) <<std::endl;
            return EXIT_FAILURE;
        }

    } catch(std::exception& except) {
        std::cerr << except.what();
        return EXIT_FAILURE;
    }
}