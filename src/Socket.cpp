#include "Socket.h"
#include "SocketsOps.h"
#include "logger.h"
#include "InetAddress.h"

#include <sys/types.h> /* See NOTES */
#include <sys/socket.h>
#include <netinet/tcp.h>
/**
 * @file Socket.cpp
 * @brief 套接字操作封装类实现
 */

/**
 * @class Socket
 * @brief 封装TCP套接字生命周期及常用操作
 * @note 功能特性：
 *       - RAII方式管理套接字描述符
 *       - 封装bind/listen/accept等系统调用
 *       - 提供常用套接字选项设置接口
 * @warning 所有方法非线程安全，需在相同IO线程调用
 */

/**
 * @brief 析构函数，关闭套接字描述符
 * @note 自动释放套接字资源，确保无描述符泄漏
 */
Socket::~Socket()
{
    ::close(sockfd_);
}

/**
 * @brief 绑定本地地址到套接字
 * @param localaddr 要绑定的本地地址对象
 * @note 实际调用::bind系统调用
 * @warning 必须在listen/connect前调用
 */
void Socket::bind_address(const InetAddress &localaddr)
{
    sockets::bind(sockfd_, (sockaddr *)localaddr.get_sock_addr());
}

/**
 * @brief 启动套接字监听
 * @note 设置套接字为被动监听模式
 * @warning 需先成功调用bind_address
 */
void Socket::listen()
{
    sockets::listen(sockfd_);
}

/**
 * @brief 接受新连接
 * @param[out] peeraddr 输出参数，接收对端地址信息
 * @return 新连接的文件描述符（>0表示成功）
 * @note 典型用法：
 *       while((connfd = accept(peeraddr)) > 0) {
 *           // 处理新连接
 *       }
 * @warning 返回的connfd需由调用者管理生命周期
 */
int Socket::accept(InetAddress &peeraddr)
{
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(sockaddr_in));
    int connfd = sockets::accept(sockfd_, (sockaddr *)&addr);
    if (connfd > 0)
    {
        peeraddr.set_sock_addr(addr);
    }
    return connfd;
}

/**
 * @brief 关闭写端（SHUT_WR）
 * @note 发送FIN报文，触发四次挥手过程
 * @warning 半关闭状态，仍可接收数据
 */
void Socket::shutdown_write()
{
    ::shutdown(sockfd_, SHUT_WR);
}

/**
 * @brief 设置TCP_NODELAY选项（禁用Nagle算法）
 * @param on 开关标志
 * @note 适用于需要低延迟的场景（如游戏服务器）
 * @warning 可能增加小包网络开销
 */
void Socket::set_tcp_no_delay(bool on)
{
    int optval = on ? 1 : 0;
    ::setsockopt(sockfd_, IPPROTO_TCP, TCP_NODELAY, &optval, sizeof(optval));
}

/**
 * @brief 设置SO_REUSEADDR选项（地址重用）
 * @param on 开关标志
 * @note 允许立即重用处于TIME_WAIT状态的地址
 * @warning 服务器重启时建议启用
 */
void Socket::set_reuse_addr(bool on)
{
    int optval = on ? 1 : 0;
    ::setsockopt(sockfd_, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
}

/**
 * @brief 设置SO_REUSEPORT选项（端口重用）
 * @param on 开关标志
 * @note 允许多个套接字绑定相同端口（Linux 3.9+）
 * @warning 需要内核支持，用于多进程负载均衡
 */
void Socket::set_reuse_port(bool on)
{
    int optval = on ? 1 : 0;
    ::setsockopt(sockfd_, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval));
}

/**
 * @brief 设置SO_KEEPALIVE选项（保活机制）
 * @param on 开关标志
 * @note 自动检测死连接，默认间隔较长（需结合TCP_KEEPIDLE等参数）
 * @warning 实际保活策略因系统而异
 */
void Socket::set_keep_alive(bool on)
{
    int optval = on ? 1 : 0;
    ::setsockopt(sockfd_, SOL_SOCKET, SO_KEEPALIVE, &optval, sizeof(optval));
}