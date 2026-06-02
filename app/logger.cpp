/**
 * @文件路径         : /shu/projects/demo-qemu-cpp/app/logger.cpp
 * @作者           : 树
 * @创建时间         : 2026-05-27 17:31:30
 * @最后编辑         : 树
 * @最后编辑时间       : 2026-06-02 14:10:36
 * @Version      : V1.0.0
 * @功能描述         :
 * @Copyright    : Copyright (c) 2026 by 树, All Rights Reserved.
 */
#include "logger.hpp"

#include <chrono>   // 时间库，用于获取当前系统时间
#include <ctime>    // C 风格时间库，用于 time_t、tm 等时间结构
#include <iomanip>  // 用于 std::put_time 格式化时间
#include <iostream> // 用于 std::cout、std::cerr 输出到终端
#include <sstream>  // 用于 std::ostringstream 拼接字符串
#include <mutex>    // 用于 std::mutex 保护日志文件写入的线程安全

/**
 * @brief 创建日志对象并打开日志文件。
 *
 * 构造 Logger 对象时，会根据传入的文件路径打开日志文件。
 * 日志文件采用追加模式打开，新的日志内容会写入文件末尾，
 * 不会覆盖已有日志。
 *
 * 如果日志文件打开失败，会向标准错误输出错误信息。
 *
 * @param path 日志文件路径。
 */
Logger::Logger(const std::string &path)
{
    // 以追加模式打开日志文件
    // std::ios::app 表示 append，新的日志会追加到文件末尾，不会覆盖旧日志
    file_.open(path, std::ios::app);
    if (!file_.is_open())
    {
        // 如果打开失败，向标准错误输出错误信息
        std::cerr << "open log file failed" << path << std::endl;
    }
}

/**
 * @brief 输出 INFO 级别日志。
 *
 * 用于记录系统正常运行过程中的关键状态信息，例如程序启动、
 * 配置加载成功、连接服务器成功、任务执行完成等。
 *
 * @param msg 日志内容。
 */
void Logger::info(const std::string &msg)
{
    write("INFO", msg);
}

/**
 * @brief 输出 WARN 级别日志。
 *
 * 用于记录程序运行过程中的异常风险或非致命问题。
 * 例如：配置项缺失但使用默认值、网络短暂异常、服务响应较慢等。
 *
 * @param msg 日志内容。
 */
void Logger::warn(const std::string &msg)
{
    write("WARN", msg);
}

/**
 * @brief 输出 ERROR 级别日志。
 *
 * 用于记录程序运行过程中的错误信息。
 * 例如：文件打开失败、连接服务器失败、数据发送失败、系统调用异常等。
 *
 * @param msg 日志内容。
 */
void Logger::error(const std::string &msg)
{
    write("ERROR", msg);
}

/**
 * @brief 获取当前本地时间字符串。
 *
 * 通过系统时钟获取当前时间，并转换成本地时间格式。
 * 当前格式为：yy-MM-dd HH:mm:ss。
 *
 * 例如：
 * 26-05-29 10:30:15
 *
 * @return 格式化后的当前时间字符串。
 */
std::string Logger::nowString() const
{
    // 获取当前系统时间点
    const auto now = std::chrono::system_clock::now();
    // 将 C++ 时间点转换为 C 风格的 time_t 时间
    const std::time_t t = std::chrono::system_clock::to_time_t(now);

    // 定义 tm 结构体，用于保存本地时间的年月日时分秒
    std::tm tm{};
    // 将 time_t 转换成本地时间
    // localtime_r 是线程安全版本，比 localtime 更适合工程项目
    localtime_r(&t, &tm);

    // 使用字符串流拼接格式化后的时间
    std::ostringstream oss;
    // 格式化时间
    // %y：两位年份，例如 26
    // %m：月份
    // %d：日期
    // %H：小时，24 小时制
    // %M：分钟
    // %S：秒
    oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
    // 返回格式化后的时间字符串
    return oss.str();
}

/**
 * @brief 写入一条日志。
 *
 * 该函数是日志输出的统一入口，负责将时间、日志级别和日志内容
 * 拼接成一行完整日志。
 *
 * 日志会同时输出到控制台和日志文件。
 * 如果日志文件没有成功打开，则只输出到控制台。
 *
 * @param level 日志级别，例如 INFO、WARN、ERROR。
 * @param msg 日志内容。
 */
void Logger::write(const std::string &level, const std::string &msg)
{
    std::lock_guard<std::mutex> lock(mutex_); // 加锁保护日志写入，确保多线程环境下日志不会混乱
    // 拼接一整行日志
    // 例如：26-05-29 10:20:30[INFO]server started
    const std::string line = nowString() + " [" + level + "] " + msg;
    // 先输出到终端，方便调试时直接看到日志
    std::cout << line << std::endl;

    // 如果日志文件已经成功打开，则同时写入日志文件
    if (file_.is_open())
    {
        // 写入文件并换行
        file_ << line << std::endl;
    }
}