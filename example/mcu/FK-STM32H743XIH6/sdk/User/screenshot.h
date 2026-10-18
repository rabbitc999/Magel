#ifndef __SCREENSHOT_H
#define __SCREENSHOT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SCREEN_SELECT_MAIN_WINDOWS   0
#define SCREEN_SELECT_OLED_WINDOWS_1 1
#define SCREEN_SELECT_OLED_WINDOWS_2 2

uint32_t Screenshot_Run(char *filename, uint32_t filenameSize, uint8_t selected);
void Screenshot_BufferDrawBuffer(uint16_t x, uint16_t y, const uint8_t *buffer, uint16_t Width, uint16_t Height);
void Screenshot_BufferFillColor(uint16_t x, uint16_t y, uint16_t Width, uint16_t Height, uint32_t color);
void Screenshot_BufferDrawPoint(uint16_t x, uint16_t y, uint32_t color);
void Screenshot_OLED_BufferDrawBuffer(uint16_t x, uint16_t y, const uint8_t *buffer, uint16_t Width, uint16_t Height);
void Screenshot_OLED_BufferFillColor(uint16_t x, uint16_t y, uint16_t Width, uint16_t Height, uint32_t color);
void Screenshot_OLED_BufferDrawPoint(uint16_t x, uint16_t y, uint32_t color);

#ifdef __cplusplus
}
#endif

#endif /* MOTOR_CONTROL_H */
