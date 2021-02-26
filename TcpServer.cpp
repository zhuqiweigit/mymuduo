#include "TcpServer.h"  
#include "TcpConnection.h"
#include "Logger.h"
#include <string.h>


static EventLoop* CheckLoopNotNull(EventLoop *loop)
{
    if (loop == nullptr)
    {
        LOG_FATAL("%s:%s:%d mainLoop is null! \n", __FILE__, __FUNCTION__, __LINE__);
    }
    return loop;
}



/**
 * TcpServer:
 * 维护了一个acceptor、一个connections的map、一个threadPool
 * acceptor: 启动listen、设置newCOnnection回调
 * connection：设置从map中删除conn、destory connection(关闭channel等)的close回调。这是被动关闭，用于客户端主动关闭后，跟着关闭的方法
 * threadPool: 设置数量 + 启动
 * 析构函数：主动关闭。即server析构的时候，disable Channel，删除channel
 * 
 * TcpServer的start：1. 启动线程池   2.在main loop 中启动listen
*/


TcpServer::TcpServer(EventLoop *loop, const InetAddress& listenAddr, 
        const std::string& name, Option option)
        :loop_(CheckLoopNotNull(loop)), 
        acceptor_(new Acceptor(loop, listenAddr, option == kReusePort)), 
        ipPort_(listenAddr.toIpPort()), 
        name_(name),
        threadPool_(new EventLoopThreadPool(loop, name)),
        connectionCallback_(),
        messageCallback_(),
        nextConnId(1),
        started_(0)
        {

        acceptor_->setNewConnectionCallback(std::bind(&TcpServer::newConnection, this, std::placeholders::_1, std::placeholders::_2));
}

TcpServer::~TcpServer(){
    for(auto item : connections_){
        TcpConnectionPtr conn(item.second);
        item.second.reset();
        conn->getLoop()->runInLoop(std::bind(&TcpConnection::connectionDestroyed, conn));
    }
}



void TcpServer::setThreadNum(int numThreads){
    threadPool_->setThreadNum(numThreads);
}

void TcpServer::start(){
    if(started_++ == 0){
        threadPool_->start(threadInitCallback_);
        loop_->runInLoop(std::bind(&Acceptor::listen, acceptor_.get()));
    }
}


void TcpServer::newConnection(int sockFd, const InetAddress& peerAddress){
    EventLoop *ioLoop = threadPool_->getNextLoop();
    char buf[64] = {0};
    snprintf(buf, sizeof buf, "-%s#%d", ipPort_.c_str(), nextConnId);
    ++nextConnId;
    std::string connName = name_ + buf;

    LOG_INFO("TcpServer::newConnection [%s] - new connection [%s] from %s",
        name_.c_str(), connName.c_str(), peerAddress.toIpPort().c_str());
    
    sockaddr_in local;
    memset(&local, 0, sizeof(sockaddr_in));
    socklen_t addrlen = sizeof local;
    if(::getsockname(sockFd, (sockaddr*)&local, &addrlen) < 0){
        LOG_ERROR("sockets:getLocalAddr");
    }
    InetAddress localAddr(local);

    TcpConnectionPtr conn(new TcpConnection(ioLoop,
        connName,
        sockFd,
        localAddr,
        peerAddress));
    connections_[connName] = conn;
    conn->setConnectionCallback(connectionCallback_);
    conn->setMessageCallback(messageCallback_);
    conn->setWriteCompleleCallback(writeCompleteCallback_);

    conn->setCloseCallback(
        std::bind(&TcpServer::removeConnection, this, std::placeholders::_1)
    );
    ioLoop->runInLoop(std::bind(&TcpConnection::connectionEstablished, conn));
}

void TcpServer::removeConnection(const TcpConnectionPtr& conn){
    loop_->runInLoop(std::bind(&TcpServer::removeConnectionInLoop, this, conn));
}

void TcpServer::removeConnectionInLoop(const TcpConnectionPtr& conn){
    LOG_INFO("TcpServer::removeConnectionInLoop [%s] - connection %s\n", 
        name_.c_str(), conn->name().c_str());
    connections_.erase(conn->name());
    EventLoop *ioLoop = conn->getLoop();
    ioLoop->queueInLoop(
        std::bind(&TcpConnection::connectionDestroyed, conn)
    );
}