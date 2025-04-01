#include "config.h"

Config::Config(){
    PORT = 3333;        //默认端口号
    LOGWrite = 0;       //默认同步写入日志
    TRIGMode = 0;       //默认触发模式为 LT+LT
    LISTENTrigmode = 0;     //默认为LT
    sql_num = 8;        //数据库连接池默认数量
    OPT_LINGER = 0;     //默认不使用
    CONNTrigmode = 0;       //默认为LT
    thread_num = 8;     //线程池默认线程数
    close_log = 0;      //默认使用日志；
    actor_model = 0;        //并发模型 默认为proactor
}

void Config::parse_arg(int argc , char *argv[]){
    int opt;
    const char *str = "p:l:m:o:s:t:c:a:";
    while((opt = getopt(argc , argv , str) != -1)){        // 用于解析命令行选项
        switch(opt){
            case 'p':
            {
                PORT = atoi(optarg);
                break;
            }
            case 'l':
            {
                LOGWrite = atoi(optarg);
                break;
            }
            case 'm':
            {
                TRIGMode = atoi(optarg);
                break;
            }
            case 'o':
            {
                OPT_LINGER= atoi(optarg);
                break;
            }
            case 's':
            {
                sql_num = atoi(optarg);
                break;
            }
            case 't':
            {
                thread_num = atoi(optarg);
                break;
            }
            case 'c':
            {
                close_log = atoi(optarg);
                break;
            }
            case 'a':
            {
                actor_model = atoi(optarg);
                break;
            }
            default:
                break;
        }
    }
}