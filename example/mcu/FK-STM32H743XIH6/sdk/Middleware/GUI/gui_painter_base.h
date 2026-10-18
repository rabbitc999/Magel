#ifndef GUI_PAINTER_H
#define GUI_PAINTER_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* RGB565格式颜色定义 (16位) */
#define GUI_WHITE         0xFFFF
#define GUI_BLACK         0x0000
#define GUI_BLUE          0x001F
#define GUI_BRED          0XF81F
#define GUI_RED           0xF800
#define GUI_MAGENTA       0xF81F
#define GUI_GREEN         0x07E0
#define GUI_CYAN          0x7FFF
#define GUI_YELLOW        0xFFE0
#define GUI_BROWN         0XBC40 /* 棕色 */
#define GUI_GRAY          0X8430 /* 灰色 */
#define GUI_DARK_BLUE     0X01CF /* 深蓝色 */
#define GUI_LIGHT_BLUE    0X7D7C /* 浅蓝色 */
#define GUI_GRAY_BLUE     0X5458 /* 灰蓝色 */
#define GUI_LIGHT_GREEN   0X841F /* 浅绿色 */
#define GUI_LIGHT_RED     0XF810 /* 浅红色 - 248,8,0 */
#define GUI_LIGHT_YELLOW  0XFFF0 /* 浅黄色 - 255,255,0 */
#define GUI_LIGHT_MAGENTA 0XF81F /* 浅洋红色 - 248,8,255 */
#define GUI_LIGHT_CYAN    0X07FF /* 浅青色 - 0,255,255 */

/* 填充模式枚举 */
typedef enum __GUI_PainterFillMode_T
{
    GUI_PAINTER_FILL_NONE = 0, /* 不填充（仅绘制边框） */
    GUI_PAINTER_FILL_COLOR     /* 填充颜色（绘制实心图形） */
} GUI_PainterFillMode_T;

/* 字体编码枚举 */
typedef enum __GUI_FontEncoding_T
{
    GUI_FONT_ENCODING_ASCII = 0,  /* ASCII编码：标准英文字符集，每个字符占1字节 */
    GUI_FONT_ENCODING_GB2312 = 1, /* GB2312编码：简体中文字符集，中文占2字节 */
    GUI_FONT_ENCODING_UTF8 = 2    /* UTF8编码：通用字符编码，中文通常占3字节 */
} GUI_FontEncoding_T;

/* 前向声明 */
typedef struct __GUI_DeviceInfo_T GUI_DeviceInfo_T;
typedef struct __GUI_FontInfo_T GUI_FontInfo_T;

/* GUI设备信息结构体 */
struct __GUI_DeviceInfo_T
{
    void (*deviceInit)(GUI_DeviceInfo_T *deviceInfo);
    void (*drawBufferCallback)(GUI_DeviceInfo_T *deviceInfo, uint16_t x, uint16_t y, const uint8_t *buffer, uint16_t Width, uint16_t Height);
    void (*drawMonoImageCallback)(GUI_DeviceInfo_T *deviceInfo, uint16_t x, uint16_t y, const uint8_t *buffer, uint16_t Width, uint16_t Height, uint32_t foreColor, uint32_t backColor);
    void (*drawPointCallback)(GUI_DeviceInfo_T *deviceInfo, uint16_t x, uint16_t y, uint32_t color);
    void (*fillColorCallback)(GUI_DeviceInfo_T *deviceInfo, uint16_t x, uint16_t y, uint16_t Width, uint16_t Height, uint32_t color);

    uint8_t InitFlag;
    uint8_t swapFlag;
    uint32_t Width;
    uint32_t Height;
    GUI_FontInfo_T *showFont;
    GUI_PainterFillMode_T fillMode;
    void *tagInfo;
};

/* GUI字体信息结构体 */
struct __GUI_FontInfo_T
{
    void (*FontBufferInit)(GUI_FontInfo_T *pFont);
    void (*getFontBuffer)(GUI_FontInfo_T *pFont, char ch[4], uint8_t **FontBuffer, uint16_t FontWidth, uint16_t FontHeight);

    uint8_t InitFlag;
    uint16_t FontHeight;
    uint16_t asciiFontWidth;
    uint16_t otherFontWidth;
    GUI_FontEncoding_T encoding;
    void *tagInfo;
};

/* 基础图形绘制函数 */
void GUI_BasePainter_DrawLine(GUI_DeviceInfo_T *deviceInfo, uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint32_t color);
void GUI_BasePainter_DrawThickLine(GUI_DeviceInfo_T *deviceInfo, uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint8_t thickness, uint32_t color);
void GUI_BasePainter_DrawRectangle(GUI_DeviceInfo_T *deviceInfo, uint16_t x, uint16_t y, uint16_t Width, uint16_t Height, uint32_t color);
void GUI_BasePainter_DrawSquare(GUI_DeviceInfo_T *deviceInfo, uint16_t x, uint16_t y, uint16_t size, uint32_t color);
void GUI_BasePainter_Fill(GUI_DeviceInfo_T *deviceInfo, uint16_t x, uint16_t y, uint16_t Width, uint16_t Height, uint32_t color);

/* 圆形/椭圆绘制函数 */
void GUI_BasePainter_DrawCircle(GUI_DeviceInfo_T *deviceInfo, uint16_t x, uint16_t y, uint8_t radius, uint32_t color);
void GUI_BasePainter_DrawThickCircle(GUI_DeviceInfo_T *deviceInfo, uint16_t x, uint16_t y, uint8_t radius, uint8_t thickness, uint32_t color);
void GUI_BasePainter_DrawEllipse(GUI_DeviceInfo_T *deviceInfo, uint16_t x, uint16_t y, uint8_t a, uint8_t b, uint32_t color);
void GUI_BasePainter_DrawThickEllipse(GUI_DeviceInfo_T *deviceInfo, uint16_t x, uint16_t y, uint8_t a, uint8_t b, uint8_t thickness, uint32_t color);

/* 圆弧绘制函数 */
void GUI_BasePainter_DrawArc(GUI_DeviceInfo_T *deviceInfo, uint16_t x, uint16_t y, uint8_t radius, int16_t startAngle, int16_t endAngle, uint32_t color);
void GUI_BasePainter_DrawThickArc(GUI_DeviceInfo_T *deviceInfo, uint16_t x, uint16_t y, uint8_t radius, uint8_t thickness, int16_t startAngle, int16_t endAngle, uint32_t color);

/* 图像显示函数 */
void GUI_BasePainter_ShowMonoImage(GUI_DeviceInfo_T *deviceInfo, uint16_t x, uint16_t y, uint16_t Width, uint16_t Height, const uint8_t *image, uint32_t foreColor, uint32_t backColor);
void GUI_BasePainter_ShowColorImage(GUI_DeviceInfo_T *deviceInfo, uint16_t x, uint16_t y, uint16_t Width, uint16_t Height, const uint8_t *image);

/* 多边形绘制函数 */
void GUI_BasePainter_DrawTriangle(GUI_DeviceInfo_T *deviceInfo, uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint32_t color);
void GUI_BasePainter_DrawPoly(GUI_DeviceInfo_T *deviceInfo, uint16_t vx[], uint16_t vy[], int n, uint32_t color);

/* 字体绘制函数 */
void GUI_BasePainter_DrawChar(GUI_DeviceInfo_T *deviceInfo, uint16_t x, uint16_t y, char ch[4], uint32_t foreColor, uint32_t backColor);
void GUI_BasePainter_ShowString(GUI_DeviceInfo_T *deviceInfo, uint16_t x, uint16_t y, const char *str, uint32_t foreColor, uint32_t backColor);
void GUI_BasePainter_ShowStringBlock(GUI_DeviceInfo_T *deviceInfo, uint16_t x, uint16_t y, uint16_t Width, uint16_t Height, const char *str, uint32_t foreColor, uint32_t backColor);
void GUI_BasePainter_ShowNumber(GUI_DeviceInfo_T *deviceInfo, uint16_t x, uint16_t y, int32_t number, uint8_t length, uint32_t foreColor, uint32_t backColor, uint8_t showSign);
void GUI_BasePainter_ShowHexNumber(GUI_DeviceInfo_T *deviceInfo, uint16_t x, uint16_t y, uint32_t number, uint8_t length, uint32_t foreColor, uint32_t backColor);
void GUI_BasePainter_ShowBinNumber(GUI_DeviceInfo_T *deviceInfo, uint16_t x, uint16_t y, uint32_t number, uint8_t length, uint32_t foreColor, uint32_t backColor);
void GUI_BasePainter_ShowFloatNumber(GUI_DeviceInfo_T *deviceInfo, uint16_t x, uint16_t y, double number, uint8_t intLength, uint8_t fraLength, uint32_t foreColor, uint32_t backColor, uint8_t showSign);
void GUI_BasePainter_Printf(GUI_DeviceInfo_T *deviceInfo, uint16_t x, uint16_t y, uint32_t foreColor, uint32_t backColor, const char *format, ...);
void GUI_BasePainter_PrintfBlock(GUI_DeviceInfo_T *deviceInfo, uint16_t x, uint16_t y, uint16_t Width, uint16_t Height, uint32_t foreColor, uint32_t backColor, const char *format, ...);
void GUI_BasePainter_ShowProgress(GUI_DeviceInfo_T *deviceInfo, uint16_t x, uint16_t y, uint16_t Width, uint16_t Height, uint8_t progress, uint32_t foreColor, uint32_t backColor, uint8_t showPercent);

/* 设置模式 */
void GUI_BasePainter_SetShowFont(GUI_DeviceInfo_T *deviceInfo, GUI_FontInfo_T *pFont);
void GUI_BasePainter_SetFillMode(GUI_DeviceInfo_T *deviceInfo, GUI_PainterFillMode_T mode);

#ifdef __cplusplus
}
#endif

#endif /* GUI_PAINTER_H */
