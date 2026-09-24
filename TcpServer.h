#pragma once

/**
 * 用户使用muduo库编写服务器程序
 * 避免用户编写程序的时候需要包含很多头文件，这里直接统一先包含上,而不是前向声明
 */

#include "EventLoop.h"
#include "Acceptor.h"
#include "InetAddress.h"
#include "noncopyable.h"
#include "EventLoopThreadPool.h"
#include "Callbacks.h"
#include "TcpConnection.h"
#include "Buffer.h"

#include <functional>
#include <string>
#include <memory>
#include <atomic>
#include <unordered_map>




// 对外的服务器编程使用的类
class TcpServer : noncopyable {
public:
    using ThreadInitCallback = std::function<void(EventLoop*)>;

    enum Option{
        KNoReusePort,
        KReusePort,
    };

    TcpServer(EventLoop *loop, const InetAddress &listenAddr, const std::string nameArg ,Option option = KNoReusePort);
    ~TcpServer();

    void setThreadInitCallback(const ThreadInitCallback &cb) { threadInitCallback_ = cb; }
    void setConnectionCallback(const ConnectionCallback &cb) { connectionCallback_ = cb; }
    void setMessageCallback(const MessageCallback &cb) { messageCallback_ = cb; }
    void setWriteCompleteCallback(const WriteCompleteCallback &cb) { writeCompleteCallback_ = cb; }


    // 设置底层subLoop的个数
    void setThreadNum(int numThreadNum);

    // 开启服务器监听   实际 mainLoop => acceptor.listen 
    void start();    
private:
    void newConnection(int sockfd, const InetAddress &peerAddr);
    void removeConnection(const TcpConnectionPtr &conn);
    void removeConnectionInLoop(const TcpConnectionPtr &conn);

    using ConnectionMap = std::unordered_map<std::string, TcpConnectionPtr>;

    EventLoop *loop_;   // baseloop 用户创建的loop

    const std::string ipPort_;
    const std::string name_;

    std::unique_ptr<Acceptor> acceptor_;    // mainLoop里的，任务就是监听新连接事件

    // 事件循环线程池
    std::shared_ptr<EventLoopThreadPool> threadPool_;

    ConnectionCallback connectionCallback_; //有新连接时的回调
    MessageCallback messageCallback_;   //有消息时的回调
    WriteCompleteCallback writeCompleteCallback_;   // 消息发送完的回调

    ThreadInitCallback threadInitCallback_; // loop线程初始化的回调
    
    std::atomic_int started_;

    int nextConnId_;
    ConnectionMap connections_; // 保存所有的连接


};