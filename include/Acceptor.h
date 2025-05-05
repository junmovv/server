#ifndef ACCEPTOR_H
#define ACCEPTOR_H
#include "Channel.h"
#include "Socket.h"

#include <functional>


// Acceptor 的作用与归属​​
// ​​角色​​：Acceptor 负责监听服务端端口，接受新连接，​​属于主 Reactor（baseLoop）​​。
// ​​关键组件​​：
// ​​监听套接字（listenfd）​​：绑定到服务端端口，用于接收新连接。 对应 acceptSocket_
// ​​Channel 对象​​：封装 listenfd 的事件监听，注册到 baseLoop 的 Poller 中。  对应 acceptChannel_

class Acceptor : noncopyable
{

public:
    using NewConnectionCallback = std::function<void(int sockfd, const InetAddress &)>;
    Acceptor(EventLoop *loop, const InetAddress &listenAddr, bool reuseport);
    ~Acceptor();
    void set_new_connection_callback(const NewConnectionCallback &cb) { NewConnectionCallback_ = std::move(cb); }
    bool listenning() const { return listenning_; }
    void listen();

private:
    void handle_read();

    EventLoop *loop_; // baseLoop
    Socket acceptSocket_; // 封装了 listenfd
    Channel acceptChannel_; // 封装了 listenfd的事件监听
    // 对应 TcpServer::new_connection(int sockfd, const InetAddress &peerAddr) 有新连接到来执行此回调
    NewConnectionCallback NewConnectionCallback_;
    bool listenning_;
};

#endif
