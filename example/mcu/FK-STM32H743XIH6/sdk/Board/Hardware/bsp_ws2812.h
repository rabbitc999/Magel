#ifndef __BSP_WS2812_H
#define __BSP_WS2812_H

#include "bsp_system.h"

#include <stdio.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void WS2812_Init(void);
void WS2812_Update(void);
void WS2812_SetRGB(uint8_t led_id, uint8_t r, uint8_t g, uint8_t b);
void WS2812_Set(uint8_t led_id, uint32_t color);
void WS2812_SetAll(uint32_t color);

#ifdef __cplusplus
}
#endif

#endif