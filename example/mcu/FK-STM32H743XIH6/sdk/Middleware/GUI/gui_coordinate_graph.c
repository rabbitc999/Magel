#include "gui.h"

#include <stdlib.h>
#include <math.h>

/* 静态全局内存设备 */
static GUI_DeviceInfo_T s_MemDevice = { 0 };

/* 静态函数声明 */
static void GUI_GraphClearBuffer(GUI_CoordinateGraph_T *graph);
static uint16_t GUI_CoordinateDrawLegend(GUI_CoordinateGraph_T *graph);
static void GUI_CoordinateDrawGrid(GUI_CoordinateGraph_T *graph, uint16_t graphAreaY, uint16_t graphAreaHeight);
static void GUI_CoordinateDrawCurves(GUI_CoordinateGraph_T *graph, uint16_t graphAreaY, uint16_t graphAreaHeight);

/* 外部LCD设备声明 */
extern GUI_DeviceInfo_T g_LCD_DeviceInfo;

/**
 * @brief 内存设备绘制点回调函数
 * @param deviceInfo 设备信息结构体指针
 * @param x 目标点X坐标
 * @param y 目标点Y坐标
 * @param color 绘制颜色值
 * @note 将点绘制到内存图形缓冲区，自动进行边界检查
 */
static void memDeviceDrawPoint(GUI_DeviceInfo_T *deviceInfo, uint16_t x, uint16_t y, uint32_t color)
{
    GUI_CoordinateGraph_T *graph = (GUI_CoordinateGraph_T *)deviceInfo->tagInfo;
    if (x < graph->Width && y < graph->Height)
    {
        graph->GraphBuffer[y * graph->Width + x] = g_LCD_DeviceInfo.swapFlag ? (uint16_t)GUI_SwapBytes(color) : (uint16_t)(color);
    }
}

/**
 * @brief 内存设备区域填充回调函数
 * @param deviceInfo 设备信息结构体指针
 * @param x 起始X坐标
 * @param y 起始Y坐标
 * @param Width 填充区域宽度
 * @param Height 填充区域高度
 * @param color 填充颜色值
 * @note 自动裁剪填充区域至缓冲区边界内
 */
static void memDeviceFillColor(GUI_DeviceInfo_T *deviceInfo, uint16_t x, uint16_t y, uint16_t Width, uint16_t Height, uint32_t color)
{
    GUI_CoordinateGraph_T *graph = (GUI_CoordinateGraph_T *)deviceInfo->tagInfo;
    uint16_t i, j;

    /* 限制在缓冲区范围内 */
    if (x >= graph->Width || y >= graph->Height) return;
    if (x + Width > graph->Width) Width = graph->Width - x;
    if (y + Height > graph->Height) Height = graph->Height - y;

    for (j = y; j < y + Height; j++)
    {
        for (i = x; i < x + Width; i++)
        {
            graph->GraphBuffer[j * graph->Width + i] = g_LCD_DeviceInfo.swapFlag ? (uint16_t)GUI_SwapBytes(color) : (uint16_t)(color);
        }
    }
}

/**
 * @brief 清空图形缓冲区
 * @param graph 图形对象指针
 */
static void GUI_GraphClearBuffer(GUI_CoordinateGraph_T *graph)
{
    GUI_BasePainter_Fill(&s_MemDevice, 0, 0, graph->Width, graph->Height, graph->BlackColor);
}

/**
 * @brief 绘制图例
 * @param graph 图形对象指针
 * @return 图例区域高度
 */
static uint16_t GUI_CoordinateDrawLegend(GUI_CoordinateGraph_T *graph)
{
    if (!graph->ShowLegend || graph->pFont == NULL)
    {
        return 0;
    }

    uint16_t currentX = 0;
    uint16_t currentY = 2;
    uint8_t showFlag = 0;

    for (uint16_t ch = 0; ch < graph->Channels && ch < graph->Width; ch++)
    {
        GUI_CoordinateGraphBuffer_T *curve = &graph->Curves[ch];

        if (!curve->IsShowLabel) { continue; }

        /* 如果有名称，显示名称 */
        if (curve->Name != NULL)
        {
            /* 使用曲线的颜色绘制文字 */
            GUI_BasePainter_ShowString(&s_MemDevice, currentX, currentY, curve->Name, curve->PaintColor, graph->BlackColor);
            currentX += String_GetLength(curve->Name, s_MemDevice.Width / s_MemDevice.showFont->asciiFontWidth) * graph->pFont->asciiFontWidth;
        }
        else
        {
            /* 如果没有名称，显示通道号 */
            char channelName[16];
            snprintf(channelName, sizeof(channelName), "Ch%d", ch + 1);

            /* 使用曲线的颜色绘制文字 */
            GUI_BasePainter_ShowString(&s_MemDevice, currentX, currentY, channelName, curve->PaintColor, graph->BlackColor);
            currentX += String_GetLength(channelName, s_MemDevice.Width / s_MemDevice.showFont->asciiFontWidth) * graph->pFont->asciiFontWidth;
        }

        showFlag = 1;
    }

    return (showFlag == 1) ? (graph->LegendHeight + 4) : 0;
}

/**
 * @brief 绘制网格
 * @param graph 图形对象指针
 * @param graphAreaY 图形区域起始Y坐标
 * @param graphAreaHeight 图形区域高度
 */
static void GUI_CoordinateDrawGrid(GUI_CoordinateGraph_T *graph, uint16_t graphAreaY, uint16_t graphAreaHeight)
{
    /* 垂直网格线 */
    for (uint16_t x = 0; x <= graph->Width; x += graph->GridXInterval)
    {
        GUI_BasePainter_DrawLine(&s_MemDevice, x, graphAreaY, x, graph->Height - 1, graph->GridColor);
    }

    /* 水平网格线 */
    for (uint16_t y = 0; y <= graphAreaHeight; y += graph->GridYInterval)
    {
        GUI_BasePainter_DrawLine(&s_MemDevice, 0, graphAreaY + y, graph->Width - 1, graphAreaY + y, graph->GridColor);
    }
}

/**
 * @brief 绘制所有曲线
 * @param graph 图形对象指针
 * @param graphAreaY 图形区域起始Y坐标
 * @param graphAreaHeight 图形区域高度
 */
static void GUI_CoordinateDrawCurves(GUI_CoordinateGraph_T *graph, uint16_t graphAreaY, uint16_t graphAreaHeight)
{
    for (uint16_t ch = 0; ch < graph->Channels; ch++)
    {
        GUI_CoordinateGraphBuffer_T *curve = &graph->Curves[ch];

        uint32_t dataCount = FIFO_GetLength(&curve->FIFO);
        if (dataCount < 2) continue;

        /* 分配临时数组存储数据 */
        float *data = (float *)Memory_Malloc(MP_RAM_EX, dataCount * sizeof(float));
        if (data == NULL) continue;

        /* 使用FIFO_get_buffer读取所有数据（不移除） */
        uint32_t readCount = FIFO_GetBuffer(&curve->FIFO, data, 0, dataCount);

        if (readCount < 2)
        {
            Memory_Free(MP_RAM_EX, data);
            continue;
        }

        /* 计算实际绘制的数据点数（考虑间隔） */
        uint32_t visiblePoints = (readCount + graph->PointInterval - 1) / graph->PointInterval;
        if (visiblePoints < 2)
        {
            Memory_Free(MP_RAM_EX, data);
            continue;
        }

        /* 计算X步长（考虑间隔） */
        float xStep = graph->PointInterval;

        /* 绘制曲线，按间隔采样 */
        uint16_t lastX = 0, lastY = 0;
        uint8_t firstPoint = 1;

        for (uint32_t i = 0; i < readCount; i++)
        {
            /* 计算Y坐标（映射到图像区域） */
            float value = data[i];

            /* 限制在曲线范围内 */
            if (value < curve->Min) value = curve->Min;
            if (value > curve->Max) value = curve->Max;

            /* 映射到屏幕坐标（考虑图像区域的Y偏移） */
            uint16_t y = graphAreaY + (graphAreaHeight - 1) - (uint16_t)((value - curve->Min) / (curve->Max - curve->Min) * (graphAreaHeight - 1));

            /* 计算X坐标 */
            uint16_t x = (uint16_t)((i / graph->PointInterval) * xStep);

            /* 如果是第一个点，只记录坐标 */
            if (firstPoint)
            {
                lastX = x;
                lastY = y;
                firstPoint = 0;
            }
            else
            {
                /* 绘制从前一点到当前点的线段 */
                GUI_BasePainter_DrawLine(&s_MemDevice, lastX, lastY, x, y, curve->PaintColor);
                GUI_BasePainter_DrawLine(&s_MemDevice, lastX, lastY + 1, x, y + 1, curve->PaintColor);
                GUI_BasePainter_DrawLine(&s_MemDevice, lastX, lastY - 1, x, y - 1, curve->PaintColor);
                GUI_BasePainter_DrawLine(&s_MemDevice, lastX + 1, lastY, x + 1, y, curve->PaintColor);
                GUI_BasePainter_DrawLine(&s_MemDevice, lastX, lastY, x - 1, y, curve->PaintColor);
                lastX = x;
                lastY = y;
            }
        }

        /* 释放临时数组 */
        Memory_Free(MP_RAM_EX, data);
    }
}

/**
 * @brief 反初始化图形对象
 * @param graph 图形对象指针
 * @note 释放所有动态分配的内存资源
 */
void GUI_CoordinateGraphDeinit(GUI_CoordinateGraph_T *graph)
{
    if (graph == NULL) return;

    /* 释放图形缓冲区 */
    if (graph->GraphBuffer)
    {
        Memory_Free(MP_RAM_EX, graph->GraphBuffer);
        graph->GraphBuffer = NULL;
    }

    /* 释放每条曲线的FIFO缓冲区 */
    for (uint16_t i = 0; i < graph->Channels; i++)
    {
        if (graph->Curves[i].FIFO.buffer)
        {
            Memory_Free(MP_RAM_EX, (void *)graph->Curves[i].FIFO.buffer);
            graph->Curves[i].FIFO.buffer = NULL;
        }
    }

    graph->InitFlag = 0;
}

/**
 * @brief 初始化图形对象
 * @param graph 图形对象指针
 * @note 分配图形缓冲区和曲线FIFO缓冲区，设置默认参数
 */
void GUI_CoordinateGraphInit(GUI_CoordinateGraph_T *graph)
{
    if (graph == NULL) return;

    /* 首先反初始化，目的是防止内存碎片 */
    GUI_CoordinateGraphDeinit(graph);

    /* 分配图形缓冲区 */
    graph->GraphBuffer = (uint16_t *)Memory_Malloc(MP_RAM_EX, graph->Width * graph->Height * sizeof(uint16_t));
    if (graph->GraphBuffer == NULL) return;

    /* 初始化每条曲线的FIFO */
    for (uint16_t i = 0; i < graph->Channels; i++)
    {
        /* 分配FIFO缓冲区，每个元素是float大小，多分配1个字节解决循环队列问题 */
        uint8_t *FIFOBuffer = (uint8_t *)Memory_Malloc(MP_RAM_EX, (graph->Width + 5) * sizeof(float) + 1);
        if (FIFOBuffer == NULL)
        {
            /* 分配失败，清理已分配的内存 */
            GUI_CoordinateGraphDeinit(graph);
            return;
        }

        /* 初始化FIFO */
        FIFO_Init(&graph->Curves[i].FIFO, FIFOBuffer, sizeof(float), graph->Width + 5); /* 元素数量 */
    }

    /* 设置默认值 */
    if (graph->GridXInterval == 0) graph->GridXInterval = 20;
    if (graph->GridYInterval == 0) graph->GridYInterval = 20;
    if (graph->AxisColor == 0) graph->AxisColor = GUI_WHITE;
    if (graph->GridColor == 0) graph->GridColor = GUI_GRAY;
    if (graph->BlackColor == 0) graph->BlackColor = GUI_BLACK;
    if (graph->PointInterval == 0) graph->PointInterval = 2; /* 默认每2个像素绘制一个点 */
    if (graph->LegendHeight == 0) graph->LegendHeight = 16;  /* 默认图例高度16像素 */
    graph->ShowLegend = 1;                                   /* 默认显示图例 */

    /* 如果没有设置字体，使用LCD设备的字体 */
    if (graph->pFont == NULL)
    {
        graph->pFont = g_LCD_DeviceInfo.showFont;
    }

    /* 初始化静态内存设备 */
    s_MemDevice.drawPointCallback = memDeviceDrawPoint;
    s_MemDevice.fillColorCallback = memDeviceFillColor;
    s_MemDevice.Width = graph->Width;
    s_MemDevice.Height = graph->Height;
    s_MemDevice.tagInfo = graph;
    s_MemDevice.showFont = graph->pFont; /* 使用图表指定的字体 */
    s_MemDevice.fillMode = GUI_PAINTER_FILL_COLOR;

    graph->InitFlag = 1;
}

/**
 * @brief 绘制图形到屏幕
 * @param graph 图形对象指针
 * @note 执行完整绘制流程：清空缓冲区→绘制图例→绘制网格→绘制坐标轴→绘制曲线→刷新到屏幕
 */
void GUI_CoordinateGraphDraw(GUI_CoordinateGraph_T *graph)
{
    if (graph == NULL || !graph->InitFlag) return;

    /* 更新内存设备参数 */
    s_MemDevice.Width = graph->Width;
    s_MemDevice.Height = graph->Height;
    s_MemDevice.tagInfo = graph;
    s_MemDevice.showFont = graph->pFont; /* 确保使用正确的字体 */

    /* 1. 清空缓冲区 */
    GUI_GraphClearBuffer(graph);

    /* 2. 绘制图例并获取图例区域高度 */
    uint16_t legendAreaHeight = GUI_CoordinateDrawLegend(graph);

    /* 3. 计算图形区域 */
    uint16_t graphAreaHeight = graph->Height - legendAreaHeight;
    uint16_t graphAreaY = legendAreaHeight;

    /* 4. 绘制网格 */
    GUI_CoordinateDrawGrid(graph, graphAreaY, graphAreaHeight);

    /* 5. 绘制曲线 */
    GUI_CoordinateDrawCurves(graph, graphAreaY, graphAreaHeight);

    /* 6. 将缓冲区绘制到屏幕 */
    GUI_BasePainter_ShowColorImage(&g_LCD_DeviceInfo, graph->StartX, graph->StartY, graph->Width, graph->Height, (uint8_t *)graph->GraphBuffer);
}

/**
 * @brief 向所有曲线添加新数据点
 * @param graph 图形对象指针
 * @param ... 可变参数列表，每个曲线对应一个float值
 * @note 按曲线顺序传入数据值，自动写入各曲线FIFO缓冲区
 */
void GUI_CoordinateGraphAddData(GUI_CoordinateGraph_T *graph, ...)
{
    va_list args;
    va_start(args, graph);

    for (uint16_t i = 0; i < graph->Channels; i++)
    {
        FIFO_T *pFifo = &graph->Curves[i].FIFO;
        float temp = va_arg(args, double);
        FIFO_WriteOrMove(pFifo, &temp, 1);
    }

    va_end(args);
}
