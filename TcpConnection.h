#pragma once

#include "noncopyable.h"
#include "InetAddress.h"
#include "Callbacks.h"
#include "Buffer.h"
#include <string>
#include <atomic>
#include <memory>

class EventLoop;
class Channel;
class Socket;

class TcpConnection : noncopyable, public std::enable_shared_from_this<TcpConnection>{
public:
    TcpConnection(EventLoop* loop, const std::string& name, int sockfd, const InetAddress& localAddr, 
                    const InetAddress& peerAddr);
    ~TcpConnection();

    EventLoop* getLoop(){
        return loop_;
    }

    const std::string& name(){
        return name_;
    }

    const InetAddress& localAddress(){
        return localAddr_;
    }

    const InetAddress& peerAddress(){
        return peerAddr_;
    }

    bool connected(){
        return state_ == kConnected;
    }

    void send(const std::string& buf);

    void shutdown();

    void setConnectionCallback(const ConnectionCallback& cb){
        connectionCallback_ = cb;
    }

    void setMessageCallback(const MessageCallback& cb){
        messageCallback_ = cb;
    }

    void setWriteCompleleCallback(const WriteCompleteCallback& cb){
        writeCompleteCallback_ = cb;
    }

    void setHighWaterMarkCallback(const HighWaterMarkCallback& cb){
        highWaterMarkCallback_ = cb;
    }

    void setCloseCallback(const CloseCallback& cb){
        closeCallback_ = cb;
    }

    void connectionEstablished();

    void connectionDestroyed();


private:
    enum StateE {kDisconnected, kConnected, kConnecting, kDisconnecting};

    void setState(StateE state){ 
        state_ = state;
    }

    void handleRead(TimeStamp);
    void handleWrite();
    void handleClose();
    void handleError();

    void sendInLoop(const void* message, size_t len);
    void shutdownInLoop();

    EventLoop *loop_;
    const std::string name_;
    std::atomic_int state_;
    bool reading_;

    std::unique_ptr<Channel> channel_;
    std::unique_ptr<Socket> socket_;

    const InetAddress localAddr_;
    const InetAddress peerAddr_;

    ConnectionCallback connectionCallback_;
    MessageCallback messageCallback_;
    WriteCompleteCallback writeCompleteCallback_;
    HighWaterMarkCallback highWaterMarkCallback_;

    CloseCallback closeCallback_;
    size_t highWaterMark_;

    Buffer inputBuffer_;
    Buffer outputBuffer_;
};