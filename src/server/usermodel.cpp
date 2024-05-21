#include "usermodel.hpp"
#include "db.h"
#include "user.hpp"
#include <charconv>
#include <cstdio>
#include <iostream>
using namespace std;

bool UserModel::insert(User& user)
{
    // 1.组装sql语句
    char sql[1024] = {0};
    sprintf(
        sql, "insert into User(name, password, state) values('%s', '%s', '%s')",
        user.getName().c_str(), user.getPwd().c_str(), user.getState().c_str());

    MySQL mysql;
    if (mysql.connect()) {
        if (mysql.update(sql)) {
            // 获取插入成功的用户数据生成的用户id
            user.setId(mysql_insert_id(mysql.getConneciton()));
            return true;
        }
    }

    return false;
}

User UserModel::query(int& id) 
{ 
    char sql[1024];
    sprintf(sql, "select * from User WHERE id = %d", id);
    MySQL mysql;
    
}