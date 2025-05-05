#include "logger.h"
#include <sys/syscall.h>

/**
 * @class Wlogger
 * @brief 线程安全的日志记录系统，支持多级别日志输出和文件滚动存储
 */

/*----------------------------------------------------------
                    生命周期管理
----------------------------------------------------------*/

/**​
 * @brief 构造函数初始化默认配置
 * @details 初始化日志配置并创建首个日志文件
 */
Wlogger::Wlogger()
    : curFileSize_(0),
      terminalLogLevel_(0),
      fileLogLevel_(0),
      fileIndex_(0)
{
    logger_.logOutputLevelTerminal = "1,3";
    logger_.logTerminalSwitch = "on";
    init_log_config();
}
/**​
 * @brief 析构函数确保资源释放
 * @details 关闭所有打开的日志文件
 */
Wlogger::~Wlogger()
{
    if (file_.is_open())
    {
        file_.close();
    }
}

/*----------------------------------------------------------
                    主要公共接口
----------------------------------------------------------*/

/**​
 * @brief 记录日志的主入口函数
 * @param level 日志级别（DEBUG/INFO/WARNING/ERROR）
 * @param fmt 格式化字符串（兼容printf格式）
 * @note 同时支持终端和文件输出，自动添加时间戳和线程ID
 */
void Wlogger::log(LogLevel level, const char *fmt, ...)
{
    char buf[4096] = {0};
    std::string str = leverStr[static_cast<int>(level)] + get_time_tid();

    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    str += buf;
    if (str.back() != '\n')
    {
        str += '\n';
    }

    // 终端输出
    if (get_log_terminal_switch() == "on" && is_level_enabled_for_terminal(level))
    {
        ::write(STDOUT_FILENO, str.c_str(), str.size());
    }

    // 文件输出
    if (get_log_file_switch() == "on" && is_level_enabled_for_file(level))
    {
        write_log_to_file(str);
    }
}

/*----------------------------------------------------------
                    配置文件管理
----------------------------------------------------------*/

/**​
 * @brief 初始化日志系统配置
 * @details 从配置文件加载设置，处理路径创建和文件初始化
 */
void Wlogger::init_log_config()
{
    std::map<std::string, std::string *> logConfInfo = {
        {"logTerminalSwitch", &logger_.logTerminalSwitch},
        {"logOutputLevelTerminal", &logger_.logOutputLevelTerminal},
        {"logFileSwitch", &logger_.logFileSwitch},
        {"logOutputLevelFile", &logger_.logOutputLevelFile},
        {"logFilePath", &logger_.logFilePath},
        {"logMaxSize", &logger_.logMaxSize}};

    const std::string config_path = "./log/config/logConf.conf";
    std::ifstream configFile(config_path);

    if (!configFile.is_open())
    {
        std::cerr << "[" << __FILE__ << ":" << __LINE__ << "]"
                  << "[" << __FUNCTION__ << "] Error opening config file: "
                  << strerror(errno) << " (" << config_path << ")" << std::endl;
    }
    else
    {
        std::string line;
        while (std::getline(configFile, line))
        {
            if (line.empty() || line[0] == '#')
                continue;

            std::string str_copy = line;
            str_copy.erase(std::remove(str_copy.begin(), str_copy.end(), ' '), str_copy.end());

            size_t equalPos = str_copy.find('=');
            if (equalPos == std::string::npos || equalPos == 0)
            {
                std::cerr << "Invalid config line: " << str_copy << std::endl;
                continue;
            }

            std::string key = str_copy.substr(0, equalPos);
            std::string value = (equalPos < str_copy.size() - 1) ? str_copy.substr(equalPos + 1) : "";

            if (logConfInfo.find(key) != logConfInfo.end())
            {
                *(logConfInfo[key]) = value;
            }
            else
            {
                std::cerr << "Unknown config key: " << key << std::endl;
            }
        }
        configFile.close();
    }

    // 初始化文件输出
    if (logger_.logFileSwitch == "on")
    {
        open_log_file();
        try
        {
            logMaxSize_ = std::stoi(logger_.logMaxSize) * 1024 * 1024;
        }
        catch (const std::exception &e)
        {
            std::cerr << "Invalid logMaxSize, using default 10MB." << std::endl;
            logMaxSize_ = 10 * 1024 * 1024;
        }
    }

    parse_log_level_masks();
    print_config_info();
}

/*----------------------------------------------------------
                    文件操作管理
----------------------------------------------------------*/

/**​
 * @brief 创建或打开新的日志文件
 * @return int 成功返回true，失败返回false
 * @note 自动创建目录并处理文件序号递增
 */
int Wlogger::open_log_file()
{
    // 创建日志目录
    int ret = mkdir(logger_.logFilePath.c_str(), S_IRWXU | S_IRWXG | S_IXOTH);
    if (ret != 0 && errno != EEXIST)
    {
        std::cerr << "mkdir failed: " << logger_.logFilePath
                  << " Error: " << strerror(errno) << std::endl;
        return false;
    }

    // 关闭现有文件
    if (file_.is_open())
    {
        file_.close();
    }

    // 生成新文件名并打开
    std::string fileName = get_file_path_name();
    file_.open(fileName, std::ios::app);
    if (!file_.is_open())
    {
        std::cerr << "Failed to open log file: " << fileName
                  << " Error: " << strerror(errno) << std::endl;
        return false;
    }

    curFileSize_ = 0;
    fileIndex_++;
    return true;
}

/**​
 * @brief 写入日志内容到文件
 * @param buf 要写入的日志内容
 * @note 自动处理文件大小限制和滚动
 */
void Wlogger::write_log_to_file(const std::string &buf)
{
    std::lock_guard<std::mutex> locker(fileLock_);

    // 检查文件大小限制
    if (curFileSize_ + buf.size() >= logMaxSize_)
    {
        if (!open_log_file())
        {
            std::cerr << "Failed to rotate log file!" << std::endl;
            return;
        }
    }

    // 写入文件
    if (file_.is_open())
    {
        file_ << buf;
        curFileSize_ += buf.size();
        file_.flush(); // 确保写入磁盘
    }
}

/*----------------------------------------------------------
                    日志级别处理
----------------------------------------------------------*/

/**​
 * @brief 解析日志级别配置字符串
 * @param inputStr 逗号分隔的级别字符串（例如"0,1,3"）
 * @param outputMask 输出的级别掩码（按位或结果）
 * @note 有效级别范围：0-31
 */
void Wlogger::parse_log_level(const std::string &inputStr, unsigned int &outputMask)
{
    outputMask = 0;
    std::istringstream ss(inputStr);
    std::string token;

    while (std::getline(ss, token, ','))
    {
        try
        {
            int index = std::stoi(token);
            if (index >= 0 && index < 32)
            { // 确保在合法位范围内
                outputMask |= (1 << index);
            }
            else
            {
                std::cerr << "Invalid log level index: " << index << std::endl;
            }
        }
        catch (const std::exception &e)
        {
            std::cerr << "Invalid level format: " << token << std::endl;
        }
    }
}
/**​
 * @brief 解析配置中的日志级别掩码
 */
void Wlogger::parse_log_level_masks()
{
    parse_log_level(logger_.logOutputLevelTerminal, terminalLogLevel_);
    parse_log_level(logger_.logOutputLevelFile, fileLogLevel_);
}

/**​
 *@brief 检查文件输出是否启用指定级别
 *@param fileLevel 要检查的日志级别
 *@ return bool 启用返回true，否则false
 */
bool Wlogger::is_level_enabled_for_file(LogLevel fileLevel) const
{
    return (fileLogLevel_ & (1 << static_cast<int>(fileLevel))) != 0;
}

/**​
 *@brief 检查终端输出是否启用指定级别
 *@param terminalLevel 要检查的日志级别
 *@ return bool 启用返回true，否则false
 */
bool Wlogger::is_level_enabled_for_terminal(LogLevel terminalLevel) const
{
    return (terminalLogLevel_ & (1 << static_cast<int>(terminalLevel))) != 0;
}

/*----------------------------------------------------------
                    辅助工具函数
----------------------------------------------------------*/

/**​
 * @brief 生成带时间戳和线程ID的日志头
 * @return std::string 格式示例："[2023-08-01 14:30:00.123][1234]"
 */
std::string Wlogger::get_time_tid()
{
    struct timeval now;
    ::gettimeofday(&now, nullptr);

    struct tm tm_now;
    ::localtime_r(&now.tv_sec, &tm_now);

    char time_str[128] = {0};
    snprintf(time_str, sizeof(time_str),
             "[%04d-%02d-%02d %02d:%02d:%02d.%03ld][%ld]",
             tm_now.tm_year + 1900, tm_now.tm_mon + 1, tm_now.tm_mday,
             tm_now.tm_hour, tm_now.tm_min, tm_now.tm_sec,
             now.tv_usec / 1000,
             ::syscall(SYS_gettid));

    return time_str;
}

/**​
 * @brief 生成带时间戳的日志文件名
 * @return std::string 格式示例："/log/path/2023-08-01_14:30:00_0.log"
 */
std::string Wlogger::get_file_path_name() const
{
    struct timeval now;
    gettimeofday(&now, nullptr);

    struct tm tm_now;
    localtime_r(&now.tv_sec, &tm_now);

    char timeStr[128];
    snprintf(timeStr, sizeof(timeStr), "%04d%02d%02d_%02d%02d%02d",
             tm_now.tm_year + 1900, tm_now.tm_mon + 1, tm_now.tm_mday,
             tm_now.tm_hour, tm_now.tm_min, tm_now.tm_sec);

    return logger_.logFilePath + "/" + timeStr + "_" + std::to_string(fileIndex_) + ".log";
}

/*----------------------------------------------------------
                    状态输出函数
----------------------------------------------------------*/

/**​
 * @brief 打印当前配置信息到终端
 * @details 使用绿色ANSI颜色输出配置信息
 */
void Wlogger::print_config_info()
{
    const int totalWidth = logger_.logFilePath.size() + 25;

    // 顶部装饰线
    std::cout << ANSI_COLOR_GREEN;
    for (int i = 0; i < totalWidth; ++i)
    {
        std::cout << "-";
        if (i == totalWidth / 2)
            std::cout << " Wlogger Config ";
    }
    std::cout << ANSI_COLOR_RESET << std::endl;

    // 配置项输出
    auto printItem = [](const std::string &name, const std::string &value)
    {
        std::cout << ANSI_COLOR_GREEN << std::left << std::setw(20)
                  << (name + ":") << value << ANSI_COLOR_RESET << std::endl;
    };

    printItem("Terminal Switch", logger_.logTerminalSwitch);
    printItem("Terminal Levels", logger_.logOutputLevelTerminal);
    printItem("File Switch", logger_.logFileSwitch);
    printItem("File Levels", logger_.logOutputLevelFile);
    printItem("Log Path", logger_.logFilePath);
    printItem("Max Size (MB)", logger_.logMaxSize);

    // 底部装饰线
    std::cout << ANSI_COLOR_GREEN;
    for (int i = 0; i < totalWidth; ++i)
        std::cout << "-";
    std::cout << ANSI_COLOR_RESET << std::endl
              << std::endl;
}

/*----------------------------------------------------------
                    Getter 函数
----------------------------------------------------------*/

/**​
 * @brief 获取文件输出开关状态
 * @return std::string "on"表示启用，"off"表示禁用
 */
std::string Wlogger::get_log_file_switch() const
{
    return logger_.logFileSwitch;
}

/**​
 * @brief 获取终端输出开关状态
 * @return std::string "on"表示启用，"off"表示禁用
 */
std::string Wlogger::get_log_terminal_switch() const
{
    return logger_.logTerminalSwitch;
}