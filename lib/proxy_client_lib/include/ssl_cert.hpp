#pragma once

#include <openssl/ssl.h>
#include <openssl/err.h>

#include <string>

namespace proxy {
    class SSLCert {
    public:
        static bool file_init(std::string cert_file, std::string key_file);
        static bool is_inited();
        static bool base_init();
        static void free_cert();

        static SSL_CTX* get_client_cert();

        static SSL_CTX* get_server_cert();

        static void init_ssl_lib();

    private:
        static bool     _is_init_lib;
        static bool     _is_init;
        static SSL_CTX* _client_cert;
        static SSL_CTX* _server_cert;
    };
}