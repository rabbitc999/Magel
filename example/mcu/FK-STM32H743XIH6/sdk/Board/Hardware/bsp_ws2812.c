#include "bsp_periph.h"
#include "bsp_ws2812.h"

#define WS2812_NUM  2
#define WS2812_PIN  GPIO_PIN_6
#define WS2812_PORT GPIOA

/* LED颜色 */
uint32_t g_WS2812_Color[WS2812_NUM] = { 0 };

/* 当前LED颜色 */
static uint32_t s_WS2812_CurrentColor[3 * WS2812_NUM];

/* PWM定时器句柄 */
TIM_HandleTypeDef s_TimHandle;
DMA_HandleTypeDef s_DmaHandle;

/**
 * @brief  初始化WS2812驱动（配置GPIO并默认熄灭所有LED）
 */
void WS2812_Init(void)
{
    BSP_GPIO_FastModeInit(WS2812_PORT, WS2812_PIN, OUT_PP);

    // 初始化为低电平（复位状态）
    BSP_GPIO_WritePin(WS2812_PORT, WS2812_PIN, 0);

    // 默认熄灭LED灯
    WS2812_Update();
}

/**
 * @brief  RGB转换为24bit颜色
 * @param  r: 红色亮度（0-255）
 * @param  g: 绿色亮度（0-255）
 * @param  b: 蓝色亮度（0-255）
 * @retval 24bit颜色
 */
uint32_t RGB2Color(uint8_t r, uint8_t g, uint8_t b)
{
    return (r << 16) | (g << 8) | b;
}

/**
 * @brief  24bit颜色转换为RGB
 * @param  color: 24bit颜色
 * @param  r: 红色亮度（0-255）
 * @param  g: 绿色亮度（0-255）
 * @param  b: 蓝色亮度（0-255）
 */
void Color2RGB(uint32_t color, uint8_t *r, uint8_t *g, uint8_t *b)
{
    *r = (color >> 16) & 0xFF;
    *g = (color >> 8) & 0xFF;
    *b = color & 0xFF;
}

/**
 * @brief  WS2812内部延时函数（通过空指令循环延时）
 * @param  cycles 目标延时周期数
 */
static inline void WS2812_DelayCycles(uint32_t cycles)
{
    __asm volatile(
        "1: subs %0, #1\n"  // cycles--
        "bne 1b\n"          // 非0则跳回1
        : "=r"(cycles)      // 输出
        : "0"(cycles)       // 输入
    );
}

/**
 * @brief  发送1个bit数据（按WS2812时序产生高/低电平）
 * @param  bit 待发送的位，0或1
 */
static inline void WS2812_SendBit(uint8_t bit)
{
    if (bit != 0)
    {
        // 1码：高电平≈600ns，低电平≈400ns
        BSP_GPIO_WritePin(WS2812_PORT, WS2812_PIN, 1);
        WS2812_DelayCycles(128);
        BSP_GPIO_WritePin(WS2812_PORT, WS2812_PIN, 0);
        WS2812_DelayCycles(56);
    }
    else
    {
        // 0码：高电平≈300ns，低电平≈700ns
        BSP_GPIO_WritePin(WS2812_PORT, WS2812_PIN, 1);
        WS2812_DelayCycles(45);
        BSP_GPIO_WritePin(WS2812_PORT, WS2812_PIN, 0);
        WS2812_DelayCycles(106);
    }
}

/**
 * @brief  发送1个字节数据（高位先发）
 * @param  byte 待发送的字节
 */
static inline void WS2812_SendByte(uint8_t byte)
{
    for (int8_t i = 7; i >= 0; i--)
    {
        WS2812_SendBit((byte >> i) & 0x01);  // 从最高位开始发送
    }
}

/**
 * @brief  发送复位信号（低电平≥50μs）
 */
static inline void WS2812_SendReset(void)
{
    BSP_GPIO_WritePin(WS2812_PORT, WS2812_PIN, 0);
    DWT_DelayUs(75);
}

/**
 * @brief  更新LED显示（发送所有LED数据）
 * @note   发送期间会临时关闭中断以保证WS2812时序不被打断
 */
void WS2812_Update(void)
{
    // 复制颜色数据，避免发送过程中被修改
    for (uint8_t i = 0; i < WS2812_NUM; i++)
    {
        uint8_t r, g, b;
        Color2RGB(g_WS2812_Color[i], &r, &g, &b);
        s_WS2812_CurrentColor[3 * i + 0] = g;
        s_WS2812_CurrentColor[3 * i + 1] = r;
        s_WS2812_CurrentColor[3 * i + 2] = b;
    }

    WS2812_SendReset();  // 发送复位信号

    __disable_irq();  // 关闭中断，防止时序被打断
    for (uint16_t i = 0; i < 3 * WS2812_NUM; i++)
    {
        WS2812_SendByte(s_WS2812_CurrentColor[i]);
    }
    __enable_irq();  // 恢复中断

    WS2812_SendReset();  // 发送结束复位
}

/**
 * @brief  设置LED颜色(RGB格式)
 * @param  led_id LED编号（一共有2个LED，编号范围0-1）
 * @param  r 红色亮度（0-255）
 * @param  g 绿色亮度（0-255）
 * @param  b 蓝色亮度（0-255）
 */
void WS2812_SetRGB(uint8_t led_id, uint8_t r, uint8_t g, uint8_t b)
{
    g_WS2812_Color[led_id] = RGB2Color(r, g, b);
}

/**
 * @brief  设置LED颜色（24bit颜色格式）
 * @param  led_id LED编号（一共有2个LED，编号范围0-1）
 * @param  color 24bit颜色
 */
void WS2812_Set(uint8_t led_id, uint32_t color)
{
    g_WS2812_Color[led_id] = color;
}

/**
 * @brief  设置所有LED颜色（24bit颜色格式）
 * @param  color 24bit颜色
 */
void WS2812_SetAll(uint32_t color)
{
    for (uint8_t led_id = 0; led_id < WS2812_NUM; led_id++)
    {
        g_WS2812_Color[led_id] = color;
    }
}
