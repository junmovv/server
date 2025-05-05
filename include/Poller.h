#ifndef POLLER_H
#define POLLER_H

#include <unordered_map>
#include <vector>
#include "Timestamp.h"
#include "noncopyable.h"

class Channel;
class EventLoop;

class Poller : noncopyable
{
public:
    using ChannelList = std::vector<Channel *>;
    Poller(EventLoop *loop);
    virtual ~Poller();
    virtual Timestamp poll(int timeoutMs, ChannelList *activeChannels) = 0;
    virtual void update_channel(Channel *channel) = 0;
    virtual void remove_channel(Channel *channel) = 0;

    bool has_channel(Channel *channel) const;
    static Poller *new_default_poller(EventLoop *loop);

protected:
    using ChannelMap = std::unordered_map<int, Channel *>; // 实际监听的  fd  channel
    ChannelMap channels_;

private:
    EventLoop *loop_;
};

#endif