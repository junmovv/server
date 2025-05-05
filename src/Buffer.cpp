#include "Buffer.h"
#include "logger.h"
#include <errno.h>
#include <sys/uio.h>
#include <unistd.h>

/**
 * @file Buffer.cpp
 * @brief 缓冲区IO操作实现
 */

/**
 * @brief 使用readv系统调用优化读取操作
 * @note 采用双缓冲区策略：
 *       1. 首先尝试填充缓冲区剩余空间
 *       2. 剩余空间不足时使用栈上的64K临时缓冲区
 *       3. 最后将临时缓冲区的数据追加到主缓冲区
 */
ssize_t Buffer::read_fd(int fd, int &saveErrno)
{
    char extrabuf[65536] = {0}; // 64KB栈临时缓冲区
    struct iovec vec[2];

    const size_t writable = writable_bytes();
    vec[0].iov_base = begin_write();
    vec[0].iov_len = writable;
    vec[1].iov_base = extrabuf;
    vec[1].iov_len = sizeof(extrabuf);

    // 当剩余空间小于64K时启用双缓冲区
    const int iovcnt = (writable < sizeof(extrabuf)) ? 2 : 1;
    const ssize_t n = ::readv(fd, vec, iovcnt);

    if (n < 0)
    {
        saveErrno = errno;
    }
    else if (n <= writable)
    {
        writerIndex_ += n; // 数据完全存入主缓冲区
    }
    else
    {
        writerIndex_ = buffer_.size();  // 主缓冲区写满
        append(extrabuf, n - writable); // 将额外数据追加到缓冲区
    }
    return n;
}