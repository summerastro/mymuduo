#include "EventLoopThread.h"
#include "EventLoop.h"
#include "Logger.h"



EventLoopThread::EventLoopThread(const ThreadInitCallback &cb, const std::string &name)
    : loop_(nullptr)
    , existing_(false)
    , thread_(std::bind(&EventLoopThread::threadFunc, this), name)
    , mutex_()
    , cond_()
    , callback_(cb)
{

}
EventLoopThread::~EventLoopThread()
{
    existing_ = true;
    if(loop_ != nullptr)
    {
        loop_->quit();
        thread_.join();
    }
}

EventLoop *EventLoopThread::startLoop()
{
    thread_.start();    //启动底层的新线程

    EventLoop *loop = nullptr;
    {
        std::unique_lock<std::mutex> lock(mutex_);
        // 改1
        while(loop_ == nullptr)
        {
            cond_.wait(lock);
        }
        loop = loop_;
    }

    return loop;

}


// 这个方法是在单独的新线程里面运行的
void EventLoopThread::threadFunc()
{
    EventLoop loop; // 创建一个独立的EventLoop, 和上面的线程是一一对应的， one loop per thread

    if(callback_)
    {
        callback_(&loop);
    }

    {
        std::unique_lock<std::mutex> lock(mutex_);
        loop_ = &loop;
        cond_.notify_one();
    }

    loop.loop();    // Event loop  ===> Poller.poll
    // 上面是程序一直在运行，如果运行了下面的，代表服务器程序要关闭了
    std::unique_lock<std::mutex> lock(mutex_);
    LOG_INFO("loop.loop()执行完毕，正在退出\n");
    
    loop_ = nullptr;



}