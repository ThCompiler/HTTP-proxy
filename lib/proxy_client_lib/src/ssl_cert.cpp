#include "include/ssl_cert.hpp"

#include <cstring>
#include <utility>
#include <filesystem>

namespace fs = std::filesystem;

bool proxy::SSLCert::_is_init = false;
bool proxy::SSLCert::_is_init_lib = false;
std::string proxy::SSLCert::_key_file;
std::string proxy::SSLCert::_root_dir;

bool proxy::SSLCert::is_inited() {
    return _is_init;
}

SSL_CTX *proxy::SSLCert::get_client_cert() {
    if (!_is_init) {
        return nullptr;
    }

    SSL_CTX *client_cert = SSL_CTX_new(TLS_client_method());

    if (client_cert == nullptr) {
        ERR_print_errors_fp(stderr);
        ERR_clear_error();
        return nullptr;
    }
    return client_cert;
}

SSL_CTX *proxy::SSLCert::get_server_cert(const std::string &domain) {
    if (!_is_init) {
        return nullptr;
    }

    const SSL_METHOD *method_server = TLS_server_method();
    SSL_CTX *server_cert = SSL_CTX_new(method_server);

    if (server_cert == nullptr) {
        ERR_print_errors_fp(stderr);
        ERR_clear_error();
        return nullptr;
    }

    file_cert(server_cert, domain);

    return server_cert;
}

std::string
generate_cert(const std::string &domain, const std::string &certs_dir) {
    std::string result = certs_dir + "/" + domain + ".crt";
    bool found = false;
    for (const auto &entry: fs::directory_iterator(certs_dir)) {
        if (entry.path().filename() == domain + ".crt") {
            found = true;
            break;
        }
    }

    if (found) {
        return result;
    }

    if (std::system(std::string(
            "cd " + certs_dir + " && sh gen_cert.sh " + domain).c_str()) != 0) {
        return "";
    }
    return result;
}

bool proxy::SSLCert::file_cert(SSL_CTX *cert, const std::string &domain) {
    auto cert_file = generate_cert(domain, _root_dir);

    _is_init = true;
    if (SSL_CTX_load_verify_locations(cert, cert_file.data(),
                                      _key_file.data()) != 1) {
        ERR_print_errors_fp(stderr);
        ERR_clear_error();
        return false;
    }

    if (SSL_CTX_set_default_verify_paths(cert) != 1) {
        ERR_print_errors_fp(stderr);
        ERR_clear_error();
        return false;
    }

    if (SSL_CTX_use_certificate_file(cert, cert_file.data(),
                                     SSL_FILETYPE_PEM) <= 0) {
        ERR_print_errors_fp(stderr);
        ERR_clear_error();
        return false;
    }

    if (SSL_CTX_use_PrivateKey_file(cert, _key_file.data(), SSL_FILETYPE_PEM) <=
        0) {
        ERR_print_errors_fp(stderr);
        ERR_clear_error();
        return false;
    }

    if (!SSL_CTX_check_private_key(cert)) {
        fprintf(stderr, "Private key does not match the public certificate\n");
        return false;
    }

    return true;
}

void proxy::SSLCert::free_cert(SSL_CTX *_cert) {
    SSL_CTX_free(_cert);
}

void proxy::SSLCert::init_ssl_lib() {
    SSL_load_error_strings();
    ERR_load_crypto_strings();

    OpenSSL_add_all_algorithms();
    SSL_library_init();
    _is_init_lib = true;
}

void proxy::SSLCert::init(std::string root_dir, std::string key_file) {
    if (!_is_init_lib) {
        init_ssl_lib();
    }

    if (!fs::is_directory(fs::path(root_dir))
        || !fs::exists(fs::path(key_file))) {
        return;
    }

    proxy::SSLCert::_root_dir = std::move(root_dir);
    proxy::SSLCert::_key_file = std::move(key_file);
    _is_init = true;
}

bool proxy::SSLCert::clear_cert_dir() {
    if (!_is_init) {
        return false;
    }
    return std::system(std::string(
            "cd " + _root_dir + " && rm -rf *.crt").c_str()) != 0;
}

