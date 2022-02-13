#include "tcp_server.hpp"

#include <iostream>

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

int main() {
    using namespace std::chrono_literals;
    TcpServer server(8081,
                     {1, 1, 1}, // Keep alive{idle:1s, interval: 1s, pk_count: 1}

                     [](uniq_ptr<TcpServer::Client>& client){ // Data handler
                         tcp_data_t data(1024);
                         client->recv_from(data.data(), 1024);
                         std::cout << "Client "<< getHostStr(client) <<" send data [ " << data.size() << " bytes ]: " << (char*)data.data() << '\n';
                         client->send_to("Hello, client!\0", sizeof("Hello, client!\0"));
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