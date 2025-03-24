#include <mysql/mysql.h>
#include <stdio.h>
#include <string>
#include <string.h>
#include <stdlib.h>
#include <list>
#include <pthread.h>
#include <iostream>
#include "sql_connection_pool.h"

using namespace std;

connection_pool::connection_pool(){
    m_CurConn = 0;
    m_FreeConn = 0;
}

connection_pool::~connection_pool(){
    DestroyPool();
}

void connection_pool::init(string url , string User , string PassWord , string DBName , int Port , int MaxConn , int close_log){
    m_url = url;
    m_User = User;
    m_PassWord = PassWord;
    m_MaxConn = MaxConn;
    m_close_log = close_log;
    m_DatabassName = DBName;
    m_Port = Port;

    for(int i = 0 ; i < m_MaxConn ; i++){       //创建MaxConn个连接数
        MYSQL *con = NULL;
        con = mysql_init(con);      //创建一个空的数据库连接对象 
        if(con == NULL){
            LOG_ERROR("MYSQL Error");
            exit(-1);
        }

        con = mysql_real_connect(con , url.c_str() , User.c_str() , PassWord.c_str() , DBName.c_str() , Port , NULL , 0);       //连接数据库

        if(con == NULL){
            LOG_ERROR("MYSQL Error");
            exit(-1);            
        }
        connList.push_back(con);
        ++m_FreeConn;           //新增一个空闲连接
    }
    reserve = sem(m_FreeConn);      //用空闲连接数初始化信号的值
    m_MaxConn = m_FreeConn;
}

MYSQL *connection_pool::GetConnection(){        //从数据库连接池中获取一个连接
    MYSQL *con = NULL;
    if(connList.size() == 0){       //数据库池中没有连接
        return NULL;
    }
    reserve.wait();     //信号量-- 操作
    lock.lock();
    con = connList.front();     //取出数据库连接池中队头元素
    connList.pop_front();           //删除数据库连接池中的队头元素
    m_FreeConn--;
    m_CurConn++;
    lock.unlock();
    return con;
}

bool connection_pool::ReleaseConnection(MYSQL *con){        //释放一个数据库连接
    if(con == NULL){
        return false;
    }
    lock.lock();
    connList.push_back(con);        //向数据库连接池尾部加入一个新连接
    m_FreeConn++;
    m_CurConn--;
    lock.unlock();
    reserve.post();

    return true;
}

void connection_pool::DestroyPool(){        //销毁数据库连接池
    lock.lock();
    if(connList.size() > 0){
        for(auto it = connList.begin() ; it != connList.end() ; it ++){
            MYSQL *con = *it;
            mysql_close(con);
        }
        m_CurConn = 0;
        m_FreeConn = 0;
        connList.clear();
    }
    lock.unlock();
}

int connection_pool::GetFreeConn(){
    return this->m_FreeConn;
}

connectionRAII::connectionRAII(MYSQL **SQL , connection_pool *connPool){
    *SQL = connPool->GetConnection();       // 从连接池中获取一个连接   
    conRAII = *SQL;
    poolRAII = connPool;
}

connectionRAII::~connectionRAII(){
    poolRAII->ReleaseConnection(conRAII);
}