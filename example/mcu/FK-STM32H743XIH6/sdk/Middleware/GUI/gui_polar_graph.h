#ifndef __GUI_POLAR_GRAPH_H
#define __GUI_POLAR_GRAPH_H

#include <stdint.h>

#include "gui_painter_base.h"
#include "bsp_FIFO.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 极坐标图最大点数 */
#define GUI_POLAR_MAX_RING_COUNT 8

/* 极坐标点数据结构 */
typedef struct __GUI_PolarPoint_T
{
    float angle;        /* 角度（弧度） */
    float radius;       /* 原始半径 */
    float scaledRadius; /* 缩放后的半径 */
    uint16_t color;     /* 点颜色 */
} GUI_PolarPoint_T;

/* 极坐标环（距离圈）数据结构 */
typedef struct __GUI_PolarRing_T
{
    float radius;   /* 半径值（归一化后） */
    uint16_t color; /* 环颜色 */
    char *label;    /* 环标签 */
} GUI_PolarRing_T;

/* 极坐标图数据结构 */
typedef struct __GUI_PolarGraph_T
{
    /* 图形缓冲区 */
    uint16_t *GraphBuffer;
    uint16_t Width;
    uint16_t Height;
    uint16_t StartX;
    uint16_t StartY;

    /* 中心点坐标 */
    uint16_t centerX;
    uint16_t centerY;

    /* 颜色设置 */
    uint16_t BackgroundColor;
    uint16_t AxisColor;
    uint16_t GridColor;
    uint16_t ringColor;
    uint16_t maxPointColor;

    /* 显示控制 */
    uint8_t showGrid;
    uint8_t showRings;
    uint8_t showAxis;
    uint8_t showLabels;
    uint8_t showInBuffer;

    /* 点设置 */
    uint16_t pointSize;
    uint16_t maxPointSize;

    /* 数据相关 */
    GUI_PolarPoint_T *points;
    uint16_t pointCount;
    uint16_t maxPointCount;
    float maxRadius;
    float fixedBaseRadius; /* 固定的基准距离 */
    float scaleFactor;     /* 缩放因子 */

    /* 环设置 */
    GUI_PolarRing_T rings[GUI_POLAR_MAX_RING_COUNT];
    uint8_t ringCount;

    /* 字体 */
    GUI_FontInfo_T *pFont;
    uint16_t InitFlag;
} GUI_PolarGraph_T;

/* 函数声明 */
void GUI_PolarGraphDeinit(GUI_PolarGraph_T *polarGraph);
void GUI_PolarGraphInit(GUI_PolarGraph_T *polarGraph);
void GUI_PolarGraphDraw(GUI_PolarGraph_T *polarGraph);
void GUI_PolarGraphAddPoint(GUI_PolarGraph_T *polarGraph, float angle, float radius, uint16_t color);
void GUI_PolarGraphSetRings(GUI_PolarGraph_T *polarGraph, const float *ringRadii, const uint16_t *ringColors, const char **ringLabels, uint8_t count);
void GUI_PolarGraphClearPoints(GUI_PolarGraph_T *polarGraph);
void GUI_PolarGraphCalculateScale(GUI_PolarGraph_T *polarGraph);
void GUI_PolarGraphSetCenter(GUI_PolarGraph_T *polarGraph, uint16_t centerX, uint16_t centerY);
void GUI_PolarGraphSetFixedBaseRadius(GUI_PolarGraph_T *polarGraph, float fixedBaseRadius);

#ifdef __cplusplus
}
#endif

#endif /* __GUI_POLAR_GRAPH_H */