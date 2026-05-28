/**
 * @文件路径         : /shu/projects/demo-qemu-cpp/app/app_config.cpp
 * @作者           : 树
 * @创建时间         : 2026-05-27 17:30:12
 * @最后编辑         : 树
 * @最后编辑时间       : 2026-05-28 11:06:00
 * @Version      : V1.0.0
 * @功能描述         :
 * @Copyright    : Copyright (c) 2026 by 树, All Rights Reserved.
 */
#include "app_config.hpp"

#include <fstream>
#include <iostream>
#include <string>

// 去掉字符串开头和结尾的空白字符
std::string trim(const std::string &text)
{
    const std::string spaces = " \t\n\r";                     // 字符串开头和结尾如果是 tab、换行、回车，就去掉。
    const std::size_t begin = text.find_first_not_of(spaces); // 找到第一个不是空白字符的位置

    // 判断字符串是不是全是空白字符
    if (begin == std::string::npos)
    {
        return "";
    }

    const std::size_t end = text.find_last_not_of(spaces); // 找到最后一个不是空白字符的位置
    return text.substr(begin, end - begin + 1);            // 截取中间有效字符串，substr 是截取字符串
}

// 从配置文件加载应用程序配置
// 说明：此函数读取指定路径的配置文件，并解析其中的参数来覆盖 AppConfig 结构体中的默认值。
// 配置文件格式为 key=value，每行一个参数
bool loadConfig(const std::string &path, AppConfig &cfg)
{
    std::ifstream file(path); // 打开配置文件
    // 判断文件是否打开成功
    if (!file.is_open())
    {
        std::cerr << "open config failed:" << path << std::endl;
        return false;
    }

    std::string line;
    // 逐行读取配置文件
    while (std::getline(file, line))
    {
        line = trim(line);

        if (line.empty() || line[0] == '#') // 跳过空行和注释行
        {
            continue;
        }

        const std::size_t pos = line.find('='); // 查找等号=

        if (pos == std::string::npos)
        {
            continue;
        }

        // 拆出 key 和 value
        const std::string key = trim(line.substr(0, pos));
        const std::string value = trim(line.substr(pos + 1));

        if (key == "server_ip")
        {
            cfg.server_ip = value;
        }
        else if (key == "server_port")
        {
            cfg.server_port = std::stoi(value);
        }
        else if (key == "period_ms")
        {
            cfg.period_ms = std::stoi(value);
        }
        else if (key == "vx")
        {
            cfg.vx = std::stod(value);
        }
        else if (key == "vy")
        {
            cfg.vy = std::stod(value);
        }
        else if (key == "wz")
        {
            cfg.wz = std::stod(value);
        }
        else if (key == "log_file")
        {
            cfg.log_fileh = value;
        }
        else if (key == "crash_after")
        {
            cfg.crash_after = std::stoi(value);
        }
        else if (key == "max_fail_count")
        {
            cfg.max_fail_count = std::stoi(value);
        }
        else if (key == "safe_vx")
        {
            cfg.safe_vx = std::stod(value);
        }
        else if (key == "safe_vy")
        {
            cfg.safe_vy = std::stod(value);
        }
        else if (key == "safe_wz")
        {
            cfg.safe_wz = std::stod(value);
        }
    }
    return true;
}

// 打印当前配置参数
void printConfig(const AppConfig &cfg)
{
    std::cout << "server_ip:" << cfg.server_ip << std::endl;
    std::cout << "server_port:" << cfg.server_port << std::endl;
    std::cout << "period_ms:" << cfg.period_ms << std::endl;
    std::cout << "vx:" << cfg.vx << std::endl;
    std::cout << "vy:" << cfg.vy << std::endl;
    std::cout << "wz:" << cfg.wz << std::endl;
    std::cout << "log_file:" << cfg.log_fileh << std::endl;
    std::cout << "crash_after:" << cfg.crash_after << std::endl;
    std::cout << "max_fail_count:" << cfg.max_fail_count << std::endl;
    std::cout << "safe_vx:" << cfg.safe_vx << std::endl;
    std::cout << "safe_vy:" << cfg.safe_vy << std::endl;
    std::cout << "safe_wz:" << cfg.safe_wz << std::endl;
}