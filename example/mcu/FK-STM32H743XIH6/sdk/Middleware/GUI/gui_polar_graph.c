#include "gui_polar_graph.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "gui.h"
#include "fast_math.h"

/* 静态内存设备 */
static GUI_DeviceInfo_T s_polarMemDevice = { 0 };

/* 静态函数声明 */
static void GUI_PolarGraphDrawGrid(GUI_PolarGraph_T *polarGraph);
static void GUI_PolarGraphDrawRings(GUI_PolarGraph_T *polarGraph);
static void GUI_PolarGraphDrawPoints(GUI_PolarGraph_T *polarGraph);
static void polarMemDeviceDrawPoint(GUI_DeviceInfo_T *deviceInfo, uint16_t x, uint16_t y, uint32_t color);
static void polarMemDeviceFillColor(GUI_DeviceInfo_T *deviceInfo, uint16_t x, uint16_t y, uint16_t Width, uint16_t Height, uint32_t color);

/* 外部LCD设备声明 */
extern GUI_DeviceInfo_T g_LCD_DeviceInfo;

/**
 * @brief  内存设备绘制点回调函数（极坐标图专用）
 * @param  deviceInfo 设备信息结构体指针
 * @param  x 目标点X坐标
 * @param  y 目标点Y坐标
 * @param  color 绘制颜色值
 * @note   将点绘制到极坐标图缓冲区，自动进行边界检查
 */
static void polarMemDeviceDrawPoint(GUI_DeviceInfo_T *deviceInfo, uint16_t x, uint16_t y, uint32_t color)
{
    GUI_PolarGraph_T *polarGraph = (GUI_PolarGraph_T *)deviceInfo->tagInfo;
    if (x < polarGraph->Width && y < polarGraph->Height)
    {
        polarGraph->GraphBuffer[y * polarGraph->Width + x] =
            g_LCD_DeviceInfo.swapFlag ? (uint16_t)GUI_SwapBytes(color) : (uint16_t)(color);
    }
}

/**
 * @brief  内存设备区域填充回调函数（极坐标图专用）
 * @param  deviceInfo 设备信息结构体指针
 * @param  x 起始X坐标
 * @param  y 起始Y坐标
 * @param  Width 填充区域宽度
 * @param  Height 填充区域高度
 * @param  color 填充颜色值
 * @note   自动裁剪填充区域至缓冲区边界内
 */
static void polarMemDeviceFillColor(GUI_DeviceInfo_T *deviceInfo, uint16_t x, uint16_t y, uint16_t Width, uint16_t Height, uint32_t color)
{
    GUI_PolarGraph_T *polarGraph = (GUI_PolarGraph_T *)deviceInfo->tagInfo;
    uint16_t i, j;

    /* 限制在缓冲区范围内 */
    if (x >= polarGraph->Width || y >= polarGraph->Height) return;
    if (x + Width > polarGraph->Width) Width = polarGraph->Width - x;
    if (y + Height > polarGraph->Height) Height = polarGraph->Height - y;

    for (j = y; j < y + Height; j++)
    {
        for (i = x; i < x + Width; i++)
        {
            polarGraph->GraphBuffer[j * polarGraph->Width + i] =
                g_LCD_DeviceInfo.swapFlag ? (uint16_t)GUI_SwapBytes(color) : (uint16_t)(color);
        }
    }
}

/**
 * @brief  计算缩放因子
 * @param  polarGraph 极坐标图指针
 * @note   基于固定基准距离计算缩放因子
 */
void GUI_PolarGraphCalculateScale(GUI_PolarGraph_T *polarGraph)
{
    if (polarGraph->pointCount == 0)
    {
        polarGraph->scaleFactor = 1.0f;
        polarGraph->maxRadius = 0.0f;
        return;
    }

    /* 找到最大半径 */
    polarGraph->maxRadius = 0.0f;

    for (uint16_t i = 0; i < polarGraph->pointCount; i++)
    {
        if (polarGraph->points[i].radius > polarGraph->maxRadius)
        {
            polarGraph->maxRadius = polarGraph->points[i].radius;
        }
    }

    /* 计算最大显示半径（基于图中心到边缘的最小距离） */
    uint16_t maxDisplayRadius = (polarGraph->Width < polarGraph->Height) ?
                                    (polarGraph->Width / 2) :
                                    (polarGraph->Height / 2);
    maxDisplayRadius -= 10; /* 留出边距 */

    /* 如果固定基准距离未设置或为0，使用最大半径作为基准 */
    if (polarGraph->fixedBaseRadius <= 0.0f)
    {
        polarGraph->fixedBaseRadius = polarGraph->maxRadius;
    }

    /* 计算缩放因子：使固定基准距离位于合适位置 */
    if (polarGraph->fixedBaseRadius > 0.0f)
    {
        /* 缩放因子使固定基准距离位于75%的最大显示半径处 */
        polarGraph->scaleFactor = (maxDisplayRadius * 0.75f) / polarGraph->fixedBaseRadius;

        /* 计算缩放后的半径 */
        for (uint16_t i = 0; i < polarGraph->pointCount; i++)
        {
            polarGraph->points[i].scaledRadius = polarGraph->points[i].radius * polarGraph->scaleFactor;

            /* 限制在显示范围内 */
            if (polarGraph->points[i].scaledRadius > maxDisplayRadius)
            {
                polarGraph->points[i].scaledRadius = maxDisplayRadius;
                polarGraph->points[i].color = GUI_GRAY;
            }
        }
    }
    else
    {
        polarGraph->scaleFactor = 1.0f;
    }
}

/**
 * @brief  绘制极坐标网格
 * @param  polarGraph 极坐标图指针
 */
static void GUI_PolarGraphDrawGrid(GUI_PolarGraph_T *polarGraph)
{
    if (!polarGraph->showGrid)
    {
        return;
    }

    uint16_t centerX = polarGraph->centerX;
    uint16_t centerY = polarGraph->centerY;
    uint16_t maxRadius = (polarGraph->Width < polarGraph->Height) ? (polarGraph->Width / 2) : (polarGraph->Height / 2);

    /* 绘制角度线（0°, 90°, 180°, 270°） */
    GUI_BasePainter_DrawLine(&s_polarMemDevice, centerX, centerY, centerX + maxRadius, centerY, polarGraph->GridColor);
    GUI_BasePainter_DrawLine(&s_polarMemDevice, centerX, centerY, centerX, centerY + maxRadius, polarGraph->GridColor);
    GUI_BasePainter_DrawLine(&s_polarMemDevice, centerX, centerY, centerX - maxRadius, centerY, polarGraph->GridColor);
    GUI_BasePainter_DrawLine(&s_polarMemDevice, centerX, centerY, centerX, centerY - maxRadius, polarGraph->GridColor);

    /* 绘制45°线 */
    int16_t diagonal = (int16_t)(maxRadius * 0.707f); /* cos(45°) = sin(45°) ≈ 0.707 */
    GUI_BasePainter_DrawLine(&s_polarMemDevice, centerX, centerY, centerX + diagonal, centerY + diagonal, polarGraph->GridColor);
    GUI_BasePainter_DrawLine(&s_polarMemDevice, centerX, centerY, centerX - diagonal, centerY + diagonal, polarGraph->GridColor);
    GUI_BasePainter_DrawLine(&s_polarMemDevice, centerX, centerY, centerX + diagonal, centerY - diagonal, polarGraph->GridColor);
    GUI_BasePainter_DrawLine(&s_polarMemDevice, centerX, centerY, centerX - diagonal, centerY - diagonal, polarGraph->GridColor);
}

/**
 * @brief  绘制距离环
 * @param  polarGraph 极坐标图指针
 */
static void GUI_PolarGraphDrawRings(GUI_PolarGraph_T *polarGraph)
{
    if (!polarGraph->showRings || polarGraph->ringCount == 0)
    {
        return;
    }

    uint16_t centerX = polarGraph->centerX;
    uint16_t centerY = polarGraph->centerY;
    uint16_t maxDisplayRadius = (polarGraph->Width < polarGraph->Height) ? (polarGraph->Width / 2) : (polarGraph->Height / 2);

    for (uint8_t i = 0; i < polarGraph->ringCount; i++)
    {
        GUI_PolarRing_T *ring = &polarGraph->rings[i];
        uint16_t radius = (uint16_t)(ring->radius * maxDisplayRadius);

        if (radius > 0 && radius <= maxDisplayRadius)
        {
            /* 绘制圆环 */
            GUI_BasePainter_SetFillMode(&s_polarMemDevice, GUI_PAINTER_FILL_NONE);
            GUI_BasePainter_DrawCircle(&s_polarMemDevice, centerX, centerY, radius, ring->color);
            GUI_BasePainter_SetFillMode(&s_polarMemDevice, GUI_PAINTER_FILL_COLOR);

            /* 绘制标签 */
            if (polarGraph->showLabels && ring->label != NULL && polarGraph->pFont != NULL)
            {
                char labelText[32];
                snprintf(labelText, sizeof(labelText), "%s", ring->label);

                uint16_t labelX = centerX + radius + 5;
                uint16_t labelY = centerY - 8;

                GUI_BasePainter_ShowString(&s_polarMemDevice, labelX, labelY, labelText, ring->color, polarGraph->BackgroundColor);
            }
        }
    }
}

/**
 * @brief  绘制数据点
 * @param  polarGraph 极坐标图指针
 */
static void GUI_PolarGraphDrawPoints(GUI_PolarGraph_T *polarGraph)
{
    if (polarGraph->pointCount == 0)
    {
        return;
    }

    uint16_t centerX = polarGraph->centerX;
    uint16_t centerY = polarGraph->centerY;

    for (uint16_t i = 0; i < polarGraph->pointCount; i++)
    {
        GUI_PolarPoint_T *point = &polarGraph->points[i];

        /* 将极坐标转换为直角坐标 */
        int16_t x = centerX + (int16_t)(point->scaledRadius * Math_Cos(point->angle));
        int16_t y = centerY - (int16_t)(point->scaledRadius * Math_Sin(point->angle)); /* Y轴向下为正 */

        /* 绘制点 */
        if (x >= 0 && x < polarGraph->Width && y >= 0 && y < polarGraph->Height)
        {
            GUI_BasePainter_DrawCircle(&s_polarMemDevice, x, y, polarGraph->pointSize, point->color);
        }
    }
}

/**
 * @brief  反初始化极坐标图
 * @param  polarGraph 极坐标图指针
 * @note   释放所有动态分配的内存资源
 */
void GUI_PolarGraphDeinit(GUI_PolarGraph_T *polarGraph)
{
    if (polarGraph == NULL) return;

    /* 释放图形缓冲区 */
    if (polarGraph->GraphBuffer)
    {
        Memory_Free(MP_RAM_EX, polarGraph->GraphBuffer);
        polarGraph->GraphBuffer = NULL;
    }

    /* 释放点数组 */
    if (polarGraph->points)
    {
        Memory_Free(MP_RAM_EX, polarGraph->points);
        polarGraph->points = NULL;
    }

    /* 释放环标签 */
    for (uint8_t i = 0; i < polarGraph->ringCount; i++)
    {
        if (polarGraph->rings[i].label)
        {
            Memory_Free(MP_RAM_EX, polarGraph->rings[i].label);
            polarGraph->rings[i].label = NULL;
        }
    }

    polarGraph->InitFlag = 0;
}

/**
 * @brief  初始化极坐标图
 * @param  polarGraph 极坐标图指针
 * @note   分配图形缓冲区和点数组，设置默认参数
 */
void GUI_PolarGraphInit(GUI_PolarGraph_T *polarGraph)
{
    if (polarGraph == NULL) return;

    /* 首先反初始化，防止内存碎片 */
    GUI_PolarGraphDeinit(polarGraph);

    /* 分配图形缓冲区 */
    polarGraph->GraphBuffer = (uint16_t *)Memory_Malloc(MP_RAM_EX, polarGraph->Width * polarGraph->Height * sizeof(uint16_t));
    if (polarGraph->GraphBuffer == NULL) return;

    /* 分配点数组 */
    polarGraph->points = (GUI_PolarPoint_T *)Memory_Malloc(MP_RAM_EX, polarGraph->maxPointCount * sizeof(GUI_PolarPoint_T));
    if (polarGraph->points == NULL)
    {
        Memory_Free(MP_RAM_EX, polarGraph->GraphBuffer);
        polarGraph->GraphBuffer = NULL;
        return;
    }

    polarGraph->pointCount = 0;

    /* 计算中心点 */
    if (polarGraph->centerX == 0 && polarGraph->centerY == 0)
    {
        polarGraph->centerX = polarGraph->Width / 2;
        polarGraph->centerY = polarGraph->Height / 2;
    }

    /* 设置默认值 */
    if (polarGraph->BackgroundColor == 0) polarGraph->BackgroundColor = GUI_BLACK;
    if (polarGraph->AxisColor == 0) polarGraph->AxisColor = GUI_WHITE;
    if (polarGraph->GridColor == 0) polarGraph->GridColor = GUI_GRAY;
    if (polarGraph->ringColor == 0) polarGraph->ringColor = GUI_GREEN;
    if (polarGraph->maxPointColor == 0) polarGraph->maxPointColor = GUI_RED;

    if (polarGraph->pointSize == 0) polarGraph->pointSize = 2;
    if (polarGraph->maxPointSize == 0) polarGraph->maxPointSize = 2;

    /* 默认值 */
    polarGraph->showGrid = 1;
    polarGraph->showRings = 1;
    polarGraph->showAxis = 1;
    polarGraph->showLabels = 1;

    polarGraph->maxRadius = 0.0f;
    polarGraph->scaleFactor = 1.0f;

    /* 如果没有设置字体，使用LCD设备的字体 */
    if (polarGraph->pFont == NULL)
    {
        polarGraph->pFont = g_LCD_DeviceInfo.showFont;
    }

    /* 初始化静态内存设备 */
    s_polarMemDevice.drawPointCallback = polarMemDeviceDrawPoint;
    s_polarMemDevice.fillColorCallback = polarMemDeviceFillColor;
    s_polarMemDevice.Width = polarGraph->Width;
    s_polarMemDevice.Height = polarGraph->Height;
    s_polarMemDevice.tagInfo = polarGraph;
    s_polarMemDevice.showFont = polarGraph->pFont;
    s_polarMemDevice.fillMode = GUI_PAINTER_FILL_COLOR;

    polarGraph->InitFlag = 1;
}

/**
 * @brief  绘制极坐标图
 * @param  polarGraph 极坐标图指针
 * @note   执行完整绘制流程：清空缓冲区→计算缩放→绘制网格→绘制环→绘制点→刷新到屏幕
 */
void GUI_PolarGraphDraw(GUI_PolarGraph_T *polarGraph)
{
    if (polarGraph == NULL || !polarGraph->InitFlag) return;

    /* 更新内存设备参数 */
    s_polarMemDevice.Width = polarGraph->Width;
    s_polarMemDevice.Height = polarGraph->Height;
    s_polarMemDevice.tagInfo = polarGraph;
    s_polarMemDevice.showFont = polarGraph->pFont;

    /* 1. 清空缓冲区 */
    GUI_BasePainter_Fill(&s_polarMemDevice, 0, 0, polarGraph->Width, polarGraph->Height, polarGraph->BackgroundColor);

    /* 2. 计算缩放因子（基于固定基准距离） */
    GUI_PolarGraphCalculateScale(polarGraph);

    /* 3. 绘制网格 */
    GUI_PolarGraphDrawGrid(polarGraph);

    /* 4. 绘制距离环 */
    GUI_PolarGraphDrawRings(polarGraph);

    /* 5. 绘制数据点 */
    GUI_PolarGraphDrawPoints(polarGraph);

    /* 6. 将缓冲区绘制到屏幕 */
    if (!polarGraph->showInBuffer)
    {
        GUI_BasePainter_ShowColorImage(&g_LCD_DeviceInfo, polarGraph->StartX, polarGraph->StartY,
                                       polarGraph->Width, polarGraph->Height, (uint8_t *)polarGraph->GraphBuffer);
    }
}

/**
 * @brief  添加数据点到极坐标图
 * @param  polarGraph 极坐标图指针
 * @param  angle 角度（弧度）
 * @param  radius 半径
 * @param  color 点颜色
 * @note   自动检查点数量限制，角度自动归一化到0-2π
 */
void GUI_PolarGraphAddPoint(GUI_PolarGraph_T *polarGraph, float angle, float radius, uint16_t color)
{
    if (polarGraph == NULL || !polarGraph->InitFlag) return;

    if (polarGraph->pointCount >= polarGraph->maxPointCount)
    {
        return;
    }

    /* 归一化角度到0-360 */
    angle = fmodf(angle, 360);

    /* 添加点 */
    GUI_PolarPoint_T *point = &polarGraph->points[polarGraph->pointCount];
    point->angle = angle;
    point->radius = radius;
    point->color = color;

    polarGraph->pointCount++;
}

/**
 * @brief  设置极坐标环
 * @param  polarGraph 极坐标图指针
 * @param  ringRadii 环半径数组（归一化值，0.0-1.0）
 * @param  ringColors 环颜色数组
 * @param  ringLabels 环标签数组
 * @param  count 环数量
 */
void GUI_PolarGraphSetRings(GUI_PolarGraph_T *polarGraph, const float *ringRadii, const uint16_t *ringColors, const char **ringLabels, uint8_t count)
{
    if (polarGraph == NULL || count == 0 || count > GUI_POLAR_MAX_RING_COUNT)
    {
        return;
    }

    /* 释放旧的环标签 */
    for (uint8_t i = 0; i < polarGraph->ringCount; i++)
    {
        if (polarGraph->rings[i].label)
        {
            Memory_Free(MP_RAM_EX, polarGraph->rings[i].label);
            polarGraph->rings[i].label = NULL;
        }
    }

    /* 设置新的环 */
    polarGraph->ringCount = count;

    for (uint8_t i = 0; i < count; i++)
    {
        GUI_PolarRing_T *ring = &polarGraph->rings[i];

        ring->radius = ringRadii[i];
        ring->color = (ringColors != NULL) ? ringColors[i] : polarGraph->ringColor;

        if (ringLabels != NULL && ringLabels[i] != NULL)
        {
            size_t labelLen = String_GetLength(ringLabels[i], s_polarMemDevice.Width / s_polarMemDevice.showFont->asciiFontWidth);
            ring->label = (char *)Memory_Malloc(MP_RAM_EX, labelLen + 1);
            if (ring->label)
            {
                strcpy(ring->label, ringLabels[i]);
            }
        }
        else
        {
            char defaultLabel[16];
            snprintf(defaultLabel, sizeof(defaultLabel), "%.1f", ringRadii[i]);

            size_t labelLen = String_GetLength(defaultLabel, s_polarMemDevice.Width / s_polarMemDevice.showFont->asciiFontWidth);
            ring->label = (char *)Memory_Malloc(MP_RAM_EX, labelLen + 1);
            if (ring->label)
            {
                strcpy(ring->label, defaultLabel);
            }
        }
    }
}

/**
 * @brief  清空所有数据点
 * @param  polarGraph 极坐标图指针
 */
void GUI_PolarGraphClearPoints(GUI_PolarGraph_T *polarGraph)
{
    if (polarGraph == NULL) return;

    polarGraph->pointCount = 0;
    polarGraph->maxRadius = 0.0f;
    polarGraph->scaleFactor = 1.0f;
}

/**
 * @brief  设置极坐标图中心点
 * @param  polarGraph 极坐标图指针
 * @param  centerX 中心点X坐标
 * @param  centerY 中心点Y坐标
 */
void GUI_PolarGraphSetCenter(GUI_PolarGraph_T *polarGraph, uint16_t centerX, uint16_t centerY)
{
    if (polarGraph == NULL) return;

    polarGraph->centerX = centerX;
    polarGraph->centerY = centerY;
}

/**
 * @brief  设置固定基准距离
 * @param  polarGraph 极坐标图指针
 * @param  fixedBaseRadius 固定基准距离
 */
void GUI_PolarGraphSetFixedBaseRadius(GUI_PolarGraph_T *polarGraph, float fixedBaseRadius)
{
    if (polarGraph == NULL) return;

    polarGraph->fixedBaseRadius = fixedBaseRadius;
}