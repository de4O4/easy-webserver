#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <list>
#include <cstdio>
#include <exception>
#include <pthread.h>
#include "../lock/locker.h"
#include "../CGImysql/sql_connection_pool.h"

template <typename T>
class threadpool{           //线程池类
    private:
        int m_thread_number;        //线程池中的线程数
        int m_max_requests;         //请求队列中所运训的最大请求数
        pthread_t *m_threads;       //描述线程池的数组
        std::list<T *> m_workqueue;     //请求队列既任务队列
        locker m_queuelocker;      //保护请求队列的互斥锁
        sem m_queuestat;        //信号量 代表是否有任务要处理
        connection_pool *m_connPool;        //数据库连接池
        int m_actor_model;          //模型切换

        static void *worker(void *arg);     //工作线程运行的函数 不断从任务队列中取出任务并执行
        void run();

    public:
        threadpool(int actor_model , connection_pool *connPool , int thread_number = 8 , int max_request = 10000);        
        ~threadpool();
        bool append(T *request , int state);
        bool append_p(T *request);
};

template <typename T>
threadpool<T>::threadpool(int actor_model , connection_pool *connPool , int thread_number , int max_requests) : m_actor_model(actor_model)  ,m_thread_number(thread_number) , m_max_requests(max_requests) , m_threads(NULL) , m_connPool(connPool){
    if(thread_number <= 0 || max_requests <= 0){
        throw std::exception();
    }
    m_threads = new pthread_t[m_thread_number];     //new一个线程池数组 里面保存每个线程的线程号
    if(!m_threads){
        throw std::exception();
    }
    for(int i = 0 ; i < m_thread_number ; i++){         //创建m_thread_number个线程
        if(pthread_create(m_threads + i , NULL , worker , this) != 0){          //创建线程失败
            delete [] m_threads;
            throw std::exception();
        }
        if(pthread_detach(m_threads[i])){           //分离线程 线程会在执行完毕后自动释放资源
            delete [] m_threads;
            throw std::exception();
        }            
    }
}

template <typename T>
threadpool<T>::~threadpool(){
    delete [] m_threads;
}

template <typename T>
bool threadpool<T>::append(T *request , int state){         //将任务添加至任务队列 reactor模式
    m_queuelocker.lock();
    if(m_workqueue.size() >= m_max_requests){           //现存容量大于最大可承接任务数
        m_queuelocker.unlock();
        return false;
    }
    request->m_state = state;
    m_workqueue.push_back(request);
    m_queuelocker.unlock();
    m_queuestat.post();         //有任务放入任务队列 则信号量++
    return true;
}

template <typename T>
bool threadpool<T>::append_p(T *request){
    m_queuelocker.lock();
    if(m_workqueue.size() >= m_max_requests){
        m_queuelocker.unlock();
        return false;
    }
    m_workqueue.push_back(request);
    m_queuelocker.unlock();
    m_queuestat.post();
    return true;
}

template <typename T>
void *threadpool<T>::worker(void *arg){
    threadpool *pool = (threadpool *)arg;
    pool->run();
    return pool;
}

template <typename T>
void threadpool<T>::run(){
    while(1){
        m_queuestat.wait();         //取出任务 则信号量--
        m_queuelocker.lock();
        if(m_workqueue.empty()){
            m_queuelocker.unlock();
            continue;               //若请求队列为空则跳出循环
        }
        T *request = m_workqueue.front();       //取出任务队列队头元素
        m_workqueue.pop_front();            //删除任务队列队头元素
        m_queuelocker.unlock();
        if(!request){       //任务为NULL则跳出循环
            continue;
        }
        if(1 == m_actor_model){             //反应堆模型 1为Reactor模型    0为Proactor模型
            if(request->m_state == 0){      //m_state为读写状态 0为读 1为写
                if(request->read_once()){
                    request->improv = 1;
                    connectionRAII mysqlcon(&request->mysql , m_connPool);          //实例化一个数据库连接
                    request->process();
                }
                else{
                    request->improv = 1;
                    request->timer_flag = 1;
                }
            }
            else{           //为写事件
                if(request->write()){
                    request->improv = 1;
                }
                else{
                    request->improv = 1;
                    request->timer_flag = 1;
                }
            }
        }
        else{               //为proactor模型
            connectionRAII mysqlcon(&request->mysql , m_connPool);
            request->process();
        }
    }
}


#endif 