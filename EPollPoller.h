#include <vector>
#include "Poller.h"

class epoll_event;

class EPollPoller : public Poller{

public:
    EPollPoller(EventLoop* loop);
    
    ~EPollPoller() override;

    TimeStamp poll(int timeoutMs, ChannelList* activeChannels) override;
    void updateChannel(Channel* channel) override;
    void removeChannel(Channel* channel) override;

private:
    static const int kInitEventListSize = 16;
    
    void fillActiveChannels(int numEvents, ChannelList* activeChannels) const;

    void update(int operation, Channel* channel);

    using EventList = std::vector<epoll_event>;


    int epoll_fd_;
    EventList eventsList_;
};