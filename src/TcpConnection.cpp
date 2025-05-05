/** @file TcpConnection.cpp */
/** @brief TCP连接管理实现文件 */

#include "TcpConnection.h"
#include "Socket.h"
#include "logger.h"
#include "Channel.h"
#include "EventLoop.h"

/**
 • @brief 检查事件循环指针有效性
 • @param loop 事件循环指针
 • @return 有效的事件循环指针
 • @note 若传入空指针会记录错误日志，但依然返回原指针
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
 • @brief TCP连接构造函数
 • @param loop 所属事件循环
 • @param name 连接名称
 • @param sockfd 套接字描述符
 • @param localAddr 本地地址信息
 • @param peerAddr 对端地址信息
 • @note 初始化通道回调并设置TCP保持连接选项
 */
TcpConnection::TcpConnection(EventLoop *loop,
                             const std::string &name,
                             int sockfd,
                             const InetAddress &localAddr,
                             const InetAddress &peerAddr)
    : loop_(check_loop_not_null(loop)),
      name_(name),
      state_(kConnecting),
      reading_(true),
      socket_(new Socket(sockfd)),
      connChannel_(new Channel(loop, sockfd)),
      localAddr_(localAddr),
      peerAddr_(peerAddr),
      highWaterMark_(64 * 1024 * 1024) // 64MB高水位标记
{
    // 设置通道回调函数
    connChannel_->set_read_callback(std::bind(&TcpConnection::handle_read, this, std::placeholders::_1));
    connChannel_->set_write_callback(std::bind(&TcpConnection::handle_write, this));
    connChannel_->set_close_callback(std::bind(&TcpConnection::handle_close, this));
    connChannel_->set_error_callback(std::bind(&TcpConnection::handle_error, this));

    LogInfo("tcpConnection name[%s],fd[%d]\n", name_.c_str(), sockfd);
    socket_->set_keep_alive(true); // 启用TCP keepalive
}

/**
 • @brief TCP连接析构函数
 • @note 记录连接销毁日志，自动关闭关联的通道
 */
TcpConnection::~TcpConnection()
{
    LogInfo("tcpconnection dtor name [%s],fd [%d]\n", name_.c_str(), connChannel_->fd());
}

/**
 • @brief 发送字符串数据
 • @param buf 要发送的数据缓冲区
 • @note 线程安全，支持跨线程调用
 */
void TcpConnection::send(const std::string &buf)
{
    if (state_ == kConnected)
    {
        if (loop_->is_in_loop_thread()) // 在IO线程直接发送
        {
            send_in_loop(buf.data(), buf.size());
        }
        else // 跨线程时通过事件队列转发
        {
            loop_->run_in_loop(std::bind(&TcpConnection::send_in_loop, this, buf.c_str(), buf.size()));
        }
    }
}

/**
 • @brief 发送原始内存数据
 • @param message 数据指针
 • @param len 数据长度
 */
void TcpConnection::send(void *message, size_t len)
{
    send(std::string(static_cast<char *>(message), len));
}

/**
 • @brief 在事件循环中实际执行发送操作
 • @param data 数据指针
 • @param len 数据长度
 • @note 处理非阻塞写、缓冲区管理及高水位回调
 */
void TcpConnection::send_in_loop(const void *data, size_t len)
{
    ssize_t nwrote = 0;
    size_t remaining = len;
    bool faultError = false;

    if (state_ == kDisconnected)
    {
        LogError("disconneted,give up writing\n");
        return;
    }

    // 直接写通道且输出缓冲区为空时尝试立即发送
    if (!connChannel_->is_writing_event() && outputBuffer_.readable_bytes() == 0)
    {
        nwrote = ::write(connChannel_->fd(), data, len);
        if (nwrote >= 0)
        {
            remaining = len - nwrote;
            if (remaining == 0 && writeCompleteCallback_) // 数据全部发送完成
            {
                loop_->queue_in_loop(std::bind(writeCompleteCallback_, shared_from_this()));
            }
        }
        else // 处理写错误
        {
            nwrote = 0;
            if (errno != EWOULDBLOCK) // 非预期错误
            {
                LogError("TcpConnection::sendInLoop");
                if (errno == EPIPE || errno == ECONNRESET) // 连接重置类错误
                {
                    faultError = true;
                }
            }
        }
    }

    // 未发送完或发生非致命错误时缓存剩余数据
    if (!faultError && remaining > 0)
    {
        size_t oldLen = outputBuffer_.readable_bytes();
        if (oldLen + remaining >= highWaterMark_ &&
            oldLen < highWaterMark_ &&
            highWaterMarkCallback_) // 触发高水位回调
        {
            loop_->queue_in_loop(std::bind(highWaterMarkCallback_, shared_from_this(), oldLen + remaining));
        }
        outputBuffer_.append(static_cast<const char *>(data) + nwrote, remaining);

        if (!connChannel_->is_writing_event()) // 注册写事件监听
        {
            connChannel_->enable_writing();
        }
    }
}

/**
 • @brief 发起关闭连接请求
 • @note 非立即关闭，将关闭操作提交到事件循环
 */
void TcpConnection::shut_down()
{
    if (state_ == kConnected)
    {
        set_state(kDisconnecting);
        loop_->run_in_loop(std::bind(&TcpConnection::shut_down_in_loop, this));
    }
}

/**
 • @brief 在事件循环中执行实际关闭操作
 • @note 安全关闭写方向，若仍有数据待发送会延迟关闭
 */
void TcpConnection::shut_down_in_loop()
{
    if (!connChannel_->is_writing_event()) // 确保所有数据已发送
    {
        socket_->shutdown_write(); // 关闭写端
    }
}

/**
 • @brief 建立连接完成后的初始化操作
 • @note 注册读事件监听，触发连接建立回调
 */
void TcpConnection::connect_Established()
{
    set_state(kConnected);
    connChannel_->tie(shared_from_this()); // 绑定生命周期
    connChannel_->enable_reading();        // 开始监听读事件

    if (connectionCallback_)
    {
        connectionCallback_(shared_from_this()); // 用户连接建立回调
    }
}

/**
 • @brief 销毁连接
 • @note 清理通道，触发连接关闭回调
 */
void TcpConnection::connect_destroyed()
{
    if (state_ == kConnected)
    {
        set_state(kDisconnected);
        connChannel_->disable_all(); // 取消所有事件监听
        if (connectionCallback_)
        {
            connectionCallback_(shared_from_this()); // 用户连接关闭回调
        }
    }
    connChannel_->remove(); // 从poller移除
}

/**
 • @brief 处理读事件
 • @param receiveTime 数据到达时间戳
 */
void TcpConnection::handle_read(Timestamp receiveTime)
{
    int saveErrno = 0;
    ssize_t n = inputBuffer_.read_fd(connChannel_->fd(), saveErrno);

    if (n > 0) // 正常读取数据
    {
        messageCallback_(shared_from_this(), inputBuffer_, receiveTime);
    }
    else if (n == 0) // 对端关闭连接
    {
        handle_close();
    }
    else // 读取错误
    {
        errno = saveErrno;
        LogError("TcpConnection::handleRead");
        handle_error();
    }
}

/**
 • @brief 处理写事件
 • @note 发送输出缓冲区数据，处理写完成回调
 */
void TcpConnection::handle_write()
{
    if (connChannel_->is_writing_event())
    {
        ssize_t n = ::write(connChannel_->fd(),
                            outputBuffer_.peek(),
                            outputBuffer_.readable_bytes());
        if (n > 0)
        {
            outputBuffer_.retrieve(n);               // 移除已发送数据
            if (outputBuffer_.readable_bytes() == 0) // 发送完成
            {
                connChannel_->disable_writing(); // 取消写事件监听
                if (writeCompleteCallback_)
                {
                    loop_->queue_in_loop(std::bind(writeCompleteCallback_, shared_from_this()));
                }
                if (state_ == kDisconnecting) // 延迟关闭处理
                {
                    shut_down_in_loop();
                }
            }
        }
        else
        {
            LogError("TcpConnection::handleWrite");
        }
    }
    else
    {
        LogError("TcpConnection fd=%d is down, no more writing \n", connChannel_->fd());
    }
}

/**
 • @brief 处理连接关闭事件
 • @note 清理通道状态，触发关闭回调
 */
void TcpConnection::handle_close()
{
    LogInfo("TcpConnection::handleClose fd=%d state=%d \n", connChannel_->fd(), (int)state_);
    set_state(kDisconnected);
    connChannel_->disable_all(); // 取消所有事件监听

    TcpConnectionPtr connPtr(shared_from_this());
    connectionCallback_(connPtr); // 用户连接关闭回调
    closeCallback_(connPtr);      // 内部关闭回调
}

/**
 • @brief 处理错误事件
 • @note 获取套接字错误码并记录日志
 */
void TcpConnection::handle_error()
{
    int optval;
    socklen_t optlen = sizeof(optval);
    int err = 0;
    if (::getsockopt(connChannel_->fd(), SOL_SOCKET, SO_ERROR, &optval, &optlen) < 0)
    {
        err = errno;
    }
    LogError("TcpConnection::handleError name:%s - SO_ERROR:%d \n", name_.c_str(), err);
}