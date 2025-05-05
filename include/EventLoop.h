#ifndef EVENTLOOP_H
#define EVENTLOOP_H
#include "noncopyable.h"
#include "CurrentThread.h"
#include "Timestamp.h"
#include <atomic>
#include <memory>
#include <vector>
#include <functional>
#include <mutex>
class Channel;
class Poller;

class EventLoop : public noncopyable
{

public:
    using Functor = std::function<void()>;
    EventLoop(/* args */);
    ~EventLoop();
    void loop();
    void quit();
    Timestamp poll_return_time() const { return pollReturnTime_; }

    void run_in_loop(Functor cb);
    void queue_in_loop(Functor cb);

    void wakeup();

    void update_channel(Channel *channel);
    void remove_channel(Channel *channel);
    
    bool has_channel(Channel *channel);

    bool is_in_loop_thread() const { return threadId_ == CurrentThread::tid(); }

private:
    void handle_read();
    void do_pending_functors();

    std::atomic<bool> looping_;
    std::atomic<bool> quit_;

    const pid_t threadId_;
    Timestamp pollReturnTime_;
    std::unique_ptr<Poller> poller_;

    int wakeupFd_;
    std::unique_ptr<Channel> wakeupChannel_;

    std::vector<Channel *> activeChannels_;
    std::atomic<bool> callingPendingFunctors_;
    std::vector<Functor> pendingFuntors_;

    std::mutex mutex_;
};

#endif