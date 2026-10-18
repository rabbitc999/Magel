#include "bsp_light.h"
#include "bsp_periph.h"
#include "bsp_system.h"

static TIM_HandleTypeDef s_LightTimHandle;
uint8_t g_LightBrightness = 0;

/**
 * @brief  初始化调光模块（已改用PCA9685，无需额外初始化）
 */
void Light_Init(void)
{
    // 由于换为了PCA9685，因此可以不用考虑初始化
}

/**
 * @brief  设置灯光亮度（前照灯）
 * @param  brightness 亮度值，范围0-100
 */
void Light_SetBrightness(uint8_t brightness)
{
    if (brightness >= 100) { brightness = 100; }
    PCA9685_SetPWM(8, 0, 10 * brightness);  // 第8个通道用于控制LED灯，设置为0表示关闭LED灯
    PCA9685_SetPWM(9, 0, 10 * brightness);  // 第9个通道用于控制LED灯，设置为0表示关闭LED灯
    g_LightBrightness = brightness;
}

/**
 * @brief  关闭灯光（前照灯）
 */
void Light_TurnOff(void)
{
    Light_SetBrightness(0);
}
