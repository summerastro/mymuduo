#include "Buffer.h"

#include <errno.h>
#include <sys/uio.h>
#include <unistd.h>



/**
 * 从fd读取数据， Poller工作在LT模式，没读完数据一直读
 * Buffer缓冲区是有大小的，但是从fd上面读数据不知道tcp的数据最终有多大
 * 
 * 返回实际读到的数据字节数
 */
ssize_t Buffer::readFd(int fd, int* saveErrno) {
    char extrabuf[65536] = {0};  // 栈上分配额外的空间 64K
    struct iovec vec[2];

    const size_t writable = writableBytes();

    vec[0].iov_base = begin() + writerIndex_;
    vec[0].iov_len = writable;

    vec[1].iov_base = extrabuf;
    vec[1].iov_len = sizeof extrabuf;

    const int iovcnt = (writable < sizeof extrabuf) ? 2 : 1;
    // readv 可以往地址不连续的多块缓冲区里 ，写入同一个 fd 上读出的数据
    // 可以判断是在一个缓冲区里写完，还是在多个缓冲区里写
    const ssize_t n = ::readv(fd, vec, iovcnt);

    if(n < 0) {
        *saveErrno = errno;
    } else if(n <= writable) {  // 一个缓冲区够
        writerIndex_ += n;
        // Buffer不够了，需要先把缓冲区扩容，再把额外缓冲区的数据写到Buffer的可写里面
    } else {
        writerIndex_ = buffer_.size();
        append(extrabuf, n - writable); // 从writeIndex_ 开始写 n-writable 大小的数据
    }

    return n;


}

ssize_t Buffer::writeFd(int fd, int* saveErrno)
{
    // 把output的可读区域里的数据读出来，写到fd上面
    // peek返回的是可读区域的起始地址，readableBytes是可读的字节数
    ssize_t n =::write(fd, peek(), readableBytes());
    if(n < 0)
    {
        *saveErrno = errno;
    }

    return n;

}