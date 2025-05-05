#ifndef LOGGING_H
#define LOGGING_H

#include <iostream>
#include <string>
#include <map>
#include <fstream>
#include <algorithm>
#include <ios>
#include <iomanip>
#include <mutex>
#include <queue>

#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <string.h>
// 文字颜色
#define ANSI_COLOR_RESET "\e[0m"
#define ANSI_COLOR_BLACK "\e[30m"
#define ANSI_COLOR_RED "\e[31m"
#define ANSI_COLOR_GREEN "\e[32m"
#define ANSI_COLOR_YELLOW "\e[33m"
#define ANSI_COLOR_BLUE "\e[34m"
#define ANSI_COLOR_MAGENTA "\e[35m"
#define ANSI_COLOR_CYAN "\e[36m"
#define ANSI_COLOR_WHITE "\e[37m"
enum class LogLevel : unsigned char
{
    DEBUG,
    INFO,
    WARN,
    ERROR,
    FATAL
};

static const char *leverStr[5] =
    {"[DEBUG]", "[INFO] ", "[WARN] ", "[ERROR]", "[FATAL]"};

struct Logger
{
    std::string logTerminalSwitch;      // 是否打印到终端
    std::string logOutputLevelTerminal; // 日志输出等级（终端）

    std::string logFileSwitch;      // 是否写入文件
    std::string logOutputLevelFile; // 日志输出等级(文件)
    std::string logFilePath;        // 日志文件保存路径
    std::string logMaxSize;         // 日志文件最大大小
};

class Wlogger
{
public:
    void log(LogLevel level, const char *fmt, ...);
    static Wlogger *instance()
    {
        static Wlogger singleObject;
        return &singleObject;
    }

private:
    void init_log_config();
    void print_config_info();

    std::string get_log_file_switch() const;
    std::string get_log_terminal_switch() const;

    std::string get_file_path_name() const;
    int open_log_file();
    void parse_log_level(const std::string &inputStr, unsigned int &outputMask);
    void parse_log_level_masks();
    bool is_level_enabled_for_file(LogLevel fileLevel) const;
    bool is_level_enabled_for_terminal(LogLevel terminalLevel) const;

    void write_log_to_file(const std::string &buf);

    std::string get_time_tid();

private:
    std::mutex fileLock_;
    Logger logger_;

    std::ofstream file_;
    size_t curFileSize_;
    size_t logMaxSize_;
    size_t fileIndex_;
    bool isUseFile_;
    unsigned int fileLogLevel_;
    unsigned int terminalLogLevel_;

private:
    Wlogger();
    ~Wlogger();
    Wlogger(const Wlogger &other) = delete;
    Wlogger &operator=(const Wlogger &other) = delete;
};

#define LogerCout(level, fmt, ...) Wlogger::instance()->log(level, "[%s:%d][%s] " fmt, __FILE__, __LINE__, __FUNCTION__, ##__VA_ARGS__)

#define LogDebug(fmt, ...)                              \
    do                                                  \
    {                                                   \
        LogerCout(LogLevel::DEBUG, fmt, ##__VA_ARGS__); \
    } while (0)

#define LogInfo(fmt, args...)                   \
    do                                          \
    {                                           \
        LogerCout(LogLevel::INFO, fmt, ##args); \
    } while (0)

#define LogWarn(fmt, args...)                   \
    do                                          \
    {                                           \
        LogerCout(LogLevel::WARN, fmt, ##args); \
    } while (0)

#define LogError(fmt, args...)                   \
    do                                           \
    {                                            \
        LogerCout(LogLevel::ERROR, fmt, ##args); \
    } while (0)

#define LogFatal(fmt, ...)                              \
    do                                                  \
    {                                                   \
        LogerCout(LogLevel::FATAL, fmt, ##__VA_ARGS__); \
        abort();                                        \
    } while (0)
#endif