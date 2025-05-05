#ifndef INETADDRESS_H
#define INETADDRESS_H

#include <netinet/in.h>
#include <string>
class InetAddress
{
public:
    explicit InetAddress(uint16_t port = 0, std::string ip = "0.0.0.0");

    explicit InetAddress(const struct sockaddr_in &addr);

    sa_family_t family() const;

    std::string to_ip() const;

    std::string to_ip_port() const;

    uint16_t port() const;

    static bool get_host_by_name(const std::string &hostname, struct sockaddr_in *addr);

    const sockaddr_in *get_sock_addr() const { return &addr_; }
    void set_sock_addr(const sockaddr_in &addr) { addr_ = addr; }

private:
    struct sockaddr_in addr_;
};
#endif
