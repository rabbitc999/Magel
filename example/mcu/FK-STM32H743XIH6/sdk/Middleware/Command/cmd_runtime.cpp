#include "cmd_runtime.h"
#include "cmd_hex_parser.hpp"
#include "cmd_string_parser.hpp"
#include "../Allocator/pool.h"
#include <string.h>

/* No STL, exceptions, RTTI, global new, or runtime static constructors. */
namespace
{
struct Channel
{
    Command::HexParser hex;
    Command::StringParser text;
    CmdProtocol_T protocol;
    uint8_t owner;
    uint32_t lastByteMs;
};

class Runtime
{
public:
    MemoryPool pool;
    CmdRuntimeHooks_T hooks;
    CmdRuntimeStats_T stats;
    Channel channels[CMD_COM_COUNT];
    bool initialized, feeding;

    /**
     * @brief 从命令运行时内存池申请对齐缓冲，失败时累计分配失败次数。
     * @param context 所属命令运行时实例。
     * @param bytes 本次需要申请的字节容量。
     * @return 申请到的缓冲指针；失败为空。
     */
    static void *allocate(void *context, size_t bytes)
    {
        Runtime *self = static_cast<Runtime *>(context);
        void *result = memoryPoolAllocate(&self->pool, bytes, 8);
        if (result == nullptr) ++self->stats.allocationFailures;
        return result;
    }

    /**
     * @brief 归还命令缓冲，无效释放累计到统计中。
     * @param context 所属命令运行时实例。
     * @param pointer 先前从此运行时申请的指针。
     */
    static void release(void *context, void *pointer)
    {
        Runtime *self = static_cast<Runtime *>(context);
        if (!memoryPoolRelease(&self->pool, pointer)) ++self->stats.invalidFrees;
    }

    /**
     * @brief 累计二进制帧计数并同步转发完整命令包。
     * @param context 所属命令运行时实例。
     * @param port 命令通信通道编号。
     * @param packet 调用期间借用的命令包与载荷缓冲。
     */
    static void hexPacket(void *context, uint8_t port, CmdHexDataPacket_T *packet)
    {
        Runtime *self = static_cast<Runtime *>(context);
        ++self->stats.hexFrames;
        if (self->hooks.hexPacket != nullptr)
            self->hooks.hexPacket(self->hooks.context, port, packet);
    }

    /**
     * @brief 累计命令行计数并同步转发完整文本行。
     * @param context 所属命令运行时实例。
     * @param port 命令通信通道编号。
     * @param line 完整命令行，回调返回后不得继续持有。
     * @param length 数据缓冲中有效内容的字节数。
     */
    static void stringLine(void *context, uint8_t port, char *line, uint16_t length)
    {
        Runtime *self = static_cast<Runtime *>(context);
        ++self->stats.stringLines;
        if (self->hooks.stringLine != nullptr)
            self->hooks.stringLine(self->hooks.context, port, line, length);
    }

    /**
     * @brief 累计 CRC 错误次数并通知宿主校验失败。
     * @param context 所属命令运行时实例。
     * @param port 命令通信通道编号。
     * @param command 校验失败的命令编号。
     * @param expected 根据接收内容算出的 CRC。
     * @param received 线上收到的 CRC。
     */
    static void crcError(void *context, uint8_t port, uint8_t command, uint32_t expected, uint32_t received)
    {
        Runtime *self = static_cast<Runtime *>(context);
        ++self->stats.crcErrors;
        if (self->hooks.crcError != nullptr)
            self->hooks.crcError(self->hooks.context, port, command, expected, received);
    }

    /**
     * @brief 累计文本行缓冲溢出次数。
     * @param context 所属命令运行时实例。
     */
    static void overflow(void *context, uint8_t)
    {
        ++static_cast<Runtime *>(context)->stats.stringOverflows;
    }

    /**
     * @brief 释放指定通道的接收缓冲并清除自动协议归属。
     * @param port 命令通信通道编号。
     */
    void reset(uint8_t port)
    {
        channels[port].hex.reset();
        channels[port].text.reset();
        channels[port].owner = 0;
    }

    /**
     * @brief 清理全部通道的接收状态并将命令运行时标为未初始化。
     */
    void deinit()
    {
        if (initialized)
            for (uint8_t port = 0; port < CMD_COM_COUNT; ++port) reset(port);
        initialized = false;
    }

    /**
     * @brief 绑定命令内存池和宿主回调，初始化每个通道为自动识别协议。
     * @param arena 由调用方维持有效的连续内存池缓冲。
     * @param bytes 外部命令内存池的总字节容量。
     * @param output 初始化时复制的宿主回调表。
     * @return 初始化成功为一，正在接收、回调无效或内存池无效时为零。
     */
    uint8_t init(void *arena, size_t bytes, const CmdRuntimeHooks_T *output)
    {
        if (feeding) return 0;
        deinit();
        memset(&stats, 0, sizeof(stats));
        if (output == nullptr || !memoryPoolInit(&pool, arena, bytes)) return 0;
        hooks = *output;
        Command::Allocator allocator = { this, allocate, release };
        Command::HexSink hexSink = { this, hexPacket, crcError };
        Command::StringSink stringSink = { this, stringLine, overflow };
        for (uint8_t port = 0; port < CMD_COM_COUNT; ++port)
        {
            Channel &channel = channels[port];
            channel.hex.init(allocator, hexSink, port);
            channel.text.init(allocator, stringSink, port, CMD_STR_BUFFER_SIZE);
            channel.protocol = CMD_PROTOCOL_AUTO;
            channel.owner = 0;
            channel.lastByteMs = 0;
        }
        initialized = true;
        return 1;
    }

    /**
     * @brief 判断字节能否作为自动协议模式中文本命令的起始字符。
     * @param byte 当前接收的单个字节。
     * @return 字节为字母或下划线时为 true，否则为 false。
     */
    static bool textStart(uint8_t byte)
    {
        return (byte >= 'a' && byte <= 'z') || (byte >= 'A' && byte <= 'Z') || byte == '_';
    }

    /**
     * @brief 按通道协议分派接收字节，处理超时并拒绝重入。
     * @param port 命令通信通道编号。
     * @param data 待处理的数据缓冲。
     * @param bytes 本段输入数据的字节数。
     * @param strict 是否强制检查所有二进制帧的 CRC；文件操作始终校验。
     * @return 本次处理产生二进制完整帧时为一，否则为零。
     */
    uint8_t feed(uint8_t port, const uint8_t *data, size_t bytes, bool strict)
    {
        if (!initialized || port >= CMD_COM_COUNT || data == nullptr || bytes == 0) return 0;
        if (feeding)
        {
            ++stats.reentryRejected;
            return 0;
        }
        Channel &channel = channels[port];
        if (channel.protocol == CMD_PROTOCOL_DISABLED) return 0;
        feeding = true;
        const uint32_t now = hooks.milliseconds != nullptr ? hooks.milliseconds(hooks.context) : 0;
        if ((channel.hex.active() || channel.text.active()) && hooks.milliseconds != nullptr && uint32_t(now - channel.lastByteMs) >= CMD_RUNTIME_TIMEOUT_MS)
        {
            reset(port);
            ++stats.timeouts;
        }
        channel.lastByteMs = now;
        const uint32_t before = stats.hexFrames;
        for (size_t index = 0; index < bytes; ++index)
        {
            const uint8_t byte = data[index];
            if (channel.protocol == CMD_PROTOCOL_HEX)
                channel.hex.feed(byte, strict);
            else if (channel.protocol == CMD_PROTOCOL_STRING)
                channel.text.feed(byte);
            else
            {
                if (channel.owner == 0)
                {
                    if (byte == 0x55)
                        channel.owner = 1;
                    else if (textStart(byte))
                        channel.owner = 2;
                    else
                    {
                        if (hooks.idleByte != nullptr) hooks.idleByte(hooks.context, port, byte);
                        continue;
                    }
                }
                /* ASCII 'U' is also the first HEX header byte. Replay a non-HEX prefix to text. */
                if (channel.owner == 1 && channel.hex.awaitingHeader() && byte != 0x55 && byte != 0xAA)
                {
                    channel.hex.reset();
                    channel.owner = 2;
                    channel.text.feed('U');
                }
                if (channel.owner == 1)
                {
                    channel.hex.feed(byte, strict);
                    if (!channel.hex.active()) channel.owner = 0;
                }
                else
                {
                    channel.text.feed(byte);
                    if (!channel.text.active()) channel.owner = 0;
                }
            }
        }
        feeding = false;
        return stats.hexFrames != before;
    }

    /**
     * @brief 构造包含时间戳和 CRC 的二进制帧，调用宿主发送后归还临时缓冲。
     * @param port 命令通信通道编号。
     * @param packet 调用期间借用的命令包与载荷缓冲。
     * @param timestamp 写入发送帧的毫秒时间戳。
     * @return 宿主接受发送时为一，参数错误、分配失败或发送拒绝时为零。
     */
    uint8_t send(uint16_t port, const CmdHexDataPacket_T *packet, uint64_t timestamp)
    {
        if (!initialized || port >= CMD_COM_COUNT || packet == nullptr || packet->dataLength > CMD_HEX_MAX_PAYLOAD || (packet->dataLength != 0 && packet->data == nullptr) || hooks.send == nullptr)
        {
            ++stats.transmitRejected;
            return 0;
        }
        const uint16_t length = packet->dataLength, bytes = length + 20;
        uint8_t *frame = static_cast<uint8_t *>(allocate(this, bytes));
        if (frame == nullptr)
        {
            ++stats.transmitRejected;
            return 0;
        }
        frame[0] = 0x55;
        frame[1] = 0xAA;
        frame[2] = packet->commandId;
        frame[3] = packet->funcId;
        frame[4] = length >> 8;
        frame[5] = length;
        for (unsigned i = 0; i < 8; ++i) frame[6 + i] = timestamp >> (56 - 8 * i);
        if (length != 0) memcpy(frame + 14, packet->data, length);
        const uint32_t crc = Command::HexParser::crc32(frame + 2, length + 12);
        for (unsigned i = 0; i < 4; ++i) frame[14 + length + i] = crc >> (24 - 8 * i);
        frame[18 + length] = 0xFF;
        frame[19 + length] = 0xFE;
        const uint8_t accepted = hooks.send(hooks.context, port, frame, bytes);
        release(this, frame);
        if (!accepted) ++stats.transmitRejected;
        return accepted;
    }
};

Runtime runtime;
}  // namespace

extern "C" {
/**
 * @brief 用调用方提供的池和回调初始化各命令通道。
 * @param arena 由调用方维持有效的连续内存池缓冲。
 * @param bytes 外部命令内存池的总字节容量。
 * @param hooks 运行时借用的宿主回调表。
 * @return 初始化成功为一，参数无效或当前正在接收时为零。
 */
uint8_t CMD_RuntimeInit(void *arena, size_t bytes, const CmdRuntimeHooks_T *hooks)
{
    return runtime.init(arena, bytes, hooks);
}

/**
 * @brief 在非接收回调期间清理全部通道并停用命令运行时。
 */
void CMD_RuntimeDeinit(void)
{
    if (!runtime.feeding) runtime.deinit();
}

/**
 * @brief 巡检命令通道并释放超过接收超时的半帧或半行。
 */
void CMD_RuntimePoll(void)
{
    if (!runtime.initialized || runtime.feeding || runtime.hooks.milliseconds == nullptr) return;
    const uint32_t now = runtime.hooks.milliseconds(runtime.hooks.context);
    for (uint8_t port = 0; port < CMD_COM_COUNT; ++port)
    {
        Channel &channel = runtime.channels[port];
        if ((channel.hex.active() || channel.text.active()) && uint32_t(now - channel.lastByteMs) >= CMD_RUNTIME_TIMEOUT_MS)
        {
            runtime.reset(port);
            ++runtime.stats.timeouts;
        }
    }
}

/**
 * @brief 把一段指定通道的数据交给命令协议状态机。
 * @param port 命令通信通道编号。
 * @param data 待处理的数据缓冲。
 * @param bytes 本段输入数据的字节数。
 * @param strict 是否强制检查所有二进制帧的 CRC；文件操作始终校验。
 * @return 处理产生二进制完整帧时为一，否则为零。
 */
uint8_t CMD_RuntimeFeed(uint8_t port, const uint8_t *data, size_t bytes, uint8_t strict)
{
    return runtime.feed(port, data, bytes, strict != 0);
}

/**
 * @brief 清空通道半包并切换其协议类型，拒绝无效通道和重入。
 * @param port 命令通信通道编号。
 * @param protocol 通道需要使用的自动、二进制、文本或禁用模式。
 * @return 切换成功为一，参数或运行状态不允许时为零。
 */
uint8_t CMD_RuntimeSetProtocol(uint8_t port, CmdProtocol_T protocol)
{
    if (!runtime.initialized || runtime.feeding || port >= CMD_COM_COUNT || protocol < CMD_PROTOCOL_AUTO || protocol > CMD_PROTOCOL_DISABLED) return 0;
    runtime.reset(port);
    runtime.channels[port].protocol = protocol;
    return 1;
}

/**
 * @brief 在运行时已初始化且未接收时清空一个有效通道的状态。
 * @param port 命令通信通道编号。
 */
void CMD_RuntimeResetPort(uint8_t port)
{
    if (runtime.initialized && !runtime.feeding && port < CMD_COM_COUNT) runtime.reset(port);
}

/**
 * @brief 通过命令运行时构造并发送一个二进制命令包。
 * @param port 命令通信通道编号。
 * @param packet 调用期间借用的命令包与载荷缓冲。
 * @param timestampMs 写入发送帧的毫秒时间戳。
 * @return 发送被接受时为一，否则为零。
 */
uint8_t CMD_RuntimeSendHex(uint16_t port, const CmdHexDataPacket_T *packet, uint64_t timestampMs)
{
    return runtime.send(port, packet, timestampMs);
}

/**
 * @brief 复制命令统计并补充内存池容量、占用和峰值。
 * @param stats 接收运行时计数与内存使用统计。
 */
void CMD_RuntimeGetStats(CmdRuntimeStats_T *stats)
{
    if (stats == nullptr) return;
    *stats = runtime.stats;
    stats->capacity = static_cast<uint32_t>(runtime.pool.capacity);
    stats->usedBytes = static_cast<uint32_t>(runtime.pool.stats.liveBytes);
    stats->peakBytes = static_cast<uint32_t>(runtime.pool.stats.peakBytes);
    stats->liveBlocks = static_cast<uint32_t>(runtime.pool.stats.liveBlocks);
}
}
