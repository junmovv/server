#include "Acceptor.h"
#include "SocketsOps.h"
#include "InetAddress.h"
#include "logger.h"
/**
 * @file Acceptor.cpp
 * @brief 网络连接接收器实现，用于监听和接受新连接
 */

/**
 * @class Acceptor
 * @brief 封装TCP监听套接字操作，实现新连接接收处理
 * @note 功能特性：
 *       - 基于EventLoop的事件驱动模型
 *       - 支持地址/端口重用选项
 *       - 异步接受新连接并通过回调通知
 * @warning 必须在EventLoop所属线程构造和销毁
 */

/**
 * @brief 构造接收器并初始化监听套接字
 * @param loop 所属事件循环
 * @param listenAddr 监听地址信息
 * @param reuseport 是否启用SO_REUSEPORT
 * @note 关键初始化步骤：
 *       1. 创建非阻塞监听套接字
 *       2. 设置地址重用选项
 *       3. 绑定监听地址
 *       4. 注册可读事件回调
 */
Acceptor::Acceptor(EventLoop *loop, const InetAddress &listenAddr, bool reuseport)
    : loop_(loop),
      acceptSocket_(sockets::create_non_blocking(listenAddr.family())),
      acceptChannel_(loop, acceptSocket_.fd()),
      listenning_(false)
{
    acceptSocket_.set_reuse_addr(true);
    acceptSocket_.set_reuse_port(reuseport);
    acceptSocket_.bind_address(listenAddr);
    acceptChannel_.set_read_callback(std::bind(&Acceptor::handle_read, this));
}

/**
 * @brief 析构函数，清理资源
 * @note 安全关闭监听套接字：
 *       1. 取消所有事件监控
 *       2. 从Poller移除channel
 */
Acceptor::~Acceptor()
{
    acceptChannel_.disable_all();
    acceptChannel_.remove();
}

/**
 * @brief 启动监听并注册可读事件
 * @warning 必须在EventLoop线程调用
 */
void Acceptor::listen()
{
    listenning_ = true;
    acceptSocket_.listen();          // 开启内核监听
    acceptChannel_.enable_reading(); // 注册到Poller监控可读事件
}

/**
 * @brief 处理新连接到达事件
 * @note 操作流程：
 *       1. 接受新连接获取对端地址
 *       2. 存在回调时转交连接套接字
 *       3. 无回调时立即关闭连接
 *       4. 特殊处理文件描述符耗尽情况
 */
void Acceptor::handle_read()
{
    InetAddress peerAddr;
    int connfd = acceptSocket_.accept(peerAddr);
    if (connfd >= 0)
    {
        if (NewConnectionCallback_)  
        {
            NewConnectionCallback_(connfd, peerAddr); // 转交新连接给上层
        }
        else
        {
            ::close(connfd); // 安全关闭未处理的连接
        }
    }
    else
    {
        LogError("accept err:%d \n", errno);
        // 处理EMFILE（进程fd资源耗尽）
        if (errno == EMFILE)
        {
            LogError(" sockfd reached limit! \n");
            // 此处可添加拒绝连接策略
        }
    }
}