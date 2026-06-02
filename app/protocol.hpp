/**
 * @文件路径         : /shu/projects/demo-qemu-cpp/app/protocol.hpp
 * @作者           : 树
 * @创建时间         : 2026-05-27 17:30:25
 * @最后编辑         : 树
 * @最后编辑时间       : 2026-06-02 11:52:45
 * @Version      : V1.0.0
 * @功能描述         : TCP 客户端通信协议定义文件。用于定义运动控制命令 MotionCommand、服务端状态响应 Status，以及命令构建、状态解析、错误码转换等协议相关接口。
 * @Copyright    : Copyright (c) 2026 by 树, All Rights Reserved.
 */
#pragma once

#include "app_config.hpp"

#include <string>

// 定义状态数据结构体
struct Status
{
    int seq = -1; // 表示还没有收到有效序号
    double vx = 0.00;
    double vy = 0.00;
    double wz = 0.00;
    double battery_voltage = 0.00; // 电池电压，单位：V
    int err = -1;
};

struct MotionCommand
{
    double vx = 0.0;
    double vy = 0.0;
    double wz = 0.0;
};

// 构建命令字符串,返回值类型是字符串
std::string buildCommand(const MotionCommand &cmd, int seq);
// 解析状态数据
bool parseStatus(const std::string &text, Status &status);
// 把错误码 err 转换成能看懂的错误文本
std::string errToText(int err);