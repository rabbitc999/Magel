#ifndef __GUI_USER_MAIN_DEFINE_H
#define __GUI_USER_MAIN_DEFINE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "gui.h"

const char *s_LidarDevice[] = {
    "N/A ",
    "X2  ",
    "N10 ",
    "N10P",
};

/* ========== 图形尺寸 ========== */
#define GRAPH_AREA_WIDTH  MENU_AREA_WIDTH
#define GRAPH_AREA_HEIGHT (MENU_AREA_HEIGHT - 150)

/* 绘制图像定义 */
GUI_CoordinateGraph_T g_CarGraph = {
    .Width = GRAPH_AREA_WIDTH,
    .Height = GRAPH_AREA_HEIGHT,
    .StartX = 0,
    .StartY = 0,
    .Channels = 2,
    .PointInterval = 1,
    .LegendHeight = 26,
    .ShowLegend = 1,
    .GridXInterval = 30,
    .GridYInterval = 30,
    .AxisColor = GUI_WHITE,
    .GridColor = GUI_GRAY,
    .BlackColor = GUI_BLACK,
    .pFont = &g_GUI_FontGB2312_16,
    .Curves = {
        { .Name = "前进速度", .PaintColor = GUI_RED, .Min = -0.5f, .Max = 0.5f, .IsShowLabel = 1 },
        { .Name = "旋转速度", .PaintColor = GUI_MAGENTA, .Min = -200.0f, .Max = 200.0f, .IsShowLabel = 1 },
    }
};

GUI_CoordinateGraph_T g_MotorMapperGraph = {
    .Width = GRAPH_AREA_WIDTH,
    .Height = GRAPH_AREA_HEIGHT,
    .StartX = 0,
    .StartY = 0,
    .Channels = 8,
    .PointInterval = 1,
    .LegendHeight = 26,
    .ShowLegend = 1,
    .GridXInterval = 30,
    .GridYInterval = 30,
    .AxisColor = GUI_WHITE,
    .GridColor = GUI_GRAY,
    .BlackColor = GUI_BLACK,
    .pFont = &g_GUI_FontGB2312_16,
    .Curves = {
        { .Name = "M1O", .PaintColor = GUI_GREEN, .Min = -1000.0f, .Max = 1000.0f, .IsShowLabel = 1 },
        { .Name = "M1E", .PaintColor = GUI_YELLOW, .Min = -50.0f, .Max = 50.0f, .IsShowLabel = 1 },
        { .Name = "M2O", .PaintColor = GUI_BLUE, .Min = -1000.0f, .Max = 1000.0f, .IsShowLabel = 1 },
        { .Name = "M2E", .PaintColor = GUI_RED, .Min = -50.0f, .Max = 50.0f, .IsShowLabel = 1 },
        { .Name = "M3O", .PaintColor = GUI_CYAN, .Min = -1000.0f, .Max = 1000.0f, .IsShowLabel = 1 },
        { .Name = "M3E", .PaintColor = GUI_MAGENTA, .Min = -50.0f, .Max = 50.0f, .IsShowLabel = 1 },
        { .Name = "M4O", .PaintColor = GUI_BROWN, .Min = -1000.0f, .Max = 1000.0f, .IsShowLabel = 1 },
        { .Name = "M4E", .PaintColor = GUI_GRAY_BLUE, .Min = -50.0f, .Max = 50.0f, .IsShowLabel = 1 },
    }
};

GUI_CoordinateGraph_T g_ImuGraph = {
    .Width = GRAPH_AREA_WIDTH,
    .Height = GRAPH_AREA_HEIGHT,
    .StartX = 0,
    .StartY = 0,
    .Channels = 3,
    .PointInterval = 1,
    .LegendHeight = 26,
    .ShowLegend = 1,
    .GridXInterval = 30,
    .GridYInterval = 30,
    .AxisColor = GUI_WHITE,
    .GridColor = GUI_GRAY,
    .BlackColor = GUI_BLACK,
    .pFont = &g_GUI_FontGB2312_16,
    .Curves = {
        { .Name = "俯仰角", .PaintColor = GUI_GREEN, .Min = -90.0f, .Max = 90.0f, .IsShowLabel = 1 },
        { .Name = "滚转角", .PaintColor = GUI_YELLOW, .Min = -180.0f, .Max = 180.0f, .IsShowLabel = 1 },
        { .Name = "偏航角", .PaintColor = GUI_BLUE, .Min = -180.0f, .Max = 180.0f, .IsShowLabel = 1 },
    }
};

GUI_CoordinateGraph_T g_DetailGraph = {
    .Width = GRAPH_AREA_WIDTH,
    .Height = GRAPH_AREA_HEIGHT,
    .StartX = 0,
    .StartY = 0,
    .Channels = 2,
    .PointInterval = 1,
    .LegendHeight = 26,
    .ShowLegend = 1,
    .GridXInterval = 30,
    .GridYInterval = 30,
    .AxisColor = GUI_WHITE,
    .GridColor = GUI_GRAY,
    .BlackColor = GUI_BLACK,
    .pFont = &g_GUI_FontGB2312_16,
    .Curves = {
        { .Name = "测试图像1", .PaintColor = GUI_GREEN, .Min = -1.0f, .Max = 1.0f, .IsShowLabel = 1 },
        { .Name = "测试图像2", .PaintColor = GUI_YELLOW, .Min = -1.0f, .Max = 1.0f, .IsShowLabel = 1 },
    }
};

GUI_CoordinateGraph_T g_SlipGraph = {
    .Width = GRAPH_AREA_WIDTH,
    .Height = GRAPH_AREA_HEIGHT,
    .StartX = 0,
    .StartY = 0,
    .Channels = 3,
    .PointInterval = 1,
    .LegendHeight = 26,
    .ShowLegend = 1,
    .GridXInterval = 30,
    .GridYInterval = 30,
    .AxisColor = GUI_WHITE,
    .GridColor = GUI_GRAY,
    .BlackColor = GUI_BLACK,
    .pFont = &g_GUI_FontGB2312_16,
    .Curves = {
        { .Name = "轮子α", .PaintColor = GUI_GREEN, .Min = 0.0f, .Max = 1.0f, .IsShowLabel = 1 },
        { .Name = "轮间β", .PaintColor = GUI_BLUE, .Min = 0.0f, .Max = 1.0f, .IsShowLabel = 1 },
        { .Name = "整体γ", .PaintColor = GUI_YELLOW, .Min = 0.0f, .Max = 1.0f, .IsShowLabel = 1 },
    }
};

GUI_PolarGraph_T s_polarGraph = {
    .Width = GRAPH_AREA_WIDTH,
    .Height = GRAPH_AREA_HEIGHT,
    .StartX = 0,
    .StartY = 0,
    .fixedBaseRadius = 500,
    .maxPointCount = PACKET_LIDAR_MAX_SAMPLES + 1
};

/* GUI用户定义 */
struct __GUI_User_T g_GUI_User = {
    .device = {
        .menuPainter = {
            .charHeight = CHAR_HEIGHT,
            .charWidth = CHAR_WIDTH,
            .lineInterval = LINE_SPACING,
            .Height = GUI_SCREEN_HEIGHT,
            .Width = MENU_AREA_WIDTH,
            .StartX = 0,
            .StartY = 0,
            .scrollBarWidth = SCROLL_BAR_WIDTH,
        },
    }
};

#ifdef __cplusplus
}
#endif

#endif