#include "tcp_server_lib.hpp"

int bstcp::hostname_to_ip(const char *hostname, bstcp::socket_addr_in *addr) {
    struct addrinfo hints{}, *servinfo, *p;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if (getaddrinfo( hostname , "http" , &hints , &servinfo) != 0) {
        return -1;
    }
    p = servinfo;
    *addr = *(socket_addr_in *) p->ai_addr;

    freeaddrinfo(servinfo); // all done with this structure
    return 0;
}