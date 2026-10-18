#ifndef __BSP_LCD_H
#define __BSP_LCD_H

#include "bsp_system.h"
#include "bsp_lcd_data.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* RGB565格式颜色定义 (16位) */
#define LCD_WHITE         0xFFFF
#define LCD_BLACK         0x0000
#define LCD_BLUE          0x001F
#define LCD_BRED          0XF81F
#define LCD_RED           0xF800
#define LCD_MAGENTA       0xF81F
#define LCD_GREEN         0x07E0
#define LCD_CYAN          0x7FFF
#define LCD_YELLOW        0xFFE0
#define LCD_BROWN         0XBC40 /* 棕色 */
#define LCD_GRAY          0X8430 /* 灰色 */
#define LCD_DARK_BLUE     0X01CF /* 深蓝色 */
#define LCD_LIGHT_BLUE    0X7D7C /* 浅蓝色 */
#define LCD_GRAY_BLUE     0X5458 /* 灰蓝色 */
#define LCD_LIGHT_GREEN   0X841F /* 浅绿色 */
#define LCD_LIGHT_RED     0XF810 /* 浅红色 - 248,8,0 */
#define LCD_LIGHT_YELLOW  0XFFF0 /* 浅黄色 - 255,255,0 */
#define LCD_LIGHT_MAGENTA 0XF81F /* 浅洋红色 - 248,8,255 */
#define LCD_LIGHT_CYAN    0X07FF /* 浅青色 - 0,255,255 */

/* 定义LCD方向枚举 */
typedef enum __LCD_Direction_T
{
    LCD_HORIZONTAL = 0,     /* 水平 */
    LCD_REV_HORIZONTAL = 1, /* 水平反转 */
    LCD_VERTICAL = 2,       /* 垂直 */
    LCD_REV_VERTICAL = 3    /* 垂直反转 */
} LCD_Direction_T;

// 定义LCD方向枚举
typedef enum __LCD_FontSize_T
{
    LCD_FontSize12 = 12,  // 1206
    LCD_FontSize16 = 16,  // 1608
    LCD_FontSize24 = 24,  // 2412
    LCD_FontSize32 = 32,  // 3216
} LCD_FontSize_T;

// 定义LCD绘图方式
typedef enum __LCD_MonoFill_T
{
    LCD_Cover = 0,        // 覆盖模式
    LCD_Transparent = 1,  // 透明模式
} LCD_MonoFill_T;

/* 函数声明 */
void LCD_Init(void);
extern void (*LCD_SetDirection)(LCD_Direction_T Direction);
extern void (*LCD_DrawPoint)(uint16_t x, uint16_t y, uint16_t color);
extern void (*LCD_ShowImage)(uint16_t x, uint16_t y, uint16_t Width, uint16_t Height, const uint8_t *image);
extern void (*LCD_Fill)(uint16_t StartX, uint16_t StartY, uint16_t Width, uint16_t Height, uint16_t color);
extern void (*LCD_ChangeMemoryAccess)(void);
extern void (*LCD_ShowMonoImage)(uint16_t X, uint16_t Y, uint16_t Width, uint16_t Height, const uint8_t *Image, uint16_t FontColor, uint16_t BlackColor);
extern void (*LCD_ON)(void);
extern void (*LCD_OFF)(void);

void LCD_RGB_DrawPoint(uint16_t x, uint16_t y, uint16_t color);
void LCD_RGB_ShowImage(uint16_t x, uint16_t y, uint16_t Width, uint16_t Height, const uint8_t *image);
void LCD_RGB_Fill(uint16_t StartX, uint16_t StartY, uint16_t Width, uint16_t Height, uint16_t color);

// 显示函数
void LCD_SetFontSize(LCD_FontSize_T Size);
void LCD_SetMonoFill(LCD_MonoFill_T Fill);
void LCD_ShowChar(uint16_t X, uint16_t Y, uint8_t ShowChar, uint16_t FontColor, uint16_t BlackColor);
void LCD_ShowString(uint16_t X, uint16_t Y, const char *String, uint16_t FontColor, uint16_t BlackColor);
void LCD_ShowNumber(uint16_t X, uint16_t Y, int32_t Number, uint8_t Length, uint16_t FontColor, uint16_t BackColor, uint8_t ShowSign);
void LCD_ShowHexNumber(int16_t X, int16_t Y, uint32_t Number, uint8_t Length, uint16_t FontColor, uint16_t BlackColor);
void LCD_ShowBinNumber(int16_t X, int16_t Y, uint32_t Number, uint8_t Length, uint16_t FontColor, uint16_t BlackColor);
void LCD_ShowFloatNumber(int16_t X, int16_t Y, double Number, uint8_t IntLength, uint8_t FraLength, uint16_t FontColor, uint16_t BackColor, uint8_t showSign);
void LCD_Printf(uint16_t X, uint16_t Y, uint16_t FontColor, uint16_t BlackColor, const char *String, ...);

// 绘图函数
void LCD_DrawLine(uint16_t X0, uint16_t Y0, uint16_t X1, uint16_t Y1, uint16_t Color);
void LCD_DrawRectangle(uint16_t X, uint16_t Y, uint16_t Width, uint16_t Height, uint16_t Color);
void LCD_DrawTriangle(uint16_t X0, uint16_t Y0, uint16_t X1, uint16_t Y1, uint16_t X2, uint16_t Y2, uint16_t Color);
void LCD_DrawPoly(uint16_t vx[], uint16_t vy[], int n, uint16_t Color);
void LCD_DrawCircle(uint16_t x, uint16_t y, uint16_t radius, uint16_t color);
void LCD_DrawEllipse(uint16_t X, uint16_t Y, uint16_t A, uint16_t B, uint16_t Color);
void LCD_DrawArc(uint16_t X, uint16_t Y, uint8_t Radius, int16_t StartAngle, int16_t EndAngle, uint16_t Color);
void LCD_DrawThickLine(uint16_t X0, uint16_t Y0, uint16_t X1, uint16_t Y1, uint16_t thickness, uint16_t Color);

// DMA2D绘制函数
void DMA2D_MemCopy(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint32_t *src_ptr, uint32_t *dst_ptr, uint16_t lcd_width);
void DMA2D_MemFill(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color, uint32_t *dst_ptr, uint16_t lcd_width);

/* 定义一个结构体，用于存储LCD设备的相关参数 */
typedef struct __LCD_T
{
    volatile LCD_Direction_T Direction; /* 显示方向 */
    volatile LCD_FontSize_T FontSize;   // 字体大小
    volatile LCD_MonoFill_T MonoFill;   // 单色覆盖
    volatile uint16_t Height;           /* 显示高度 */
    volatile uint16_t Width;            /* 显示宽度 */
    volatile uint8_t FinishFlag;        /* 发送完成标志 */
    volatile uint8_t TftFlag;           /* 是否为TFT屏幕 */

    SPI_HandleTypeDef SPI; /* SPI_HandleTypeDef 结构体变量 */
    DMA_HandleTypeDef DMA; /* DMA_HandleTypeDef 结构体变量 */
} LCD_T;

extern LCD_T g_LCD;

#ifdef __cplusplus
}
#endif

#endif /* _LCD_H */
