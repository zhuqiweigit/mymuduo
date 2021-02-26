#include "EventLoop.h"
#include "Poller.h"
#include "CurrentThread.h"
#include "Logger.h"
#include "Channel.h"

#include <sys/eventfd.h>

/**
 * 维护了一个pendingFunctors，是vector保存的一堆待执行函数。由mutex负责保护
 * 维护了一个vector<channel*>activeChannels_，和poller里的eventsList_一样，临时存储活跃的channel。由poller负责写入
 * 维护了一个wakeupFd_和wakeupChannel，并在这个wakeupChannel上设置了handleRead函数，就是读取8个字节无意义数据，用于唤醒epoll
 * 维护了一个poller，不用解释
 * 维护了std::atomic_bool looping_ 和 quit_变量，用于给loop循环看，是否循环，是循环还是退出
 * 
 * loop函数：先epoll_wait，找到活跃的channel，执行上面的回调函数；
 *          然后查看pendingFactors上是否有任务，有的话就全部执行掉。
 * 
 * quit函数：修改状态为quit_，然后检查是否是在本线程内。如果是在本线程内，说明这个quit函数肯定是在本线程的loop函数内执行的（muduo的所有函数都是在loop内执行的），
 *          那么肯定能运行到下一轮while，检查quit_标志，就可以退出了。
 *          但如果这个quit函数不是在本线程执行的，是由在其他线程内调用loop的quit_函数，则本loop有可能还阻塞在epoll_wait上，需要先唤醒一下
 * 
 * runInLoop函数：先检查是否在本线程内，如果在的话，说明runInLoop可能运行在本线程的doPendingFunctors内，所以直接执行runInLoop的cb就行。
 *          如果不在本线程内，就需要加入所持有的loop的queue，并wakeUp这个loop（这个loop可能阻塞在epoll_wait，也可能没阻塞，正在执行pendingFunctions或ActiveChannel），
 *          但无论如何唤醒一下总是没错的。
 * 
*/

__thread EventLoop* t_loopInThisThread = nullptr;

const int kPollTimeMs = 10000;

int creatEventFd(){
    int evtFd = ::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    if(evtFd < 0)
        LOG_FATAL("eventFd err, errno: %d\n", errno);
    return evtFd;
}

EventLoop::EventLoop() : looping_(false), quit_(false), threadId_(CurrentThread::tid()),
    poller_(Poller::newDefaultPoller(this)),
    wakeupFd_(creatEventFd()),
    wakeupChannel_(new Channel(this, wakeupFd_)),
    callPendingFunctors_(false){
    
    LOG_DEBUG("EventLoop create : %p in thread %d \n", this, threadId_);

    if(::t_loopInThisThread != nullptr)
        LOG_FATAL("another eventLoop has been created in this thread: %p", this);
    t_loopInThisThread = this;

    wakeupChannel_->setReadCallback(std::bind(&EventLoop::handleRead, this));
    wakeupChannel_->enableReading();
}

EventLoop::~EventLoop(){
    wakeupChannel_->disableAll();
    wakeupChannel_->remove();
    ::close(wakeupFd_);
    t_loopInThisThread = nullptr;
}

void EventLoop::handleRead(){
    uint64_t one = 1;
    size_t n = ::read(wakeupFd_, &one, sizeof one);
    if(n != sizeof one){
        LOG_ERROR("EventLoop::handleRead() read %ld bytes data", n);
    }
}


//事件循环就这一个
void EventLoop::loop(){
    looping_ = true;
    quit_ = false;

    LOG_INFO("eventLoop %p start looping\n", this);

    while(!quit_){

        activeChannels_.clear();
        
        pollReturnTime_ = poller_->poll(kPollTimeMs, &activeChannels_);
        
        //处理用户业务事件 + 可能的读8字节wakefd
        for(auto channel : activeChannels_){
            channel->handleEvent(pollReturnTime_);
        }

        //处理非用户事件
        doPendingFunctors();
    }
}

void EventLoop::quit(){
    quit_ = true;
    if(!isInLoopThread()){
        wakeup();
    }
}

void EventLoop::runInLoop(Functor cb){
    if(isInLoopThread()){
        cb();
    }else{
        queueInLoop(cb);
    }
}

void EventLoop::queueInLoop(Functor cb){
    {
        std::unique_lock<std::mutex> lock(mutex_);
        pendingFunctors_.emplace_back(cb);
    }
    //1. 其它线程持有本loop，并且插入了任务函数，则需要唤醒本线程
    //2. 本线程在执行pendingFunctors的时候，某些任务又需要插入新任务，则需要再次唤醒一下epoll。这样，在执行完缓存的本队列后，
    //    epoll又会触发并再次装填待执行任务
    if(!isInLoopThread() || callPendingFunctors_){
        wakeup();
    }
}

void EventLoop::wakeup(){
    uint64_t one = 1;
    size_t n = write(wakeupFd_, &one, sizeof one);
    if(n != sizeof one){
        LOG_ERROR("EventLoop::wakeup() write %lu bytes," , n);
    }
}

void EventLoop::updateChannel(Channel* channel){
    poller_->updateChannel(channel);
}

void EventLoop::removeChannel(Channel* channel){
    poller_->removeChannel(channel);
}

bool EventLoop::hasChannel(Channel* channel){
    return poller_->hasChannel(channel);
}

void EventLoop::doPendingFunctors(){
    std::vector<Functor> functors;
    callPendingFunctors_ = true;
    {
        std::unique_lock<std::mutex> lock(mutex_);
        functors.swap(pendingFunctors_);
    }

    for(auto f : functors){
        f();
    }

    callPendingFunctors_ = false;
}
