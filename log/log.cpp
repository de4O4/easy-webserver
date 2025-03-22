#include<string.h>
#include<time.h>
#include<sys/time.h>
#include<stdarg.h>
#include<pthread.h>
#include "log.h"
using namespace std;

Log::Log(){
    m_cout = 0;
    m_is_async = false;
}

Log::~Log(){        //关闭日志文件的文件指针
    if(m_fp != NULL){
        fclose(m_fp);
    }
}

bool Log::init(const char *file_name , int close_log , int log_buf_size , int split_lines , int max_queue_szie){
    if(max_queue_szie >= 1){        //若设置了max_queue_szie 则设置为异步日志
        m_is_async = true;
        m_log_queue = new block_queue<string>(max_queue_szie);      //new后面跟的是block_queue的构造函数
        pthread_t tid;
        pthread_create(&tid , NULL , flush_log_thread , NULL);      //flush_log_thread为回调函数， 为创建线程异步写日志
    }
    m_close_log = close_log;
    m_log_buf_size = log_buf_size;
    m_buf = new char[m_log_buf_size];
    memset(m_buf , '\0' , m_log_buf_size);
    m_split_lines = split_lines;

    time_t t = time(NULL);      // 获取当前时间的秒数
    struct tm *sys_tm = localtime(&t);      // 将time_t转换为当前本地时间的结构体
    struct tm my_ym = *sys_tm;

    const char *p = strrchr(file_name , '/');       //查找字符串 file_name 中最后一个出现的斜杠字符 ('/') 的位置
    char log_full_name[256] = {0};

    if(p == NULL){      //当日志文件不存在 则创建一个日志文件
        snprintf(log_full_name , 255 , "%s%d_%02d_%02d_%s" , dir_name , my_ym.tm_year + 1900 , my_ym.tm_mon + 1 , my_ym.tm_mday , file_name);       //格式化log_full_name
    }

}