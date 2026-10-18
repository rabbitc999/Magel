#include "bsp_servo.h"
#include "bsp_periph.h"
#include "bsp_pca9685.h"
#include <math.h>

#define SERVO_LIMIT(angle, Min, Max) ((angle) < (Min) ? (Min) : (((angle) > (Max)) ? (Max) : (angle)))

/* 舵机角度数组 */
float g_ServoAngle[SERVO_COUNT] = { 0 };

/* TIM8句柄 */
TIM_HandleTypeDef s_ServoHandle;

/**
 * @brief  舵机初始化（配置TIM1 PWM输出，并将两路舵机归位到90度）
 */
void Servo_Init(void)
{
#ifdef CAR_BRINGUP_NO_ACTUATION
    return; /* Startup verification: do not output servo pulses. */
#endif
    TIM_MasterConfigTypeDef sMasterConfig = { 0 };
    TIM_OC_InitTypeDef sConfigOC = { 0 };
    TIM_BreakDeadTimeConfigTypeDef sBreakDeadTimeConfig = { 0 };
    TIM_ClockConfigTypeDef sClockSourceConfig = { 0 };
    GPIO_InitTypeDef GPIO_InitStruct = { 0 };

    __HAL_RCC_GPIOB_CLK_ENABLE();

    GPIO_InitStruct.Pin = GPIO_PIN_15 | GPIO_PIN_14;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    GPIO_InitStruct.Alternate = GPIO_AF1_TIM1;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    // 舵机直连STM32内部
    BSP_TIM_ClkEnable(TIM1);
    s_ServoHandle.Instance = TIM1;
    s_ServoHandle.Init.Prescaler = 240 - 1;
    s_ServoHandle.Init.CounterMode = TIM_COUNTERMODE_UP;
    s_ServoHandle.Init.Period = 20000 - 1;
    s_ServoHandle.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    s_ServoHandle.Init.RepetitionCounter = 0;
    s_ServoHandle.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
    HAL_TIM_PWM_Init(&s_ServoHandle);

    /* 定时器时钟源配置 */
    sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
    HAL_TIM_ConfigClockSource(&s_ServoHandle, &sClockSourceConfig);
    if (HAL_TIM_PWM_Init(&s_ServoHandle) != HAL_OK)
    {
        Assert_Failed(__FILE__, __LINE__);
    }

    //    /* 定时器主模式配置 */
    //    sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
    //    sMasterConfig.MasterOutputTrigger2 = TIM_TRGO2_RESET;
    //    sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
    //    HAL_TIMEx_MasterConfigSynchronization(&s_ServoHandle, &sMasterConfig);

    /* 定时器输出比较配置 */
    sConfigOC.Pulse = 1500; /* Start both direct servo outputs at 90 degrees. */
    sConfigOC.OCMode = TIM_OCMODE_PWM1;
    sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
    sConfigOC.OCNPolarity = TIM_OCNPOLARITY_HIGH;
    sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
    sConfigOC.OCIdleState = TIM_OCNIDLESTATE_RESET;
    sConfigOC.OCNIdleState = TIM_OCNIDLESTATE_RESET;

    HAL_TIM_PWM_ConfigChannel(&s_ServoHandle, &sConfigOC, TIM_CHANNEL_2);
    HAL_TIM_PWM_ConfigChannel(&s_ServoHandle, &sConfigOC, TIM_CHANNEL_3);

    //    sBreakDeadTimeConfig.OffStateRunMode = TIM_OSSR_DISABLE;
    //    sBreakDeadTimeConfig.OffStateIDLEMode = TIM_OSSI_DISABLE;
    //    sBreakDeadTimeConfig.LockLevel = TIM_LOCKLEVEL_OFF;
    //    sBreakDeadTimeConfig.DeadTime = 0;
    //    sBreakDeadTimeConfig.BreakState = TIM_BREAK_DISABLE;
    //    sBreakDeadTimeConfig.BreakPolarity = TIM_BREAKPOLARITY_HIGH;
    //    sBreakDeadTimeConfig.BreakFilter = 0;
    //    sBreakDeadTimeConfig.Break2State = TIM_BREAK2_DISABLE;
    //    sBreakDeadTimeConfig.Break2Polarity = TIM_BREAK2POLARITY_HIGH;
    //    sBreakDeadTimeConfig.Break2Filter = 0;
    //    sBreakDeadTimeConfig.AutomaticOutput = TIM_AUTOMATICOUTPUT_DISABLE;
    //    HAL_TIMEx_ConfigBreakDeadTime(&s_ServoHandle, &sBreakDeadTimeConfig);

    if (HAL_TIMEx_PWMN_Start(&s_ServoHandle, TIM_CHANNEL_2) != HAL_OK)
    {
        Assert_Failed(__FILE__, __LINE__);
    }
    if (HAL_TIMEx_PWMN_Start(&s_ServoHandle, TIM_CHANNEL_3) != HAL_OK)
    {
        Assert_Failed(__FILE__, __LINE__);
    }

    Servo_ReturnAllTo90();
}

/**
 * @brief  设置舵机角度
 * @param  index 舵机编号（SERVO_1或SERVO_2）
 * @param  angle 目标角度，范围为0-180度，超出则自动限幅
 */
void Servo_SetAngle(ServoSelect_T index, float angle)
{
#ifdef CAR_BRINGUP_NO_ACTUATION
    return; /* Startup verification: do not output servo pulses. */
#endif
    if ((unsigned)index >= SERVO_COUNT || !isfinite(angle)) { return; }
    angle = SERVO_LIMIT(angle, 0, 180);
    uint16_t pwm = 2000 * angle / 180.0f + 500;
    g_ServoAngle[index] = angle;

    if (index == SERVO_1) { TIM1->CCR2 = pwm; }
    if (index == SERVO_2) { TIM1->CCR3 = pwm; }
}

/**
 * @brief 全部已使用的8路舵机回到90度（中位脉宽1500us）。
 * PCA9685仅使用0~5通道，共6路；另2路为STM32 TIM1的
 * CH2N/PB14（SERVO_1）和CH3N/PB15（SERVO_2）。
 * PCA9685的6~15通道不在归中范围内，8、9通道为灯光输出。
 * 必须在PCA9685_Init和TIM1 PWM初始化完成后调用。
 * 按当前PCA9685预分频值换算1500us，不修改共享PWM频率。
 * 归中期间应停止其他舵机控制任务，避免后续命令覆盖中位。
 * 90度为500~2500us映射中的软件中位，机械中位需实物校准。
 */
void Servo_ReturnAllTo90(void)
{
#ifdef CAR_BRINGUP_NO_ACTUATION
    return;
#endif
    uint8_t prescale = PCA9685_Read(0xFE);
    if (prescale < 3) { return; }
    uint32_t ticks = (37500U + (prescale + 1U) / 2U) / (prescale + 1U);
    for (uint8_t channel = 0; channel < 6; ++channel)
    {
        PCA9685_SetPWM(channel, 0, ticks);
    }
    Servo_SetAngle(SERVO_1, 90.0f);
    Servo_SetAngle(SERVO_2, 90.0f);
}
