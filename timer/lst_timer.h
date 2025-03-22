#ifndef LST_TIMER
#define LST_TIMER

#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <sys/stat.h>
#include <string.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/uio.h>
#include <time.h>

#include "../log/log.h"

class util_timer;

struct client_data{
    sockaddr_in address;
    int sockfd;
    util_timer *timer;
};

class util_timer{       //定时器类
    public:
        util_timer() : prev(NULL) , next(NULL) {}
        time_t expire;      //过期时间
        void (* cb_func)(client_data *);        //回调函数
        client_data *user_data;
        util_timer *prev;
        util_timer *next;
};

class sort_timer_lst            //定时器升序 双向链表
{
    private:
        void add_timer(util_timer *timer , util_timer *lst_head);

        util_timer *head;       //链表头
        util_timer *tail;       //链表尾
    public:
        sort_timer_lst();
        ~sort_timer_lst();

        void add_timer(util_timer *timer);
        void adjust_timer(util_timer *timer);
        void del_timer(util_timer *timer);
        void tick();        //信号每次触发在其信号处理函数中执行一次tick()
};


class Utils{        
    public:
        static int *u_pipefd;
        sort_timer_lst m_timer_lst;
        static int u_epollfd;
        int m_TIMESLOT;

        Utils() {}
        ~Utils() {}
        void init(int tiemslot);
        int setnonblocking(int fd);     //对文件描述符设置为非阻塞
        void addfd(int epollfd , int fd , bool oneshot , int TRIGMode);     //向内核事件表注册事件 ET模式  选择开启EPOLLONESHOT
        static void sig_handler(int sig);       //信号处理函数
        void addsig(int sig , void(handler)(int) , bool restart = true);        //添加信号函数
        void timer_handler();       //定时处理任务 重新定时以不断触发SIGALRM信号
        void show_error(int connfd , const char *info);
};


void cb_func(client_data *user_data);

#endif