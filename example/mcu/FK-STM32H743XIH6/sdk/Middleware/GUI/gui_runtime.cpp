#include "gui_runtime.h"
#include "../Allocator/pool.h"
#include <string.h>
/* Pool core adapted from ParseEngine/library/allocator, linked-block mode.
 * No STL, exceptions, RTTI, global new, or dynamic C++ initialization. */
volatile uint32_t g_GUIAllocationFailures, g_GUIInvalidFrees;
volatile uint32_t g_GUITransferBytes, g_GUIDirtyBands, g_GUISkippedBands, g_GUITransferErrors;
namespace
{
class GuiRuntime
{
public:
    MemoryPool pool;
    uint8_t *shadow;
    size_t shadowBytes;
    const uint8_t *frame;
    uint16_t width, height, row;
    bool active, valid;
    /**
     * @brief 绑定 GUI 内存池和上一帧缓冲，清空传输状态。
     * @param arena 由调用方维持有效的连续内存池缓冲。
     * @param bytes GUI 内存池的总字节容量。
     * @param previous 用于保存上一帧内容的影子缓冲。
     * @param previousBytes 上一帧影子缓冲容量。
     */
    void init(void *arena, size_t bytes, void *previous, size_t previousBytes)
    {
        memoryPoolInit(&pool, arena, bytes);
        shadow = static_cast<uint8_t *>(previous);
        shadowBytes = previousBytes;
        active = valid = false;
    }
    /**
     * @brief 登记待显示 RGB565 帧，尺寸变化时使上一帧比较失效。
     * @param pixels 借用的 RGB565 帧像素，传输结束前保持有效。
     * @param w 帧宽像素数。
     * @param h 帧高像素数。
     * @return 接受帧为一；忙、参数无效或缓冲不足时为零。
     */
    int present(const void *pixels, uint16_t w, uint16_t h)
    {
        if (active || !pixels || !shadow || !w || !h || size_t(w) * h * 2 > shadowBytes)
            return 0;
        if (w != width || h != height)
            valid = false;
        frame = static_cast<const uint8_t *>(pixels);
        width = w;
        height = h;
        row = 0;
        active = true;
        return 1;
    }
    /**
     * @brief 扫描变化条带，每次最多发送一个条带并更新上一帧缓冲。
     * @param write 把指定行条带写入 LCD 的回调。
     * @return 发送了一个条带为一；完成、空闲或发送失败时为零。
     */
    int service(GUI_BandWriter write)
    {
        if (!active)
            return 0;
        while (row < height)
        {
            const uint16_t rows = height - row < 16 ? height - row : 16;
            const size_t offset = size_t(row) * width * 2, bytes = size_t(rows) * width * 2;
            if (!valid || memcmp(frame + offset, shadow + offset, bytes))
            {
                if (!write || !write(row, width, rows, frame + offset))
                {
                    ++g_GUITransferErrors;
                    valid = false;
                    active = false;
                    return 0;
                }
                memcpy(shadow + offset, frame + offset, bytes);
                g_GUITransferBytes += bytes;
                ++g_GUIDirtyBands;
                row += rows;
                /* Yield after one band, so UART/IMU tasks can run. */
                return 1;
            }
            ++g_GUISkippedBands;
            row += rows;
        }
        valid = true;
        active = false;
        return 0;
    }
};
GuiRuntime runtime{};
}  // namespace
extern "C" {
/**
 * @brief 绑定 GUI 内存池与用于脏区比较的上一帧缓冲。
 * @param a GUI 内存池起始地址。
 * @param n GUI 内存池的总字节容量。
 * @param s 用于保存上一帧内容的影子缓冲。
 * @param sn 影子缓冲容量，单位为字节。
 */
void GUI_RuntimeInit(void *a, size_t n, void *s, size_t sn)
{
    runtime.init(a, n, s, sn);
}
/**
 * @brief 从 GUI 池申请八字节对齐内存，失败时累计次数。
 * @param n 本次申请的字节数。
 * @return 分配到的指针；失败为空。
 */
void *GUI_RuntimeAllocate(size_t n)
{
    void *p = memoryPoolAllocate(&runtime.pool, n, 8);
    if (!p)
        ++g_GUIAllocationFailures;
    return p;
}
/**
 * @brief 归还 GUI 内存，无效释放累计到诊断计数。
 * @param p 先前从 GUI 池申请的指针；空指针无操作。
 */
void GUI_RuntimeFree(void *p)
{
    if (p && !memoryPoolRelease(&runtime.pool, p))
        ++g_GUIInvalidFrees;
}
/**
 * @brief 读取 GUI 内存池绑定的总容量。
 * @return 池容量，单位为字节。
 */
uint32_t GUI_RuntimeCapacity(void)
{
    return runtime.pool.capacity;
}
/**
 * @brief 读取 GUI 内存池当前仍在使用的容量。
 * @return 当前占用，单位为字节。
 */
uint32_t GUI_RuntimeUsed(void)
{
    return runtime.pool.stats.liveBytes;
}
/**
 * @brief 提交一帧供 GUI 条带差分传输处理，传输期间借用帧缓冲。
 * @param p 借用的 RGB565 像素缓冲。
 * @param w 帧宽像素数。
 * @param h 帧高像素数。
 * @return 接受帧为一，忙或帧参数无效时为零。
 */
int GUI_RuntimePresent(const void *p, uint16_t w, uint16_t h)
{
    return runtime.present(p, w, h);
}
/**
 * @brief 执行一次条带差分传输，允许任务在条带间让出处理器。
 * @param w 把指定条带写入 LCD 的回调。
 * @return 发送了一个条带为一，否则为零。
 */
int GUI_RuntimeService(GUI_BandWriter w)
{
    return runtime.service(w);
}
/**
 * @brief 在完整帧已显示且空闲时复制影子帧，并输出其尺寸。
 * @param destination 接收完整 RGB565 帧的输出缓冲。
 * @param capacity 输出帧缓冲的字节容量。
 * @param width 接收已复制帧的宽度。
 * @param height 接收已复制帧的高度。
 * @return 复制成功为一，忙、帧无效或输出空间不足时为零。
 */
int GUI_RuntimeCopyFrame(void *destination, size_t capacity, uint16_t *width, uint16_t *height)
{
    size_t bytes = size_t(runtime.width) * runtime.height * 2;
    if (runtime.active || !runtime.valid || !destination || !width || !height || capacity < bytes)
    {
        return 0;
    }
    memcpy(destination, runtime.shadow, bytes);
    *width = runtime.width;
    *height = runtime.height;
    return 1;
}
}
