/** @file Thread.cpp */
/** @brief 线程封装实现文件 */

#include "Thread.h"
#include "CurrentThread.h"
#include "logger.h"
#include <sys/prctl.h>

std::atomic<int> Thread::numCreated_(0); ///< 线程创建计数器原子变量

/**
 • @brief 线程内部数据传递结构
 • @note 用于封装线程启动时的初始化参数
 */
struct ThreadData
{
    using ThreadFunc = Thread::ThreadFunc;
    ThreadFunc threadFunc_;  /// 事件循环线程函数 绑定为 EventLoopThread::thread_func()
    std::string name_; ///< 线程名称
    pid_t *tid_;       ///< 指向线程真实ID的指针
    sem_t *sem_;       ///< 同步信号量指针

    /**
     ◦ @brief 构造函数
     ◦ @param func 线程执行函数
     ◦ @param name 线程名称
     ◦ @param tid 内核线程ID存储地址
     ◦ @param sem 同步信号量
     */
    ThreadData(ThreadFunc func, const std::string &name, pid_t *tid, sem_t *sem)
        : threadFunc_(std::move(func)), name_(name), tid_(tid), sem_(sem) {}

    /**
     ◦ @brief 线程实际执行入口
     ◦ @note 完成线程ID记录、线程命名、异常捕获等初始化工作
     */
    void run_in_thread()
    {
        // 记录真实线程ID并通知主线程
        *tid_ = CurrentThread::tid();
        tid_ = nullptr;
        sem_post(sem_);

        // 设置线程名称（Linux特有方式）
        CurrentThread::t_threadName = name_.empty() ? "muduoThread" : name_.c_str();
        ::prctl(PR_SET_NAME, CurrentThread::t_threadName);

        try
        {
            threadFunc_(); // 执行事件循环回调函数  EventLoopThread::thread_func()
            CurrentThread::t_threadName = "finished";
        }
        catch (const std::exception &e)
        {
            // 异常捕获并记录日志，保证不异常退出
            LogError("%s is error [%s]", CurrentThread::t_threadName, e.what());
        }
    }
};

/**
 • @brief POSIX线程启动函数
 • @param obj 线程数据对象指针
 • @return 永远返回nullptr
 • @note C++线程入口到C函数的适配层
 */
void *start_thread(void *obj)
{
    ThreadData *data = static_cast<ThreadData *>(obj);
    data->run_in_thread();
    delete data; // 清理堆内存
    return nullptr;
}

/**
 • @brief 线程类构造函数
 • @param func 线程执行函数
 • @param name 线程名称（可选）
 • @note 初始化信号量并自动生成默认线程名
 */
Thread::Thread(ThreadFunc func, const std::string &name)
    : started_(false), joined_(false),
      pthreadId_(0), tid_(0),
      threadFunc_(std::move(func)), name_(name)
{
    sem_init(&sem_, false, 0); // 进程内共享，初始计数0
    set_default_name();
}

/**
 • @brief 线程类析构函数
 • @note 自动detach未join的线程，避免资源泄漏
 */
Thread::~Thread()
{
    if (started_ && !joined_)
    {
        pthread_detach(pthreadId_); // 分离线程自动回收
    }
    sem_destroy(&sem_); // 销毁信号量
}

/**
 • @brief 启动线程
 • @note 使用信号量同步等待线程真实ID获取
 • @warning 创建失败会重置启动状态
 */
void Thread::start()
{
    started_ = true;
    ThreadData *data = new ThreadData(threadFunc_, name_, &tid_, &sem_);

    if (pthread_create(&pthreadId_, nullptr, start_thread, data) < 0)
    {
        started_ = false;
        delete data; // 清理资源
        LogError("Failed in pthread_create\n");
    }
    else
    {
        sem_wait(&sem_); // 等待子线程设置完成真实ID
    }
}

/**
 • @brief 等待线程结束
 • @return pthread_join的返回值
 • @note 只能调用一次，调用后joined_标记为true
 */
int Thread::join()
{
    joined_ = true;
    return pthread_join(pthreadId_, nullptr);
}

/**
 • @brief 设置默认线程名称
 • @note 当用户未指定名称时，自动生成"Thread+序号"的格式
 */
void Thread::set_default_name()
{
    int num = num_created();
    if (name_.empty())
    {
        name_ = "Thread" + std::to_string(num);
    }
}

