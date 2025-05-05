#ifndef EVENTLOOPTHREADPOOL_H
#define EVENTLOOPTHREADPOOL_H

#include <functional>
#include <memory>
#include <vector>

class EventLoop;
class EventLoopThread;

class EventLoopThreadPool
{
public:
    using ThreadInitCallback = std::function<void(EventLoop *)>;

    EventLoopThreadPool(EventLoop *baseLoop, const std::string &serverName);
    ~EventLoopThreadPool();

    void set_thread_num(int numThreads)
    {
        numThreads_ = numThreads;
    }

    void start(const ThreadInitCallback &cb = ThreadInitCallback());
    EventLoop *get_next_loop();
    std::vector<EventLoop *> get_all_loops();

    bool started() const
    {
        return started_;
    }

    const std::string name() const
    {
        return serverName_;
    }

private:
    /* data */
    EventLoop *baseLoop_;
    std::string serverName_;
    bool started_;
    int numThreads_;
    int next_;
    std::vector<std::unique_ptr<EventLoopThread>> threads_;
    std::vector<EventLoop *> loops_; // 绑定的是栈上loop
};

#endif