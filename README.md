# demo-qemu-cpp
创建 TcpClient 对象
        ↓
connectTo() 创建 socket
        ↓
设置服务器 IP 和端口
        ↓
connect() 连接服务器
        ↓
sendText() 发送数据
        ↓
receiveText() 接收服务器回复
        ↓
对象销毁时自动调用 ~TcpClient()
        ↓
closeSocket() 关闭连接