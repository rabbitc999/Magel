#include "bsp_soft_i2c.h"
#include "bsp_pca9685.h"
#include <math.h>

#define PCA_SERVO_DEFAULT_ANGLE 90  // 定义默认舵机角度为90度
#define PCA_I2C_ADDRESS         0x82
#define PCA_I2C_REG_MODEL       0x00
#define PCA_I2C_REG_LED0_ON_L   0x06
#define PCA_I2C_REG_LED0_ON_H   0x07
#define PCA_I2C_REG_LED0_OFF_L  0x08
#define PCA_I2C_REG_LED0_OFF_H  0x09
#define PCA_I2C_REG_PRESCALE    0xFE

/**
 * @brief  PCA9685初始化函数（复位并设置200Hz PWM频率，关闭各通道输出）
 */
void PCA9685_Init(void)
{
    PCA9685_Write(PCA_I2C_REG_MODEL, 0x00);
    PCA9685_SetFreq(200);
    PCA9685_SetPWM(0, 0, 0);
    PCA9685_SetPWM(1, 0, 0);
    PCA9685_SetPWM(2, 0, 0);
    PCA9685_SetPWM(3, 0, 0);
    PCA9685_SetPWM(4, 0, 0);
    PCA9685_SetPWM(5, 0, 0);
    PCA9685_SetPWM(6, 0, 0);
    PCA9685_SetPWM(7, 0, 0);
    PCA9685_SetPWM(8, 0, 100);  // 第8个通道用于控制LED灯，设置为0表示关闭LED灯
    PCA9685_SetPWM(9, 0, 100);  // 第9个通道用于控制LED灯，设置为0表示关闭LED灯
    DWT_DelayMs(100);
}

/**
 * @brief  PCA9685写寄存器函数
 * @param  addr 寄存器地址
 * @param  data 要写入的数据
 */
void PCA9685_Write(uint8_t addr, uint8_t data)  //   addr 表示要写入数据的寄存器地址，data 表示要写入的数据
{
    SoftI2C_WriteByte(PCA_I2C_ADDRESS, addr, data);
}

/**
 * @brief  PCA9685读寄存器函数
 * @param  addr 寄存器地址
 * @retval 从指定寄存器读取到的数据
 */
uint8_t PCA9685_Read(uint8_t addr)  //   addr 表示要读取数据的寄存器地址
{
    return SoftI2C_ReadReg(PCA_I2C_ADDRESS, addr);  //   返回读取到的数据。
}

/**
 * @brief  PCA9685设置PWM输出函数
 * @param  num PWM通道号，范围为0-15
 * @param  on PWM的起始位置，范围为0-4095
 * @param  off PWM的结束位置，范围为0-4095
 */
void PCA9685_SetPWM(uint8_t num, uint32_t on, uint32_t off)  // num 表示 PWM 通道号，on 表示 PWM 的起始位置，off 表示 PWM 的结束位置（即从高电平切换到低电平的时刻）
{
#ifdef CAR_BRINGUP_NO_ACTUATION
    if (num < 8)
    {
        on = 0;
        off = 4096;
    } /* Full off for actuator channels. */
#endif
    uint8_t SendArray[] = { on & 0xFF, on >> 8, off & 0xFF, off >> 8 };
    SoftI2C_WriteMultiBytes(PCA_I2C_ADDRESS, PCA_I2C_REG_LED0_ON_L + 4 * num, SendArray, 4);
}

/**
 * @brief  PCA9685设置PWM频率函数
 * @param  freq PWM频率，单位为赫兹
 */
void PCA9685_SetFreq(float freq)
{
    uint8_t prescale, oldmode, newmode;         // 定义了三个无符号 8 位整型变量 用于存储预分频器值、旧的模式寄存器值和新的模式寄存器值
    double prescaleval;                         // 定义了一个双精度浮点型变量 prescaleval，用于计算预分频器的值。
    freq *= 0.98;                               // 将传入的频率值乘以 0.98，这是为了微调频率值以适应 PCA9685 的实际需求
    prescaleval = 25000000;                     // 这是 PCA9685 内部振荡器的频率
    prescaleval /= 4096;                        // 每个周期从0计数到4095，除以 4096，得到每个计数器周期的时间，
    prescaleval /= freq;                        // 除以所需的频率值，得到预分频器的值。
    prescaleval -= 1;                           // 减去 1，得到最终的预分频器值
    prescale = floor(prescaleval + 0.5f);       // 将计算得到的预分频器值四舍五入取整，并将其赋值给 prescale 变量。
    oldmode = PCA9685_Read(PCA_I2C_REG_MODEL);  // 通过调用 PCA9685_Read 函数读取当前 PCA9685 寄存器中的模式值，并将其存储在 oldmode 变量中。

    newmode = (oldmode & 0x7F) | 0x10;              // 根据旧的模式值计算出新的模式值，将最高位清零（bit 7）并将第 5 位设为1（bit 4），表示将 PCA9685 设置为睡眠模式。
    PCA9685_Write(PCA_I2C_REG_MODEL, newmode);      // 将新的模式值写入 PCA9685 的模式寄存器。
    PCA9685_Write(PCA_I2C_REG_PRESCALE, prescale);  // 将计算得到的预分频器值写入 PCA9685 的预分频器寄存器。
    PCA9685_Write(PCA_I2C_REG_MODEL, oldmode);      // 恢复旧的模式值。

#if STSTEM_USE_RTOS
    if (xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED)
    {
        vTaskDelay(5);  // 延时 5 毫秒，等待 PCA9685 完全启动
    }
    else
    {
#endif
        DWT_DelayMs(5);
#if STSTEM_USE_RTOS
    }
#endif

    PCA9685_Write(PCA_I2C_REG_MODEL, oldmode | 0xa1);  // 将模式值的最高位和第 1 位设为1，表示将 PCA9685 设置为正常工作模式。
}

/*
void setAngle(uint8_t num, uint16_t angle)
{
    uint32_t off = 0;
    off = (uint32_t)(103 + angle * 2.28);  // 360度舵机，每转动一度=2.28   0度起始位置：103
    PCA9685_setPWM(num, 0, angle);
}
*/