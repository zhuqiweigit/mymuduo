#include "Channel.h"
#include "EventLoop.h"
#include "Logger.h"

#include <sys/epoll.h>

const int Channel::kNoneEvent = 0;
const int Channel::kReadEvent = EPOLLIN | EPOLLPRI;
const int Channel::kWriteEvent = EPOLLOUT;

/**
 * Channel: 主要维护了tie<void> tied、events、revents
 * weak_ptr tie<void>和tied：由于一个TcpConnection会持有一个Channel，并且Channel负责通知这个conn，是否有事件发生。
 *          但是TcpConnection很可能被析构掉，因此需要在执行回调函数之前，先检查conn是否还存在。
 *          Acceptor也会持有Channel，Channel也需要给Acceptor做通知，但是Acceptor的fd是listenFd，不会断开连接。不需要检测tie
 * events：由用户来设置。channel的用户是TCPConnection和Acceptor。
 * revents：由EpollPoller进行epollwait之后，回填。然后Channel就可以根据revents来知道执行哪种回调
 * 
 * Channel还负责维护自己：维护路径：channel update -> loop update -> epoll update
 *      channel update自己的事件：TcpConnection中新建立的Channel，只有在调用disableAll等更新函数后，才会被自动添加到Epoll和它的Map中
 *      channel update自己
 *      channel remove自己
 *      
*/


Channel::Channel(EventLoop* loop, int fd)
    : loop_(loop), fd_(fd), events_(0), revents_(0), index_(-1), tied_(false)
{

}

void Channel::tie(const std::shared_ptr<void>& obj){
    tie_ = obj;
    tied_ = true;
}

void Channel::update(){
    loop_->updateChannel(this);
}

void Channel::remove(){
    loop_->removeChannel(this);
}

void Channel::handleEvent(TimeStamp receiveTime){
    if(tied_){
        std::shared_ptr<void> guard = tie_.lock();
        if(guard){
            handleEventWithGuard(receiveTime);
        }
    }else{
        handleEventWithGuard(receiveTime);
    }
}

void Channel::handleEventWithGuard(TimeStamp receiveTime){
    LOG_INFO("channel handleEvents: %d\n", revents_);
    if((revents_ & EPOLLHUP) && !(revents_ & EPOLLIN)){
        if(closeCallback_){
            closeCallback_();
        }
    }
    if(revents_ & EPOLLERR){
        if(errorCallback_){
            errorCallback_();
        }
    }
    if(revents_ & (EPOLLIN | EPOLLPRI)){
        if(readCallback_){
            readCallback_(receiveTime);
        }
    }
    if(revents_ & EPOLLOUT){
        if(writeCallback_){
            writeCallback_();
        }
    }

}