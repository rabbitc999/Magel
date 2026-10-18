#include "logger.h"
#include "logger_config.h"
#include "stm32h7xx.h"
extern "C" uint64_t Timer_GetTimestampMs(void);
extern "C" uint8_t Log_PortSend(uint8_t port, const char *data, uint16_t length);
extern "C" void Log_PortReply(uint8_t port, const char *text);
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

/* Bounded queue shared by task producers. Never format inside a critical section. */
namespace
{
constexpr uint32_t QueueSize = 32, LineSize = 224;
struct Record
{
    uint16_t length;
    char text[LineSize];
};
Record records[QueueSize];
LogStats_T stats;
LogFormatter_T formatter;
uint32_t head, tail;
LogLevel_T threshold = LOG_INFO;
uint8_t outputEnabled, outputPort;
/**
 * @brief 保存中断屏蔽状态并进入日志队列临界区。
 * @return 进入前的 PRIMASK 值。
 */
uint32_t Lock(void)
{
    uint32_t state = __get_PRIMASK();
    __disable_irq();
    return state;
}
/**
 * @brief 恢复进入日志队列临界区之前的中断状态。
 * @param state 先前保存的 PRIMASK 中断屏蔽值。
 */
void Unlock(uint32_t state) { __set_PRIMASK(state); }
}  // namespace
/**
 * @brief 清空日志队列和统计，恢复默认级别并关闭串口输出。
 */
extern "C" void Log_Init(void)
{
    uint32_t state = Lock();
    memset(&stats, 0, sizeof(stats));
    head = tail = 0;
    threshold = LOG_INFO;
    outputEnabled = 0;
    outputPort = 0;
    formatter = nullptr;
    Unlock(state);
}
/**
 * @brief 在临界区内替换日志文本格式化回调。
 * @param value 格式化回调；为空时使用默认日志格式。
 */
extern "C" void Log_SetFormatter(LogFormatter_T value)
{
    uint32_t state = Lock();
    formatter = value;
    Unlock(state);
}
/**
 * @brief 设置日志级别门限，忽略范围外的级别值。
 * @param level 日志级别或过滤门限。
 */
extern "C" void Log_SetLevel(LogLevel_T level)
{
    if ((unsigned)level > LOG_OFF) return;
    uint32_t state = Lock();
    threshold = level;
    Unlock(state);
}
/**
 * @brief 在临界区内复制日志队列与丢弃统计。
 * @param result 接收日志队列与发送统计。
 */
extern "C" void Log_GetStats(LogStats_T *result)
{
    if (!result) return;
    uint32_t state = Lock();
    *result = stats;
    Unlock(state);
}
/**
 * @brief 格式化任务日志并尝试放入有界队列，拒绝中断调用和队列溢出。
 * @param level 日志级别或过滤门限。
 * @param module 日志模块标签；为空时使用 APP。
 * @param format 日志正文的 printf 格式串。
 * @param ... 与格式串对应的日志参数。
 */
extern "C" void Log_Write(LogLevel_T level, const char *module, const char *format, ...)
{
    if (__get_IPSR() || !format || (unsigned)level >= LOG_OFF) return;
    uint32_t state = Lock();
    if (level < threshold)
    {
        ++stats.filtered;
        Unlock(state);
        return;
    }
    LogFormatter_T render = formatter;
    Unlock(state);
    Record item = {};
    const uint64_t ms = Timer_GetTimestampMs();
    char message[LineSize] = {};
    va_list args;
    va_start(args, format);
    int messageCount = vsnprintf(message, sizeof(message), format, args);
    va_end(args);
    if (messageCount < 0) return;
    const char *tag = module ? module : "APP";
    const size_t capacity = LineSize - 2; /* Reserve CRLF; snprintf reserves NUL. */
    int count = render ? render(item.text, capacity, ms, level, tag, message) : snprintf(item.text, capacity, LOG_DEFAULT_FORMAT, (unsigned long long)ms, "DIWE"[level], tag, message);
    if (count < 0) return;
    uint32_t used = (uint32_t)count;
    bool truncated = (uint32_t)messageCount >= sizeof(message) || used >= capacity;
    if (used >= capacity) used = capacity - 1;
    /* Keep each record on one line, including custom formatter output. */
    for (uint32_t i = 0; i < used; ++i)
        if (item.text[i] == '\r' || item.text[i] == '\n') item.text[i] = ' ';
    item.text[used++] = '\r';
    item.text[used++] = '\n';
    item.length = used;
    state = Lock();
    if (truncated) ++stats.truncated;
    if (stats.queued == QueueSize)
    {
        ++stats.dropped;
        Unlock(state);
        return;
    }
    records[head] = item;
    head = (head + 1) % QueueSize;
    ++stats.queued;
    ++stats.written;
    if (stats.queued > stats.peak) stats.peak = stats.queued;
    Unlock(state);
}
/**
 * @brief 非阻塞发送至多两条排队日志，输出繁忙时保留记录。
 */
extern "C" void Log_Poll(void)
{
    /* At most two records per call; never wait for DMA or FIFO space. */
    for (unsigned i = 0; i < 2; ++i)
    {
        Record item;
        uint32_t state = Lock();
        if (!outputEnabled || !stats.queued)
        {
            Unlock(state);
            return;
        }
        item = records[tail];
        uint8_t port = outputPort;
        Unlock(state);
        if (!Log_PortSend(port, item.text, item.length))
        {
            state = Lock();
            ++stats.sinkBusy;
            Unlock(state);
            return;
        }
        state = Lock();
        tail = (tail + 1) % QueueSize;
        --stats.queued;
        Unlock(state);
    }
}
/**
 * @brief 处理日志开关、级别、测试和统计查询命令。
 * @param port 命令通信通道编号。
 * @param action 日志开关、级别、测试或统计命令。
 * @return 识别并处理命令时为一，否则为零。
 */
extern "C" uint8_t Log_Command(uint8_t port, const char *action)
{
    if (!action) return 0;
    if (!strcmp(action, "on") || !strcmp(action, "off"))
    {
        uint32_t state = Lock();
        outputPort = port;
        outputEnabled = !strcmp(action, "on");
        Unlock(state);
        Log_PortReply(port, outputEnabled ? "LOG output=on\r\n" : "LOG output=off\r\n");
        return 1;
    }
    const char *levels[] = { "debug", "info", "warn", "error", "quiet" };
    for (unsigned i = 0; i < 5; ++i)
        if (!strcmp(action, levels[i]))
        {
            Log_SetLevel((LogLevel_T)i);
            return 1;
        }
    if (!strcmp(action, "test"))
    {
        Log_Write(LOG_INFO, "TEST", "logger ready");
        return 1;
    }
    if (!strcmp(action, "status"))
    {
        LogStats_T s;
        Log_GetStats(&s);
        char text[224];
        snprintf(text, sizeof(text), "LOG queued=%lu peak=%lu written=%lu dropped=%lu truncated=%lu filtered=%lu busy=%lu\r\n",
                 (unsigned long)s.queued, (unsigned long)s.peak, (unsigned long)s.written,
                 (unsigned long)s.dropped, (unsigned long)s.truncated,
                 (unsigned long)s.filtered, (unsigned long)s.sinkBusy);
        Log_PortReply(port, text);
        return 1;
    }
    return 0;
}
