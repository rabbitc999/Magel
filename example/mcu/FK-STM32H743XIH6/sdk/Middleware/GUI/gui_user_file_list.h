#ifndef __GUI_USER_FILE_LIST_H
#define __GUI_USER_FILE_LIST_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "gui_menu.h"
#include "file.h"

/* 文件选择器状态枚举 */
typedef enum __FileSelectorState_T
{
    FILE_SELECTOR_IDLE,      /* 空闲状态 */
    FILE_SELECTOR_SELECTING, /* 选择文件中 */
    FILE_SELECTOR_ERROR      /* 错误状态 */
} FileSelectorState_T;

/* 文件选择器配置结构体 */
typedef struct __FileSelectorConfig_T
{
    char directoryPath[64];   /* 目录路径 */
    char filePattern[16];     /* 文件模式(如*.bin, *.lua) */
    uint16_t maxDisplayItems; /* 最大显示项数 */
    uint16_t itemHeight;      /* 每项高度 */
    uint16_t StartY;          /* 开始Y坐标 */
    uint16_t titleHeight;     /* 标题高度 */
    uint16_t hintHeight;      /* 提示高度 */
    const char *showTitle;    /* 显示标题 */
} FileSelectorConfig_T;

/* 文件选择器实例结构体 */
typedef struct __FileSelector_T
{
    FileSelectorConfig_T config; /* 配置 */
    FatFsFileList_T fileList;    /* 文件列表 */
    uint16_t currentIndex;       /* 当前索引 */
    char currentFilePath[128];   /* 当前文件路径 */
    char statusMessage[128];     /* 状态消息 */
    FileSelectorState_T state;   /* 状态 */
    uint8_t InitFlag;            /* 初始化标志 */
    uint16_t lastStartIndex;     /* 上次开始索引 */
} FileSelector_T;

/* 文件选择器回调函数类型 */
typedef void (*FileSelectionCallback)(FileSelector_T *selector, const char *filePath);

/* 文件选择器函数声明 */
void FileSelector_Init(FileSelector_T *selector, const FileSelectorConfig_T *config);
void FileSelector_Deinit(FileSelector_T *selector);
uint8_t FileSelector_LoadFileList(FileSelector_T *selector);
void FileSelector_SelectFile(FileSelector_T *selector, uint16_t index);
void FileSelector_DrawFileList(FileSelector_T *selector);
void FileSelector_HandleKeyInput(FileSelector_T *selector, FileSelectionCallback callback);
const char *FileSelector_GetCurrentFilePath(FileSelector_T *selector);
const char *FileSelector_GetStatusMessage(FileSelector_T *selector);
FileSelectorState_T FileSelector_GetState(FileSelector_T *selector);

#ifdef __cplusplus
}
#endif

#endif
