#pragma once

/*
    noncopyable 被继承后，派生类对象可以这次构造和析构，但是不能拷贝构造和复制操作
*/

class noncopyable
{
public:
    noncopyable(const noncopyable&) = delete;
    noncopyable& operator=(const noncopyable&) = delete;
protected:
    noncopyable() = default;
    ~noncopyable() = default;
};