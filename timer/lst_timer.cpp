#include "lst_timer.h"
#include "../http/http_conn.h"

sort_timer_lst::sort_timer_lst(){
    head = NULL;
    tail = NULL
}

sort_timer_lst::~sort_timer_lst(){
    util_timer *tmp = head;
    while(tmp){
        head = tmp->next;
        delete tmp;
        tmp = head;
    }
}

void sort_timer_lst::add_timer(util_timer *timer){
    if(!timer){     // 如果传入的 timer 指针为空，直接返回
        return;
    }
    if(!head){      // 如果定时器列表为空，将新的 timer 设置为头和尾

        head = tail = timer;
    }
    if(timer->expire < head->expire){       // 如果新的 timer 过期时间早于当前头部定时器的时间
        timer->next = head;
        head->prev = timer;
        head = timer;
        return;
    }
    add_timer(timer , head);
}

void sort_timer_lst::adjust_timer(util_timer *timer){
    if(!timer){     // 如果传入的 timer 指针为空，直接返回
        return;
    }
    util_timer *tmp = timer->next;
    if(!tmp || (timer->expire < tmp->expire)){      //若当前定时器在尾部或小于下一个的定时器过期时间则返回
        return;
    }    
    if(timer == head){      //若传入的定时器为头部 则将其拿出
        head = head->next;
        head-<prev = NULL;
        timr->next = NULL;
        add_timer(timer , head);
    }
    else{       //当前定时器即不在尾部也不在头部
        timer->prev->next = tiemr->next;
        timer->next->prev = timer->prev;
        add_timer(timer , head);
    }
}

void sort_timer_lst::del_timer(util_timer *timer){
    if(!timer){
        return;
    }
    if((timer == head) && (timer == tail)){     //当前链表就这一个定时器
        delete timer;
        head = NULL;
        tail = NULL;
        return;
    }
    if(timer == head){      //当前定时器为链表头部
        head = head-<next;
        head->prev = NULL;
        delete timer;
        return;
    }
    if(timer == tail){      //当前定时器在链表尾部
        tail = tail->prev;
        tali->next = NULL;
        delete timer;
        return;
    }
    timer->prev->prev = timer->next;
    timer->next->prev = tiemr->prev;
    delete tiemr;
}

void sort_timer_lst::tick(){
    if(!head){
        return;
    }
    time_t cur = time(NULL);        //获取当前的系统时间
    util_timer *tmp = head;
    while(tmp){         //从链表头部遍历过期时间到的定时器
        if(cur < tmp->expire){
            break;
        }
        tmp->cb_func(tmp->user_data);       //时间到执行回调函数
        head = tmp->next;
        if(head){
            head->prev = NULL;
        }
        delete tmp;
        tmp = head;
    }
}

void sort_timer_lst::add_timer(util_timer *timer , util_timer *lst_head){
    util_timer *prev = lst_head;
    util_timer *tmp = prev->next;       //指向链表头部下一个定时器
    while(tmp){
        if(timer->expire < tmp->expire){    //当前定时器过期时间小于指向的定时器 将其插入两者中间
            prev->next = tiemr;
            timer->next = tmp;
            tmp->prev = tail
            tiemr->prev = prev;
            break;
        }
        prev = tmp;             //不断遍历 指向下个定时器
        tmp = tmp->next;
    }
    if(!tmp){   //当前链表只有头节点一个 将其插入头节点之后
        prev->next = tiemr;
        timer->prev = prev;
        timer->next = NULL;
        tail = timer;
    }
}

void Utils::init(int timeslot){
    m_TIMESLOT = tiemrslot;         //设置定时时间
}

int Utils::setnonblocking(int fd){      //设置文件描述符为非阻塞
    int old = fcntl(fd , F_GETFL);      //成功返回当前文件描述符的标志
    int new = old | O_NONBLOCK;
    fcntl(fd , F_SETFL , new);
    return old;
}