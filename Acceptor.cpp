#include "Acceptor.h"
#include "Logger.h"
#include "InetAddress.h"
#include <sys/types.h>    
#include <sys/socket.h>
#include <errno.h>
#include <unistd.h>


static int createNoneBlock(){
    int sockfd = ::socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
    if(sockfd < 0){
        LOG_FATAL("%s:%s:%d listen socket create err:%d", __FILE__, __FUNCTION__, __LINE__, errno);
    }
    return sockfd;
}

/**
 * 维护了一个监听的channel和listenFd包裹的socket
 * 仅仅需要一个handleRead，注册在了channel内，当有新连接时，handleRead内部会调用TcpServer作为用户传入的newTcpConnection函数，建立新的conn。
 * 
*/
Acceptor::Acceptor(EventLoop* loop, const InetAddress& listenAddr, bool reusePort)
    :loop_(loop), 
    acceptSocket_(createNoneBlock()),
    acceptChannel_(loop, acceptSocket_.fd()),
    listening_(false){

    acceptSocket_.setReuseAddr(true);
    acceptSocket_.setReusePort(reusePort);
    acceptSocket_.bindAddress(listenAddr);

    acceptChannel_.setReadCallback(std::bind(&Acceptor::handleRead, this));
}

Acceptor::~Acceptor(){
    acceptChannel_.disableAll();
    acceptChannel_.remove();
}

void Acceptor::listen(){
    listening_ = true;
    acceptSocket_.listen();
    acceptChannel_.enableReading();
}


void Acceptor::handleRead(){
    InetAddress peerAddr;
    int connfd = acceptSocket_.accept(&peerAddr);
    if(connfd >= 0){
        if(newConnectionCallback_){
            newConnectionCallback_(connfd, peerAddr);
        }else{
            ::close(connfd);
        }
    }else{
        LOG_ERROR("%s:%s:%d accept , err %d", __FILE__, __FUNCTION__, __LINE__, errno);
        if(errno == EMFILE){
            LOG_ERROR("%s:%s:%d sockfd reach limit, %d", __FILE__, __FUNCTION__, __LINE__, errno);
        }
    }
}