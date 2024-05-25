#include "ChatService.hpp"
#include "group.hpp"
#include "public.hpp"
#include "user.hpp"
#include <arpa/inet.h>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <netinet/in.h>
#include <ostream>
#include <sys/socket.h>
#include <unistd.h>
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
void mainMenu();

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

                int len = send(clientfd, &js, strlen(request.c_str()) + 1, 0);
                if (len == -1) {
                    cerr << "send login msg error" << endl;
                } else {
                    char buffer[1024];
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
                        }
                    }
                }
            }
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
                            cerr << name << "is already exist, register error!"
                                 << endl;
                        } else { // 注册成功
                            cout << name << "register success ,userid is:"
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