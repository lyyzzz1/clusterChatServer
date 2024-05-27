#include "redis.hpp"
#include <cstdlib>
#include <hiredis/hiredis.h>
#include <iostream>
#include <ostream>
#include <thread>
using namespace std;

Redis::Redis() : _publish_context(nullptr), _subcribe_context(nullptr) {}

Redis::~Redis()
{
    if (_publish_context != nullptr) {
        redisFree(_publish_context);
    }
    if (_subcribe_context) {
        redisFree(_subcribe_context);
    }
}

bool Redis::connect()
{
    // 负责publish发布消息的上下文连接
    _publish_context = redisConnect("127.0.0.1", 6379);
    if (_publish_context == nullptr) {
        cerr << "connect redis failed!" << endl;
        return false;
    }
    // 负责订阅的上下文连接
    _subcribe_context = redisConnect("127.0.0.1", 6379);
    if (_subcribe_context == nullptr) {
        cerr << "connect redis failed!" << endl;
        return false;
    }

    // 在单独的线程中监听通道上的事件，有消息给业务层进行上报
    thread t([&]() { observer_channel_message(); });
    t.detach();

    cout << "connect redis success!" << endl;
    return true;
}

bool Redis::publish(int channel, string message)
{
    redisReply* reply = (redisReply*)redisCommand(
        _publish_context, "PUBLISH %d %s", channel, message.c_str());
    if (reply == nullptr) {
        cerr << "publish command failed!" << endl;
        return false;
    }
    freeReplyObject(reply);
    return true;
}

bool Redis::subscribe(int channel)
{
    // 把命令组装好
    if (redisAppendCommand(this->_subcribe_context, "SUBSCRIBE %d", channel) ==
        REDIS_ERR) {
        cerr << "subscribe command failed!" << endl;
        return false;
    }
    int done = 0;
    // redisBufferWrite可以循环发送缓冲区，直到缓冲区发送完毕(done被置为1)
    // 发送到redis上而不进行接收，因为接收会阻塞
    while (!done) {
        if (redisBufferWrite(this->_subcribe_context, &done) == REDIS_ERR) {
            cerr << "subscribe command failed!" << endl;
            return false;
        }
    }
    // redisGetReply
    return true;
}

bool Redis::unsubscribe(int channel)
{
    // 把命令组装好
    if (redisAppendCommand(this->_subcribe_context, "UNSUBSCRIBE %d",
                           channel) == REDIS_ERR) {
        cerr << "unsubscribe command failed!" << endl;
        return false;
    }
    int done = 0;
    // redisBufferWrite可以循环发送缓冲区，直到缓冲区发送完毕(done被置为1)
    // 发送到redis上而不进行接收，因为接收会阻塞
    while (!done) {
        if (redisBufferWrite(this->_subcribe_context, &done) == REDIS_ERR) {
            cerr << "unsubscribe command failed!" << endl;
            return false;
        }
    }
    // redisGetReply
    return true;
}

void Redis::observer_channel_message()
{
    redisReply* reply = nullptr;
    while (redisGetReply(this->_subcribe_context, (void**)&reply) == REDIS_OK) {
        // 订阅收到的消息是一个带三元素的数组
        if (reply != nullptr && reply->element[2] != nullptr &&
            reply->element[2]->str != nullptr) {
            _notify_message_handler(atoi(reply->element[1]->str),
                                    reply->element[2]->str);
        }
        freeReplyObject(reply);
    }
}

void Redis::init_notify_handler(function<void(int, string)> fn)
{
    this->_notify_message_handler = fn;
}