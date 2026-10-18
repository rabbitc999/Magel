#ifndef __BSP_PWR_H
#define __BSP_PWR_H

#include "stm32h7xx_hal.h"

#ifdef __cplusplus
extern "C" {
#endif

void PWR_RecordTime(void);
uint8_t PWR_IsAlarm(void);
void PWR_SetPowerDownTime(uint32_t WaitToPowerDownTime);

#ifdef __cplusplus
}
#endif

#endif /* _BSP_DWT_H */