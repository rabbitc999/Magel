#include "gui.h"

extern GUI_DeviceInfo_T g_LCD_DeviceInfo;

uint16_t GUI_MenuGetStringLineUp(GUI_Device_T *baseUi, uint16_t line);
uint16_t GUI_MenuGetCharOffset(GUI_Device_T *baseUi);
uint16_t GUI_MenuGetScrollBarWidth(GUI_Device_T *baseUi);

/* ========== 基础绘图函数 ========== */

void GUI_LCD_DrawPoint(uint16_t x, uint16_t y, uint16_t color)
{
    GUI_LCD_DrawPointAdapter(&g_LCD_DeviceInfo, x, y, color);
}

void GUI_LCD_DrawLine(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color)
{
    GUI_BasePainter_DrawLine(&g_LCD_DeviceInfo, x1, y1, x2, y2, color);
}

void GUI_LCD_DrawCircle(uint16_t x, uint16_t y, uint16_t radius, uint16_t color)
{
    GUI_BasePainter_DrawCircle(&g_LCD_DeviceInfo, x, y, radius, color);
}

void GUI_LCD_DrawBuffer(uint16_t x, uint16_t y, uint16_t Width, uint16_t Height, uint16_t *buffer)
{
    GUI_LCD_DrawBufferAdapter(&g_LCD_DeviceInfo, x, y, (uint8_t *)buffer, Width, Height);
}

/* ========== 菜单相关绘图函数 ========== */

void GUI_MenuShowString(GUI_Device_T *baseUi, uint16_t x, uint16_t line, const char *string, uint16_t fontColor, uint16_t BlackColor)
{
    GUI_LCD_SetShowFont(GUI_FONT_TYPE_GB2312);
    uint16_t StartX, StartY, Width, Height;
    StartX = baseUi->menuPainter.StartX + x;
    StartY = baseUi->menuPainter.StartY + GUI_MenuGetStringLineUp(baseUi, line) + GUI_MenuGetCharOffset(baseUi);
    Width = String_GetLength(string, baseUi->menuPainter.Width / baseUi->menuPainter.charWidth) * baseUi->menuPainter.charWidth;
    Height = baseUi->menuPainter.charHeight;
    GUI_LCD_FillColor(StartX, StartY + Height - 1, Width, 1, BlackColor);
    GUI_LCD_ShowString(StartX, StartY, string, fontColor, BlackColor);
}

void GUI_MenuShowInt(GUI_Device_T *baseUi, uint16_t x, uint16_t line, int32_t number, uint16_t fontColor, uint16_t BlackColor)
{
    static char temp[9];
    sprintf(temp, "%8d", number);
    GUI_MenuShowString(baseUi, baseUi->menuPainter.StartX + x, line, temp, fontColor, BlackColor);
}

void GUI_MenuShowFloat(GUI_Device_T *baseUi, uint16_t x, uint16_t line, float number, uint16_t fontColor, uint16_t BlackColor)
{
    static char temp[9];
    sprintf(temp, "%8.2f", number);
    GUI_MenuShowString(baseUi, baseUi->menuPainter.StartX + x, line, temp, fontColor, BlackColor);
}

/**
 * @brief 绘制菜单项的背景矩形 — 关键修改，宽度使用 MENU_AREA_WIDTH
 */
void GUI_LCD_MenuDrawRectangle(GUI_Device_T *baseUi, uint16_t line, uint16_t color)
{
    uint16_t x = baseUi->menuPainter.StartX;
    uint16_t y = baseUi->menuPainter.StartY + GUI_MenuGetStringLineUp(baseUi, line);
    uint16_t Width = MENU_AREA_WIDTH;  // 改用宏，不再用 baseUi->menuPainter.Width
    if (line >= 1)
    {
        Width -= GUI_MenuGetScrollBarWidth(baseUi);
    }
    uint16_t Height = baseUi->menuPainter.lineInterval;
    GUI_LCD_FillAdapter(&g_LCD_DeviceInfo, x, y, Width, Height, color);
}

/**
 * @brief 清除菜单显示区域 — 关键修改，尺寸使用宏
 */
void GUI_LCD_MenuClearShow(GUI_Device_T *baseUi)
{
    GUI_LCD_FillAdapter(&g_LCD_DeviceInfo,
                        baseUi->menuPainter.StartX,
                        baseUi->menuPainter.StartY,
                        MENU_AREA_WIDTH,
                        GUI_SCREEN_HEIGHT,
                        GUI_BLACK);
}

/* ========== 扩展绘图函数 ========== */

void GUI_LCD_DrawThickLine(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint8_t thickness, uint16_t color)
{
    GUI_BasePainter_DrawThickLine(&g_LCD_DeviceInfo, x0, y0, x1, y1, thickness, color);
}

void GUI_LCD_FillColor(uint16_t x, uint16_t y, uint16_t Width, uint16_t Height, uint16_t color)
{
    GUI_BasePainter_Fill(&g_LCD_DeviceInfo, x, y, Width, Height, color);
}

void GUI_LCD_DrawFilledCircle(uint16_t x, uint16_t y, uint8_t radius, uint16_t color)
{
    GUI_BasePainter_DrawCircle(&g_LCD_DeviceInfo, x, y, radius, color);
}

void GUI_LCD_DrawEllipse(uint16_t x, uint16_t y, uint8_t a, uint8_t b, uint16_t color)
{
    GUI_BasePainter_DrawEllipse(&g_LCD_DeviceInfo, x, y, a, b, color);
}

void GUI_LCD_DrawTriangle(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color)
{
    GUI_BasePainter_DrawTriangle(&g_LCD_DeviceInfo, x0, y0, x1, y1, x2, y2, color);
}

/* ========== 兼容性函数 ========== */

void GUI_MenuClearShow(GUI_Device_T *baseUi)
{
    GUI_LCD_MenuClearShow(baseUi);
}

void GUI_MenuDrawRectangle(GUI_Device_T *baseUi, uint16_t line, uint16_t color)
{
    GUI_LCD_MenuDrawRectangle(baseUi, line, color);
}

void GUI_MenuDrawRectangleEx(GUI_Device_T *baseUi, uint16_t x, uint16_t y, uint16_t Width, uint16_t Height, uint16_t color)
{
    if (baseUi == NULL) return;
    GUI_LCD_FillAdapter(&g_LCD_DeviceInfo, x, y, Width, Height, color);
}

void GUI_DrawPoint(uint16_t x, uint16_t y, uint16_t color)
{
    GUI_LCD_DrawPoint(x, y, color);
}

void GUI_DrawLine(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color)
{
    GUI_LCD_DrawLine(x1, y1, x2, y2, color);
}

void GUI_DrawCircle(uint16_t x, uint16_t y, uint16_t radius, uint16_t color)
{
    GUI_LCD_DrawCircle(x, y, radius, color);
}

void GUI_DrawBuffer(uint16_t x, uint16_t y, uint16_t Width, uint16_t Height, uint16_t *buffer)
{
    GUI_LCD_DrawBuffer(x, y, Width, Height, buffer);
}

/* ========== 字体显示函数 ========== */

void GUI_LCD_ShowString(uint16_t x, uint16_t y, const char *str, uint16_t foreColor, uint16_t backColor)
{
    GUI_BasePainter_ShowString(&g_LCD_DeviceInfo, x, y, str, foreColor, backColor);
}

void GUI_LCD_ShowStringBlock(uint16_t x, uint16_t y, uint16_t Width, uint16_t Height, const char *str, uint16_t foreColor, uint16_t backColor)
{
    GUI_BasePainter_ShowStringBlock(&g_LCD_DeviceInfo, x, y, Width, Height, str, foreColor, backColor);
}

void GUI_LCD_ShowNumber(uint16_t x, uint16_t y, int32_t number, uint8_t length, uint16_t foreColor, uint16_t backColor, uint8_t showSign)
{
    GUI_BasePainter_ShowNumber(&g_LCD_DeviceInfo, x, y, number, length, foreColor, backColor, showSign);
}

void GUI_LCD_ShowHexNumber(uint16_t x, uint16_t y, uint32_t number, uint8_t length, uint16_t foreColor, uint16_t backColor)
{
    GUI_BasePainter_ShowHexNumber(&g_LCD_DeviceInfo, x, y, number, length, foreColor, backColor);
}

void GUI_LCD_ShowBinNumber(uint16_t x, uint16_t y, uint32_t number, uint8_t length, uint16_t foreColor, uint16_t backColor)
{
    GUI_BasePainter_ShowBinNumber(&g_LCD_DeviceInfo, x, y, number, length, foreColor, backColor);
}

void GUI_LCD_ShowFloatNumber(uint16_t x, uint16_t y, double number, uint8_t intLength, uint8_t fraLength, uint16_t foreColor, uint16_t backColor, uint8_t showSign)
{
    GUI_BasePainter_ShowFloatNumber(&g_LCD_DeviceInfo, x, y, number, intLength, fraLength, foreColor, backColor, showSign);
}

void GUI_LCD_Printf(uint16_t x, uint16_t y, uint16_t foreColor, uint16_t backColor, const char *format, ...)
{
    va_list args;
    va_start(args, format);
    static char buffer[256];
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    GUI_BasePainter_ShowString(&g_LCD_DeviceInfo, x, y, buffer, foreColor, backColor);
}

void GUI_LCD_PrintfBlock(uint16_t x, uint16_t y, uint16_t Width, uint16_t Height, uint16_t foreColor, uint16_t backColor, const char *format, ...)
{
    va_list args;
    va_start(args, format);
    static char buffer[256];
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    GUI_BasePainter_ShowStringBlock(&g_LCD_DeviceInfo, x, y, Width, Height, buffer, foreColor, backColor);
}

void GUI_LCD_ShowProgress(uint16_t x, uint16_t y, uint16_t Width, uint16_t Height, uint8_t progress, uint16_t foreColor, uint16_t backColor, uint8_t showPercent)
{
    GUI_BasePainter_ShowProgress(&g_LCD_DeviceInfo, x, y, Width, Height, progress, foreColor, backColor, showPercent);
}

void GUI_LCD_SetShowFontInfoPtr(GUI_FontInfo_T *pFont)
{
    g_LCD_DeviceInfo.showFont = pFont;
}

void GUI_LCD_SetShowFont(GUI_FontType_T pFont)
{
    GUI_FontInfo_T *fontPtr = GUI_FontGetInfo(pFont);
    g_LCD_DeviceInfo.showFont = fontPtr;
}

void GUI_LCD_SetFillMode(GUI_PainterFillMode_T mode)
{
    g_LCD_DeviceInfo.fillMode = mode;
}