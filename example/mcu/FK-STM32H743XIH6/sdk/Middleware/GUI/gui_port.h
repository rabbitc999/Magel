#ifndef __GUI_PORT_H
#define __GUI_PORT_H

#include <math.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdarg.h>

#include "bsp_uart.h"

#ifndef __STATIC_INLINE
#define __STATIC_INLINE static inline
#endif

/* 布尔标志 */
#ifndef TRUE
#define TRUE  1
#define FALSE 0
#endif

/* ========== 小屏尺寸配置 (320x240) ========== */
#define GUI_SCREEN_WIDTH                         240
#define GUI_SCREEN_HEIGHT                        320

#define CHAR_WIDTH                               8
#define CHAR_HEIGHT                              16
#define LINE_SPACING                             20

#define TITLE_HEIGHT                             20
#define STATUS_HEIGHT                            20
#define SCROLL_BAR_WIDTH                         2

#define MENU_AREA_WIDTH                          GUI_SCREEN_WIDTH
#define MENU_AREA_HEIGHT                         GUI_SCREEN_HEIGHT

#define CHAR_OFFSET                              ((LINE_SPACING - CHAR_HEIGHT) / 2)

/* ========== 行列转换宏 ========== */
#define B_ROW(y)                                 ((LINE_SPACING) * (y))
#define ROW(y)                                   ((LINE_SPACING) * (y) + (CHAR_OFFSET))
#define COL(x)                                   (CHAR_WIDTH * (x))

/* ========== 菜单相关宏 ========== */
#define GUI_MENU_EXIT_ITEM_TITLE_STRING          "返回上一级.."
#define GUI_MENU_DEFALUT_VALUE_ITEM_TITLE_STRING "设置默认值.."

#define GUI_MENU_REMOVE_ITEM_SUCCESS             0
#define GUI_MENU_REMOVE_ITEM_FAIL                1
#define GUI_MENU_REMOVE_GUI_MENU_SUCCESS         0
#define GUI_MENU_REMOVE_GUI_MENU_FAIL            1

#define GUI_USE_DEFAULT_FONT                     g_GUI_Font16
#define GUI_INTERVAL                             LINE_SPACING
#define GUI_HEIGHT                               GUI_SCREEN_HEIGHT
#define GUI_WIDTH                                MENU_AREA_WIDTH

#define GUI_MENU_CHECK_BIT(x)                    (0x01 << (x))
#define GUI_MENU_IS_CHECKED(x, bit)              (((x) & GUI_MENU_CHECK_BIT(bit)) != 0)
#define GUI_MENU_LIMIT(x, Min, Max)              ((x) < (Min) ? (Min) : ((x) > (Max) ? (Max) : (x)))
#define GUI_MENU_IS_OVERFLOW(x, Min, Max)        ((x) < (Min) ? (TRUE) : ((x) > (Max) ? (TRUE) : (FALSE)))
#define GUI_MENU_ASSERT_TYPE(x, mask, type)      (((x) & (mask)) == (type))
#define GUI_MAX(x, y)                            ((x) > (y) ? (x) : (y))
#define GUI_MIN(x, y)                            ((x) < (y) ? (x) : (y))
#define GUI_GET_ARRAY_COUNT(x)                   (sizeof(x) / sizeof(x[0]))
#define GUI_DEBUG_PRINTF(format, ...)            DEBUG_PRINTF("File %s(%d): " format "\r\n", __FILE__, __LINE__, __VA_ARGS__)
#define GUI_ASSERT_WITH_RETURN(test, ret_val)            \
    if (!(test))                                         \
    {                                                    \
        GUI_DEBUG_PRINTF("Assertion failed: %s", #test); \
        return (ret_val);                                \
    }
#define GUI_ASSERT(test)                                 \
    if (!(test))                                         \
    {                                                    \
        GUI_DEBUG_PRINTF("Assertion failed: %s", #test); \
        return;                                          \
    }

#ifdef __cplusplus
extern "C" {
#endif

void GUI_DrawPoint(uint16_t x, uint16_t y, uint16_t color);
void GUI_DrawLine(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color);
void GUI_DrawCircle(uint16_t x, uint16_t y, uint16_t radius, uint16_t color);
void GUI_DrawBuffer(uint16_t x, uint16_t y, uint16_t Width, uint16_t Height, uint16_t *buffer);

typedef struct __GUI_Device_T GUI_Device_T;

void GUI_MenuDrawRectangle(GUI_Device_T *baseUi, uint16_t line, uint16_t color);
void GUI_MenuShowInt(GUI_Device_T *baseUi, uint16_t x, uint16_t y, int32_t number, uint16_t fontColor, uint16_t BlackColor);
void GUI_MenuShowFloat(GUI_Device_T *baseUi, uint16_t x, uint16_t y, float number, uint16_t fontColor, uint16_t BlackColor);
void GUI_MenuShowString(GUI_Device_T *baseUi, uint16_t x, uint16_t y, const char *string, uint16_t fontColor, uint16_t BlackColor);
void GUI_MenuClearShow(GUI_Device_T *baseUi);
void GUI_MenuKeyBasicOperator(GUI_Device_T *baseUi);

void *GUI_MenuMalloc(size_t size);
void GUI_MenuFree(void *block);
uint32_t GUI_MenuGetMallocUsed(void);
uint32_t GUI_MenuGetMallocTotal(void);

uint32_t GUI_GetTick(void);
uint32_t GUI_GetDiffMs(uint32_t current);

void GUI_Init(void);
void GUI_Loop(void);

__STATIC_INLINE uint16_t GUI_SwapBytes(uint16_t num)
{
    return (num << 8) | (num >> 8);
}

#ifdef __cplusplus
}
#endif

#endif