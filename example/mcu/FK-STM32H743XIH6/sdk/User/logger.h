#ifndef LOGGER_H
#define LOGGER_H
#include <stdint.h>
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
/**
 * @brief 日志过滤级别，按详细到关闭输出的顺序排列。
 */
typedef enum
{
    LOG_DEBUG,
    LOG_INFO,
    LOG_WARN,
    LOG_ERROR,
    LOG_OFF
} LogLevel_T;
/**
 * @brief 有界日志队列的占用、发送、丢弃和过滤统计。
 */
typedef struct
{
    uint32_t queued, peak, written, dropped, truncated, filtered, sinkBusy;
} LogStats_T;
/* snprintf contract: return required length excluding NUL, or negative on error.
 * Callback receives a bounded, NUL terminated message. Never retain its pointers.
 * Set during startup, after Log_Init. NULL restores the configured default.
 * Callback must respect capacity; no recursive logging or blocking operations.
 */
/**
 * @brief 同步日志格式化回调；遵守输出容量，返回所需长度，失败返回负数。
 */
typedef int (*LogFormatter_T)(char *buffer, size_t capacity, uint64_t timestampMs,
                              LogLevel_T level, const char *module, const char *message);
/**
 * @brief 在临界区内替换日志文本格式化回调。
 * @param formatter 格式化回调；为空时使用默认日志格式。
 */
void Log_SetFormatter(LogFormatter_T formatter);
/* Task context only. Fixed storage; no allocation or waiting. */
/**
 * @brief 清空日志队列和统计，恢复默认级别并关闭串口输出。
 */
void Log_Init(void);
/**
 * @brief 格式化任务日志并尝试放入有界队列，拒绝中断调用和队列溢出。
 * @param level 日志级别或过滤门限。
 * @param module 日志模块标签；为空时使用 APP。
 * @param format 日志正文的 printf 格式串。
 * @param ... 与格式串对应的日志参数。
 */
void Log_Write(LogLevel_T level, const char *module, const char *format, ...);
/**
 * @brief 非阻塞发送至多两条排队日志，输出繁忙时保留记录。
 */
void Log_Poll(void); /* Exactly one consumer. */
/**
 * @brief 设置日志级别门限，忽略范围外的级别值。
 * @param level 日志级别或过滤门限。
 */
void Log_SetLevel(LogLevel_T level);
/**
 * @brief 在临界区内复制日志队列与丢弃统计。
 * @param stats 接收日志队列与发送统计。
 */
void Log_GetStats(LogStats_T *stats);
/**
 * @brief 处理日志开关、级别、测试和统计查询命令。
 * @param port 命令通信通道编号。
 * @param action 日志开关、级别、测试或统计命令。
 * @return 识别并处理命令时为一，否则为零。
 */
uint8_t Log_Command(uint8_t port, const char *action);
#ifdef __cplusplus
}
#endif
#endif
