#include "bsp_soft_i2c.h"
#include "bsp_timer.h"
#include "bsp_dwt.h"
#include "bsp_periph.h"
#include "bsp_system.h"
#include "stm32h7xx_ll_gpio.h"

/* 软件I2C引脚配置宏 */
#define SOFT_I2C_SCL_PORT GPIOH
#define SOFT_I2C_SCL_PIN  GPIO_PIN_11
#define SOFT_I2C_SDA_PORT GPIOH
#define SOFT_I2C_SDA_PIN  GPIO_PIN_12

/**
 * @brief  I2C延时函数
 */
static void I2C_Delay(void)
{
    DWT_DelayUs(10);
}

/**
 * @brief  设置SCL电平
 * @param  pinState SCL引脚电平状态(0或1)
 */
static void SoftI2C_WR_SCL(uint8_t pinState)
{
    BSP_GPIO_WritePin(SOFT_I2C_SCL_PORT, SOFT_I2C_SCL_PIN, pinState);
    I2C_Delay();
}

/**
 * @brief  设置SDA电平
 * @param  pinState SDA引脚电平状态(0或1)
 */
static void SoftI2C_WR_SDA(uint8_t pinState)
{
    BSP_GPIO_WritePin(SOFT_I2C_SDA_PORT, SOFT_I2C_SDA_PIN, pinState);
    I2C_Delay();
}

/**
 * @brief  读取SDA引脚状态
 * @retval  SDA引脚状态(0或1)
 */
static uint8_t SoftI2C_RD_SDA(void)
{
    uint8_t pinState;

    /* 先设置SDA为输入模式 */
    LL_GPIO_SetPinMode(SOFT_I2C_SDA_PORT, SOFT_I2C_SDA_PIN, LL_GPIO_MODE_INPUT);

    /* 读取电平 */
    pinState = BSP_GPIO_ReadPin(SOFT_I2C_SDA_PORT, SOFT_I2C_SDA_PIN);

    /* 恢复为输出模式 */
    LL_GPIO_SetPinMode(SOFT_I2C_SDA_PORT, SOFT_I2C_SDA_PIN, LL_GPIO_MODE_OUTPUT);

    /* 适当延时 */
    I2C_Delay();

    return pinState;
}

/**
 * @brief  发送起始信号
 */
void SoftI2C_Start(void)
{
    SoftI2C_WR_SDA(1);
    SoftI2C_WR_SCL(1);
    SoftI2C_WR_SDA(0);
    SoftI2C_WR_SCL(0);
}

/**
 * @brief  发送停止信号
 */
void SoftI2C_Stop(void)
{
    SoftI2C_WR_SDA(0);
    SoftI2C_WR_SCL(1);
    SoftI2C_WR_SDA(1);
}

/**
 * @brief  发送一个字节
 * @param  Byte 要发送的字节
 */
void SoftI2C_SendByte(uint8_t Byte)
{
    uint8_t i;
    for (i = 0; i < 8; i++)
    {
        SoftI2C_WR_SDA(Byte & (0x80 >> i));
        SoftI2C_WR_SCL(1);
        SoftI2C_WR_SCL(0);
    }
}

/**
 * @brief  接收一个字节
 * @retval  接收到的字节
 */
uint8_t SoftI2C_ReceiveByte(void)
{
    uint8_t i, Byte = 0x00;
    SoftI2C_WR_SDA(1);  // 释放SDA
    for (i = 0; i < 8; i++)
    {
        SoftI2C_WR_SCL(1);
        if (SoftI2C_RD_SDA()) { Byte |= (0x80 >> i); }
        SoftI2C_WR_SCL(0);
    }
    return Byte;
}

/**
 * @brief  发送应答信号
 * @param  AckBit 应答位(0-应答, 1-非应答)
 */
void SoftI2C_SendAck(uint8_t AckBit)
{
    SoftI2C_WR_SDA(AckBit);
    SoftI2C_WR_SCL(1);
    SoftI2C_WR_SCL(0);
}

/**
 * @brief  接收应答信号
 * @retval  0-收到应答, 1-收到非应答
 */
uint8_t SoftI2C_ReceiveAck(void)
{
    uint8_t AckBit;
    SoftI2C_WR_SDA(1);  // 释放SDA
    SoftI2C_WR_SCL(1);
    AckBit = SoftI2C_RD_SDA();
    SoftI2C_WR_SCL(0);
    return AckBit;
}

/**
 * @brief  初始化软件I2C设备
 * @retval  操作结果
 */
int8_t SoftI2C_Init(void)
{
    /* 初始化GPIO */
    BSP_GPIO_FastModeInit(SOFT_I2C_SCL_PORT, SOFT_I2C_SCL_PIN, OUT_OD_U);
    BSP_GPIO_FastModeInit(SOFT_I2C_SDA_PORT, SOFT_I2C_SDA_PIN, OUT_OD_U);

    /* 初始状态: SCL和SDA都置高 */
    BSP_GPIO_WritePin(SOFT_I2C_SCL_PORT, SOFT_I2C_SCL_PIN, 1);
    BSP_GPIO_WritePin(SOFT_I2C_SDA_PORT, SOFT_I2C_SDA_PIN, 1);

    return SOFT_I2C_RESULT_OK;
}

/**
 * @brief  开启互斥锁
 */
void SoftI2C_Lock(void)
{
}

/**
 * @brief  释放互斥锁
 */
void SoftI2C_UnLock(void)
{
}

/**
 * @brief  写入单个字节到I2C设备
 * @param  devAddr 设备地址(7位)
 * @param  regAddr 寄存器地址
 * @param  data 要写入的数据
 * @retval  操作结果
 */
int8_t SoftI2C_WriteByte(uint8_t devAddr, uint8_t regAddr, uint8_t data)
{
    /* 发送起始信号 */
    SoftI2C_Lock();
    SoftI2C_Start();

    /* 发送设备地址(写模式) */
    SoftI2C_SendByte(devAddr | SOFT_I2C_DIRECTION_WRITE);

    /* 等待应答 */
    if (SoftI2C_ReceiveAck())
    {
        SoftI2C_Stop();
        SoftI2C_UnLock();
        return SOFT_I2C_RESULT_NACK;
    }

    /* 发送寄存器地址 */
    SoftI2C_SendByte(regAddr);

    /* 等待应答 */
    if (SoftI2C_ReceiveAck())
    {
        SoftI2C_Stop();
        SoftI2C_UnLock();
        return SOFT_I2C_RESULT_NACK;
    }

    /* 发送数据 */
    SoftI2C_SendByte(data);

    /* 等待应答 */
    if (SoftI2C_ReceiveAck())
    {
        SoftI2C_Stop();
        SoftI2C_UnLock();
        return SOFT_I2C_RESULT_NACK;
    }

    /* 发送停止信号 */
    SoftI2C_Stop();
    SoftI2C_UnLock();

    return SOFT_I2C_RESULT_OK;
}

/**
 * @brief  从I2C设备读取单个字节
 * @param  devAddr 设备地址(7位)
 * @param  regAddr 寄存器地址
 * @param  data 读取数据的缓冲区指针
 * @retval  操作结果
 */
int8_t SoftI2C_ReadByte(uint8_t devAddr, uint8_t regAddr, uint8_t *data)
{
    /* 发送起始信号 */
    SoftI2C_Lock();
    SoftI2C_Start();

    /* 发送设备地址(写模式) */
    SoftI2C_SendByte(devAddr | SOFT_I2C_DIRECTION_WRITE);

    /* 等待应答 */
    SoftI2C_ReceiveAck();

    /* 发送寄存器地址 */
    SoftI2C_SendByte(regAddr);

    /* 等待应答 */
    if (SoftI2C_ReceiveAck())
    {
        SoftI2C_Stop();
        SoftI2C_UnLock();
        return SOFT_I2C_RESULT_NACK;
    }

    /* 重新发送起始信号 */
    SoftI2C_Start();

    /* 发送设备地址(读模式) */
    SoftI2C_SendByte(devAddr | SOFT_I2C_DIRECTION_READ);

    /* 等待应答 */
    if (SoftI2C_ReceiveAck())
    {
        SoftI2C_Stop();
        SoftI2C_UnLock();
        return SOFT_I2C_RESULT_NACK;
    }

    /* 接收数据 */
    *data = SoftI2C_ReceiveByte();

    /* 发送非应答信号 */
    SoftI2C_SendAck(1);

    /* 发送停止信号 */
    SoftI2C_Stop();
    SoftI2C_UnLock();

    return SOFT_I2C_RESULT_OK;
}

/**
 * @brief  从I2C设备读取单个字节并返回
 * @param  devAddr 设备地址(7位)
 * @param  regAddr 寄存器地址
 * @retval  读取的数据
 */
int8_t SoftI2C_ReadReg(uint8_t devAddr, uint8_t regAddr)
{
    uint8_t data = 0x00;
    SoftI2C_ReadByte(devAddr, regAddr, &data);
    return data;
}

/**
 * @brief  写入多个字节到I2C设备
 * @param  devAddr 设备地址(7位)
 * @param  regAddr 寄存器地址
 * @param  data 要写入的数据缓冲区指针
 * @param  length 数据长度
 * @retval  操作结果
 */
int8_t SoftI2C_WriteMultiBytes(uint8_t devAddr, uint8_t regAddr, const uint8_t *data, uint16_t length)
{
    uint16_t i = 0;

    /* 发送起始信号 */
    SoftI2C_Lock();
    SoftI2C_Start();

    /* 发送设备地址(写模式) */
    SoftI2C_SendByte(devAddr | SOFT_I2C_DIRECTION_WRITE);

    /* 等待应答 */
    if (SoftI2C_ReceiveAck())
    {
        SoftI2C_Stop();
        SoftI2C_UnLock();
        return SOFT_I2C_RESULT_NACK;
    }

    /* 发送寄存器地址 */
    SoftI2C_SendByte(regAddr);

    /* 等待应答 */
    if (SoftI2C_ReceiveAck())
    {
        SoftI2C_Stop();
        SoftI2C_UnLock();
        return SOFT_I2C_RESULT_NACK;
    }

    /* 发送多个数据字节 */
    for (i = 0; i < length; i++)
    {
        SoftI2C_SendByte(data[i]);

        /* 等待应答 */
        if (SoftI2C_ReceiveAck())
        {
            SoftI2C_Stop();
            SoftI2C_UnLock();
            return SOFT_I2C_RESULT_NACK;
        }
    }

    /* 发送停止信号 */
    SoftI2C_Stop();
    SoftI2C_UnLock();

    return SOFT_I2C_RESULT_OK;
}

/**
 * @brief  从I2C设备读取多个字节
 * @param  devAddr 设备地址(7位)
 * @param  regAddr 寄存器地址
 * @param  data 读取数据的缓冲区指针
 * @param  length 要读取的数据长度
 * @retval  操作结果
 */
int8_t SoftI2C_ReadMultiBytes(uint8_t devAddr, uint8_t regAddr, uint8_t *data, uint16_t length)
{
    uint16_t i;

    /* 发送起始信号 */
    SoftI2C_Lock();
    SoftI2C_Start();

    /* 发送设备地址(写模式) */
    SoftI2C_SendByte(devAddr | SOFT_I2C_DIRECTION_WRITE);

    /* 等待应答 */
    if (SoftI2C_ReceiveAck())
    {
        SoftI2C_Stop();
        SoftI2C_UnLock();
        return SOFT_I2C_RESULT_NACK;
    }

    /* 发送寄存器地址 */
    SoftI2C_SendByte(regAddr);

    /* 等待应答 */
    if (SoftI2C_ReceiveAck())
    {
        SoftI2C_Stop();
        SoftI2C_UnLock();
        return SOFT_I2C_RESULT_NACK;
    }

    /* 重新发送起始信号 */
    SoftI2C_Start();

    /* 发送设备地址(读模式) */
    SoftI2C_SendByte(devAddr | SOFT_I2C_DIRECTION_READ);

    /* 等待应答 */
    if (SoftI2C_ReceiveAck())
    {
        SoftI2C_Stop();
        SoftI2C_UnLock();
        return SOFT_I2C_RESULT_NACK;
    }

    /* 接收多个数据字节 */
    for (i = 0; i < length; i++)
    {
        data[i] = SoftI2C_ReceiveByte();

        /* 发送应答或非应答信号 */
        SoftI2C_SendAck(i == length - 1);
    }

    /* 发送停止信号 */
    SoftI2C_Stop();
    SoftI2C_UnLock();

    return SOFT_I2C_RESULT_OK;
}

/**
 * @brief  探测I2C设备是否存在
 * @param  devAddr 设备地址(7位)
 * @retval  操作结果(存在:SOFT_I2C_RESULT_OK, 不存在:SOFT_I2C_RESULT_NACK)
 */
int8_t SoftI2C_ProbeDevice(uint8_t devAddr)
{
    /* 发送起始信号 */
    SoftI2C_Lock();
    SoftI2C_Start();

    /* 发送设备地址(写模式) */
    SoftI2C_SendByte(devAddr | SOFT_I2C_DIRECTION_WRITE);

    /* 等待应答 */
    if (SoftI2C_ReceiveAck())
    {
        SoftI2C_Stop();
        SoftI2C_UnLock();
        return SOFT_I2C_RESULT_NACK;
    }

    /* 发送停止信号 */
    SoftI2C_Stop();
    SoftI2C_UnLock();

    return SOFT_I2C_RESULT_OK;
}
