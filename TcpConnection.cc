#include "TcpConnection.h"
#include "Logger.h"
#include "Socket.h"
#include "Channel.h"
#include "EventLoop.h"

#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <strings.h>
#include <netinet/tcp.h>

static EventLoop* checkLoopNotNull(EventLoop* loop)
{
    if(loop == nullptr)
    {
        LOG_FATAL("%s:%s:%d TcpConnection Loop is null! \n", __FILE__, __FUNCTION__, __LINE__);
    }
    return loop;
}

TcpConnection::TcpConnection(EventLoop *loop,
                std::string &nameArg,
                int sockfd,
                const InetAddress &localAddr,
                const InetAddress &peerAddr)
    : loop_(checkLoopNotNull(loop))
    , name_(nameArg)
    , state_(kConnecting)
    , reading_(true)
    , socket_(new Socket(sockfd))
    , channel_(new Channel(loop, sockfd))
    , localAddr_(localAddr)
    , peerAddr_(peerAddr)
    , highWaterMark_(64*1024*1024)  // 64M
{
    // 给channel设置相应的回调函数，poller给channel通知感兴趣事件发生了， channel调用相应的操作
    channel_->setReadCallback(std::bind(&TcpConnection::handleRead, this, std::placeholders:: _1));
    channel_->setWriteCallback(std::bind(&TcpConnection::handleWrite, this));
    channel_->setCloseCallback(std::bind(&TcpConnection::handleClose, this));
    channel_->setErrorClaaback(std::bind(&TcpConnection::handleError, this));

    LOG_INFO("TcpConnection::ctor[%s] at fd=%d\n", name_.c_str(), sockfd);

    socket_->setKeepAlive(true);
}

TcpConnection::~TcpConnection()
{
    LOG_INFO("TcpConnection::dtor[%s] at fd=%d state=%d \n", 
            name_.c_str(), channel_->fd() , (int)state_);
}

void TcpConnection::send(const std::string &buf)
{
    if(state_ = kConnected)
    {
        if(loop_->isInLoopThread())
        {
            sendInLoop(buf.c_str(), buf.size());
        } else {
            loop_->runInLoop(std::bind(
                &TcpConnection::sendInLoop,
                this,
                buf.c_str(),
                buf.size()
            ));
        }
    }
}


//发送数据  应用写的快，而内核发送数据慢，需要把待发送数据写入缓冲区，而且设置了水位回调
void TcpConnection::sendInLoop(const void* data, size_t len)
{
    ssize_t nwrote = 0;
    size_t remaining = len; // 还未发送完的数据
    bool faultError = false;

    // 之前调用过该connection的shutdown,不能再进行发送了
    if(state_ == kDisconnected)
    {
        LOG_ERROR("disconnected, giv up writing!");
        return;
    }

    if(!channel_->isWriting() && outputBuffer_.readableBytes() == 0)
    {
        nwrote == ::write(channel_->fd(), &data, len);
        if(nwrote > 0)
        {
            remaining = len - nwrote;
            if(remaining == 0 && writeCompleteCallback_)
            {
                loop_->queueInLoop(
                    std::bind(writeCompleteCallback_, shared_from_this())
                );
            }
        } else {
            nwrote = 0;
            // 由于非阻塞没有数据，没有数据
            if( errno != EWOULDBLOCK)
            {
                LOG_ERROR("TcpConnection::sendInLoop");
                if(errno == EPIPE || errno == ECONNRESET) //链接重置
                {
                    faultError = true;
                }
            }
        }

        //说明当前write没有把数据全部发送出去，说明数据要保存到缓冲区中，然后给channel
        // 注册epollout事件， poller会发现tcp发送缓冲区有空就，会通知相应的sock-channel调用writeCallback_回调方法
        // 也就是调用TcpConnection::handlewrite方法，把发送缓冲区的数据全部发送完

        if(!faultError && remaining > 0)    
        {
            size_t oldLen = outputBuffer_.readableBytes();
            if(oldLen + remaining >= highWaterMark_
                && oldLen < highWaterMark_
                && highWaterMarkCallback_)
            {
                loop_->queueInLoop(
                    std::bind(highWaterMarkCallback_, shared_from_this(), oldLen+remaining)
                );
            }
            outputBuffer_.append((char*)data + nwrote, remaining);
            if(!channel_->isWriting())
            {
                channel_->enableWriting();  // 这里一定要注册channel的写事件，否则poller不会给channel通知epollout
            }
        }
    }
}





void TcpConnection::handleRead(Timestamp receiveTime)
{
    int saveErrno = 0;
    ssize_t n = inputBuffer_.readFd(channel_->fd(),&saveErrno);
    if(n > 0)
    {
        // 已建立连接的用户，有可读事件发生了，调用用户传入的回调操作 onMessage
        // shared_from_this是获取 TcpConnection 的智能指针
        messageCallback_(shared_from_this(), &inputBuffer_, receiveTime);
    } else if(n == 0){
        handleClose();
    } else {
        errno = saveErrno;
        LOG_ERROR("TcpConnection::handleRead");
        handleError();
    }
}

void TcpConnection::handleWrite()
{
    if(channel_->isWriting())
    {
        int saveErrno = 0;
        ssize_t n = outputBuffer_.writeFd(channel_->fd(), &saveErrno);
        if(n > 0)
        {
            outputBuffer_.retrive(n);
            if(outputBuffer_.readableBytes() == 0)
            {
                channel_->disableWriting();
                // 如果用户定义了数据发送完的回调，就执行下面
                if(writeCompleteCallback_)
                {
                    // 唤醒loop所在的线程执行回调
                    loop_->queueInLoop(
                        std::bind(writeCompleteCallback_, shared_from_this())
                    );
                }
                if(state_ == kDisconnecting)
                {
                    shutdownInLoop();
                }
            }
        } else {
            LOG_ERROR("TcpConnection::handleWrite");
        }
        
    } else {
        LOG_ERROR("TcpConnection fd = %d is down, no more writing \n", channel_->fd());
    }
    
}

// poller通知channel调用 channel::closeCallback ==> TcpConnection::handleClose
void TcpConnection::handleClose()
{
    LOG_INFO("fd=%d state=%d \n", channel_->fd(), (int)state_);
    setstate(kDisconnected);
    channel_->disableAll();

    TcpConnectionPtr connPtr(shared_from_this());
    connectionCallback_(connPtr);   //执行连接关闭的回调
    closeCallback_(connPtr);    //关闭连接的回调    执行的是 TcpServer::removeConnection 回调方法

}

void TcpConnection::handleError()
{
    int optval;
    socklen_t optlen = sizeof optval;
    int err = 0;
    // 连接出错时，检查这个socket有什么错误
    if(::getsockopt(channel_->fd(), SOL_SOCKET, SO_ERROR, &optval, &optlen) < 0)
    {
        err = errno;    // 检查失败
    } else {
        err = optval;   // 检查成功
    }
    LOG_ERROR("TcpConnection::handleError name:%s - SO_ERROR:%d \n", name_.c_str(), err);
}


// 连接建立
void TcpConnection::connecetEstablished()
{
    setstate(kConnected);
    // channel 绑定的都是TcpConnection的成员函数，防止channel正在进行，而TcpConnection被销毁
    channel_->tie(shared_from_this());
    channel_->enableReading();  // 向poller注册channel的epollin事件

    // 新连接建立，执行回调
    connectionCallback_(shared_from_this());
}

// 连接销毁
void TcpConnection::connectDestroyed()
{
    if(state_ == kConnected)
    {
        setstate(kDisconnected);    
        channel_->disableAll(); // 把channel所有感兴趣的事件，从poller中del
        connectionCallback_(shared_from_this());
    }
    channel_->remove();
}

void TcpConnection::shutdown()
{
    if(state_ == kConnected)
    {
        setstate(kDisconnecting);
        loop_->runInLoop(std::bind(&TcpConnection::shutdownInLoop, this));
    }
}


void TcpConnection::shutdownInLoop()
{
    if(!channel_->isWriting())  //说明outputBuffer数据已经全部发送完成
    {
        socket_->shutdownWrite();   //关闭写端
    }
}