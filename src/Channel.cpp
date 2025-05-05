#include "Channel.h"
#include "EventLoop.h"

/**
 * @class Channel
 * @brief 表示事件循环中与文件描述符关联的通信通道
 */

/**
 * @brief 构造函数，创建与事件循环和文件描述符关联的通道
 * @param loop 管理本通道的事件循环指针
 * @param fd 关联的文件描述符
 * @note 初始化时：
 *       - 未监听任何事件（events_ = 0）
 *       - 无已触发事件（revents_ = 0）
 *       - 初始为未绑定状态（tied_ = false）
 *       - 未注册到poller中（index_ = -1）
 */
Channel::Channel(EventLoop *loop, int fd)
    : loop_(loop), fd_(fd), events_(0), revents_(0), index_(-1), tied_(false)
{
}

/**
 * @brief 析构函数
 * @note 确保通道资源的正确清理，如需具体销毁逻辑应在类定义中实现
 */
Channel::~Channel()
{
}

/**
 * @brief 处理I/O事件（带生命周期保护）
 * @param recvTime 事件触发的时间戳
 * @note 通过tie机制保证事件处理期间宿主对象存活：
 *       - 当绑定对象存在时，使用guard保证对象生命周期
 *       - 直接处理事件前会检查弱引用是否有效
 */
void Channel::handle_event(Timestamp recvTime)
{
    if (tied_)
    {
        std::shared_ptr<void> guard = tie_.lock();
        if (guard)
        {
            handle_event_with_guard(recvTime);
        }
    }
    else
    {
        handle_event_with_guard(recvTime);
    }
}

/**
 * @brief 绑定通道到共享对象进行生命周期管理
 * @param obj 宿主对象的共享指针
 * @post 启用tied_标志并保存弱引用，防止宿主销毁后执行回调
 */
void Channel::tie(const std::shared_ptr<void> &obj)
{
    tied_ = true;
    tie_ = obj;
}

/**
 * @brief 从事件循环中移除本通道
 * @note 调用EventLoop::remove_channel()实现线程安全的移除操作
 */
void Channel::remove()
{
    loop_->remove_channel(this);
}

/**
 * @brief 更新通道的事件监听状态
 * @note 调用EventLoop::update_channel()刷新事件监听设置
 */
void Channel::update()
{
    loop_->update_channel(this);
}

/**
 * @brief 实际处理事件的核心逻辑（无生命周期检查）
 * @param recvTime 事件触发时间戳
 * @note 事件处理优先级：
 *       1. 连接关闭事件（EPOLLHUP且无待读数据）
 *       2. 错误事件（EPOLLERR）
 *       3. 可读事件（EPOLLIN/EPOLLPRI）
 *       4. 可写事件（EPOLLOUT）
 * @warning 必须在确保对象存活的上下文中调用
 */
void Channel::handle_event_with_guard(Timestamp recvTime)
{
    // 处理远端关闭连接（无待读数据的情况）
    if ((revents_ & EPOLLHUP) && !(revents_ & EPOLLIN))
    {
        if (closeCallback_)
        {
            closeCallback_(); // 执行连接关闭回调
        }
    }

    // 处理套接字错误
    if (revents_ & EPOLLERR)
    {
        if (errorCallback_)
        {
            errorCallback_(); // 执行错误处理回调
        }
    }

    // 处理数据可读事件（含紧急数据）
    if (revents_ & (EPOLLIN | EPOLLPRI))
    {
        if (readCallBack_)
        {
            readCallBack_(recvTime); // 执行读回调，传递事件时间戳
        }
    }

    // 处理写缓冲区就绪
    if (revents_ & EPOLLOUT)
    {
        if (writeCallback_)
        {
            writeCallback_(); // 执行写回调
        }
    }
}