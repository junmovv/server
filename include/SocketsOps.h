#ifndef SOCKETSOPS_H
#define SOCKETSOPS_H

#include <arpa/inet.h>

namespace   sockets
{

    int create_non_blocking(sa_family_t family);

    int connect(int sockfd, const struct sockaddr *addr);

    int bind(int sockfd, const struct sockaddr *addr);

    int listen(int sockfd);

    int accept(int sockfd, struct sockaddr *addr);

    int close(int sockfd);

    void to_ip(char *buf, size_t size, const struct sockaddr_in *addr);

    void to_ip_port(char *buf, size_t size, const struct sockaddr_in *addr);

    void from_ip_port(const char *ip, uint16_t port, struct sockaddr_in *addr);

    struct sockaddr_in get_local_addr(int sockfd);

    struct sockaddr_in get_peer_addr(int sockfd);

}

#endif
