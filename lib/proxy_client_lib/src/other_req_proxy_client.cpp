#include "proxy_client.hpp"
#include "tls_socket.hpp"

#include <exception>
#include <iostream>

const char* get_list_requests = "/list";
const char* repeat_requests = "/repeat";
const char* search_vulnerability_requests = "/search";

const char* injection_1 = ";cat /etc/passwd;";
const char* injection_2 = "|cat /etc/passwd|";
const char* injection_3 = "`cat /etc/passwd`";
const char* syndrom_injection = "root:";

const char* limit_param = "limit";
const char* id_param = "id";

const size_t client_chank_size = 1024;
const size_t server_chank_size = 20000;

namespace proxy {
    std::string ProxyClient::_get_list(http::Request& req) {
        auto param = req.get_param(limit_param);
        size_t limit = 0;
        if (param.empty()) {
            limit = 10;
        } else {
            limit = strtol(param.data(), nullptr, 10);
            if (limit <= 0) {
                return "HTTP/1.1 400 Bad request  \n Not allow limit \n\n";
            }
        }
        std::vector <rp::request_t> list;

        try {
            list = _rep->get_list(limit);
        } catch(std::exception& ex) {
            std::cerr << ex.what() << "\n";
            return "HTTP/1.1 500 Server error  \n BD error \n\n";
        }

        std::string res = "HTTP/1.1 200 OK\n\n";
        for (auto& item : list) {
            res += "id = " + std::to_string(item.id) + "\n Request: \n";
            res += item.request.string();
            res += "\n";
        }
        return res;
    }

    std::string ProxyClient::_resend_request(rp::request_t& req) {
        TcpSocket to;
        auto res = _init_client_socket(req.host, req.port, to);
        if (!res.empty()) {
            return res;
        }

        if (req.is_https) {
            SSLSocket ssl_socket;
            if (ssl_socket.init(std::move(to)) != bstcp::status::connected) {
                return "HTTP/1.1 525 SSL Handshake Failed \n Can't connect to server by tls \n\n";
            }
            _send_to_socket(ssl_socket, req.request.string(), client_chank_size);
            auto answ = _read_from_socket(ssl_socket, server_chank_size);
            return answ;
        }

        _send_to_socket(to, req.request.string(), client_chank_size);

        if (!to.is_allow_to_read(2000)) {
            return "HTTP/1.1 408 Request Timeout  \n 2s time out \n\n";
        }

        auto answ = _read_from_socket(to, server_chank_size);
        return answ;
    }


    std::string ProxyClient::_repeat_request(http::Request& req) {
        auto param = req.get_param(id_param);
        size_t id = 0;
        if (param.empty()) {
            return "HTTP/1.1 400 Bad request  \n Not found id \n\n";
        } else {
            id = strtol(param.data(), nullptr, 10);
            if (id <= 0) {
                return "HTTP/1.1 400 Bad request  \n Not allow id \n\n";
            }
        }

        rp::request_t res;
        try {
            res = _rep->get_by_id(id);
        } catch(std::exception& ex) {
            std::cerr << ex.what() << "\n";
            return "HTTP/1.1 500 Server error  \n BD error \n\n";
        }
        return _resend_request(res);
    }

    std::string ProxyClient::_search_vulnerability(http::Request& req) {
        auto param = req.get_param(id_param);
        size_t id = 0;
        if (param.empty()) {
            return "HTTP/1.1 400 Bad request  \n Not found id \n\n";
        } else {
            id = strtol(param.data(), nullptr, 10);
            if (id <= 0) {
                return "HTTP/1.1 400 Bad request  \n Not allow id \n\n";
            }
        }

        rp::request_t save_req;
        try {
            save_req = _rep->get_by_id(id);
        } catch(std::exception& ex) {
            std::cerr << ex.what() << "\n";
            return "HTTP/1.1 500 Server error  \n BD error \n\n";
        }

        auto tmp = save_req;

        std::string answ = "HTTP/1.1 200 OK \n\n";

        auto params = tmp.request.get_params();
        for (auto [key, value] : params) {
            tmp.request.set_param(key, value + injection_1);
            auto res = _resend_request(tmp);
            if (res.find(syndrom_injection) != std::string::npos) {
                answ += "Found vulnerability with param " + key + " with injection " +injection_1;
                answ += "\n" + res;
            } else {
                answ += "Not found vulnerability with param " + key + " and with injection " + injection_1;
                answ += "\n";
            }

            tmp.request.set_param(key, value + injection_2);
            res = _resend_request(tmp);
            if (res.find(syndrom_injection) != std::string::npos) {
                answ += "Found vulnerability with param " + key + " with injection " + injection_2;
                answ += "\n" + res;
            } else {
                answ += "Not found vulnerability with param " + key + " and with injection " + injection_2;
                answ += "\n";
            }

            tmp.request.set_param(key, value + injection_3);
            res = _resend_request(tmp);
            if (res.find(syndrom_injection) != std::string::npos) {
                answ += "Found vulnerability with param " + key + " with injection " + injection_3;
                answ += "\n" + res;
            } else {
                answ += "Not found vulnerability with param " + key + " and with injection " + injection_3;
                answ += "\n";
            }
            tmp = save_req;
        }

        auto headers = tmp.request.get_headers();
        for (auto [key, value] : headers) {
            tmp.request.set_header(key, value + injection_1);
            auto res = _resend_request(tmp);
            if (res.find(syndrom_injection) != std::string::npos) {
                answ += "Found vulnerability with header " + key + " with injection " +injection_1;
                answ += "\n" + res;
            } else {
                answ += "Not found vulnerability with header " + key + " and with injection " + injection_1;
                answ += "\n";
            }

            tmp.request.set_header(key, value + injection_2);
            res = _resend_request(tmp);
            if (res.find(syndrom_injection) != std::string::npos) {
                answ += "Found vulnerability with header " + key + " with injection " + injection_2;
                answ += "\n" + res;
            } else {
                answ += "Not found vulnerability with header " + key + " and with injection " + injection_2;
                answ += "\n";
            }

            tmp.request.set_header(key, value + injection_3);
            res = _resend_request(tmp);
            if (res.find(syndrom_injection) != std::string::npos) {
                answ += "Found vulnerability with header " + key + " with injection " + injection_3;
                answ += "\n" + res;
            } else {
                answ += "Not found vulnerability with header " + key + " and with injection " + injection_3;
                answ += "\n";
            }
            tmp = save_req;
        }

        auto cookies = tmp.request.get_cookies();
        for (auto [key, value] : cookies) {
            tmp.request.set_cookie(key, value + injection_1);
            auto res = _resend_request(tmp);
            if (res.find(syndrom_injection) != std::string::npos) {
                answ += "Found vulnerability with cookie " + key + " with injection " +injection_1;
                answ += "\n" + res;
            } else {
                answ += "Not found vulnerability with cookie " + key + " and with injection " + injection_1;
                answ += "\n";
            }

            tmp.request.set_cookie(key, value + injection_2);
            res = _resend_request(tmp);
            if (res.find(syndrom_injection) != std::string::npos) {
                answ += "Found vulnerability with cookie " + key + " with injection " + injection_2;
                answ += "\n" + res;
            } else {
                answ += "Not found vulnerability with cookie " + key + " and with injection " + injection_2;
                answ += "\n";
            }

            tmp.request.set_cookie(key, value + injection_3);
            res = _resend_request(tmp);
            if (res.find(syndrom_injection) != std::string::npos) {
                answ += "Found vulnerability with cookie " + key + " with injection " + injection_3;
                answ += "\n" + res;
            } else {
                answ += "Not found vulnerability with cookie " + key + " and with injection " + injection_3;
                answ += "\n";
            }
            tmp = save_req;
        }

        return answ;
    }

    std::string ProxyClient::_parse_not_proxy_request(http::Request& req) {
        auto url = req.get_url();
        if (url == get_list_requests) {
            return _get_list(req);
        }
        if (url == repeat_requests) {
            return _repeat_request(req);
        }
        if (url == search_vulnerability_requests) {
            return _search_vulnerability(req);
        }
        return "HTTP/1.1 400 Bad request  \n Not allow this path \n\n";
    }
}