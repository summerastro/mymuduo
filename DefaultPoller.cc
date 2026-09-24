#include "Poller.h"
#include "EPollPoller.h"

#include <stdlib.h>

// Poller默认使用 epoll，因此需要用这个单独的DefaultPoller，不能在Poller里引用EPollPoller
// 因为 EPollPoller 是继承自 Poller 这个公共类，那么Poller默认构造EPoll的时候，不能反过来引用子类
// 因此需要单独写个文件构造默认Poller
Poller* Poller::newDefaultPoller(EventLoop *loop)
{
    if(::getenv("MUDUO_USE_POLL"))
    {
        return nullptr;     //生成poll的实例
    }
    else{
        return new EPollPoller(loop);      // 生成epoll的实例
    }
}