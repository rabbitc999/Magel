#ifndef __BSP_SERVO_H
#define __BSP_SERVO_H

#include "bsp_system.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum __ServoSelect_T
{
    SERVO_1,
    SERVO_2,
    SERVO_COUNT
} ServoSelect_T;

void Servo_Init(void);
/* PCA9685 CH0..5 + TIM1 CH2N/CH3N; call after hardware initialization. */
void Servo_ReturnAllTo90(void);
void Servo_SetAngle(ServoSelect_T index, float angle);

extern float g_ServoAngle[SERVO_COUNT];

#ifdef __cplusplus
}
#endif

#endif
