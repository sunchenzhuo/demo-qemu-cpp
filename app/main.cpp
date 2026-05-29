/**
 * @文件路径         : /shu/projects/demo-qemu-cpp/app/main.cpp
 * @作者           : 树
 * @创建时间         : 2026-05-27 17:29:50
 * @最后编辑         : 树
 * @最后编辑时间       : 2026-05-29 14:00:36
 * @Version      : V1.0.0
 * @功能描述         :
 * @Copyright    : Copyright (c) 2026 by 树, All Rights Reserved.
 */

#include "app_config.hpp"
#include "protocol.hpp"
#include "tcp_client.hpp"
#include "logger.hpp"

#include <string>
#include <iostream>
#include <sstream>
#include <chrono>
#include <thread>

/**
 * @brief 执行一次完整的客户端通信流程。
 *
 * 该函数负责根据当前配置和序号构造一条控制命令，
 * 然后创建 TCP 客户端连接服务器，发送命令并接收服务器响应。
 *
 * 响应内容会被解析为底盘状态数据，并根据返回状态进行日志记录、
 * 序号校验、低电压告警和底盘错误处理。
 *
 * 一次通信流程包括：
 * 1. 构造发送命令
 * 2. 建立 TCP 连接
 * 3. 发送命令
 * 4. 接收响应
 * 5. 解析状态
 * 6. 判断异常状态
 *
 * @param cfg 系统配置对象，包含服务器 IP、端口、日志路径、周期等参数。
 * @param logger 日志对象，用于记录发送、接收和异常信息。
 * @param seq 当前命令序号，用于请求和响应匹配校验。
 * @return true 本次通信成功；false 本次通信失败。
 */
bool sendOnce(const AppConfig &cfg, Logger &logger, int seq)
{
    // 根据配置和当前序号构造一条控制命令
    const std::string cmd = buildCommand(cfg, seq);

    // 创建 TCP 客户端对象
    // 该对象生命周期只在本次 sendOnce 内有效
    // 函数结束时会自动析构并关闭 socket
    TcpClient client;

    // 连接服务器，如果连接失败，直接返回 false
    if (!client.connectTo(cfg.server_ip, cfg.server_port))
    {
        return false;
    }
    // 记录发送日志，TX 表示 transmit，即发送数据
    logger.info("TX:" + cmd);

    // 发送控制命令
    if (!client.sendText(cmd))
    {
        return false;
    }
    // 接收服务器返回内容
    std::string response;
    if (!client.receiveText(response))
    {
        return false;
    }

    // 记录接收日志，RX 表示 receive，即接收数据
    logger.info("RX:" + response);

    // 定义状态对象，用于保存解析后的响应结果
    Status status;

    // 解析服务器返回的状态字符串
    // 如果格式不符合预期，记录告警并返回失败
    if (!parseStatus(response, status))
    {
        logger.warn("bad response format");
        return false;
    }
    // 校验响应序号是否与发送序号一致
    // 如果不一致，说明可能出现了响应错乱、延迟包或服务端异常
    if (status.seq != seq)
    {
        std::ostringstream oss;
        oss << "seq mismatch,tx_seq=" << seq << ",rx_seq=" << status.seq;
        logger.warn(oss.str());
    }
    // 拼接并记录底盘状态信息
    // 包括线速度 vx、vy，角速度 wz，电池电压和错误码
    std::ostringstream oss;
    oss << "status vx=" << status.vx
        << " ,vy=" << status.vy
        << " ,wz=" << status.wz
        << " ,battery_voltage=" << status.bettery_voltage
        << " ,err=" << errToText(status.err);

    logger.warn(oss.str());

    // err == 1 表示低电压告警
    if (status.err == 1)
    {
        std::ostringstream warn;
        warn << "low battery:" << status.bettery_voltage;
        logger.warn(warn.str());
    }
    // err != 0 表示存在其他底盘错误
    else if (status.err != 0)
    {
        logger.error("chassis error:" + errToText(status.err));
    }
    // 本次通信流程完成
    return true;
}
/**
 * @brief 程序入口函数。
 *
 * 主函数负责完成客户端启动流程，包括：
 * 1. 读取配置文件路径
 * 2. 加载配置文件
 * 3. 初始化日志系统
 * 4. 按固定周期执行 TCP 通信
 * 5. 统计连续通信失败次数
 * 6. 在连续失败过多时进入安全模式
 * 7. 通信恢复后退出安全模式
 *
 * 程序默认读取当前目录下的 base-client-test.conf。
 * 如果启动时传入命令行参数，则使用第一个参数作为配置文件路径。
 *
 * @param argc 命令行参数数量。
 * @param argv 命令行参数数组。
 * @return 0 正常退出；1 配置加载失败；2 模拟崩溃退出。
 */
int main(int argc, char const *argv[])
{
    // 默认配置文件路径
    std::string config_path = "./base-client-test.conf";
    // 如果命令行传入了配置文件路径，则优先使用命令行参数
    if (argc > 1)
    {
        config_path = argv[1];
    }

    // 创建配置对象，用于保存配置文件中的参数
    AppConfig cfg;

    // 加载配置文件
    // 如果加载失败，说明配置文件不存在、格式错误或关键参数缺失
    if (!loadConfig(config_path, cfg))
    {
        std::cerr << "bad config fail:" << config_path << std::endl;
        return 1;
    }

    // 根据配置文件中的日志路径创建日志对象
    Logger logger(cfg.log_file);
    // 记录程序启动日志
    logger.info("base client cpp started");
    // 打印当前配置，方便启动时确认配置是否正确
    printConfig(cfg);

    // 当前发送命令序号，每发送一轮加 1
    int seq = 0;
    // 连续通信失败次数
    int fail_count = 0;
    // 标记之前是否发生过通信中断
    // 用于通信恢复后打印恢复日志
    bool was_disconnected = false;
    // 标记当前是否处于安全模式
    bool safe_mode = false;

    // 主循环：按照配置周期持续执行通信任务
    while (true)
    {
        // 模拟崩溃逻辑
        // 如果配置了 crash_after，并且当前序号达到阈值，则主动退出程序
        // 主要用于测试 systemd、守护进程或重启机制
        if (cfg.crash_after >= 0 && seq >= cfg.crash_after)
        {
            std::ostringstream oss;
            oss << "simulate crash after seq=" << seq;
            logger.error(oss.str());
            return 2;
        }
        // 执行一次完整通信
        const bool ok = sendOnce(cfg, logger, seq);

        // 本次通信失败
        if (!ok)
        {
            // 连续失败次数加 1
            fail_count++;
            // 标记发生过通信中断
            was_disconnected = true;

            // 记录通信失败日志
            std::ostringstream oss;
            oss << "communication failed,seq=" << seq
                << ",fail_count=" << fail_count
                << ",will retry";
            logger.warn(oss.str());
            // 如果连续失败次数达到配置阈值，并且当前还没有进入安全模式
            // 则进入安全模式
            if (fail_count >= cfg.max_fail_count && !safe_mode)
            {
                safe_mode = true;

                std::ostringstream safe_msg;
                safe_msg << "too many communication failures,enter safe mode,safe_cmd="
                         << cfg.safe_vx << " "
                         << cfg.safe_vy << " "
                         << cfg.safe_wz;
                logger.error(safe_msg.str());
            }
        }
        // 本次通信成功
        else
        {
            // 如果之前发生过通信中断，则说明当前通信已经恢复
            if (was_disconnected)
            {
                std::ostringstream oss;
                oss << "communication recovered at seq=" << seq
                    << " after " << fail_count << " failures";
                logger.info(oss.str());
            }

            // 如果之前处于安全模式，通信恢复后退出安全模式
            if (safe_mode)
            {
                logger.info("leave safe mode");
            }

            // 通信成功后，清空失败计数和异常状态
            fail_count = 0;
            was_disconnected = false;
            safe_mode = false;
        }
        // 命令序号递增，下一轮发送新的 seq
        seq++;
        // 按配置周期休眠，避免无限高速循环
        std::this_thread::sleep_for(std::chrono::milliseconds(cfg.period_ms));
    }

    return 0;
}
