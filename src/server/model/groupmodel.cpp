#include "groupmodel.hpp"
#include "db.h"
#include "group.hpp"
#include "groupuser.hpp"
#include <cstdio>
#include <cstdlib>
#include <vector>

//创建群组
bool GroupModel::createGroup(Group& group)
{
    char sql[1024] = {0};
    sprintf(sql,
            "insert into allgroup(groupname, groupdesc) values ('%s', '%s')",
            group.getName().c_str(), group.getDesc().c_str());

    MySQL mysql;
    if (mysql.connect()) {
        if (mysql.update(sql)) {
            group.setId(mysql_insert_id(mysql.getConneciton()));
            return true;
        }
    }
    return false;
}
//加入群组
void GroupModel::addGroup(int userid, int groupid, string role)
{
    char sql[1024] = {0};
    sprintf(sql, "insert into groupuser values (%d, %d,'%s')", userid, groupid,
            role.c_str());

    MySQL mysql;
    if (mysql.connect()) {
        mysql.update(sql);
    }
}
//查询用户所在群组的信息
vector<Group> GroupModel::queryGroups(int userid)
{
    char sql[1024] = {0};
    sprintf(sql,
            "SELECT a.id a.groupname a.groupdesc FROM allgroup a JOIN "
            "groupuser b ON b.groupid = a.id WHERE b.userid = %d ",
            userid);

    vector<Group> groupvec;
    MySQL mysql;
    if (mysql.connect()) {
        MYSQL_RES* res = mysql.query(sql);
        if (res != nullptr) {
            MYSQL_ROW row;
            while ((row = mysql_fetch_row(res)) != nullptr) {
                Group group;
                group.setId(atoi(row[0]));
                group.setName(row[1]);
                group.setDesc(row[2]);
                groupvec.push_back(group);
            }
            mysql_free_result(res);
        }
    }

    for (Group& group : groupvec) {
        sprintf(sql,
                "SELECT a.id,a.name,a.state,b.grouprole FROM User a JOIN "
                "groupuser b ON b.userid = a.id WHERE b.groupid = %d",
                group.getId());
        MYSQL_RES* res = mysql.query(sql);
        if (res != nullptr) {
            MYSQL_ROW row;
            while ((row = mysql_fetch_row(res)) != nullptr) {
                GroupUser groupuser;
                groupuser.setId(atoi(row[0]));
                groupuser.setName(row[1]);
                groupuser.setState(row[2]);
                groupuser.setRole(row[3]);
                group.getUsers().push_back(groupuser);
            }
            mysql_free_result(res);
        }
    }
    return groupvec;
}
//根据指定的groupid查询群组用户id列表，除userid自己，主要用户群聊业务给群组其它成员群发消息
vector<int> GroupModel::queryGroupUsers(int userid, int groupid)
{
    char sql[1024] = {0};
    sprintf(sql,
            "select userid from groupuser where groupid = %d and userid != %d",
            groupid, userid);
    MySQL mysql;
    vector<int> usersId;
    MYSQL_RES* res = mysql.query(sql);
    if (res != nullptr) {
        MYSQL_ROW row;
        while ((row = mysql_fetch_row(res)) != nullptr) {
            usersId.push_back(atoi(row[0]));
        }
        mysql_free_result(res);
    }
    return usersId;
}
