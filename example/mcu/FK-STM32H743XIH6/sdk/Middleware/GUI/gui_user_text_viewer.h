#ifndef __GUI_USER_TEXT_VIEWER_H
#define __GUI_USER_TEXT_VIEWER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "gui.h"
#include "file.h"
#include "ff.h"

/* 文本显示器状态枚举 */
typedef enum __TextViewerState_T
{
    TEXT_VIEWER_IDLE,    /* 空闲状态 */
    TEXT_VIEWER_VIEWING, /* 查看状态 */
    TEXT_VIEWER_ERROR    /* 错误状态 */
} TextViewerState_T;

/* 文本显示器配置结构体 */
typedef struct __TextViewerConfig_T
{
    uint16_t StartY;          /* 开始Y坐标 */
    uint16_t titleHeight;     /* 标题高度 */
    uint16_t statusBarHeight; /* 状态栏高度 */
    const char *showTitle;    /* 显示标题 */
} TextViewerConfig_T;

/* 文本显示器实例结构体 */
typedef struct __TextViewer_T
{
    TextViewerConfig_T config;     /* 配置 */
    uint8_t *previewBuffer;        /* 预览缓冲区 */
    uint32_t previewSize;          /* 预览大小 */
    uint16_t previewScrollY;       /* 预览滚动位置 */
    char fileName[FF_MAX_LFN + 1]; /* 当前文件名 */
    char statusMessage[128];       /* 状态消息 */
    TextViewerState_T state;       /* 状态 */
    uint8_t InitFlag;              /* 初始化标志 */

    /* 绘制控制标志 */
    uint8_t needRedrawStatic;    /* 静态内容重绘标志 */
    uint8_t needRedrawDynamic;   /* 动态内容重绘标志 */
    uint8_t needRedrawScrollBar; /* 滚动条重绘标志 */
    TextViewerState_T lastState; /* 上次状态 */
    uint16_t lastPreviewScrollY; /* 上次预览滚动位置 */
} TextViewer_T;

/* 文本显示器函数声明 */
void TextViewer_Init(TextViewer_T *viewer, const TextViewerConfig_T *config);
void TextViewer_Deinit(TextViewer_T *viewer);
uint8_t TextViewer_LoadFile(TextViewer_T *viewer, const char *filePath);
void TextViewer_HandleKeyInput(TextViewer_T *viewer);
void TextViewer_Draw(TextViewer_T *viewer);
void TextViewer_DrawTextPreview(TextViewer_T *viewer);
uint16_t TextViewer_Function(GUI_FuncItem_T *sender, GUI_MenuParam_T *param);
TextViewer_T *TextViewer_GetInstance(void);

#ifdef __cplusplus
}
#endif

#endif
