#include "Socket.h"
#include "InetAddress.h"
#include "Logger.h"

#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <netinet/tcp.h>

/**
 * 维护了一个sockFd：这个fd由外界传入。对于Acceptor，它的fd是整个baseLoop的listenFd，是由static::createNoBlock{ ::socket(...)} 直接创建的、
 *                  对于TcpConnection的fd，是由epoll通知，有可读事件，然后Acceptor调用自己的socket，然后Accept函数拿到的
 * 
 * 包装了sock的常用流程函数
 * 绑定：接受一个InetAddress，根据这个InetAddress设置绑定本地端口
 * listen：对listenFd进行监听；
 * shutdownWrite：单方面关闭
 * Accept：拿到一个connectionFd，并且填充一个InetAddress
 * 
*/


Socket::~Socket(){
    ::close(sockfd_);
}

void Socket::bindAddress(const InetAddress& localAddr){
    if(0 != ::bind(sockfd_, (sockaddr*)localAddr.getSockaddr(), sizeof(sockaddr_in))){
        LOG_FATAL("bind sockfd %d fail", sockfd_);
    }
}

void Socket::listen(){
    if(0 != ::listen(sockfd_, 1024)){
        LOG_FATAL("listen sockfd %d fail", sockfd_);
    }
}

int Socket::accept(InetAddress *peerAddr){
    sockaddr_in addr;
    socklen_t len = sizeof addr;
    memset(&addr, 0, len);
    int connfd = ::accept4(sockfd_, (sockaddr*)&addr, &len, SOCK_NONBLOCK | SOCK_CLOEXEC);
    if(connfd > 0){
        peerAddr->setSockaddr(addr);
    }
    return connfd;
}

void Socket::shutdownWrite(){
    if(::shutdown(sockfd_, SHUT_WR) < 0){
        LOG_ERROR("shutdown fd %d", sockfd_);
    }
}

void Socket::setTcpNoDeley(bool on){
    int optval = on ? 1 : 0;
    ::setsockopt(sockfd_, IPPROTO_TCP, TCP_NODELAY, &optval, sizeof(optval));
}

void Socket::setReuseAddr(bool on){
    int optval = on ? 1 : 0;
    ::setsockopt(sockfd_, IPPROTO_TCP, SO_REUSEADDR, &optval, sizeof(optval));
}

void Socket::setReusePort(bool on){
    int optval = on ? 1 :0;
    ::setsockopt(sockfd_, IPPROTO_TCP, SO_REUSEPORT, &optval, sizeof optval);
}

void Socket::setKeepAlive(bool on){
    int optval = on ? 1 : 0;
    ::setsockopt(sockfd_, IPPROTO_TCP, SO_KEEPALIVE, &optval, sizeof optval);
}
