#ifndef __BSP_ULTRASONIC_H
#define __BSP_ULTRASONIC_H

#include "bsp_system.h"
#include <stdint.h>

#include "stm32h7xx_hal.h"

#ifdef __cplusplus
extern "C" {
#endif

/************************ 全局变量声明 ************************/
extern float g_CS100A_FrontDistance, g_CS100A_BackDistance;  // 测量距离(单位: cm)

/************************ 函数声明 ************************/
void CS100A_Init(void);         // 传感器初始化
void CS100A_SendTrigger(void);  // 触发一次测距

#ifdef __cplusplus
}
#endif

#endif