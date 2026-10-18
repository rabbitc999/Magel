#include "cmd_hex_parser.hpp"
#include <string.h>

namespace Command
{
/**
 * @brief 绑定二进制命令解析器的内存、回调和通道并清空状态。
 * @param memory 解析器借用的分配与释放接口。
 * @param output 完整帧与校验错误回调。
 * @param port 命令通信通道编号。
 */
void HexParser::init(Allocator memory, HexSink output, uint8_t port)
{
    allocator = memory;
    sink = output;
    channel = port;
    body = nullptr;
    state = 0;
    index = bodyLength = 0;
    receivedCrc = 0;
}

/**
 * @brief 释放当前帧缓冲并返回等待二进制帧头的状态。
 */
void HexParser::reset()
{
    if (body != nullptr) allocator.release(allocator.context, body);
    body = nullptr;
    state = 0;
    index = bodyLength = 0;
    receivedCrc = 0;
}

/**
 * @brief 判断二进制解析器是否正在接收尚未结束的帧。
 * @return 正在接收帧时为 true，否则为 false。
 */
bool HexParser::active() const
{
    return state != 0;
}

/**
 * @brief 判断解析器是否只接收到帧头的第一个字节。
 * @return 正在等待帧头第二字节时为 true，否则为 false。
 */
bool HexParser::awaitingHeader() const
{
    return state == 1;
}

/**
 * @brief 推进二进制帧状态机，完整帧通过校验后同步调用接收回调。
 * @param byte 当前接收的单个字节。
 * @param strict 是否强制检查所有二进制帧的 CRC；文件操作始终校验。
 * @return 本字节完成有效帧时为 true，否则为 false。
 */
bool HexParser::feed(uint8_t byte, bool strict)
{
    switch (state)
    {
    case 0:
        if (byte == 0x55) state = 1;
        break;
    case 1:
        if (byte == 0xAA)
        {
            state = 2;
            index = 0;
        }
        else if (byte != 0x55)
            reset();
        break;
    case 2:
        header[index++] = byte;
        if (index == sizeof(header))
        {
            const uint16_t length = (uint16_t(header[2]) << 8) | header[3];
            if (length > CMD_HEX_MAX_PAYLOAD)
            {
                reset();
                break;
            }
            bodyLength = length + 12;
            body = static_cast<uint8_t *>(allocator.allocate(allocator.context, bodyLength));
            if (body != nullptr) memcpy(body, header, sizeof(header));
            /* Even on OOM, consume this entire frame without routing its payload to text. */
            index = sizeof(header);
            state = 3;
        }
        break;
    case 3:
        if (body != nullptr) body[index] = byte;
        if (++index == bodyLength)
        {
            receivedCrc = 0;
            index = 0;
            state = 4;
        }
        break;
    case 4:
        receivedCrc = (receivedCrc << 8) | byte;
        if (++index == 4) state = 5;
        break;
    case 5:
        if (byte == 0xFF)
            state = 6;
        else
            reset();
        break;
    case 6:
    {
        bool complete = false;
        if (byte == 0xFE && body != nullptr)
        {
            const uint32_t calculated = crc32(body, bodyLength);
            /* File operations always require a valid wire CRC. */
            if ((!strict && header[0] != 0x12) || calculated == receivedCrc)
            {
                CmdHexDataPacket_T packet = {};
                packet.commandId = body[0];
                packet.funcId = body[1];
                packet.dataLength = bodyLength - 12;
                for (unsigned i = 0; i < 8; ++i)
                    packet.timestampMs = (packet.timestampMs << 8) | body[4 + i];
                packet.data = body + 12;
                if (sink.packet != nullptr) sink.packet(sink.context, channel, &packet);
                complete = true;
            }
            else if (sink.crcError != nullptr)
                sink.crcError(sink.context, channel, header[0], calculated, receivedCrc);
        }
        reset();
        return complete;
    }
    default:
        reset();
        break;
    }
    return false;
}

/**
 * @brief 按 IEEE 多项式计算一段字节的 CRC32，包含初始值与最终取反。
 * @param data 待处理的数据缓冲。
 * @param bytes 本段输入数据的字节数。
 * @return 完整的 CRC32 校验值。
 */
uint32_t HexParser::crc32(const uint8_t *data, size_t bytes)
{
    uint32_t crc = 0xFFFFFFFFU;
    while (bytes-- != 0)
    {
        crc ^= *data++;
        for (unsigned bit = 0; bit < 8; ++bit)
            crc = (crc >> 1) ^ (0xEDB88320U & (0U - (crc & 1U)));
    }
    return ~crc;
}
}  // namespace Command
