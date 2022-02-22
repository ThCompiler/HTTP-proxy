#include "include/ssl_cert.hpp"

#include <cstring>

SSL_CTX *proxy::SSLCert::_server_cert = nullptr;
SSL_CTX *proxy::SSLCert::_client_cert = nullptr;
bool proxy::SSLCert::_is_init = false;
bool proxy::SSLCert::_is_init_lib = false;

bool proxy::SSLCert::is_inited() {
    return _is_init;
}

int password_cb(char *buf, int size, int, void *password) {
    strncpy(buf, (char *)(password), size);
    buf[size - 1] = '\0';
    return (int)strlen(buf);
}

EVP_PKEY *generate_pk() {
    EVP_PKEY *pkey = nullptr;
    EVP_PKEY_CTX *pctx = EVP_PKEY_CTX_new_id(EVP_PKEY_RSA, nullptr);
    EVP_PKEY_keygen_init(pctx);
    EVP_PKEY_CTX_set_rsa_keygen_bits(pctx, 2048);
    EVP_PKEY_keygen(pctx, &pkey);
    return pkey;
}

X509 *generate_cert(EVP_PKEY *pkey) {
    X509 *x509 = X509_new();
    X509_set_version(x509, 2);
    ASN1_INTEGER_set(X509_get_serialNumber(x509), 0);
    X509_gmtime_adj(X509_get_notBefore(x509), 0);
    X509_gmtime_adj(X509_get_notAfter(x509), (long)60*60*24*365);
    X509_set_pubkey(x509, pkey);

    X509_NAME *name = X509_get_subject_name(x509);
    X509_NAME_add_entry_by_txt(name, "C", MBSTRING_ASC, (const unsigned char*)"RUS", -1, -1, 0);
    X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC, (const unsigned char*)"http-proxy", -1, -1, 0);
    X509_set_issuer_name(x509, name);
    X509_sign(x509, pkey, EVP_sha3_512());

    return x509;
}

bool proxy::SSLCert::base_init() {
    if (!_is_init_lib) {
        init_ssl_lib();
    }

    if (_is_init) {
        return true;
    }

    const SSL_METHOD *method_client = TLS_client_method();
    _client_cert = SSL_CTX_new(method_client);

    if (_client_cert == nullptr) {
        ERR_print_errors_fp(stderr);
        return false;
    }

    const SSL_METHOD *method_server = TLS_server_method();
    _server_cert = SSL_CTX_new(method_server);

    if (_server_cert == nullptr) {
        ERR_print_errors_fp(stderr);
        SSL_CTX_free(_client_cert);
        _client_cert = nullptr;
        return false;
    }

    EVP_PKEY *pkey = generate_pk();
    X509 *x509 = generate_cert(pkey);
    SSL_CTX_use_certificate(_server_cert, x509);
    SSL_CTX_set_default_passwd_cb(_server_cert, password_cb);
    SSL_CTX_use_PrivateKey(_server_cert, pkey);

    SSL_CTX_set_verify(_server_cert, SSL_VERIFY_NONE, nullptr);

    return _is_init = true;
}

SSL_CTX *proxy::SSLCert::get_client_cert() {
    return _client_cert;
}

SSL_CTX *proxy::SSLCert::get_server_cert() {
    return _server_cert;
}

bool proxy::SSLCert::file_init(std::string cert_file, std::string key_file) {
    if (!_is_init_lib) {
        init_ssl_lib();
    }

    if (_is_init) {
        return true;
    }

    const SSL_METHOD *method_client = TLS_client_method();
    _client_cert = SSL_CTX_new(method_client);

    if (_client_cert == nullptr) {
        ERR_print_errors_fp(stderr);
        return false;
    }

    const SSL_METHOD *method_server = TLS_server_method();
    _server_cert = SSL_CTX_new(method_server);

    if (_server_cert == nullptr) {
        ERR_print_errors_fp(stderr);
        SSL_CTX_free(_client_cert);
        _client_cert = nullptr;
        return false;
    }

    _is_init = true;
    if (SSL_CTX_load_verify_locations(_server_cert, cert_file.data(), key_file.data()) != 1) {
        ERR_print_errors_fp(stderr);
        free_cert();
        return false;
    }

    if (SSL_CTX_set_default_verify_paths(_server_cert) != 1) {
        ERR_print_errors_fp(stderr);
        free_cert();
        return false;
    }

    if ( SSL_CTX_use_certificate_file(_server_cert, cert_file.data(), SSL_FILETYPE_PEM) <= 0 ) {
        ERR_print_errors_fp(stderr);
        free_cert();
        return false;
    }

    if ( SSL_CTX_use_PrivateKey_file(_server_cert, key_file.data(), SSL_FILETYPE_PEM) <= 0 ) {
        ERR_print_errors_fp(stderr);
        free_cert();
        return false;
    }

    if ( !SSL_CTX_check_private_key(_server_cert) ) {
        fprintf(stderr, "Private key does not match the public certificate\n");
        free_cert();
        return false;
    }

    return true;
}

void proxy::SSLCert::free_cert() {
    if (is_inited()) {
        SSL_CTX_free(_client_cert);
        _client_cert = nullptr;
        SSL_CTX_free(_server_cert);
        _server_cert = nullptr;
        _is_init = false;
    }
}

void proxy::SSLCert::init_ssl_lib() {
    SSL_load_error_strings();
    ERR_load_crypto_strings();

    OpenSSL_add_all_algorithms();
    SSL_library_init();
    _is_init_lib = true;
}

