#include "TcpServer.h"
#include "logger.h"

#include <string>
#include <functional>

class EchoServer
{
public:
    // 调用最核心的server的构造
    EchoServer(EventLoop *loop, const InetAddress &addr, const std::string &name)
        : server_(loop, addr, name)
    {
        // 注册回调函数
        server_.set_connection_callback(
            std::bind(&EchoServer::on_connection, this, std::placeholders::_1));

        server_.set_message_callback(
            std::bind(&EchoServer::on_message, this,
                      std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));

        // 设置合适的loop线程数量 loopthread
        server_.set_thread_num(2);
    }
    void start()
    {
        server_.start();
    }

private:
    void on_connection(const TcpConnectionPtr &conn)
    {
        if (conn->connected())
        {
            LogError("Connection UP : %s", conn->peer_address().to_ip_port().c_str());
        }
        else
        {
            LogError("Connection DOWN : %s", conn->peer_address().to_ip_port().c_str());
        }
    }
    void on_message(const TcpConnectionPtr &conn, Buffer &buff, Timestamp time)
    {
        std::string msg = buff.retrieve_all_as_string();
        conn->send(msg);
        conn->shut_down(); // 写端   EPOLLHUP =》 closeCallback_
    }
    TcpServer server_;
};

int main()
{
    // 用户自定义的 loop 也就是 baseLoop
    EventLoop loop;
    // 将ip和端口 转换为 InetAddr内部就是 sockaddr_in
    InetAddress addr(8000);
    EchoServer server(&loop, addr, "EchoServer"); // Acceptor non-blocking listenfd  create bind
    server.start();                                  // listen  loopthread  listenfd => acceptChannel => mainLoop => listen
    loop.loop();                                     // 启动mainLoop的底层Poller

    return 0;
}