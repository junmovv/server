/** @file Timestamp.cpp */
/** @brief 时间戳工具类实现文件 */

#include "Timestamp.h"
#include <sys/time.h>
#include <stdio.h>

/**
 • @brief 默认构造函数
 • @note 初始化时间为纪元起始点（1970-01-01 00:00:00）
 */
Timestamp::Timestamp()
    : microSecondsSinceEpoch_(0) {}

/**
 • @brief 带参构造函数
 • @param microSecondsSinceEpoch 自纪元起经过的微秒数
 */
Timestamp::Timestamp(int64_t microSecondsSinceEpoch)
    : microSecondsSinceEpoch_(microSecondsSinceEpoch) {}

/**
 • @brief 获取当前时间戳
 • @return 包含当前时间的Timestamp对象
 • @note 使用gettimeofday系统调用，精度可达微秒级
 */
Timestamp Timestamp::now()
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    int64_t seconds = tv.tv_sec;
    return Timestamp(seconds * kMicroSecondsPerSecond + tv.tv_usec);
}

/**
 • @brief 格式化时间字符串
 • @param showMicroseconds 是否显示微秒级精度
 • @return 格式化后的时间字符串
 • @retval 格式1: "YYYYMMDD HH:MM:SS.ffffff"（微秒模式）
 • @retval 格式2: "YYYYMMDD HH:MM:SS"（秒级模式）
 • @note 使用线程安全的localtime_r函数转换本地时间
 */
std::string Timestamp::to_formatted_string(bool showMicroseconds) const
{
    char buf[64] = {0};
    time_t seconds = static_cast<time_t>(microSecondsSinceEpoch_ / kMicroSecondsPerSecond);
    struct tm tm_time;
    localtime_r(&seconds, &tm_time); // 线程安全的时间转换

    if (showMicroseconds)
    {
        int microseconds = static_cast<int>(microSecondsSinceEpoch_ % kMicroSecondsPerSecond);
        snprintf(buf, sizeof(buf), "%04d%02d%02d %02d:%02d:%02d.%06d",
                 tm_time.tm_year + 1900, // 年份从1900算起
                 tm_time.tm_mon + 1,     // 月份0-11转为1-12
                 tm_time.tm_mday,
                 tm_time.tm_hour,
                 tm_time.tm_min,
                 tm_time.tm_sec,
                 microseconds);
    }
    else
    {
        snprintf(buf, sizeof(buf), "%04d%02d%02d %02d:%02d:%02d",
                 tm_time.tm_year + 1900,
                 tm_time.tm_mon + 1,
                 tm_time.tm_mday,
                 tm_time.tm_hour,
                 tm_time.tm_min,
                 tm_time.tm_sec);
    }
    return buf;
}
