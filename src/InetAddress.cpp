#include "InetAddress.h"
#include "SocketsOps.h"
#include "logger.h"
#include <netdb.h>
#include <netinet/in.h>
#include <cstring>
/**
 * @file InetAddress.cpp
 * @brief IPv4网络地址封装实现
 */

/**
 * @class InetAddress
 * @brief IPv4网络地址操作工具类
 * @note 功能特性：
 *       - 支持点分十进制IP/端口构造
 *       - 支持sockaddr_in直接构造
 *       - 提供IP/端口字符串转换
 *       - 提供DNS域名解析能力
 * @warning 仅支持IPv4地址处理
 */

/**
 * @brief 通过IP字符串和端口构造地址对象
 * @param ip IPv4点分十进制字符串（如"127.0.0.1"）
 * @param port 16位主机字节序端口号
 * @note 内部转换使用网络字节序存储
 * @warning ip参数格式错误可能导致不可预知行为
 */
InetAddress::InetAddress(uint16_t port, std::string ip)
{
    memset(&addr_, 0, sizeof(addr_));
    sockets::from_ip_port(ip.c_str(), port, &addr_);
}

/**
 * @brief 通过sockaddr_in结构体构造地址对象
 * @param addr 已初始化的IPv4地址结构
 * @note 强制设置地址族为AF_INET
 */
InetAddress::InetAddress(const struct sockaddr_in &addr)
    : addr_(addr)
{
    addr_.sin_family = AF_INET;
}

/**
 * @brief 获取地址族类型
 * @return 固定返回AF_INET
 */
sa_family_t InetAddress::family() const
{
    return addr_.sin_family;
}

/**
 * @brief 获取IP字符串表示
 * @return 点分十进制IP字符串（如"192.168.1.1"）
 * @note 自动转换网络字节序到主机字节序
 */
std::string InetAddress::to_ip() const
{
    char buf[64] = {0};
    sockets::to_ip(buf, sizeof(buf), &addr_);
    return buf;
}

/**
 * @brief 获取IP:Port组合字符串
 * @return 格式化字符串（如"192.168.1.1:80"）
 */
std::string InetAddress::to_ip_port() const
{
    char buf[64] = {0};
    sockets::to_ip_port(buf, sizeof(buf), &addr_);
    return buf;
}

/**
 * @brief 获取端口号（主机字节序）
 * @return 16位无符号整数端口
 */
uint16_t InetAddress::port() const
{
    return ntohs(addr_.sin_port);
}

/**
 * @brief 解析主机名到IPv4地址
 * @param hostname 域名或主机名（如"www.example.com"）
 * @param[out] addr 输出参数，存储解析结果
 * @retval true 解析成功且找到IPv4地址
 * @retval false 解析失败或无有效地址
 * @note 实现特点：
 *       - 仅返回第一个找到的IPv4地址
 *       - 自动过滤非IPv4地址结果
 *       - 线程安全（使用getaddrinfo）
 * @warning 可能阻塞（依赖DNS服务器响应）
 */
bool InetAddress::get_host_by_name(const std::string &hostname, struct sockaddr_in *addr)
{
    struct addrinfo hints, *res = nullptr;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET; // 强制仅解析IPv4地址
    hints.ai_flags = AI_PASSIVE;
    hints.ai_socktype = SOCK_STREAM;

    int ret = getaddrinfo(hostname.c_str(), nullptr, &hints, &res);
    if (ret != 0)
    {
        LogError("Resolve %s failed: %s\n", hostname.c_str(), gai_strerror(ret));
        return false;
    }

    bool success = false;
    for (struct addrinfo *p = res; p != nullptr; p = p->ai_next)
    {
        if (p->ai_family == AF_INET && p->ai_addr != nullptr)
        {
            struct sockaddr_in *addr4 = reinterpret_cast<struct sockaddr_in *>(p->ai_addr);
            addr->sin_addr = addr4->sin_addr;
            success = true;
            break; // 取第一个有效IPv4地址
        }
    }

    freeaddrinfo(res);
    if (!success)
    {
        LogError("No valid IPv4 address found for %s\n", hostname.c_str());
    }
    return success;
}