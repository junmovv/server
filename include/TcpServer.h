#ifndef TCPSERVER_H
#define TCPSERVER_H

#include "TcpConnection.h"
#include "EventLoop.h"
#include <unordered_map>
#include <mutex>

class Acceptor;
class EventLoop;
class EventLoopThreadPool;

class TcpServer
{

public:
    using ThreadInitCallback = std::function<void(EventLoop *)>;
    enum Option
    {
        kNoReusePort,
        kReusePort,
    };
    TcpServer(EventLoop *loop, const InetAddress &listenAddr, const std::string &serverName, Option option = kNoReusePort);
    ~TcpServer();

    void set_thread_init_callback(const ThreadInitCallback &cb) { threadInitCallback_ = std::move(cb); }
    void set_connection_callback(const ConnectionCallback &cb) { connectionCallback_ = std::move(cb); }
    void set_message_callback(const MessageCallback &cb) { messageCallback_ = std::move(cb); }
    void set_write_complete_callback(const WriteCompleteCallback &cb) { writeCompleteCallback_ = std::move(cb); }

    // 设置subLoop个数
    void set_thread_num(int numThreads);

    void start();

private:
    void new_connection(int connfd, const InetAddress &peerAddr);
    void remove_connection(const TcpConnectionPtr &conn);
    void remove_connection_in_loop(const TcpConnectionPtr &conn);

    EventLoop *loop_;              // baseLoop 用户定义的loop
    const std::string ipPort_;     // 服务器的ip和端口
    const std::string serverName_; // 服务器自定义的名称

    std::unique_ptr<Acceptor> acceptor_; // 运行在baseLoop 负责监听新事件连接

    std::shared_ptr<EventLoopThreadPool> threadPool_; // one loop per thread  subLoop用来处理连接逻辑

    ThreadInitCallback threadInitCallback_;       // loop线程初始化的回调 用户定义 最终调用处为EventLoopThread::thread_func
    ConnectionCallback connectionCallback_;       // 管理连接的回调  连接建立或者关闭 调用此函数 最终调用处为TcpConnection
    MessageCallback messageCallback_;             // 有读写消息回调  用户定义 最终调用处为TcpConnection
    WriteCompleteCallback writeCompleteCallback_; //  消息发送完成以后的回调 用户定义 最终调用处为TcpConnection

    //    std::atomic<int> started_;
    std::once_flag started_;                                        // 保证start内部的函数只被调用一次
    int nextConnId_;                                                // 一共有多少客户端连接过
    std::unordered_map<std::string, TcpConnectionPtr> connections_; // 保存所有连接
};

#endif