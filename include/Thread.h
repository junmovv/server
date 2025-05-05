
#ifndef THREAD_H
#define THREAD_H
#include "noncopyable.h"
#include <string>
#include <functional>
#include <pthread.h>
#include <semaphore.h>
#include <atomic>
class Thread : public noncopyable
{
public:
    using ThreadFunc = std::function<void()>;
    explicit Thread(ThreadFunc func, const std::string &name = std::string());
    ~Thread();

    void start();
    int join();

    bool started() const { return started_; }
    pid_t tid() const { return tid_; }
    std::string name() const { return name_; }
    static int num_created() { return numCreated_; }

private:
    void set_default_name();
    bool started_;
    bool joined_;
    pthread_t pthreadId_;
    pid_t tid_;
    ThreadFunc threadFunc_;
    std::string name_;
    sem_t sem_;
    static std::atomic<int> numCreated_;
};
#endif