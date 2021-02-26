#include "EventLoopThreadPool.h"
#include "EventLoopThread.h"



/**
 * 维护了一个EventLoopThread的vector，和一个Loop的vector
 * 本class的函数运行在主线程
 * start函数：负责用循环new EventLoopThread，并且调用EventLoopThread的start函数启动多线程；
 *          EventLoopThread的start函数内部会调用Thread的start，Thread的start内会创建线程，并放入线程函数并执行线程函数。
 *          线程函数内部，会创建一个函数作用域的loop，并把这个loop设置在EventLoopThread的成员变量上，然后线程函数会一直执行loop。这里
 *          需要同步，因为创建loop并设置，是在新线程完成的，而EventLoopThread的start函数需要保证返回时，loop已经设置好，因此使用条件变量来进行
 *          新线程和原线程的线程同步。
 * 
 * getNextLoop：利用next_变量返回下一个loop即可。
*/

EventLoopThreadPool::EventLoopThreadPool(EventLoop* baseLoop, const std::string& nameArg)
    : baseLoop_(baseLoop), name_(nameArg), started_(false), numThreads_(0), next_(0){

}

EventLoopThreadPool::~EventLoopThreadPool(){ }

void EventLoopThreadPool::start(const ThreadInitCallback& cb){
    started_ = true;
    for(int i = 0; i < numThreads_; ++i){
        char buf[name_.size() + 32];
        snprintf(buf, sizeof(buf), "%s %d", name_.c_str(), i);
        EventLoopThread *t = new EventLoopThread(cb, buf);
        threads_.push_back(std::unique_ptr<EventLoopThread>(t));
        loops_.push_back(t->startLoop());
    }
    if(numThreads_ == 0 && cb){
        cb(baseLoop_);
    }
}


EventLoop* EventLoopThreadPool::getNextLoop(){
    EventLoop* loop = baseLoop_;
    if(!loops_.empty()){
        loop = loops_[next_++];
        if(next_ >= loops_.size()){
            next_ = 0;
        }
    }
    return loop;
}

std::vector<EventLoop*> EventLoopThreadPool::getAllLoops(){
    if(loops_.empty()){
        return std::vector<EventLoop*>(1, baseLoop_);
    }else{
        return loops_;
    }
}