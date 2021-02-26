#pragma once
#include "noncopyable.h"
#include "TimeStamp.h"
#include <memory>
#include <functional>

class EventLoop;

class Channel : noncopyable{
public:
    typedef std::function<void()> EventCallback;
    typedef std::function<void(TimeStamp)> ReadEventCallback;

    Channel(EventLoop* loop, int fd);
    ~Channel() = default;

    void handleEvent(TimeStamp receiveTime);
    void setReadCallback(ReadEventCallback cb){
        readCallback_ = std::move(cb);
    }
    void setWriteCallback(EventCallback cb){
        writeCallback_ = std::move(cb);
    }
    void setCloseCallback(EventCallback cb){
        closeCallback_ = std::move(cb);
    }
    void setErrorCallback(EventCallback cb){
        errorCallback_ = std::move(cb);
    }

    void tie(const std::shared_ptr<void>&);

    int fd() const { 
        return fd_; 
    }

    int events()const{
        return events_;
    }

    void set_revents(int revt){
        revents_ = revt;
    }

    bool isNoneEvent(){
        return events_ == kNoneEvent;
    }

    void enableReading(){
        events_ |= kReadEvent;
        update();
    }

    void disableReading(){
        events_ &= ~kReadEvent;
        update();
    }

    void enableWriting(){
        events_ |= kWriteEvent;
        update();
    }

    void disableWriting(){
        events_ &= ~kWriteEvent;
        update();
    }

    void disableAll(){
        events_ = kNoneEvent;
        update();
    }

    bool isWriting(){
        return events_ & kWriteEvent;
    }

    bool isReading(){
        return events_ & kReadEvent;
    }

    int index(){
        return index_;
    }

    void set_index(int idx){
        index_ = idx;
    }

    EventLoop* ownerLoop(){
        return loop_;
    }

    void remove();

private:
    EventLoop* loop_;
    const int fd_;
    int events_;
    int revents_;
    int index_;
    
    std::weak_ptr<void> tie_;
    bool tied_;

    ReadEventCallback readCallback_;
    EventCallback writeCallback_;
    EventCallback closeCallback_;
    EventCallback errorCallback_;

    static const int kNoneEvent;
    static const int kReadEvent;
    static const int kWriteEvent;

    void update();
    void handleEventWithGuard(TimeStamp receiveTime);

};