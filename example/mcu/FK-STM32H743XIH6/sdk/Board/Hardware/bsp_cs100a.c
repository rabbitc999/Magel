#include "bsp_cs100a.h"
#include "bsp_periph.h"

/** ********************** 硬件引脚重定义 ************************/
// 前方TRIG引脚
#define CS100A_FRONT_TRIG_PORT GPIOD
#define CS100A_FRONT_TRIG_PIN  GPIO_PIN_3
// 前方ECHO引脚
#define CS100A_FRONT_ECHO_PORT GPIOD
#define CS100A_FRONT_ECHO_PIN  GPIO_PIN_7

// 后方TRIG引脚
#define CS100A_BACK_TRIG_PORT  GPIOC
#define CS100A_BACK_TRIG_PIN   GPIO_PIN_11
// 后方ECHO引脚
#define CS100A_BACK_ECHO_PORT  GPIOD
#define CS100A_BACK_ECHO_PIN   GPIO_PIN_11

/************************ 全局变量定义 ************************/
float g_CS100A_FrontDistance = 0.0f;         // 超声波测量距离(cm)
float g_CS100A_BackDistance = 0.0f;          // 超声波测量距离(cm)
uint32_t g_FrontCount = 0, g_BackCount = 0;  // 前进和后退距离超过的计数

/************************ 传感器总初始化 ************************/
/**
 * @brief  初始化前后两路CS100A超声波传感器（TRIG输出、ECHO外部中断）
 */
void CS100A_Init(void)
{
    // 1. TRIG引脚配置
    BSP_GPIO_Init(CS100A_FRONT_TRIG_PORT, CS100A_FRONT_TRIG_PIN, OUT_PP);
    BSP_GPIO_Init(CS100A_BACK_TRIG_PORT, CS100A_BACK_TRIG_PIN, OUT_PP);

    // 2. ECHO引脚配置
    BSP_GPIO_Init(CS100A_FRONT_ECHO_PORT, CS100A_FRONT_ECHO_PIN, IT_F_R);
    BSP_GPIO_Init(CS100A_BACK_ECHO_PORT, CS100A_BACK_ECHO_PIN, IT_F_R);

    // 3. EXTI中断配置（最高优先级）
    BSP_NVIC_Init(EXTI9_5_IRQn, 0, 0);    // 抢占优先级0、子优先级0
    BSP_NVIC_Init(EXTI15_10_IRQn, 0, 0);  // 抢占优先级0、子优先级0

    // 4. TRIG默认低电平
    BSP_GPIO_WritePin(CS100A_FRONT_TRIG_PORT, CS100A_FRONT_TRIG_PIN, 0);
    BSP_GPIO_WritePin(CS100A_BACK_TRIG_PORT, CS100A_BACK_TRIG_PIN, 0);
}

/** ********************** 触发超声波测距 ************************/
/**
 * @brief  向前后两路超声波传感器发送触发信号（50us高电平）
 */
void CS100A_SendTrigger(void)
{
    // 发送50us高电平触发超声波（满足CS100A的TRIG触发要求）
    BSP_GPIO_WritePin(CS100A_BACK_TRIG_PORT, CS100A_BACK_TRIG_PIN, 1);
    BSP_GPIO_WritePin(CS100A_FRONT_TRIG_PORT, CS100A_FRONT_TRIG_PIN, 1);
    DWT_DelayUs(50);
    BSP_GPIO_WritePin(CS100A_BACK_TRIG_PORT, CS100A_BACK_TRIG_PIN, 0);
    BSP_GPIO_WritePin(CS100A_FRONT_TRIG_PORT, CS100A_FRONT_TRIG_PIN, 0);
}

/** ********************** EXTI2中断回调函数（ECHO引脚双边沿处理） ************************/
/**
 * @brief  EXTI中断回调函数，处理ECHO引脚双边沿并计算超声波距离
 * @param  GPIO_Pin 触发中断的GPIO引脚号
 * @note   上升沿开始计时，下降沿根据脉宽计算距离(cm)；超时连续5次则置为999.9
 */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    static uint64_t s_FrontStartTick, s_BackStartTick;

    if (GPIO_Pin == CS100A_FRONT_ECHO_PIN)  // 仅处理PB8的EXTI2中断
    {
        if (BSP_GPIO_ReadPin(CS100A_FRONT_ECHO_PORT, CS100A_FRONT_ECHO_PIN))
        {
            // ECHO上升沿：开始计时
            s_FrontStartTick = Timer_GetTicks();  // 溢出计数清零
        }
        else
        {
            // ECHO下降沿：停止计数并计算距离
            uint32_t delayUs = Timer_GetDiffUs(s_FrontStartTick);
            // 总时间=当前计数值×1us→转换为cm（声速340m/s=34cm/ms）
            if (delayUs <= 6000)
            {
                g_CS100A_FrontDistance = (float)delayUs / 1000 * 34.0 / 2.0;
                g_FrontCount = 0;
            }
            else
            {
                if (++g_FrontCount >= 5)
                {
                    g_CS100A_FrontDistance = 999.9;
                }
            }
        }
    }
    if (GPIO_Pin == CS100A_BACK_ECHO_PIN)  // 仅处理PB2的EXTI2中断
    {
        if (BSP_GPIO_ReadPin(CS100A_BACK_ECHO_PORT, CS100A_BACK_ECHO_PIN))
        {
            // ECHO上升沿：开始计时
            s_BackStartTick = Timer_GetTicks();  // 溢出计数清零
        }
        else
        {
            // ECHO下降沿：停止计数并计算距离
            uint32_t delayUs = Timer_GetDiffUs(s_BackStartTick);
            // 总时间=当前计数值×1us→转换为cm（声速340m/s=34cm/ms）
            if (delayUs <= 6000)
            {
                g_CS100A_BackDistance = (float)delayUs / 1000 * 34.0 / 2.0;
                g_BackCount = 0;
            }
            else
            {
                if (++g_BackCount >= 5)
                {
                    g_CS100A_BackDistance = 999.9;
                }
            }
        }
    }
}

// EXTI中断服务函数
/**
 * @brief  EXTI15_10中断服务函数（后方ECHO引脚）
 */
void EXTI15_10_IRQHandler(void)
{
    HAL_GPIO_EXTI_IRQHandler(CS100A_BACK_ECHO_PIN);
}

// EXTI中断服务函数
/**
 * @brief  EXTI9_5中断服务函数（前方ECHO引脚）
 */
void EXTI9_5_IRQHandler(void)
{
    HAL_GPIO_EXTI_IRQHandler(CS100A_FRONT_ECHO_PIN);
}
