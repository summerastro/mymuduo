#include "Acceptor.h"
#include "Logger.h"
#include "InetAddress.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <errno.h>
#include <unistd.h>

static int createNonblocking(){
    int sockfd = ::socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, IPPROTO_TCP);
    if( sockfd < 0){
        // 打印在哪个文件，哪个函数，哪行出错
        LOG_FATAL("%s:%s:%d listen socket create err:%d \n", __FILE__, __FUNCTION__, __LINE__, errno);
    }
    return sockfd;
}

Acceptor::Acceptor(EventLoop *loop, const InetAddress &listenAddr, bool reuseport)
    : loop_(loop)
    , acceptSocket_(createNonblocking())
    , acceptChannel_(loop, acceptSocket_.fd())
    , listenning_(false)
{
    acceptSocket_.setReuseAddr(true);
    acceptSocket_.setReusePort(true);
    acceptSocket_.bindAddress(listenAddr);  // bind
    // TCPServer::start() 启动 Acceptor.listen()  
    // 这时候又新用户拦截，要执行一个回调(connfd >> channel >> subLoop)
    // baseloop 
    acceptChannel_.setReadCallback(std::bind(&Acceptor::handleRead, this));


}
Acceptor::~Acceptor(){
    acceptChannel_.disableAll();
    acceptChannel_.remove();
}


// TcpServer 的 start 会调用这个
void Acceptor::listen(){
    listenning_ = true;
    acceptSocket_.listen();
    // acceptChannel 注册到 poller 里，才能帮忙监听是否有事件监听
    acceptChannel_.enableReading(); 

}

// listenfd有事件发生了，就是有新用户连接了
void Acceptor::handleRead(){
    LOG_INFO("Acceptor::listen() called, sockfd=%d", acceptSocket_.fd());
    InetAddress peerAddr;
    int connfd = acceptSocket_.accept(&peerAddr);
    if( connfd >= 0 ){
        if(newConnectionCallback_){
            newConnectionCallback_(connfd, peerAddr);   // 轮询找到subLoop，唤醒，分发当前新客户端的Channel

        }else{
            ::close(connfd);
        }
    }else{
        LOG_ERROR("%s:%s:%d accept err:%d \n", __FILE__, __FUNCTION__, __LINE__, errno);
        if(errno == EMFILE){
            LOG_ERROR("%s:%s:%d sockfd reached limit \n", __FILE__, __FUNCTION__, __LINE__);
        }
    }

}