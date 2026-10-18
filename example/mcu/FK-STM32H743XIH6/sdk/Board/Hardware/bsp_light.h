#ifndef __BSP_LIGHT_H
#define __BSP_LIGHT_H

#include <stdint.h>

#define LIGHT_BACK_LEFT_ID  0
#define LIGHT_BACK_RIGHT_ID 1

#ifdef __cplusplus
extern "C" {
#endif

void Light_Init(void);
void Light_SetBrightness(uint8_t brightness);
void Light_TurnOff(void);

extern uint8_t g_LightBrightness;

#ifdef __cplusplus
}
#endif

#endif /* __BSP_LIGHT_H */
