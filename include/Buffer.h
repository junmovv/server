#ifndef BUFFER_H
#define BUFFER_H

#include "noncopyable.h"
#include <vector>
#include <string>
#include <algorithm>
/**
 * @file Buffer.h
 * @brief 网络数据缓冲区实现，参考Netty ChannelBuffer设计
 */

/**
 * @class Buffer
 * @brief 高效网络数据缓冲区，支持自动扩容和内存复用
 *
 * 缓冲区内存布局：
 * @code
 * +-------------------+------------------+------------------+
 * | prependable bytes |  readable bytes  |  writable bytes  |
 * |                   |     (CONTENT)    |                  |
 * +-------------------+------------------+------------------+
 * |                   |                  |                  |
 * 0      <=      readerIndex   <=   writerIndex    <=     size
 * @endcode
 *
 * @note 特性：
 *       - 支持预分配空间减少内存分配次数
 *       - 自动处理读写指针移动
 *       - 支持高效数据追加和提取
 *       - 优化socket读取操作（分散读）
 */
class Buffer : public noncopyable
{
    static const size_t kCheapPrepend = 8;   ///< 预分配空间默认大小（用于存储协议头等元信息）
    static const size_t kInitialSize = 1024; ///< 初始缓冲区大小

public:
    /**
     * @brief 构造函数，初始化缓冲区
     * @param initialSize 初始可写空间大小（不含预分配区域）
     */
    explicit Buffer(size_t initialSize = kInitialSize)
        : buffer_(kCheapPrepend + initialSize),
          readerIndex_(kCheapPrepend),
          writerIndex_(kCheapPrepend) {}

    ~Buffer() = default;

    /**
     * @brief 获取可读数据字节数
     * @return 可读数据长度（字节）
     */
    size_t readable_bytes() const { return writerIndex_ - readerIndex_; }

    /**
     * @brief 获取当前可写空间字节数
     * @return 剩余可写空间
     */
    size_t writable_bytes() const { return buffer_.size() - writerIndex_; }

    /**
     * @brief 获取可复用前置空间字节数
     * @return 可通过移动数据复用的空间
     */
    size_t prependable_bytes() const { return readerIndex_; }

    /**
     * @brief 获取可读数据起始地址
     * @return 只读指针，指向第一个可读字节
     */
    const char *peek() const { return begin() + readerIndex_; }

    /**
     * @brief 移动读指针，标记数据已消费
     * @param len 要跳过的字节数
     * @note 当len超过可读数据时自动重置所有指针
     */
    void retrieve(size_t len)
    {
        if (len < readable_bytes())
        {
            readerIndex_ += len;
        }
        else
        {
            retrieve_all();
        }
    }

    /**
     * @brief 重置读写指针，回收所有空间
     */
    void retrieve_all()
    {
        readerIndex_ = writerIndex_ = kCheapPrepend;
    }

    /**
     * @brief 提取指定长度数据为字符串
     * @param len 要提取的字节数
     * @return 包含提取数据的字符串
     * @note 自动移动读指针，len超过可读数据时提取全部
     */
    std::string retrieve_as_string(size_t len)
    {
        len = std::min(len, readable_bytes());
        std::string result(peek(), len);
        retrieve(len);
        return result;
    }

    /**
     * @brief 提取全部可读数据为字符串
     * @return 包含所有可读数据的字符串
     */
    std::string retrieve_all_as_string()
    {
        return retrieve_as_string(readable_bytes());
    }

    /**
     * @brief 确保至少有len字节的可写空间
     * @param len 需要的最小可写空间
     * @note 空间不足时自动扩展或移动数据
     */
    void ensure_writable_bytes(size_t len)
    {
        if (writable_bytes() < len)
        {
            make_space(len);
        }
    }

    /**
     * @brief 追加数据到缓冲区
     * @param data 要追加的字符串
     */
    void append(const std::string &data)
    {
        append(data.data(), data.size());
    }

    /**
     * @brief 追加原始数据到缓冲区
     * @param data 要追加的数据指针
     * @param len 数据长度
     */
    void append(const char *data, size_t len)
    {
        ensure_writable_bytes(len);
        std::copy(data, data + len, begin_write());
        writerIndex_ += len;
    }

    /**
     * @brief 获取当前写位置指针
     * @return 可写区域起始地址
     */
    char *begin_write() { return begin() + writerIndex_; }

    /**
     * @brief 获取当前写位置指针（const版本）
     */
    const char *begin_write() const { return begin() + writerIndex_; }

    /**
     * @brief 从文件描述符读取数据到缓冲区
     * @param fd 源文件描述符
     * @param saveErrno 输出参数，保存错误码
     * @return 读取的字节数（<0表示错误）
     * @note 使用分散读优化大数据量接收，避免多次内存拷贝
     */
    ssize_t read_fd(int fd, int &saveErrno);

private:
    /**
     * @brief 获取底层缓冲区首地址
     */
    char *begin() { return buffer_.data(); }
    const char *begin() const { return buffer_.data(); }

    /**
     * @brief 调整缓冲区空间策略
     * @param len 需要的最小空间
     * @note 扩容策略：
     *       1. 当总可用空间不足时，直接扩容
     *       2. 当可通过移动数据获得足够空间时，移动数据复用空间
     */
    void make_space(size_t len)
    {
        if (writable_bytes() + prependable_bytes() < len + kCheapPrepend)
        {
            buffer_.resize(writerIndex_ + len);
        }
        else
        {
            size_t readable = readable_bytes();
            std::copy(begin() + readerIndex_, begin_write(), begin() + kCheapPrepend);
            readerIndex_ = kCheapPrepend;
            writerIndex_ = readerIndex_ + readable;
        }
    }

    std::vector<char> buffer_; ///< 底层数据存储容器
    size_t readerIndex_;       ///< 读指针位置
    size_t writerIndex_;       ///< 写指针位置
};
#endif