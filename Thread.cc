#include "Thread.h"
#include "CurrentThread.h"

#include <semaphore.h>

// 静态的成员变量，在类外要单独的进行初始化
// 注意：必须是 Thread::numCreated_，否则定义的只是一个同名的全局变量，
// 而类内声明的静态成员 Thread::numCreated_ 依然没有定义，链接时就会报
// undefined reference to `Thread::numCreated_'
std::atomic_int Thread::numCreated_ (0);

Thread::Thread(ThreadFunc func, const std::string &name)
    : started_(false)
    , joined_(false)
    , tid_(0)
    , func_(std::move(func))
    , name_(name)
{
    setDefaultName();
}


Thread::~Thread()
{
    /**
     * 这里表示，线程里还有个线程正在运行，join表示当前线程必须等待子线程执行完毕后才能关闭，这时候joined 为 true
     * 因为子线程已经执行完完毕了
     * detach 表示，当前子线程还没有执行完毕，没有返回，线程放弃对子线程的管理
     */
    if(started_ && !joined_)
    {
        thread_->detach();  // thread类提供的设置分离线程的方法
    }
}

void Thread::start()    // 一个 Thread对象，记录的就是一个线程的详细信息
{
    started_ = true;
    sem_t sem;
    sem_init(&sem, false, 0);

    // 开启线程
    thread_ = std::shared_ptr<std::thread>(
        // 这里new 一个裸指针，然后用这个指针构造智能指针
        new std::thread([&]() {
            // 获取线程的tid值
            tid_ = CurrentThread::tid();
            sem_post(&sem);
            func_();    // 开启一个线程，专门执行该线程函数
        })
    );

    // 这里必须等待获取上面的新线程的线程值
    sem_wait(&sem);

}

void Thread::join()
{
    joined_ = true;
    thread_->join();
}

void Thread::setDefaultName()
{
    int num = ++numCreated_;
    if(name_.empty())
    {
        char buf[32] = {0};
        snprintf(buf, sizeof buf, "Thread%d", num);
        name_ = buf;
    }
}