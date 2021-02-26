#include "EventLoopThread.h"
#include "EventLoop.h"
#include <functional>

/**
 * 维护一个loop、thread；
 * 其中thread内部的真正thread由Thread类在调用start方法后创建
 * loop的创建：startLoop函数负责调用Thread类的start方法，从此新线程开始运行。在旧线程内使用条件变量等待新线程把loop创建完成，然后返回loop。
 *            新线程则一直调用loop循环。直到退出loop后，使用互斥量将loop_设置为nullptr
 *与Thread类的区别：Thread类是通用的，可以执行任何线程函数。
  EventLoopThread类的功能：创建loop、利用Thread线程类执行Loop类的loop函数
*/

EventLoopThread::EventLoopThread(const ThreadInitCallBack& cb , const std::string &name)
    : loop_(nullptr), exiting_(false), 
    thread_(std::bind(&EventLoopThread::threadFunc, this), name), mutex_(), cond_(), callback_(cb){

}

EventLoopThread::~EventLoopThread(){
    exiting_ = true;
    if(loop_ != nullptr){
        loop_->quit();
        thread_.join();
    }
}

EventLoop* EventLoopThread::startLoop(){
    thread_.start();
    EventLoop *loop = nullptr;
    {
        std::unique_lock<std::mutex> lock(mutex_);
        while(loop_ == nullptr){
            cond_.wait(lock);
        }
        loop = loop_;

    }
    return loop;
}


void EventLoopThread::threadFunc(){
    EventLoop loop;
    if(callback_){
        callback_(&loop);
    }
    {
        std::unique_lock<std::mutex> lock(mutex_);
        loop_ = &loop;
        cond_.notify_one();
    }
    loop.loop();
    std::unique_lock<std::mutex> lock(mutex_);
    loop_ = nullptr;
}