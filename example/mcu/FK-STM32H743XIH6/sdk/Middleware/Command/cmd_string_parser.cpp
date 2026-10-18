#include "cmd_string_parser.hpp"

namespace Command
{
/**
 * @brief 绑定文本命令解析器的内存、回调、通道和行缓冲容量。
 * @param memory 解析器借用的分配与释放接口。
 * @param output 完整行与溢出事件回调。
 * @param port 命令通信通道编号。
 * @param bytes 单行缓冲容量，包含字符串结束符。
 */
void StringParser::init(Allocator memory, StringSink output, uint8_t port, uint16_t bytes)
{
    allocator = memory;
    sink = output;
    channel = port;
    capacity = bytes;
    buffer = nullptr;
    index = 0;
    receiving = dropping = finalCarriage = false;
}

/**
 * @brief 释放文本行缓冲并清空接收、溢出和回车状态。
 */
void StringParser::reset()
{
    if (buffer != nullptr) allocator.release(allocator.context, buffer);
    buffer = nullptr;
    index = 0;
    receiving = dropping = finalCarriage = false;
}

/**
 * @brief 判断文本解析器是否正在接收一条命令行。
 * @return 已有未结束命令行时为 true，否则为 false。
 */
bool StringParser::active() const
{
    return receiving;
}

/**
 * @brief 接收一个文本字节，换行完成时回调，溢出后丢弃整行。
 * @param byte 当前接收的单个字节。
 * @return 本字节结束了可执行的完整命令行时为 true，否则为 false。
 */
bool StringParser::feed(uint8_t byte)
{
    if (!receiving)
    {
        if (byte == '\r' || byte == '\n') return false;
        receiving = true;
        buffer = static_cast<char *>(allocator.allocate(allocator.context, capacity));
        dropping = buffer == nullptr || capacity < 2;
    }
    if (byte == '\n')
    {
        const bool complete = !dropping;
        if (complete)
        {
            if (!finalCarriage && index != 0 && buffer[index - 1] == '\r') --index;
            buffer[index] = '\0';
            if (sink.line != nullptr) sink.line(sink.context, channel, buffer, index);
        }
        reset();
        return complete;
    }
    if (!dropping)
    {
        if (index + 1 < capacity && byte != 0 && !finalCarriage)
            buffer[index++] = static_cast<char>(byte);
        else if (index + 1 == capacity && byte == '\r' && !finalCarriage)
            finalCarriage = true;
        else
        {
            /* Never execute a truncated line or the tail of an overflowing command. */
            dropping = true;
            if (sink.overflow != nullptr) sink.overflow(sink.context, channel);
        }
    }
    return false;
}
}  // namespace Command
