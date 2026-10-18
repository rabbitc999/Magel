#include "bsp_system.h"
#include "magel_board.h"

volatile uint32_t g_RunTimerTicks;
void MPU_Config(void);
void System_ClockConfig(void);

/**
 * @brief 初始化 VM 所需外设；不启动小车、舵机、电机或应用任务。
 * @retval 无正常返回路径，测试结束后等待复位。
 */
int main(void)
{
    __disable_irq();
    MPU_Config();
    SCB_EnableICache();
    SCB_EnableDCache();
    HAL_Init();
    System_ClockConfig();
    DWT_Init();
    Timer_Init();
    SDRAM_Init();
    Memory_Init();
    UART_Init();
    Key_Init();
    __enable_irq();
    UART_SendStringImm(COM1, "\r\nMAGEL FK-STM32H743XIH6 BOOT\r\n");
    LCD_Init();
    LCD_SetFontSize(LCD_FontSize16);
    W25QXX_Init();
    Magel_BoardRun();
    for (;;) __WFI();
}

/**
 * @brief 为 HAL 毫秒时基与按键消抖服务；不调用小车调度任务。
 */
void TIM6_DAC_IRQHandler(void)
{
    if (LL_TIM_IsActiveFlag_UPDATE(TIM6))
    {
        ++g_RunTimerTicks;
        if (g_RunTimerTicks % 10u == 0u) Key_Tick();
        LL_TIM_ClearFlag_UPDATE(TIM6);
    }
}

/**
 * @brief 确认参考 BSP 创建的 TIM7 中断，不执行电机控制。
 */
void TIM7_IRQHandler(void)
{
    LL_TIM_ClearFlag_UPDATE(TIM7);
}
