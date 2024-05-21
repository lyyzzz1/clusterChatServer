#ifndef DB_H
#define DB_H

#include <cstddef>
#include <mysql/mysql.h>
#include <string>
#include <muduo/base/Logging.h>

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