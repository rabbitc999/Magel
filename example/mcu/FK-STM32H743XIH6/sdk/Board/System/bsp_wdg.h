#ifndef __BSP_WDG_H
#define __BSP_WDG_H

#include "bsp_system.h"

void IWDG_Init(uint32_t prer, uint16_t rlr); /* 初始化IWDG，并使能IWDG */
void IWDG_Feed(void);                        /* 喂狗 */

extern uint32_t g_CPU_Serial[3];

#endif
