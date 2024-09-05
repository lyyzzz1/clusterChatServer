#include "sqlConnectionPool.h"
#include "muduo/base/Logging.h"
#include <cstddef>
#include <mysql/mysql.h>
#include <string>

using namespace std;

int SqlConnectionPool::maxConnectionCount_ = 10;
int SqlConnectionPool::currentConnectionCount_ = 0;

SqlConnectionPool* SqlConnectionPool::getInstance()
{
    static SqlConnectionPool instance;
    return &instance;
}

SqlConnectionPool::SqlConnectionPool() { init(); }

SqlConnectionPool::~SqlConnectionPool() { destroy(); }

void SqlConnectionPool::setMaxConnectionCount(int maxConnectionCount)
{
    maxConnectionCount_ = maxConnectionCount;
}

void SqlConnectionPool::init()
{
    char* server = "mysql-stu.mysql.database.azure.com";
    char* user = "chat";
    char* password = "123456"; /* 请根据实际情况修改密码 */
    char* database = "chat";
    LOG_INFO << "SqlConnectionPool::init()";
    for (int i = 0; i < maxConnectionCount_; ++i) {
        MYSQL* connection = nullptr;
        connection = mysql_init(connection);
        if (connection == nullptr) {
            LOG_ERROR << "Failed to initialize connection";
            exit(EXIT_FAILURE);
        }
        // 真正进行连接
        connection = mysql_real_connect(connection, server, user, password,
                                        database, 0, NULL, 0);
        if (connection == nullptr) {
            LOG_ERROR << "Failed to connect to database"
                      << mysql_error(connection);
        } else {
            LOG_INFO << "connect to database success! now connection count: "
                     << currentConnectionCount_ << "address: " << connection;
        }
        connectionQueue_.push(connection);
        ++currentConnectionCount_;
    }
}

MYSQL* SqlConnectionPool::getConnection()
{
    MYSQL* connection = connectionQueue_.front();
    connectionQueue_.pop();
    return connection;
}

void SqlConnectionPool::releaseConnection(MYSQL* connection)
{
    connectionQueue_.push(connection);
}

int SqlConnectionPool::getFreeConnectionCount()
{
    return connectionQueue_.size();
}

void SqlConnectionPool::destroy()
{
    MYSQL* connection = nullptr;
    while (!connectionQueue_.empty()) {
        connection = connectionQueue_.front();
        mysql_close(connection);
        connectionQueue_.pop();
    }
}