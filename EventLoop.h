#pragma once
#include <functional>
#include <vector>
#include <atomic>
#include <memory>
#include <mutex>
#include "noncopyable.h"
#include "TimeStamp.h"
#include "CurrentThread.h"

class Channel;
class Poller;

class EventLoop : noncopyable{
public:
    using Functor = std::function<void()>;

    EventLoop();
    ~EventLoop();

    void loop();
    void quit();

    TimeStamp pollReturnTime() const {
        return pollReturnTime_;
    }

    void runInLoop(Functor cb);
    void queueInLoop(Functor cb);

    void wakeup();

    void updateChannel(Channel*);
    void removeChannel(Channel*);
    bool hasChannel(Channel*);

    bool isInLoopThread() const{
        return CurrentThread::tid() == threadId_;
    }

private:
    void handleRead();
    void doPendingFunctors();

    using ChannelList = std::vector<Channel*>;
    std::atomic_bool looping_;
    std::atomic_bool quit_;

    const pid_t threadId_;

    TimeStamp pollReturnTime_;
    std::unique_ptr<Poller> poller_;

    int wakeupFd_;
    std::unique_ptr<Channel> wakeupChannel_;

    ChannelList activeChannels_;

    std::atomic_bool callPendingFunctors_;
    std::vector<Functor> pendingFunctors_;
    std::mutex mutex_;  //mutex 用于保护 vector<Functor> pendingFunctors_

};