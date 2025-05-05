/**
 * @file EventLoop.h
 * @brief EventLoop核心类，实现事件循环机制
 */

#include "EventLoop.h"
#include "logger.h"
#include "Poller.h"
#include "Channel.h"
#include <sys/eventfd.h>

__thread EventLoop *t_loopInThisThread = nullptr; ///< 线程局部存储，记录当前线程所属的EventLoop实例

const int kPollTimeMs = 10 * 1000; ///< Poller默认超时时间（单位：毫秒） 10s

/**
 * @brief 创建eventfd文件描述符
 * @return int 成功返回有效的文件描述符，失败终止进程
 * @note 使用EFD_NONBLOCK非阻塞模式和EFD_CLOEXEC执行时关闭标志
 */
int create_eventfd()
{
    int evtfd = ::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    if (evtfd < 0)
    {
        LogError("eventfd error:%d \n", errno);
        abort();
    }
    return evtfd;
}

/**
 * @brief EventLoop构造函数
 * @note 1. 初始化Poller 2. 创建wakeupfd 3. 设置线程绑定关系
 * @warning 每个线程最多创建一个EventLoop实例
 */
EventLoop::EventLoop()
    : looping_(false),
      quit_(false),
      callingPendingFunctors_(false),
      threadId_(CurrentThread::tid()),
      poller_(Poller::new_default_poller(this)),
      wakeupFd_(create_eventfd()),
      wakeupChannel_(new Channel(this, wakeupFd_))
{
    LogInfo("EventLoop created %p in thread %d \n", this, threadId_);
    if (t_loopInThisThread)
    {
        LogError("Another EventLoop %p exists in this thread %d \n", t_loopInThisThread, threadId_);
    }
    else
    {
        t_loopInThisThread = this;
    }
    wakeupChannel_->enable_reading();
    wakeupChannel_->set_read_callback(std::bind(&EventLoop::handle_read, this));
}

/**
 * @brief EventLoop析构函数
 * @note 1. 清理wakeup通道 2. 关闭文件描述符 3. 重置线程局部存储
 */
EventLoop::~EventLoop()
{
    wakeupChannel_->disable_all();
    wakeupChannel_->remove();
    ::close(wakeupFd_);
    t_loopInThisThread = nullptr;
}

/**
 * @brief 启动事件循环
 * @note 核心工作流程：
 * 1. 调用Poller获取就绪通道
 * 2. 处理所有就绪通道事件
 * 3. 执行pending回调
 * 4. 循环直到quit标志置位
 */
void EventLoop::loop()
{
    looping_ = true;
    quit_ = false;

    LogInfo("EventLoop %p start looping \n", this);
    while (!quit_)
    {
        activeChannels_.clear();
        pollReturnTime_ = poller_->poll(kPollTimeMs, &activeChannels_);

        for (Channel *channel : activeChannels_)
        {
            channel->handle_event(pollReturnTime_);
        }

        do_pending_functors();
    }
    LogInfo("EventLoop %p stop looping. \n", this);
    looping_ = false;
}

/**
 * @brief 安全退出事件循环
 * @note 跨线程调用时需唤醒事件循环
 */
void EventLoop::quit()
{
    quit_ = true;
    if (!is_in_loop_thread())
    {
        wakeup();
    }
}

/**
 * @brief 执行或排队回调函数
 * @param cb 待执行的回调函数
 * @note 线程安全：
 * - 当前线程直接执行
 * - 其他线程排队后唤醒事件循环
 */
void EventLoop::run_in_loop(Functor cb)
{
    if (is_in_loop_thread())
    {
        cb();
    }
    else
    {
        queue_in_loop(cb);
    }
}

/**
 * @brief 将回调加入待执行队列
 * @param cb 待排队回调
 * @note 线程安全：使用互斥锁保护pendingFunctors_
 */
void EventLoop::queue_in_loop(Functor cb)
{
    {
        std::lock_guard<std::mutex> locker(mutex_);
        pendingFuntors_.emplace_back(cb);
    }
    if (!is_in_loop_thread() || callingPendingFunctors_)
    {
        wakeup();
    }
}

/**
 * @brief 唤醒事件循环
 * @note 通过向wakeupFd_写入8字节数据触发epoll事件
 */
void EventLoop::wakeup()
{
    uint64_t one = 1;
    ssize_t n = ::write(wakeupFd_, &one, sizeof(one));
    if (n != sizeof(one))
    {
        LogError("EventLoop::wakeup() writes %lu bytes instead of 8 \n", n);
    }
}

/**
 * @brief 处理wakeup事件
 * @note 读取8字节数据清空事件通知，保持epoll状态
 */
void EventLoop::handle_read()
{
    uint64_t one = 1;
    ssize_t n = ::read(wakeupFd_, &one, sizeof(one));
    if (n != sizeof(one))
    {
        LogError("EventLoop::handleRead() reads %lu bytes instead of 8", n);
    }
}

/**
 * @brief 执行待处理回调队列
 * @note 关键技术：
 * 1. 使用swap减少临界区时间
 * 2. callingPendingFunctors_标志防止重复唤醒
 */
void EventLoop::do_pending_functors()
{
    std::vector<Functor> functors;
    callingPendingFunctors_ = true;

    {
        std::lock_guard<std::mutex> locker(mutex_);
        functors.swap(pendingFuntors_);
    }

    for (const auto &functor : functors)
    {
        functor();
    }
    callingPendingFunctors_ = false;
}

/**
 * @brief 更新通道状态
 * @param channel 需要更新的通道指针
 * @see Poller::updateChannel
 */
void EventLoop::update_channel(Channel *channel)
{
    poller_->update_channel(channel);
}

/**
 * @brief 移除通道
 * @param channel 需要移除的通道指针
 * @see Poller::removeChannel
 */
void EventLoop::remove_channel(Channel *channel)
{
    poller_->remove_channel(channel);
}

/**
 * @brief 检查通道是否已注册
 * @param channel 需要检查的通道指针
 * @return bool 存在返回true
 * @see Poller::hasChannel
 */
bool EventLoop::has_channel(Channel *channel)
{
    return poller_->has_channel(channel);
}