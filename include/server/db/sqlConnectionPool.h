#ifndef __SQLCONNECTIONPOOL_H__
#define __SQLCONNECTIONPOOL_H__

#include <mysql/mysql.h>
#include <queue>

// 数据库连接池
// 该类为单例模式的懒汉模式
class SqlConnectionPool {
public:
    // 获取数据库连接池实例
    static SqlConnectionPool* getInstance();
    // 设置最大连接数
    static void setMaxConnectionCount(int maxConnectionCount);
    // 获取数据库连接
    MYSQL* getConnection();
    // 释放数据库连接
    void releaseConnection(MYSQL* connection);
    // 获取空闲数据库连接数
    int getFreeConnectionCount();

private:
    SqlConnectionPool();
    ~SqlConnectionPool();
    // 初始化数据库连接池
    void init();
    // 销毁数据库连接池
    void destroy();
    // 存储数据库连接
    std::queue<MYSQL*> connectionQueue_;
    // 最大连接数
    static int maxConnectionCount_;
    // 当前连接数
    static int currentConnectionCount_;
};

#endif // __SQLCONNECTIONPOOL_H__