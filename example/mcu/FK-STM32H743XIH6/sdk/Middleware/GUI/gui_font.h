#ifndef __GUI_FONT_H
#define __GUI_FONT_H

#include <stdint.h>

#include "gui_font_data.h"
#include "gui_painter_base.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 字体类型枚举 */
typedef enum __GUI_FontType_T
{
    GUI_FONT_TYPE_ASCII,  /* 24x12 ASCII字体 */
    GUI_FONT_TYPE_GB2312, /* 24x24 GB2312字体 */
    GUI_FONT_TYPE_MAX
} GUI_FontType_T;

/* 函数声明 */
int8_t GUI_FontInit(GUI_FontInfo_T *pFont, GUI_FontType_T FontType);
void GUI_FontGetBuffer(GUI_FontInfo_T *pFont, char ch[4], uint8_t **FontBuffer, uint16_t FontWidth, uint16_t FontHeight);
void GUI_FontBufferInit(GUI_FontInfo_T *pFont);
void GUI_FontDeinit(GUI_FontInfo_T *pFont);
int8_t GUI_FontInitAll(void);
GUI_FontInfo_T *GUI_FontGetInfo(GUI_FontType_T FontType);
void GUI_FontShowString(GUI_DeviceInfo_T *deviceInfo, GUI_FontType_T FontType, uint16_t x, uint16_t y, const char *str, uint32_t foreColor, uint32_t backColor);

/* 外部全局变量声明 */
extern GUI_FontInfo_T g_GUI_FontASCII_16;
extern GUI_FontInfo_T g_GUI_FontGB2312_16;

#ifdef __cplusplus
}
#endif

#endif /* __GUI_FONT_DATA_H */
