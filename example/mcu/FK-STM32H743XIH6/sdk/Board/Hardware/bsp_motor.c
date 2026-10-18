#include "bsp_motor.h"
#include "bsp_periph.h"

/* 电机引脚配置宏 */
#define MOTOR_TIM           TIM8
#define MOTOR_TIM_AF        GPIO_AF3_TIM8 /* 复用功能 */

/* 电机独立PWM通道/引脚配置宏 */
#define MOTOR_A_PWM_CHANNEL TIM_CHANNEL_1
#define MOTOR_A_PWM_PORT    GPIOB
#define MOTOR_A_PWM_PIN     GPIO_PIN_1

#define MOTOR_B_PWM_CHANNEL TIM_CHANNEL_2
#define MOTOR_B_PWM_PORT    GPIOC
#define MOTOR_B_PWM_PIN     GPIO_PIN_6

#define MOTOR_C_PWM_CHANNEL TIM_CHANNEL_3
#define MOTOR_C_PWM_PORT    GPIOC
#define MOTOR_C_PWM_PIN     GPIO_PIN_7

#define MOTOR_D_PWM_CHANNEL TIM_CHANNEL_4
#define MOTOR_D_PWM_PORT    GPIOI
#define MOTOR_D_PWM_PIN     GPIO_PIN_2

/* 电机方向控制引脚宏 */
#define MOTOR_A_DIR_PORT    GPIOB       // MOTOR_A_IN1 (TB6612，下述同理)
#define MOTOR_A_DIR_PIN     GPIO_PIN_0  // MOTOR_A_IN1
#define MOTOR_B_DIR_PORT    GPIOC       // MOTOR_B_IN1
#define MOTOR_B_DIR_PIN     GPIO_PIN_5  // MOTOR_B_IN2
#define MOTOR_C_DIR_PORT    GPIOA       // MOTOR_C_IN1
#define MOTOR_C_DIR_PIN     GPIO_PIN_4  // MOTOR_C_IN1
#define MOTOR_D_DIR_PORT    GPIOA       // MOTOR_D_IN1
#define MOTOR_D_DIR_PIN     GPIO_PIN_3  // MOTOR_D_IN1
#define MOTOR_A_EN_PORT     GPIOI       // MOTOR_A_IN2
#define MOTOR_A_EN_PIN      GPIO_PIN_0  // MOTOR_A_IN2
#define MOTOR_B_EN_PORT     GPIOI       // MOTOR_B_IN1
#define MOTOR_B_EN_PIN      GPIO_PIN_4  // MOTOR_B_IN1
#define MOTOR_C_EN_PORT     GPIOI       // MOTOR_C_IN2
#define MOTOR_C_EN_PIN      GPIO_PIN_5  // MOTOR_C_IN2
#define MOTOR_D_EN_PORT     GPIOI       // MOTOR_D_IN2
#define MOTOR_D_EN_PIN      GPIO_PIN_6  // MOTOR_D_IN2

/* 定时器句柄结构体 */
static TIM_HandleTypeDef s_TimMotorHandle;

/* 用于记录电机的控制模式 */
static MotorMode_T s_MotorModes = MOTOR_MODE_UNIPOLAR_NO_EN;

/* GPIO数组 */
static GPIO_TypeDef *const s_MotorDirPorts[4] = { MOTOR_A_DIR_PORT, MOTOR_B_DIR_PORT, MOTOR_C_DIR_PORT, MOTOR_D_DIR_PORT };
static uint16_t const s_MotorDirPins[4] = { MOTOR_A_DIR_PIN, MOTOR_B_DIR_PIN, MOTOR_C_DIR_PIN, MOTOR_D_DIR_PIN };
static GPIO_TypeDef *const s_MotorEnPorts[4] = { MOTOR_A_EN_PORT, MOTOR_B_EN_PORT, MOTOR_C_EN_PORT, MOTOR_D_EN_PORT };
static uint16_t const s_MotorEnPins[4] = { MOTOR_A_EN_PIN, MOTOR_B_EN_PIN, MOTOR_C_EN_PIN, MOTOR_D_EN_PIN };

/**
 * @brief  初始化直流电机相关的GPIO和定时器
 */
void Motor_Init(void)
{
    /* 开启相关外设时钟 */
    BSP_TIM_ClkEnable(MOTOR_TIM);

    TIM_ClockConfigTypeDef sClockSourceConfig = { 0 };
    TIM_OC_InitTypeDef sConfigOC = { 0 };

    /* 配置GPIO控制引脚(用于电机方向或电机使能控制) */
    BSP_GPIO_Init(MOTOR_A_DIR_PORT, MOTOR_A_DIR_PIN, OUT_PP);
    BSP_GPIO_Init(MOTOR_B_DIR_PORT, MOTOR_B_DIR_PIN, OUT_PP);
    BSP_GPIO_Init(MOTOR_C_DIR_PORT, MOTOR_C_DIR_PIN, OUT_PP);
    BSP_GPIO_Init(MOTOR_D_DIR_PORT, MOTOR_D_DIR_PIN, OUT_PP);
    BSP_GPIO_Init(MOTOR_A_EN_PORT, MOTOR_A_EN_PIN, OUT_PP);
    BSP_GPIO_Init(MOTOR_B_EN_PORT, MOTOR_B_EN_PIN, OUT_PP);
    BSP_GPIO_Init(MOTOR_C_EN_PORT, MOTOR_C_EN_PIN, OUT_PP);
    BSP_GPIO_Init(MOTOR_D_EN_PORT, MOTOR_D_EN_PIN, OUT_PP);

    /* PWM输出引脚配置 */
    BSP_GPIO_InitEx(MOTOR_A_PWM_PORT, MOTOR_A_PWM_PIN, OUT_AF_PP, MOTOR_TIM_AF);
    BSP_GPIO_InitEx(MOTOR_B_PWM_PORT, MOTOR_B_PWM_PIN, OUT_AF_PP, MOTOR_TIM_AF);
    BSP_GPIO_InitEx(MOTOR_C_PWM_PORT, MOTOR_C_PWM_PIN, OUT_AF_PP, MOTOR_TIM_AF);
    BSP_GPIO_InitEx(MOTOR_D_PWM_PORT, MOTOR_D_PWM_PIN, OUT_AF_PP, MOTOR_TIM_AF);

    /* 定时器配置 */
    s_TimMotorHandle.Instance = MOTOR_TIM;
    s_TimMotorHandle.Init.Prescaler = 12 - 1;
    s_TimMotorHandle.Init.Period = 1000 - 1;
    s_TimMotorHandle.Init.CounterMode = TIM_COUNTERMODE_UP;
    s_TimMotorHandle.Init.RepetitionCounter = 0;
    s_TimMotorHandle.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    s_TimMotorHandle.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
    s_TimMotorHandle.Init.CounterMode = TIM_COUNTERMODE_UP;
    if (HAL_TIM_Base_Init(&s_TimMotorHandle) != HAL_OK)
    {
        Assert_Failed(__FILE__, __LINE__);
    }

    /* 定时器时钟源配置 */
    sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
    HAL_TIM_ConfigClockSource(&s_TimMotorHandle, &sClockSourceConfig);
    if (HAL_TIM_PWM_Init(&s_TimMotorHandle) != HAL_OK)
    {
        Assert_Failed(__FILE__, __LINE__);
    }

    /* 定时器输出比较配置 */
    sConfigOC.OCMode = TIM_OCMODE_PWM1;
    sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
    sConfigOC.OCNPolarity = TIM_OCNPOLARITY_HIGH;
    sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
    sConfigOC.OCIdleState = TIM_OCNIDLESTATE_RESET;
    sConfigOC.OCNIdleState = TIM_OCNIDLESTATE_RESET;
    sConfigOC.Pulse = 0;

    if (HAL_TIM_PWM_ConfigChannel(&s_TimMotorHandle, &sConfigOC, TIM_CHANNEL_1) != HAL_OK)
    {
        Assert_Failed(__FILE__, __LINE__);
    }
    if (HAL_TIM_PWM_ConfigChannel(&s_TimMotorHandle, &sConfigOC, TIM_CHANNEL_2) != HAL_OK)
    {
        Assert_Failed(__FILE__, __LINE__);
    }
    if (HAL_TIM_PWM_ConfigChannel(&s_TimMotorHandle, &sConfigOC, TIM_CHANNEL_3) != HAL_OK)
    {
        Assert_Failed(__FILE__, __LINE__);
    }
    if (HAL_TIM_PWM_ConfigChannel(&s_TimMotorHandle, &sConfigOC, TIM_CHANNEL_4) != HAL_OK)
    {
        Assert_Failed(__FILE__, __LINE__);
    }

    /* 使能PWM输出 */
    if (HAL_TIM_PWM_Start(&s_TimMotorHandle, TIM_CHANNEL_1) != HAL_OK)
    {
        Assert_Failed(__FILE__, __LINE__);
    }
    if (HAL_TIM_PWM_Start(&s_TimMotorHandle, TIM_CHANNEL_2) != HAL_OK)
    {
        Assert_Failed(__FILE__, __LINE__);
    }
    if (HAL_TIMEx_PWMN_Start(&s_TimMotorHandle, TIM_CHANNEL_3) != HAL_OK)
    {
        Assert_Failed(__FILE__, __LINE__);
    }
    if (HAL_TIM_PWM_Start(&s_TimMotorHandle, TIM_CHANNEL_4) != HAL_OK)
    {
        Assert_Failed(__FILE__, __LINE__);
    }

    /* 将所有电机设置为安全的停止/失能状态 */
    Motor_Stop();
    Motor_Disable(MOTOR_A);
    Motor_Disable(MOTOR_B);
    Motor_Disable(MOTOR_C);
    Motor_Disable(MOTOR_D);
}

#define ABS(x) ((x) > 0 ? (x) : -(x))

/**
 * @brief  设置电机的PWM值（内部函数）
 * @param  index 电机索引
 * @param  pwm PWM值，范围为-1000到1000
 */
static void __Motor_SetPWM(MotorSelect_T index, int16_t pwm)
{
#ifdef CAR_BRINGUP_NO_ACTUATION
    pwm = 0;
#endif
    switch (index)
    {
    case MOTOR_A:
    {
        MOTOR_TIM->CCR1 = ABS(pwm);
        break;
    }
    case MOTOR_B:
    {
        MOTOR_TIM->CCR2 = ABS(pwm);
        break;
    }
    case MOTOR_C:
    {
        MOTOR_TIM->CCR3 = ABS(pwm);
        break;
    }
    case MOTOR_D:
    {
        MOTOR_TIM->CCR4 = ABS(pwm);
        break;
    }
    default:
        break;
    }
}

/**
 * @brief  设置电机的PWM值
 * @param  index 电机索引
 * @param  pwm PWM值，范围为-1000到1000
 */
void Motor_SetPWM(MotorSelect_T index, int16_t pwm)
{
    if (index >= MOTOR_COUNT) return;

    uint16_t abs_speed = ABS(pwm);
    uint8_t is_reverse = (pwm < 0);

    switch (s_MotorModes)
    {
    case MOTOR_MODE_UNIPOLAR_NO_EN:
    {
        // 不带使能单极性：DIR决定方向，PWM调速
        BSP_GPIO_WritePin(s_MotorDirPorts[index], s_MotorDirPins[index], is_reverse);
        __Motor_SetPWM(index, abs_speed);
        break;
    }
    case MOTOR_MODE_UNIPOLAR_WITH_EN:
    {
        // 带使能单极性：DIR决定方向，EN拉高使能，PWM调速
        BSP_GPIO_WritePin(s_MotorDirPorts[index], s_MotorDirPins[index], is_reverse);
        __Motor_SetPWM(index, abs_speed);
        break;
    }
    case MOTOR_MODE_DUAL_INPUT:
    {
        // 双输入/TB6612模式：IN1(DIR)和IN2(EN)组合控制正反转，PWM调速
        // 正转: IN1=1, IN2=0 | 反转: IN1=0, IN2=1
        if (is_reverse)
        {
            BSP_GPIO_WritePin(s_MotorDirPorts[index], s_MotorDirPins[index], 0);  // IN1 = 0
            BSP_GPIO_WritePin(s_MotorEnPorts[index], s_MotorEnPins[index], 1);    // IN2 = 1
        }
        else
        {
            BSP_GPIO_WritePin(s_MotorDirPorts[index], s_MotorDirPins[index], 1);  // IN1 = 1
            BSP_GPIO_WritePin(s_MotorEnPorts[index], s_MotorEnPins[index], 0);    // IN2 = 0
        }
        __Motor_SetPWM(index, abs_speed);
        break;
    }
    case MOTOR_MODE_BIPOLAR:
    {
        int16_t pwm_bipolar = (pwm >> 1) + 500;
        if (pwm_bipolar < 0) pwm_bipolar = 0;
        if (pwm_bipolar > 1000) pwm_bipolar = 1000;
        __Motor_SetPWM(index, pwm_bipolar);
        break;
    }
    default:
        break;
    }
}

/**
 * @brief  使能指定电机
 * @param  index 电机索引
 */
void Motor_Enable(MotorSelect_T index)
{
#ifdef CAR_BRINGUP_NO_ACTUATION
    Motor_Disable(index);
    return;
#endif
    if (index >= MOTOR_COUNT) { return; }
    if (s_MotorModes == MOTOR_MODE_UNIPOLAR_NO_EN || s_MotorModes == MOTOR_MODE_DUAL_INPUT) { return; }
    BSP_GPIO_WritePin(s_MotorEnPorts[index], s_MotorEnPins[index], 1);
}

/**
 * @brief  失能指定电机
 * @param  index 电机索引
 */
void Motor_Disable(MotorSelect_T index)
{
    if (index >= MOTOR_COUNT) { return; }
    if (s_MotorModes == MOTOR_MODE_UNIPOLAR_NO_EN || s_MotorModes == MOTOR_MODE_DUAL_INPUT) { return; }
    BSP_GPIO_WritePin(s_MotorEnPorts[index], s_MotorEnPins[index], 0);
}

/**
 * @brief  停止所有直流电机
 */
void Motor_Stop(void)
{
    Motor_SetPWM(MOTOR_A, 0);
    Motor_SetPWM(MOTOR_B, 0);
    Motor_SetPWM(MOTOR_C, 0);
    Motor_SetPWM(MOTOR_D, 0);
}

/**
 * @brief  设置电机的控制模式
 * @param  mode 电机控制模式
 */
void Motor_SetMode(MotorMode_T mode)
{
    s_MotorModes = mode;
}
