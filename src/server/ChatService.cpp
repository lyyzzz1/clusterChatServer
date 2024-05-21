#include "ChatService.hpp"

#include <muduo/base/Logging.h>

#include <functional>
#include <mutex>
#include <utility>

#include "public.hpp"
#include "user.hpp"
#include "usermodel.hpp"

using namespace muduo;

//获取单例对象的接口函数
ChatService* ChatService::instance()
{
    static ChatService service;
    return &service;
}

// 注册消息以及对应的回调Handler回调函数
ChatService::ChatService()
{
    _msgHandlerMap.insert(make_pair(
        LOGIN_MSG, std::bind(&ChatService::login, this, placeholders::_1,
                             placeholders::_2, placeholders::_3)));
    _msgHandlerMap.insert(
        make_pair(REG_MSG, std::bind(&ChatService::reg, this, placeholders::_1,
                                     placeholders::_2, placeholders::_3)));
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
            response["errmsg"] = "该账号已经登录，请重新输入新账号";
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
            conn->send(response.dump());
        }
    } else {
        // 登陆失败
        json response;
        response["msgid"] = LOGIN_MSG_ACK;
        response["errno"] = 1;
        response["errmsg"] = "用户名或者密码错误";
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