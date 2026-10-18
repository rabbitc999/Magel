#ifndef __GUI_USER_FILE_BROWSER_H
#define __GUI_USER_FILE_BROWSER_H

#include <stdint.h>
#include "gui.h"
#include "file.h"
#include "ff.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 文件类型枚举 */
typedef enum __FileType_T
{
    FILE_TYPE_UNKNOWN,   /* 未知类型 */
    FILE_TYPE_DIRECTORY, /* 目录 */
    FILE_TYPE_TEXT,      /* 文本文件 */
    FILE_TYPE_BINARY,    /* 二进制文件 */
} FileType_T;

/* 文件列表状态枚举 */
typedef enum __FileListState_T
{
    FILE_LIST_IDLE,     /* 空闲状态 */
    FILE_LIST_BROWSING, /* 浏览状态 */
    FILE_LIST_ERROR     /* 错误状态 */
} FileListState_T;

/* 文件列表配置结构体 */
typedef struct __FileListConfig_T
{
    char rootPath[64];        /* 根目录路径 */
    uint16_t maxDisplayItems; /* 最大显示项数 */
    uint16_t itemHeight;      /* 每项高度 */
    uint16_t StartY;          /* 开始Y坐标 */
    uint16_t titleHeight;     /* 标题高度 */
    uint16_t statusBarHeight; /* 状态栏高度 */
    const char *showTitle;    /* 显示标题 */
} FileListConfig_T;

/* 文件列表实例结构体 */
typedef struct __FileList_T
{
    FileListConfig_T config;  /* 配置 */
    FatFsFileList_T fileList; /* 使用ff_file的文件列表 */
    uint16_t currentIndex;    /* 当前索引 */
    uint16_t scrollOffset;    /* 滚动偏移 */
    char currentPath[256];    /* 当前路径 */
    char statusMessage[128];  /* 状态消息 */
    FileListState_T state;    /* 状态 */
    uint8_t InitFlag;         /* 初始化标志 */
    uint8_t deep;             /* 目录深度 */

    /* 绘制控制标志 */
    uint8_t needRedrawStatic;    /* 静态内容重绘标志 */
    uint8_t needRedrawDynamic;   /* 动态内容重绘标志 */
    uint8_t needRedrawScrollBar; /* 滚动条重绘标志 */
    FileListState_T lastState;   /* 上次状态 */
    uint16_t lastScrollOffset;   /* 上次滚动偏移 */
} FileList_T;

/* 文件列表函数声明 */
void FileList_Init(FileList_T *fileList, const FileListConfig_T *config);
void FileList_Deinit(FileList_T *fileList);
uint8_t FileList_LoadDirectory(FileList_T *fileList, const char *path);
void FileList_HandleKeyInput(FileList_T *fileList);
void FileList_Draw(FileList_T *fileList);
void FileList_EnterDirectory(FileList_T *fileList, uint16_t index, uint8_t deep);
void FileList_BackToParent(FileList_T *fileList);
FileType_T FileList_GetFileType(const char *fileName);
uint16_t FileBrowser_Function(GUI_FuncItem_T *sender, GUI_MenuParam_T *param);

#ifdef __cplusplus
}
#endif

#endif
