#include "bsp_wdg.h"
#include "bsp_uart.h"

#if SYSTEM_DEBUG_MODE
#define IWDG_ENABLE_WWDG 0 /* 开启看门狗 */
#else
#define IWDG_ENABLE_WWDG 0 /* 开启看门狗 */
#endif
static IWDG_HandleTypeDef s_IwdgHandle; /* 独立看门狗句柄 */
uint32_t g_CPU_Serial[3];

/**
 * @brief  初始化独立看门狗
 * @param  prer IWDG分频系数，IWDG_PRESCALER_4~IWDG_PRESCALER_256，对应4~256分频
 *   @arg  分频因子 = 4 * 2^prer. 但最大值只能是256!
 * @param  rlr 自动重装载值，0~0XFFF
 * @note   时间计算(大概):Tout=((4 * 2^prer) * rlr) / 32 (ms)，同时读取CPU唯一ID到g_CPU_Serial
 */
void IWDG_Init(uint32_t prer, uint16_t rlr)
{
    /* STM32H7系列的唯一ID地址 */
    g_CPU_Serial[0] = *(__IO uint32_t *)(0x1FF1E800 + 0);
    g_CPU_Serial[1] = *(__IO uint32_t *)(0x1FF1E800 + 4);
    g_CPU_Serial[2] = *(__IO uint32_t *)(0x1FF1E800 + 8);

    s_IwdgHandle.Instance = IWDG1;
    s_IwdgHandle.Init.Prescaler = prer;             /* 设置IWDG分频系数 */
    s_IwdgHandle.Init.Reload = rlr;                 /* 从加载寄存器 IWDG->RLR 重装载值 */
    s_IwdgHandle.Init.Window = IWDG_WINDOW_DISABLE; /* 关闭窗口功能 */
#if IWDG_ENABLE_WWDG
    if (HAL_IWDG_Init(&s_IwdgHandle) != HAL_OK) /* 初始化IWDG并使能 */
    {
        Assert_Failed(__FILE__, __LINE__);
    }
#endif
}

/**
 * @brief  喂狗（重载独立看门狗计数器）
 */
void IWDG_Feed(void)
{
#if IWDG_ENABLE_WWDG
    HAL_IWDG_Refresh(&s_IwdgHandle); /* 喂狗 */
#endif
}
