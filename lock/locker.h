#ifndef LOCKER_H
#define LOCKER_H

#include <exception>
#include <pthread.h>
#include <semaphore.h>

class sem{              //信号量类
    private:
        sem_t m_sem;
    public:
        sem(){
            if(sem_init(&m_sem,0,0) != 0){            //初始化信号量
                throw std::exception();
            }
        }
        sem(int num){
            if(sem_init(&m_sem,0,num) != 0){            //多态 给m_sem特定的值初始化信号量
                throw std::exception();
            }
        }
        ~sem(){
            sem_destroy(&m_sem);                //sem的析构函数 销毁信号量
        }
        bool wait(){
            return sem_wait(&m_sem) == 0;           //等待信号量 并尝试减少 0为成功            
        }
        bool post(){
            return sem_post(&m_sem) == 0;            //增加信号量值，唤醒等待的线程
        }
};
class locker                    //锁类
{
private:
    pthread_mutex_t m_mutex;
public:
    locker(){                                               //初始化锁
        if(pthread_mutex_init(&m_mutex , NULL) != 0){
            throw std::exception();
        }
    }
    ~locker(){
        pthread_mutex_destroy(&m_mutex);
    }
    bool lock(){
        return pthread_mutex_lock(&m_mutex) == 0;
    }
    bool unlock(){
        return pthread_mutex_unlock(&m_mutex) == 0;
    }
    pthread_mutex_t *get(){
        return &m_mutex;
    }
};
class cond                  //条件变量类
{
private:
    pthread_cond_t m_cond;
public:
    cond(){
        if(pthread_cond_init(&m_cond,NULL) != 0){
            throw std::exception();
        }
    }
    ~cond(){
        pthread_cond_destroy(&m_cond);
    }
    bool wait(pthread_mutex_t *m_mutex){                //等待条件变量
        int r = 0;
        r = pthread_cond_wait(&m_cond , m_mutex);
        return r == 0;
    }
    bool timewait(pthread_mutex_t *m_mutex , struct timespec t){            //设置超时等待
        int r = 0;
        r = pthread_cond_timedwait(&m_cond , m_mutex , &t);
        return r == 0;
    }
    bool signal(){                      //唤醒一个等待条件变量的线程
        return pthread_cond_signal(&m_cond) == 0;
    }
    bool broadcast(){                               //唤醒所有等待条件变量的线程
        return pthread_cond_broadcast(&m_cond) == 0;
    }
};

#endif



