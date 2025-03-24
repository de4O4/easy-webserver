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
    public:
        static const int FILENAME_LEN = 200;
        static const int READ_BUFFER_SIZE = 2048;
        static const int WRITE_BUFFER_SIZE = 1024;
        enum METHOD{        //HTTP请求方法
            GET = 0,
            PSOT,
            HEAD,
            PUT,
            DELETE,
            TRACE,
            OPTIONS,
            CONNECT,
            PATH
        };
        enum CHECK_STATE{           //解析客户请求时 主状态机所处的状态
            CHECK_STATE_REQUESTLINE = 0,
            CHECK_STATE_HEADER,
            CHECK_STATE_CONTENT
        };
        enum HTTP_CODE{             //服务器处理HTTP请求的可能结果
            NO_REQUEST,
            GET_REQUEST,
            BAD_REQUEST,
            NO_RESOURCE,
            FORBIDDEN_REQUEST,
            FILE_REQUEST,
            INTERNAL_ERROR,
            CLOSED_CONNECTION
        };
        enum LINE_STATUS{           //行的读取状态
            LINE_OK = 0,
            LINE_BAD,
            LINE_OPEN
        };
        http_conn();
        ~http_conn();

        void init(int sockfd , const sockaddr_in &addr , char * , int , int , string user , string passwd , string sqlname);
        void close_conn(bool real_close = true);
        void process();
        bool read_once();
        bool write();
        sockaddr_in *get_address(){
            return &m_address;
        }
        void initmysql_result(connection_pool *connPool);
        int timer_flag;
        int improv;

        static int m_epollfd;
        static int m_user_count;
        MYSQL *mysql;
        int m_state;        //读为0 写为1

    private:
        int m_sockfd;
        sockaddr_in m_address;
        char m_read_buf[READ_BUFFER_SIZE];
        long m_read_idx;        //缓冲区已经读入的客户数据的最后一个字节的下一个位置
        long m_checked_idx;     //当前正在解析的字符在缓冲区中的位置
        int m_start_line;       //当前正在解析的行的起始位置
        char m_write_buf[WRITE_BUFFER_SIZE];
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
        int bytes_to_send;      //剩余发送字节数
        int bytes_have_send;        //已经发送的字节数
        char *doc_root;
        map<string , string> m_users;       //存储用户名和密码
        int m_TRIGMode;     //
        int m_close_log;
        char sql_user[100];
        char sql_passwd[100];
        char sql_name[100];

        void init();
        HTTP_CODE process_read();           //解析HTTP请求
        bool process_write(HTTP_CODE ret);          //填充HTTP应答
        HTTP_CODE parse_request_line(char *text);       //解析请求行
        HTTP_CODE parse_headers(char *text);            //解析请求头
        HTTP_CODE parse_content(char *text);            //解析正文
        HTTP_CODE do_request();             //生成响应报文
        char *get_line(){           //用于将指针向后偏移，指向未处理的字符
            return m_read_buf + m_start_line;
        }
        LINE_STATUS parse_line();       //从状态机读取一行，分析是请求报文的哪一部分
        void umap();
        bool add_response(const char *format , ...);
        bool add_content(const char *content);
        bool add_status_line(int status , const char *title);
        bool add_headers(int content_length);
        bool add_content_type();
        bool add_content_length(int content_length);
        bool add_linger();
        bool add_blank_line();      
};


#endif