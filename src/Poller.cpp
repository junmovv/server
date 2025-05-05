#include "Poller.h"
#include "Channel.h"
#include "EPollPoller.h"
/**
 * @class Poller
 * @brief 跨平台的IO多路复用器抽象基类
 * @note 提供统一的事件监听接口，具体实现由子类完成（如EPollPoller）
 */

/**
 * @brief 构造函数，绑定所属事件循环
 * @param loop 关联的EventLoop对象指针
 * @note 初始化时尚未管理任何Channel通道
 */
Poller::Poller(EventLoop *loop) : loop_(loop) {}

/**
 * @brief 虚析构函数（默认实现）
 * @note 保证派生类对象的正确析构，资源清理由具体实现类负责
 */
Poller::~Poller() = default;

/**
 * @brief 创建平台默认的Poller实现
 * @param loop 关联的EventLoop对象指针
 * @return 返回新建的Poller对象指针（调用方拥有所有权）
 * @note 当前默认返回EPollPoller实现（Linux平台最优选择）
 *       未来可扩展为根据环境变量选择不同实现
 */
Poller *Poller::new_default_poller(EventLoop *loop)
{
    return new EPollPoller(loop);
}

/**
 * @brief 检查通道是否已被本Poller监控
 * @param channel 要检查的Channel对象指针
 * @return 当且仅当满足以下条件时返回true：
 *         - 通道的文件描述符存在于注册表中
 *         - 对应的Channel指针与参数一致
 * @note 使用文件描述符作为键进行查找，时间复杂度O(logN)
 */
bool Poller::has_channel(Channel *channel) const
{
    auto it = channels_.find(channel->fd());
    return it != channels_.end() && it->second == channel;
}