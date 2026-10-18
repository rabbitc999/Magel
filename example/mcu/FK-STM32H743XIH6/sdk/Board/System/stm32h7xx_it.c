#include "stm32h7xx_it.h"
#include "bsp_system.h"

/**
 * @brief  NMI不可屏蔽中断处理函数
 */
void NMI_Handler(void)
{
}

/**
 * @brief  存储器管理异常处理函数，发生该异常时进入死循环
 */
void MemManage_Handler(void)
{
    /* Go to infinite loop when Memory Manage exception occurs */
    while (1)
    {
    }
}

/**
 * @brief  总线错误异常处理函数，发生该异常时进入死循环
 */
void BusFault_Handler(void)
{
    /* Go to infinite loop when Bus Fault exception occurs */
    while (1)
    {
    }
}

/**
 * @brief  用法错误异常处理函数，发生该异常时进入死循环
 */
void UsageFault_Handler(void)
{
    /* Go to infinite loop when Usage Fault exception occurs */
    while (1)
    {
    }
}

/**
 * @brief  调试监视器异常处理函数
 */
void DebugMon_Handler(void)
{
}

#if 1
/**
 * @brief  SysTick滴答定时器中断处理函数
 */
void SysTick_Handler(void)
{
}
#endif

#if 0
/**
 * @brief  PendSV可挂起服务异常处理函数
 */
void PendSV_Handler(void)
{
}

/**
 * @brief  SVC系统服务调用异常处理函数
 */
void SVC_Handler(void)
{
}
#endif
