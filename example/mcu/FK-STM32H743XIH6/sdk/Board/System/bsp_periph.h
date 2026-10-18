#ifndef __BSP_PERIPH_H
#define __BSP_PERIPH_H

#include "stm32h7xx_hal.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum __GPIO_MODE_T
{
    IN_U = 0,         /* 上拉输入 */
    IN_D = 1,         /* 下拉输入 */
    IN_F = 2,         /* 浮空输入 */
    OUT_PP = 3,       /* 推挽输出 */
    OUT_OD_U = 4,     /* 开漏输出，上拉 */
    OUT_OD_D = 5,     /* 开漏输出，下拉 */
    OUT_OD_F = 6,     /* 开漏输出，浮空 */
    OUT_AF_PP = 7,    /* 复用推挽输出 */
    OUT_AF_OD_U = 8,  /* 复用开漏输出，上拉 */
    OUT_AF_OD_D = 9,  /* 复用开漏输出，下拉 */
    OUT_AF_OD_F = 10, /* 复用开漏输出，浮空 */
    ANALOG = 11,      /* 模拟输入 */
    IT_F = 12,        /* 下降沿中断 */
    IT_R = 13,        /* 上升沿中断 */
    IT_F_R = 14,      /* 下降沿/上升沿中断 */
} GPIO_MODE_T;

void BSP_GPIO_ClkEnable(GPIO_TypeDef *port);
void BSP_TIM_ClkEnable(TIM_TypeDef *timInstance);
void BSP_I2C_ClkEnable(I2C_TypeDef *i2cInstance);
void BSP_SPI_ClkEnable(SPI_TypeDef *spiInstance);
void BSP_USART_ClkEnable(USART_TypeDef *usartInstance);
void BSP_DMA_ClkEnable(DMA_TypeDef *dmaInstance);

void BSP_GPIO_Init(GPIO_TypeDef *port, uint16_t pin, GPIO_MODE_T mode);
void BSP_GPIO_FastModeInit(GPIO_TypeDef *port, uint16_t pin, GPIO_MODE_T mode);
void BSP_GPIO_InitEx(GPIO_TypeDef *port, uint16_t pin, GPIO_MODE_T mode, uint32_t alternate);
void BSP_TIM_Base_Init(TIM_TypeDef *TIMx, uint32_t ulTime, uint8_t _PreemptionPriority, uint8_t _SubPriority);

void BSP_NVIC_Init(IRQn_Type irq, uint8_t preemption, uint8_t sub);

/**
 * @brief  GPIO引脚写入函数
 * @param  PORT: GPIO端口（如GPIOA）
 * @param  pin: GPIO引脚（如GPIO_PIN_0）
 * @param  state: GPIO引脚状态（GPIO_PIN_SET或GPIO_PIN_RESET）
 * @note 不检测参数有效性，调用前请确保参数正确
 */
#define BSP_GPIO_WritePin(PORT, PIN, state) (PORT->BSRR = ((state) ? PIN : (PIN << 16)))

/**
 * @brief  GPIO引脚写入函数
 * @param  PORT: GPIO端口（如GPIOA）
 * @param  pin: GPIO引脚（如GPIO_PIN_0）
 * @param  state: GPIO引脚状态（GPIO_PIN_SET或GPIO_PIN_RESET）
 * @note 不检测参数有效性，调用前请确保参数正确
 */
#define BSP_GPIO_ReadPin(PORT, PIN)         ((PORT->IDR & PIN) ? 1 : 0)

/**
 * @brief  GPIO引脚获取函数
 * @param  PORT: GPIO端口（如GPIOA）
 * @param  MASK: GPIO引脚掩码（如GPIO_PIN_0 | GPIO_PIN_1）
 * @retval GPIO引脚16位状态（1-高电平，0-低电平）
 * @note 不检测参数有效性，调用前请确保参数正确
 */
#define BSP_GPIO_ReadPinEx(PORT, MASK)      (PORT->IDR & MASK)

/**
 * @brief  GPIO引脚翻转函数
 * @param  PORT: GPIO端口（如GPIOA）
 * @param  pin: GPIO引脚（如GPIO_PIN_0）
 * @retval GPIO引脚新状态（1-高电平，0-低电平）
 * @note 不检测参数有效性，调用前请确保参数正确
 */
#define BSP_GPIO_TogglePin(PORT, PIN)                    \
    do                                                   \
    {                                                    \
        __set_PRIMASK(1);                                \
        uint16_t ODR = PORT->ODR;                        \
        PORT->BSRR = ((ODR & PIN) << 16) | (~ODR & PIN); \
        __set_PRIMASK(0);                                \
    } while (0)

/**
 * @brief  GPIO引脚写入函数
 * @param  PORT: GPIO端口（如GPIOA）
 * @param  MASK: GPIO引脚掩码（如GPIO_PIN_0 | GPIO_PIN_1）
 * @param  STATE: GPIO 16位引脚状态（1-高电平，0-低电平）
 * @note 不检测参数有效性，调用前请确保参数正确
 */
#define BSP_GPIO_WritePinEx(PORT, MASK, STATE)                                    \
    do                                                                            \
    {                                                                             \
        __set_PRIMASK(1);                                                         \
        uint16_t ODR = PORT->ODR;                                                 \
        PORT->BSRR = (((~ODR & MASK) & STATE) << 16) | (((ODR & MASK) & ~STATE)); \
        __set_PRIMASK(0);                                                         \
    } while (0)

/*
*********************************************************************************************************
*	函 数 名: BSP_TIM_PWM_SetDutyCycle
*	功能说明: 设置已初始化PWM通道的占空比，使用寄存器直接操作
*			  注意：此函数假设定时器已由BSP_TIM_PWM_Init正确初始化
*	形    参: TIMx : TIM1 - TIM17
*             _ucChannel：使用的定时器通道，范围1 - 4
*			  _ulDutyCycle : PWM信号占空比，单位: 万分之一。如5000，表示50.00%的占空比
*	返 回 值: 无
*********************************************************************************************************
*/
#define BSP_TIM_PWM_SetDutyCycle(TIMx, channel, duty)     \
    do                                                    \
    {                                                     \
        uint32_t _Duty = duty > 10000 ? 10000 : duty;     \
        TIMx->CCR##channel = (_Duty * TIMx->ARR) / 10000; \
        TIMx->EGR = TIM_EGR_UG;                           \
    } while (0)

#ifdef __cplusplus
}
#endif

#endif /* __BSP_GPIO_H */