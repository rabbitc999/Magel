#ifndef __CMD_HEX_PARSER_HPP
#define __CMD_HEX_PARSER_HPP

#include "cmd_allocator.hpp"
#include "cmd_hex.h"

namespace Command
{
/**
 * @brief 二进制完整帧与 CRC 失败回调，帧缓冲在回调返回后不再有效。
 */
struct HexSink
{
    void *context;
    void (*packet)(void *, uint8_t, CmdHexDataPacket_T *);
    void (*crcError)(void *, uint8_t, uint8_t, uint32_t, uint32_t);
};

/**
 * @brief 使用调用方内存池逐字节解析带长度和 CRC32 的二进制命令帧。
 */
class HexParser
{
public:
    /**
     * @brief 绑定二进制命令解析器的内存、回调和通道并清空状态。
     * @param memory 解析器借用的分配与释放接口。
     * @param output 完整帧与校验错误回调。
     * @param port 命令通信通道编号。
     */
    void init(Allocator memory, HexSink output, uint8_t port);
    /**
     * @brief 释放当前帧缓冲并返回等待二进制帧头的状态。
     */
    void reset();
    /**
     * @brief 推进二进制帧状态机，完整帧通过校验后同步调用接收回调。
     * @param byte 当前接收的单个字节。
     * @param strict 是否强制检查所有二进制帧的 CRC；文件操作始终校验。
     * @return 本字节完成有效帧时为 true，否则为 false。
     */
    bool feed(uint8_t byte, bool strict);
    /**
     * @brief 判断二进制解析器是否正在接收尚未结束的帧。
     * @return 正在接收帧时为 true，否则为 false。
     */
    bool active() const;
    /**
     * @brief 判断解析器是否只接收到帧头的第一个字节。
     * @return 正在等待帧头第二字节时为 true，否则为 false。
     */
    bool awaitingHeader() const;
    /**
     * @brief 按 IEEE 多项式计算一段字节的 CRC32，包含初始值与最终取反。
     * @param data 待处理的数据缓冲。
     * @param bytes 本段输入数据的字节数。
     * @return 完整的 CRC32 校验值。
     */
    static uint32_t crc32(const uint8_t *data, size_t bytes);

private:
    Allocator allocator;
    HexSink sink;
    uint8_t channel, state, header[4];
    uint8_t *body;
    uint16_t index, bodyLength;
    uint32_t receivedCrc;
};
}  // namespace Command

#endif
