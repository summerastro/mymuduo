#include "EventLoop.h"
#include "Logger.h"
#include "Poller.h"
#include "Channel.h"


#include <sys/eventfd.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <memory>


// 防止一个线程创建多个EventLoop    thread_loacl
__thread EventLoop *t_loopInThisThread = nullptr;

// 定义默认的Poller IO复用接口超时时间
const int kPollTimeMs = 10000;

// 创建wakeupfd,用来notify唤醒subReactor处理新来的channel
int createEventfd()
{
    int evtfd = ::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    if(evtfd < 0)
    {
        LOG_FATAL("eventfd error:%d\n", errno);
    }
    return evtfd;
}

EventLoop::EventLoop()
    : looping_(false)
    , quit_(false)
    , callingPendingFunctors_(false)
    , threadId_(CurrentThread::tid())
    , poller_(Poller::newDefaultPoller(this))   // 当前对象本身就是loop，因此用this就可以
    , wakeupFd_(createEventfd())
    , wakeupChannel_(new Channel(this, wakeupFd_))
{
    LOG_DEBUG("EventLoop created %p in thread %d \n", this, threadId_);
    if(t_loopInThisThread)
    {
        LOG_FATAL("Another EventLoop %p exist in this thread %d \n", t_loopInThisThread, threadId_);
    }
    else
    {
        t_loopInThisThread = this;
    }

    // 设置wakeupFd的事件类型以及发生事件后的回调操作
    wakeupChannel_->setReadCallback(std::bind(&EventLoop::handleRead, this));
    // 每个eventLoop都将监听wakeupChannel的EPOLLIN读事件了
    wakeupChannel_->enableReading();

}

EventLoop::~EventLoop()
{
    wakeupChannel_->disableAll();
    wakeupChannel_->remove();
    ::close(wakeupFd_);
    t_loopInThisThread = nullptr;
}

// 开启事件循环
void EventLoop::loop()
{
    looping_ = true;
    quit_ = false;

    LOG_INFO("EventLoop %p start looping \n", this);

    while (!quit_)
    {
        activeChannels_.clear();
        // 监听两类fd  一类是client的fd，一种在wakeupfd
        pollReturnTime_ = poller_->poll(kPollTimeMs, &activeChannels_); 
        for(Channel *channel : activeChannels_)
        {
            // Poller监听哪些channel发生了事件，然后上报给EventLoop，通知channel处理相应的事件
            // 这里是subLoop苏醒后，把wakeupFd的回调执行，也就是清空内核计数器，消除EPOLLIN事件，执行完后，才去执行runInLoop的业务回调
            channel->handleEvent(pollReturnTime_);
        }
        // 执行当前EventLoop时间循环需要处理的回调操作
        /**
         * IO线程 mainLoop 主要工作：accept， 接收用户的连接，返回一个 fd,由channel打包，分发给subloop
         * mainLoop 事先注册一个回调 cb（需要subloop执行）    wakeup subloop后执行下面的方法，执行之前mainLoop注册的cb操作
         */
        doPendingFunctors();
    }

    LOG_INFO("EventLoop %p stop looping. \n", this);
    looping_ = false;
    

}
// 退出事件循环      
/**
 *  1.loop在自己的线程中调用quit
 *  2.在非loop的线程中，调用loop的quit。
 * 需要调用其他loop的quit，就需要唤醒这个loop，否则无法调用
 */
void EventLoop::quit()
{
    quit_ = true;
    // 如果是在其他线程中调用quit， 在一个subloop中，调用了mainLoop的quit
    if(!isInLoopThread())       
    {
        wakeup();
    }
}

// 在当前loop中执行cb
void EventLoop::runInLoop(Functor cb)
{
    if(isInLoopThread())  // 在当前loop的线程中执行cb
    {
        cb();
    }
    else    // 在非当前loop线程中执行cb，就需要唤醒loop所在的线程，执行cb
    {
        queueInLoop(cb);
    }
}

// 把cb放入队列中，唤醒其他loop所在的线程，执行cb
void EventLoop::queueInLoop(Functor cb)
{
    {
        std::unique_lock<std::mutex> lock(mutex_);
        pendingFunctors_.emplace_back(cb);
    }

    // 唤醒响应的需要执行上面回调操作的loop的线程了
    // callingPendingFunctors_含义：当前loop正在执行回调，但是loop又有了新的回调，没有这个的话，loop会阻塞在wait
    // 有了这个，loop会在执行完上一轮的回调后，直接去执行新塞进来的回调
    if(!isInLoopThread() || callingPendingFunctors_ )
    {
        wakeup();   //唤醒loop所在线程
    }
}




void EventLoop::handleRead()
{
    uint64_t one = 1;
    ssize_t n = read(wakeupFd_, &one, sizeof one);
    if(n != sizeof one)
    {
        LOG_ERROR("EventLoop::handlerRead() reads %lu bytes instead of 8", n);
    }
}


// 唤醒loop所在的线程   向 wankeupfd写一个数据 wakeupChannel 就发生读事件，当前loop线程就会被唤醒
void EventLoop::wakeup()
{
    uint64_t one = 1;
    ssize_t n = write(wakeupFd_, &one, sizeof one);
    if( n!= sizeof one)
    {
        LOG_ERROR("EventLoop::wakeup() writes %lu bytes instead of 8 \n", n);
    }
}

// EventLoop的方法 => Poller的方法
/**
 * 这个方法是 channel 向 poller问的，但是channel和poller无法沟通
 * 因此 channel和poller是通过 EventLoop 进行沟通
 */
void EventLoop::updateChannel(Channel *channel)
{
    poller_->updateChannel(channel);
}
void EventLoop::removeChannel(Channel *channel)
{
    poller_->removeChannel(channel);
}
bool EventLoop::hasChannel(Channel *channel)
{
    return poller_->hasChannel(channel);
}

void EventLoop::doPendingFunctors()  // 执行回调
{
    std::vector<Functor> functors;
    callingPendingFunctors_ = true;

    {
        std::unique_lock<std::mutex> lock(mutex_);
        functors.swap(pendingFunctors_);
    }

    for(const Functor &functor : functors)
    {
        functor();  // 执行当前loop需要执行的回调操作
    }

    callingPendingFunctors_ = false;
}