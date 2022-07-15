#include "Buffer.h"

#include <errno.h>
#include <sys/uio.h>
#include <unistd.h>
/**
 * 从fd上读取数据  poller默认工作在LT模式
 * Buffer缓冲区是有大小的，但是从fd上读数据的时候，却不知道tcp数据最终的大小
 * 解决方案：开辟一个栈空间来暂存放不下的数据
 */
ssize_t Buffer::readFd(int fd, int* saveErrno)
{
    char extrabuf[65536] = {0}; // 栈上的内存空间 64k

    struct iovec vec[2];

    const size_t writable = writableBytes(); // 这是buffer底层缓冲区剩余的可写空间大小
    vec[0].iov_base = begin() + writerIndex_;
    vec[0].iov_len = writable;

    vec[1].iov_base = extrabuf;
    vec[1].iov_len = sizeof extrabuf;

    // 如果可写缓冲区的大小小于64k，则总缓冲区为可写缓冲区+栈缓冲区；
    // 如果可写缓冲区的大小大于64k，则总缓冲区为可写缓冲区；
    const int iovcnt = (writable < sizeof extrabuf) ? 2 : 1; 
    const ssize_t n = ::readv(fd, vec, iovcnt);
    if (n < 0)
    {
        *saveErrno = errno;
    }
    else if (n <= writable) // Buffer的可写缓冲区已经够存储读出来的数据了
    {
        writerIndex_ += n;
    }
    else // extrabuf里面也写入了数据
    {
        writerIndex_ = buffer_.size();
        append(extrabuf, n - writable); // writerIndex_开始写 n-writable大小的数据
    }

    return n;
}

ssize_t Buffer::writeFd(int fd, int* saveErrno)
{
    ssize_t n = ::write(fd, peek(), readableBytes());
    if (n < 0)
    {
        *saveErrno = errno;
    }
    return n;
}
