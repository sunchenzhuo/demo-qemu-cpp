/**
 * @文件路径         : /shu/projects/demo-qemu-cpp/app/tcp_client.hpp
 * @作者           : 树
 * @创建时间         : 2026-05-27 17:31:04
 * @最后编辑         : 树
 * @最后编辑时间       : 2026-05-29 17:33:10
 * @Version      : V1.0.0
 * @功能描述         :
 * @Copyright    : Copyright (c) 2026 by 树, All Rights Reserved.
 */

#pragma once

#include <string>

/*构造时 sock_ = -1
connectTo 时创建 socket
析构时自动 close
禁止拷贝，避免两个对象关闭同一个 fd*/
class TcpClient
{
private:
    // socket 文件描述符如果你复制一个 TCPClient，两个对象就可能共用同一个 socket。一个对象关闭了 socket，另一个对象还以为 socket 能用，就容易出 bug。
    int sock_ = -1;

public:
    TcpClient() = default;
    ~TcpClient();

    /*
    TCPClient c1;
    TCPClient c2 = c1;   // 禁止
    */
    TcpClient(const TcpClient &) = delete;
    TcpClient &operator=(const TcpClient &) = delete; // 禁止复制 TCPClient 对象

    bool connectTo(const std::string &ip, int port);
    bool sendAll(const std::string &text);
    bool receiveLine(std::string &text);
    void closeSocket();
};
