#include "gui.h"
#include "algorithm.h"
#include "file.h"

#include "gui_user_file_browser.h"
#include "gui_user_text_viewer.h"

#include <string.h>
#include <stdlib.h>

/* 静态全局变量 */
static FileList_T s_FileList; /* 文件列表实例 */
static GUI_Device_T *currentDevice;

/* 外部函数 */
uint16_t TextViewer_Function(GUI_FuncItem_T *sender, GUI_MenuParam_T *param);

/**
 * @brief 获取文件类型
 * @param fileName 文件名
 * @return 文件类型
 */
FileType_T FileList_GetFileType(const char *fileName)
{
    if (fileName == NULL)
    {
        return FILE_TYPE_UNKNOWN;
    }

    /* 获取文件扩展名 */
    const char *ext = strrchr(fileName, '.');
    if (ext == NULL)
    {
        return FILE_TYPE_BINARY;
    }

    ext++; /* 跳过'.' */

    /* 判断文件类型 */
    if (strcasecmp(ext, "txt") == 0 || strcasecmp(ext, "log") == 0 || strcasecmp(ext, "cfg") == 0 || strcasecmp(ext, "lua") == 0)
    {
        return FILE_TYPE_TEXT;
    }
    else
    {
        return FILE_TYPE_BINARY;
    }
}

/**
 * @brief 截断文件名以适应显示宽度
 * @param fileName 原始文件名
 * @param buffer 输出缓冲区
 * @param bufferSize 缓冲区大小
 * @param maxDisplayWidth 最大显示宽度（字符数）
 */
static void FileList_TruncateFileName(const char *fileName, char *buffer, uint16_t bufferSize, uint16_t maxDisplayWidth)
{
    if (fileName == NULL || buffer == NULL || bufferSize == 0)
    {
        return;
    }

    uint16_t fileNameLen = String_GetLength(fileName, FATFS_PATH_LEN_MAX);
    uint16_t displayWidth = 0; /* 显示宽度（汉字算2，ASCII算1） */
    uint16_t byteIndex = 0;    /* 字节索引 */

    /* 计算能显示的最大字节数 */
    while (byteIndex < fileNameLen && displayWidth < maxDisplayWidth)
    {
        uint8_t c = (uint8_t)fileName[byteIndex];

        if (c < 0x80)
        {
            /* ASCII字符，宽度为1 */
            displayWidth++;
            byteIndex++;
        }
        else if (byteIndex + 1 < fileNameLen && (c >= 0xA1 && c <= 0xF7))
        {
            /* GB2312中文字符，宽度为2 */
            displayWidth += 2;
            byteIndex += 2;
        }
        else
        {
            /* 其他字符，当作单字节处理 */
            displayWidth++;
            byteIndex++;
        }
    }

    /* 如果文件名完全能显示，直接复制 */
    if (byteIndex >= fileNameLen && displayWidth <= maxDisplayWidth)
    {
        strncpy(buffer, fileName, bufferSize - 1);
        buffer[bufferSize - 1] = '\0';
        return;
    }

    /* 需要截断的情况 */
    uint16_t copyBytes = byteIndex;

    /* 如果最后一个字符是汉字的一半，需要回退 */
    if (copyBytes > 0 && copyBytes < fileNameLen)
    {
        uint8_t lastByte = (uint8_t)fileName[copyBytes - 1];
        uint8_t prevByte = (uint8_t)fileName[copyBytes - 2];

        if (lastByte >= 0x80 && lastByte <= 0xFF && (copyBytes == 1 || !(prevByte >= 0xA1 && prevByte <= 0xF7)))
        {
            /* 这是一个不完整的汉字，回退一个字节 */
            copyBytes--;
            displayWidth--;
        }
    }

    /* 为*号预留空间（ASCII字符，宽度为1） */
    if (displayWidth >= maxDisplayWidth)
    {
        /* 需要进一步缩减以腾出空间给*号 */
        while (copyBytes > 0 && displayWidth >= maxDisplayWidth)
        {
            uint8_t c = (uint8_t)fileName[copyBytes - 1];
            uint8_t prevByte = (uint8_t)fileName[copyBytes - 2];

            if (c < 0x80)
            {
                /* ASCII字符 */
                displayWidth--;
                copyBytes--;
            }
            else if (copyBytes >= 2 && (prevByte >= 0xA1 && prevByte <= 0xF7))
            {
                /* 汉字字符 */
                displayWidth -= 2;
                copyBytes -= 2;
            }
            else
            {
                /* 其他字符 */
                displayWidth--;
                copyBytes--;
            }
        }
    }

    /* 复制截断后的文件名 */
    if (copyBytes > 0)
    {
        strncpy(buffer, fileName, copyBytes);
        buffer[copyBytes] = '\0';

        /* 添加*号表示截断 */
        uint16_t bufferLen = String_GetLength(buffer, FATFS_PATH_LEN_MAX);
        if (bufferLen + 1 < bufferSize)
        {
            buffer[bufferLen] = '*';
            buffer[bufferLen + 1] = '\0';
        }
    }
    else
    {
        /* 如果什么都没复制，只显示* */
        if (bufferSize > 1)
        {
            buffer[0] = '*';
            buffer[1] = '\0';
        }
    }
}

/**
 * @brief 初始化文件列表
 * @param fileList 文件列表实例
 * @param config 配置参数
 */
void FileList_Init(FileList_T *fileList, const FileListConfig_T *config)
{
    if (fileList == NULL || config == NULL)
    {
        return;
    }

    /* 释放旧资源 */
    if (fileList->fileList.files != NULL)
    {
        FatFS_FreeFileList(&fileList->fileList);
    }

    /* 复制配置 */
    Memory_Copy(&fileList->config, config, sizeof(FileListConfig_T));

    /* 初始化状态 */
    fileList->fileList.files = NULL;
    fileList->fileList.count = 0;
    fileList->currentIndex = 0;
    fileList->scrollOffset = 0;
    fileList->currentPath[0] = '\0';
    fileList->statusMessage[0] = '\0';
    fileList->state = FILE_LIST_IDLE;
    fileList->InitFlag = 0;
    fileList->deep = 0;

    /* 初始化绘制控制标志 */
    fileList->needRedrawStatic = 1;
    fileList->needRedrawDynamic = 1;
    fileList->needRedrawScrollBar = 1;
    fileList->lastState = FILE_LIST_IDLE;
    fileList->lastScrollOffset = 0;

    /* 加载根目录 */
    if (FileList_LoadDirectory(fileList, config->rootPath))
    {
        fileList->state = FILE_LIST_BROWSING;
        fileList->InitFlag = 1;
    }
    else
    {
        fileList->state = FILE_LIST_ERROR;
        snprintf(fileList->statusMessage, sizeof(fileList->statusMessage), "初始化失败");
    }
}

/**
 * @brief 释放文件列表资源
 * @param fileList 文件列表实例
 */
void FileList_Deinit(FileList_T *fileList)
{
    if (fileList == NULL)
    {
        return;
    }

    /* 释放文件列表 */
    if (fileList->fileList.files != NULL)
    {
        FatFS_FreeFileList(&fileList->fileList);
        fileList->fileList.files = NULL;
        fileList->fileList.count = 0;
    }

    /* 重置所有状态标志 */
    fileList->InitFlag = 0;
    fileList->state = FILE_LIST_IDLE;
    fileList->currentIndex = 0;
    fileList->scrollOffset = 0;
    fileList->needRedrawStatic = 0;
    fileList->needRedrawDynamic = 0;
    fileList->needRedrawScrollBar = 0;
}

/**
 * @brief 比较两个文件/目录，用于排序
 * @param a 第一个文件信息指针
 * @param b 第二个文件信息指针
 * @return 比较结果（-1/0/1）
 */
static int FileList_CompareFiles(const void *a, const void *b)
{
    const FatFsFileInfo_T *fileA = (const FatFsFileInfo_T *)a;
    const FatFsFileInfo_T *fileB = (const FatFsFileInfo_T *)b;

    if (fileA->type == FATFS_TYPE_DIRECTORY && fileB->type != FATFS_TYPE_DIRECTORY)
    {
        return -1;
    }
    else if (fileA->type != FATFS_TYPE_DIRECTORY && fileB->type == FATFS_TYPE_DIRECTORY)
    {
        return 1;
    }
    else
    {
        return strcasecmp(fileA->Name, fileB->Name);
    }
}

/**
 * @brief 加载目录内容
 * @param fileList 文件列表实例
 * @param path 目录路径
 * @return 1:成功 0:失败
 */
uint8_t FileList_LoadDirectory(FileList_T *fileList, const char *path)
{
    if (fileList == NULL || path == NULL)
    {
        return 0;
    }

    /* 释放旧文件列表（原有代码） */
    if (fileList->fileList.files != NULL)
    {
        FatFS_FreeFileList(&fileList->fileList);
    }

    /* 临时路径处理（原有代码） */
    char *tmpPath = (char *)Memory_Malloc(MP_RAM_IN, FF_MAX_LFN + 1);
    if (tmpPath == NULL)
    {
        snprintf(fileList->statusMessage, sizeof(fileList->statusMessage), "内存分配失败");
        return 0;
    }
    Memory_Set(tmpPath, 0, FF_MAX_LFN);
    strcpy(tmpPath, path);

    /* 获取文件列表（原有代码） */
    if (FatFS_GetFileList(tmpPath, "*", &fileList->fileList, TRUE) != FATFS_RET_OK)
    {
        Memory_Free(MP_RAM_IN, tmpPath);
        snprintf(fileList->statusMessage, sizeof(fileList->statusMessage), "无法获取目录");
        return 0;
    }

    /* ===== 排序逻辑 ===== */
    /* 对文件列表按规则排序（目录在前，同类型按文件名排序） */
    if (fileList->fileList.count > 1)
    {
        HeapSort(fileList->fileList.files, fileList->fileList.count, sizeof(FatFsFileInfo_T), FileList_CompareFiles);
    }
    /* ======================== */

    /* 更新当前路径等状态（原有代码） */
    Memory_Set(fileList->currentPath, 0, sizeof(fileList->currentPath));
    strncpy(fileList->currentPath, path, sizeof(fileList->currentPath) - 1);

    fileList->currentIndex = 0;
    fileList->scrollOffset = 0;
    fileList->state = FILE_LIST_BROWSING;

    fileList->needRedrawStatic = 1;
    fileList->needRedrawDynamic = 1;
    fileList->needRedrawScrollBar = 1;

    snprintf(fileList->statusMessage, sizeof(fileList->statusMessage), "找到 %d 个项目", fileList->fileList.count);
    Memory_Free(MP_RAM_IN, tmpPath);

    return 1;
}

/**
 * @brief 进入目录
 * @param fileList 文件列表实例
 * @param index 文件索引
 * @param deep 目录深度
 */
void FileList_EnterDirectory(FileList_T *fileList, uint16_t index, uint8_t deep)
{
    if (fileList == NULL || index >= fileList->fileList.count)
    {
        return;
    }

    FatFsFileInfo_T *fileInfo = &fileList->fileList.files[index];

    if (fileInfo->type == FATFS_TYPE_DIRECTORY)
    {
        /* 进入子目录 */
        if (FileList_LoadDirectory(fileList, fileInfo->fullPath))
        {
            /* 成功进入目录 */
            snprintf(fileList->statusMessage, sizeof(fileList->statusMessage), "已进入目录");
            fileList->needRedrawDynamic = 1;
        }
    }
    else
    {
        /* 文件处理 - 可以在这里调用文本显示器或其他处理器 */
        FileType_T fileType = FileList_GetFileType(fileInfo->Name);
        if (fileType == FILE_TYPE_TEXT)
        {
            snprintf(fileList->statusMessage, sizeof(fileList->statusMessage), "文本文件预览");
            GUI_EnterPage(currentDevice, TextViewer_Function); /* 切换显示界面 */

            /* 配置文本显示器 */
            TextViewer_T *instance = TextViewer_GetInstance(); /* 获取文本管理器的实例 */
            TextViewerConfig_T config = {
                .StartY = 60,
                .titleHeight = 30,
                .statusBarHeight = 60,
                .showTitle = "文本文件查看器"
            };
            TextViewer_Init(instance, &config);
            TextViewer_LoadFile(instance, fileInfo->fullPath);
        }
        else
        {
            snprintf(fileList->statusMessage, sizeof(fileList->statusMessage), "二进制文件不支持预览");
            fileList->needRedrawDynamic = 1;
        }
    }
}

/**
 * @brief 返回上级目录
 * @param fileList 文件列表实例
 */
void FileList_BackToParent(FileList_T *fileList)
{
    if (fileList == NULL)
    {
        return;
    }

    /* 返回根目录 */
    if (FileList_LoadDirectory(fileList, fileList->config.rootPath))
    {
        snprintf(fileList->statusMessage, sizeof(fileList->statusMessage), "已返回根目录");
    }
    else
    {
        snprintf(fileList->statusMessage, sizeof(fileList->statusMessage), "无法返回根目录");
        fileList->needRedrawDynamic = 1;
    }
}

/**
 * @brief 绘制浏览模式的图片显示
 */
static void GetFileIcon(const FatFsFileInfo_T *fileInfo, char *iconStr, size_t iconStrSize)
{
    /* 确保iconStrSize至少为6 */
    if (iconStrSize < 6) { return; }

    /* 添加结束符 */
    iconStr[5] = '\0';

    /* 文件夹 */
    if (fileInfo->type == FATFS_TYPE_DIRECTORY)
    {
        strncpy(iconStr, "[DIR]", 6);
        return;
    }

    /* 文件 */
    strncpy(iconStr, "[   ]", 6);
    if (fileInfo->extension == NULL) { return; }

    for (uint8_t i = 0; i < 3 && fileInfo->extension[i] != '\0'; i++)
    {
        iconStr[i + 1] = toupper(fileInfo->extension[i]);
    }
}

/**
 * @brief 绘制单个文件项
 * @param fileList 文件列表实例
 * @param index 文件索引
 */
static void FileList_DrawSingleItem(FileList_T *fileList, uint16_t index)
{
    if (fileList == NULL || index >= fileList->fileList.count)
    {
        return;
    }

    /* ===== 使用宏计算位置 ===== */
    uint16_t StartY = fileList->config.titleHeight + ROW(1) - 10;
    uint16_t availableHeight = GUI_SCREEN_HEIGHT - StartY - fileList->config.statusBarHeight;
    uint16_t maxItems = availableHeight / fileList->config.itemHeight;

    /* 检查是否在可见范围内 */
    if (index < fileList->scrollOffset || index >= fileList->scrollOffset + maxItems)
    {
        fileList->needRedrawDynamic = 1;
        return;
    }

    uint16_t displayIndex = index - fileList->scrollOffset;
    uint16_t y = StartY + displayIndex * fileList->config.itemHeight;
    FatFsFileInfo_T *fileInfo = &fileList->fileList.files[index];

    /* 背景色 */
    uint16_t bgColor = (index == fileList->currentIndex) ? GUI_GRAY : GUI_BLACK;
    uint16_t textColor = (index == fileList->currentIndex) ? GUI_WHITE : GUI_WHITE;
    uint16_t iconColor = (index == fileList->currentIndex) ? GUI_RED : GUI_MAGENTA;
    uint16_t sizeColor = (index == fileList->currentIndex) ? GUI_RED : GUI_YELLOW;

    /* 绘制背景 */
    GUI_LCD_FillColor(COL(0), y, MENU_AREA_WIDTH, fileList->config.itemHeight, bgColor);

    /* 文件图标 */
    char iconStr[6];
    GetFileIcon(fileInfo, iconStr, sizeof(iconStr));
    GUI_LCD_ShowString(COL(0), y + 3, iconStr, iconColor, bgColor);

    /* 处理文件名截断（小屏适配，减小最大显示宽度） */
    char displayName[FF_MAX_LFN + 2];
    uint16_t maxFileNameWidth = 22; /* 小屏：320px - 图标区域 - 大小区域 */
    FileList_TruncateFileName(fileInfo->Name, displayName, sizeof(displayName), maxFileNameWidth);
    GUI_LCD_ShowString(COL(6), y + 3, displayName, textColor, bgColor);

    /* 文件大小（右对齐） */
    if (fileInfo->type != FATFS_TYPE_DIRECTORY)
    {
        char sizeStr[16];
        if (fileInfo->size < 1024)
        {
            snprintf(sizeStr, sizeof(sizeStr), "%uB", fileInfo->size);
        }
        else if (fileInfo->size < 1024 * 1024)
        {
            snprintf(sizeStr, sizeof(sizeStr), "%uK", (uint32_t)(fileInfo->size / 1024.0f + 0.5f));
        }
        else
        {
            snprintf(sizeStr, sizeof(sizeStr), "%uM", (uint32_t)(fileInfo->size / (1024 * 1024.0f) + 0.5f));
        }
        /* 右对齐到第30列（30×8=240px），留出滚动条空间 */
        GUI_LCD_ShowString(COL(30), y + 3, sizeStr, sizeColor, bgColor);
    }
}

/**
 * @brief 处理按键输入
 * @param fileList 文件列表实例
 */
void FileList_HandleKeyInput(FileList_T *fileList)
{
    if (fileList == NULL)
    {
        return;
    }

    switch (fileList->state)
    {
    case FILE_LIST_BROWSING:
    {
        if (Key_IsAction(KEY_UP))
        {
            if (fileList->fileList.count > 0)
            {
                uint16_t oldIndex = fileList->currentIndex;
                fileList->currentIndex = (fileList->currentIndex == 0) ? fileList->fileList.count - 1 : fileList->currentIndex - 1;

                if (abs(fileList->currentIndex - oldIndex) == 1)
                {
                    FileList_DrawSingleItem(fileList, oldIndex);
                    FileList_DrawSingleItem(fileList, fileList->currentIndex);
                }
                else
                {
                    fileList->needRedrawDynamic = 1;
                }
                fileList->needRedrawScrollBar = 1;
            }
        }
        else if (Key_IsAction(KEY_DOWN))
        {
            if (fileList->fileList.count > 0)
            {
                uint16_t oldIndex = fileList->currentIndex;
                fileList->currentIndex = (fileList->currentIndex == fileList->fileList.count - 1) ? 0 : fileList->currentIndex + 1;

                if (abs(fileList->currentIndex - oldIndex) == 1)
                {
                    FileList_DrawSingleItem(fileList, oldIndex);
                    FileList_DrawSingleItem(fileList, fileList->currentIndex);
                }
                else
                {
                    fileList->needRedrawDynamic = 1;
                }
                fileList->needRedrawScrollBar = 1;
            }
        }
        else if (Key_IsAction(KEY_ENTER))
        {
            if (fileList->currentIndex < fileList->fileList.count)
            {
                FileList_EnterDirectory(fileList, fileList->currentIndex, fileList->deep);
                fileList->needRedrawDynamic = 1;
                fileList->deep++;
            }
        }
        else if (Key_IsAction(KEY_MODE))
        {
            FileList_BackToParent(fileList);
            fileList->needRedrawDynamic = 1;
            fileList->deep--;
        }
    }
    break;

    default:
        break;
    }
}

/**
 * @brief 绘制浏览模式的动态内容
 * @param fileList 文件列表实例
 */
static void FileList_DrawBrowsingDynamic(FileList_T *fileList)
{
    if (fileList == NULL)
    {
        return;
    }

    /* ===== 使用宏 ===== */
    uint16_t StartY = fileList->config.titleHeight + ROW(1) - 10;
    uint16_t availableHeight = GUI_SCREEN_HEIGHT - StartY - fileList->config.statusBarHeight;
    uint16_t maxItems = availableHeight / fileList->config.itemHeight;

    /* 绘制路径（只在路径变化时重绘） */
    GUI_LCD_ShowString(COL(0), fileList->config.titleHeight + 3, "文件/文件夹列表", GUI_YELLOW, GUI_BLACK);

    /* 计算滚动偏移 */
    if (fileList->currentIndex >= fileList->scrollOffset + maxItems)
    {
        fileList->scrollOffset = fileList->currentIndex - maxItems + 1;
        fileList->needRedrawScrollBar = 1;
    }
    else if (fileList->currentIndex < fileList->scrollOffset)
    {
        fileList->scrollOffset = fileList->currentIndex;
        fileList->needRedrawScrollBar = 1;
    }

    /* 绘制可见项目 */
    uint16_t startIdx = fileList->scrollOffset;
    uint16_t endIdx = GUI_MIN(startIdx + maxItems, fileList->fileList.count);

    for (uint16_t i = 0; i < maxItems && (startIdx + i) < fileList->fileList.count; i++)
    {
        FileList_DrawSingleItem(fileList, i);
    }

    /* 清除超出范围的区域 */
    if (endIdx < startIdx + maxItems)
    {
        uint16_t clearY = StartY + (endIdx - startIdx) * fileList->config.itemHeight;
        uint16_t clearHeight = (startIdx + maxItems - endIdx) * fileList->config.itemHeight;
        GUI_LCD_FillColor(COL(0), clearY, MENU_AREA_WIDTH, clearHeight, GUI_BLACK);
    }
}

/**
 * @brief 绘制滚动条
 * @param fileList 文件列表实例
 */
static void FileList_DrawScrollBar(FileList_T *fileList)
{
    if (fileList == NULL || !fileList->needRedrawScrollBar)
    {
        return;
    }

    uint16_t StartY = fileList->config.titleHeight + ROW(1) - 10;
    uint16_t availableHeight = GUI_SCREEN_HEIGHT - StartY - fileList->config.statusBarHeight;
    uint16_t maxItems = availableHeight / fileList->config.itemHeight;

    if (fileList->state == FILE_LIST_BROWSING && fileList->fileList.count > maxItems)
    {
        uint16_t scrollBarHeight = (availableHeight * maxItems) / fileList->fileList.count;
        uint16_t scrollBarPos = (fileList->scrollOffset * availableHeight) / fileList->fileList.count;

        if (scrollBarHeight < 5)
            scrollBarHeight = 5;

        if (scrollBarPos + scrollBarHeight > availableHeight)
            scrollBarPos = availableHeight - scrollBarHeight;

        /* 滚动条X坐标 = 菜单区域宽度 - 滚动条宽度 - 2px边距 */
        uint16_t scrollBarX = MENU_AREA_WIDTH - SCROLL_BAR_WIDTH - 2;

        /* 清除旧滚动条 */
        GUI_LCD_FillColor(scrollBarX, StartY, SCROLL_BAR_WIDTH, availableHeight, GUI_GRAY);
        /* 绘制新滚动条 */
        GUI_LCD_FillColor(scrollBarX, StartY + scrollBarPos, SCROLL_BAR_WIDTH, scrollBarHeight, GUI_WHITE);
    }
    else
    {
        /* 没有滚动条时清除滚动条区域 */
        uint16_t scrollBarX = MENU_AREA_WIDTH - SCROLL_BAR_WIDTH - 2;
        uint16_t StartY = fileList->config.titleHeight + ROW(1) - 10;
        uint16_t availableHeight = GUI_SCREEN_HEIGHT - StartY - fileList->config.statusBarHeight;
        GUI_LCD_FillColor(scrollBarX, StartY, SCROLL_BAR_WIDTH, availableHeight, GUI_BLACK);
    }

    fileList->needRedrawScrollBar = 0;
}

/**
 * @brief 绘制静态内容（标题栏、状态栏背景等）
 * @param fileList 文件列表实例
 */
static void FileList_DrawStaticContent(FileList_T *fileList)
{
    if (fileList == NULL || !fileList->needRedrawStatic)
    {
        return;
    }

    /* ===== 标题栏 ===== */
    GUI_LCD_FillColor(COL(0), ROW(0), MENU_AREA_WIDTH, fileList->config.titleHeight, GUI_BLUE);

    /* ===== 状态栏 ===== */
    uint16_t statusBarY = GUI_SCREEN_HEIGHT - fileList->config.statusBarHeight;
    GUI_LCD_FillColor(COL(0), statusBarY, MENU_AREA_WIDTH, fileList->config.statusBarHeight, GUI_GRAY);

    /* 根据状态绘制标题 */
    switch (fileList->state)
    {
    case FILE_LIST_BROWSING:
        GUI_LCD_Printf(COL(0), ROW(0) + 3, GUI_WHITE, GUI_BLUE, "%s", fileList->config.showTitle);
        break;
    case FILE_LIST_ERROR:
        GUI_LCD_ShowString(COL(0), ROW(0) + 3, "错误", GUI_WHITE, GUI_RED);
        break;
    default:
        break;
    }

    /* 绘制操作提示（状态栏上方） */
    if (fileList->state == FILE_LIST_BROWSING)
    {
        GUI_LCD_ShowString(COL(0), statusBarY + 13, "上下:选择 确定:进入 模式:返回", GUI_WHITE, GUI_GRAY);
    }

    fileList->needRedrawStatic = 0;
}

/**
 * @brief 绘制状态消息
 * @param fileList 文件列表实例
 */
static void FileList_DrawStatusMessage(FileList_T *fileList)
{
    if (fileList == NULL)
    {
        return;
    }

    uint16_t statusBarY = GUI_SCREEN_HEIGHT - fileList->config.statusBarHeight;
    GUI_LCD_ShowString(COL(0), statusBarY + 3, fileList->statusMessage, GUI_WHITE, GUI_GRAY);
}

/**
 * @brief 绘制文件列表
 * @param fileList 文件列表实例
 */
void FileList_Draw(FileList_T *fileList)
{
    if (fileList == NULL)
    {
        return;
    }

    if (!fileList->needRedrawStatic && !fileList->needRedrawDynamic && !fileList->needRedrawScrollBar)
    {
        return;
    }

    if (fileList->state != fileList->lastState)
    {
        fileList->needRedrawStatic = 1;
        fileList->needRedrawDynamic = 1;
        fileList->lastState = fileList->state;

        if (fileList->state == FILE_LIST_BROWSING)
        {
            uint16_t StartY = fileList->config.titleHeight + ROW(1) - 10;
            uint16_t availableHeight = GUI_SCREEN_HEIGHT - StartY - fileList->config.statusBarHeight;
            GUI_LCD_FillColor(COL(0), StartY, MENU_AREA_WIDTH, availableHeight, GUI_BLACK);
        }
    }

    if (fileList->needRedrawStatic)
    {
        FileList_DrawStaticContent(fileList);
    }

    if (fileList->needRedrawDynamic)
    {
        if (fileList->state == FILE_LIST_BROWSING)
        {
            FileList_DrawBrowsingDynamic(fileList);
        }
        uint16_t statusBarY = GUI_SCREEN_HEIGHT - fileList->config.statusBarHeight;
        GUI_LCD_ShowString(COL(0), statusBarY + 3, fileList->statusMessage, GUI_WHITE, GUI_GRAY);
        fileList->needRedrawDynamic = 0;
    }

    FileList_DrawScrollBar(fileList);
    FileList_DrawStatusMessage(fileList);
}

/**
 * @brief 文件列表主函数
 * @param sender 事件发送者
 * @param key 按键值
 * @param state 当前状态
 * @param InitFlag 初始化标志
 * @return 下一个状态
 */
uint16_t FileBrowser_Function(GUI_FuncItem_T *sender, GUI_MenuParam_T *param)
{
    /* 首次进入时初始化 */
    if (param->InitFlag != 0)
    {
        /* ===== 清屏（使用宏） ===== */
        GUI_LCD_FillColor(COL(0), ROW(0), MENU_AREA_WIDTH, GUI_SCREEN_HEIGHT, GUI_BLACK);
        /* 龙脉图像已删除 */
        Key_ClearAll();

        /* 配置文件列表（小屏适配） */
        FileListConfig_T config = {
            .rootPath = "0:/",
            .maxDisplayItems = 6, /* 小屏每页显示6个文件 */
            .itemHeight = 28,
            .StartY = 60,
            .titleHeight = TITLE_HEIGHT,
            .statusBarHeight = 55,
            .showTitle = "NAND Flash 文件列表"
        };

        FileList_Init(&s_FileList, &config);
        currentDevice = param->baseUI;

        return 1;
    }

    /* 优先检测长按事件 */
    if (Key_IsLongPressed(KEY_MODE))
    {
        FileList_Deinit(&s_FileList);
        GUI_ExitPage(param->baseUI);
        return 0;
    }
    else
    {
        FileList_HandleKeyInput(&s_FileList);
    }

    FileList_Draw(&s_FileList);

    return param->userState;
}