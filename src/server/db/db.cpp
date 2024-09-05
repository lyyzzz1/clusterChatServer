#include "db.h"
#include "sqlConnectionPool.h"

MySQL::MySQL()
{
    _conn = SqlConnectionPool::getInstance()->getConnection();
}

MySQL::~MySQL()
{
    SqlConnectionPool::getInstance()->releaseConnection(_conn);
}

bool MySQL::connect()
{
    if (_conn != nullptr) {
        mysql_query(_conn, "set names gbk");
        LOG_INFO << "connect mysql success!";
    } else {
        LOG_INFO << "connect mysql failed!";
    }
    return _conn;
}

bool MySQL::update(string sql)
{
    if (mysql_query(_conn, sql.c_str())) {
        LOG_INFO << __FILE__ << ":" << __LINE__ << ":" << sql << "更新失败!";
        return false;
    }
    return true;
}

MYSQL_RES* MySQL::query(string sql)
{
    if (mysql_query(_conn, sql.c_str())) {
        LOG_INFO << __FILE__ << ":" << __LINE__ << ":" << sql
                 << "查询失败!  错误信息: " << mysql_error(_conn);
        return nullptr;
    }
    return mysql_use_result(_conn);
}

MYSQL* MySQL::getConneciton() { return this->_conn; }