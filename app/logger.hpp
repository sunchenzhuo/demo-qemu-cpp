/**
 * @文件路径         : /shu/projects/demo-qemu-cpp/app/logger.hpp
 * @作者           : 树
 * @创建时间         : 2026-05-27 17:31:24
 * @最后编辑         : 树
 * @最后编辑时间       : 2026-05-29 10:47:13
 * @Version      : V1.0.0
 * @功能描述         :
 * @Copyright    : Copyright (c) 2026 by 树, All Rights Reserved.
 */
#pragma once

#include <fstream>
#include <string>

/**
 * @brief 简单日志工具类。
 *
 * Logger 用于向控制台和指定日志文件输出日志信息。
 * 支持 INFO、WARN、ERROR 三种日志级别。
 *
 * 使用方式：
 * Logger logger("/tmp/app.log");
 * logger.info("system started");
 * logger.warn("network unstable");
 * logger.error("connect failed");
 */
class Logger
{
private:
    std::ofstream file_;

private:
    /**
     * @brief 获取当前时间字符串。
     *
     * @return 当前时间字符串。
     */
    std::string nowString() const;
    /**
     * @brief 统一写日志入口。
     *
     * @param level 日志级别。
     * @param msg 日志内容。
     */
    void write(const std::string &level, const std::string &msg);

public:
    /**
     * @brief 创建日志对象并打开日志文件。
     *
     * @param path 日志文件路径。
     */
    explicit Logger(const std::string &path);

    /**
     * @brief 输出 INFO 级别日志。
     *
     * @param msg 日志内容。
     */
    void info(const std::string &msg);
    /**
     * @brief 输出 WARN 级别日志。
     *
     * @param msg 日志内容。
     */
    void warn(const std::string &msg);
    /**
     * @brief 输出 ERROR 级别日志。
     *
     * @param msg 日志内容。
     */
    void error(const std::string &msg);
};
