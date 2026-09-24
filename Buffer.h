#pragma once

#include "noncopyable.h"

#include <vector>
#include <cstddef>
#include <string>
#include <algorithm>

// 网络库底层的缓冲区类型定义
class Buffer : noncopyable{
public:
    static const size_t kCheapPrepend = 8;  // 记录数据包长度区域的大小
    static const size_t kInintialSize = 1024;   // 缓冲区的初始大小
    explicit Buffer(size_t initialSize = kInintialSize)
        : buffer_(kCheapPrepend + kInintialSize)
        , readerIndex_(kCheapPrepend)
        , writerIndex_(kCheapPrepend)
    {}

  

    size_t readableBytes() const {
        return writerIndex_ - readerIndex_;
    }

    size_t writableBytes() const {
        return buffer_.size() - writerIndex_;
    }

    size_t prependableBytes() const {
        return readerIndex_;
    }

    // 读数据的时候用
    const char* begin() const {
        return &*buffer_.begin();
    }

    // 返回数据缓存区中可读数据的起始地址
    const char* peek() const {
        return begin() + readerIndex_;
    }

    // onMessage 数据到缓冲区的时候，需要把数据从Buffer类型转化为string类型
    void retrive(size_t len){
        if(len < readableBytes()) {
            readerIndex_ += len;    //应用只读取了可读缓冲区的一部分，就是len长度，还剩下 readerIndex_ += len --------> writeIndex_ 这些数据
        } else {
            retriveAll();
        }
    }

    // 读完了进行复位
    void retriveAll() {
        readerIndex_ = writerIndex_ = kCheapPrepend;
    }
    
    // 把onMessage 函数上报的Buffer数据转化为string类型数据返回
    std::string retriveAllAsString() {
        return retriveAsString(readableBytes());
    }

    std::string retriveAsString(size_t len) {
        // string的构造函数，从peek()地址开始，读取len长度的字符构造出string对象
        std::string result(peek(), len);
        // 上面已经把缓冲区中可读的数据，读取出来了，这里就要对缓冲区进行复位操作
        retrive(len);
        return result;
    }

    // 写入数据要和可写缓冲区比较
    void ensureWriteableBytes(size_t len) {
        if(writableBytes() < len) {
            makeSpace(len); //扩容函数
        }
    }

    // 把[data,data+len]内存上的数据，添加到wrritable缓冲区中
    void append(const char* data, size_t len) {
        ensureWriteableBytes(len);
        std::copy(data, data+len, beginWrite());
        writerIndex_ += len;
    }


    // 从fd上读取数据
    ssize_t readFd(int fd, int* saveErrno);
    // 通过fd发送数据
    ssize_t writeFd(int fd, int* saveErrno);


private:

    // 写数据的时候用
    char* begin(){
        // *buffer_.begin() 获取vector的第一个元素，加上&后获得第一个元素的起始地址
        return &*buffer_.begin();
    }

    void makeSpace(size_t len) {
        // 读缓冲区被读后，readerIndex_ 不会在kCheapPrepend位置，只会往后移动读了的字节数
        // 例如，读了10字节，这时候 readerIndex_ = 10 + kCheapPrepend
        // 因此，prependableBytes() = readerIndex_
        // 下面意思就是：
        // 读缓冲区空出来的空间 + 可写的空间 < 我要写的长度
        if(prependableBytes() + writableBytes() < len + kCheapPrepend){
            // 把可写缓冲区变成len大小，刚好够，不多不少
            buffer_.resize(writerIndex_ + len);
        } else {    // 虽然可写缓冲区比len小，但是 加上 prependableBytes() - 8 空间够
            size_t readable = readableBytes();
            std::copy(begin() + readerIndex_, begin() + writerIndex_, begin() + kCheapPrepend);
            readerIndex_ = kCheapPrepend;
            writerIndex_ = readerIndex_ + readable;
        }
    }

    


    char *beginWrite() {
        return begin() + writerIndex_;
    }

    const char *beginWrite() const {
        return begin() + writerIndex_;
    }

    
    





    std::vector<char> buffer_;
    size_t readerIndex_;
    size_t writerIndex_;
};