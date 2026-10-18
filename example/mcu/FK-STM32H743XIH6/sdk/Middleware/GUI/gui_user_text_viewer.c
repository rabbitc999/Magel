#include "gui_user_text_viewer.h"
#include "gui.h"

#include "file.h"

#include "bsp_system.h"
#include <string.h>
#include <stdlib.h>

/* 静态全局变量 */
static TextViewer_T s_TextViewer; /* 文本显示器实例 */
static GUI_Device_T *currentDevice;
static GUI_DeviceInfo_T s_MemDevice = { 0 };
static uint16_t *s_drawBuffer, s_drawWidth, s_drawHeight;

/* 外部函数 */
uint16_t FileBrowser_Function(GUI_FuncItem_T *sender, GUI_MenuParam_T *param);

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
    if (x < s_drawWidth && y < s_drawHeight)
    {
        s_drawBuffer[y * s_drawWidth + x] = g_LCD_DeviceInfo.swapFlag ? (uint16_t)GUI_SwapBytes(color) : (uint16_t)(color);
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
    uint16_t i, j;

    if (x >= s_drawWidth || y >= s_drawHeight) return;
    if (x + Width > s_drawWidth) Width = s_drawWidth - x;
    if (y + Height > s_drawHeight) Height = s_drawHeight - y;

    for (j = y; j < y + Height; j++)
    {
        for (i = x; i < x + Width; i++)
        {
            s_drawBuffer[j * s_drawWidth + i] = g_LCD_DeviceInfo.swapFlag ? (uint16_t)GUI_SwapBytes(color) : (uint16_t)(color);
        }
    }
}

/**
 * @brief 清空图形缓冲区
 */
static void TextViewer_ClearDrawBuffer(void)
{
    GUI_BasePainter_Fill(&s_MemDevice, 0, 0, s_drawWidth, s_drawHeight, GUI_BLACK);
}

/**
 * @brief 获取文本浏览器示例
 * @return 文本浏览器示例指针
 */
TextViewer_T *TextViewer_GetInstance(void)
{
    return &s_TextViewer;
}

/**
 * @brief 获取字符显示宽度（GB2312编码）
 * @param text 文本指针
 * @param remaining 剩余字节数
 * @return 字符宽度（字节数）
 */
static uint8_t TextViewer_GetCharWidth(const char *text, uint32_t remaining)
{
    if (remaining == 0) return 0;

    uint8_t c = (uint8_t)text[0];

    if (c < 0x80)
    {
        return 1;
    }
    else if (remaining >= 2 && (c >= 0xA1 && c <= 0xF7))
    {
        return 2;
    }
    else
    {
        return 1;
    }
}

/**
 * @brief 初始化文本显示器
 * @param viewer 文本显示器实例
 * @param config 配置参数
 */
void TextViewer_Init(TextViewer_T *viewer, const TextViewerConfig_T *config)
{
    if (viewer == NULL || config == NULL)
    {
        return;
    }

    if (viewer->previewBuffer != NULL)
    {
        Memory_Free(MP_RAM_EX, viewer->previewBuffer);
        viewer->previewBuffer = NULL;
    }

    Memory_Copy(&viewer->config, config, sizeof(TextViewerConfig_T));

    viewer->previewBuffer = NULL;
    viewer->previewSize = 0;
    viewer->previewScrollY = 0;
    viewer->fileName[0] = '\0';
    viewer->statusMessage[0] = '\0';
    viewer->state = TEXT_VIEWER_IDLE;
    viewer->InitFlag = 0;

    viewer->needRedrawStatic = 1;
    viewer->needRedrawDynamic = 1;
    viewer->needRedrawScrollBar = 1;
    viewer->lastState = TEXT_VIEWER_IDLE;
    viewer->lastPreviewScrollY = 0;

    /* ===== 初始化绘图缓冲区（小屏适配） ===== */
    s_drawWidth = MENU_AREA_WIDTH - SCROLL_BAR_WIDTH - 4;
    s_drawHeight = GUI_SCREEN_HEIGHT - viewer->config.titleHeight - viewer->config.statusBarHeight - 8;
    s_drawBuffer = (uint16_t *)Memory_Malloc(MP_RAM_EX, s_drawWidth * s_drawHeight * sizeof(uint16_t));
    GUI_BasePainter_Fill(&s_MemDevice, 0, 0, s_drawWidth, s_drawHeight, GUI_BLACK);

    s_MemDevice.drawPointCallback = memDeviceDrawPoint;
    s_MemDevice.fillColorCallback = memDeviceFillColor;
    s_MemDevice.Width = s_drawWidth;
    s_MemDevice.Height = s_drawHeight;
    s_MemDevice.showFont = &g_GUI_FontGB2312_16;
    s_MemDevice.fillMode = GUI_PAINTER_FILL_COLOR;
}

/**
 * @brief 释放文本显示器资源
 * @param viewer 文本显示器实例
 */
void TextViewer_Deinit(TextViewer_T *viewer)
{
    if (viewer == NULL)
    {
        return;
    }

    if (viewer->previewBuffer != NULL)
    {
        Memory_Free(MP_RAM_EX, viewer->previewBuffer);
        viewer->previewBuffer = NULL;
        viewer->previewSize = 0;
    }

    if (s_drawBuffer != NULL)
    {
        Memory_Free(MP_RAM_EX, s_drawBuffer);
        s_drawBuffer = NULL;
    }

    viewer->InitFlag = 0;
    viewer->state = TEXT_VIEWER_IDLE;
    viewer->previewScrollY = 0;
    viewer->needRedrawStatic = 0;
    viewer->needRedrawDynamic = 0;
    viewer->needRedrawScrollBar = 0;
}

/**
 * @brief 加载文本文件
 * @param viewer 文本显示器实例
 * @param filePath 文件路径
 * @return 1:成功 0:失败
 */
uint8_t TextViewer_LoadFile(TextViewer_T *viewer, const char *filePath)
{
    if (viewer == NULL || filePath == NULL)
    {
        return 0;
    }

    if (viewer->previewBuffer != NULL)
    {
        Memory_Free(MP_RAM_EX, viewer->previewBuffer);
        viewer->previewBuffer = NULL;
        viewer->previewSize = 0;
    }

    viewer->previewSize = FatFS_ReadFileAll(MP_RAM_EX, filePath, &viewer->previewBuffer);
    if (viewer->previewSize > 0)
    {
        const char *fileName = strrchr(filePath, '/');
        if (fileName == NULL)
        {
            fileName = strrchr(filePath, '\\');
        }
        if (fileName == NULL)
        {
            fileName = filePath;
        }
        else
        {
            fileName++;
        }
        strncpy(viewer->fileName, fileName, sizeof(viewer->fileName) - 1);
        viewer->fileName[sizeof(viewer->fileName) - 1] = '\0';

        snprintf(viewer->statusMessage, sizeof(viewer->statusMessage), "预览模式 (%u 字节)", viewer->previewSize);

        viewer->state = TEXT_VIEWER_VIEWING;
        viewer->previewScrollY = 0;
        viewer->InitFlag = 1;

        viewer->needRedrawStatic = 1;
        viewer->needRedrawDynamic = 1;
        viewer->needRedrawScrollBar = 1;

        return 1;
    }
    else
    {
        viewer->state = TEXT_VIEWER_ERROR;
        snprintf(viewer->statusMessage, sizeof(viewer->statusMessage), "无法读取文件: %s", viewer->fileName);
        return 0;
    }
}

/**
 * @brief 处理按键输入
 * @param viewer 文本显示器实例
 */
void TextViewer_HandleKeyInput(TextViewer_T *viewer)
{
    if (viewer == NULL)
    {
        return;
    }

    switch (viewer->state)
    {
    case TEXT_VIEWER_VIEWING:
    {
        /* ===== 滚动步长 ===== */
        uint16_t scrollStep = CHAR_HEIGHT + 4; /* 约 20px */
        if (Key_IsAction(KEY_UP))
        {
            if (viewer->previewScrollY >= scrollStep)
            {
                viewer->previewScrollY -= scrollStep;
                viewer->needRedrawDynamic = 1;
                viewer->needRedrawScrollBar = 1;
            }
        }
        if (Key_IsAction(KEY_DOWN))
        {
            viewer->previewScrollY += scrollStep;
            viewer->needRedrawDynamic = 1;
            viewer->needRedrawScrollBar = 1;
        }
        break;
    }
    default:
        break;
    }

    if (Key_IsAction(KEY_ENTER) || Key_IsAction(KEY_MODE))
    {
        viewer->state = TEXT_VIEWER_IDLE;
        viewer->needRedrawStatic = 1;
        viewer->needRedrawDynamic = 1;
        snprintf(viewer->statusMessage, sizeof(viewer->statusMessage), "已退出预览");
        TextViewer_Deinit(viewer);
        GUI_ExitPage(currentDevice);
    }
}

/**
 * @brief 绘制文本预览
 * @param viewer 文本显示器实例
 */
void TextViewer_DrawTextPreview(TextViewer_T *viewer)
{
    if (viewer == NULL || viewer->previewBuffer == NULL)
    {
        return;
    }

    /* ===== 使用宏计算布局 ===== */
    uint16_t StartY = viewer->config.titleHeight + ROW(1) - 10;
    uint16_t availableHeight = GUI_SCREEN_HEIGHT - StartY - viewer->config.statusBarHeight;
    uint16_t lineHeight = CHAR_HEIGHT + 4; /* 行高 = 字体高度 + 行间距 */
    uint16_t maxLines = availableHeight / lineHeight;
    uint16_t charWidth = CHAR_WIDTH;

    uint16_t maxCharsPerLine = (MENU_AREA_WIDTH - SCROLL_BAR_WIDTH - 10) / charWidth;

    TextViewer_ClearDrawBuffer();

    uint16_t startLine = viewer->previewScrollY / lineHeight;
    uint16_t currentY = viewer->previewScrollY % lineHeight;

    char *textPtr = (char *)viewer->previewBuffer;
    uint16_t currentLine = 0;
    uint16_t displayWidthInLine = 0;
    char lineBuffer[256];
    uint16_t lineBufferPos = 0;
    uint32_t remaining = viewer->previewSize;

    while (remaining > 0 && *textPtr != '\0' && currentLine < startLine + maxLines)
    {
        char c = *textPtr;

        if (c == '\n')
        {
            if (lineBufferPos > 0)
            {
                lineBuffer[lineBufferPos] = '\0';
                if (currentLine >= startLine)
                {
                    GUI_BasePainter_ShowString(&s_MemDevice, 0, currentY, lineBuffer, GUI_WHITE, GUI_BLACK);

                    char *lastChar = lineBuffer + String_GetLength(lineBuffer, maxCharsPerLine) - 1;
                    uint8_t lastCharBytes = 1;

                    if (*lastChar & 0x80)
                    {
                        lastChar--;
                        lastCharBytes++;
                    }

                    uint16_t lastCharDisplayWidth = (lastCharBytes > 1) ? 2 : 1;
                    uint16_t lastCharX = (displayWidthInLine - lastCharDisplayWidth) * charWidth;

                    GUI_BasePainter_ShowString(&s_MemDevice, lastCharX, currentY, lastChar, GUI_WHITE, GUI_GRAY);

                    currentY += lineHeight;
                }
                currentLine++;
                lineBufferPos = 0;
                displayWidthInLine = 0;
            }
            textPtr++;
            remaining--;
        }
        else if (c != '\r')
        {
            if (lineBufferPos == 0 && (c == ' ' || c == '\t'))
            {
                while (remaining > 0 && (*textPtr == ' ' || *textPtr == '\t'))
                {
                    textPtr++;
                    remaining--;
                }
                continue;
            }

            uint8_t charBytes = TextViewer_GetCharWidth(textPtr, remaining);
            uint8_t charDisplayWidth = (charBytes > 1) ? 2 : 1;

            if (displayWidthInLine + charDisplayWidth > maxCharsPerLine)
            {
                if (lineBufferPos > 0)
                {
                    lineBuffer[lineBufferPos] = '\0';
                    if (currentLine >= startLine)
                    {
                        GUI_BasePainter_ShowString(&s_MemDevice, 0, currentY, lineBuffer, GUI_WHITE, GUI_BLACK);
                        currentY += lineHeight;
                    }
                    currentLine++;
                    lineBufferPos = 0;
                    displayWidthInLine = 0;
                }
            }

            if (lineBufferPos + charBytes < sizeof(lineBuffer) - 1)
            {
                for (uint8_t i = 0; i < charBytes; i++)
                {
                    lineBuffer[lineBufferPos++] = textPtr[i];
                }
                displayWidthInLine += charDisplayWidth;
            }

            textPtr += charBytes;
            remaining -= charBytes;
        }
        else
        {
            textPtr++;
            remaining--;
        }
    }

    if (lineBufferPos > 0 && currentLine >= startLine && currentLine < startLine + maxLines)
    {
        lineBuffer[lineBufferPos] = '\0';
        GUI_BasePainter_ShowString(&s_MemDevice, 0, currentY, lineBuffer, GUI_WHITE, GUI_BLACK);
    }

    GUI_LCD_DrawBuffer(COL(0), StartY, s_drawWidth, s_drawHeight, s_drawBuffer);
}

/**
 * @brief 绘制静态内容（标题栏、状态栏背景等）
 * @param viewer 文本显示器实例
 */
static void TextViewer_DrawStaticContent(TextViewer_T *viewer)
{
    if (viewer == NULL || !viewer->needRedrawStatic)
    {
        return;
    }

    /* ===== 标题栏 ===== */
    GUI_LCD_FillColor(COL(0), ROW(0), MENU_AREA_WIDTH, viewer->config.titleHeight, GUI_BLUE);

    /* ===== 状态栏 ===== */
    uint16_t statusBarY = GUI_SCREEN_HEIGHT - viewer->config.statusBarHeight;
    GUI_LCD_FillColor(COL(0), statusBarY, MENU_AREA_WIDTH, viewer->config.statusBarHeight, GUI_GRAY);

    /* 根据状态绘制标题 */
    switch (viewer->state)
    {
    case TEXT_VIEWER_VIEWING:
        GUI_LCD_ShowString(COL(0), ROW(0) + 3, "文件预览", GUI_WHITE, GUI_BLUE);
        break;
    case TEXT_VIEWER_ERROR:
        GUI_LCD_ShowString(COL(0), ROW(0) + 3, "错误", GUI_WHITE, GUI_RED);
        break;
    default:
        break;
    }

    /* 绘制操作提示 */
    if (viewer->state == TEXT_VIEWER_VIEWING)
    {
        GUI_LCD_ShowString(COL(0), statusBarY + 13, "上下:滚动 确定:返回", GUI_WHITE, GUI_GRAY);
    }

    viewer->needRedrawStatic = 0;
}

/**
 * @brief 绘制滚动条
 * @param viewer 文本显示器实例
 */
static void TextViewer_DrawScrollBar(TextViewer_T *viewer)
{
    if (viewer == NULL || !viewer->needRedrawScrollBar)
    {
        return;
    }

    /* ===== 使用宏 ===== */
    uint16_t StartY = viewer->config.titleHeight + ROW(0) + 4;
    uint16_t availableHeight = GUI_SCREEN_HEIGHT - StartY - viewer->config.statusBarHeight - 4;
    uint16_t scrollBarX = MENU_AREA_WIDTH - SCROLL_BAR_WIDTH - 2;

    if (viewer->state == TEXT_VIEWER_VIEWING && viewer->previewSize > 0)
    {
        uint16_t lineHeight = CHAR_HEIGHT + 4;
        uint16_t charWidth = CHAR_WIDTH;
        uint16_t maxCharsPerLine = (MENU_AREA_WIDTH - SCROLL_BAR_WIDTH - 10) / charWidth;

        /* 计算总行数 */
        uint32_t totalLines = 0;
        char *textPtr = (char *)viewer->previewBuffer;
        uint32_t remaining = viewer->previewSize;
        uint16_t charsInCurrentLine = 0;

        while (remaining > 0 && *textPtr != '\0')
        {
            if (*textPtr == '\n')
            {
                totalLines++;
                charsInCurrentLine = 0;
                textPtr++;
                remaining--;
            }
            else if (*textPtr == '\r')
            {
                textPtr++;
                remaining--;
            }
            else
            {
                uint8_t charBytes = TextViewer_GetCharWidth(textPtr, remaining);
                charsInCurrentLine++;

                if (charsInCurrentLine > maxCharsPerLine)
                {
                    totalLines++;
                    charsInCurrentLine = 1;
                }

                textPtr += charBytes;
                remaining -= charBytes;
            }
        }

        if (charsInCurrentLine > 0)
            totalLines++;

        if (totalLines == 0)
            totalLines = 1;

        uint16_t totalHeight = totalLines * lineHeight;
        uint16_t scrollBarHeight = (availableHeight * availableHeight) / totalHeight;
        uint16_t scrollBarPos = (viewer->previewScrollY * availableHeight) / totalHeight;

        if (scrollBarHeight < 5)
            scrollBarHeight = 5;

        if (scrollBarPos + scrollBarHeight > availableHeight)
            scrollBarPos = availableHeight - scrollBarHeight;

        GUI_LCD_FillColor(scrollBarX, StartY, SCROLL_BAR_WIDTH, availableHeight, GUI_GRAY);
        GUI_LCD_FillColor(scrollBarX, StartY + scrollBarPos, SCROLL_BAR_WIDTH, scrollBarHeight, GUI_WHITE);
    }
    else
    {
        GUI_LCD_FillColor(scrollBarX, StartY, SCROLL_BAR_WIDTH, availableHeight, GUI_BLACK);
    }

    viewer->needRedrawScrollBar = 0;
}

/**
 * @brief 绘制预览模式的动态内容
 * @param viewer 文本显示器实例
 */
static void TextViewer_DrawViewingDynamic(TextViewer_T *viewer)
{
    if (viewer == NULL)
    {
        return;
    }

    GUI_LCD_ShowString(COL(0), viewer->config.titleHeight + 3, viewer->fileName, GUI_YELLOW, GUI_BLACK);
    TextViewer_DrawTextPreview(viewer);
}

/**
 * @brief 绘制状态消息
 * @param viewer 文本显示器实例
 */
static void TextViewer_DrawStatusMessage(TextViewer_T *viewer)
{
    if (viewer == NULL)
    {
        return;
    }

    uint16_t statusBarY = GUI_SCREEN_HEIGHT - viewer->config.statusBarHeight;
    GUI_LCD_ShowString(COL(0), statusBarY + 3, viewer->statusMessage, GUI_WHITE, GUI_GRAY);
}

/**
 * @brief 绘制文本显示器
 * @param viewer 文本显示器实例
 */
void TextViewer_Draw(TextViewer_T *viewer)
{
    if (viewer == NULL)
    {
        return;
    }

    if (!viewer->needRedrawStatic && !viewer->needRedrawDynamic && !viewer->needRedrawScrollBar)
    {
        return;
    }

    if (viewer->state != viewer->lastState)
    {
        viewer->needRedrawStatic = 1;
        viewer->needRedrawDynamic = 1;
        viewer->lastState = viewer->state;

        if (viewer->state == TEXT_VIEWER_VIEWING)
        {
            uint16_t StartY = viewer->config.titleHeight + ROW(1) - 10;
            uint16_t availableHeight = GUI_SCREEN_HEIGHT - StartY - viewer->config.statusBarHeight;
            GUI_LCD_FillColor(COL(0), StartY, MENU_AREA_WIDTH, availableHeight, GUI_BLACK);
        }
    }

    if (viewer->needRedrawStatic)
    {
        TextViewer_DrawStaticContent(viewer);
    }

    if (viewer->needRedrawDynamic)
    {
        switch (viewer->state)
        {
        case TEXT_VIEWER_VIEWING:
            TextViewer_DrawViewingDynamic(viewer);
            break;
        case TEXT_VIEWER_ERROR:
            GUI_LCD_ShowString(COL(0), ROW(5) + 10, viewer->statusMessage, GUI_RED, GUI_BLACK);
            break;
        default:
            break;
        }
        viewer->needRedrawDynamic = 0;
    }

    TextViewer_DrawScrollBar(viewer);
    TextViewer_DrawStatusMessage(viewer);
}

/**
 * @brief 文本显示器主函数
 * @param sender 事件发送者
 * @param key 按键值
 * @param state 当前状态
 * @param InitFlag 初始化标志
 * @return 下一个状态
 */
uint16_t TextViewer_Function(GUI_FuncItem_T *sender, GUI_MenuParam_T *param)
{
    if (param->InitFlag != 0)
    {
        GUI_LCD_FillColor(COL(0), ROW(0), MENU_AREA_WIDTH, GUI_SCREEN_HEIGHT, GUI_BLACK);
        Key_ClearAll();
        currentDevice = param->baseUI;
        return 1;
    }

    TextViewer_Draw(&s_TextViewer);
    TextViewer_HandleKeyInput(&s_TextViewer);

    return param->userState;
}