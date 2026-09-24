#include "CurrentThread.h"



// 必须和 CurrentThread.h 里声明的命名空间一致！
// 如果写成匿名命名空间 namespace {}，
// 符号就会变成 匿名空间 的 t_cacheTid/cacheTid，
// 而 CurrentThread::t_cacheTid / CurrentThread::cacheTid() 依旧没有定义，
// 链接时报 undefined reference
namespace CurrentThread
{
    __thread int t_cacheTid = 0;

    void cacheTid()
    {
        if(t_cacheTid == 0)
        {
            // 通过Linux系统获取当前线程的tid值
            t_cacheTid = static_cast<pid_t>(::syscall(SYS_gettid));
        }
    }
}