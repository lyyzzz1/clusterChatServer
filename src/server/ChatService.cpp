#include "ChatService.hpp"

#include <muduo/base/Logging.h>

#include "group.hpp"
#include "public.hpp"
#include "user.hpp"
#include "usermodel.hpp"
#include <functional>
#include <iostream>
#include <map>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

using namespace muduo;
using namespace std::placeholders;
using namespace std;

//获取单例对象的接口函数
ChatService* ChatService::instance()
{
    static ChatService service;
    return &service;
}

// 注册消息以及对应的回调Handler回调函数
ChatService::ChatService()
{
    _msgHandlerMap.insert(
        make_pair(LOGIN_MSG, std::bind(&ChatService::login, this, _1, _2, _3)));
    _msgHandlerMap.insert(
        make_pair(REG_MSG, std::bind(&ChatService::reg, this, _1, _2, _3)));
    _msgHandlerMap.insert(make_pair(
        ONE_CHAT_MSG, std::bind(&ChatService::oneChat, this, _1, _2, _3)));
    _msgHandlerMap.insert(make_pair(
        ADD_FRIEND_MSG, std::bind(&ChatService::addFriend, this, _1, _2, _3)));
    _msgHandlerMap.insert(
        make_pair(CREATE_GROUP_MSG,
                  std::bind(&ChatService::createGroup, this, _1, _2, _3)));
    _msgHandlerMap.insert(make_pair(
        ADD_GROUP_MSG, std::bind(&ChatService::addGroup, this, _1, _2, _3)));
    _msgHandlerMap.insert(make_pair(
        GROUP_CHAT_MSG, std::bind(&ChatService::groupChat, this, _1, _2, _3)));
}

MsgHandler ChatService::getHandler(int msgid)
{
    // 记录错误日志，msgid没有对应的事件处理回调
    auto it = _msgHandlerMap.find(msgid);
    if (it == _msgHandlerMap.end()) {
        // 返回一个默认的处理器，空操作
        return [=](const TcpConnectionPtr& conn, json& js,
                   Timestamp time) -> void {
            LOG_ERROR << "msgid:" << msgid << "can not find handler";
        };
    } else {
        return it->second;
    }
}

// 处理登录业务
void ChatService::login(const TcpConnectionPtr& conn, json& js, Timestamp time)
{
    int id = js["id"].get<int>();
    string password = js["password"];
    User user = _userModel.query(id);
    if (user.getId() != -1 && user.getPwd() == password) {
        if (user.getState() == "online") {
            json response;
            response["msgid"] = LOGIN_MSG_ACK;
            response["errno"] = 2;
            response["errmsg"] = "This account is already logged in, please re-enter a new one";
            conn->send(response.dump());
        } else {
            {
                lock_guard<mutex> lk(_connMutex);
                // 登录成功，记录住用户的tcp连接，以便之后发送消息
                _userConnMap.insert(make_pair(id, conn));
            }
            // 登录成功，更新用户状态信息 state offline->online
            user.setState("online");
            _userModel.updateState(user);
            json response;
            response["msgid"] = LOGIN_MSG_ACK;
            response["errno"] = 0;
            response["id"] = user.getId();
            response["name"] = user.getName();
            // 查询用户是否有离线消息，如果有就转发给用户
            vector<string> vec = _offlineMsgModel.query(id);
            if (!vec.empty()) {
                response["offlinemsg"] = vec;
                // 读取该用户的离线消息后，把该用户的所有离线消息删除
                _offlineMsgModel.remove(id);
            }
            // 查询该用户的好友信息并返回
            vector<User> userVec = _friendModel.query(id);
            if (!userVec.empty()) {
                vector<string> vec2;
                for (User& user : userVec) {
                    json js;
                    js["id"] = user.getId();
                    js["name"] = user.getName();
                    js["state"] = user.getState();
                    vec2.push_back(js.dump());
                }
                response["friends"] = vec2;
            }

            // 如果有群组，则发送群组的的消息
            vector<Group> groupVec = _groupModel.queryGroups(id);
            if (!groupVec.empty()) {
                // 非空的情况下发送到客户端
                // group:[{groupid:[xxx,xxx,xxx]}]
                vector<string> groupV;
                for (Group& group : groupVec) {
                    json groupjson;
                    groupjson["id"] = group.getId();
                    groupjson["groupname"] = group.getName();
                    groupjson["groupdesc"] = group.getDesc();

                    vector<string> userV;
                    for(GroupUser& user:group.getUsers()){
                        json js;
                        js["id"] = user.getId();
                        js["name"] = user.getName();
                        js["state"] = user.getState();
                        js["role"] = user.getRole();
                        userV.push_back(js.dump());
                    }
                    groupjson["users"] = userV;
                    groupV.push_back(groupjson);
                }
                response["groups"] = groupV;
            }
            conn->send(response.dump());
        }
    } else {
        // 登陆失败
        json response;
        response["msgid"] = LOGIN_MSG_ACK;
        response["errno"] = 1;
        response["errmsg"] = "Wrong username or password";
        conn->send(response.dump());
    }
}

//处理注册业务
void ChatService::reg(const TcpConnectionPtr& conn, json& js, Timestamp time)
{
    string name = js["name"];
    string password = js["password"];
    User user;
    user.setName(name);
    user.setPwd(password);
    bool state = _userModel.insert(user);
    if (state) {
        json response;
        response["msgid"] = REG_MSG_ACK;
        response["errno"] = 0;
        response["id"] = user.getId();
        conn->send(response.dump());
    } else {
        json response;
        response["msgid"] = REG_MSG_ACK;
        response["errno"] = 1;
        response["errmsg"] = "sigh up failded";
        conn->send(response.dump());
    }
}

//添加好友业务
void ChatService::addFriend(const TcpConnectionPtr& conn, json& js,
                            Timestamp time)
{
    // js msgid:6 id: friendid:
    int userid = js["id"].get<int>();
    int friendid = js["friendid"].get<int>();
    _friendModel.insert(userid, friendid);
}

//创建群组
void ChatService::createGroup(const TcpConnectionPtr& conn, json& js,
                              Timestamp time)
{
    // msgid:7 id:1 groupname:a groupdesc:no
    int userid = js["id"].get<int>();
    string name = js["groupname"];
    string desc = js["groupdesc"];

    Group group(-1, name, desc);
    if (_groupModel.createGroup(group)) {
        //创建成功，将用户设置为管理员
        _groupModel.addGroup(userid, group.getId(), "creator");
    }
}

void ChatService::addGroup(const TcpConnectionPtr& conn, json& js,
                           Timestamp time)
{
    int userid = js["id"];
    int groupid = js["groupid"];
    _groupModel.addGroup(userid, groupid, "normal");
}

void ChatService::groupChat(const TcpConnectionPtr& conn, json& js,
                            Timestamp time)
{
    // id:1 groupid:2
    int userid = js["id"];
    int groupid = js["groupid"];

    vector<int> useridVec = _groupModel.queryGroupUsers(userid, groupid);
    lock_guard<mutex> lock(_connMutex);
    for (int id : useridVec) {
        auto it = _userConnMap.find(id);
        if (it != _userConnMap.end()) {
            // 在线，直接转发
            it->second->send(js.dump());
        } else {
            // 不在线，存入离线列表
            _offlineMsgModel.insert(id, js.dump());
        }
    }
}

//处理客户端异常退出
void ChatService::clientCloseException(const TcpConnectionPtr& conn)
{
    User user;
    {
        lock_guard<mutex> lock(_connMutex);
        for (auto it : _userConnMap) {
            if (it.second == conn) {
                _userConnMap.erase(it.first);
                user.setId(it.first);
                break;
            }
        }
    }
    if (user.getId() != -1) {
        user.setState("offline");
        _userModel.updateState(user);
    }
}

void ChatService::oneChat(const TcpConnectionPtr& conn, json& js,
                          Timestamp time)
{
    int toid = js["to"].get<int>();
    {
        lock_guard<mutex> lock(_connMutex);
        auto it = _userConnMap.find(toid);
        if (it != _userConnMap.end()) {
            // toid在线，转发消息
            it->second->send(js.dump());
            return;
        }
    }
    // toid不在线，存储离线消息
    _offlineMsgModel.insert(toid, js.dump());
}

void ChatService::reset()
{
    //将user表中的所有state更新为offline
    _userModel.resetState();
}