#ifndef __CMD_RUNTIME_H
#define __CMD_RUNTIME_H

#include "cmd_hex.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 命令通道的自动识别、二进制、文本或停用协议模式。
 */
typedef enum
{
    CMD_PROTOCOL_AUTO,
    CMD_PROTOCOL_HEX,
    CMD_PROTOCOL_STRING,
    CMD_PROTOCOL_DISABLED
} CmdProtocol_T;

/**
 * @brief 命令解析及发送回调表；缓冲仅在同步回调期间借用。
 */
typedef struct
{
    void *context;
    void (*hexPacket)(void *, uint8_t, CmdHexDataPacket_T *);
    void (*stringLine)(void *, uint8_t, char *, uint16_t);
    void (*idleByte)(void *, uint8_t, uint8_t);
    void (*crcError)(void *, uint8_t, uint8_t, uint32_t, uint32_t);
    /* Must synchronously copy all bytes, or reject without sending any. */
    uint8_t (*send)(void *, uint16_t, const uint8_t *, uint16_t);
    uint32_t (*milliseconds)(void *);
} CmdRuntimeHooks_T;

/**
 * @brief 命令池占用、协议帧、超时、拒绝及分配失败的累计统计。
 */
typedef struct
{
    uint32_t capacity, usedBytes, peakBytes, liveBlocks;
    uint32_t allocationFailures, invalidFrees;
    uint32_t hexFrames, stringLines, crcErrors, stringOverflows;
    uint32_t timeouts, reentryRejected, transmitRejected;
} CmdRuntimeStats_T;

/* Foreground cooperative task only. Reset/init must not run inside callbacks. */
/**
 * @brief 用调用方提供的池和回调初始化各命令通道。
 * @param arena 由调用方维持有效的连续内存池缓冲。
 * @param bytes 外部命令内存池的总字节容量。
 * @param hooks 运行时借用的宿主回调表。
 * @return 初始化成功为一，参数无效或当前正在接收时为零。
 */
uint8_t CMD_RuntimeInit(void *arena, size_t bytes, const CmdRuntimeHooks_T *hooks);
/**
 * @brief 在非接收回调期间清理全部通道并停用命令运行时。
 */
void CMD_RuntimeDeinit(void);
/**
 * @brief 巡检命令通道并释放超过接收超时的半帧或半行。
 */
void CMD_RuntimePoll(void);
/**
 * @brief 把一段指定通道的数据交给命令协议状态机。
 * @param port 命令通信通道编号。
 * @param data 待处理的数据缓冲。
 * @param bytes 本段输入数据的字节数。
 * @param strict 是否强制检查所有二进制帧的 CRC；文件操作始终校验。
 * @return 处理产生二进制完整帧时为一，否则为零。
 */
uint8_t CMD_RuntimeFeed(uint8_t port, const uint8_t *data, size_t bytes, uint8_t strict);
/**
 * @brief 清空通道半包并切换其协议类型，拒绝无效通道和重入。
 * @param port 命令通信通道编号。
 * @param protocol 通道需要使用的自动、二进制、文本或禁用模式。
 * @return 切换成功为一，参数或运行状态不允许时为零。
 */
uint8_t CMD_RuntimeSetProtocol(uint8_t port, CmdProtocol_T protocol);
/**
 * @brief 在运行时已初始化且未接收时清空一个有效通道的状态。
 * @param port 命令通信通道编号。
 */
void CMD_RuntimeResetPort(uint8_t port);
/**
 * @brief 通过命令运行时构造并发送一个二进制命令包。
 * @param port 命令通信通道编号。
 * @param packet 调用期间借用的命令包与载荷缓冲。
 * @param timestampMs 写入发送帧的毫秒时间戳。
 * @return 发送被接受时为一，否则为零。
 */
uint8_t CMD_RuntimeSendHex(uint16_t port, const CmdHexDataPacket_T *packet, uint64_t timestampMs);
/**
 * @brief 复制命令统计并补充内存池容量、占用和峰值。
 * @param stats 接收运行时计数与内存使用统计。
 */
void CMD_RuntimeGetStats(CmdRuntimeStats_T *stats);

#ifdef __cplusplus
}
#endif

#endif
