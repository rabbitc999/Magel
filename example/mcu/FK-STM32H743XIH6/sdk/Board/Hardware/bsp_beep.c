#include "bsp_beep.h"
#include "bsp_periph.h"

#define BEEP_GPIO_PORT      GPIOE
#define BEEP_GPIO_PIN       GPIO_PIN_5
#define BEEP_TIMER_CALLBACK 20

/* 使用静态变量存储蜂鸣器命令 */
static volatile Beep_T s_Beep = { 0 };

/**
 * @brief  初始化蜂鸣器相关的GPIO引脚
 */
void Beep_Init(void)
{
    /* 配置GPIO引脚 */
    BSP_GPIO_Init(BEEP_GPIO_PORT, BEEP_GPIO_PIN, OUT_PP);

    /* 设置默认电平 */
    BSP_GPIO_WritePin(BEEP_GPIO_PORT, BEEP_GPIO_PIN, 0);
}

/**
 * @brief  蜂鸣器开启（受使能开关s_Beep.enable控制）
 */
void Beep_On(void)
{
    BSP_GPIO_WritePin(BEEP_GPIO_PORT, BEEP_GPIO_PIN, s_Beep.enable);
}

/**
 * @brief  蜂鸣器关闭
 */
void Beep_Off(void)
{
    BSP_GPIO_WritePin(BEEP_GPIO_PORT, BEEP_GPIO_PIN, 0);
}

/**
 * @brief  蜂鸣器定时器回调函数（按开/关时间交替控制蜂鸣）
 * @note   该函数应在5ms定时器中断中调用
 */
void Beep_Tick(void)
{
    /* 蜂鸣器未激活时关闭蜂鸣器并直接返回 */
    if (s_Beep.cycles > 0)
    {
        s_Beep.timer += BEEP_TIMER_CALLBACK; /* 5ms中断调用 */

        if (s_Beep.state == 0 && s_Beep.timer > s_Beep.offTime) /* 未开启 */
        {
            s_Beep.timer = 0;
            s_Beep.state = 1;
            Beep_On();
            return;
        }
        if (s_Beep.state == 1 && s_Beep.timer > s_Beep.onTime)
        {
            s_Beep.cycles--;
            s_Beep.timer = 0;
            s_Beep.state = 0;
            Beep_Off();
            return;
        }
    }
    else
    {
        s_Beep.state = 0;
        s_Beep.timer = 0;
        s_Beep.cycles = 0;
        Beep_Off();
        return;
    }
}

/**
 * @brief  发送蜂鸣控制命令
 * @param  onTime 蜂鸣持续时间(ms)
 * @param  offTime 两次蜂鸣之间的间隔时间(ms)
 * @param  cycles 蜂鸣重复次数
 */
void Beep_Play(uint32_t onTime, uint32_t offTime, uint32_t cycles)
{
    if (s_Beep.cycles > 0) { return; }

    /* 设置新命令参数 */
    s_Beep.onTime = onTime;
    s_Beep.offTime = offTime;
    s_Beep.cycles = cycles;
    s_Beep.timer = onTime;
    s_Beep.state = 0;
}

/**
 * @brief  立即停止蜂鸣
 */
void Beep_Stop(void)
{
    /* 停止蜂鸣器 */
    s_Beep.cycles = 0;
    Beep_Off();
}

/**
 * @brief  禁止蜂鸣（蜂鸣器输出恒为低电平）
 */
void Beep_Disable(void)
{
    /* 停止蜂鸣器 */
    s_Beep.enable = 0;
    Beep_Stop();
}

/**
 * @brief  允许蜂鸣（恢复蜂鸣器输出）
 */
void Beep_Enable(void)
{
    /* 停止蜂鸣器 */
    s_Beep.enable = 1;
}
