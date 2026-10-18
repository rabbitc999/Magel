#include "bsp_periph.h"

void Assert_Failed(char *file, uint32_t line);

/**
 * @brief  使能指定 GPIO 端口的时钟
 * @param  port GPIO 端口（如 GPIOA）
 * @note   仅内部调用，自动匹配端口对应的时钟使能函数
 */
void BSP_GPIO_ClkEnable(GPIO_TypeDef *port)
{
    /* 参数有效性校验 */
    if (port == NULL)
    {
        return;
    }

    if (port == GPIOA)
    {
        __HAL_RCC_GPIOA_CLK_ENABLE();
    }
    else if (port == GPIOB)
    {
        __HAL_RCC_GPIOB_CLK_ENABLE();
    }
    else if (port == GPIOC)
    {
        __HAL_RCC_GPIOC_CLK_ENABLE();
    }
    else if (port == GPIOD)
    {
        __HAL_RCC_GPIOD_CLK_ENABLE();
    }
    else if (port == GPIOE)
    {
        __HAL_RCC_GPIOE_CLK_ENABLE();
    }
    else if (port == GPIOF)
    {
        __HAL_RCC_GPIOF_CLK_ENABLE();
    }
    else if (port == GPIOG)
    {
        __HAL_RCC_GPIOG_CLK_ENABLE();
    }
    else if (port == GPIOH)
    {
        __HAL_RCC_GPIOH_CLK_ENABLE();
    }
    else if (port == GPIOI)
    {
        __HAL_RCC_GPIOI_CLK_ENABLE();
    }
    else if (port == GPIOJ)
    {
        __HAL_RCC_GPIOJ_CLK_ENABLE();
    }
    else if (port == GPIOK)
    {
        __HAL_RCC_GPIOK_CLK_ENABLE();
    }
}

/**
 * @brief  使能指定定时器的外设时钟
 * @param  timInstance 定时器实例指针（如TIM1、TIM8等）
 * @note   1. 自动匹配定时器对应的RCC时钟使能函数；
 *         2. 支持STM32H7系列常用定时器（TIM1~TIM17，STM32H7没有TIM9，TIM10，TIM11定时器）；
 *         3. 重复调用无副作用（RCC时钟使能可多次执行）
 */
void BSP_TIM_ClkEnable(TIM_TypeDef *timInstance)
{
    /* 参数有效性校验 */
    if (timInstance == NULL)
    {
        return;
    }

    if (timInstance == TIM1)
    {
        __HAL_RCC_TIM1_CLK_ENABLE();
    }
    else if (timInstance == TIM2)
    {
        __HAL_RCC_TIM2_CLK_ENABLE();
    }
    else if (timInstance == TIM3)
    {
        __HAL_RCC_TIM3_CLK_ENABLE();
    }
    else if (timInstance == TIM4)
    {
        __HAL_RCC_TIM4_CLK_ENABLE();
    }
    else if (timInstance == TIM5)
    {
        __HAL_RCC_TIM5_CLK_ENABLE();
    }
    else if (timInstance == TIM6)
    {
        __HAL_RCC_TIM6_CLK_ENABLE();
    }
    else if (timInstance == TIM7)
    {
        __HAL_RCC_TIM7_CLK_ENABLE();
    }
    else if (timInstance == TIM8)
    {
        __HAL_RCC_TIM8_CLK_ENABLE();
    }
    else if (timInstance == TIM12)
    {
        __HAL_RCC_TIM12_CLK_ENABLE();
    }
    else if (timInstance == TIM13)
    {
        __HAL_RCC_TIM13_CLK_ENABLE();
    }
    else if (timInstance == TIM14)
    {
        __HAL_RCC_TIM14_CLK_ENABLE();
    }
    else if (timInstance == TIM15)
    {
        __HAL_RCC_TIM15_CLK_ENABLE();
    }
    else if (timInstance == TIM16)
    {
        __HAL_RCC_TIM16_CLK_ENABLE();
    }
    else if (timInstance == TIM17)
    {
        __HAL_RCC_TIM17_CLK_ENABLE();
    }
}

/**
 * @brief  使能指定I2C外设的时钟
 * @param  i2cInstance I2C实例指针（如I2C1、I2C2等）
 * @note   1. 自动匹配I2C对应的RCC时钟使能函数；
 *         2. 支持STM32H7系列常用I2C（I2C1~I2C4）；
 *         3. 重复调用无副作用（RCC时钟使能可多次执行）
 */
void BSP_I2C_ClkEnable(I2C_TypeDef *i2cInstance)
{
    /* 参数有效性校验 */
    if (i2cInstance == NULL)
    {
        return;
    }

    if (i2cInstance == I2C1)
    {
        __HAL_RCC_I2C1_CLK_ENABLE();
    }
    else if (i2cInstance == I2C2)
    {
        __HAL_RCC_I2C2_CLK_ENABLE();
    }
    else if (i2cInstance == I2C3)
    {
        __HAL_RCC_I2C3_CLK_ENABLE();
    }
    else if (i2cInstance == I2C4)
    {
        __HAL_RCC_I2C4_CLK_ENABLE();
    }
}

/**
 * @brief  使能指定SPI外设的时钟
 * @param  spiInstance SPI实例指针（如SPI1、SPI2等）
 * @note   1. 自动匹配SPI对应的RCC时钟使能函数；
 *         2. 支持STM32H7系列常用SPI（SPI1~SPI6）；
 *         3. 重复调用无副作用（RCC时钟使能可多次执行）
 */
void BSP_SPI_ClkEnable(SPI_TypeDef *spiInstance)
{
    /* 参数有效性校验 */
    if (spiInstance == NULL)
    {
        return;
    }

    if (spiInstance == SPI1)
    {
        __HAL_RCC_SPI1_CLK_ENABLE();
    }
    else if (spiInstance == SPI2)
    {
        __HAL_RCC_SPI2_CLK_ENABLE();
    }
    else if (spiInstance == SPI3)
    {
        __HAL_RCC_SPI3_CLK_ENABLE();
    }
    else if (spiInstance == SPI4)
    {
        __HAL_RCC_SPI4_CLK_ENABLE();
    }
    else if (spiInstance == SPI5)
    {
        __HAL_RCC_SPI5_CLK_ENABLE();
    }
    else if (spiInstance == SPI6)
    {
        __HAL_RCC_SPI6_CLK_ENABLE();
    }
}

/**
 * @brief  使能指定串口外设的时钟
 * @param  usartInstance 串口实例指针（如USART1、UART4等）
 * @note   1. 自动匹配串口对应的RCC时钟使能函数，兼容USART/UART前缀实例；
 *         2. 支持STM32H7系列常用串口（USART1~USART3、UART4~UART8）；
 *         3. 重复调用无副作用（RCC时钟使能可多次执行）；
 *         4. 仅支持硬件串口，不包含虚拟串口（如USB转串口）
 */
void BSP_USART_ClkEnable(USART_TypeDef *usartInstance)
{
    /* 参数有效性校验 */
    if (usartInstance == NULL)
    {
        return;
    }

    if (usartInstance == USART1)
    {
        __HAL_RCC_USART1_CLK_ENABLE();
    }
    else if (usartInstance == USART2)
    {
        __HAL_RCC_USART2_CLK_ENABLE();
    }
    else if (usartInstance == USART3)
    {
        __HAL_RCC_USART3_CLK_ENABLE();
    }
    else if (usartInstance == UART4)
    {
        __HAL_RCC_UART4_CLK_ENABLE();
    }
    else if (usartInstance == UART5)
    {
        __HAL_RCC_UART5_CLK_ENABLE();
    }
    else if (usartInstance == USART6)
    {
        __HAL_RCC_USART6_CLK_ENABLE();
    }
    else if (usartInstance == UART7)
    {
        __HAL_RCC_UART7_CLK_ENABLE();
    }
    else if (usartInstance == UART8)
    {
        __HAL_RCC_UART8_CLK_ENABLE();
    }
}

/**
 * @brief  使能指定DMA控制器的时钟
 * @param  dmaInstance DMA实例指针（如DMA1、DMA2）
 * @note   1. 支持STM32H7常用DMA控制器（DMA1、DMA2）；
 *         2. 重复调用无副作用
 */
void BSP_DMA_ClkEnable(DMA_TypeDef *dmaInstance)
{
    if (dmaInstance == NULL)
    {
        return;
    }

    if (dmaInstance == DMA1)
    {
        __HAL_RCC_DMA1_CLK_ENABLE();
    }
    else if (dmaInstance == DMA2)
    {
        __HAL_RCC_DMA2_CLK_ENABLE();
    }
}

/**
 * @brief  GPIO引脚初始化函数
 * @param  port GPIO端口（如GPIOA）
 * @param  pin GPIO引脚（如GPIO_PIN_0）
 * @param  mode GPIO模式（如GPIO_MODE_OUTPUT_PP）
 */
void BSP_GPIO_Init(GPIO_TypeDef *port, uint16_t pin, GPIO_MODE_T mode)
{
    GPIO_InitTypeDef GPIO_InitStruct = { 0 };

    /* 参数有效性校验 */
    if (port == NULL)
    {
        return;
    }

    BSP_GPIO_ClkEnable(port);  // 确保GPIO时钟已使能

    GPIO_InitStruct.Pin = pin;
    switch (mode)
    {
    case IN_U:
        GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
        GPIO_InitStruct.Pull = GPIO_PULLUP;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
        break;
    case IN_D:
        GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
        GPIO_InitStruct.Pull = GPIO_PULLDOWN;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
        break;
    case IN_F:
        GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
        break;
    case OUT_PP:
        GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
        break;
    case OUT_OD_U:
        GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD;
        GPIO_InitStruct.Pull = GPIO_PULLUP;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
        break;
    case OUT_OD_D:
        GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD;
        GPIO_InitStruct.Pull = GPIO_PULLDOWN;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
        break;
    case OUT_OD_F:
        GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
        break;
    case OUT_AF_PP:
        GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
        break;
    case OUT_AF_OD_D:
        GPIO_InitStruct.Mode = GPIO_MODE_AF_OD;
        GPIO_InitStruct.Pull = GPIO_PULLDOWN;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
        break;
    case OUT_AF_OD_U:
        GPIO_InitStruct.Mode = GPIO_MODE_AF_OD;
        GPIO_InitStruct.Pull = GPIO_PULLUP;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
        break;
    case OUT_AF_OD_F:
        GPIO_InitStruct.Mode = GPIO_MODE_AF_OD;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
        break;
    case ANALOG:
        GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
        break;
    case IT_F:
        GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
        break;
    case IT_R:
        GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
        break;
    case IT_F_R:
        GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING_FALLING;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
        break;
    default:
        GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
        break;
    }

    HAL_GPIO_Init(port, &GPIO_InitStruct);
}

/**
 * @brief  GPIO引脚初始化函数
 * @param  port GPIO端口（如GPIOA）
 * @param  pin GPIO引脚（如GPIO_PIN_0）
 * @param  mode GPIO模式（如GPIO_MODE_OUTPUT_PP）
 * @param  alternate GPIO复用功能（如GPIO_AF0_RTC_50Hz）
 */
void BSP_GPIO_InitEx(GPIO_TypeDef *port, uint16_t pin, GPIO_MODE_T mode, uint32_t alternate)
{
    GPIO_InitTypeDef GPIO_InitStruct = { 0 };

    /* 参数有效性校验 */
    if (port == NULL)
    {
        return;
    }

    BSP_GPIO_ClkEnable(port);  // 确保GPIO时钟已使能

    GPIO_InitStruct.Pin = pin;
    GPIO_InitStruct.Mode = mode;
    switch (mode)
    {
    case IN_U:
        GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
        GPIO_InitStruct.Pull = GPIO_PULLUP;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
        break;
    case IN_D:
        GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
        GPIO_InitStruct.Pull = GPIO_PULLDOWN;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
        break;
    case IN_F:
        GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
        break;
    case OUT_PP:
        GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
        break;
    case OUT_OD_U:
        GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD;
        GPIO_InitStruct.Pull = GPIO_PULLUP;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
        break;
    case OUT_OD_D:
        GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD;
        GPIO_InitStruct.Pull = GPIO_PULLDOWN;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
        break;
    case OUT_OD_F:
        GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
        break;
    case OUT_AF_PP:
        GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
        GPIO_InitStruct.Alternate = alternate;
        break;
    case OUT_AF_OD_D:
        GPIO_InitStruct.Mode = GPIO_MODE_AF_OD;
        GPIO_InitStruct.Pull = GPIO_PULLDOWN;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
        GPIO_InitStruct.Alternate = alternate;
        break;
    case OUT_AF_OD_U:
        GPIO_InitStruct.Mode = GPIO_MODE_AF_OD;
        GPIO_InitStruct.Pull = GPIO_PULLUP;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
        GPIO_InitStruct.Alternate = alternate;
        break;
    case OUT_AF_OD_F:
        GPIO_InitStruct.Mode = GPIO_MODE_AF_OD;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
        GPIO_InitStruct.Alternate = alternate;
        break;
    case ANALOG:
        GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
        break;
    case IT_F:
        GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
        break;
    case IT_R:
        GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
        break;
    case IT_F_R:
        GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING_FALLING;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
        break;
    default:
        GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
        break;
    }

    HAL_GPIO_Init(port, &GPIO_InitStruct);
}

/**
 * @brief  GPIO引脚初始化函数
 * @param  port GPIO端口（如GPIOA）
 * @param  pin GPIO引脚（如GPIO_PIN_0）
 * @param  mode GPIO模式（如GPIO_MODE_OUTPUT_PP）
 */
void BSP_GPIO_FastModeInit(GPIO_TypeDef *port, uint16_t pin, GPIO_MODE_T mode)
{
    GPIO_InitTypeDef GPIO_InitStruct = { 0 };

    /* 参数有效性校验 */
    if (port == NULL)
    {
        return;
    }

    BSP_GPIO_ClkEnable(port);  // 确保GPIO时钟已使能

    GPIO_InitStruct.Pin = pin;
    GPIO_InitStruct.Mode = mode;
    switch (mode)
    {
    case IN_U:
        GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
        GPIO_InitStruct.Pull = GPIO_PULLUP;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
        break;
    case IN_D:
        GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
        GPIO_InitStruct.Pull = GPIO_PULLDOWN;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
        break;
    case IN_F:
        GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
        break;
    case OUT_PP:
        GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
        break;
    case OUT_OD_U:
        GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD;
        GPIO_InitStruct.Pull = GPIO_PULLUP;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
        break;
    case OUT_OD_D:
        GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD;
        GPIO_InitStruct.Pull = GPIO_PULLDOWN;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
        break;
    case OUT_OD_F:
        GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
        break;
    case ANALOG:
        GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
        break;
    case IT_F:
        GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
        break;
    case IT_R:
        GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
        break;
    case IT_F_R:
        GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING_FALLING;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
        break;
    default:
        GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
        break;
    }

    HAL_GPIO_Init(port, &GPIO_InitStruct);
}

/**
 * @brief  NVIC中断初始化，并开启中断
 * @param  irq 中断线
 * @param  preemption 抢占优先级
 * @param  sub 子优先级
 */
void BSP_NVIC_Init(IRQn_Type irq, uint8_t preemption, uint8_t sub)
{
    HAL_NVIC_SetPriority(irq, preemption, sub);
    HAL_NVIC_EnableIRQ(irq);
}

/**
 * @brief  配置TIM和NVIC，用于简单的定时中断，开启定时中断。另外注意中断服务程序需要由用户应用程序实现。
 * @param  TIMx 定时器
 * @param  ulTime 定时周期（us），0表示关闭
 * @param  _PreemptionPriority 抢占优先级
 * @param  _SubPriority 子优先级
 * @note   TIM定时中断服务程序范例，必须清中断标志
 *         void TIM6_DAC_IRQHandler(void)
 *         {
 *             if (LL_TIM_IsActiveFlag_UPDATE(TIM6))
 *             {
 *                 //添加用户代码
 *                 LL_TIM_ClearFlag_UPDATE(TIM6);
 *             }
 *         }
 */
void BSP_TIM_Base_Init(TIM_TypeDef *TIMx, uint32_t ulTime, uint8_t _PreemptionPriority, uint8_t _SubPriority)
{
    TIM_HandleTypeDef TimHandle = { 0 };
    uint32_t uiTIMxCLK;

    /* 使能TIM时钟 */
    BSP_TIM_ClkEnable(TIMx);

    /*-----------------------------------------------------------------------
        bsp_system.c 文件中 void SystemClock_Config(void) 函数对时钟的配置如下:
        System Clock source       = PLL (HSE)
        SYSCLK(Hz)                = 480000000 (CPU Clock)
        HCLK(Hz)                  = 240000000 (AXI and AHBs Clock)
        AHB Prescaler             = 2
        D1 APB3 Prescaler         = 2 (APB3 Clock  120MHz)
        D2 APB1 Prescaler         = 2 (APB1 Clock  120MHz)
        D2 APB2 Prescaler         = 2 (APB2 Clock  120MHz)
        D3 APB4 Prescaler         = 2 (APB4 Clock  120MHz)
        因为APB1 prescaler != 1, 所以 APB1上的TIMxCLK = APB1 x 2 = 240MHz;
        因为APB2 prescaler != 1, 所以 APB2上的TIMxCLK = APB2 x 2 = 240MHz;
        APB1 定时器有 TIM2, TIM3 ,TIM4, TIM5, TIM6, TIM7, TIM12, TIM13, TIM14
        APB2 定时器有 TIM1, TIM8 , TIM15, TIM16，TIM17
    ----------------------------------------------------------------------- */
    if ((TIMx == TIM1) || (TIMx == TIM8) || (TIMx == TIM15) || (TIMx == TIM16) || (TIMx == TIM17))
    {
        /* APB2 定时器时钟 = 240M */
        uiTIMxCLK = SystemCoreClock / 2 / 1000000UL;
    }
    else
    {
        /* APB1 定时器 = 240M */
        uiTIMxCLK = SystemCoreClock / 2 / 1000000UL;
    }

    /*
       定时器中断更新周期 = TIMxCLK / usPrescaler + 1）/usPeriod + 1）
    */
    TimHandle.Instance = TIMx;
    TimHandle.Init.Prescaler = uiTIMxCLK - 1;
    TimHandle.Init.Period = ulTime - 1;
    TimHandle.Init.ClockDivision = 0;
    TimHandle.Init.CounterMode = TIM_COUNTERMODE_UP;
    TimHandle.Init.RepetitionCounter = 0;
    TimHandle.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
    if (HAL_TIM_Base_Init(&TimHandle) != HAL_OK)
    {
        Assert_Failed(__FILE__, __LINE__);
    }

    /* 使能定时器中断  */
    __HAL_TIM_ENABLE_IT(&TimHandle, TIM_IT_UPDATE);

    /* 配置TIM定时更新中断 (Update) */
    {
        uint8_t irq = 0; /* 中断号, 定义在 stm32h7xx.h */

        if (TIMx == TIM1)
            irq = TIM1_UP_IRQn;
        else if (TIMx == TIM2)
            irq = TIM2_IRQn;
        else if (TIMx == TIM3)
            irq = TIM3_IRQn;
        else if (TIMx == TIM4)
            irq = TIM4_IRQn;
        else if (TIMx == TIM5)
            irq = TIM5_IRQn;
        else if (TIMx == TIM6)
            irq = TIM6_DAC_IRQn;
        else if (TIMx == TIM7)
            irq = TIM7_IRQn;
        else if (TIMx == TIM8)
            irq = TIM8_UP_TIM13_IRQn;
        else if (TIMx == TIM12)
            irq = TIM8_BRK_TIM12_IRQn;
        else if (TIMx == TIM13)
            irq = TIM8_UP_TIM13_IRQn;
        else if (TIMx == TIM14)
            irq = TIM8_TRG_COM_TIM14_IRQn;
        else if (TIMx == TIM15)
            irq = TIM15_IRQn;
        else if (TIMx == TIM16)
            irq = TIM16_IRQn;
        else if (TIMx == TIM17)
            irq = TIM17_IRQn;
        else
        {
            Assert_Failed(__FILE__, __LINE__);
        }
        BSP_NVIC_Init((IRQn_Type)irq, _PreemptionPriority, _SubPriority);
    }

    HAL_TIM_Base_Start(&TimHandle);
}
