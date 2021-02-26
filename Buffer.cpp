#include "Buffer.h"
#include <errno.h>
#include <sys/uio.h>
#include <unistd.h>

/**
 * 维护了一个vector<char>Buffer, 和两个读写的int Index，使用int而非指针或迭代器，是因为vector的迭代器失效问题
 * 
 * Buffer的布局：  0-----8---------------readIndex--------------writeIndex-----------------size-----capacity
 *                空白头部  读过后的空闲区            待读数据                  可写的空白区
 * 
 * append函数：往buffer写入数据，写之前先检查可写空白区间够不够，如果不够，就使用makeSpace扩容。
 * 
 * makeSpace：先检查【读后的空白区 + 可写的空白区】是否够写
 *            不够：直接扩容，假设需要写入n字节，则扩容至 writeIndex + n
 *             够：把待读数据挪动到开头，把空白区合并即可
 * 
 * 注：我们使用的容量，应该看size，而非capacity。
*/


size_t Buffer::readFd(int fd, int* saveErrno){
    char extrabuf[65536] = {0};
    struct iovec vec[2];

    const size_t writable = writeableBytes();

    vec[0].iov_base = beginWrite();
    vec[0].iov_len = writable;

    vec[1].iov_base = extrabuf;
    vec[1].iov_len = sizeof extrabuf;

    const int iovcnt = writable < sizeof(extrabuf) ? 2 : 1;
    const size_t n = ::readv(fd, vec, iovcnt);

    if(n < 0){
        *saveErrno = errno;
    }else if(n <= writable){
        writeIndex_ += n;
    }else{
        writeIndex_ = buffer_.size();
        append(extrabuf, n - writable);
    }
    return n;
}

size_t Buffer::writeFd(int fd, int* saveErrno){
    size_t n = ::write(fd, peek(), readableBytes());
    if(n < 0){
        *saveErrno = errno;
    }
    return n;
}