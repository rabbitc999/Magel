#ifndef __BSP_TIMER_H
#define __BSP_TIMER_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void Timer_Init(void);
void DWT_DelayMs(uint32_t Delay);

uint32_t Timer_GetDiffTick(uint32_t startTick, uint32_t endTick);
uint32_t Timer_GetDiffMs(uint32_t startTick);
uint64_t Timer_GetDiffUs(uint64_t startTick);
uint64_t Timer_GetUsTicks(void);
void Timer_ClearUsTicks(void);

uint64_t Timer_GetTicks(void);
uint64_t Timer_GetTimestampMs(void);
void Timer_SetTimestampMs(uint64_t timestamp);

extern uint32_t g_SystemTick;

#ifdef __cplusplus
}
#endif
#endif
