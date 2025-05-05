#ifndef EVENTLOOPTHREAD_H
#define EVENTLOOPTHREAD_H

#include "noncopyable.h"
#include "Thread.h"
#include <mutex>
#include <condition_variable>
#include <string>
#include <functional>

class EventLoop;

class EventLoopThread : public noncopyable
{
public:
    using ThreadInitCallback = std::function<void(EventLoop *)>;

    EventLoopThread(const ThreadInitCallback &cb = ThreadInitCallback(), const std::string &name = std::string());
    ~EventLoopThread();

    EventLoop *start_loop();

private:
    void thread_func();

    EventLoop *loop_;
    bool exiting_;
    Thread thread_; // 绑定  EventLoopThread::thread_func
    std::mutex mutex_;
    std::condition_variable cond_;
    ThreadInitCallback ThreadInitCallback_;
};

#endif