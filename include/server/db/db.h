#ifndef DB_H
#define DB_H

#include "sqlConnectionPool.h"
#include <muduo/base/Logging.h>
#include <mutex>
#include <mysql/mysql.h>
#include <string>

using namespace std;

class MySQL {
public:
    MySQL();
    ~MySQL();
    bool connect();
    bool update(string sql);
    MYSQL_RES* query(string sql);
    MYSQL* getConneciton();

private:
    MYSQL* _conn;
};

#endif