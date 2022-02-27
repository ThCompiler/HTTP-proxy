#pragma once

#include <openssl/ssl.h>
#include <openssl/err.h>

#include <string>

namespace proxy {
class SSLCert {
  public:
    static void init(std::string root_dir, std::string key_file);

    static bool is_inited();

    static void free_cert(SSL_CTX *_cert);

    static SSL_CTX *get_client_cert();

    static SSL_CTX *get_server_cert(const std::string &domain);

    static void init_ssl_lib();

    static bool clear_cert_dir();

  private:
    static bool file_cert(SSL_CTX *cert, const std::string &domain);

    static std::string  _root_dir;
    static std::string  _key_file;
    static bool         _is_init_lib;
    static bool         _is_init;
};
}