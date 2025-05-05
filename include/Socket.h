#ifndef MUDUO_NET_SOCKET_H
#define MUDUO_NET_SOCKET_H

#include "noncopyable.h"

class InetAddress;

class Socket : public noncopyable
{
public:
    explicit Socket(int sockfd) : sockfd_(sockfd) {}
    ~Socket();

    int fd() const { return sockfd_; }

    void bind_address(const InetAddress &localaddr);
    void listen();
    int accept(InetAddress &peeraddr);
    void shutdown_write();
    void set_tcp_no_delay(bool on);
    void set_reuse_addr(bool on);
    void set_reuse_port(bool on);
    void set_keep_alive(bool on);

private:
    const int sockfd_;
};

#endif