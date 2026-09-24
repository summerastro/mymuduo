#include "Channel.h"
#include "EventLoop.h"
#include "Logger.h"

#include <sys/epoll.h>

const int Channel::kNoneEvent = 0;
// EPOLLIN:fd 可读事件,EPOLLPRI: 高优先级可读事件.普通 TCP 数据包触发 EPOLLIN；紧急数据触发 EPOLLPRI。
// 业务代码大部分场景不需要区分普通数据和 OOB 数据，统一交给 read 回调处理，简化上层使用。
const int Channel::KReadEVent = EPOLLIN | EPOLLPRI;
const int Channel::kWriteEvent = EPOLLOUT;

// EventLoop 包含了 ChannelList Poller
Channel::Channel(EventLoop *loop, int fd)
    : loop_(loop), fd_(fd), events_(0), revents_(0), index_(-1), tied_(false)
{

}

Channel::~Channel()
{

}

// 一个TcpConnection新连接创建的时候，
/**
 * Channel 本身不持有连接对象（TcpConnection）。Channel 只封装 fd、事件、回调。
 * TcpConnection 持有 Channel；但 EventLoop 只操作 Channel。
 * 当 fd 触发事件，EventLoop 调用 Channel::handleEvent()，执行注册的回调。
 */
void Channel::tie(const std::shared_ptr<void> &obj)
{
    tie_ = obj;
    tied_ = true;
}

/*
    当改变channle所表示fd的events事件后，update负责在poller里面更改fd相应的事件epoll_ctl

*/
void Channel::update()
{
    //通过channel所属的EvenLoop，调用poller相应的方法，注册fd的events事件
    loop_->updateChannel(this);
}

// 在channel所属的EventLoop中，把当前的channel删除掉
void Channel::remove()
{
    loop_->removeChannel(this);
}

void Channel::handleEvent(Timestamp receiveTime)
{
    if(tied_){
        // 尝试把 弱智能指针tie_ 提升为 shared_ptr 
        std::shared_ptr<void> guard = tie_.lock();
        // 此时guard是shared_ptr，提升成功则说明对象还活着，这里是指TcpConnection还存活
        if(guard){
            handleEventWithGuard(receiveTime);
        }
        // 失败不执行回调
    }else{
        handleEventWithGuard(receiveTime);
    }
}

// 根据具体的事件执行相应的回调操作，由channel负责调用具体的回调操作，也就是触发回调
void Channel::handleEventWithGuard(Timestamp receiveTime)
{
    LOG_INFO("channel handleEvent revents:%d\n", revents_);

    // EPOLLHUP 对端关闭连接且没有读事件的时候，执行close回调操作
    if((revents_ & EPOLLHUP) && !(events_ & EPOLLIN)){
        if(closeCallback_){
            closeCallback_();
        }
    }

    // EPOLLERR 错误
    if(revents_ & EPOLLERR){
        if(errorCallback_){
            errorCallback_();
        }
    }

    // 读事件
    if(revents_ & (EPOLLIN | EPOLLPRI)){
        if(readCallback_){
            readCallback_(receiveTime);
        }
    }

    // EPOLLOUT 写事件
    if(revents_ & EPOLLOUT){
        if(writeCallback_){
            writeCallback_();
        }
    }
}