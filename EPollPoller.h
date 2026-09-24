#pragma once

#include "Poller.h"
#include "Timestamp.h"

#include <vector>
#include <sys/epoll.h>

class Channel;

/*
    epoll的使用
    epoll_create
    epoll_ctl   add/del/mod
    epoll_wait
*/
class EPollPoller : public Poller{
public:
    EPollPoller(EventLoop *loop);
    ~EPollPoller()  override;

    // 重写基类poller 的抽象方法
    /**
     * poll 将所监听fd上面发生的事件 填进 ChannelList 里
     */
    Timestamp poll(int timeoutMs, ChannelList* activeChannels) override;
    void updateChannel(Channel *channel) override;
    void removeChannel(Channel *channel) override;
    
private:
    // 初始化 EventList 长度
    static const int KInintEventListSize = 16;

    // 填写活跃的连接
    void fillActiveChannel(int numEvents, ChannelList* activeChannels) const;
    // 更新 Channel通道
    void update(int operation, Channel *channel);


    // epoll_event: 在epoll中，fd会向epfd注册自己感兴趣的事件
    // 当这些被监听的 fd 真的发生感兴趣 IO 事件时，
    // epoll_wait 就把就绪的事件拷贝到你给的 struct epoll_event events[] 数组返回给用户态程序。
    using EventList = std::vector<epoll_event>;

    int epollfd_;
    EventList events_;
};