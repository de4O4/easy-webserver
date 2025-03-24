#ifndef HTTPCONNECTION_H
#define HTTPCONNECTION_H
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
#include <errno.h>
#include <stdarg.h>
#include <sys/wait.h>
#include <sys/uio.h>
#include <map>

#include "../lock/locker.h"
#include "../CGImysql/sql_connection_pool.h"
#include "../timer/lst_timer.h"
#include "../log/log.h"

class http_conn{            //http连接类
    private:
        int m_sockfd;
        sockaddr_in m_address;
        char m_read_buf[READ_BUFFER_SIZE];
        long m_read_idx;        //缓冲区已经读入的客户数据的最后一个字节的下一个位置
        long m_checked_idx;     //当前正在解析的字符在缓冲区中的位置
        int m_start_line;       //当前正在解析的行的起始位置
        char m_write_buf[WRITE_BUFFER_SZIE];
        int m_write_idx;        //写缓冲区中待发送的字节数
        CHECK_STATE m_check_state;      //主状态机当前状态
        METHOD m_method;        //请求方法
        char m_real_file[FILENAME_LEN];
        char *m_url;            //客户请求的目标文件的文件名
        char *m_version;        //HTTP协议版本号
        char *m_host;           //主机名
        long m_content_length;      //消息体长度
        bool m_linger;          //HTTP请求是否要保持连接
        char *m_file_address;       //客户请求的目标文件被mmap到内存中的起始位置
        struct stat m_file_stat;        //目标文件的状态
        struct iovec m_iv[2];
        int m_iv_count;     //被写内存块数量
        int cgi;        //是否启用POST
        char *m_string;     //存储请求头数据
        int bytes_to_send;      //要发送的字节数
        int bytes_have_send;        //已经发送的字节数
        char *doc_root;
        map<string , string> m_users;       //存储用户名和密码
        int m_TRIGMode;     //
        int m_close_log;
        char sql_user[100];
        char sql_passwd[100];
        char sql_name[100];

};