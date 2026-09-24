#pragma once

#include "noncopyable.h"
#include "Timestamp.h"

#include <functional>
#include <memory>

class EventLoop;


/*
    理清楚 EventLoop、Channle、Poller 之间的关系，Reactor 模型上对应 Demultiplex 
    一个 线程上有一个 EventLoop，一个EventLoop有一个 Poller，管理若干个Channel指针，
    Channel 可以理解为一个通道，封装了 sockfd 和其感兴趣的event，如EPOLLIN，EPOLLOUT事件
    还绑定了poller返回的具体事件
*/
class Channel : noncopyable
{
public:
    using EventCallback = std::function<void()>;
    using ReadEventCallback = std::function<void(Timestamp)>;

    Channel(EventLoop *loop, int fd);
    ~Channel();

    // fd得到poller通知以后，处理事件。调用相应的回调方法
    void handleEvent(Timestamp receiveTime);

    // 设置回调函数对象，注册回调，真正执行回调的操作是在 handleEvent 这个方法里面的例如 readCallback_()
    void setReadCallback(ReadEventCallback cb){ readCallback_ = std::move(cb); }
    void setWriteCallback(EventCallback cb) { writeCallback_ = std::move(cb); }
    void setCloseCallback(EventCallback cb) { closeCallback_ = std::move(cb); }
    void setErrorClaaback(EventCallback cb) { errorCallback_ = std::move(cb); }

    // 主要防止channle被手动remove掉，channle还在执行回调操作(就是上面四个回调操作)
    void tie(const std::shared_ptr<void>&);

    int fd() const { return fd_; }
    int events() const {return events_; }
    // poller 监听 fd 上面发生的事情，然后设置channel相应发生的事件
    void set_revents(int revt) { revents_ = revt; }
    

    // 设置fd相应的事件状态
    void enableReading() { events_ |= KReadEVent; update(); }
    void disableReading() { events_ &= ~KReadEVent; update(); }
    void enableWriting() { events_ |= kWriteEvent; update(); }
    void disableWriting() { events_ &= ~kWriteEvent; update(); }
    void disableAll() { events_ = kNoneEvent; update(); }
    
    // 返回 fd 当前的事件状态
    bool isNoneEvent() const { return events_ == kNoneEvent; }
    bool isWriting() const { return  events_ & kWriteEvent; }
    bool isReading() const { return events_ & KReadEVent; }

    int index() { return index_; }
    void set_index(int idx) { index_ = idx; }

    EventLoop *ownerLoop() { return loop_; }
    void remove();

private:
    void update();
    void handleEventWithGuard(Timestamp receiveTime);

    // 代表 fd 的状态，可理解成 对感兴趣事件状态信息的描述
    static const int kNoneEvent;    // 对任何事都不感兴趣
    static const int KReadEVent;    // 对读事件感兴趣
    static const int kWriteEvent;   // 对写事件感兴趣

    EventLoop *loop_;       // 事件循环
    const int fd_;          // fd,Poller监听的对象
    int events_;            // 注册 fd 感兴趣的事件
    int revents_;           // poller 返回的具体发生的事件
    int index_;             // Poller 使用的，表示 channel在poller中的状态 为添加/已添加/删除

    std::weak_ptr<void> tie_;   // 弱智能指针。不增加引用计数，但可以观察对象是否存活。
    bool tied_;

    // 因为 channle 通道里面可以获知fd最终发生的具体事件 revents，所有他负责调用具体发生的事件的回调操作
    ReadEventCallback readCallback_;
    EventCallback writeCallback_;
    EventCallback closeCallback_;
    EventCallback errorCallback_;

};

