#include "ChatServer.hpp"
#include <iostream>
#include <muduo/net/EventLoop.h>
#include <muduo/net/InetAddress.h>
using namespace std;

int main(){
    EventLoop loop;
    InetAddress addr("127.0.0.1",6000);
    ChatServer server(&loop,addr,"ChatServer");

    server.start();
    loop.loop();

    return 0;
}