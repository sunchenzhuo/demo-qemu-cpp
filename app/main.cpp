/**
 * @文件路径         : /shu/projects/demo-qemu-cpp/app/main.cpp
 * @作者           : 树
 * @创建时间         : 2026-05-27 17:29:50
 * @最后编辑         : 树
 * @最后编辑时间       : 2026-05-29 10:52:26
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

int main(int argc, char const *argv[])
{
    std::string config_path = "./base-client-test.conf"; // 创建配置对象
    if (argc > 1)
    {
        config_path = argv[1];
    }

    AppConfig cfg;
    // 配置文件失败就直接退出
    if (!loadConfig(config_path, cfg))
    {
        std::cerr << "bad config fail:" << config_path << std::endl;
        return 1;
    }

    printConfig(cfg);

    const int seq = 0;
    const std::string cmd = buildCommand(cfg, 0);

    TcpClient client;

    if (!client.connectTo(cfg.server_ip, cfg.server_port))
    {
        return 1;
    }

    std::cout << "TX:" << cmd;

    if (!client.sendText(cmd))
    {
        return 1;
    }

    std::string response;
    if (!client.receiveText(response))
    {
        return 1;
    }

    std::cout << "RX:" << response;

    Status status;
    Logger logger(cfg.log_file);
    logger.info("base client cpp started");

    if (!parseStatus(response, status))
    {
        std::cerr << "bad response format" << std::endl;
        return 1;
    }
    std::ostringstream oss;
    oss << "STATUS vx=" << status.vx
        << ",vy=" << status.vy
        << ",wz=" << status.wz
        << ",battery_voltage=" << status.better_voltage
        << ",err=" << errToText(status.err)
        << std::endl;
    logger.info(oss.str());

    return 0;
}
