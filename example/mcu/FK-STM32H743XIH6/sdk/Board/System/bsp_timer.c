#include "bsp_timer.h"
#include "bsp_periph.h"
#include "bsp_system.h"
#include <stdlib.h>

void Delay_Init(uint16_t sysclk);

/* 全局变量定义 */
static volatile uint64_t s_OverflowCount1;
static volatile uint32_t s_OverflowCount2;
static uint64_t s_TimeBaseMs, s_TimeBaseTicks;
extern volatile uint32_t g_RunTimerTicks;

/**
 * @brief  初始化定时器（TIM6/TIM7/TIM16/TIM17基础定时功能）
 */
void Timer_Init(void)
{
    BSP_TIM_Base_Init(TIM6, 1000, 6, 0);
    BSP_TIM_Base_Init(TIM7, 20000, 6, 0);
    BSP_TIM_Base_Init(TIM16, 0x10000, 6, 0);
    BSP_TIM_Base_Init(TIM17, 0x10000, 6, 0);
}

/**
 * @brief  计算两个时刻计数值的时间差（处理溢出）
 * @param  startTick-起始计数值（HAL_GetTick()返回值）
 * @param  endTick-结束计数值（HAL_GetTick()返回值）
 * @retval 时间差（单位：ms）
 * @note   自动处理计数器溢出情况，适用于测量时间间隔
 */
uint32_t Timer_GetDiffTick(uint32_t startTick, uint32_t endTick)
{
    uint32_t diffTicks = 0U;

    /* 计算计数值差（处理溢出） */
    if (endTick >= startTick)
    {
        diffTicks = endTick - startTick;
    }
    else
    {
        /* 溢出情况：diff = (0xFFFFFFFF - startTick + 1) + endTick */
        diffTicks = (0xFFFFFFFF - startTick + 1U) + endTick;
    }

    /* 转换为毫秒 */
    return diffTicks;
}

/**
 * @brief  计算此时刻与当前时刻计数值的时间差（处理溢出）
 * @param  startTick-起始计数值（HAL_GetTick()返回值）
 * @retval 时间差（单位：ms）
 * @note   自动处理计数器溢出情况，适用于测量时间间隔
 */
uint32_t Timer_GetDiffMs(uint32_t startTick)
{
    uint32_t diffTicks = 0U, endTick = HAL_GetTick();

    /* 计算计数值差（处理溢出） */
    if (endTick >= startTick)
    {
        diffTicks = endTick - startTick;
    }
    else
    {
        /* 溢出情况：diff = (0xFFFFFFFF - startTick + 1) + endTick */
        diffTicks = (0xFFFFFFFF - startTick + 1U) + endTick;
    }

    /* 转换为毫秒 */
    return diffTicks;
}

/**
 * @brief  计算此时刻与当前时刻计数值的时间差（处理溢出）
 * @param  startTick-起始计数值（HAL_GetTick()返回值）
 * @retval 时间差（单位：ms）
 * @note   自动处理计数器溢出情况，适用于测量时间间隔
 */
uint64_t Timer_GetDiffUs(uint64_t startTick)
{
    uint64_t diffTicks = 0U, endTick = Timer_GetTicks();

    /* 转换为微秒 */
    return endTick - startTick;
}

/**
 * @brief  获取计数器2的当前计时值（微秒）
 * @retval 当前微秒计数值
 * @note   结合溢出计数和TIM17当前计数值计算总微秒数
 */
uint64_t Timer_GetUsTicks(void)
{
    uint32_t overflow, counter;
    uint64_t ticks;

    /* 为了避免在读取过程中发生中断导致数据不一致，禁用中断来原子地读取这些值 */
    __disable_irq();
    overflow = s_OverflowCount2;
    counter = LL_TIM_GetCounter(TIM17);
    __enable_irq();

    /* 计算总计数 = 溢出次数 * 65536 + 当前计数值 */
    ticks = (uint64_t)overflow * 65536ULL + counter;

    /* 转换为微秒（因为每个计数是1微秒） */
    return ticks;
}

/**
 * @brief  清空计数器2的当前计时值（微秒）
 * @note   将TIM17溢出计数和当前计数值同时清零
 */
void Timer_ClearUsTicks(void)
{
    s_OverflowCount2 = 0;
    LL_TIM_SetCounter(TIM17, 0);
}

/**
 * @brief  获取计数器1的当前计时值（微秒）
 * @retval 当前微秒计数值
 * @note   结合溢出计数和TIM16当前计数值计算总微秒数
 */
uint64_t Timer_GetTicks(void)
{
    uint32_t primask = __get_PRIMASK();
    __disable_irq();
    uint64_t overflow = s_OverflowCount1;
    uint32_t counter = LL_TIM_GetCounter(TIM16);
    /* Account for rollover that occurred before its IRQ could run. */
    if (LL_TIM_IsActiveFlag_UPDATE(TIM16))
    {
        overflow++;
        counter = LL_TIM_GetCounter(TIM16);
    }
    __set_PRIMASK(primask);
    return overflow * 65536ULL + counter;
}

uint64_t Timer_GetTimestampMs(void)
{
    uint32_t primask = __get_PRIMASK();
    __disable_irq();
    uint64_t result = s_TimeBaseMs + (Timer_GetTicks() - s_TimeBaseTicks) / 1000ULL;
    __set_PRIMASK(primask);
    return result;
}

void Timer_SetTimestampMs(uint64_t timestamp)
{
    uint32_t primask = __get_PRIMASK();
    __disable_irq();
    s_TimeBaseTicks = Timer_GetTicks();
    s_TimeBaseMs = timestamp;
    __set_PRIMASK(primask);
}

/**
 * @brief  重定向毫秒延迟函数。替换HAL中的函数
 * @param  Delay 延时时长（单位：ms）
 * @note   调度器已启动时使用FreeRTOS的vTaskDelay，否则使用DWT延时
 */
void HAL_Delay(uint32_t Delay)
{
    // 假设每次循环迭代需要一定时钟周期，这里需要校准
    DWT_DelayMs(Delay);
}

/**
 * @brief  重定向毫秒初始化函数。替换HAL中的函数
 * @param  TickPriority 滴答中断优先级（未使用）
 * @retval HAL_OK
 */
HAL_StatusTypeDef HAL_InitTick(uint32_t TickPriority)
{
    return HAL_OK;
}

/**
 * @brief  重定向毫秒获取函数。替换HAL中的函数
 * @retval 当前系统运行的毫秒数（单位：ms）
 */
uint32_t HAL_GetTick(void)
{
    return g_RunTimerTicks;
}

/**
 * @brief  TIM16定时器中断处理函数（更新溢出计数）
 */
void TIM16_IRQHandler(void)
{
    if (LL_TIM_IsActiveFlag_UPDATE(TIM16))
    {
        s_OverflowCount1++;
        LL_TIM_ClearFlag_UPDATE(TIM16);
    }
}

/**
 * @brief  TIM17定时器中断处理函数（更新溢出计数）
 */
void TIM17_IRQHandler(void)
{
    if (LL_TIM_IsActiveFlag_UPDATE(TIM17))
    {
        s_OverflowCount2++;
        LL_TIM_ClearFlag_UPDATE(TIM17);
    }
}

/* 以下为定时器6中断回调函数模板
void TIM6_DAC_IRQHandler(void)
{
    LL_TIM_ClearFlag_UPDATE(TIM6);
}
*/
