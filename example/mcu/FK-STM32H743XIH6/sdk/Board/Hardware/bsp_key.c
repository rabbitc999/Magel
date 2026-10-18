#include "bsp_key.h"
#include "bsp_timer.h"
#include "bsp_key_sm.h"
#include "bsp_periph.h"
#include "stm32h7xx_ll_gpio.h"

#define KEY_MODE_PORT          GPIOA
#define KEY_MODE_PIN           GPIO_PIN_2
#define KEY_MODE_PULL          IN_U
#define KEY_MODE_ACTIVE_LEVEL  0

#define KEY_ENTER_PORT         GPIOC
#define KEY_ENTER_PIN          GPIO_PIN_1
#define KEY_ENTER_PULL         IN_U
#define KEY_ENTER_ACTIVE_LEVEL 0

#define KEY_UP_PORT            GPIOC
#define KEY_UP_PIN             GPIO_PIN_3
#define KEY_UP_PULL            IN_D
#define KEY_UP_ACTIVE_LEVEL    1

#define KEY_DOWN_PORT          GPIOI
#define KEY_DOWN_PIN           GPIO_PIN_8
#define KEY_DOWN_PULL          IN_U
#define KEY_DOWN_ACTIVE_LEVEL  0

/**
 * @brief 按键GPIO配置结构体数组
 * @details 定义每个按键的GPIO端口、引脚和上下拉配置
 */
typedef struct
{
    GPIO_TypeDef *port;  /* GPIO端口 */
    uint16_t pin;        /* GPIO引脚 */
    uint32_t mode;       /* 上下拉配置 */
    uint8_t activeLevel; /* 按键激活电平（0为低电平激活，1为高电平激活） */
} KeyGpioConfig_T;

/**
 * @brief 按键GPIO配置表
 * @details 根据硬件设计定义每个按键的GPIO配置参数
 */
static const KeyGpioConfig_T s_KeyGpioConfig[KEY_NUMBER] = {
    /* MODE按键 */
    { KEY_MODE_PORT, KEY_MODE_PIN, KEY_MODE_PULL, KEY_MODE_ACTIVE_LEVEL },
    /* ENTER按键 */
    { KEY_ENTER_PORT, KEY_ENTER_PIN, KEY_ENTER_PULL, KEY_ENTER_ACTIVE_LEVEL },
    /* UP按键 */
    { KEY_UP_PORT, KEY_UP_PIN, KEY_UP_PULL, KEY_UP_ACTIVE_LEVEL },
    /* DOWN按键 */
    { KEY_DOWN_PORT, KEY_DOWN_PIN, KEY_DOWN_PULL, KEY_DOWN_ACTIVE_LEVEL }
};

/* 按键状态机数组 */
static KeySM_T s_KeySm[KEY_NUMBER];

/**
 * @brief  MODE键输入函数
 * @retval 1 按键按下；0 按键释放
 */
static uint8_t Key_ModeInput(void)
{
    return (Key_GetVoltage(KEY_MODE) == KEY_VOLATGE_PRESSED) ? 1 : 0;
}

/**
 * @brief  ENTER键输入函数
 * @retval 1 按键按下；0 按键释放
 */
static uint8_t Key_EnterInput(void)
{
    return (Key_GetVoltage(KEY_ENTER) == KEY_VOLATGE_PRESSED) ? 1 : 0;
}

/**
 * @brief  UP键输入函数
 * @retval 1 按键按下；0 按键释放
 */
static uint8_t Key_UpInput(void)
{
    return (Key_GetVoltage(KEY_UP) == KEY_VOLATGE_PRESSED) ? 1 : 0;
}

/**
 * @brief  DOWN键输入函数
 * @retval 1 按键按下；0 按键释放
 */
static uint8_t Key_DownInput(void)
{
    return (Key_GetVoltage(KEY_DOWN) == KEY_VOLATGE_PRESSED) ? 1 : 0;
}

/**
 * @brief  按键硬件初始化
 * @note   初始化所有按键相关的GPIO引脚，配置为输入模式并设置上下拉电阻，
 *         使能GPIO时钟、配置上下拉、初始化所有按键状态机
 */
void Key_Init(void)
{
    /* 开启时钟 */
    BSP_GPIO_ClkEnable(KEY_MODE_PORT);
    BSP_GPIO_ClkEnable(KEY_ENTER_PORT);
    BSP_GPIO_ClkEnable(KEY_UP_PORT);
    BSP_GPIO_ClkEnable(KEY_DOWN_PORT);

    /* 遍历所有按键，配置GPIO */
    for (uint8_t i = 0; i < KEY_NUMBER; i++)
    {
        BSP_GPIO_Init(s_KeyGpioConfig[i].port, s_KeyGpioConfig[i].pin, s_KeyGpioConfig[i].mode);
    }

    /* 初始化所有按键状态机 */
    KeySM_Init(&s_KeySm[KEY_MODE], Key_ModeInput);
    KeySM_Init(&s_KeySm[KEY_ENTER], Key_EnterInput);
    KeySM_Init(&s_KeySm[KEY_UP], Key_UpInput);
    KeySM_Init(&s_KeySm[KEY_DOWN], Key_DownInput);

    /* 设置特殊按键事件 */
    s_KeySm[KEY_MODE].doubleEnable = 1;
    s_KeySm[KEY_UP].repeatEnable = 1;
    s_KeySm[KEY_DOWN].repeatEnable = 1;
}

/**
 * @brief  获取按键电压状态
 * @param  key 按键枚举值
 * @retval KEY_VOLATGE_PRESSED 按键按下
 * @retval KEY_VOLATGE_RELEASED 按键释放
 * @note   读取指定按键的GPIO引脚电平，返回对应的电压状态
 */
KeyVolatge_T Key_GetVoltage(KeyEnum_T key)
{
    /* 参数检查 */
    if (key >= KEY_NUMBER)
    {
        return KEY_VOLATGE_RELEASED;
    }

    /* 读取GPIO电平 */
    const KeyGpioConfig_T *config = &s_KeyGpioConfig[key];
    uint8_t pinState = BSP_GPIO_ReadPin(config->port, config->pin);

    /* 根据激活电平返回按键状态 */
    if (config->activeLevel)
    {
        /* 高电平激活 */
        return (pinState) ? KEY_VOLATGE_PRESSED : KEY_VOLATGE_RELEASED;
    }
    else
    {
        /* 低电平激活 */
        return (pinState) ? KEY_VOLATGE_RELEASED : KEY_VOLATGE_PRESSED;
    }
}

/**
 * @brief  按键状态机处理函数
 * @note   处理所有按键的状态机逻辑，应在主循环中定期调用
 */
volatile uint32_t g_KeyScanCount = 0;
volatile uint32_t g_KeyLastScanUs = 0;
volatile uint32_t g_KeyMaxScanUs = 0;

void Key_Tick(void)
{
    uint64_t startTick = Timer_GetTicks();
    /* 调用状态机处理函数 */
    KeySM_Process(&s_KeySm[KEY_MODE]);
    KeySM_Process(&s_KeySm[KEY_ENTER]);
    KeySM_Process(&s_KeySm[KEY_UP]);
    KeySM_Process(&s_KeySm[KEY_DOWN]);
    g_KeyLastScanUs = (uint32_t)(Timer_GetTicks() - startTick);
    if (g_KeyLastScanUs > g_KeyMaxScanUs) { g_KeyMaxScanUs = g_KeyLastScanUs; }
    g_KeyScanCount++;
}

/**
 * @brief  获取按键状态机指针
 * @param  key 按键枚举值
 * @retval 指向按键状态机的指针，无效时返回NULL
 */
KeySM_T *Key_GetSm(KeyEnum_T key)
{
    if (key >= KEY_NUMBER) { return NULL; }
    return &s_KeySm[key];
}

/**
 * @brief  设置按键释放回调函数
 * @param  key 按键枚举值
 * @param  callback 回调函数指针
 */
void Key_SetUpCallback(KeyEnum_T key, void (*callback)(void))
{
    if (key >= KEY_NUMBER) return;
    KeySM_SetUpCallback(&s_KeySm[key], callback);
}

/**
 * @brief  设置按键按下回调函数
 * @param  key 按键枚举值
 * @param  callback 回调函数指针
 */
void Key_SetDownCallback(KeyEnum_T key, void (*callback)(void))
{
    if (key >= KEY_NUMBER) return;
    KeySM_SetDownCallback(&s_KeySm[key], callback);
}

/**
 * @brief  设置单击回调函数
 * @param  key 按键枚举值
 * @param  callback 回调函数指针
 */
void Key_SetClickCallback(KeyEnum_T key, void (*callback)(void))
{
    if (key >= KEY_NUMBER) return;
    KeySM_SetClickCallback(&s_KeySm[key], callback);
}

/**
 * @brief  设置长按回调函数
 * @param  key 按键枚举值
 * @param  callback 回调函数指针
 */
void Key_SetLongCallback(KeyEnum_T key, void (*callback)(void))
{
    if (key >= KEY_NUMBER) return;
    KeySM_SetLongCallback(&s_KeySm[key], callback);
}

/**
 * @brief  设置双击回调函数
 * @param  key 按键枚举值
 * @param  callback 回调函数指针
 */
void Key_SetDoubleClickCallback(KeyEnum_T key, void (*callback)(void))
{
    if (key >= KEY_NUMBER) return;
    KeySM_SetDoubleClickCallback(&s_KeySm[key], callback);
}

/**
 * @brief  设置连击回调函数
 * @param  key 按键枚举值
 * @param  callback 回调函数指针
 */
void Key_SetRepeatCallback(KeyEnum_T key, void (*callback)(void))
{
    if (key >= KEY_NUMBER) return;
    KeySM_SetRepeatCallback(&s_KeySm[key], callback);
}

/**
 * @brief  设置空闲回调函数
 * @param  key 按键枚举值
 * @param  callback 回调函数指针
 */
void Key_SetIdleCallback(KeyEnum_T key, void (*callback)(void))
{
    if (key >= KEY_NUMBER) return;
    KeySM_SetIdleCallback(&s_KeySm[key], callback);
}

/**
 * @brief  检测按键单击事件
 * @param  key 按键枚举值
 * @retval 1 检测到单击事件；0 未检测到
 */
uint8_t Key_IsClicked(KeyEnum_T key)
{
    if (key >= KEY_NUMBER) return 0;
    return KeySM_IsClicked(&s_KeySm[key]);
}

/**
 * @brief  检测按键双击事件
 * @param  key 按键枚举值
 * @retval 1 检测到双击事件；0 未检测到
 */
uint8_t Key_IsDoubleClicked(KeyEnum_T key)
{
    if (key >= KEY_NUMBER) return 0;
    return KeySM_IsDoubleClicked(&s_KeySm[key]);
}

/**
 * @brief  检测按键长按事件
 * @param  key 按键枚举值
 * @retval 1 检测到长按事件；0 未检测到
 */
uint8_t Key_IsLongPressed(KeyEnum_T key)
{
    if (key >= KEY_NUMBER) return 0;
    return KeySM_IsLongPressed(&s_KeySm[key]);
}

/**
 * @brief  检测按键连击事件
 * @param  key 按键枚举值
 * @retval 1 检测到连击事件；0 未检测到
 */
uint8_t Key_IsRepeated(KeyEnum_T key)
{
    if (key >= KEY_NUMBER) return 0;
    return KeySM_IsRepeated(&s_KeySm[key]);
}

/**
 * @brief  检测按键特定事件
 * @param  key 按键枚举值
 * @param  event 事件类型
 * @retval 1 检测到指定事件；0 未检测到
 */
uint8_t Key_HasEvent(KeyEnum_T key, KeyEvent_T event)
{
    if (key >= KEY_NUMBER) return 0;
    return KeySM_HasEvent(&s_KeySm[key], event);
}

/**
 * @brief  检测按键是否具有事件
 * @param  key 按键枚举值
 * @retval 1 检测到指定事件；0 未检测到
 */
uint8_t Key_IsAction(KeyEnum_T key)
{
    if (key >= KEY_NUMBER) return 0;
    return KeySM_IsAction(&s_KeySm[key]);
}

/**
 * @brief  清除按键特定事件
 * @param  key 按键枚举值
 * @param  event 要清除的事件类型
 */
void Key_ClearEvent(KeyEnum_T key, KeyEvent_T event)
{
    if (key >= KEY_NUMBER) return;
    KeySM_ClearEvent(&s_KeySm[key], event);
}

/**
 * @brief  重置按键状态机状态
 * @param  key 按键枚举值
 */
void Key_ResetState(KeyEnum_T key)
{
    if (key >= KEY_NUMBER) return;
    KeySM_ResetState(&s_KeySm[key]);
}

/**
 * @brief  检测任意按键是否具有事件
 * @retval 1 检测到指定事件；0 未检测到
 */
uint8_t Key_IsAnyKeyPressed(void)
{
    uint8_t flag = 0;
    for (int i = 0; i < KEY_NUMBER; i++)
    {
        flag |= Key_IsAction(i);
        Key_ClearEvent(i, KEY_EVENT_ALL);
    }
    return flag;
}

/**
 * @brief  设置按键特定事件
 * @param  key 按键枚举值
 * @param  event 要设置的事件类型
 * @note   此函数用于设置指定的事件标志，常用于虚拟按键的事件触发，
 *         可以用于模拟物理按键的按下、释放等操作，适用于自动化测试或软件触发的按键事件
 */
void Key_SetEvent(KeyEnum_T key, KeyEvent_T event)
{
    if (key >= KEY_NUMBER) return;
    KeySM_SetEvent(&s_KeySm[key], event);
}

/**
 * @brief  清除所有按键事件
 */
void Key_ClearAll(void)
{
    for (int i = 0; i < KEY_NUMBER; i++)
    {
        Key_ResetState(i);
    }
}
