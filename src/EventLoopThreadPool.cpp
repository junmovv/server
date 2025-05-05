#include "EventLoopThreadPool.h"
#include "EventLoopThread.h"
/**
 * @file EventLoopThreadPool.cpp
 * @brief 事件循环线程池实现，管理多个EventLoop线程
 */

/**
 * @class EventLoopThreadPool
 * @brief 事件循环线程池，支持主从多Reactor模式
 * @note 特性：
 *       - 内置一个主EventLoop（baseLoop_）
 *       - 可创建多个子EventLoop线程组成线程池
 *       - 采用轮询算法分配连接给子EventLoop
 *       - 支持全线程停止时的资源自动回收
 */

/**
 * @brief 构造函数，初始化线程池基础属性
 * @param baseLoop 主事件循环（通常由主线程创建）
 * @param nameArg 线程池名称（用于日志标识）
 * @note 初始状态：
 *       - 未启动（started_ = false）
 *       - 子线程数为0（numThreads_ = 0）
 *       - 使用轮询指针初始位置（next_ = 0）
 */
EventLoopThreadPool::EventLoopThreadPool(EventLoop *baseLoop, const std::string &serverName)
    : baseLoop_(baseLoop),
      serverName_(serverName),
      started_(false),
      numThreads_(0),
      next_(0)
{
}

/**
 * @brief 析构函数（默认实现）
 * @note 依赖智能指针自动回收子线程资源
 */
EventLoopThreadPool::~EventLoopThreadPool() = default;

/**
 * @brief 启动线程池中的所有事件循环线程
 * @param cb 线程初始化回调函数（适用于每个子线程）
 * @note 关键流程：
 *       1. 创建指定数量的EventLoopThread
 *       2. 启动所有子线程并收集其EventLoop指针
 *       3. 当线程数为0时，直接在主循环执行回调
 * @warning 必须在主EventLoop线程调用
 */
void EventLoopThreadPool::start(const ThreadInitCallback &cb)
{
    started_ = true;

    // 创建子线程及其事件循环
    for (int i = 0; i < numThreads_; ++i)
    {
        std::string threadName = serverName_ + std::to_string(i); // 生成线程名称
        EventLoopThread *t = new EventLoopThread(cb, threadName);
        threads_.push_back(std::unique_ptr<EventLoopThread>(t)); // 托管线程对象
        loops_.push_back(t->start_loop());                       // 存储子线程的事件循环
    }

    // 处理单线程模式下的回调
    if (numThreads_ == 0 && cb)
    {
        cb(baseLoop_); // 直接使用主循环执行初始化
    }
}

/**
 * @brief 获取下一个事件循环（轮询算法）
 * @return 可用的EventLoop指针
 * @note 分配策略：
 *       1. 存在子线程时，轮询返回子EventLoop
 *       2. 无子线程时返回主EventLoop
 *       3. 线程安全由调用方保证（通常在IO线程调用）
 */
EventLoop *EventLoopThreadPool::get_next_loop()
{
    EventLoop *loop = baseLoop_;

    if (!loops_.empty())
    {
        // 轮询算法选择子线程
        loop = loops_[next_];
        if (++next_ >= loops_.size())
        {
            next_ = 0; // 环形递增
        }
    }
    return loop;
}

/**
 * @brief 获取所有子事件循环（含主循环兜底）
 * @return EventLoop指针列表
 * @note 当未创建子线程时，返回包含主循环的列表
 *       用于统一管理所有事件循环
 */
std::vector<EventLoop *> EventLoopThreadPool::get_all_loops()
{
    return loops_.empty() ? std::vector<EventLoop *>{baseLoop_} : loops_;
}