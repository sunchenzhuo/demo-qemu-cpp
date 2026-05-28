/**
 * @文件路径         : /shu/projects/demo-qemu-cpp/app/tcp_client.cpp
 * @作者           : 树
 * @创建时间         : 2026-05-27 17:31:14
 * @最后编辑         : 树
 * @最后编辑时间       : 2026-05-28 17:59:03
 * @Version      : V1.0.0
 * @功能描述         :
 * @Copyright    : Copyright (c) 2026 by 树, All Rights Reserved.
 */

#include "tcp_client.hpp"

#include <arpa/inet.h>
#include <cstring>
#include <iostream>
#include <sys/socket.h>
#include <unistd.h>

// 析构函数：当 TcpClient 对象销毁时会自动调用
TcpClient::~TcpClient()
{
    // 对象销毁前，主动关闭 socket，避免资源泄漏
    closeSocket();
}

// 连接 TCP 服务器
bool TcpClient::connectTo(const std::string &ip, int port)
{
    // 连接新服务器之前，先关闭旧连接
    // 防止重复连接导致 socket 资源泄漏
    closeSocket();
    // 创建 socket
    // AF_INET      ：使用 IPv4
    // SOCK_STREAM  ：使用 TCP 协议
    // 0            ：让系统自动选择默认协议
    sock_ = socket(AF_INET, SOCK_STREAM, 0);
    // socket 创建失败时，socket() 返回 -1
    // 注意：这里建议判断 sock_ < 0，而不是 !sock
    if (!sock_)
    {
        std::cerr << "socket failed:" << std::endl;
        return false;
    }

    sockaddr_in server_addr{};        // 创建 socket地址结构体，存储服务器的 IP 和端口信息
    server_addr.sin_family = AF_INET; // 设置地址族为 IPv4
    // 设置服务器端口
    // htons 的作用：把主机字节序转换为网络字节序
    server_addr.sin_port = htons(port);

    // 将字符串形式的 IP 地址转换为网络二进制格式
    // 例如把 "192.168.1.10" 转成 sockaddr_in 能识别的格式
    if (inet_pton(AF_INET, ip.c_str(), &server_addr.sin_addr) != 1)
    {
        std::cerr << "bad server ip:" << ip << std::endl;
        // IP 格式错误，关闭已经创建的 socket
        closeSocket();
        return false;
    }

    // 发起 TCP 连接
    // connect 需要 sockaddr* 类型，所以这里使用 reinterpret_cast 强制转换
    if (connect(sock_, reinterpret_cast<sockaddr *>(&server_addr), sizeof(server_addr)) < 0)
    {
        std::cerr << "connect faild:" << ip << ":" << port << std::endl;
        // 连接失败，关闭 socket
        closeSocket();
        return false;
    }
    // 连接成功
    return true;
}

// 发送字符串数据
bool TcpClient::sendText(const std::string &text)
{
    // sock_ < 0 表示当前没有可用连接
    if (sock_ < 0)
    {
        std::cerr << "socket not connected" << std::endl;
        return false;
    }

    // 通过 TCP socket 发送数据
    // text.c_str() ：字符串数据地址
    // text.size()  ：要发送的数据长度
    // 0            ：默认发送方式
    const ssize_t n = send(sock_, text.c_str(), text.size(), 0);
    // send 返回值小于 0，表示发送失败
    if (n < 0)
    {
        std::cerr << "send failed" << std::endl;
        return false;
    }
    // send 返回的是实际发送的字节数
    // 如果实际发送字节数小于 text.size()，说明只发送了一部分
    if (static_cast<std::size_t>(n) != text.size())
    {
        std::cerr << "partial send" << std::endl;
        return false;
    }
    // 数据完整发送成功
    return true;
}

// 接收服务器返回的字符串数据
bool TcpClient::receiveText(std::string &text)
{
    // 判断当前 socket 是否已经连接
    if (sock_ < 0)
    {
        std::cerr << "socket not connected" << std::endl;
        return false;
    }

    // 定义接收缓冲区，大小为 256 字节
    // {} 表示全部初始化为 0
    char buffer[256]{};
    // 从 socket 接收数据
    // sizeof(buffer) - 1 是为了预留一个位置放 '\0'
    const ssize_t n = recv(sock_, buffer, sizeof(buffer) - 1, 0);

    // n < 0 ：接收失败
    // n == 0：服务器关闭了连接
    if (n <= 0)
    {
        std::cerr << "recv failed or server closed" << std::endl;
        return false;
    }
    // 手动补字符串结束符
    // 这样 buffer 才能安全地当 C 风格字符串使用
    buffer[n] = '\0';
    // 把接收到的内容赋值给外部传入的 text
    text = buffer;

    return true;
}

// 关闭 socket 连接
void TcpClient::closeSocket()
{
    // sock_ >= 0 表示 socket 是有效的
    if (sock_ >= 0)
    {
        // 关闭 socket 文件描述符
        close(sock_);
        // 关闭后把 sock_ 重置为 -1
        // 表示当前没有连接
        sock_ = -1;
    }
}