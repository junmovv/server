#include "CurrentThread.h"

/**
 * @file CurrentThread.h
 * @brief 当前线程信息获取工具
 */

/**
 * @namespace CurrentThread
 * @brief 封装当前线程相关信息的获取和缓存
 * @note 特性：
 *       - 使用线程局部存储缓存线程特定信息
 *       - 避免频繁系统调用带来的性能开销
 *       - 提供轻量级的线程信息访问接口
 */
namespace CurrentThread
{
    /**
     * @var __thread int t_cachedTid
     * @brief 线程局部缓存的线程ID
     * @note 使用GCC的__thread关键字实现线程局部存储
     *       首次访问时通过syscall获取并缓存
     */
    __thread int t_cachedTid = 0;

    /**
     * @var __thread const char* t_threadName
     * @brief 线程局部缓存的线程名称
     * @note 默认值为"unknown"，需外部显式设置
     *       常用于调试日志和性能分析
     */
    __thread const char *t_threadName = "unknown";

    /**
     * @brief 缓存当前线程ID到线程局部存储
     * @note 实现细节：
     *       1. 使用Linux系统调用SYS_gettid直接获取内核级线程ID
     *       2. 相比pthread_self()更高效且唯一性保证
     *       3. 缓存机制避免重复系统调用开销
     * @warning 必须在需要获取TID的线程至少调用一次
     */
    void cacheTid()
    {
        if (t_cachedTid == 0)
        {
            // 直接通过系统调用获取线程ID（避免pthread的包装开销）
            t_cachedTid = static_cast<pid_t>(::syscall(SYS_gettid));
        }
    }

} // namespace CurrentThread
