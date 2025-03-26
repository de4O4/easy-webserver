#ifndef BLOCK_QUEUE_H
#define BLOCK_QUEUE_H

#include<iostream>
#include<stdlib.h>
#include<pthread.h>
#include<sys/time.h>
#include "../lock/locker.h"
using namespace std;

/**环形队列来实现阻塞队列     为了线程安全  并且每个操作前都需加锁 操作完后解锁  */

template <class T>
class block_queue{              //阻塞队列类
    private:
        locker m_mutex;
        cond m_cond;

        T *m_array;             //类数组
        int m_size;             //数组现存个数
        int m_max_size;         //数组最大容量
        int m_front;              //队头   指向队头前一个位置
        int m_back;               //队尾
    public:
        block_queue(int max_size = 1000){        //初始化数组默认最大容量为1000
            if(max_size <= 0){
                exit(-1);
            }

            m_max_size = max_size;
            m_array = new T[m_max_size];
            m_size = 0;
            m_front = -1;
            m_back = -1;
        }
        ~block_queue(){                 //析构函数 若数组仍存在 则delete
            m_mutex.lock();
            if(m_array != NULL){
                delete [] m_array;
            }
            m_mutex.unlock();
        }
        void clear(){               //清空数组
            m_mutex.lock();
            m_size = 0;
            m_front = -1;
            m_back = -1;
            m_mutex.unlock();
        }
        bool full(){            //判断队列是否为满
            m_mutex.lock();
            if(m_max_size <= m_size){
                m_mutex.unlock();
                return true;
            }
            m_mutex.unlock();
            return false;
        }
        bool empty(){               //判断队列是否为空
            m_mutex.lock();
            if(0 == m_size){
                m_mutex.unlock();
                return true;
            }
            m_mutex.unlock();
            return false;
        }
        bool front(T &value){       //返回队列队首元素
            bool r = empty();
            if(!r){
                m_mutex.lock();
                value = m_array[m_front];
                m_mutex.unlock();
                return true;
            }
            return false;
        }    
        bool back(T &value){           //返回队列队尾元素
            bool r = empty();
            if(!r){
                m_mutex.lock();
                value = m_array[m_back];
                m_mutex.unlock();
                return true;
            }
            return false;
        }
        int size(){         //返回队列的长度既大小
            int tmp = 0;
            m_mutex.lock();
            tmp == m_size;
            m_mutex.unlock();
            return tmp;
        }      
        int max_size(){         //返回队列的最大容量
            int tmp = 0;
            m_mutex.lock();
            tmp = m_max_size;
            m_mutex.unlock();
            return tmp;
        }     
        bool push(const T &item){       //向队列中添加元素
            m_mutex.lock();
            if(m_size >= m_max_size){
                m_cond.broadcast();     //队列已满 通知消费者线程进行处理
                m_mutex.unlock();
                return false;
            }
            m_back = (m_back+1) % m_max_size;       //更新插入元素的位置 既队尾位置
            m_array[back] = item;
            m_size++;
            m_cond.broadcast();     //通知所有等待线程来处理
            m_mutex.unlock();
            return true;
        }
        bool pop(T &item){      //取出队头元素
            bool r = empty();
            if(!r){
                m_mutex.lock();
                m_front = (m_front + 1) % m_max_size;       //更新队头元素位置
                item = m_array[m_front];
                m_size--;
                m_mutex.unlock();
                return true;
            }
        }
        bool pop(T &item , int ms_timeout){
            struct timespec t = {0 , 0};
            struct timeval now = {0 , 0};
            gettimeofday(&now , NULL);      //获取当前的时间
            m_mutex.lock();
            if(m_size <= 0){
                t.tv_sec = now.tv_sec + ms_timeout / 1000;
                t.tv_nsec = (ms_timeout % 1000) * 1000;
                if(!m_cond.timewait(m_mutex.get() , t)){
                    m_mutex.unlock();
                    return false;
                }
            }
            if(m_size <= 0){
                m_mutex.unlock();
                return false;
            }
            m_front = (m_front + 1) % m_max_size;
            item = m_array[m_front];
            m_size--;
            m_mutex.unlock();
            return true;
        }

};
#endif