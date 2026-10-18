#ifndef __GUI_MEMORY_PAINTER_H
#define __GUI_MEMORY_PAINTER_H

#include <stdint.h>
#include "gui_painter_base.h"  // 依赖基础绘图函数

#ifdef __cplusplus
extern "C" {
#endif

/*
 * 内存绘图设备结构体
 * 封装了缓冲区信息和回调函数
 */
typedef struct __GUI_MemoryDevice_T
{
    uint16_t *Buffer;      // 指向内存缓冲区的指针
    uint16_t Width;        // 缓冲区宽度
    uint16_t Height;       // 缓冲区高度
    GUI_DeviceInfo_T Dev;  // GUI核心设备信息，绑定了回调
} GUI_MemoryDevice_T;

/*
 * 函数声明
 */

/**
 * @brief 初始化内存绘图设备
 * @param memDev 内存设备结构体指针
 * @param buffer 外部传入的缓冲区指针 (建议使用外部SRAM)
 * @param width 缓冲区宽度
 * @param height 缓冲区高度
 * @param startX 图像在屏幕上的X偏移
 * @param startY 图像在屏幕上的Y偏移
 * @note 必须先调用此函数才能使用绘图功能
 */
void GUI_MemoryPainter_Init(GUI_MemoryDevice_T *memDev, uint16_t *buffer, uint16_t width, uint16_t height);

/**
 * @brief 清空内存缓冲区
 * @param memDev 内存设备结构体指针
 * @param color 颜色值
 */
void GUI_MemoryPainter_Clear(GUI_MemoryDevice_T *memDev, uint32_t color);

#ifdef __cplusplus
}
#endif

#endif /* __GUI_MEMORY_PAINTER_H */