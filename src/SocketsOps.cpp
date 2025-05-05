#include "SocketsOps.h"
#include "logger.h"
#include <errno.h>
#include <fcntl.h>
#include <stdio.h> // snprintf
#include <sys/socket.h>
#include <sys/uio.h> // readv
#include <unistd.h>
/**
 * @file SocketsOps.cpp
 * @brief 套接字操作工具函数集合
 * @note 功能特性：
 *       - 封装底层socket系统调用
 *       - 统一错误处理机制
 *       - 支持IPv4地址转换操作
 *       - 非阻塞套接字创建
 * @warning 当前实现仅支持IPv4协议
 */

/**
 * @def ERROR_CHECK(ret, msg)
 * @brief 套接字操作错误检查宏
 * @param ret 系统调用返回值
 * @param msg 错误描述信息
 * @note 功能：
 *       1. 检查返回值是否小于0
 *       2. 记录错误日志（含errno信息）
 *       3. 统一返回-1表示失败
 * @warning 仅适用于返回整数错误码的系统调用
 */
#define ERROR_CHECK(ret, msg)                      \
    do                                             \
    {                                              \
        if (ret < 0)                               \
        {                                          \
            LogError("[%s], error %d, errmsg %s",  \
                     msg, errno, strerror(errno)); \
            return -1;                             \
        }                                          \
    } while (0)

/**
 * @brief 设置套接字非阻塞和close-on-exec标志
 * @param sockfd 要设置的套接字描述符
 * @note 操作步骤：
 *       1. 获取当前文件状态标志
 *       2. 添加O_NONBLOCK非阻塞标志
 *       3. 添加FD_CLOEXEC子进程关闭标志
 * @warning 需在创建连接后立即设置
 */
void set_non_block_and_close_on_exec(int sockfd)
{
    // 设置非阻塞模式
    int flags = ::fcntl(sockfd, F_GETFL, 0);
    flags |= O_NONBLOCK;
    int ret = ::fcntl(sockfd, F_SETFL, flags);

    // 设置exec时关闭
    flags = ::fcntl(sockfd, F_GETFD, 0);
    flags |= FD_CLOEXEC;
    ret = ::fcntl(sockfd, F_SETFD, flags);
    (void)ret; // 消除编译器警告
}

/**
 * @brief 创建非阻塞TCP套接字
 * @param family 协议族（AF_INET/AF_INET6）
 * @return 成功返回套接字描述符，失败返回-1
 * @note 使用SOCK_NONBLOCK|SOCK_CLOEXEC标志：
 *       - 避免额外的fcntl系统调用
 *       - 保证子进程不继承套接字
 */
int sockets::create_non_blocking(sa_family_t family)
{
    int socketFd = ::socket(family,
                            SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC,
                            IPPROTO_TCP);
    ERROR_CHECK(socketFd, "socket create");
    return socketFd;
}

/**
 * @brief 绑定套接字到指定地址
 * @param sockfd 套接字描述符
 * @param addr 要绑定的地址结构指针
 * @return 成功返回0，失败返回-1
 * @warning 地址结构必须与套接字协议族匹配
 */
int sockets::bind(int sockfd, const struct sockaddr *addr)
{
    int ret = ::bind(sockfd, addr,
                     static_cast<socklen_t>(sizeof(struct sockaddr_in)));
    ERROR_CHECK(ret, "bind");
    return 0;
}

/**
 * @brief 启动套接字监听
 * @param sockfd 已绑定的套接字描述符
 * @return 成功返回0，失败返回-1
 * @note 使用SOMAXCONN作为最大等待连接数
 *       实际值取决于系统配置（通常128+）
 */
int sockets::listen(int sockfd)
{
    int ret = ::listen(sockfd, SOMAXCONN);
    ERROR_CHECK(ret, "listen");
    return 0;
}

/**
 * @brief 接受新连接
 * @param sockfd 监听套接字描述符
 * @param addr 输出参数，接收对端地址
 * @return 成功返回新连接描述符，失败返回-1
 * @note 自动为新连接设置非阻塞和CLOEXEC标志
 */
int sockets::accept(int sockfd, struct sockaddr *addr)
{
    socklen_t addrlen = static_cast<socklen_t>(sizeof(*addr));
    int connfd = ::accept(sockfd, addr, &addrlen);
    ERROR_CHECK(connfd, "accept");
    set_non_block_and_close_on_exec(connfd);
    return connfd;
}

/**
 * @brief 关闭套接字
 * @param sockfd 要关闭的描述符
 * @return 成功返回0，失败返回-1
 * @warning 重复关闭已关闭的套接字会导致未定义行为
 */
int sockets::close(int sockfd)
{
    int ret = ::close(sockfd);
    ERROR_CHECK(ret, "close");
    return 0;
}

/**
 * @brief 转换地址结构为IP字符串
 * @param[out] buf 输出缓冲区
 * @param size 缓冲区大小（至少INET_ADDRSTRLEN=16）
 * @param addr 要转换的地址结构
 * @note 仅支持IPv4地址转换
 */
void sockets::to_ip(char *buf, size_t size, const struct sockaddr_in *addr)
{
    const struct sockaddr *addr4 = (const struct sockaddr *)addr;
    if (addr4->sa_family == AF_INET)
    {
        ::inet_ntop(AF_INET, &addr->sin_addr,
                    buf, static_cast<socklen_t>(size));
    }
    else
    {
        LogError("IPv6 not supported\n");
    }
}

/**
 * @brief 转换地址结构为IP:PORT格式
 * @param[out] buf 输出缓冲区
 * @param size 缓冲区大小（至少IP:PORT_MAX=21）
 * @param addr 要转换的地址结构
 * @note 格式示例："192.168.1.1:8080"
 */
void sockets::to_ip_port(char *buf, size_t size, const struct sockaddr_in *addr)
{
    const struct sockaddr *addr4 = (const struct sockaddr *)addr;
    if (addr4->sa_family == AF_INET)
    {
        to_ip(buf, size, addr);
        size_t end = strlen(buf);
        uint16_t port = ntohs(addr->sin_port);
        snprintf(buf + end, size - end, ":%u", port);
    }
    else
    {
        LogError("IPv6 not supported\n");
    }
}

/**
 * @brief 从字符串解析IP和端口到地址结构
 * @param ip 点分十进制IP字符串
 * @param port 主机字节序端口号
 * @param[out] addr 输出参数，存储解析结果
 * @warning 无效IP会导致未初始化地址结构
 */
void sockets::from_ip_port(const char *ip, uint16_t port, struct sockaddr_in *addr)
{
    addr->sin_family = AF_INET;
    addr->sin_port = htons(port);
    if (::inet_pton(AF_INET, ip, &addr->sin_addr) <= 0)
    {
        LogError("Invalid IPv4 address: %s\n", ip);
    }
}

/**
 * @brief 获取套接字本地地址信息
 * @param sockfd 已连接的套接字描述符
 * @return sockaddr_in结构体包含本地地址
 * @note 失败时返回全零地址结构
 */
struct sockaddr_in sockets::get_local_addr(int sockfd)
{
    struct sockaddr_in localaddr;
    socklen_t addrlen = sizeof(localaddr);
    memset(&localaddr, 0, sizeof(localaddr));
    if (::getsockname(sockfd, (sockaddr *)&localaddr, &addrlen) < 0)
    {
        LogError("getsockname failed\n");
    }
    return localaddr;
}

/**
 * @brief 获取套接字对端地址信息
 * @param sockfd 已连接的套接字描述符
 * @return sockaddr_in结构体包含对端地址
 * @note 失败时返回全零地址结构
 */
struct sockaddr_in sockets::get_peer_addr(int sockfd)
{
    struct sockaddr_in peeraddr;
    socklen_t addrlen = sizeof(peeraddr);
    memset(&peeraddr, 0, sizeof(peeraddr));
    if (::getpeername(sockfd, (sockaddr *)&peeraddr, &addrlen) < 0)
    {
        LogError("getpeername failed\n");
    }
    return peeraddr;
}