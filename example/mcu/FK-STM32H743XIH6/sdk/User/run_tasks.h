#ifndef __FREERTOS_TASK_H
#define __FREERTOS_TASK_H

#include "bsp_system.h"
#include "cmd.h"
#include "imu.h"
#include "car.h"
#include "store.h"

#include "fast_math.h"
#include "expression.h"
#include "condition_task.h"
#include "algorithm.h"

/******************************************************************************************************/
/* 消息结构体定义 */
typedef enum __NRF24L01_SetMode_T
{
    NRF24L01_SET_MODE_SERVO,
    NRF24L01_SET_MODE_LIGHT,
    NRF24L01_SET_MODE_SPEED,
    NRF24L01_SET_MODE_COUNT
} NRF24L01_SetMode_T;

void ProgramTask_Init(void);
extern volatile float g_FrontUltraSonicDistance, g_BackUltraSonicDistance;

#endif
