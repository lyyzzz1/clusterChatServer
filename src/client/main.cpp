#include "ChatService.hpp"
#include "group.hpp"
#include "groupuser.hpp"
#include "public.hpp"
#include "user.hpp"
#include <arpa/inet.h>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <functional>
#include <iostream>
#include <netinet/in.h>
#include <ostream>
#include <string>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>
#include <unordered_map>
#include <vector>

using namespace std;

// 记录当前用户的个人信息
User g_currentUser;
// 记录当前用户的好友列表信息
vector<User> g_currentUserFriendList;
// 记录当前用户的群组信息
vector<Group> g_currentUserGroupList;
// 显示目前登录成功的用户的信息
void showCurrentUserData();

// 接受线程
void readTaskHandler(int clientfd);
// 获取系统时间（聊天消息需要添加时间信息）
string getCurrentTime();
// 主聊天页面程序
void mainMenu(int clientfd);
// 控制聊天页面程序
bool isMainMenuRunning = false;

int main(int argc, char** argv)
{
    if (argc < 3) {
        cerr << "command invalid! example: ./ChatClient 127.0.0.1 6000" << endl;
        exit(-1);
    }

    // 解析通过命令行传递的ip和port
    char* ip = argv[1];
    uint16_t port = atoi(argv[2]);

    // 创建client段的socket
    int clientfd = socket(AF_INET, SOCK_STREAM, 0);
    if (clientfd == -1) {
        cerr << "socket create error" << endl;
        exit(-1);
    }

    // 填写client需要连接的server信息 ip+port
    sockaddr_in server;
    memset(&server, 0, sizeof(server));

    server.sin_family = AF_INET;
    server.sin_port = htons(port);
    server.sin_addr.s_addr = inet_addr(ip);

    int ret = connect(clientfd, (sockaddr*)&server, sizeof(sockaddr_in));
    if (ret == -1) {
        cerr << "connect server error" << endl;
        close(clientfd);
        exit(-1);
    }

    while (true) {
        // 显示首页面的菜单 登录 注册 退出
        cout << "======================" << endl;
        cout << "1. login" << endl;
        cout << "2. register" << endl;
        cout << "3. quit" << endl;
        cout << "======================" << endl;
        cout << "choice:";
        int choice = 0;
        cin >> choice;
        cin.get();

        switch (choice) {
            case 1: { // login业务
                int id = 0;
                char pwd[50] = {0};
                cout << "userid:";
                cin >> id;
                //清除缓冲区残留的回车
                cin.get();
                cout << "password:";
                cin.getline(pwd, 50);

                json js;
                js["msgid"] = LOGIN_MSG;
                js["id"] = id;
                js["password"] = pwd;
                string request = js.dump();
                int len = send(clientfd, request.c_str(),
                               strlen(request.c_str()) + 1, 0);
                if (len == -1) {
                    cerr << "send login msg error" << endl;
                } else {
                    char buffer[1024] = {0};
                    len = recv(clientfd, buffer, 1024, 0);
                    if (len == -1) {
                        cerr << "recv login msg error" << endl;
                    } else {
                        //接收成功
                        json responsJs = json::parse(buffer);
                        if (responsJs["errno"] != 0) {
                            cerr << responsJs["errmsg"] << endl;
                        } else { // 登录成功
                            // 记录当前用户的id与name
                            g_currentUser.setId(responsJs["id"]);
                            g_currentUser.setName(responsJs["name"]);

                            if (responsJs.contains("friends")) {
                                // 如果存在好友，那么将好友列表存储
                                vector<string> vec = responsJs["friends"];
                                for (string& str : vec) {
                                    json js = json::parse(str);
                                    User user;
                                    user.setId(js["id"].get<int>());
                                    user.setName(js["name"]);
                                    user.setState(js["state"]);
                                    g_currentUserFriendList.push_back(user);
                                }
                            }

                            if (responsJs.contains("groups")) {
                                // 如果存在好友，那么将好友列表存储
                                vector<string> vec1 = responsJs["groups"];
                                for (string& str : vec1) {
                                    json groupjs = json::parse(str);
                                    Group group;
                                    group.setId(groupjs["id"].get<int>());
                                    group.setDesc(groupjs["groupdesc"]);
                                    group.setName(groupjs["groupname"]);

                                    vector<string> groupvec2 = groupjs["users"];
                                    for (string& userstr : groupvec2) {
                                        GroupUser user;
                                        json userinfo = json::parse(userstr);
                                        user.setId(userinfo["id"].get<int>());
                                        user.setRole(userinfo["role"]);
                                        user.setName(userinfo["name"]);
                                        user.setState(userinfo["state"]);
                                        group.getUsers().push_back(user);
                                    }

                                    g_currentUserGroupList.push_back(group);
                                }
                            }
                            showCurrentUserData();
                            if (responsJs.contains("offlinemsg")) {
                                //如果有离线消息
                                vector<string> vec = responsJs["offlinemsg"];
                                for (string& str : vec) {
                                    json js = json::parse(str);
                                    // time + [id] + name + " said" + xxx
                                    if (js["msgid"] == ONE_CHAT_MSG) {
                                        cout << "私聊"
                                             << js["time"].get<string>() << " ["
                                             << js["id"] << "]" << js["name"]
                                             << " said: " << js["msg"] << endl;
                                    }
                                    if (js["msgid"] == GROUP_CHAT_MSG) {
                                        cout << "群消息" << js["time"]
                                             << "group:" << js["groupid"]
                                             << " 's member: [" << js["id"]
                                             << "] name:" << js["name"]
                                             << " said:" << js["msg"] << endl;
                                    }
                                }
                            }

                            // 启动接收线程负责接收数据，该线程只启动一次
                            static bool threadRunning = false;
                            if (!threadRunning) {
                                std::thread readTask(readTaskHandler, clientfd);
                                readTask.detach();
                                threadRunning = true;
                            }

                            // 进入聊天主菜单页面
                            isMainMenuRunning = true;
                            mainMenu(clientfd);
                        }
                    }
                }
            } break;
            case 2: { // register业务
                char name[50] = {0};
                char password[50] = {0};
                cout << "username:";
                // cin与scanf一样遇到回车或者空格都会直接截断，cin.getline不会
                cin.getline(name, 50);
                cout << "password:";
                cin.getline(password, 50);

                json js;
                js["msgid"] = REG_MSG;
                js["name"] = name;
                js["password"] = password;
                string request = js.dump();

                int len = send(clientfd, request.c_str(),
                               strlen(request.c_str()) + 1, 0);

                if (len == -1) {
                    cerr << "send reg msg error:" << request << endl;
                    exit(-1);
                } else { //发送成功
                    char buffer[1024] = {0};
                    len = recv(clientfd, buffer, 1024, 0);
                    if (len == -1) {
                        cerr << "recv reg response fail" << endl;
                    } else {
                        json responsejs = json::parse(buffer);
                        if (responsejs["errno"].get<int>() != 0) { // 注册失败
                            cerr << name << " is already exist, register error!"
                                 << endl;
                        } else { // 注册成功
                            cout << name << " register success ,userid is:"
                                 << responsejs["id"] << ", don't forget it!"
                                 << endl;
                        }
                    }
                }
            } break;
            case 3:
                close(clientfd);
                exit(0);
            default:
                cerr << "invalid input!" << endl;
                break;
        }
    }
}

void showCurrentUserData()
{
    cout << "=====================login user=====================" << endl;
    cout << "current login user => id:" << g_currentUser.getId()
         << " name:" << g_currentUser.getName() << endl;
    cout << "--------------------FriendList--------------------" << endl;
    if (!g_currentUserFriendList.empty()) {
        //好友列表不为空，则输出
        for (User& user : g_currentUserFriendList) {
            cout << user.getId() << " " << user.getName() << " "
                 << user.getState() << endl;
        }
    }
    cout << "--------------------GroupList--------------------" << endl;
    if (!g_currentUserGroupList.empty()) {
        for (Group& group : g_currentUserGroupList) {
            cout << "groupid:" << group.getId()
                 << " groupname:" << group.getName()
                 << " groupdesc:" << group.getDesc() << endl;
            cout << "---------groupuserinfo---------" << endl;
            for (GroupUser& user : group.getUsers()) {
                cout << "userid:" << user.getId()
                     << " username:" << user.getName()
                     << " state:" << user.getState() << " " << user.getRole()
                     << endl;
            }
            cout << "-------group" << group.getId() << "info is over-------"
                 << endl;
        }
    }
}

void readTaskHandler(int clientfd)
{
    while (true) {
        char buffer[1024] = {0};
        int len = recv(clientfd, buffer, 1024, 0);
        if (len == 0 | len == -1) {
            close(clientfd);
            exit(0);
        }

        json js = json::parse(buffer);
        if (js["msgid"] == ONE_CHAT_MSG) {
            cout << js["time"].get<string>() << " [" << js["id"] << "]"
                 << js["name"] << " said: " << js["msg"] << endl;
            continue;
        }
        if (js["msgid"] == GROUP_CHAT_MSG) {
            cout << js["time"] << "group:" << js["groupid"] << " 's member: ["
                 << js["id"] << "] name:" << js["name"] << " said:" << js["msg"]
                 << endl;
            continue;
        }
        if (js["msgid"] == CREATE_GROUP_ACK) {
            cout << js["msg"] << endl;
        }
    }
}

void help(int = 0, string = "");
void chat(int, string);
void addfriend(int, string);
void creategroup(int, string);
void addgroup(int, string);
void groupchat(int, string);
void loginout(int, string);

// 系统支持的客户端命令列表
unordered_map<string, string> commandMap = {
    {"help", "显示所有支持的命令,格式help"},
    {"chat", "一对一聊天,格式chat:friendid:message"},
    {"addfriend", "添加好友,格式addfriend:friendid"},
    {"creategroup", "创建群组,格式creategroup:groupname:groupdesc"},
    {"addgroup", "加入群组,格式addgroup:groupid"},
    {"groupchat", "群聊,格式groupchat:groupid:message"},
    {"loginout", "注销,格式loginout"}};

// 注册系统支持的客户端处理命令
unordered_map<string, function<void(int, string)> > commandHandlerMap = {
    {"help", help},           {"chat", chat},
    {"addfriend", addfriend}, {"creategroup", creategroup},
    {"addgroup", addgroup},   {"groupchat", groupchat},
    {"loginout", loginout}};

void mainMenu(int clientfd)
{
    help();
    char buffer[1024];
    while (isMainMenuRunning) {
        cin.getline(buffer, 1024);
        string commandbuf(buffer);
        string command; // 存储命令
        int idx = commandbuf.find(":");
        if (idx == -1) {
            command = commandbuf;
        } else {
            command = commandbuf.substr(0, idx);
        }
        auto it = commandHandlerMap.find(command);
        if (it == commandHandlerMap.end()) {
            cerr << "invalid input command" << endl;
            continue;
        } else {
            // 调用相应的函数的回调，添加新功能不需要修改mune函数
            it->second(clientfd,
                       commandbuf.substr(idx + 1, commandbuf.size() - idx));
        }
    }
}

void help(int, string)
{
    cout << "show command list >>>" << endl;
    for (auto& it : commandMap) {
        cout << it.first << ":" << it.second << endl;
    }
    cout << endl;
}

void addfriend(int clientfd, string str)
{
    int friendid = atoi(str.c_str());
    json js;
    js["msgid"] = ADD_FRIEND_MSG;
    js["id"] = g_currentUser.getId();
    js["friendid"] = friendid;
    string buffer = js.dump();

    int len = send(clientfd, buffer.c_str(), strlen(buffer.c_str()) + 1, 0);
    if (len == -1) {
        cerr << "addfriend send error -> " << buffer << endl;
    }
}

// friendid:message
void chat(int clientfd, string str)
{
    int idx = str.find(":");
    if (idx == -1) {
        cerr << "chat command invalid!" << endl;
    }
    int friendid = atoi(str.substr(0, idx).c_str());
    string message = str.substr(idx + 1, str.size() - idx);

    json js;
    js["msgid"] = ONE_CHAT_MSG;
    js["id"] = g_currentUser.getId();
    js["name"] = g_currentUser.getName();
    js["toid"] = friendid;
    js["msg"] = message;
    js["time"] = getCurrentTime();

    string buffer = js.dump();
    int len = send(clientfd, buffer.c_str(), strlen(buffer.c_str()) + 1, 0);
    if (len == -1) {
        cerr << "chat send message error! -> " << buffer << endl;
    }
}

// groupname:groupdesc
void creategroup(int clientfd, string str)
{
    int idx = str.find(":");
    if (idx == -1) {
        cerr << "creategroup command invaild!" << endl;
        return;
    }
    string groupname = str.substr(0, idx);
    string groupdesc = str.substr(idx + 1, str.size() - idx);

    json js;
    js["msgid"] = CREATE_GROUP_MSG;
    js["id"] = g_currentUser.getId();
    js["groupname"] = groupname;
    js["groupdesc"] = groupdesc;

    string buffer = js.dump();
    int len = send(clientfd, buffer.c_str(), strlen(buffer.c_str()) + 1, 0);
    if (len == -1) {
        cerr << "create send message error! -> " << buffer << endl;
    }
}

// groupid
void addgroup(int clientfd, string str)
{
    int groupid = stoi(str);

    json js;
    js["msgid"] = ADD_GROUP_MSG;
    js["id"] = g_currentUser.getId();
    js["groupid"] = groupid;

    string buffer = js.dump();
    int len = send(clientfd, buffer.c_str(), strlen(buffer.c_str()) + 1, 0);
    if (len == -1) {
        cerr << "addgroup send message error! -> " << buffer << endl;
    }
}

// groupid:message
void groupchat(int clientfd, string str)
{
    int idx = str.find(":");
    if (idx == -1) {
        cerr << "creategroup command invaild!" << endl;
        return;
    }
    int groupid = stoi(str.substr(0, idx));
    string message = str.substr(idx + 1, str.size() - idx);

    json js;
    js["msgid"] = GROUP_CHAT_MSG;
    js["id"] = g_currentUser.getId();
    js["name"] = g_currentUser.getName();
    js["groupid"] = groupid;
    js["msg"] = message;
    js["time"] = getCurrentTime();

    string buffer = js.dump();
    int len = send(clientfd, buffer.c_str(), strlen(buffer.c_str()) + 1, 0);
    if (len == -1) {
        cerr << "groupchat send message error! -> " << buffer << endl;
    }
}

void loginout(int clientfd, string str)
{
    json js;
    js["msgid"] = LOGIN_OUT_MSG;
    js["id"] = g_currentUser.getId();

    string buffer = js.dump();
    int len = send(clientfd, buffer.c_str(), strlen(buffer.c_str()) + 1, 0);
    if (len == -1) {
        cerr << "groupchat send message error! -> " << buffer << endl;
    } else {
        isMainMenuRunning = false;
        g_currentUserFriendList.clear();
        g_currentUserGroupList.clear();
        cout << "loginout success! bye!" << endl;
    }
}

string getCurrentTime()
{
    auto tt =
        std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    struct tm* ptm = localtime(&tt);
    char date[60] = {0};
    sprintf(date, "%d-%02d-%02d %02d:%02d:%02d", (int)ptm->tm_year + 1900,
            (int)ptm->tm_mon + 1, (int)ptm->tm_mday, (int)ptm->tm_hour,
            (int)ptm->tm_min, (int)ptm->tm_sec);
    return string(date);
}