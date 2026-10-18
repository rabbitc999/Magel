#ifndef GUI_RUNTIME_H
#define GUI_RUNTIME_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Cooperative GUI task only; never allocate from an interrupt. */
/**
 * @brief 同步输出 RGB565 条带的回调；像素仅在调用期间有效，成功返回非零。
 */
typedef int (*GUI_BandWriter)(uint16_t y, uint16_t width, uint16_t rows, const uint8_t *pixels);
/**
 * @brief 绑定 GUI 内存池与用于脏区比较的上一帧缓冲。
 * @param arena GUI 内存池起始地址。
 * @param bytes GUI 内存池的总字节容量。
 * @param shadow 用于保存上一帧内容的影子缓冲。
 * @param shadowBytes 影子缓冲容量，单位为字节。
 */
void GUI_RuntimeInit(void *arena, size_t bytes, void *shadow, size_t shadowBytes);
/**
 * @brief 从 GUI 池申请八字节对齐内存，失败时累计次数。
 * @param bytes 本次申请的字节数。
 * @return 分配到的指针；失败为空。
 */
void *GUI_RuntimeAllocate(size_t bytes);
/**
 * @brief 归还 GUI 内存，无效释放累计到诊断计数。
 * @param pointer 先前从 GUI 池申请的指针；空指针无操作。
 */
void GUI_RuntimeFree(void *pointer);
/**
 * @brief 读取 GUI 内存池绑定的总容量。
 * @return 池容量，单位为字节。
 */
uint32_t GUI_RuntimeCapacity(void);
/**
 * @brief 读取 GUI 内存池当前仍在使用的容量。
 * @return 当前占用，单位为字节。
 */
uint32_t GUI_RuntimeUsed(void);
/**
 * @brief 提交一帧供 GUI 条带差分传输处理，传输期间借用帧缓冲。
 * @param frame 借用的 RGB565 像素缓冲。
 * @param width 帧宽像素数。
 * @param height 帧高像素数。
 * @return 接受帧为一，忙或帧参数无效时为零。
 */
int GUI_RuntimePresent(const void *frame, uint16_t width, uint16_t height);
/**
 * @brief 执行一次条带差分传输，允许任务在条带间让出处理器。
 * @param writer 把指定条带写入 LCD 的回调。
 * @return 发送了一个条带为一，否则为零。
 */
int GUI_RuntimeService(GUI_BandWriter writer);
/**
 * @brief 在完整帧已显示且空闲时复制影子帧，并输出其尺寸。
 * @param destination 接收完整 RGB565 帧的输出缓冲。
 * @param capacity 输出帧缓冲的字节容量。
 * @param width 接收已复制帧的宽度。
 * @param height 接收已复制帧的高度。
 * @return 复制成功为一，忙、帧无效或输出空间不足时为零。
 */
int GUI_RuntimeCopyFrame(void *destination, size_t capacity, uint16_t *width, uint16_t *height);
extern volatile uint32_t g_GUIAllocationFailures, g_GUIInvalidFrees;
extern volatile uint32_t g_GUITransferBytes, g_GUIDirtyBands, g_GUISkippedBands, g_GUITransferErrors;
#ifdef __cplusplus
}
#endif
#endif
