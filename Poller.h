#pragma once

#include "noncopyable.h"
#include "Timestamp.h"

#include <vector>
#include <unordered_map>

class Channel;
class EventLoop;

// muduo库中多路事件分发器的核心IO复用模块
class Poller
{
public:
    using ChannelList = std::vector<Channel*>;

    Poller(EventLoop *loop);
    virtual ~Poller() = default;

    // 给所有IO复用保留统一的接口
    virtual Timestamp poll(int timeoutMs, ChannelList *activateChannels ) = 0;
    virtual void updateChannel(Channel *channel) = 0;
    virtual void removeChannel(Channel *channel) = 0;

    // 判断参数channel是否在当前的poller当中
    bool hasChannel(Channel *channel) const;

    // EventLoop 可以通过这个接口获取默认的IO复用的具体实现
    static Poller *newDefaultPoller(EventLoop *loop);



protected:
    // map的 key: sockfd  value: sockfd所属的channel通道类型
    using ChannelMap = std::unordered_map<int, Channel*>;
    ChannelMap channels_;
private:
    EventLoop *ownerLoop_;  // 定义Poller所属的事件循环EventLoop
};