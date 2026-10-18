#include "bsp_encoder.h"
#include "bsp_periph.h"

/* 编码器引脚配置宏 */
#define ENCODER_A_TIM      TIM2
#define ENCODER_A_CH1_PORT GPIOA
#define ENCODER_A_CH1_PIN  GPIO_PIN_15
#define ENCODER_A_CH2_PORT GPIOB
#define ENCODER_A_CH2_PIN  GPIO_PIN_3
#define ENCODER_A_AF       GPIO_AF1_TIM2

#define ENCODER_B_TIM      TIM3
#define ENCODER_B_CH1_PORT GPIOB
#define ENCODER_B_CH1_PIN  GPIO_PIN_4
#define ENCODER_B_CH2_PORT GPIOB
#define ENCODER_B_CH2_PIN  GPIO_PIN_5
#define ENCODER_B_AF       GPIO_AF2_TIM3

#define ENCODER_C_TIM      TIM4
#define ENCODER_C_CH1_PORT GPIOB
#define ENCODER_C_CH1_PIN  GPIO_PIN_6
#define ENCODER_C_CH2_PORT GPIOB
#define ENCODER_C_CH2_PIN  GPIO_PIN_7
#define ENCODER_C_AF       GPIO_AF2_TIM4

#define ENCODER_D_TIM      TIM5
#define ENCODER_D_CH1_PORT GPIOA
#define ENCODER_D_CH1_PIN  GPIO_PIN_0
#define ENCODER_D_CH2_PORT GPIOA
#define ENCODER_D_CH2_PIN  GPIO_PIN_1
#define ENCODER_D_AF       GPIO_AF2_TIM5

/* 定时器句柄结构体 */
static TIM_HandleTypeDef s_TimEncode[ENCODER_COUNT] = { 0 };

/**
 * @brief  编码器初始化（配置TIM2/3/4/5为编码器模式并启动计数）
 */
void Encoder_Init(void)
{
    TIM_Encoder_InitTypeDef sConfig = {
        .EncoderMode = TIM_ENCODERMODE_TI12,
        .IC1Selection = TIM_ICSELECTION_DIRECTTI,
        .IC1Prescaler = TIM_ICPSC_DIV1,
        .IC1Filter = 0xF,
        .IC2Selection = TIM_ICSELECTION_DIRECTTI,
        .IC2Prescaler = TIM_ICPSC_DIV1,
        .IC2Filter = 0xF
    };

    TIM_MasterConfigTypeDef sMasterConfig = {
        .MasterOutputTrigger = TIM_TRGO_RESET,
        .MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE
    };

    /* TIM 时钟使能配置 */
    BSP_TIM_ClkEnable(ENCODER_A_TIM);
    BSP_TIM_ClkEnable(ENCODER_B_TIM);
    BSP_TIM_ClkEnable(ENCODER_C_TIM);
    BSP_TIM_ClkEnable(ENCODER_D_TIM);

    /* ENCODER_A 编码器配置 */
    s_TimEncode[ENCODER_A].Instance = ENCODER_A_TIM;
    s_TimEncode[ENCODER_A].Init.Prescaler = 0;
    s_TimEncode[ENCODER_A].Init.CounterMode = TIM_COUNTERMODE_UP;
    s_TimEncode[ENCODER_A].Init.Period = 65535;
    s_TimEncode[ENCODER_A].Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    s_TimEncode[ENCODER_A].Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
    sConfig.IC1Polarity = TIM_ICPOLARITY_RISING;
    sConfig.IC2Polarity = TIM_ICPOLARITY_RISING;
    HAL_TIM_Encoder_Init(&s_TimEncode[ENCODER_A], &sConfig);
    HAL_TIMEx_MasterConfigSynchronization(&s_TimEncode[ENCODER_A], &sMasterConfig);

    /* ENCODER_B 编码器配置 */
    s_TimEncode[ENCODER_B].Instance = ENCODER_B_TIM;
    s_TimEncode[ENCODER_B].Init.Prescaler = 0;
    s_TimEncode[ENCODER_B].Init.CounterMode = TIM_COUNTERMODE_UP;
    s_TimEncode[ENCODER_B].Init.Period = 65535;
    s_TimEncode[ENCODER_B].Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    s_TimEncode[ENCODER_B].Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
    sConfig.IC1Polarity = TIM_ICPOLARITY_RISING;
    sConfig.IC2Polarity = TIM_ICPOLARITY_RISING;
    HAL_TIM_Encoder_Init(&s_TimEncode[ENCODER_B], &sConfig);
    HAL_TIMEx_MasterConfigSynchronization(&s_TimEncode[ENCODER_B], &sMasterConfig);

    /* ENCODER_C 编码器配置 */
    s_TimEncode[ENCODER_C].Instance = ENCODER_C_TIM;
    s_TimEncode[ENCODER_C].Init.Prescaler = 0;
    s_TimEncode[ENCODER_C].Init.CounterMode = TIM_COUNTERMODE_UP;
    s_TimEncode[ENCODER_C].Init.Period = 65535;
    s_TimEncode[ENCODER_C].Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    s_TimEncode[ENCODER_C].Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
    sConfig.IC1Polarity = TIM_ICPOLARITY_RISING;
    sConfig.IC2Polarity = TIM_ICPOLARITY_RISING;
    HAL_TIM_Encoder_Init(&s_TimEncode[ENCODER_C], &sConfig);
    HAL_TIMEx_MasterConfigSynchronization(&s_TimEncode[ENCODER_C], &sMasterConfig);

    /* ENCODER_D 编码器配置 */
    s_TimEncode[ENCODER_D].Instance = ENCODER_D_TIM;
    s_TimEncode[ENCODER_D].Init.Prescaler = 0;
    s_TimEncode[ENCODER_D].Init.CounterMode = TIM_COUNTERMODE_UP;
    s_TimEncode[ENCODER_D].Init.Period = 65535;
    s_TimEncode[ENCODER_D].Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    s_TimEncode[ENCODER_D].Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
    sConfig.IC1Polarity = TIM_ICPOLARITY_RISING;
    sConfig.IC2Polarity = TIM_ICPOLARITY_RISING;
    HAL_TIM_Encoder_Init(&s_TimEncode[ENCODER_D], &sConfig);
    HAL_TIMEx_MasterConfigSynchronization(&s_TimEncode[ENCODER_D], &sMasterConfig);

    /* GPIO初始化 */
    /* ENCODER_A */
    BSP_GPIO_InitEx(ENCODER_A_CH1_PORT, ENCODER_A_CH1_PIN, OUT_AF_PP, ENCODER_A_AF);
    BSP_GPIO_InitEx(ENCODER_A_CH2_PORT, ENCODER_A_CH2_PIN, OUT_AF_PP, ENCODER_A_AF);

    /* ENCODER_B */
    BSP_GPIO_InitEx(ENCODER_B_CH1_PORT, ENCODER_B_CH1_PIN, OUT_AF_PP, ENCODER_B_AF);
    BSP_GPIO_InitEx(ENCODER_B_CH2_PORT, ENCODER_B_CH2_PIN, OUT_AF_PP, ENCODER_B_AF);

    /* ENCODER_C */
    BSP_GPIO_InitEx(ENCODER_C_CH1_PORT, ENCODER_C_CH1_PIN, OUT_AF_PP, ENCODER_C_AF);
    BSP_GPIO_InitEx(ENCODER_C_CH2_PORT, ENCODER_C_CH2_PIN, OUT_AF_PP, ENCODER_C_AF);

    /* ENCODER_D */
    BSP_GPIO_InitEx(ENCODER_D_CH1_PORT, ENCODER_D_CH1_PIN, OUT_AF_PP, ENCODER_D_AF);
    BSP_GPIO_InitEx(ENCODER_D_CH2_PORT, ENCODER_D_CH2_PIN, OUT_AF_PP, ENCODER_D_AF);

    /* 编码器使能 */
    HAL_TIM_Encoder_Start(&s_TimEncode[ENCODER_A], TIM_CHANNEL_ALL);
    HAL_TIM_Encoder_Start(&s_TimEncode[ENCODER_B], TIM_CHANNEL_ALL);
    HAL_TIM_Encoder_Start(&s_TimEncode[ENCODER_C], TIM_CHANNEL_ALL);
    HAL_TIM_Encoder_Start(&s_TimEncode[ENCODER_D], TIM_CHANNEL_ALL);
}

/**
 * @brief  获取指定编码器增量值（读取并清零计数值）
 * @param  index 编码器索引
 * @retval 指定编码器的增量值
 */
int16_t Encoder_GetCounter(EncoderSelect_T index)
{
    int16_t temp;
    switch (index)
    {
    case ENCODER_A:
    {
        temp = ENCODER_A_TIM->CNT;
        ENCODER_A_TIM->CNT = 0;
        return temp;
    }
    case ENCODER_B:
    {
        temp = ENCODER_B_TIM->CNT;
        ENCODER_B_TIM->CNT = 0;
        return temp;
    }
    case ENCODER_C:
    {
        temp = ENCODER_C_TIM->CNT;
        ENCODER_C_TIM->CNT = 0;
        return temp;
    }
    case ENCODER_D:
    {
        temp = ENCODER_D_TIM->CNT;
        ENCODER_D_TIM->CNT = 0;
        return temp;
    }
    default:
    {
        return 0;
    }
    }
}

/**
 * @brief  清零所有编码器的计数值
 */
void Encoder_Clear(void)
{
    TIM2->CNT = TIM3->CNT = TIM4->CNT = TIM5->CNT = 0;
}
