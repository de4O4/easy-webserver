#include "config.h"

int main(int argc , char *argv[]){
    string user = "debian-sys-maint";
    string passwd = "DPdRlnLnmoibAFy3";
    string databassname = "yourdb";

    Config config;
    config.parse_arg(argc , argv);
    WebServer server;

    server.init(config.PORT , user , passwd , databassname , config.LOGWrite , config.OPT_LINGER , config.TRIGMode , config.sql_num , config.thread_num , config.close_log , config.actor_model);       //初始化服务器配置

    server.log_write();     //启动日志

    server.sql_pool();      //启动数据库连接池

    server.thread_pool();       //启动线程池

    server.trig_mode();     //选择epoll触发模式

    server.eventListen();       //开启监听

    server.eventLoop();     //运行服务器

    return 0;

}