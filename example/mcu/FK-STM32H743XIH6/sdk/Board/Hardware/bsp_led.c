#include "bsp_led.h"
#include "bsp_periph.h"

#define LED1_GPIO_PORT GPIOE
#define LED1_GPIO_PIN  GPIO_PIN_4

#define LED2_GPIO_PORT GPIOI
#define LED2_GPIO_PIN  GPIO_PIN_9

/**
 * @brief  初始化LED相关的GPIO引脚
 */
void LED_Init(void)
{
    BSP_GPIO_Init(LED1_GPIO_PORT, LED1_GPIO_PIN, OUT_PP);
    BSP_GPIO_Init(LED2_GPIO_PORT, LED2_GPIO_PIN, OUT_PP);

    LED1_OFF();
    LED2_OFF();
}

/**
 * @brief  点亮LED1
 * @note   LED1引脚为GPIOE_PIN_4，低电平点亮
 */
void LED1_ON(void)
{
    BSP_GPIO_WritePin(LED1_GPIO_PORT, LED1_GPIO_PIN, 0);
}

/**
 * @brief  熄灭LED1
 * @note   LED1引脚为GPIOE_PIN_4，高电平熄灭
 */
void LED1_OFF(void)
{
    BSP_GPIO_WritePin(LED1_GPIO_PORT, LED1_GPIO_PIN, 1);
}

/**
 * @brief  翻转LED1状态
 */
void LED1_Turn(void)
{
    BSP_GPIO_TogglePin(LED1_GPIO_PORT, LED1_GPIO_PIN);
}

/**
 * @brief  点亮LED2
 * @note   LED2引脚为GPIOI_PIN_9，低电平点亮
 */
void LED2_ON(void)
{
    BSP_GPIO_WritePin(LED2_GPIO_PORT, LED2_GPIO_PIN, 0);
}

/**
 * @brief  熄灭LED2
 * @note   LED2引脚为GPIOI_PIN_9，高电平熄灭
 */
void LED2_OFF(void)
{
    BSP_GPIO_WritePin(LED2_GPIO_PORT, LED2_GPIO_PIN, 1);
}

/**
 * @brief  翻转LED2状态
 */
void LED2_Turn(void)
{
    BSP_GPIO_TogglePin(LED2_GPIO_PORT, LED2_GPIO_PIN);
}
