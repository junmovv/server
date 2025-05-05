
#ifndef EPOLLPOLLER_H
#define EPOLLPOLLER_H

#include "Poller.h"

#include <vector>

struct epoll_event;

class EPollPoller : public Poller
{
public:
    EPollPoller(EventLoop *loop);
    virtual ~EPollPoller() override;
    virtual Timestamp poll(int timeoutMs, ChannelList *activeChannels) override;
    virtual void update_channel(Channel *channel) override;
    virtual void remove_channel(Channel *channel) override;

private:
    static const int KInitEventListSize_ = 16;
    void fill_active_channels(int eventNum, ChannelList *activeChannels);
    void update(int operation, Channel *channel);
    using EventList = std::vector<struct epoll_event>;
    int epollfd_;
    EventList events_; // 就绪的事件 通过接口赋值给channel的revents_
};

#endif
