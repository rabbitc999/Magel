#ifndef _BSP_SOFT_I2C_H
#define _BSP_SOFT_I2C_H

#include "stm32h7xx_hal.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 超时时间宏定义 */
#define SOFT_I2C_TIMEOUT_MS 10U /* 超时时间: 10ms */

/* I2C操作结果枚举 */
typedef enum __SoftI2cResult_T
{
    SOFT_I2C_RESULT_OK = 0,       /* 操作成功 */
    SOFT_I2C_RESULT_ERROR = -1,   /* 操作失败 */
    SOFT_I2C_RESULT_TIMEOUT = -2, /* 超时错误 */
    SOFT_I2C_RESULT_NACK = -3,    /* NACK错误 */
    SOFT_I2C_RESULT_BUS_BUSY = -4 /* 总线忙错误 */
} SoftI2cResult_T;

/* I2C传输方向枚举 */
typedef enum __SoftI2cDirection_T
{
    SOFT_I2C_DIRECTION_WRITE = 0, /* 写操作 */
    SOFT_I2C_DIRECTION_READ = 1   /* 读操作 */
} SoftI2cDirection_T;

/* 函数声明 */
int8_t SoftI2C_Init(void);
int8_t SoftI2C_WriteByte(uint8_t devAddr, uint8_t regAddr, uint8_t data);
int8_t SoftI2C_ReadByte(uint8_t devAddr, uint8_t regAddr, uint8_t *data);
int8_t SoftI2C_ReadReg(uint8_t devAddr, uint8_t regAddr);
int8_t SoftI2C_WriteMultiBytes(uint8_t devAddr, uint8_t regAddr, const uint8_t *data, uint16_t length);
int8_t SoftI2C_ReadMultiBytes(uint8_t devAddr, uint8_t regAddr, uint8_t *data, uint16_t length);
int8_t SoftI2C_ProbeDevice(uint8_t devAddr);

#ifdef __cplusplus
}
#endif

#endif /* _BSP_SOFT_I2C_H */