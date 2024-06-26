#ifndef CHATSERVER_H
#define CHATSERVER_H

#include <muduo/net/Callbacks.h>
#include <muduo/net/EventLoop.h>
#include <muduo/net/TcpServer.h>

using namespace muduo;
using namespace muduo::net;
using namespace std;

class ChatServer {
public:
    // 初始化聊天服务器对象
    ChatServer(EventLoop* loop, const InetAddress& listenAddr,
               const string& nameArg);
    // 启动服务
    void start();

private:
    // 注册连接相关信息的回调函数
    void onConnection(const TcpConnectionPtr&);

    // 注册读写事件相关信息的回调函数
    void onMessage(const TcpConnectionPtr&,
                            Buffer*,
                            Timestamp);
    TcpServer _server; // 组合的muduo库对象
    EventLoop* _loop;   // 事件循环指针
};

#endif