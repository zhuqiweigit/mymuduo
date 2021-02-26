#include "EPollPoller.h"
#include "Logger.h"
#include "Channel.h"
#include <string.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <unistd.h>

namespace{
    const int kNew = -1;
    const int kAdded = 1;
    const int kDeleted = 2;
}

/**
 * 负责维护：map<sockfd, Channel*>类型的一个MapList，里面是它监控的所有channel。
 *          eventsList_是一个<epoll_event>类型的vector，作为epoll_wait的返回参数，传出所有的活跃事件
 * 
 * map<sockfd, Channel*>的维护：可以增删改查自己维护的Channel列表。一般是由 channel update -> eventLoop update -> epoll update执行
 * eventsList_ 是vector<epoll_event>，相当于一个事件暂存的容器，每次epoll_wait的返回数据都存在这里。
 *              会有一个扩容机制：当返回事件个数达到自己的容量时，会resize * 2。
 * 
*/
EPollPoller::EPollPoller(EventLoop* loop):
    Poller(loop), 
    epoll_fd_(::epoll_create1(EPOLL_CLOEXEC)), 
    eventsList_(kInitEventListSize){
    
    if(epoll_fd_ < 0){
        LOG_FATAL("EPollPoller::EPollerPoller");
    }
}

EPollPoller::~EPollPoller(){
    ::close(epoll_fd_);
}

TimeStamp EPollPoller::poll(int timeoutMs, ChannelList* activeChannels){
    int numEvents = epoll_wait(epoll_fd_, &*eventsList_.begin(), static_cast<int>(eventsList_.size()), timeoutMs);
    int savedErrno = errno;
    TimeStamp now(TimeStamp::now());
    if(numEvents > 0){
        LOG_DEBUG("%d events happened.", numEvents);
        fillActiveChannels(numEvents, activeChannels);
        if(numEvents == eventsList_.size()){
            eventsList_.resize(eventsList_.size() * 2);
        }
    }else if(numEvents == 0){
        LOG_DEBUG("nothing happened");
    }else{
        if(savedErrno != EINTR){
            LOG_ERROR("EPOLLPOLLER: poll()");
        }
    }
    return now;
}

void EPollPoller::fillActiveChannels(int numEvents, ChannelList* activeChannels) const{
    for(int i = 0; i < numEvents; i++){
        Channel* channel = static_cast<Channel*>(eventsList_[i].data.ptr);
        channel->set_revents(eventsList_[i].events);
        activeChannels->push_back(channel);
    }
}

void EPollPoller::updateChannel(Channel* channel){
    const int index = channel->index();
    LOG_INFO("function %s, fd %d, events %d, index %d", __FUNCTION__, channel->fd(), channel->events(), index);
    if(index == kNew || index == kDeleted){
        int fd = channel->fd();
        if(index == kNew){
            channels_[fd] = channel;
        }

        channel->set_index(kAdded);
        update(EPOLL_CTL_ADD, channel);
    }else{
        int fd = channel->fd();
        if(channel->isNoneEvent()){
            update(EPOLL_CTL_DEL, channel);
            channel->set_index(kDeleted);
        }else{
            update(EPOLL_CTL_MOD, channel);
        }
    }
}
    
void EPollPoller::removeChannel(Channel* channel){
    int fd = channel->fd();
    int index = channel->index();
    size_t sz = channels_.erase(fd);

    if(index == kAdded){
        update(EPOLL_CTL_DEL, channel);
    }
    channel->set_index(kNew);
}

void EPollPoller::update(int operation, Channel* channel){
    
    epoll_event epevt;
    memset(&epevt, 0, sizeof epevt);
    
    epevt.events = channel->events();
    epevt.data.ptr = channel;
    int fd = channel->fd();

    if(::epoll_ctl(epoll_fd_, operation, fd, &epevt) < 0){
        if(operation == EPOLL_CTL_DEL){
            LOG_ERROR("epoll_ctl op %d", operation);
        }else{
            LOG_FATAL("epoll_ctl op %d", operation);
        }
    }
}
    

