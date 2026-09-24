#pragma once

#include "noncopyable.h"

#include <functional>
#include <thread>
#include <memory>
#include <string>
#include <atomic>

class Thread : noncopyable
{
public:
    using ThreadFunc = std::function<void()>;

    // 这里name默认为null
    explicit Thread(ThreadFunc, const std::string &name = std::string());
    ~Thread();

    void start();
    void join();

    bool started() const { return started_; }
    pid_t tid() const { return tid_; }
    // 这里是引用地址，不发生拷贝
    const std::string &name() const { return name_; }

    static int numCreated() { return numCreated_; }


private:
    void setDefaultName();

    bool started_;  // 
    bool joined_;
    /** 这里不能直接声明 std::thread thread_ 这里会直接启动线程了
     * 我们要控制线程启动的时机，
     * 需要定义一个智能指针来封装，自己掌控线程对象产生的时机
     */
    std::shared_ptr<std::thread> thread_;
    pid_t tid_;
    ThreadFunc func_;
    std::string name_;

    static std::atomic_int numCreated_;






};