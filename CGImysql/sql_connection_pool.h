#ifndef _CONNECTION_POOL_
#define _CONNECTION_POOL_

#include <stdio.h>
#include <list>
#include <mysql/mysql.h>
#include <error.h>
#include <string.h>
#include<iostream>
#include<string>
#include "../lock/locker.h"
#include "../log/log.h"

using namespace std;

class connection_pool{      //数据库连接池
    private:
        int m_MaxConn;      //数据库最大可连接数
        int m_CurConn;      //当前的数据库已用连接数
        int m_FreeConn;     //当前的数据库中空闲的连接数
        locker lock;
        list<MYSQL *> connList;     //数据库连接池
        sem reserve;

        connection_pool();
        ~connection_pool();
    
    public:
        string m_url;       //主机地址
        string m_Port;      //数据库端口
        string m_User;      //数据库用户名
        string m_PassWord;      //数据库用户密码
        string m_DatabassName;      //使用的数据库名
        int m_close_log;            //日志开关

        MYSQL *GetConnection();     //获取数据库连接
        bool ReleaseConnection(MYSQL *conn);        //释放连接
        int GetFreeConn();          //获取空闲连接
        void DestroyPool();         //销毁所有连接
        static connection_pool *GetInstance();      //单例模式
        void init(string url , string User , string PassWord , string DatabassName , int Port , int MaxConn , int close_log);       //初始化连接池
};

class connectionRAII{       //资源获取即初始化  确保数据库连接在离开作用域时自动释放，无需手动调用释放函数。
    private:
        MYSQL *conRAII;
        connection_pool *poolRAII;
    
    public:
        connectionRAII(MYSQL **con , connection_pool *connPool);
        ~connectionRAII();
};

#endif