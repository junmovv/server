#include "EventLoopThread.h"
#include "EventLoop.h"
/**
 * @file EventLoopThread.cpp
 * @brief 事件循环线程管理类实现
 */

/**
 * @class EventLoopThread
 * @brief 整合事件循环与线程，实现多线程事件循环管理
 * @note 实现特点：
 *       - 组合Thread和EventLoop对象
 *       - 支持线程安全的EventLoop获取
 *       - 提供线程初始化回调机制
 */

/**
 * @brief 构造函数，初始化线程相关参数
 * @param cb 线程初始化回调（在子线程执行）
 * @param name 线程名称（用于调试）
 * @note 延迟初始化EventLoop对象，实际创建发生在子线程中
 */
EventLoopThread::EventLoopThread(const ThreadInitCallback &cb, const std::string &name)
    : loop_(nullptr),
      exiting_(false),
      thread_(std::bind(&EventLoopThread::thread_func, this), name),
      ThreadInitCallback_(cb)
{
}

/**
 * @brief 析构函数，安全停止事件循环
 * @note 操作步骤：
 *       1. 设置退出标志位
 *       2. 停止事件循环
 *       3. 等待线程结束
 */
EventLoopThread::~EventLoopThread()
{
    exiting_ = true;
    if (loop_)
    {
        loop_->quit();  // 停止事件循环
        thread_.join(); // 等待线程结束
    }
}

/**
 * @brief 启动事件循环线程
 * @return 返回创建好的EventLoop指针
 * @note 线程安全操作：
 *       - 使用条件变量等待子线程初始化完成
 *       - 双检锁模式保证线程安全
 */
EventLoop *EventLoopThread::start_loop()
{
    thread_.start(); // 启动底层线程

    {
        std::unique_lock<std::mutex> locker(mutex_);
        // 等待子线程完成EventLoop初始化
        cond_.wait(locker, [this]()
                   { return loop_ != nullptr; });
    }
    return loop_; // 返回完全初始化的EventLoop
}

/**
 * @brief 线程主函数（子线程执行）
 * @note 关键流程：
 *       1. 创建子线程专属的EventLoop
 *       2. 执行用户初始化回调
 *       3. 通知主线程初始化完成
 *       4. 启动事件循环
 *       5. 清理资源
 */
void EventLoopThread::thread_func()
{
    EventLoop loop; // 子线程专属事件循环

    // 执行线程初始化回调（如设置线程名称等）
    if (ThreadInitCallback_)
    {
        ThreadInitCallback_(&loop);
    }

    // 通知主线程初始化完成
    {
        std::unique_lock<std::mutex> locker(mutex_);
        loop_ = &loop;
        cond_.notify_one();
    }

    loop.loop(); // 启动事件循环（阻塞直到quit被调用）

    // 清理工作
    std::unique_lock<std::mutex> locker(mutex_);
    loop_ = nullptr; // 重置指针（此时loop已析构）
}