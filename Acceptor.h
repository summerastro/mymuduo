#pragma once

#include "noncopyable.h"
#include "Socket.h"
#include "Channel.h"

#include <functional>

class EventLoop;
class InetAddress;

class Acceptor : noncopyable {
public:
    using NewConnectionCallback = std::function<void(int sockfd, const InetAddress&)>;
    Acceptor(EventLoop *loop, const InetAddress &listenAddr, bool reuseport);
    ~Acceptor();

    void setNewConnectionCallback(const NewConnectionCallback &cb){
        newConnectionCallback_ = cb;
    }

    bool listenning() const { return listenning_; }
    void listen();

private:
    void handleRead();

    EventLoop *loop_;   // Acceptor 用的就是用户定义的baseloop，也被称作mainLoop
    // Acceptor 是监听socket和创建channel，拥有这两个，生命周期和自己一致
    // 而 loop 是Acceptor借用，是外部拥有的
    Socket acceptSocket_;
    Channel acceptChannel_;
    NewConnectionCallback newConnectionCallback_;
    bool listenning_;

};