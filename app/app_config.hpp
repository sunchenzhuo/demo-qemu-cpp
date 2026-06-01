/**
 * @文件路径         : /shu/projects/demo-qemu-cpp/app/app_config.hpp
 * @作者           : 树
 * @创建时间         : 2026-05-27 17:30:01
 * @最后编辑         : 树
 * @最后编辑时间       : 2026-06-01 15:29:20
 * @Version      : V1.0.0
 * @功能描述         :
 * @Copyright    : Copyright (c) 2026 by 树, All Rights Reserved.
 */
#pragma once

#include <string>

// 应用程序配置结构体
// 说明：此结构体保存客户端运行时所需的可配置参数及其默认值。
// 可由配置文件或命令行解析器覆盖这些默认值。
struct AppConfig
{
    std::string server_ip = "127.0.0.1"; // 服务器 IP，默认指向主机
    int server_port = 17000;             // 服务器端口
    int period_ms = 2000;                // 周期（毫秒）：客户端发送/轮询的时间间隔
    int recv_timeout_ms = 1000;          // TCP 接收超时时间，单位毫秒

    // 运动控制量（线速度 vx, vy，角速度 wz）
    // 这些值可能用于发送给远端控制器或模拟移动行为
    double vx = 0.30; // 前向速度（m/s）
    double vy = 0.00; // 侧向速度（m/s），默认无侧向运动
    double wz = 0.20; // 角速度（rad/s）

    std::string log_file = "/var/log/client_log.log"; // 日志文件路径，客户端将日志写入此文件（需要有写权限）
    int crash_after = -1;                             // 在多少次循环后模拟崩溃。-1 表示不触发崩溃。
    int max_fail_count = 3;                           // 最大失败重试次数（例如网络发送失败）

    // 安全速度：在出现故障或进入安全模式时使用的速度（通常设为 0）
    double safe_vx = 0.00;
    double safe_vy = 0.00;
    double safe_wz = 0.00;
};
// 去掉字符串开头和结尾的空白字符
std::string trim(const std::string &text);
// 从配置文件加载应用程序配置
bool loadConfig(const std::string &path, AppConfig &cfg);
// 打印当前配置参数
void printConfig(const AppConfig &cfg);