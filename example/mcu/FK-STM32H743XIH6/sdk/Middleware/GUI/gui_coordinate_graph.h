#ifndef __GUI_GRAPH
#define __GUI_GRAPH

#include <stdint.h>
#include <string.h>
#include "bsp_FIFO.h"
#include "gui_painter_base.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct __GUI_CoordinateGraphBuffer_T
{
    char *Name;          /* 曲线名称 */
    uint16_t PaintColor; /* 曲线颜色 */
    uint8_t IsShowLabel; /* 是否显示标签 */
    float Max;           /* 曲线限制最大值 */
    float Min;           /* 曲线限制最小值 */
    FIFO_T FIFO;         /* FIFO缓冲区 */
} GUI_CoordinateGraphBuffer_T;

typedef struct __GUI_CoordinateGraph_T
{
    uint16_t *GraphBuffer;                /* 图形缓冲区 */
    uint16_t Channels;                    /* 曲线数量 */
    uint16_t InitFlag;                    /* 初始化标志 */
    uint16_t Width;                       /* 图表宽度 */
    uint16_t Height;                      /* 图表高度 */
    uint16_t StartX;                      /* 图表起始X坐标 */
    uint16_t StartY;                      /* 图表起始Y坐标 */
    uint16_t BlackColor;                  /* 背景颜色 */
    uint16_t AxisColor;                   /* 坐标轴颜色 */
    uint16_t GridColor;                   /* 网格颜色 */
    uint16_t GridXInterval;               /* 水平网格间隔 */
    uint16_t GridYInterval;               /* 垂直网格间隔 */
    uint16_t PointInterval;               /* 数据点显示间隔（像素） */
    uint16_t LegendHeight;                /* 图例区域高度 */
    uint8_t ShowLegend;                   /* 是否显示图例 */
    GUI_FontInfo_T *pFont;                /* 字体对象指针 */
    GUI_CoordinateGraphBuffer_T Curves[]; /* 曲线数据 */
} GUI_CoordinateGraph_T;

void GUI_CoordinateGraphDeinit(GUI_CoordinateGraph_T *Graph);
void GUI_CoordinateGraphInit(GUI_CoordinateGraph_T *Graph);
void GUI_CoordinateGraphDraw(GUI_CoordinateGraph_T *Graph);
void GUI_CoordinateGraphAddData(GUI_CoordinateGraph_T *Graph, ...);

#ifdef __cplusplus
}
#endif

#endif
