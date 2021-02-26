#pragma once
#include <functional>
#include <atomic>
#include <unordered_map>
#include <string>

#include "Callbacks.h"
#include "EventLoop.h"
#include "InetAddress.h"
#include "Acceptor.h"
#include "EventLoopThreadPool.h"
#include "noncopyable.h"

class TcpServer : noncopyable{
public:
    using ThreadInitCallback = std::function<void(EventLoop* loop)>;

    enum Option{
        kNonReusePort,
        kReusePort
    };

    TcpServer(EventLoop *loop, const InetAddress& listenAddr, const std::string& name, Option option = kNonReusePort);
    ~TcpServer();

    void setThreadInitcallback(const ThreadInitCallback &cb) { threadInitCallback_ = cb; }
    void setConnectionCallback(const ConnectionCallback &cb) { connectionCallback_ = cb; }
    void setMessageCallback(const MessageCallback &cb) { messageCallback_ = cb; }
    void setWriteCompleteCallback(const WriteCompleteCallback &cb) { writeCompleteCallback_ = cb; }

    void setThreadNum(int numThreads);

    void start();

private:
    void newConnection(int sockFd, const InetAddress& peerAddress);
    void removeConnection(const TcpConnectionPtr& conn);
    void removeConnectionInLoop(const TcpConnectionPtr& conn);

    using ConnectionMap = std::unordered_map<std::string, TcpConnectionPtr>;

    EventLoop *loop_;

    const std::string ipPort_;
    const std::string name_;

    std::unique_ptr<Acceptor> acceptor_;
    std::shared_ptr<EventLoopThreadPool> threadPool_;

    ConnectionCallback connectionCallback_;
    MessageCallback messageCallback_;
    WriteCompleteCallback writeCompleteCallback_;

    ThreadInitCallback threadInitCallback_;
    
    std::atomic_int started_;

    int nextConnId;
    ConnectionMap connections_;
};