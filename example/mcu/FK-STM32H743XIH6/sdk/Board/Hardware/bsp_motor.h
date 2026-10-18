#ifndef __BSP_MOTOR_H
#define __BSP_MOTOR_H

#include "bsp_system.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum __MotorSelect_T
{
    MOTOR_A,
    MOTOR_B,
    MOTOR_C,
    MOTOR_D,
    MOTOR_COUNT
} MotorSelect_T;

// 电机控制模式枚举
typedef enum __MotorMode_T
{
    MOTOR_MODE_UNIPOLAR_NO_EN,    // 不带使能的单极性控制 (DIR + PWM)
    MOTOR_MODE_UNIPOLAR_WITH_EN,  // 带使能的单极性控制 (DIR + EN + PWM)
    MOTOR_MODE_DUAL_INPUT,        // 双输入控制/TB6612模式 (IN1/DIR + IN2/EN + PWM)
    MOTOR_MODE_BIPOLAR            // 双极性控制 (EN使能 + PWM, DIR无效)
} MotorMode_T;

extern uint8_t g_MotorEnableFlag;

void Motor_Init(void);
void Motor_SetPWM(MotorSelect_T index, int16_t pwm);
void Motor_Stop(void);

void Motor_SetMode(MotorMode_T mode);
void Motor_Enable(MotorSelect_T index);
void Motor_Disable(MotorSelect_T index);

#ifdef __cplusplus
}
#endif

#endif