#include "gui_memory_painter.h"
#include <string.h>
#include "gui.h"  // 包含Memory_Malloc等内存管理函数

// 静态函数声明：底层回调函数
static void _MemDev_DrawPoint(GUI_DeviceInfo_T *deviceInfo, uint16_t x, uint16_t y, uint32_t color);
static void _MemDev_FillColor(GUI_DeviceInfo_T *deviceInfo, uint16_t x, uint16_t y, uint16_t Width, uint16_t Height, uint32_t color);

/**
 * @brief 内存设备专用的画点回调
 * @note 这个函数直接操作内存数组，不涉及硬件IO
 */
static void _MemDev_DrawPoint(GUI_DeviceInfo_T *deviceInfo, uint16_t x, uint16_t y, uint32_t color)
{
    GUI_MemoryDevice_T *memDev = (GUI_MemoryDevice_T *)deviceInfo->tagInfo;

    // 边界检查
    if (x < memDev->Width && y < memDev->Height)
    {
        // 如果LCD需要Swap，这里也做Swap以保持一致
        memDev->Buffer[y * memDev->Width + x] = deviceInfo->swapFlag ? GUI_SwapBytes(color) : (uint16_t)color;
    }
}

/**
 * @brief 内存设备专用的填充回调
 * @note 比逐点绘制快得多
 */
static void _MemDev_FillColor(GUI_DeviceInfo_T *deviceInfo, uint16_t x, uint16_t y, uint16_t Width, uint16_t Height, uint32_t color)
{
    GUI_MemoryDevice_T *memDev = (GUI_MemoryDevice_T *)deviceInfo->tagInfo;
    uint16_t i, j;
    uint16_t cl = deviceInfo->swapFlag ? GUI_SwapBytes(color) : (uint16_t)color;

    // 限制在缓冲区范围内
    if (x >= memDev->Width || y >= memDev->Height) return;
    if (x + Width > memDev->Width) Width = memDev->Width - x;
    if (y + Height > memDev->Height) Height = memDev->Height - y;

    for (j = y; j < y + Height; j++)
    {
        for (i = x; i < x + Width; i++)
        {
            memDev->Buffer[j * memDev->Width + i] = cl;
        }
    }
}

/**
 * @brief 初始化内存绘图器
 */
void GUI_MemoryPainter_Init(GUI_MemoryDevice_T *memDev, uint16_t *buffer, uint16_t width, uint16_t height)
{
    if (memDev == NULL || buffer == NULL) return;

    // 1. 初始化缓冲区信息
    memDev->Buffer = buffer;
    memDev->Width = width;
    memDev->Height = height;

    // 2. 初始化 GUI_DeviceInfo 回调
    // 清空结构体
    memset(&memDev->Dev, 0, sizeof(GUI_DeviceInfo_T));

    // 设置回调函数
    memDev->Dev.drawPointCallback = _MemDev_DrawPoint;
    memDev->Dev.fillColorCallback = _MemDev_FillColor;

    // 继承 LCD 的 Swap 设置
    memDev->Dev.swapFlag = 0;  // 默认不交换，根据实际屏幕配置修改

    // 3. 设置填充模式
    memDev->Dev.fillMode = GUI_PAINTER_FILL_COLOR;

    // 4. 关键步骤：将 memDev 自身指针传递给底层回调，用于定位 Buffer
    memDev->Dev.tagInfo = memDev;

    // 清空缓冲区
    GUI_MemoryPainter_Clear(memDev, GUI_BLACK);
}

/**
 * @brief 清空内存缓冲区
 */
void GUI_MemoryPainter_Clear(GUI_MemoryDevice_T *memDev, uint32_t color)
{
    if (memDev == NULL || memDev->Buffer == NULL) return;

    // 利用基础绘图库的 Fill 函数，配合上面设置的回调来清空内存
    GUI_BasePainter_Fill(&memDev->Dev, 0, 0, memDev->Width, memDev->Height, color);
}