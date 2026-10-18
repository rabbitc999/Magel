#include "bsp_pwr.h"

uint32_t s_WaitToPowerDownTime = 30;  // 默认30秒进入待机状态
volatile uint32_t s_RecordTime = 0;   // 事件记录时间（单调毫秒）

/**
 * @brief  记录当前时间戳，作为进入待机状态的计时起点
 */
void PWR_RecordTime(void)
{
    /* Called by the key timer ISR: avoid calendar conversion and libc locks. */
    s_RecordTime = HAL_GetTick();
}

/**
 * @brief  判断是否已超过设定的等待时间需要进入待机状态
 * @retval 1-距离上次记录时间已超过等待时间，需进入待机；0-未超时
 * @note   不依赖RTC日历转换，可从按键定时器中断调用
 */
uint8_t PWR_IsAlarm(void)
{
    /* Unsigned subtraction also handles the HAL millisecond counter wrapping. */
    return (uint32_t)(HAL_GetTick() - s_RecordTime) / 1000 >= s_WaitToPowerDownTime;
}

/**
 * @brief  设置进入待机状态前的等待时间
 * @param  WaitToPowerDownTime 等待时间（单位：秒）
 */
void PWR_SetPowerDownTime(uint32_t WaitToPowerDownTime)
{
    s_WaitToPowerDownTime = WaitToPowerDownTime;
}
