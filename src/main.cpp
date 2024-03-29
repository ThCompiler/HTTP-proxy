#include "tcp_server_lib.hpp"
#include "proxy_client_lib.hpp"

#include <iostream>
#include <getopt.h>

using namespace bstcp;

//Parse ip to std::string
std::string getHostStr(const uniq_ptr<ISocket> &client) {
    uint32_t ip = client->get_host();
    return std::string() + std::to_string(int(reinterpret_cast<unsigned char *>(&ip)[0])) + '.' +
           std::to_string(int(reinterpret_cast<unsigned char *>(&ip)[1])) + '.' +
           std::to_string(int(reinterpret_cast<unsigned char *>(&ip)[2])) + '.' +
           std::to_string(int(reinterpret_cast<unsigned char *>(&ip)[3])) + ':' +
           std::to_string(client->get_port());
}



int main(int argc, char *argv[]) {
    int opt;
    proxy::SSLCert::init("certs", "certs/cert.key");

    int http_port = 8081;
    while ((opt = getopt(argc, argv, "p:")) != -1) {
        if (opt == 'p') {
            http_port = (int)strtol(optarg, nullptr, 10);
        }
    }

    proxy::ProxyClient::set_repository("host=localhost user=proxy password=pwd port=5432 dbname=proxy connect_timeout=10");

    try {
        TcpServer<proxy::TcpSocket, proxy::ProxyClient> server(http_port,
                         {1, 1, 1}, // Keep alive{idle:1s, interval: 1s, pk_count: 1}

                         [](uniq_ptr<ISocket> &client) { // Connect handler
                             std::cout << "Client " << getHostStr(client) << " connected\n";
                         },

                         [](uniq_ptr<ISocket> &client) { // Disconnect handler
                             std::cout << "Client " << getHostStr(client) << " disconnected\n";
                         },

                         std::thread::hardware_concurrency() // Thread pool size
        );

        //Start server
        if (server.start() == TcpServer<proxy::TcpSocket, proxy::ProxyClient>::ServerStatus::up) {
            std::cout << "Server listen on port: " << server.get_port() << std::endl
                      << "Server handling thread pool size: " << server.get_thread_pool().get_count_threads() << std::endl;
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

    proxy::SSLCert::clear_cert_dir();
}