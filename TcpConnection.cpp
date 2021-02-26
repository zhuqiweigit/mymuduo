#include "TcpConnection.h"
#include "Socket.h"
#include "Channel.h"
#include "InetAddress.h"
#include "Logger.h"
#include "EventLoop.h"

/**
 * ScpConnection: 
 * 维护了一个Channel、一个socket、一对InetAddress（local和peer）、两个buffer
 * 维护了状态：state、isReading
 * 
 * channel: new出来的。把自己的handle注册到channel，由channel负责通知。自己的handle内部有用户设置的回调函数；
 *          disableAll会关闭所有的epoll监听，由Tcp conn的destroy相关函数调用
 * socket: new出来的。主要设置了一个keepalive。
 *          另外在主动关闭的时候，外界调用shutdown主动关闭连接，shutdown会调用socket的shutdownOnWrite
 * inputBuffer: 以用户为中心，input里面是用户待读的数据。因此在handleRead里负责读数据，然后调用用户的onMessage方法给用户传入数据。
 *              读数据是由网络库决定的。有数据就通知用户。
 * outputBuffer: 以用户为中心来看，output内是用户写入的待发送的数据。因为发送数据是由用户决定的，因此发送流程为：用户调用网络库对外提供的send函数，
 *             把数据（char*）传入send，send函数内利用sockfd进行一次写操作：
 *             1）如果一次性写完，则调用用户注册的writeComplete函数通知用户
 *             2）如果第一次写不完，写不完的数据再存入buffer，然后开启channel的可写通知。一旦可写，则由handleWrite负责写入。如果本次写完所有数据，则
 *                关闭channel的可写通知。否则一直等待下次可写
 * 
 * 用户注册的：messageCallback、conncetionCallback、setWriteCompleteCallback
 * 
 * 关闭相关：shutDown(sockfd)，由socket负责。如果用户主动调用关闭，则会关闭。【这种关闭是单方面关闭。仅仅关闭服务端的写端】，此时还能收到客户端的消息。当客户端收到服务端
 *          发送的写端FIN关闭时，如果也发送了关闭，而一旦sockFd关闭、或者peer端关闭后，会触发conn在channel注册的handleClose。从而删除conn，而conn内持有一个socket，析构时会
 *          执行close(connfd)，相当于彻底关闭。
 *          TcpConnection::handleClose->disable channel、用户注册的messageCallbcak -> maybe shutDown sockFd , 
 *          TcpServer注册的closeCallback->TcpServer::removeConnection删除conn ->inLoop: connectionDestroyed负责disable Channel + 删除channel
 * 
 * connectEstablished和 connectionDestroyed 这一对函数由TcpServer层面负责掌控；
 *       建立：在TcpServer中新建conn后，一切设置妥当，就会调用connectEstablished，修改conn的状态为【connected】，开启channel，绑定channel，执行用户的onMessage回调
 *       销毁：在TcpServer中删除完conn后，就会调用queueInLoop -> connectionDestroyed修改conn状态为【disconnected】，执行用户的onMessage回调， 关闭channel，删除channel
 * 
 * disconnecting状态：shutdown，关闭写端之后的状态，此时不可写，可读。
 * Connecting状态：TcpConnection的构造函数内初始化为正在连接
*/

TcpConnection::TcpConnection(EventLoop* loop, const std::string& name, int sockfd, const InetAddress& localAddr, 
                    const InetAddress& peerAddr)
            :loop_(loop), name_(name), socket_(new Socket(sockfd)), 
            channel_(new Channel(loop, sockfd)),
            state_(kConnecting),
            localAddr_(localAddr),
            peerAddr_(peerAddr),
            highWaterMark_(64 * 1024 * 1024){
      channel_->setReadCallback(std::bind(&TcpConnection::handleRead, this, std::placeholders::_1));
      channel_->setWriteCallback(std::bind(&TcpConnection::handleWrite, this));
      channel_->setCloseCallback(std::bind(&TcpConnection::handleClose, this));
      channel_->setErrorCallback(std::bind(&TcpConnection::handleError, this));
      
      LOG_INFO("TcpConnection::ctor[%s] at fd=%d\n", name_.c_str(), sockfd);

      socket_->setKeepAlive(true);
}

TcpConnection::~TcpConnection(){
   LOG_INFO("TcpConnection::dtor[%s] at fd=%d state=%d \n", 
        name_.c_str(), channel_->fd(), (int)state_);
}

void TcpConnection::send(const std::string& buf){
   if(state_ == kConnected){
      if(loop_->isInLoopThread()){
         sendInLoop(buf.c_str(), buf.size());
      }else{
         loop_->runInLoop(std::bind(&TcpConnection::sendInLoop, this, buf.c_str(), buf.size()));
      }
   }
}

/**
 * 发送数据  应用写的快， 而内核发送数据慢.
 * 首次发送使用char*，如果发送完了则没问题。
 * 如果受到TCP滑动窗口限制等等，无法一次性写完所有数据，则剩下的数据保存到output_buffer，然后开启channel的写通知，
 * 一旦可写，就会调用事先绑定的write handle，利用buffer继续写
 */ 
void TcpConnection::sendInLoop(const void* message, size_t len){
   ssize_t nwrote = 0;
   size_t remaining = len;
   bool faultError = false;

   if(state_ == kDisconnected){
      LOG_ERROR("disconnected");
   }

   if(!channel_->isWriting() && outputBuffer_.writeableBytes() == 0){
      nwrote = ::write(socket_->fd(), message, len);
      if(nwrote >= 0){
         remaining = len - nwrote;
         if(remaining == 0 && writeCompleteCallback_){
            loop_->queueInLoop(std::bind(writeCompleteCallback_, shared_from_this()));
         }
      }else{
         nwrote = 0;
         if(errno != EWOULDBLOCK){
            LOG_ERROR("TcpConnection: send in loop");
            faultError = true;
         }
      }
   }

   if(!faultError && remaining > 0){
      size_t oldLen = outputBuffer_.readableBytes();
      if(oldLen + remaining > highWaterMark_ && oldLen < highWaterMark_
         && highWaterMarkCallback_){
            loop_->queueInLoop(std::bind(highWaterMarkCallback_, shared_from_this(), oldLen + remaining));
      }
      outputBuffer_.append((char*)message + nwrote, remaining);
      if(!channel_->isWriting()){
         channel_->enableWriting();
      }
   }
}

void TcpConnection::shutdown(){
   if(state_ == kConnected){
      setState(kDisconnecting);
      loop_->queueInLoop(std::bind(&TcpConnection::shutdownInLoop, this));
   }
}

void TcpConnection::shutdownInLoop(){
   if(!channel_->isWriting()){
      socket_->shutdownWrite();
   }
}

void TcpConnection::connectionEstablished(){
   setState(kConnected);
   channel_->tie(shared_from_this());
   channel_->enableReading();

   connectionCallback_(shared_from_this());
}

void TcpConnection::connectionDestroyed(){
   if(state_ == kConnected){
      setState(kDisconnected);
      channel_->disableAll();
      connectionCallback_(shared_from_this());
   }
   channel_->remove();
}

void TcpConnection::handleRead(TimeStamp receiveTime){
   int saveErrno = 0;
   ssize_t n = inputBuffer_.readFd(channel_->fd(), &saveErrno);
   if(n > 0){
      messageCallback_(shared_from_this(), &inputBuffer_, receiveTime);
   }else if(n == 0){
      handleClose();
   }else{
      errno = saveErrno;
      LOG_ERROR("handle read");
      handleError();
   }
}

void TcpConnection::handleWrite(){
   if(channel_->isWriting()){
      int saveErrno = 0;
      ssize_t n = outputBuffer_.writeFd(channel_->fd(), &saveErrno);
      if(n > 0){
         outputBuffer_.retrieve(n);
         if(outputBuffer_.readableBytes() == 0){
            channel_->disableWriting();
            if(writeCompleteCallback_){
               loop_->queueInLoop(std::bind(writeCompleteCallback_, shared_from_this()));
            }
            if(state_ == kDisconnecting){
               shutdownInLoop();
            }
         }
      }else {
         LOG_ERROR("TcpConnection::handleWrite");
      }
   }else{
       LOG_ERROR("TcpConnection fd=%d is down, no more writing \n", channel_->fd());
   }
}


void TcpConnection::handleClose(){
    LOG_INFO("TcpConnection::handleClose fd=%d state=%d \n", channel_->fd(), (int)state_);
    setState(kDisconnected);
    channel_->disableAll();

    TcpConnectionPtr connPtr(shared_from_this());
    connectionCallback_(connPtr); // 执行连接关闭的回调
    closeCallback_(connPtr); // 关闭连接的回调  执行的是TcpServer::removeConnection回调方法
}


void TcpConnection::handleError(){
    int optval;
    socklen_t optlen = sizeof optval;
    int err = 0;
    if (::getsockopt(channel_->fd(), SOL_SOCKET, SO_ERROR, &optval, &optlen) < 0)
    {
        err = errno;
    }
    else
    {
        err = optval;
    }
    LOG_ERROR("TcpConnection::handleError name:%s - SO_ERROR:%d \n", name_.c_str(), err);
}