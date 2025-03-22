#ifndef LOG_H
#define LOG_H   

#include <stdio.h>
#include <iostream>
#include <string>
#include <pthread.h>
#include <stdarg.h>
#include "block_queue.h"

using namespace std;

class Log{                 //log类
    private:
        char dir_name[128];     //路径名
        char log_name[128];     //log文件名
        int m_split_lines;      //日志最大行数
        int m_log_buf_size;     //日志缓冲区大小
        long long m_cout;       //日志行数记录
        FILE *m_fp;             //打开Log的文件指针
        int m_today;            //记录当前时间为哪一天
        char *m_buf;            //日志缓冲区
        block_queue<string> *m_log_queue;       //阻塞队列
        bool m_is_async;            //是否同步标志位
        locker m_mutex;
        int m_close_log;        //关闭日志

        Log();
        virtual ~Log();
        void *async_write_log(){
            string single_log;
            while(m_log_queue->pop(single_log)){        //从阻塞队列中取出一个stirng日志 写入文件
                m_mutex.lock();
                fputs(single_log.c_str() , m_fp);
                m_mutex.unlock();
            }

        }

    public:
        static Log *get_instance(){         //单例模式
            static Log instance;
            return &instance;
        }
        static void *flush_log_thread(void *args){
            Log::get_instance()->async_write_log();
        }
        bool init(const char *file_name , int close_log , int log_buf_size = 8192 , int split_lines = 5000000 , int max_queue_szie = 0);
        void write_log(int level , const char *format , ...);
        void flush(void);
};

#define LOG_DEBUG(format , ...) if(0 == m_close_log) {Log::get_instance()->write_log(0 , format , ##__VA__ARGS__); Log::get_instance()->flush();}
#define LOG_INFO(format , ...) if(0 == m_close_log) {Log::get_instance()->write_log(1 , format , ##__VA__ARGS__); Log::get_instance()->flush();}
#define LOG_WARN(format , ...) if(0 == m_close_log) {Log::get_instance()->write_log(2 , format , ##__VA__ARGS__); Log::get_instance()->flush();}
#define LOG_ERROR(format , ...) if(0 == m_close_log) {Log::get_instance()->write_log(3 , format , ##__VA__ARGS__); Log::get_instance()->flush();}



#endif 