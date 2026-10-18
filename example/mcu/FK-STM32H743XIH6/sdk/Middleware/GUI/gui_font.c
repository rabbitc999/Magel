#include "gui.h"
#include <string.h>

/* 外部字体信息结构体定义 */
GUI_FontInfo_T g_GUI_FontASCII_16;
GUI_FontInfo_T g_GUI_FontGB2312_16;

#define GUI_USE_ASCII_DEFAULT_FONT  g_GUI_FontASCII_16
#define GUI_USE_GB2312_DEFAULT_FONT g_GUI_FontGB2312_16
#define FONT_MASK_NUMBER            0x24242424

static uint8_t font_buffer[72], init_flag = 0;

static const uint8_t no_font[72] = {
    0x00, 0x00, 0x00, 0xFE, 0xFF, 0x7F, 0xFE, 0xFF, 0x7F, 0x06, 0x00, 0x60,
    0x06, 0x7E, 0x60, 0x06, 0xFF, 0x60, 0x86, 0xFF, 0x61, 0xC6, 0xC1, 0x61,
    0xC6, 0xC1, 0x61, 0xC6, 0xC0, 0x61, 0x06, 0xE0, 0x61, 0x06, 0xF0, 0x60,
    0x06, 0x78, 0x60, 0x06, 0x3C, 0x60, 0x06, 0x1C, 0x60, 0x06, 0x1C, 0x60,
    0x06, 0x00, 0x60, 0x06, 0x1C, 0x60, 0x06, 0x1C, 0x60, 0x06, 0x1C, 0x60,
    0x06, 0x00, 0x60, 0xFE, 0xFF, 0x7F, 0xFE, 0xFF, 0x7F, 0x00, 0x00, 0x00
};

/**
 * @brief 根据字体类型获取GB2312字体信息
 * @param FontType 字体类型
 * @return GB2312字体信息指针
 */
static GUI_GB2312Font_T *GUI_GetGb2312Font(GUI_FontType_T FontType)
{
    switch (FontType)
    {
    case GUI_FONT_TYPE_GB2312: return &g_GUI_Font16;
    default: return NULL;
    }
}

/**
 * @brief 根据字体类型获取ASCII字体信息
 * @param FontType 字体类型
 * @param Width 输出参数，返回字符宽度
 * @param Height 输出参数，返回字符高度
 * @return ASCII字体数据指针
 */
static const void *GUI_GetAsciiFont(GUI_FontType_T FontType, uint16_t *Width, uint16_t *Height, uint16_t *fontsize)
{
    switch (FontType)
    {
    case GUI_FONT_TYPE_ASCII:
        *Width = 8;
        *Height = 16;
        *fontsize = 16;
        return LCD_F16X08;

    default:
        *Width = 0;
        *Height = 0;
        *fontsize = 0;
        return NULL;
    }
}

/**
 * @brief 获取GB2312字符点阵数据
 * @param pFont GB2312字体信息指针
 * @param code GB2312编码
 * @param buffer 输出缓冲区
 * @return 获取结果：0=成功，非0=失败
 */
static uint8_t *GUI_GetGb2312CharData(GUI_GB2312Font_T *pFont, uint16_t code)
{
    if (code > 0xFEFE)
    {
        Memory_Copy(font_buffer, no_font, sizeof(font_buffer));
        return font_buffer;
    }

    if (init_flag) { goto load_buffer; }

    uint32_t mask = 0;
    if (W25QXX_ReadBuffer((uint8_t *)&mask, QSPI_ADDR_CHINESE_FONT, 4) != W25QXX_OK || mask != FONT_MASK_NUMBER)
    {
        // 从外部文件读取
        uint32_t file_size = FatFS_GetFileSize(GUI_USE_DEFAULT_FONT.filePath);
        if (file_size == 0) goto error;

        uint32_t *buffer = (uint32_t *)Memory_Malloc(MP_RAM_EX, file_size + 4);
        if (buffer == NULL) goto error;

        while (FatFS_ReadFile(GUI_USE_DEFAULT_FONT.filePath, (uint8_t *)buffer + 4, file_size) != file_size);

        buffer[0] = FONT_MASK_NUMBER;

        LCD_ShowString(0, 3, "Load Chinese Font!!!", LCD_WHITE, LCD_BLACK);
        if (W25QXX_WriteBuffer((uint8_t *)buffer, QSPI_ADDR_CHINESE_FONT, file_size + 4) != W25QXX_OK)
        {
            RTC_WriteBKP(0x33241234);
            HAL_Delay(500);
            NVIC_SystemReset();
        }

        Memory_Free(MP_RAM_EX, buffer);
    }
    init_flag = 1;

load_buffer:
    {
        /* 计算偏移地址 */
        uint8_t gb2312_h = code >> 8;
        uint8_t gb2312_l = code & 0x00FF;
        uint32_t offset = ((gb2312_h - 0xa1) * 94 + gb2312_l - 0xa1) * pFont->singleSize;

        // 读取数据到 font_buffer
        if (W25QXX_ReadBuffer(font_buffer, QSPI_ADDR_CHINESE_FONT + offset + 4, sizeof(font_buffer)) != W25QXX_OK)
        {
            goto error;
        }
        return font_buffer;
    }
error:
    {
        RTC_WriteBKP(0x33241234);
        HAL_Delay(500);
        NVIC_SystemReset();
        return NULL;
    }
}

/**
 * @brief 初始化字体信息结构体
 * @param pFont 字体信息结构体指针
 * @param FontType 字体类型
 * @return 初始化结果：0=成功，非0=失败
 */
int8_t GUI_FontInit(GUI_FontInfo_T *pFont, GUI_FontType_T FontType)
{
    if (pFont == NULL)
    {
        return -1;
    }

    /* 清零结构体 */
    Memory_Set(pFont, 0, sizeof(GUI_FontInfo_T));

    /* 设置回调函数 */
    pFont->FontBufferInit = GUI_FontBufferInit;
    pFont->getFontBuffer = GUI_FontGetBuffer;

    /* 根据字体类型设置参数 */
    switch (FontType)
    {
    case GUI_FONT_TYPE_ASCII:
        pFont->FontHeight = 16;
        pFont->asciiFontWidth = 8;
        pFont->otherFontWidth = 16;
        pFont->encoding = GUI_FONT_ENCODING_ASCII;
        break;

    case GUI_FONT_TYPE_GB2312:
        pFont->FontHeight = 16;
        pFont->asciiFontWidth = 8;
        pFont->otherFontWidth = 16;
        pFont->encoding = GUI_FONT_ENCODING_GB2312;
        break;

    default:
        return -2;
    }

    /* 保存字体类型到tagInfo */
    pFont->tagInfo = (void *)(uintptr_t)FontType;

    return 0;
}

/**
 * @brief 字体缓冲区初始化回调函数
 * @param pFont 字体信息结构体指针
 */
void GUI_FontBufferInit(GUI_FontInfo_T *pFont)
{
    if (pFont == NULL || pFont->tagInfo == NULL)
    {
        return;
    }

    pFont->InitFlag = 1;
}

/**
 * @brief 获取字体点阵数据回调函数
 * @param pFont 字体信息结构体指针
 * @param ch 字符数据（支持多字节字符）
 * @param FontBuffer 输出参数，返回字符点阵数据缓冲区指针
 * @param FontWidth 字符宽度
 * @param FontHeight 字符高度
 */
void GUI_FontGetBuffer(GUI_FontInfo_T *pFont, char ch[4], uint8_t **FontBuffer, uint16_t FontWidth, uint16_t FontHeight)
{
    if (pFont == NULL || FontBuffer == NULL || ch == NULL)
    {
        return;
    }

    GUI_FontType_T FontType = (GUI_FontType_T)(uintptr_t)pFont->tagInfo;

    /* ASCII字体处理 */
    if (FontType == GUI_FONT_TYPE_ASCII)
    {
        uint16_t Width, Height, fontsize;
        const void *asciiFont = GUI_GetAsciiFont(FontType, &Width, &Height, &fontsize);

        if (asciiFont && (uint8_t)ch[0] >= ' ' && (uint8_t)ch[0] <= '~')
        {
            uint8_t charIndex = (uint8_t)ch[0] - ' ';
            *FontBuffer = (uint8_t *)asciiFont + charIndex * Height;
            return;
        }
    }

    /* GB2312字体处理 */
    if (FontType == GUI_FONT_TYPE_GB2312)
    {
        /* 判断是否为ASCII字符 */
        if ((uint8_t)ch[0] < 0x80)
        {
            /* ASCII字符，使用对应的ASCII字体 */
            GUI_FontType_T asciiType;
            switch (FontType)
            {
            case GUI_FONT_TYPE_GB2312: asciiType = GUI_FONT_TYPE_ASCII; break;
            default: return;
            }

            uint16_t Width, Height, fontsize;
            const void *asciiFont = GUI_GetAsciiFont(asciiType, &Width, &Height, &fontsize);
            if (asciiFont && (uint8_t)ch[0] >= ' ' && (uint8_t)ch[0] <= '~')
            {
                uint8_t charIndex = (uint8_t)ch[0] - ' ';
                *FontBuffer = (uint8_t *)asciiFont + charIndex * fontsize;
                return;
            }
        }
        else
        {
            /* GB2312中文字符 */
            uint16_t code = ((uint8_t)ch[0] << 8) | (uint8_t)ch[1];
            *FontBuffer = GUI_GetGb2312CharData(&GUI_USE_DEFAULT_FONT, code);
            return;
        }
    }

    /* 如果都失败，返回空 */
    *FontBuffer = NULL;
}

/**
 * @brief 释放字体资源
 * @param pFont 字体信息结构体指针
 */
void GUI_FontDeinit(GUI_FontInfo_T *pFont)
{
    if (pFont == NULL)
    {
        return;
    }

    pFont->InitFlag = 0;
}

/* ==================== 公共接口函数实现 ==================== */

/**
 * @brief 初始化所有字体（一次性完成）
 * @return 初始化结果：0=成功，非0=失败
 */
int8_t GUI_FontInitAll(void)
{
    int8_t result = 0;

    /* 初始化ASCII字体 */
    result |= GUI_FontInit(&GUI_USE_ASCII_DEFAULT_FONT, GUI_FONT_TYPE_ASCII);

    /* 初始化GB2312字体 */
    result |= GUI_FontInit(&GUI_USE_GB2312_DEFAULT_FONT, GUI_FONT_TYPE_GB2312);

    return result;
}

/**
 * @brief 根据字体类型获取字体信息结构体
 * @param FontType 字体类型
 * @return 字体信息结构体指针
 */
GUI_FontInfo_T *GUI_FontGetInfo(GUI_FontType_T FontType)
{
    switch (FontType)
    {
    case GUI_FONT_TYPE_ASCII: return &GUI_USE_ASCII_DEFAULT_FONT;
    case GUI_FONT_TYPE_GB2312: return &GUI_USE_GB2312_DEFAULT_FONT;
    default: return NULL;
    }
}

/**
 * @brief 释放所有字体资源
 */
void GUI_FontDeinitAll(void)
{
    GUI_FontInfo_T *fonts[] = { &GUI_USE_ASCII_DEFAULT_FONT, &GUI_USE_GB2312_DEFAULT_FONT };

    for (uint32_t i = 0; i < sizeof(fonts) / sizeof(fonts[0]); i++)
    {
        fonts[i]->InitFlag = 0;
    }
}
