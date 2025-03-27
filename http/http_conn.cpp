#include "http_conn.h"

#include <mysql/mysql.h>
#include <fstream>

const char *ok_200_title = "OK";
const char *error_400_title = "Bad Request";
const char *error_400_form = "Your request has bad syntax or is inherently impossible to staisfy.\n";
const char *error_403_title = "Forbidden";
const char *error_403_form = "You do not have permission to get file form this server.\n";
const char *error_404_title = "Not Found";
const char *error_404_form = "The requested file was not found on this server.\n";
const char *error_500_title = "Internal Error";
const char *error_500_form = "There was an unusual problem serving the request file.\n";

locker m_lock;
map<string , string> users;

void http_conn::initmysql_result(connection_pool *connPool){            //初始化msql的查询结果
    MYSQL *mysql = NULL;
    connectionRAII mysqlcon(&mysql , connPool);      //实例化一个MySQL连接
    if(mysql_query(mysql , "SELECT username,passwd FROM user")){        //查询sql语句
        LOG_ERROR("SELECT error:%s\n" , mysql_errno(mysql));
    }
    MYSQL_RES *result = mysql_store_result(mysql);          //从表中查找完整的数据集
    int num_fields = mysql_num_fields(result);              //返回结果集中的列数
    MYSQL_FIELD *fields = mysql_fetch_field(result);        //返回所有字段结构的数组
    while(MYSQL_ROW row = mysql_fetch_row(result)){
        string temp1(row[0]);       //用户名
        string temp2(row[1]);       //密码
        users[temp1] = temp2;
    }
}

int setnonblocking(int fd){         //将文件描述符设置为非阻塞
    int old = fcntl(fd , F_GETFL);
    int newop = old | O_NONBLOCK;
    fcntl(fd , F_SETFL , newop);
    return old;
}

void addfd(int epollfd , int fd , bool one_shot , int TRIGMode){
    epoll_event event;
    event.data.fd = fd;
    if(1 == TRIGMode){           //ET模式
        event.events = EPOLLIN |EPOLLET | EPOLLRDHUP;
    }
    else{
        event.events = EPOLLIN | EPOLLRDHUP;
    }
    if(one_shot){
        event.events |= EPOLLONESHOT;
    }
    epoll_ctl(epollfd , EPOLL_CTL_ADD , fd , &event);
    setnonblocking(fd);
}

void removefd(int epollfd , int fd){
    epoll_ctl(epollfd , EPOLL_CTL_DEL , fd , 0);
    close(fd);
}

void modfd(int epollfd , int fd ,int ev , int TRGIMode){            //将事件重置为EPOLLONESHOT
    epoll_event event;
    event.data.fd = fd;
    if(1 ==TRGIMode){
        event.events = EPOLLET | ev |EPOLLONESHOT | EPOLLRDHUP;
    }else{
        event.events = ev | EPOLLONESHOT | EPOLLRDHUP;
    }
    epoll_ctl(epollfd , EPOLL_CTL_MOD , fd , &event);
}

int http_conn::m_user_count = 0;
int http_conn::m_epollfd = -1;

void http_conn::close_conn(bool real_close){
    if(real_close && (m_sockfd != -1)){
        std::cout<<"close "<<m_sockfd<<"\n";
        removefd(m_epollfd , m_sockfd);
        m_sockfd = -1;          //将文件描述符重置为-1
        m_user_count--;
    }
}

void http_conn::init(int sockfd , const sockaddr_in &addr , char *root , int TRGIMode , int close_log , string user , string passwd , string sqlname){
    m_sockfd = sockfd;
    m_address = addr;
    addfd(m_epollfd , sockfd , true , m_TRIGMode);
    m_user_count++;

    doc_root = root;
    m_TRIGMode = TRGIMode;
    m_close_log = close_log;

    strcpy(sql_name , sqlname.c_str());
    strcpy(sql_passwd , passwd.c_str());
    strcpy(sql_user , user.c_str());

    init();
}

void http_conn::init(){             //初始化参数的值
    mysql = NULL;
    bytes_to_send = 0;
    bytes_have_send = 0;
    m_check_state = CHECK_STATE_REQUESTLINE;
    m_linger = false;
    m_method = GET;
    m_url = 0;
    m_version = 0;
    m_content_length = 0;
    m_host = 0;
    m_start_line = 0;
    m_checked_idx = 0;
    m_read_idx = 0;
    m_write_idx = 0;
    cgi = 0;
    m_state = 0;
    timer_flag = 0;
    improv = 0;

    memset(m_read_buf , '\0' , READ_BUFFER_SIZE);
    memset(m_write_buf , '\0' , WRITE_BUFFER_SIZE);
    memset(m_real_file , '\0' , FILENAME_LEN);
}

http_conn::LINE_STATUS http_conn::parse_line(){             //从状态机，用于分析出一行内容  返回值为行的读取状态，有LINE_OK,LINE_BAD,LINE_OPEN
    char temp;      //temp为将要分析的字节
    for(; m_checked_idx < m_read_idx ; m_checked_idx++){
        temp = m_read_buf[m_checked_idx];
        if(temp == '\r'){               //如果当前是\r字符，则有可能会读取到完整行
            if((m_checked_idx + 1) == m_read_idx){          //下一个字符达到了buffer结尾，则接收不完整，需要继续接收
                return LINE_OPEN;
            }
            else if(m_read_buf[m_checked_idx + 1] == '\n'){         //下一个字符是\n，将\r\n改为\0\0
                m_read_buf[m_checked_idx++] == '\0';
                m_read_buf[m_checked_idx++] == '\0';
                return LINE_OK;
            }
            return LINE_BAD;
        }
        else if(temp == '\n'){
            if(m_checked_idx > 1 && m_read_buf[m_checked_idx - 1] == '\r'){
                m_read_buf[m_checked_idx - 1] == '\0';
                m_read_buf[m_checked_idx++] = '\0';
                return LINE_OK;
            }
            return LINE_BAD;
        }
    }
    return LINE_OPEN;
}

bool http_conn::read_once(){        ////循环读取客户数据，直到无数据可读或对方关闭连接   非阻塞ET工作模式下，需要一次性将数据读完
    if(m_read_idx >= READ_BUFFER_SIZE){
        return false;
    }
    int bytes_read = 0;
    if(0 == m_TRIGMode){            //LT模式读取数据
        bytes_read = recv(m_sockfd , m_read_buf + m_read_idx , READ_BUFFER_SIZE - m_read_idx , 0);
        m_read_idx += bytes_read;
        if(bytes_read <= 0){
            return false;
        }
        return true;
    }else{              //ET读取数据
        while(1){
            bytes_read = recv(m_sockfd , m_read_buf + m_read_idx , READ_BUFFER_SIZE - m_read_idx , 0);
            if(bytes_read == -1){
                if(errno == EAGAIN || errno == EWOULDBLOCK){
                    break;
                }
                return false;
            }else if(bytes_read == 0){
                return false;
            }
            m_read_idx += bytes_read;
        }
        return true;
    }             
}

http_conn::HTTP_CODE http_conn::parse_request_line(char *text){             //解析http请求行，获得请求方法，目标url及http版本号
    m_url = strpbrk(text , " \t");          //请求行中最先含有空格和\t任一字符的位置并返回
    if(!m_url){
        return BAD_REQUEST;
    }
    *m_url++ = '\0';             //将该位置改为\0，用于将前面数据取出
    char *method = text;         //取出数据，并通过与GET和POST比较，以确定请求方式
    if(strcasecmp(method , "GET") == 0){
        m_method = GET;
    }else if(strcasecmp(method , "POST") == 0){
        m_method = PSOT;
        cgi = 1;
    }else{
        return BAD_REQUEST;
    }
    m_url += strspn(m_url , " \t");         //m_url此时跳过了第一个空格或\t字符，但不知道之后是否还有       将m_url向后偏移，通过查找，继续跳过空格和\t字符，指向请求资源的第一个字符
    m_version = strpbrk(m_url , " \t");
    if(!m_version){
        return BAD_REQUEST;
    }
    *m_version++ = '\0';
    m_version += strspn(m_version , " \t");
    if(strcasecmp(m_version , "HTTP/1.1") != 0){        //仅支持HTTP/1.1
        return BAD_REQUEST;
    }
    if(strncasecmp(m_url , "http://" , 7) == 0){       //对请求资源前7个字符进行判断  这里主要是有些报文的请求资源中会带有http://，这里需要对这种情况进行单独处理
        m_url += 7;
        m_url = strchr(m_url , '/');
    }
    if(strncasecmp(m_url , "https://" , 8) == 0){
        m_url += 8;
        m_url = strchr(m_url , '/');
    }
    if(!m_url || m_url[0] != '/'){        //一般的不会带有上述两种符号，直接是单独的/或/后面带访问资源
        return BAD_REQUEST;
    }
    if(strlen(m_url) == 1){         //当url为/时，显示欢迎界面
        strcat(m_url , "judge.html");
    }
    m_check_state = CHECK_STATE_HEADER;     //请求行处理完毕，将主状态机转移处理请求头
    return NO_REQUEST;
}

http_conn::HTTP_CODE http_conn::parse_headers(char *text){
    if(text[0] == '\0'){            //判断是空行还是请求头
        if(m_content_length != 0){
            m_check_state = CHECK_STATE_CONTENT;
            return NO_REQUEST;
        }
        return GET_REQUEST;
    }else if(strncasecmp(text , "Connection:" , 11) == 0){      //解析请求头部连接字段
        text += 11;
        text += strspn(text , " \t");           //跳过空格和\t字符
        if(strcasecmp(text , "keep-alive") == 0){            //如果是长连接，则将linger标志设置为true
            m_linger = true;
        }       
    }else if(strncasecmp(text , "Content-length:" , 15) == 0){      //解析请求头部内容长度字段
        text += 15;
        text += strspn(text , " \t");
        m_content_length = atol(text);      //将text中的数字部分转换为long类型的整数
    }else if(strncasecmp(text , "Host:" , 5) == 0){
        text += 5;
        text += strspn(text , " \t");
        m_host = text;
    }else{
        LOG_INFO("未知请求头：%s" , text);
    }       
    return NO_REQUEST;          //请求不完整
}

http_conn::HTTP_CODE http_conn::parse_content(char *text){
    if(m_read_idx >= (m_content_length + m_checked_idx)){       //判断buffer中是否读取了消息体
        text[m_content_length] = '\0';      
        m_string = text;        //POST请求中最后为输入的用户名和密码
        return GET_REQUEST;
    }
    return NO_REQUEST;
}

http_conn::HTTP_CODE http_conn::process_read(){         //将主从状态机进行封装，对报文的每一行进行循环处理。
    LINE_STATUS line_status = LINE_OK;
    m_start_line = m_checked_idx;
    char *text = 0;
    while((m_check_state == CHECK_STATE_CONTENT && line_status == LINE_OK) || ((line_status == parse_line()) == LINE_OK)){
        text = get_line();
        m_start_line = m_checked_idx;
        LOG_INFO("%s" , text);
        http_conn::HTTP_CODE ret;
        switch (m_check_state)
        {
        case CHECK_STATE_REQUESTLINE:       //解析请求行
        {
            ret = parse_request_line(text);     
            if(ret == BAD_REQUEST){
                return BAD_REQUEST;
            }
            break;
        }
        case CHECK_STATE_HEADER:            //解析请求头
        {
            ret = parse_headers(text);
            if(ret == BAD_REQUEST){
                return BAD_REQUEST;
            }else if(ret == GET_REQUEST){
                return do_request();        //完整解析POST请求后，跳转到报文响应函数
            }
            break;
        }       
        case CHECK_STATE_CONTENT:        //解析消息体
        {
            ret = parse_content(text);
            if(ret == GET_REQUEST){
                return do_request();            //完整解析POST请求后，跳转到报文响应函数
            }
            line_status = LINE_OPEN;
            break;
        }          
        default:
            return INTERNAL_ERROR;
        }
    }
    return NO_REQUEST;
}

http_conn::HTTP_CODE http_conn::do_request(){      // 将网站根目录和url文件拼接
    strcpy(m_real_file , doc_root);          //将初始化的m_real_file赋值为网站根目录
    int len = strlen(doc_root);
    const char *p = strchr(m_url , '/');        //找到m_url中/的位置
    if(cgi == 1 && (*(p + 1) == '2' || *(p + 1) == '3')){           //实现登录和注册校验 POST方式
        char flag = m_url[1];       //根据标志判断是登录检测还是注册检测
        char *m_url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(m_url_real , "/");
        strcat(m_url_real , m_url + 2);
        strncpy(m_real_file + len , m_url_real , FILENAME_LEN - len - 1);           //将请求的文件拼接至网站根目录中
        free(m_url_real);

        char name[100] , password[100];
        int i;
        for(i = 5 ; m_string[i] != '&' ; i++){      //提取用户名
            name[i-5] = m_string[i];
        }
        name[i-5] = '\0';

        int j = 0;
        for(i = i + 10 ; m_string[i] != '\0' ; j++){        //提取密码
            password[j] = m_string[i];
        }
        password[j] = '\0';
        
        if(*(p + 1) == 3){      //如果是注册，先检测数据库中是否有重名的  没有重名的，进行增加数据
            char *sql_insert = (char *)malloc(sizeof(char) * 200);
            strcpy(sql_insert , "INSERT INTO user(username , passwd) VALUES(");
            strcat(sql_insert , "'");
            strcat(sql_insert , name);
            strcat(sql_insert , "', '");
            strcat(sql_insert , password);
            strcat(sql_insert , "')");
            if(users.find(name) == users.end()){
                m_lock.lock();
                int res = mysql_query(mysql , sql_insert);      //进行sql查询、
                users.insert(pair<string , string>(name , password));
                m_lock.unlock();
                if(!res){
                    strcpy(m_url , "/log.html");
                }else{
                    strcpy(m_url , "/registerError.html");
                }
            }else{
                strcpy(m_url , "/registerError.html");
            }
        }else if(*(p + 1) == '2'){          //如果是登录，直接判断  浏览器端输入的用户名和密码在表中可以查找到，返回1，否则返回0
            if(users.find(name) != users.end() && users[name] == password){
                strcpy(m_url , "/welcome.html");
            }else{
                strcpy(m_url , "/logError.html");
            }
        }
    }
    if(*(p + 1) == '0'){        //如果请求资源为/0，表示跳转注册界面
        char *m_url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(m_url_real , "/register.html");
        strncpy(m_real_file + len , m_url_real , strlen(m_url_real));       //将网站目录和/register.html进行拼接，更新到m_real_file中
        free(m_url_real);
    }else if(*(p + 1) == '1'){      // 如果请求资源为/1，表示跳转登录界面
        char *m_url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(m_url_real , "/log.html");
        strncpy(m_real_file + len , m_url_real , strlen(m_url_real));       //将网站目录和/register.html进行拼接，更新到m_real_file中
        free(m_url_real);      
    }else if(*(p + 1) == '5'){
        char *m_url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(m_url_real , "/picture.html");
        strncpy(m_real_file + len , m_url_real , strlen(m_url_real));       //将网站目录和/register.html进行拼接，更新到m_real_file中
        free(m_url_real);
    }else if(*(p + 1) == '6'){
        char *m_url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(m_url_real , "/video.html");
        strncpy(m_real_file + len , m_url_real , strlen(m_url_real));       //将网站目录和/register.html进行拼接，更新到m_real_file中
        free(m_url_real);
    }else if(*(p + 1) == '7'){
        char *m_url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(m_url_real , "/fans.html");
        strncpy(m_real_file + len , m_url_real , strlen(m_url_real));       //将网站目录和/register.html进行拼接，更新到m_real_file中
        free(m_url_real);
    }else{
        strncpy(m_real_file + len , m_url , FILENAME_LEN - len - 1);        //如果以上均不符合，即不是登录和注册，直接将url与网站目录拼接
    }
    if(stat(m_real_file , &m_file_stat) < 0){       //通过stat获取请求资源文件信息，成功则将信息更新到m_file_stat结构体
        return NO_REQUEST;          //失败返回NO_RESOURCE状态，表示资源不存在
    }
    if(!(m_file_stat.st_mode)){     //判断文件的权限，是否可读，不可读则返回FORBIDDEN_REQUEST状态
        return FORBIDDEN_REQUEST;
    }
    if(S_ISDIR(m_file_stat.st_mode)){           //判断文件类型，如果是目录，则返回BAD_REQUEST，表示请求报文有误
        return BAD_REQUEST;
    }
    int fd = open(m_real_file , O_RDONLY);          //以只读方式获取文件描述符，通过mmap将该文件映射到内存中
    m_file_address = (char *)mmap(0 , m_file_stat.st_size , PROT_READ , MAP_PRIVATE , fd , 0);
    close(fd);
    return FILE_REQUEST;             //表示请求文件存在，且可以访问
}       

void http_conn::unmap(){
    if(m_file_address){
        munmap(m_file_address , m_file_stat.st_size);
        m_file_address = 0;
    }
}

bool http_conn::add_response(const char *format , ...){
    if(m_write_idx >= WRITE_BUFFER_SIZE){           //如果写入内容超出m_write_buf大小则报错
        return false;
    }
    va_list arg_list;
    va_start(arg_list , format);         //将变量arg_list初始化为传入参数
    int len = vsnprintf(m_write_buf + m_write_idx , WRITE_BUFFER_SIZE - 1 - m_write_idx , format , arg_list);           //将数据format从可变参数列表写入缓冲区写，返回写入数据的长度
    if(len >= (WRITE_BUFFER_SIZE - 1 - m_write_idx)){       //如果写入的数据长度超过缓冲区剩余空间，则报错
        va_end(arg_list);
        return false;
    }
    m_write_idx += len;
    va_end(arg_list);
    LOG_INFO("request:%s" , m_write_buf);
}

bool http_conn::add_status_line(int status , const char *title){            //添加状态行
    return add_response("%s %d %s\r\n" , "HTTP/1.1" , status , title);
}

bool http_conn::add_headers(int content_len){           
    return add_content_length(content_len) && add_linger() && add_blank_line();       //添加消息报头，具体的添加文本长度、连接状态和空行
}

bool http_conn::add_content_length(int content_len){        //添加Content-Length，表示响应报文的长度
    return add_response("Content-Length:%d\r\n" , content_len);
}

bool http_conn::add_content_type(){                     //添加文本类型，这里是html
    return add_response("Content-Type:%s" , "text/html");
}

bool http_conn::add_linger(){                   //添加连接状态，通知浏览器端是保持连接还是关闭
    return add_response("Connection:%s\r\n" , (m_linger == true) ? "keep-alive" : "close");
}

bool http_conn::add_blank_line(){
    return add_response("%s" , "\r\n");
}

bool http_conn::add_content(const char *content){               //添加文本content
    return add_response("%s" , content);
}

bool http_conn::process_write(HTTP_CODE ret){           //完成响应报文
    switch (ret){
        case INTERNAL_ERROR:            //内部错误，500
        {
            add_status_line(500 , error_500_title);     //状态行
            add_headers(strlen(error_500_form));            //消息报头
            if(!add_content(error_500_form)){
                return false;
            }
            break;
        }
        case BAD_REQUEST:               //报文语法有误，404
        {
            add_status_line(404 , error_404_title);
            add_headers(strlen(error_404_form));
            if(!add_content(error_404_form)){
                return false;
            }
            break;
        }
        case FORBIDDEN_REQUEST:              //资源没有访问权限，403
        {
            add_status_line(403 , error_403_title);
            add_headers(strlen(error_403_form));
            if(!add_content(error_403_form)){
                return false;
            }
            break;
        }
        case FILE_REQUEST:          //文件存在，200
        {
            add_status_line(200 , ok_200_title);
            if(m_file_stat.st_size != 0){               //如果请求的资源存在
                add_headers(m_file_stat.st_size);
                m_iv[0].iov_base = m_write_buf;         //第一个iovec指针指向响应报文缓冲区，长度指向m_write_idx
                m_iv[0].iov_len = m_write_idx;
                m_iv[1].iov_base = m_file_address;          //第二个iovec指针指向mmap返回的文件指针，长度指向文件大小
                m_iv[1].iov_len = m_file_stat.st_size;
                m_iv_count = 2;
                bytes_to_send = m_write_idx + m_file_stat.st_size;      //发送的全部数据为响应报文头部信息和文件大小
                return true;
            }else{
                const char *ok_string = "<html><body></body></html>";           //如果请求的资源大小为0，则返回空白html文件
                add_headers(strlen(ok_string));
                if(!add_content(ok_string)){
                    return false;
                }
            }
        }
        default:
        {
            return false;
        }     
    }
    m_iv[0].iov_base = m_write_buf;
    m_iv[0].iov_len = m_write_idx;
    m_iv_count = 1;
    bytes_to_send = m_write_idx;
    return true;
}

bool http_conn::write(){
    int temp = 0;
    int newadd = 0;
    if(bytes_to_send == 0){     //若要发送的数据长度为0  表示响应报文为空 
        modfd(m_epollfd , m_sockfd , EPOLLIN , m_TRIGMode);
        init();
        return true;
    }
    while(1){
        temp =  writev(m_sockfd , m_iv , m_iv_count);
        if(temp > 0){
            bytes_have_send += temp;        //更新已发送字节
            newadd = bytes_have_send - m_write_idx;         //偏移文件iovec的指针
        }
        if(temp < 0){
            if(errno == EAGAIN){            //判断缓冲区是否满了
                if(bytes_have_send >= m_iv[0].iov_len){          //第一个iovec头部信息的数据已发送完，发送第二个iovec数据
                    m_iv[0].iov_len = 0;            //不再继续发送头部信息
                    m_iv[1].iov_base = m_file_address + newadd;
                    m_iv[1].iov_len = bytes_to_send;
                }else{          //继续发送第一个iovec头部信息的数据
                    m_iv[0].iov_base = m_write_buf + bytes_to_send;
                    m_iv[0].iov_len = m_iv[0].iov_len - bytes_have_send;
                }
                modfd(m_epollfd , m_sockfd , EPOLLOUT , m_TRIGMode);
                return true;
            }
            unmap();            //如果发送失败，但不是缓冲区问题，取消映射
            return false;
        }

    }
}