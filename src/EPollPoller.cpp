#include "EPollPoller.h"
#include "logger.h"
#include "Channel.h"
#include <errno.h>
#include <unistd.h>
#include <strings.h>
/**
 * @file EPollPoller.cpp
 * @brief epoll IO多路复用器的具体实现
 */

// 通道状态常量定义
const int kNew = -1;    ///< 通道未加入epoll监控
const int kAdded = 1;   ///< 通道已加入epoll监控
const int kDeleted = 2; ///< 通道已从epoll监控移除

/**
 * @brief 创建epoll实例并初始化事件列表
 * @param loop 所属事件循环
 * @note 使用epoll_create1创建带CLOEXEC标志的epollfd
 *       初始事件列表大小为16，自动扩容机制
 * @warning 创建失败将记录FATAL级别日志
 */
EPollPoller::EPollPoller(EventLoop *loop)
    : Poller(loop),
      epollfd_(::epoll_create1(EPOLL_CLOEXEC)),
      events_(KInitEventListSize_)
{
    if (epollfd_ < 0)
    {
        LogError("EPollPoller::EPollPoller failed: %s\n", strerror(errno));
    }
}

/**
 * @brief 关闭epoll文件描述符
 */
EPollPoller::~EPollPoller()
{
    ::close(epollfd_);
}

/**
 * @brief 执行事件监听核心逻辑
 * @param timeoutMs 超时时间（毫秒）
 * @param[out] activeChannels 输出活跃通道列表
 * @return 事件触发时间戳
 * @note 典型执行流程：
 *       1. 调用epoll_wait等待事件
 *       2. 处理错误(EINTR自动重试)
 *       3. 填充活跃通道列表
 *       4. 必要时扩容事件列表（2倍扩容策略）
 */
Timestamp EPollPoller::poll(int timeoutMs, ChannelList *activeChannels)
{
    LogInfo("fd total count [%d]\n", channels_.size());
    int eventNum = ::epoll_wait(epollfd_, events_.data(), events_.size(), timeoutMs);
    int savedErrno = errno;
    Timestamp now(Timestamp::now());

    if (eventNum > 0)
    {
        LogInfo("events happened [%d]\n", eventNum);
        fill_active_channels(eventNum, activeChannels);
        if (events_.size() == eventNum) // 事件列表满时自动扩容
        {
            events_.resize(2 * events_.size());
        }
    }
    else if (eventNum == 0)
    {
       // LogInfo("not events happened [%d]\n", eventNum);
    }
    else
    {
        if (errno != EINTR) // 忽略信号中断错误
        {
            LogError("EPollPoller::poll error: %s\n", strerror(savedErrno));
        }
    }
    return now;
}

/**
 * @brief 更新通道监控状态
 * @param channel 要操作的通道
 * @note 状态机转换逻辑：
 *       kNew/kDeleted -> 添加监控（EPOLL_CTL_ADD）
 *       kAdded -> 根据事件有无决定修改或删除
 * @warning 必须在EventLoop线程调用
 */
void EPollPoller::update_channel(Channel *channel)
{
    const int index = channel->index();
    LogInfo("fd[%d] events[%d] index [%d]\n", channel->fd(), channel->events(), channel->index());

    if (index == kNew || index == kDeleted)
    {
        // 新通道注册到epoll
        if (index == kNew)
        {
            channels_[channel->fd()] = channel; // 加入通道映射表
        }
        channel->set_index(kAdded);
        update(EPOLL_CTL_ADD, channel); // 执行epoll添加操作
    }
    else
    {
        // 已注册通道的更新
        if (channel->is_none_event()) // 无关注事件则删除监控
        {
            update(EPOLL_CTL_DEL, channel);
            channel->set_index(kDeleted);
        }
        else // 修改事件监控类型
        {
            update(EPOLL_CTL_MOD, channel);
        }
    }
}

/**
 * @brief 移除通道的监控
 * @param channel 要移除的通道
 * @note 操作步骤：
 *       1. 从通道映射表移除
 *       2. 若在监控中则执行epoll删除
 *       3. 重置通道状态为kNew
 */
void EPollPoller::remove_channel(Channel *channel)
{
    int fd = channel->fd();
    if (channels_.find(fd) != channels_.end())
    {
        channels_.erase(fd);
        LogInfo("fd[%d] \n", fd);
        if (channel->index() == kAdded)
        {
            update(EPOLL_CTL_DEL, channel); // 从epoll删除监控
        }
        channel->set_index(kNew); // 重置为初始状态
    }
}

/**
 * @brief 填充活跃通道列表
 * @param eventNum 就绪事件数量
 * @param[out] activeChannels 输出参数，存储就绪通道
 * @note 利用epoll_event的data.ptr字段获取关联的Channel对象
 *       设置通道的revents字段后加入活跃列表
 */
void EPollPoller::fill_active_channels(int eventNum, ChannelList *activeChannels)
{
    for (int i = 0; i < eventNum; ++i)
    {
        Channel *channel = static_cast<Channel *>(events_[i].data.ptr);
        channel->set_revents(events_[i].events); // 设置触发的事件类型
        activeChannels->push_back(channel);      // 加入就绪队列
    }
}

/**
 * @brief 执行底层epoll操作
 * @param operation EPOLL_CTL_ADD/MOD/DEL
 * @param channel 要操作的通道
 * @note 封装epoll_ctl系统调用，处理错误日志
 */
void EPollPoller::update(int operation, Channel *channel)
{
    struct epoll_event event;
    memset(&event, 0, sizeof(event));
    event.events = channel->events(); // 设置关注的事件类型
    event.data.ptr = channel;         // 关联通道对象指针
    int fd = channel->fd();

    LogInfo("fd[%d] events[%d] operation [%d]\n", fd, event.events, operation);
    if (::epoll_ctl(epollfd_, operation, fd, &event) < 0)
    {
        LogError("epoll_ctl failed: %s (operation=%d)\n", strerror(errno), operation);
    }
}