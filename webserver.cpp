#include "webserver.h"

WebServer::WebServer(){
    users = new http_conn[MAX_FD];          //存储了http_conn数组
    char server_path[200];
    getcwd(server_path , 200);              //取当前工作目录并存储到 server_path 数组中。
    char root[6] = "/root";
    m_root = (char *)malloc(strlen(server_path) + strlen(root) + 1);
    strcpy(m_root , server_path);
    strcat(m_root , root);          // 将 "/root" 追加到 m_root

    users_timer = new client_data[MAX_FD];      //定时器
}

WebServer::~WebServer(){
    close(m_epollfd);
    close(m_listenfd);
    close(m_pipefd[1]);
    close(m_pipefd[0]);
    delete[] users;
    delete[] users_timer;
    delete m_pool;
}


void WebServer::init(int port , string user , string passWord , string databaseName , int log_write , int opt_linger , int trigmode , int sql_num , int thread_num , int close_log , int actor_model){
    m_port = port;
    m_user = user;
    m_passWord = passWord;
    m_databaseName = databaseName;
    m_sql_num = sql_num;
    m_actormodel = actor_model;
    m_sql_num = sql_num;
    m_thread_num = thread_num;
    m_log_write = log_write;
    m_OPT_LINGER = opt_linger;
    m_TRIGMode = trigmode;
    m_close_log = close_log;
}

void WebServer::trig_mode(){                //触发模式
    if(0 == m_TRIGMode){            //LT + LT
        m_LISTENTrigmode = 0;
        m_CONNTrigmode = 0;
    }else if(1 == m_TRIGMode){      //LT + ET
        m_LISTENTrigmode = 0;
        m_CONNTrigmode = 1;
    }else if(2 == m_TRIGMode){      //ET + LT
        m_LISTENTrigmode = 1;
        m_CONNTrigmode = 0;
    }else if(3 == m_TRIGMode){      //ET + ET
        m_LISTENTrigmode = 1;
        m_CONNTrigmode = 1;
    }
}

void WebServer::log_write(){
    if(0 == m_close_log){           //不关闭日志
        if(1 == m_log_write){           //单例模式
            Log::get_instance()->init("./ServerLog" , m_close_log , 2000 , 800000 , 800);       //异步初始化日志
        }else{
            Log::get_instance()->init("./ServerLog" , m_close_log , 2000 , 800000 , 0);         //同步初始化日志
        }
    }
}

void WebServer::sql_pool(){
    m_connPool = connection_pool::GetInstance();            //单例模式获取连接池
    m_connPool->init("localhost" , m_user , m_passWord , m_databaseName , 3306 , m_sql_num , m_close_log);
    users->initmysql_result(m_connPool);        //将users数组连接至数据库池
}


void WebServer::thread_pool(){
    m_pool = new threadpool<http_conn>(m_actormodel , m_connPool , m_thread_num);       //初始化线程池类
}

void WebServer::eventListen(){      //开始进行创建套接字等一系类工作 epoll模型 I/O复用
    m_listenfd = socket(PF_INET , SOCK_STREAM , 0);
    assert(m_listenfd >= 0);

    if(0 == m_OPT_LINGER){      // 当调用 close() 关闭套接字时，立即关闭套接字，不等待任何剩余数据的传输。
        struct linger tmp = {0 , 1};
        setsockopt(m_listenfd , SOL_SOCKET , SO_LINGER , &tmp , sizeof(tmp));
    }else if(1 == m_OPT_LINGER){        // 启用 linger：等待 1 秒钟传输数据后关闭套接字。
        struct linger tmp= {1 ,1};
        setsockopt(m_listenfd , SOL_SOCKET , SO_LINGER , &tmp , sizeof(tmp));
    }

    int ret = 0;
    struct sockaddr_in address;
    bzero(&address , sizeof(address));
    address.sin_port = htons(m_port);
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_family = AF_INET;

    int flag = 1;
    setsockopt(m_listenfd , SOL_SOCKET , SO_REUSEADDR , &flag , sizeof(flag));          //设置端口复用
    ret = bind(m_listenfd , (struct sockaddr *)&address , sizeof(address));
    assert(ret >= 0);
    ret = listen(m_listenfd , 5);
    assert(ret >= 0);

    utils.init(TIMESLOT);       //设置定时时间

    epoll_event events[MAX_EVENT_NUMBER];
    m_epollfd = epoll_create1(5);           //创建epoll节点
    assert(m_epollfd != -1);

    utils.addfd(m_epollfd , m_listenfd , false , m_LISTENTrigmode);
    http_conn::m_epollfd = m_epollfd;
    
    ret =socketpair(PF_UNIX , SOCK_STREAM , 0 , m_pipefd);      //创建用于进程间通信的管道 用于信号的通知
    assert(ret != -1);
    utils.setnonblocking(m_pipefd[1]);          //设置管道写端为非阻塞
    utils.addfd(m_epollfd , m_pipefd[0] , false , 0);       //将管道读端加入epoll事件树中

    utils.addsig(SIGPIPE , SIG_IGN);
    utils.addsig(SIGALRM , utils.sig_handler , false);
    utils.addsig(SIGTERM , utils.sig_handler , false);
    alarm(TIMESLOT);            //开始定时

    Utils::u_pipefd = m_pipefd;     //传入写端
    Utils::u_epollfd = m_epollfd;
}

void WebServer::timer(int connfd , struct sockaddr_in client_address){
    users[connfd].init(connfd , client_address , m_root , m_CONNTrigmode , m_close_log , m_user , m_passWord , m_databaseName);         //将新文件描述符加入定时器数组
    users_timer[connfd].address = client_address;
    users_timer[connfd].sockfd = connfd;
    util_timer *timer = new util_timer;         //创建定时器
    timer->user_data = &users_timer[connfd];
    timer->cb_func = cb_func;
    time_t cur = time(NULL);
    timer->expire = cur + 3 * TIMESLOT;             //设置超时时间
    utils.m_timer_lst.add_timer(timer);             //将该定时器加入定时器升序链表中
}

void WebServer::adjust_timer(util_timer *timer){        //重置过期时间
    time_t cur = time(NULL);
    timer->expire = cur + 3 * TIMESLOT;
    utils.m_timer_lst.adjust_timer(timer);
}

void WebServer::deal_timer(util_timer *timer , int sockfd){
    timer->cb_func(&users_timer[sockfd]);
    if(timer){
        utils.m_timer_lst.del_timer(timer);
    }
    LOG_INFO("close fd %d" , users_timer[sockfd].sockfd);
}

bool WebServer::dealcliendata(){
    struct sockaddr_in client_address;
    socklen_t client_addrlength = sizeof(client_address);
    if(0 == m_LISTENTrigmode){
        int connfd = accept(m_listenfd , (struct sockaddr *)&client_address , &client_addrlength);
        if(connfd < 0){
            LOG_ERROR("%s: errno is:%d" , "accept error" , errno);
            return false;
        }
        if(http_conn::m_user_count >= MAX_FD){
            utils.show_error(connfd , "Internal server busy!");
            LOG_ERROR("%s " , "Internal server busy");
            return false;
        }
        timer(connfd , client_address);
    }else{
        while(1){
            int connfd = accept(m_listenfd , (struct sockaddr *)&client_address , &client_addrlength);
            if(connfd < 0){
                LOG_ERROR("%s: errno is:%d" , "accept error" , errno);
                break;
            }
            if(http_conn::m_user_count >= MAX_FD){
                utils.show_error(connfd , "Internal server busy!");
                LOG_ERROR("%s " , "Internal server busy");
                break;
            }
            timer(connfd , client_address);
        }
        return false;
    }
    return true;
}