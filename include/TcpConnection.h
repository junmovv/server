#ifndef TCPCONNECTION_H
#define TCPCONNECTION_H
#include "Callbacks.h"
#include "InetAddress.h"
#include "Buffer.h"
#include "Timestamp.h"
#include <atomic>
#include <string>

class Channel;
class Socket;
class EventLoop;

class TcpConnection : noncopyable, public std::enable_shared_from_this<TcpConnection>
{

public:
    TcpConnection(EventLoop *loop, const std::string &name, int sockfd, const InetAddress &localAddr, const InetAddress &peerAddr);
    ~TcpConnection();

    EventLoop *get_loop() const { return loop_; }
    const std::string &name() const { return name_; }
    const InetAddress &local_address() const { return localAddr_; }
    const InetAddress &peer_address() const { return peerAddr_; }

    bool connected() const { return state_ == kConnected; }

    void send(const std::string &buf);
    void send(void *message, size_t len);

    void shut_down();

    void set_connection_callback(const ConnectionCallback &cb) { connectionCallback_ = std::move(cb); }
    void set_message_callback(const MessageCallback &cb) { messageCallback_ = std::move(cb); }
    void set_write_complete_callback(const WriteCompleteCallback &cb) { writeCompleteCallback_ = std::move(cb); }
    void set_high_water_mark_callback(const HighWaterMarkCallback &cb) { highWaterMarkCallback_ = std::move(cb); }
    void set_close_callback(const CloseCallback &cb) { closeCallback_ = std::move(cb); }

    void connect_Established();
    void connect_destroyed();

private:
    enum StateE
    {
        kDisconnected,
        kConnecting,
        kConnected,
        kDisconnecting,
    };

    void set_state(StateE state) { state_ = state; }

    void handle_read(Timestamp receiveTime);
    void handle_write();
    void handle_close();
    void handle_error();

    void send_in_loop(const void *data, size_t len);
    void shut_down_in_loop();

    EventLoop *loop_; // subLoop
    const std::string name_;
    std::atomic<int> state_;

    bool reading_;

    std::unique_ptr<Socket> socket_;
    std::unique_ptr<Channel> connChannel_;

    const InetAddress localAddr_;
    const InetAddress peerAddr_;

    ConnectionCallback connectionCallback_;       // 管理连接的回调  连接建立或者关闭 调用此函数 最终调用处为TcpConnection
    MessageCallback messageCallback_;             // 有读写消息回调  用户定义 最终调用处为TcpConnection
    WriteCompleteCallback writeCompleteCallback_; //  消息发送完成以后的回调 用户定义 最终调用处为TcpConnection
    HighWaterMarkCallback highWaterMarkCallback_;
    CloseCallback closeCallback_; // 绑定  TcpServer::remove_connection

    size_t highWaterMark_;

    Buffer inputBuffer_;  // 接受缓冲区
    Buffer outputBuffer_; // 发送缓冲区
    /* data */
};

#endif
