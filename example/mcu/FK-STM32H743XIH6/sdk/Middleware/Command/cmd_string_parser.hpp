#ifndef __CMD_STRING_PARSER_HPP
#define __CMD_STRING_PARSER_HPP

#include "cmd_allocator.hpp"
#include <stdint.h>

namespace Command
{
/**
 * @brief 完整文本行及行溢出回调；行指针仅在回调期间有效。
 */
struct StringSink
{
    void *context;
    void (*line)(void *, uint8_t, char *, uint16_t);
    void (*overflow)(void *, uint8_t);
};

/**
 * @brief 使用有界池缓冲逐字节组装文本行，溢出时丢弃整行。
 */
class StringParser
{
public:
    /**
     * @brief 绑定文本命令解析器的内存、回调、通道和行缓冲容量。
     * @param memory 解析器借用的分配与释放接口。
     * @param output 完整行与溢出事件回调。
     * @param port 命令通信通道编号。
     * @param capacity 单行缓冲容量，包含字符串结束符。
     */
    void init(Allocator memory, StringSink output, uint8_t port, uint16_t capacity);
    /**
     * @brief 释放文本行缓冲并清空接收、溢出和回车状态。
     */
    void reset();
    /**
     * @brief 接收一个文本字节，换行完成时回调，溢出后丢弃整行。
     * @param byte 当前接收的单个字节。
     * @return 本字节结束了可执行的完整命令行时为 true，否则为 false。
     */
    bool feed(uint8_t byte);
    /**
     * @brief 判断文本解析器是否正在接收一条命令行。
     * @return 已有未结束命令行时为 true，否则为 false。
     */
    bool active() const;

private:
    Allocator allocator;
    StringSink sink;
    char *buffer;
    uint16_t index, capacity;
    uint8_t channel;
    bool receiving, dropping, finalCarriage;
};
}  // namespace Command

#endif
