#pragma once

#include <iostream>
#include <string>

// 事件类
class Timestamp
{
public:
    Timestamp();
    // 对于单参数构造函数，在代码上明确表示需要对象而不是隐式转换对象，就需要加上 explicit
    explicit Timestamp(int64_t microSecondsSinceEpoch);
    static Timestamp now();
    std::string toString() const;

private:
    int64_t microSecondsSinceEpoch_;

};