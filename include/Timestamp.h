#ifndef TIMESTAMP_H
#define TIMESTAMP_H
#include <sys/time.h>
#include <stdio.h>
#include <string>

class Timestamp
{

public:
    // 默认构造函数，将微秒数初始化为 0
    Timestamp();

    // 带参数的构造函数，使用传入的微秒数初始化
    Timestamp(int64_t microSecondsSinceEpoch);

    // 获取当前时间的静态方法
    static Timestamp now();

    // 将时间戳转换为时间格式的方法
    std::string to_formatted_string(bool showMicroseconds = false) const;

private:
    static const int kMicroSecondsPerSecond = 1000 * 1000;
    int64_t microSecondsSinceEpoch_;
};
#endif