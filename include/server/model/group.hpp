#ifndef GROUP_H
#define GROUP_H

#include "groupuser.hpp"
#include <string>
#include <vector>
using namespace std;

class Group {
public:
public:
    Group(int id = -1, string name = "", string desc = "")
        : id(id), name(name), desc(desc)
    {
    }

    void setId(int id) { this->id = id; }
    void setName(string name) { this->name = name; }
    void setDesc(string desc) { this->desc = desc; }

    int getId() { return this->id; }
    string getName() { return this->name; }
    string getDesc() { return this->desc; }
    vector<GroupUser>& getUsers() { return users; }

private:
    int id;
    string name;
    string desc;
    vector<GroupUser> users;
};

#endif