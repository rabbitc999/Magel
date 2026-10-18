#ifndef __BSP_SYSTEM_H
#define __BSP_SYSTEM_H

#include "stm32h7xx_hal.h"
#include "stm32h7xx_ll_rcc.h"
#include "stm32h7xx_ll_spi.h"
#include "stm32h7xx_ll_gpio.h"
#include "stm32h7xx_ll_usart.h"
#include "stm32h7xx_ll_dma.h"
#include "stm32h7xx_ll_tim.h"

#include <math.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdarg.h>

#define SYSTEM_CLOCK_MHZ  480 /* 系统时钟频率(MHz) */
#define SYSTEM_DEBUG_MODE 0   /* 系统调试标志 */
#define STSTEM_USE_RTOS   0   /* 是否使用RTOS */

#ifndef __STATIC_INLINE
#define __STATIC_INLINE static inline
#endif

/* 角度弧度转换 */
#define M_PI       3.14159265358979323846f
#define DEG2RAD(x) ((x) * M_PI / 180.0f)
#define RAD2DEG(x) ((x) * 180.0f / M_PI)

/* 调试错误宏 */
#ifdef assert
#undef assert
#endif
#define assert(x)                                                                                    \
    do                                                                                               \
    {                                                                                                \
        uint32_t code = (x);                                                                         \
        if (code == 0) { DEBUG_PRINTF("断言错误 " __FILE__ "(%d), 错误码=%d\r\n", __LINE__, code); } \
    } while (0)

/* 开关全局中断的宏 */
#define BSP_ENABLE_INT()  __set_PRIMASK(0) /* 使能全局中断 */
#define BSP_DISABLE_INT() __set_PRIMASK(1) /* 禁止全局中断 */

#ifdef __cplusplus
extern "C" {
#endif

typedef enum tagBoolean_T
{
    TRUE = 1,
    FALSE = 0
} Boolean_T;

/* 函数定义 */
void System_ClockConfig(void);
void MPU_Config(void);
void Board_Init(void);
void Assert_Failed(char *file, uint32_t line);

void Memory_Copy(void *dst, const void *src, size_t size);
void Memory_Set(void *dst, uint8_t value, size_t size);

#ifdef __cplusplus
}
#endif

/* 标准库引用 */
#include "bsp_sdram.h"
#include "bsp_wdg.h"
#include "bsp_rtc.h"
#include "bsp_timer.h"
#include "bsp_lcd.h"
#include "bsp_key_sm.h"
#include "bsp_key.h"
#include "bsp_led.h"
#include "bsp_beep.h"
#include "bsp_motor.h"
#include "bsp_battery.h"
#include "bsp_encoder.h"
#include "bsp_memory.h"
#include "bsp_qspi_w25q64.h"
#include "bsp_uart.h"
#include "bsp_usb.h"
#include "bsp_servo.h"
#include "bsp_lidar.h"
#include "bsp_nrf24l01.h"
#include "bsp_soft_i2c.h"
#include "bsp_light.h"
#include "bsp_cs100a.h"
#include "bsp_ws2812.h"
#include "bsp_bluetooth.h"
#include "bsp_esp8266.h"
#include "bsp_pwr.h"
#include "bsp_dwt.h"
#include "bsp_imu_jy901.h"
#include "bsp_touch.h"
#include "bsp_pca9685.h"

#endif
