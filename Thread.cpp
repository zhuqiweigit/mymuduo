#include "Thread.h"
#include "CurrentThread.h"
#include <semaphore.h>


std::atomic_int Thread::numCreated_(0);

/**
 *  基于对象风格的多线程编程：
 *  内部维护：std::shared_ptr<std::thread> thread_ 和 一个用户传入的待执行函数
 *  当创建Thread对象的时候，还没有创建底层的thread
 *  直到外界调用start函数，则创建一个thread，传入用户的函数
 * 
 *  sem_t 信号量：在start函数中用到，因为tid_只能在新线程里才能拿到。sem_t就能保证，在新线程里设置了tid_之后，旧线程的start函数才会返回
 * 
*/

Thread::Thread(ThreadFunction func, const std::string& name)
    :started_(false), joined_(false), tid_(0),
    func_(std::move(func)),
    name_(name){

        setDefaultName();
}

Thread::~Thread()
{
    if (started_ && !joined_)
    {
        thread_->detach(); // thread类提供的设置分离线程的方法
    }
}

void Thread::setDefaultName(){
    int n = ++numCreated_;
    if(name_.empty()){
        char buf[32];
        snprintf(buf, sizeof buf, "thread %d", n);
        name_ = buf;
    }
}

void Thread::start(){
    started_ = true;
    sem_t sem;
    sem_init(&sem, 0, 1);

    //pv操作，保证线程创建完成，tid设置正确后，start函数才返回
    thread_ = std::shared_ptr<std::thread>( new std::thread( [&](){
        tid_ = CurrentThread::tid();
        sem_post(&sem);
        func_();
    } ));

    sem_wait(&sem);
}

void Thread::join(){
    joined_ = true;
    thread_->join();
}