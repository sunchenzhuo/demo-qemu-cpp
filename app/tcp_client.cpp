/**
 * @文件路径         : /shu/projects/demo-qemu-cpp/app/tcp_client.cpp
 * @作者           : 树
 * @创建时间         : 2026-05-27 17:31:14
 * @最后编辑         : 树
 * @最后编辑时间       : 2026-06-01 14:59:51
 * @Version      : V1.0.0
 * @功能描述         :这个 TcpClient 类是对 Linux TCP socket 的一个简单封装。内部通过 sock_ 保存 socket 文件描述符，初始化为 -1 表示未连接。connectTo() 负责创建 socket、配置服务器地址并发起连接；sendText() 负责通过 send() 发送字符串数据；receiveText() 通过 recv() 接收服务器返回内容；closeSocket() 负责关闭连接并重置状态。析构函数中调用 closeSocket()，是为了保证对象销毁时自动释放 socket 资源，避免文件描述符泄漏。同时禁用拷贝构造和赋值操作，是为了防止多个对象持有同一个 socket 文件描述符，导致重复关闭或资源状态混乱。
 * @Copyright    : Copyright (c) 2026 by 树, All Rights Reserved.
 */

#include "tcp_client.hpp"

#include <arpa/inet.h>
#include <cstring>
#include <iostream>
#include <sys/socket.h>
#include <unistd.h>
#include <sys/time.h>
#include <cerrno>
#include <cstring>

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
        std::cerr << "connect failed:" << ip << ":" << port << std::endl;
        // 连接失败，关闭 socket
        closeSocket();
        return false;
    }
    // 连接成功
    return true;
}

/**
 * @brief 设置 TCP 接收超时时间。
 *
 * 该函数用于设置当前 socket 的接收超时时间，避免 recv() 在服务端无响应
 * 或网络异常时长时间阻塞。设置成功后，如果在指定时间内没有接收到数据，
 * recv() 会返回失败，调用方可以据此执行重试、告警或安全模式逻辑。
 *
 * @param timeout_ms 接收超时时间，单位为毫秒。
 * @return true 超时时间设置成功。
 * @return false socket 未连接或设置失败。
 */
bool TcpClient::setReceiveTimeout(int timeout_ms)
{
    if (sock_ < 0)
    {
        std::cerr << "socket not connected" << std::endl;
        return false;
    }
    timeval tv{};
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;
    // 这个函数用于设置 socket 选项,要设置哪个 socket,设置的是 socket 通用层面的选项,选项名称是 SO_RCVTIMEO表示设置接收超时时间,选项值是 tv 结构体,长度是 sizeof(tv)
    if (setsockopt(sock_, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0)
    {
        std::cerr << "set receive timeout failed" << std::endl;
        return false;
    }
    return true;
}
/**
 * @brief 发送完整字符串数据。
 *
 * 该函数用于通过当前已连接的 TCP socket 发送一段字符串数据。
 * 由于 send() 系统调用不保证一次调用就能把所有数据全部发送完成，
 * 因此本函数会循环调用 send()，直到字符串中的所有字节都发送完成，
 * 或者发送过程中发生错误。
 *
 * 如果当前 socket 未连接，或者发送过程中连接异常、发送失败，
 * 函数会返回 false。
 *
 * @param text 待发送的字符串数据。
 * @return true 字符串数据已完整发送成功。
 * @return false socket 未连接或发送过程中发生错误。
 */
bool TcpClient::sendAll(const std::string &text)
{
    // sock_ < 0 表示当前没有可用连接
    if (sock_ < 0)
    {
        std::cerr << "socket not connected" << std::endl;
        return false;
    }
    // 已经成功发送的字节数
    std::size_t total_sent = 0;

    // 只要还有数据没发完，就继续发送
    while (total_sent < text.size())
    {
        // data 指向当前还没发送的数据起始位置
        const char *data = text.c_str() + total_sent;
        // left 表示当前还剩多少字节没有发送
        const std::size_t left = text.size() - total_sent;

        // 发送剩余数据
        // send 返回本次实际发送成功的字节数
        const ssize_t n = send(sock_, data, left, 0);

        // n < 0 表示发送失败
        // n == 0 表示本次没有发送任何数据，继续循环可能导致死循环
        if (n <= 0)
        {
            std::cerr << "send failed" << std::endl;
            return false;
        }
        // 累加已经发送成功的字节数
        total_sent += static_cast<std::size_t>(n);
    }

    // 数据完整发送成功
    return true;
}

/**
 * @brief 接收一行服务器响应数据。
 *
 * 该函数用于从当前已连接的 TCP socket 中读取服务器返回的数据。
 * 由于 TCP 是字节流协议，单次 recv() 不保证能接收到一条完整消息，
 * 因此本函数采用逐字节读取的方式，将接收到的字符追加到 line 中，
 * 直到读取到换行符 '\n'，表示一行响应数据接收完成。
 *
 * 为了防止服务端异常或协议错误导致无限接收，函数会限制单行最大长度。
 * 当接收失败、服务器关闭连接、socket 未连接或响应行超过最大长度时，
 * 函数返回 false。
 *
 * @param line 用于保存接收到的一行响应数据，调用前会被清空。
 * @return true 成功接收到一行完整数据。
 * @return false socket 未连接、接收失败、连接关闭或响应数据过长。
 */
bool TcpClient::receiveLine(std::string &line)
{
    // 判断当前 socket 是否已经连接
    if (sock_ < 0)
    {
        std::cerr << "socket not connected" << std::endl;
        return false;
    }

    // 清空外部传入的字符串，避免残留上一次接收的数据
    line.clear();
    // 每次从 socket 中读取一个字符
    char ch = '\0';
    while (true)
    {
        // 从 TCP 连接中读取 1 个字节
        const ssize_t n = recv(sock_, &ch, 1, 0);

        // n < 0 表示接收失败
        if (n < 0)
        {
            // 判断这次 recv() 失败，是不是因为“暂时没有数据可读 / 接收超时”，而不是严重错误
            if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                std::cerr << "recv timeout" << std::endl;
            }
            else
            {
                std::cerr << "recv failed:" << std::strerror(errno) << std::endl;
            }
            return false;
        }

        // n == 0 表示对端已经关闭连接
        if (n == 0)
        {
            std::cerr << "server closed connection" << std::endl;
            return false;
        }
        // 将本次接收到的字符追加到结果字符串中
        line.push_back(ch);
        // 读取到换行符，说明一行数据接收完成
        if (ch == '\n')
        {
            break;
        }
        // 限制单行最大长度，防止异常数据导致内存持续增长
        if (line.size() > 1024)
        {
            std::cerr << "response line too long" << std::endl;
            return false;
        }
    }
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