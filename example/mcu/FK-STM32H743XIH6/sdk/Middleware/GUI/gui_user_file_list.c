#include "gui_user_file_list.h"
#include "gui.h"
#include "bsp_system.h"
#include "ff.h"

#include <string.h>
#include <stdlib.h>

/**
 * @brief 初始化文件选择器
 * @param selector 文件选择器实例
 * @param config 配置参数
 */
void FileSelector_Init(FileSelector_T *selector, const FileSelectorConfig_T *config)
{
    if (selector == NULL || config == NULL)
    {
        return;
    }

    /* 复制配置 */
    Memory_Copy(&selector->config, config, sizeof(FileSelectorConfig_T));

    /* 初始化状态 */
    selector->currentIndex = 0;
    selector->currentFilePath[0] = '\0';
    selector->statusMessage[0] = '\0';
    selector->state = FILE_SELECTOR_IDLE;
    selector->InitFlag = 0;
    selector->lastStartIndex = 0;

    /* 加载文件列表 */
    if (FileSelector_LoadFileList(selector))
    {
        selector->state = FILE_SELECTOR_SELECTING;
        selector->InitFlag = 1;

        /* 选择第一个文件 */
        if (selector->fileList.count > 0)
        {
            FileSelector_SelectFile(selector, 0);
        }
    }
    else
    {
        selector->state = FILE_SELECTOR_ERROR;
        snprintf(selector->statusMessage, sizeof(selector->statusMessage), "加载文件列表失败");
    }
}

/**
 * @brief 释放文件选择器资源
 * @param selector 文件选择器实例
 */
void FileSelector_Deinit(FileSelector_T *selector)
{
    if (selector == NULL)
    {
        return;
    }

    FatFS_FreeFileList(&selector->fileList);
    selector->InitFlag = 0;
    selector->state = FILE_SELECTOR_IDLE;
}

/**
 * @brief 截断文件名为适配显示宽度的格式（支持GB2312双字节和ASCII单字节字符）
 * @param fileName 原始文件名（输入）
 * @param buffer 输出缓冲区，用于存储截断后的文件名
 * @param bufferSize 输出缓冲区的字节大小
 * @param maxDisplayWidth 最大显示宽度（ASCII字符占1个宽度，GB2312双字节字符占2个宽度）
 */
static void FileSelector_TruncateFileName(const char *fileName, char *buffer, uint16_t bufferSize, uint16_t maxDisplayWidth)
{
    /* 入参合法性校验：空指针/缓冲区大小为0直接返回 */
    if (fileName == NULL || buffer == NULL || bufferSize == 0)
    {
        return;
    }

    uint16_t fileNameLen = String_GetLength(fileName, FATFS_PATH_LEN_MAX);
    uint16_t displayWidth = 0; /* 累计显示宽度（双字节字符计2，ASCII字符计1） */
    uint16_t byteIndex = 0;    /* 字节遍历索引 */

    /* 计算不超过最大显示宽度的最大字节数 */
    while (byteIndex < fileNameLen && displayWidth < maxDisplayWidth)
    {
        uint8_t c = (uint8_t)fileName[byteIndex];

        if (c < 0x80)
        {
            /* ASCII字符：宽度+1，字节索引+1 */
            displayWidth++;
            byteIndex++;
        }
        else if (byteIndex + 1 < fileNameLen && (c >= 0xA1 && c <= 0xF7) && ((uint8_t)fileName[byteIndex + 1] >= 0xA1))
        {
            /* GB2312双字节字符：宽度+2，字节索引+2 */
            displayWidth += 2;
            byteIndex += 2;
        }
        else
        {
            /* 其他字符（非GB2312双字节）：按单字节计算宽度 */
            displayWidth++;
            byteIndex++;
        }
    }

    /* 如果文件名完整显示不超宽度：直接复制完整文件名到缓冲区 */
    if (byteIndex >= fileNameLen && displayWidth <= maxDisplayWidth)
    {
        strncpy(buffer, fileName, bufferSize - 1);
        buffer[bufferSize - 1] = '\0';
        return;
    }

    /* 初步确定截断的字节位置 */
    uint16_t copyBytes = byteIndex;

    /* 处理边界：如果最后一个字符是双字节的后半部分，回退1个字节（避免截断半个双字节字符） */
    if (copyBytes > 0 && copyBytes < fileNameLen)
    {
        uint8_t lastByte = (uint8_t)fileName[copyBytes - 1];
        if (lastByte >= 0x80 && lastByte <= 0xFF && (copyBytes == 1 || !((uint8_t)fileName[copyBytes - 2] >= 0xA1 && (uint8_t)fileName[copyBytes - 2] <= 0xF7)))
        {
            /* 最后一个字节是双字节字符的后半部分：回退1个字节 */
            copyBytes--;
            displayWidth--;
        }
    }

    /* 为添加省略号(*)预留空间（ASCII字符占1个宽度） */
    if (displayWidth >= maxDisplayWidth)
    {
        /* 向前调整截断位置，确保能放下省略号 */
        while (copyBytes > 0 && displayWidth >= maxDisplayWidth)
        {
            uint8_t c = (uint8_t)fileName[copyBytes - 1];

            if (c < 0x80)
            {
                /* ASCII字符：宽度-1，字节索引-1 */
                displayWidth--;
                copyBytes--;
            }
            else if (copyBytes >= 2 && ((uint8_t)fileName[copyBytes - 2] >= 0xA1 && (uint8_t)fileName[copyBytes - 2] <= 0xF7))
            {
                /* 双字节字符：宽度-2，字节索引-2 */
                displayWidth -= 2;
                copyBytes -= 2;
            }
            else
            {
                /* 其他字符：宽度-1，字节索引-1 */
                displayWidth--;
                copyBytes--;
            }
        }
    }

    /* 复制截断后的文件名到缓冲区 */
    if (copyBytes > 0)
    {
        strncpy(buffer, fileName, copyBytes);
        buffer[copyBytes] = '\0';

        /* 追加*号标识文件名已截断（确保缓冲区有足够空间） */
        uint16_t bufferLen = String_GetLength(buffer, FATFS_PATH_LEN_MAX);
        if (bufferLen + 1 < bufferSize)
        {
            buffer[bufferLen] = '*';
            buffer[bufferLen + 1] = '\0';
        }
    }
    else
    {
        /* 无有效字符可复制时：缓冲区中仅显示一个* */
        if (bufferSize > 1)
        {
            buffer[0] = '*';
            buffer[1] = '\0';
        }
    }
}

/**
 * @brief 加载文件列表
 * @param selector 文件选择器实例
 * @return 1:成功 0:失败
 */
uint8_t FileSelector_LoadFileList(FileSelector_T *selector)
{
    if (selector == NULL) { return 0; }
    FatFsStatus_T ret = FatFS_GetFileList(selector->config.directoryPath, selector->config.filePattern, &selector->fileList, TRUE);
    if (selector->fileList.count <= 0) { return 0; }
    return ret == FATFS_RET_OK;
}

/**
 * @brief 选择文件
 * @param selector 文件选择器实例
 * @param index 文件索引
 */
void FileSelector_SelectFile(FileSelector_T *selector, uint16_t index)
{
    if (selector == NULL || index >= selector->fileList.count || selector->fileList.files[index].Name == NULL) { return; }
    selector->currentIndex = index;
    snprintf(selector->currentFilePath, sizeof(selector->currentFilePath), "%s/%s", selector->config.directoryPath, selector->fileList.files[index].Name);
}

/**
 * @brief 绘制滚动指示器
 * @param selector 文件选择器实例
 */
static void FileSelector_DrawScrollIndicator(FileSelector_T *selector)
{
    if (selector == NULL || selector->fileList.count <= selector->config.maxDisplayItems) { return; }

    /* ===== 滚动条位置 ===== */
    uint16_t scrollBarX = MENU_AREA_WIDTH - SCROLL_BAR_WIDTH - 2;
    uint16_t scrollAreaHeight = selector->config.maxDisplayItems * selector->config.itemHeight;

    uint16_t scrollPos = (selector->currentIndex * scrollAreaHeight) / selector->fileList.count;
    uint16_t Height = scrollAreaHeight / GUI_MIN(selector->fileList.count, 50);
    if (Height + scrollPos > scrollAreaHeight)
    {
        Height = scrollAreaHeight - scrollPos - 1;
    }
    if (Height < 3) Height = 3;

    /* 绘制滚动条背景和滑块 */
    GUI_LCD_FillColor(scrollBarX, selector->config.StartY, SCROLL_BAR_WIDTH, scrollAreaHeight, GUI_GRAY);
    GUI_LCD_FillColor(scrollBarX, selector->config.StartY + scrollPos, SCROLL_BAR_WIDTH, Height, GUI_WHITE);
}

/**
 * @brief 绘制文件选择器当前页的文件列表界面
 * @param selector 文件选择器实例指针（包含配置、文件列表等核心信息）
 * @param selectedIndex 当前选中文件项的索引
 */
static void FileSelector_DrawCurrentPage(FileSelector_T *selector, uint16_t selectedIndex)
{
    if (selector == NULL) { return; }

    /* 计算当前页的文件项起始/结束索引 */
    uint16_t currentPage = selectedIndex / selector->config.maxDisplayItems;
    uint16_t startIdx = currentPage * selector->config.maxDisplayItems;
    uint16_t endIdx = GUI_MIN(startIdx + selector->config.maxDisplayItems, selector->fileList.count);

    /* 清空文件列表显示区域 */
    GUI_LCD_FillColor(COL(0), selector->config.StartY, MENU_AREA_WIDTH,
                      selector->config.maxDisplayItems * selector->config.itemHeight, GUI_BLACK);

    /* 滚动条预留宽度 */
    uint16_t scrollBarReserve = SCROLL_BAR_WIDTH + 4;
    uint16_t itemWidth = MENU_AREA_WIDTH - scrollBarReserve;
    uint16_t arrowX = itemWidth - 24; /* 箭头位置 */

    /* 遍历绘制当前页的所有文件项 */
    for (uint16_t i = startIdx; i < endIdx; i++)
    {
        uint16_t yPos = selector->config.StartY + (i - startIdx) * selector->config.itemHeight;

        char displayName[FF_MAX_LFN + 2];
        /* 小屏：菜单宽度 300px - 滚动条(4px) - 边距(8px) - 箭头区(24px) ≈ 264px → 33个字符宽度 */
        uint16_t maxFileNameWidth = 30;
        FileSelector_TruncateFileName(selector->fileList.files[i].Name, displayName, sizeof(displayName), maxFileNameWidth);

        if (i == selectedIndex)
        {
            /* 选中项：灰色背景 + 白色文字 + 右箭头 */
            GUI_LCD_FillColor(COL(0), yPos, itemWidth, selector->config.itemHeight, GUI_GRAY);
            GUI_LCD_ShowString(COL(0), yPos + 2, displayName, GUI_WHITE, GUI_GRAY);
            GUI_LCD_ShowString(arrowX, yPos + 2, "<", GUI_WHITE, GUI_GRAY);
        }
        else
        {
            /* 非选中项：黑色背景 + 白色文字 */
            GUI_LCD_FillColor(COL(0), yPos, itemWidth, selector->config.itemHeight, GUI_BLACK);
            GUI_LCD_ShowString(COL(0), yPos + 2, displayName, GUI_WHITE, GUI_BLACK);
        }
    }
}

/**
 * @brief 绘制文件列表
 * @param selector 文件选择器实例
 */
void FileSelector_DrawFileList(FileSelector_T *selector)
{
    if (selector == NULL) { return; }

    /* ===== 清除屏幕 ===== */
    GUI_LCD_FillColor(COL(0), ROW(0), MENU_AREA_WIDTH, GUI_SCREEN_HEIGHT, GUI_BLACK);

    /* ===== 标题栏 ===== */
    GUI_LCD_FillColor(COL(0), ROW(0), MENU_AREA_WIDTH, selector->config.titleHeight, GUI_BLUE);
    GUI_LCD_Printf(COL(0), ROW(0) + 2, GUI_WHITE, GUI_BLUE, "%s", selector->config.showTitle);

    /* ===== 文件列表 ===== */
    if (selector->fileList.count == 0)
    {
        GUI_LCD_ShowString(COL(0), selector->config.StartY + 10, "未找到匹配文件!", GUI_RED, GUI_BLACK);
        GUI_LCD_ShowString(COL(0), selector->config.StartY + 40, "请检查目录和文件类型", GUI_WHITE, GUI_BLACK);
    }
    else
    {
        FileSelector_DrawCurrentPage(selector, selector->currentIndex);
        FileSelector_DrawScrollIndicator(selector);
    }

    /* ===== 操作提示（底部） ===== */
    uint16_t hintY = GUI_SCREEN_HEIGHT - selector->config.hintHeight;
    GUI_LCD_ShowString(COL(0), hintY + 3, "上下:选择文件", GUI_YELLOW, GUI_BLACK);
    GUI_LCD_ShowString(COL(0), hintY + 23, "模式:退出", GUI_YELLOW, GUI_BLACK);
    GUI_LCD_ShowString(COL(0), hintY + 43, "确定:选择文件", GUI_YELLOW, GUI_BLACK);
}

/**
 * @brief 重绘文件选择器中选中项变化的行
 * @param selector 文件选择器实例指针
 * @param oldIndex 旧选中项的索引
 * @param newIndex 新选中项的索引
 */
void FileSelector_DrawChangedLines(FileSelector_T *selector, uint16_t oldIndex, uint16_t newIndex)
{
    if (selector == NULL) { return; }

    uint16_t oldPage = oldIndex / selector->config.maxDisplayItems;
    uint16_t newPage = newIndex / selector->config.maxDisplayItems;

    char oldDisplayName[FF_MAX_LFN + 2];
    char newDisplayName[FF_MAX_LFN + 2];
    /* 小屏适配：减小文件名显示宽度 */
    uint16_t maxFileNameWidth = 30;

    if (oldPage != newPage)
    {
        FileSelector_DrawCurrentPage(selector, newIndex);
    }
    else
    {
        /* 滚动条预留宽度 */
        uint16_t scrollBarReserve = SCROLL_BAR_WIDTH + 4;
        uint16_t itemWidth = MENU_AREA_WIDTH - scrollBarReserve;
        uint16_t arrowX = itemWidth - 24;

        FileSelector_TruncateFileName(selector->fileList.files[oldIndex].Name, oldDisplayName, sizeof(oldDisplayName), maxFileNameWidth);
        FileSelector_TruncateFileName(selector->fileList.files[newIndex].Name, newDisplayName, sizeof(newDisplayName), maxFileNameWidth);

        /* 旧选中项：恢复为非选中样式 */
        uint16_t oldY = selector->config.StartY + (oldIndex % selector->config.maxDisplayItems) * selector->config.itemHeight;
        GUI_LCD_FillColor(COL(0), oldY, itemWidth, selector->config.itemHeight, GUI_BLACK);
        GUI_LCD_ShowString(COL(0), oldY + 2, oldDisplayName, GUI_WHITE, GUI_BLACK);

        /* 新选中项：绘制为选中样式 */
        uint16_t newY = selector->config.StartY + (newIndex % selector->config.maxDisplayItems) * selector->config.itemHeight;
        GUI_LCD_FillColor(COL(0), newY, itemWidth, selector->config.itemHeight, GUI_GRAY);
        GUI_LCD_ShowString(COL(0), newY + 2, newDisplayName, GUI_WHITE, GUI_GRAY);
        GUI_LCD_ShowString(arrowX, newY + 2, "<", GUI_WHITE, GUI_GRAY);
    }

    FileSelector_DrawScrollIndicator(selector);
}

/**
 * @brief 处理按键输入
 * @param selector 文件选择器实例
 * @param key 按键值
 * @param callback 选择回调函数
 */
void FileSelector_HandleKeyInput(FileSelector_T *selector, FileSelectionCallback callback)
{
    uint16_t oldIndex = selector->currentIndex;

    if (selector == NULL)
    {
        return;
    }

    if (Key_IsAction(KEY_UP))
    {
        if (selector->fileList.count > 0)
        {
            selector->currentIndex = (selector->currentIndex == 0) ?
                                         selector->fileList.count - 1 :
                                         selector->currentIndex - 1;
            FileSelector_SelectFile(selector, selector->currentIndex);
            FileSelector_DrawChangedLines(selector, oldIndex, selector->currentIndex);
        }
    }
    if (Key_IsAction(KEY_DOWN))
    {
        if (selector->fileList.count > 0)
        {
            selector->currentIndex = (selector->currentIndex == selector->fileList.count - 1) ?
                                         0 :
                                         selector->currentIndex + 1;
            FileSelector_SelectFile(selector, selector->currentIndex);
            FileSelector_DrawChangedLines(selector, oldIndex, selector->currentIndex);
        }
    }
    if (Key_IsAction(KEY_ENTER))
    {
        if (selector->fileList.count > 0 && callback != NULL)
        {
            callback(selector, selector->currentFilePath);
        }
    }
}

/**
 * @brief 获取当前选择的文件路径
 * @param selector 文件选择器实例
 * @return 文件路径字符串
 */
const char *FileSelector_GetCurrentFilePath(FileSelector_T *selector)
{
    if (selector == NULL)
    {
        return NULL;
    }

    return selector->currentFilePath;
}

/**
 * @brief 获取状态消息
 * @param selector 文件选择器实例
 * @return 状态消息字符串
 */
const char *FileSelector_GetStatusMessage(FileSelector_T *selector)
{
    if (selector == NULL)
    {
        return NULL;
    }

    return selector->statusMessage;
}

/**
 * @brief 获取文件选择器状态
 * @param selector 文件选择器实例
 * @return 文件选择器状态
 */
FileSelectorState_T FileSelector_GetState(FileSelector_T *selector)
{
    if (selector == NULL)
    {
        return FILE_SELECTOR_ERROR;
    }

    return selector->state;
}