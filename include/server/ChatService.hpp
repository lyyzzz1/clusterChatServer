#ifndef CHATSERVICE_H
#define CHATSERVICE_H

#include "json.hpp"
#include "usermodel.hpp"
#include <functional>
#include <map>
#include <muduo/base/Timestamp.h>
#include <muduo/net/Callbacks.h>
#include <muduo/net/TcpConnection.h>
#include <unordered_map>
#include <mutex>
#include "offlinemessagemodel.hpp"
#include "friendmodel.hpp"
#include "groupmodel.hpp"
#include "redis.hpp"

using namespace std;
using namespace muduo;
using namespace muduo::net;
using json = nlohmann::json;

// 表示处理消息的事件回调方法类型
using MsgHandler =
    std::function<void(const TcpConnectionPtr& conn, json& js, Timestamp time)>;

// 聊天服务器业务类
class ChatService {
public:
    //获取单例对象的接口函数
    static ChatService* instance();
    //处理登录业务
    void login(const TcpConnectionPtr& conn, json& js, Timestamp time);
    //处理注册业务
    void reg(const TcpConnectionPtr& conn, json& js, Timestamp time);
    //一对一聊天
    void oneChat(const TcpConnectionPtr& conn, json& js, Timestamp time);
    //添加好友业务
    void addFriend(const TcpConnectionPtr& conn, json& js, Timestamp time);
    //创建群组
    void createGroup(const TcpConnectionPtr& conn, json& js, Timestamp time);
    //加入群组
    void addGroup(const TcpConnectionPtr& conn, json& js, Timestamp time);
    //群聊消息
    void groupChat(const TcpConnectionPtr& conn, json& js, Timestamp time);
    //处理用户正常退出
    void loginout(const TcpConnectionPtr& conn, json& js, Timestamp time);
    //获取消息对应的处理器
    MsgHandler getHandler(int msgid);
    //处理客户端异常退出
    void clientCloseException(const TcpConnectionPtr& conn);
    //redis
    void handlerRedisSubscribeMessage(int userid,string msg);
    //处理服务器异常退出
    void reset();
private:
    ChatService();
    //存储消息id和其对应的业务处理方法
    unordered_map<int, MsgHandler> _msgHandlerMap;

    // 存储在线用户的通信连接
    unordered_map<int, TcpConnectionPtr> _userConnMap;

    // 定义互斥锁，保证_userConnMap的线程安全
    mutex _connMutex;
    // 数据操作类对象
    UserModel _userModel;
    // 离线消息操作类
    OfflineMsgModel _offlineMsgModel;
    // 好友操作类
    FriendModel _friendModel;
    // 群组操作类
    GroupModel _groupModel;
    // redis类
    Redis _redis;
};

#endif