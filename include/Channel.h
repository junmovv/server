#ifndef CHANNEL_H
#define CHANNEL_H

#include "noncopyable.h"
#include "Timestamp.h"
#include <functional>
#include <memory>
#include <sys/epoll.h>
class EventLoop;

class Channel : noncopyable
{
public:
    using EventCallback = std::function<void()>;
    using ReadEventCallback = std::function<void(Timestamp)>;

public:
    Channel(EventLoop *loop, int fd);
    ~Channel();

    void handle_event(Timestamp recvTime);

    void set_write_callback(EventCallback cb) { writeCallback_ = std::move(cb); }

    void set_close_callback(EventCallback cb) { closeCallback_ = std::move(cb); }
    void set_error_callback(EventCallback cb) { errorCallback_ = std::move(cb); }
    void set_read_callback(ReadEventCallback cb) { readCallBack_ = std::move(cb); }

    void tie(const std::shared_ptr<void> &);
    int fd() const { return fd_; }
    int events() const { return events_; }
    void set_revents(int revents) { revents_ = revents; }

    void enable_reading()
    {
        events_ |= KReadEvent;
        update();
    }
    void disable_reading()
    {
        events_ &= ~KReadEvent;
        update();
    }

    void enable_writing()
    {
        events_ |= KWriteEvent;
        update();
    }
    void disable_writing()
    {
        events_ &= ~KWriteEvent;
        update();
    }
    void disable_all()
    {
        events_ = KNoneEvent;
        update();
    }

    bool is_none_event() const { return events_ == KNoneEvent; }
    bool is_writing_event() const { return events_ & KWriteEvent; }
    bool is_reading_event() const { return events_ & KReadEvent; }

    int index() const { return index_; }
    void set_index(int index) { index_ = index; }

    EventLoop *owner_loop() const { return loop_; }
    void remove();

private:
    void update();
    void handle_event_with_guard(Timestamp recvTime);

private:
    EventLoop *loop_; // 记录自己属于哪个loop
    const int fd_;
    int events_;  // 注册的事件  读 写 其他
    int revents_; // 实际就绪的事件 会由poller返回
    int index_;
    std::weak_ptr<void> tie_;
    bool tied_;

    static const int KNoneEvent = 0;
    static const int KReadEvent = EPOLLIN | EPOLLPRI;
    static const int KWriteEvent = EPOLLOUT;

    EventCallback writeCallback_;
    EventCallback closeCallback_;
    EventCallback errorCallback_;
    // 内部的话对应的是 void Acceptor::handle_read()
    ReadEventCallback readCallBack_;
};

#endif


/**
 * @class Channel
 * @brief 表示事件循环中与文件描述符关联的通信通道
 * 



 */
