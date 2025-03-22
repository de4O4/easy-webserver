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
    else{
        strcpy(log_full_name , p+1);        //复制文件名部分（去掉路径部分）到 log_name 中。
        strncpy(dir_name , file_name , p - file_name + 1);      //将路径部分复制到 dir_name 中
        snprintf(log_full_name , 255 , "%s%d_%02d_%02d_%s" , dir_name , my_ym.tm_year + 1900 , my_ym.tm_mon + 1 , my_ym.tm_mday , file_name); 
    }
    m_today = my_ym.tm_mday;
    m_fp = fopen(log_full_name , "a");
    if(m_fp == NULL){
        return false;
    }
    return true;
}

void Log::write_log(int level , const char *format , ...){
    struct timeval now = {0 , 0};
    gettimeofday(&now , NULL);      //获取了系统的秒级和微秒级时间
    time_t t = now.tv_sec;          //time_t 通常用来存储 Unix 时间戳 自 Unix 纪元（1970 年 1 月 1 日）以来的秒数。
    struct tm *sys_tm = localtime(&t);      //localtime 函数将秒级时间转化为可读的本地时间格式
    struct tm my_tm = *sys_tm;
    char s[16] = {0};
    switch (level)
    {
    case 0:
        strcpy(s , "[debug]:");
        break;
    case 1:
        strcpy(s , "[info]:");
        break;
    case 2:
        strcpy(s , "[warn]:");
        break;
    case 3:
        strcpy(s , "[erro]:");
        break;
    default:
        strcpy(s , "[info]:");
        break;
    }
    m_mutex.lock();
    m_cout++;
    if(m_today != my_tm.tm_mday || m_cout % m_split_lines == 0){
        char new_log[256] = {0};        //用来存储新的日志文件路径
        fflush(m_fp);
        fclose(m_fp);
        char tail[16] = {0};        //用来存储日志文件名的日期部分
        snprintf(tail , 16 , "%d_%02d_%02d_" , my_tm.tm_year + 1900 , my_tm.tm_mon + 1 , my_tm.tm_mday);
        if(m_today != my_tm.tm_mday){
            snprintf(new_log , 255 , "%s%s%s" , dir_name , tail , log_name);
            m_today = my_tm.tm_mday;
            m_cout = 0;
        }
        else{
            snprintf(new_log , 255 , "%s%s%s.%lld" , dir_name , tail , log_name , m_cout / m_split_lines);
        }
        m_fp = fopen(new_log , "a");
    }
    m_mutex.unlock();
    va_list valst;
    va_start(valst , format);       //初始化 valst 变量，使其指向函数中第一个可变参数

    string log_str;
    m_mutex.lock();

    int n = snprintf(m_buf , 48 , "%d-%02d-%02d %02d:%02d:%02d.%06ld %s " , my_tm.tm_yday + 1900 , my_tm.tm_mon + 1 , my_tm.tm_mday , my_tm.tm_hour , my_tm.tm_min , my_tm.tm_sec , now.tv_usec , s);       //写入具体的时间格式
    int m = vsnprintf(m_buf + n , m_log_buf_size - n -1 , format , valst);
    m_buf[n + m] = '\n';        //将换行符添加到缓冲区，表示一行日志的结束
    m_buf[n + m + 1] = '\0';        //追加字符串结束符，确保缓冲区内容是一个有效的 C 字符串
    log_str = m_buf;
    m_mutex.unlock();

    if(m_is_async && !m_log_queue->full()){     //如果异步日志则加入阻塞队列
        m_log_queue->push(log_str);
    }
    else{           //同步日志写入操作
        m_mutex.lock();
        fputs(log_str.c_str() , m_fp);
        m_mutex.unlock();
    }
    va_end(valst);
}

void Log::flush(void){
    m_mutex.lock();
    fflush(m_fp);       ////强制刷新写入流缓冲区
    m_mutex.unlock();
}