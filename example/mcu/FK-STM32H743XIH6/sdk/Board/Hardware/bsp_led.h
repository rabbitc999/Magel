#ifndef __BSP_LED_H
#define __BSP_LED_H

#include "bsp_system.h"

#ifdef __cplusplus
extern "C" {
#endif

void LED_Init(void);
void LED1_ON(void);
void LED1_OFF(void);
void LED1_Turn(void);
void LED2_ON(void);
void LED2_OFF(void);
void LED2_Turn(void);

#ifdef __cplusplus
}
#endif

#endif
