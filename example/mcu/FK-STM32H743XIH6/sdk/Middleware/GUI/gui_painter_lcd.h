#ifndef __GUI_LCD_PAINTER_H
#define __GUI_LCD_PAINTER_H

#include <stdint.h>
#include "gui.h"
#include "gui_painter_base.h"
#include "gui_font.h"

#ifdef __cplusplus
extern "C" {
#endif

/* LCD设备信息结构体 */
extern GUI_DeviceInfo_T g_LCD_DeviceInfo;

/* 适配器函数原型 */
void GUI_LCD_DrawPointAdapter(GUI_DeviceInfo_T *deviceInfo, uint16_t x, uint16_t y, uint32_t color);
void GUI_LCD_FillAdapter(GUI_DeviceInfo_T *deviceInfo, uint16_t x, uint16_t y, uint16_t Width, uint16_t Height, uint32_t color);
void GUI_LCD_DrawBufferAdapter(GUI_DeviceInfo_T *deviceInfo, uint16_t x, uint16_t y, const uint8_t *buffer, uint16_t Width, uint16_t Height);

/* 基础绘图函数 */
void GUI_LCD_DrawPoint(uint16_t x, uint16_t y, uint16_t color);
void GUI_LCD_DrawLine(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color);
void GUI_LCD_DrawCircle(uint16_t x, uint16_t y, uint16_t radius, uint16_t color);
void GUI_LCD_DrawBuffer(uint16_t x, uint16_t y, uint16_t Width, uint16_t Height, uint16_t *buffer);

/* 菜单相关绘图函数 */
typedef struct __GUI_Device_T GUI_Device_T;
void GUI_LCD_MenuDrawRectangle(GUI_Device_T *baseUi, uint16_t line, uint16_t color);
void GUI_LCD_MenuClearShow(GUI_Device_T *baseUi);

/* 扩展的绘图函数（利用gui_painter的强大功能） */
void GUI_LCD_DrawThickLine(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint8_t thickness, uint16_t color);
void GUI_LCD_FillColor(uint16_t x, uint16_t y, uint16_t Width, uint16_t Height, uint16_t color);
void GUI_LCD_DrawFilledCircle(uint16_t x, uint16_t y, uint8_t radius, uint16_t color);
void GUI_LCD_DrawEllipse(uint16_t x, uint16_t y, uint8_t a, uint8_t b, uint16_t color);
void GUI_LCD_DrawTriangle(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color);

/* 兼容性函数 */
void GUI_MenuClearShow(GUI_Device_T *baseUi);
void GUI_MenuDrawRectangle(GUI_Device_T *baseUi, uint16_t line, uint16_t color);
void GUI_MenuDrawRectangleEx(GUI_Device_T *baseUi, uint16_t x, uint16_t y, uint16_t Width, uint16_t Height, uint16_t color);
void GUI_DrawPoint(uint16_t x, uint16_t y, uint16_t color);
void GUI_DrawLine(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color);
void GUI_DrawCircle(uint16_t x, uint16_t y, uint16_t radius, uint16_t color);
void GUI_DrawBuffer(uint16_t x, uint16_t y, uint16_t Width, uint16_t Height, uint16_t *buffer);

/* GUI字符显示函数 */
void GUI_LCD_ShowString(uint16_t x, uint16_t y, const char *str, uint16_t foreColor, uint16_t backColor);
void GUI_LCD_ShowStringBlock(uint16_t x, uint16_t y, uint16_t Width, uint16_t Height, const char *str, uint16_t foreColor, uint16_t backColor);
void GUI_LCD_ShowNumber(uint16_t x, uint16_t y, int32_t number, uint8_t length, uint16_t foreColor, uint16_t backColor, uint8_t showSign);
void GUI_LCD_ShowHexNumber(uint16_t x, uint16_t y, uint32_t number, uint8_t length, uint16_t foreColor, uint16_t backColor);
void GUI_LCD_ShowBinNumber(uint16_t x, uint16_t y, uint32_t number, uint8_t length, uint16_t foreColor, uint16_t backColor);
void GUI_LCD_ShowFloatNumber(uint16_t x, uint16_t y, double number, uint8_t intLength, uint8_t fraLength, uint16_t foreColor, uint16_t backColor, uint8_t showSign);
void GUI_LCD_Printf(uint16_t x, uint16_t y, uint16_t foreColor, uint16_t backColor, const char *format, ...);
void GUI_LCD_PrintfBlock(uint16_t x, uint16_t y, uint16_t Width, uint16_t Height, uint16_t foreColor, uint16_t backColor, const char *format, ...);
void GUI_LCD_ShowProgress(uint16_t x, uint16_t y, uint16_t Width, uint16_t Height, uint8_t progress, uint16_t foreColor, uint16_t backColor, uint8_t showPercent);

void GUI_LCD_SetShowFontInfoPtr(GUI_FontInfo_T *pFont);
void GUI_LCD_SetShowFont(GUI_FontType_T pFont);
void GUI_LCD_SetFillMode(GUI_PainterFillMode_T mode);

#ifdef __cplusplus
}
#endif

#endif /* __GUI_LCD_PAINTER_H */
