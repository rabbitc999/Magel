#ifndef __BSP_BATTERY_H
#define __BSP_BATTERY_H

#include "bsp_system.h"

#ifdef __cplusplus
extern "C" {
#endif

void Battery_Init(void);
float Battery_GetVoltage(void);
uint8_t Battery_GetPercent(float voltage);

#ifdef __cplusplus
}
#endif

#endif