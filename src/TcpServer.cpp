/** @file TcpServer.cpp */
/** @brief TCP服务器核心实现文件 */

#include "TcpServer.h"
#include "logger.h"
#include "Acceptor.h"
#include "EventLoop.h"
#include "EventLoopThreadPool.h"

/**
 • @brief 检查事件循环有效性
 • @param loop 待检查的事件循环指针
 • @return 原始事件循环指针
 • @note 空指针会记录错误日志但不中断程序，需调用者保证有效性
 */
static EventLoop *check_loop_not_null(EventLoop *loop)
{
    if (loop == nullptr)
    {
        LogError("mainloop is null\n");
    }
    return loop;
}

/**
 • @brief TCP服务器构造函数
 • @param loop 主事件循环
 • @param listenAddr 监听地址
 • @param nameArg 服务器名称
 • @param option 端口复用选项（kReusePort）
 • @note 初始化接受器并设置新连接回调，支持SO_REUSEPORT选项
 */
TcpServer::TcpServer(EventLoop *loop,
                     const InetAddress &listenAddr,
                     const std::string &serverName,
                     Option option)
    : loop_(check_loop_not_null(loop)),
      ipPort_(listenAddr.to_ip_port()),
      serverName_(serverName),
      acceptor_(new Acceptor(loop, listenAddr, option == kReusePort)),
      threadPool_(new EventLoopThreadPool(loop, serverName_)),
      nextConnId_(1)
{
    // 设置新连接到达时的处理回调
    acceptor_->set_new_connection_callback(
        std::bind(&TcpServer::new_connection, this,
                  std::placeholders::_1,
                  std::placeholders::_2));
}

/**
 • @brief TCP服务器析构函数
 • @note 安全关闭所有连接，跨线程清理连接资源
 */
TcpServer::~TcpServer()
{
    LogInfo("~TcpServer name[%s]\n", serverName_.c_str());
    for (auto &item : connections_)
    {
        TcpConnectionPtr conn(item.second);
        item.second.reset(); // 释放当前线程持有的智能指针
        // 在连接所属的IO线程执行销毁操作
        conn->get_loop()->run_in_loop(
            std::bind(&TcpConnection::connect_destroyed, conn));
    }
}

/**
 • @brief 设置IO线程池线程数量
 • @param numThreads 线程数量
 • @note 需在start()调用前设置生效
 */
void TcpServer::set_thread_num(int numThreads)
{
    threadPool_->set_thread_num(numThreads);
}

/**
 • @brief 启动服务器
 • @note 保证线程安全初始化，启动线程池并开始监听端口
 */
void TcpServer::start()
{
    // 使用call_once保证只初始化一次
    std::call_once(started_, [this]
                   {
        threadPool_->start(threadInitCallback_);  // 启动IO线程池
        loop_->run_in_loop(  // 在主循环中启动监听
            std::bind(&Acceptor::listen, acceptor_.get())); });
}

/**
 • @brief 处理新连接建立
 • @param sockfd 新连接套接字
 • @param peerAddr 对端地址
 • @note 创建TcpConnection对象并注册到IO线程
 */
void TcpServer::new_connection(int connfd, const InetAddress &peerAddr)
{
    // 轮询获取IO线程
    EventLoop *ioLoop = threadPool_->get_next_loop();

    // 生成唯一连接名称
    std::string connName = serverName_ + "-" + ipPort_ + "#" + std::to_string(++nextConnId_);
    LogInfo("TcpServer::newConnection [%s] - new connection [%s] from %s \n",
            serverName_.c_str(), connName.c_str(), peerAddr.to_ip_port().c_str());

    // 获取本地地址信息
    sockaddr_in local;
    memset(&local, 0, sizeof(local));
    socklen_t addrlen = sizeof(local);
    if (::getsockname(connfd, (sockaddr *)&local, &addrlen) < 0)
    {
        LogError("sockets::getLocalAddr");
    }
    InetAddress localAddr(local);

    // 创建连接对象并注册到连接表
    TcpConnectionPtr conn(new TcpConnection(ioLoop, connName, connfd, localAddr, peerAddr));
    connections_[connName] = conn;

    // 设置回调函数链
    conn->set_connection_callback(connectionCallback_);
    conn->set_message_callback(messageCallback_);
    conn->set_write_complete_callback(writeCompleteCallback_);

    // 设置关闭连接回调（跨线程安全）
    conn->set_close_callback(
        std::bind(&TcpServer::remove_connection, this, std::placeholders::_1));

    // 在IO线程中完成连接建立
    ioLoop->run_in_loop(
        std::bind(&TcpConnection::connect_Established, conn));
}

/**
 • @brief 移除连接（跨线程安全）
 • @param conn 要移除的连接指针
 */
void TcpServer::remove_connection(const TcpConnectionPtr &conn)
{
    loop_->run_in_loop(
        std::bind(&TcpServer::remove_connection_in_loop, this, conn));
}

/**
 • @brief 在事件循环线程中实际执行移除操作
 • @param conn 要移除的连接指针
 */
void TcpServer::remove_connection_in_loop(const TcpConnectionPtr &conn)
{
    LogInfo("TcpServer::removeConnectionInLoop [%s] - connection %s\n",
            serverName_.c_str(), conn->name().c_str());

    // 从连接表中删除
    connections_.erase(conn->name());

    // 在连接所属IO线程中执行销毁
    EventLoop *ioLoop = conn->get_loop();
    ioLoop->queue_in_loop(
        std::bind(&TcpConnection::connect_destroyed, conn));
}
