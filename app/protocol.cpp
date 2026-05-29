/**
 * @文件路径         : /shu/projects/demo-qemu-cpp/app/protocol.cpp
 * @作者           : 树
 * @创建时间         : 2026-05-27 17:30:51
 * @最后编辑         : 树
 * @最后编辑时间       : 2026-05-29 14:01:09
 * @Version      : V1.0.0
 * @功能描述         :
 * @Copyright    : Copyright (c) 2026 by 树, All Rights Reserved.
 */

#include "protocol.hpp"

#include <iomanip>
#include <sstream>

// 构建命令字符串,返回值类型是字符串
std::string buildCommand(const AppConfig &cfg, int seq)
{
    std::ostringstream oss;

    oss << "CMD "
        << seq << " "
        << std::fixed << std::setprecision(2)
        << cfg.vx << " "
        << cfg.vy << " "
        << cfg.wz << "\n";

    return oss.str();
}

// 解析状态数据
bool parseStatus(const std::string &text, Status &status)
{
    std::istringstream iss(text); // 创建字符串输入流

    std::string tag;                                                                                         // 保存状态消息开头的标识
    iss >> tag >> status.seq >> status.vx >> status.vy >> status.wz >> status.bettery_voltage >> status.err; // 解析整行数据

    if (!iss || tag != "STA") //! iss输入流状态异常，也就是解析失败了
    {
        return false;
    }
    return true;
}

// 把错误码 err 转换成能看懂的错误文本
std::string errToText(int err)
{
    switch (err)
    {
    case 0:
        return "OK"; // 正常，没有错误
    case 1:
        return "LOW_BATTERY"; // 低电压/低电量告警
    case 2:
        return "BAD_COMMAND"; // 错误命令
    case 3:
        return "SPEED_LIMIT"; // 发送的速度超过安全范围，服务端拒绝执行
    default:
        return "UNKNOW";
    }
}