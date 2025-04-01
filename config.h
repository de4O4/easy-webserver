#ifndef CONFIG_H
#define CONFIG_H

#include "webserver.h"
using namespace std;

class Config{
    public:
        Config();
        ~Config(){};
        void parse_arg(int argc , char *argv[]);
        int PORT;       //端口
        int TRIGMode;       //触发模式组合
        int LOGWrite;       //日志写入方式
        int LISTENTrigmode;         //listenfd的触发方式
        int CONNTrigmode;       //connfd的触发方式
        int OPT_LINGER;         //关闭连接的选项
        int sql_num;            //数据库连接池的数量
        int thread_num;         //线程池的数量
        int close_log;          //是否关闭日志
        int actor_model;        //并发模型选择
};

#endif