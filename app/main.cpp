/**
 * @文件路径         : /shu/projects/demo-qemu-cpp/app/main.cpp
 * @作者           : 树
 * @创建时间         : 2026-05-27 17:29:50
 * @最后编辑         : 树
 * @最后编辑时间       : 2026-06-02 09:27:18
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
#include <atomic> //这是一个线程安全的布尔变量，用来控制线程是否继续运行
#include <mutex>
#include <functional>
#include <csignal>

/**
 * @brief 客户端共享运行状态。
 *
 * 该结构体用于保存 TCP 客户端当前运行状态，包括最近一次成功通信序号、
 * 底盘速度、电池电压、错误信息、连续通信失败次数、连接状态和安全模式状态。
 *
 * SharedState 通常会被通信线程写入，被状态监控线程读取。
 * 因此在多线程环境下访问该结构体时，需要配合互斥锁使用，避免数据竞争。
 */
struct SharedState
{
    int last_seq = -1; // 上一次成功通信的序号，初始值为 -1 表示还没有成功通信过
    double vx = 0.00;
    double vy = 0.00;
    double wz = 0.00;
    double battery_voltage = 0.00;
    std::string err_text = "UNKNOWN"; // 错误信息

    int fail_count = 0;
    bool connected = false;
    bool safe_mode = false;
};

std::atomic<bool> g_running{true}; // 全局运行标志，控制所有线程的运行状态

void handleSignal(int signal)
{
    g_running = false; // 收到信号时将运行标志置为 false，通知线程退出
}

std::string stripLineEnd(std::string text)
{
    if (!text.empty() && (text.back() == '\n' || text.back() == '\r'))
    {
        text.pop_back();
    }
    return text;
}
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
bool sendOnce(const AppConfig &cfg, Logger &logger, int seq, Status &status)
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

    client.setReceiveTimeout(cfg.recv_timeout_ms); // 设置接收超时时间为cfg.recv_timeout_ms ，防止 recv() 长时间阻塞

    if (!client.setReceiveTimeout(cfg.recv_timeout_ms))
    {
        return false;
    }

    // 记录发送日志，TX 表示 transmit，即发送数据
    logger.info("TX:" + stripLineEnd(cmd));

    // 发送控制命令
    if (!client.sendAll(cmd))
    {
        return false;
    }
    // 接收服务器返回内容
    std::string response;
    if (!client.receiveLine(response))
    {
        return false;
    }

    // 记录接收日志，RX 表示 receive，即接收数据
    logger.info("RX:" + stripLineEnd(response));

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
        << " ,battery_voltage=" << status.battery_voltage
        << " ,err=" << errToText(status.err);

    logger.info(oss.str());

    // err == 1 表示低电压告警
    if (status.err == 1)
    {
        std::ostringstream warn;
        warn << "low battery:" << status.battery_voltage;
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
 * @brief 周期性输出客户端运行状态的监控线程函数。
 *
 * 该函数用于在独立线程中定期读取共享状态 ShareState，
 * 并将当前连接状态、最后通信序号、速度、电池电压、错误信息、
 * 连续失败次数和安全模式状态输出到日志。
 *
 * 为了避免长时间占用共享状态锁，函数会在加锁后快速复制一份状态快照，
 * 随后在锁外完成日志拼接和输出。
 *
 * @param running 线程运行标志。为 true 时持续运行，为 false 时退出循环。
 * @param state 共享状态对象，保存客户端当前运行状态。
 * @param state_mutex 保护共享状态对象的互斥锁。
 * @param logger 日志对象，用于输出监控日志。
 */
void statusThread(std::atomic<bool> &running, SharedState &state, std::mutex &state_mutex, Logger &logger)
{
    while (running)
    {
        /*加锁
        快速拷贝共享状态
        立刻解锁
        后面打印 snapshot，不一直占用锁*/
        SharedState snapshot;
        {
            std::lock_guard<std::mutex> lock(state_mutex);
            snapshot = state; // 复制当前状态，避免长时间持锁
        }

        std::ostringstream oss;
        oss << "monitor connected=" << snapshot.connected
            << " ,last_seq=" << snapshot.last_seq
            << " ,battery_voltage=" << snapshot.battery_voltage
            << " ,err=" << snapshot.err_text
            << " ,fail_count=" << snapshot.fail_count
            << " ,safe_mode=" << snapshot.safe_mode;
        logger.info(oss.str());
        // 让当前线程休眠 1 秒
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

/**
 * @brief 通信线程函数，负责周期性与服务端进行 TCP 通信并更新共享状态。
 *
 * 该函数通常运行在独立线程中，按照配置文件中的通信周期 period_ms
 * 持续执行以下流程：
 *
 * 1. 根据当前 seq 构造并发送控制命令；
 * 2. 接收并解析服务端返回的底盘状态；
 * 3. 根据通信结果更新连续失败次数；
 * 4. 当连续失败次数达到阈值时进入安全模式；
 * 5. 通信恢复后退出安全模式；
 * 6. 将最新通信状态写入 SharedState，供监控线程读取。
 *
 * 多线程注意事项：
 * SharedState 是多个线程共享的数据，因此写入 state 时必须使用 state_mutex 加锁，
 * 避免通信线程写数据的同时，监控线程读取数据导致数据竞争。
 *
 * @param cfg 系统配置对象，包含服务器地址、端口、通信周期、最大失败次数等参数。
 * @param running 线程运行标志，为 true 时持续运行，为 false 时退出线程。
 * @param state 共享状态对象，用于保存当前连接状态、速度、电池电压、错误信息等。
 * @param state_mutex 保护共享状态对象的互斥锁。
 * @param logger 日志对象，用于记录通信状态、异常信息和安全模式切换日志。
 */
void communicationThread(const AppConfig &cfg, std::atomic<bool> &running, SharedState &state, std::mutex &state_mutex, Logger &logger)
{
    // 当前发送命令序号，每发送一轮加 1
    int seq = 0;
    // 连续通信失败次数，通信失败时递增，通信成功后清零。
    int fail_count = 0;
    // 标记之前是否发生过通信中断
    // 用于通信恢复后打印恢复日志
    bool was_disconnected = false;
    // 标记当前是否处于安全模式
    bool safe_mode = false;

    // 主循环：只要 running 为 true，通信线程就持续运行。
    while (running)
    {
        // 模拟崩溃逻辑。
        // 如果配置了 crash_after，并且当前 seq 达到阈值，
        // 则主动停止线程，用于测试 systemd、守护进程或自动重启机制。
        if (cfg.crash_after >= 0 && seq >= cfg.crash_after)
        {
            logger.error("simulate crash after seq=....");
            // 将运行标志置为 false，通知线程退出。
            running = false;
            break;
        }

        // 保存本次通信解析出的服务端状态。
        Status status;
        // 执行一次完整通信：
        // 构造命令 -> 连接服务端 -> 发送命令 -> 接收响应 -> 解析状态。
        // 如果通信成功，status 会被填充为服务端返回的底盘状态。
        const bool ok = sendOnce(cfg, logger, seq, status);

        // 本次通信失败
        if (!ok)
        {
            // 连续失败次数加 1
            fail_count++;
            // 标记发生过通信中断
            was_disconnected = true;

            // 如果连续失败次数达到配置阈值，并且当前还没有进入安全模式
            // 则进入安全模式
            if (fail_count >= cfg.max_fail_count && !safe_mode)
            {
                safe_mode = true;
                logger.error("enter safe mode");
            }
            // 更新共享状态。
            // 这里必须加锁，因为监控线程可能同时读取 state。
            {
                std::lock_guard<std::mutex> lock(state_mutex);
                // 更新连续失败次数。
                state.fail_count = fail_count;
                // 标记当前通信未连接。
                state.connected = false;
                // 更新安全模式状态。
                state.safe_mode = safe_mode;
            }
        }
        // 本次通信成功
        else
        {
            // 如果之前发生过通信中断，而本次通信成功，
            // 说明通信已经恢复。
            if (was_disconnected)
            {
                logger.info("communication recovered");
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

            // 更新共享状态。
            // 将本次解析到的服务端状态写入 SharedState，
            // 供状态监控线程 statusThread 定期读取并输出日志。
            {
                std::lock_guard<std::mutex> lock(state_mutex);
                // 更新最后一次成功通信的序号。
                state.last_seq = status.seq;

                // 更新底盘速度状态。
                state.vx = status.vx;
                state.vy = status.vy;
                state.wz = status.wz;

                // 更新电池电压。
                state.battery_voltage = status.battery_voltage;
                // 将错误码转换成可读文本后保存。
                state.err_text = errToText(status.err);
                // 通信成功，连续失败次数清零。
                state.fail_count = 0;
                // 标记当前连接正常。
                state.connected = true;
                // 通信恢复后退出安全模式。
                state.safe_mode = false;
            }
        }
        // 命令序号递增，下一轮发送新的 seq
        seq++;
        // 按配置的通信周期休眠，避免 while 循环高速空转占满 CPU。
        std::this_thread::sleep_for(std::chrono::milliseconds(cfg.period_ms));
    }
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
    std::signal(SIGINT, handleSignal);  // SIGINT  = Ctrl+C
    std::signal(SIGTERM, handleSignal); // SIGTERM = systemd stop 或 kill 默认信号
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
    // 创建共享状态对象。
    // communicationThread 会负责更新该状态，statusThread 会负责读取该状态并输出监控日志。
    SharedState state;
    std::mutex state_mutex; // 保护共享状态的互斥锁

    // 启动通信线程。
    // communicationThread 负责周期性连接服务端、发送控制命令、接收状态响应，
    // 并根据通信结果更新共享状态 state。
    //
    // 参数说明：
    // std::cref(cfg)         ：以 const 引用方式传入配置对象，避免复制且线程内不可修改 cfg。
    // std::ref(g_running)   ：以引用方式传入运行标志，用于控制线程退出。
    // std::ref(state)       ：以引用方式传入共享状态对象，通信线程会更新它。
    // std::ref(state_mutex) ：以引用方式传入互斥锁，用于保护共享状态。
    // std::ref(logger)      ：以引用方式传入日志对象，用于记录通信日志。
    std::thread comm_thread(communicationThread, std::cref(cfg), std::ref(g_running), std::ref(state), std::ref(state_mutex), std::ref(logger));

    // 启动状态监控线程。
    // statusThread 负责每隔一段时间读取共享状态 state，
    // 并将当前连接状态、速度、电池电压、错误信息、安全模式等信息输出到日志。
    //
    // 参数说明：
    // std::ref(g_running)   ：以引用方式传入运行标志，用于控制线程退出。
    // std::ref(state)       ：以引用方式传入共享状态对象，监控线程会读取它。
    // std::ref(state_mutex) ：以引用方式传入互斥锁，读取共享状态时需要加锁。
    // std::ref(logger)      ：以引用方式传入日志对象，用于输出监控日志。
    std::thread monitor_thread(statusThread, std::ref(g_running), std::ref(state), std::ref(state_mutex), std::ref(logger));

    logger.info("threads started");
    comm_thread.join();
    monitor_thread.join();
    // 记录程序启动日志
    logger.info("base client cpp started");
    // 打印当前配置，方便启动时确认配置是否正确
    printConfig(cfg);
    return 0;
}
