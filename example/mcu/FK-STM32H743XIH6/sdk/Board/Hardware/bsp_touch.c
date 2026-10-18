/** *
 *****************************************************************************************************************************************************************************************************************
 *	@version V1.0
 *   @date    2023-12-13
 *	@author  反客科技
 *************************************************************************************************
 *  @description
 *
 *	实验平台：反客STM32H743XIH6核心板 （型号：FK743M5-XIH6）+ 反客800*480分辨率的RGB屏幕
 *	淘宝地址：https://shop212360197.taobao.com
 *	QQ交流群：536665479
 *
>>>>>	文件说明：
 *
 *  1.触摸屏相关的操作函数
 * 	2.使用模拟IIC
 *	3.通信速度默认为100KHz
 *
 ****************************************************************************************************************************************************************************************************************************************************FANKE*******
 ***/

#include "bsp_touch.h"
#include <stdio.h>

#define Touch_IIC_SCL_CLK_ENABLE __HAL_RCC_GPIOG_CLK_ENABLE()  // SCL 引脚时钟
#define Touch_IIC_SCL_PORT       GPIOG                         // SCL 引脚端口
#define Touch_IIC_SCL_PIN        GPIO_PIN_3                    // SCL 引脚

#define Touch_IIC_SDA_CLK_ENABLE __HAL_RCC_GPIOG_CLK_ENABLE()  // SDA 引脚时钟
#define Touch_IIC_SDA_PORT       GPIOG                         // SDA 引脚端口
#define Touch_IIC_SDA_PIN        GPIO_PIN_7                    // SDA 引脚

#define Touch_INT_CLK_ENABLE     __HAL_RCC_GPIOI_CLK_ENABLE()  // INT 引脚时钟
#define Touch_INT_PORT           GPIOI                         // INT 引脚端口
#define Touch_INT_PIN            GPIO_PIN_11                   // INT 引脚

#define Touch_RST_CLK_ENABLE     __HAL_RCC_GPIOI_CLK_ENABLE()  // RST 引脚时钟
#define Touch_RST_PORT           GPIOI                         // RST 引脚端口
#define Touch_RST_PIN            GPIO_PIN_10                   // RST 引脚

/*------------------------------------------ IIC相关定义 -------------------------------------------------*/

#define ACK_OK                   1  // 响应正常
#define ACK_ERR                  0  // 响应错误

// IIC通信延时，Touch_IIC_Delay()函数使用，
//	通信速度在100KHz左右
#define IIC_DelayVaule           20

/*-------------------------------------------- IO口操作 ---------------------------------------------------*/

#define Touch_IIC_SCL(a)                                                        \
    if (a)                                                                      \
        HAL_GPIO_WritePin(Touch_IIC_SCL_PORT, Touch_IIC_SCL_PIN, GPIO_PIN_SET); \
    else                                                                        \
        HAL_GPIO_WritePin(Touch_IIC_SCL_PORT, Touch_IIC_SCL_PIN, GPIO_PIN_RESET)

#define Touch_IIC_SDA(a)                                                        \
    if (a)                                                                      \
        HAL_GPIO_WritePin(Touch_IIC_SDA_PORT, Touch_IIC_SDA_PIN, GPIO_PIN_SET); \
    else                                                                        \
        HAL_GPIO_WritePin(Touch_IIC_SDA_PORT, Touch_IIC_SDA_PIN, GPIO_PIN_RESET)

/*--------------------------------------------- 函数声明 --------------------------------------------------*/

void Touch_IIC_GPIO_Config(void);               // IIC引脚初始化
void Touch_IIC_Delay(uint32_t a);               // IIC延时函数
void Touch_INT_Out(void);                       // INT数据脚配置为输出模式
void Touch_INT_In(void);                        // INT数据脚配置为输入模式
void Touch_IIC_Start(void);                     // 启动IIC通信
void Touch_IIC_Stop(void);                      // IIC停止信号
void Touch_IIC_ACK(void);                       //	发送响应信号
void Touch_IIC_NoACK(void);                     // 发送非应答信号
uint8_t Touch_IIC_WaitACK(void);                //	等待应答信号
uint8_t Touch_IIC_WriteByte(uint8_t IIC_Data);  // 写字节函数
uint8_t Touch_IIC_ReadByte(uint8_t ACK_Mode);   // 读字节函数

volatile TouchStructure g_TouchInfo;      //	触摸信息结构体，在函数 Touch_Scan() 里被调用，存储触摸数据
volatile static uint8_t Modify_Flag = 0;  // 触摸坐标修改标志位

/**
 * @brief  复位GT911
 * @note   复位GT911，并将芯片的IIC地址配置为0xBA/0xBB
 */
void GT9XX_Reset(void)
{
    Touch_INT_Out();  //	将INT引脚配置为输出

    // 初始化引脚状态
    HAL_GPIO_WritePin(Touch_INT_PORT, Touch_INT_PIN, GPIO_PIN_RESET);  // INT输出低电平
    HAL_GPIO_WritePin(Touch_RST_PORT, Touch_RST_PIN, GPIO_PIN_SET);    // RST输出高	电平
    Touch_IIC_Delay(10000);

    // 开始执行复位
    //	INT引脚保持低电平不变，将器件地址设置为0XBA/0XBB
    HAL_GPIO_WritePin(Touch_RST_PORT, Touch_RST_PIN, GPIO_PIN_RESET);  // 拉低复位引脚，此时芯片执行复位
    Touch_IIC_Delay(150000);                                           // 延时
    HAL_GPIO_WritePin(Touch_RST_PORT, Touch_RST_PIN, GPIO_PIN_SET);    // 拉高复位引脚，复位结束
    Touch_IIC_Delay(350000);                                           // 延时
    Touch_INT_In();                                                    // INT引脚转为浮空输入
    Touch_IIC_Delay(20000);                                            // 延时
}

/**
 * @brief  GT9XX写操作
 * @param  addr 要操作的寄存器地址
 * @retval  SUCCESS-操作成功, ERROR-操作失败
 */
uint8_t GT9XX_WriteHandle(uint16_t addr)
{
    uint8_t status;  // 状态标志位

    Touch_IIC_Start();                                   // 启动IIC通信
    if (Touch_IIC_WriteByte(GT9XX_IIC_WADDR) == ACK_OK)  // 写数据指令
    {
        if (Touch_IIC_WriteByte((uint8_t)(addr >> 8)) == ACK_OK)  // 写入16位地址
        {
            if (Touch_IIC_WriteByte((uint8_t)(addr)) != ACK_OK)
            {
                status = ERROR;  // 操作失败
            }
        }
    }
    status = SUCCESS;  // 操作成功
    return status;
}

/**
 * @brief  GT9XX写一字节数据
 * @param  addr 要写入的寄存器地址
 * @param  value 要写入的数据
 * @retval  SUCCESS-操作成功, ERROR-操作失败
 */
uint8_t GT9XX_WriteData(uint16_t addr, uint8_t value)
{
    uint8_t status;

    Touch_IIC_Start();  // 启动IIC通讯

    if (GT9XX_WriteHandle(addr) == SUCCESS)  // 写入要操作的寄存器
    {
        if (Touch_IIC_WriteByte(value) != ACK_OK)  // 写数据
        {
            status = ERROR;
        }
    }
    Touch_IIC_Stop();  // 停止通讯

    status = SUCCESS;  // 写入成功
    return status;
}

/**
 * @brief  GT9XX写寄存器
 * @param  addr 要写入的寄存器区域首地址
 * @param  cnt 数据长度
 * @param  value 要写入的数据区指针
 * @retval  SUCCESS-操作成功, ERROR-操作失败
 */
uint8_t GT9XX_WriteReg(uint16_t addr, uint8_t cnt, uint8_t *value)
{
    uint8_t status;
    uint8_t i;

    Touch_IIC_Start();

    if (GT9XX_WriteHandle(addr) == SUCCESS)  // 写入要操作的寄存器
    {
        for (i = 0; i < cnt; i++)  // 计数
        {
            Touch_IIC_WriteByte(value[i]);  // 写入数据
        }
        Touch_IIC_Stop();  // 停止IIC通信
        status = SUCCESS;  // 写入成功
    }
    else
    {
        Touch_IIC_Stop();  // 停止IIC通信
        status = ERROR;    // 写入失败
    }
    return status;
}

/**
 * @brief  GT9XX读寄存器
 * @param  addr 要读取的寄存器区域首地址
 * @param  cnt 数据长度
 * @param  value 读取数据的缓冲区指针
 * @retval  SUCCESS-操作成功, ERROR-操作失败
 */
uint8_t GT9XX_ReadReg(uint16_t addr, uint8_t cnt, uint8_t *value)
{
    uint8_t status;
    uint8_t i;

    status = ERROR;
    Touch_IIC_Start();  // 启动IIC通信

    if (GT9XX_WriteHandle(addr) == SUCCESS)  // 写入要操作的寄存器
    {
        Touch_IIC_Start();  // 重新启动IIC通讯

        if (Touch_IIC_WriteByte(GT9XX_IIC_RADDR) == ACK_OK)  // 发送读命令
        {
            for (i = 0; i < cnt; i++)  // 计数
            {
                if (i == (cnt - 1))
                {
                    value[i] = Touch_IIC_ReadByte(0);  // 读到最后一个数据时发送 非应答信号
                }
                else
                {
                    value[i] = Touch_IIC_ReadByte(1);  // 发送应答信号
                }
            }
            Touch_IIC_Stop();  // 停止IIC通信
            status = SUCCESS;
        }
    }
    Touch_IIC_Stop();  // 停止IIC通信
    return (status);
}

/**
 * @brief  识别屏幕的版本
 * @note   在RGB070M1-800*480 V1.1之前的硬件版本，触摸屏的分辨率为1024*600，根据RST和INT引脚的电平状态进行识别
 */
void PanelRecognition(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = { 0 };

    Touch_INT_CLK_ENABLE;  // 初始化IO口时钟
    Touch_RST_CLK_ENABLE;

    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;       //	输入模式
    GPIO_InitStruct.Pull = GPIO_PULLDOWN;         //	下拉输入
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;  // 速度等级
    GPIO_InitStruct.Pin = Touch_INT_PIN;          // 初始化 INT 引脚

    HAL_GPIO_Init(Touch_INT_PORT, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = Touch_RST_PIN;  //	RST
    HAL_GPIO_Init(Touch_RST_PORT, &GPIO_InitStruct);

    Touch_IIC_Delay(4000);  // 延时

    //	旧版本的RST和INT引脚并没有连接到核心板，因此可以根据这两个引脚的电平状态进行识别，
    //	新版本的硬件这两个脚做了上拉处理
    if ((HAL_GPIO_ReadPin(Touch_RST_PORT, Touch_RST_PIN) != 1) && (HAL_GPIO_ReadPin(Touch_INT_PORT, Touch_INT_PIN) != 1))
    {
        // 在 V1.1 之前的硬件版本，触摸屏的分辨率为1024*600，为了程序上的兼容，这里进行判断处理
        //	该变量标志主要用于判断是否需要软件修改采集到的触摸坐标
        // 此段代码只对旧版的7寸屏有用，其它尺寸的屏幕无需理会
        Modify_Flag = 1;
    }
}

/**
 * @brief  触摸IC初始化，并读取相应信息发送到串口
 * @retval  SUCCESS-初始化成功, ERROR-未检测到触摸屏
 */
uint8_t Touch_Init(void)
{
    uint8_t GT9XX_Info[11];  // 触摸屏IC信息
    uint8_t cfgVersion = 0;  // 触摸配置版本

    // 识别屏幕的版本，在 RGB070M1-800*480 V1.1以及之后的硬件版本，无需理会此段代码
    // 此段代码只对旧版的7寸屏有用，其它尺寸的屏幕无需理会
    PanelRecognition();

    Touch_IIC_GPIO_Config();  // 初始化IIC引脚
    GT9XX_Reset();            // 复位IC

    GT9XX_ReadReg(GT9XX_ID_ADDR, 11, GT9XX_Info);   // 读触摸屏IC信息
    GT9XX_ReadReg(GT9XX_CFG_ADDR, 1, &cfgVersion);  // 读触摸配置版本

    if (GT9XX_Info[0] == '9')  // 判断第一个字符是否为 9
    {
        printf("Touch ID: GT%.4s \r\n", GT9XX_Info);                                                                    // 打印触摸芯片的ID
        printf("固件版本： 0x%.4X\r\n", (GT9XX_Info[5] << 8) + GT9XX_Info[4]);                                          // 芯片固件版本
        printf("触摸分辨率：%d * %d\r\n", (GT9XX_Info[7] << 8) + GT9XX_Info[6], (GT9XX_Info[9] << 8) + GT9XX_Info[8]);  // 当前触摸分辨率
        printf("触摸参数配置版本： 0x%.2X \r\n", cfgVersion);                                                           // 触摸配置版本

        /*---------版本识别，RGB070M1-800*480 V1.1以及之后的硬件版本或者其它尺寸的屏幕，无需理会此段代码-----*/

        if (((GT9XX_Info[7] << 8) + GT9XX_Info[6]) == 1024)  // 判断触摸屏的X轴分辨率是否为1024
        {
            // 在 RGB070M1-800*480 V1.1 之前的硬件版本，触摸屏的分辨率为1024*600，为了程序上的兼容，这里进行判断处理
            //	该变量标志主要用于判断是否需要软件修改采集到的触摸坐标
            Modify_Flag = 1;
        }
        else if (((GT9XX_Info[7] << 8) + GT9XX_Info[6]) == 800)  // 触摸屏的X轴分辨率为800
        {
            Modify_Flag = 0;  // 置0标志位，无需做处理
        }

        /*-------------------------------------------------------------------------------------------------*/
        return SUCCESS;
    }
    else
    {
        printf("Touch Error\r\n");  // 错误，未检测到触摸屏
        return ERROR;
    }
}

/**
 * @brief  触摸扫描
 * @note   周期性调用该函数检测触摸操作，触摸信息存储在g_TouchInfo结构体
 */

void Touch_Scan(void)
{
    uint8_t touchData[2 + 8 * TOUCH_MAX];  // 用于存储触摸数据
    uint8_t i = 0;

    GT9XX_ReadReg(GT9XX_READ_ADDR, 2 + 8 * TOUCH_MAX, touchData);  // 读数据
    GT9XX_WriteData(GT9XX_READ_ADDR, 0);                           //	清除触摸芯片的寄存器标志位
    g_TouchInfo.number = touchData[0] & 0x0f;                      // 取当前的触摸点数

    if ((g_TouchInfo.number >= 1) && (g_TouchInfo.number <= 5))  //	当触摸数在 1-5 之间时
    {
        for (i = 0; i < g_TouchInfo.number; i++)  // 取相应的触摸坐标
        {
            g_TouchInfo.y[i] = (touchData[5 + 8 * i] << 8) | touchData[4 + 8 * i];  // 获取Y坐标
            g_TouchInfo.x[i] = (touchData[3 + 8 * i] << 8) | touchData[2 + 8 * i];  //	获取X坐标

            /*---------版本识别，RGB070M1-800*480 V1.1以及之后的硬件版本或者其它尺寸的屏幕，无需理会此段代码-----*/

            // 在 V1.1 之前的硬件版本，触摸屏的分辨率为1024*600，为了程序上的兼容，这里进行判断处理
            //	该变量标志主要用于判断是否需要软件修改采集到的触摸坐标
            if (Modify_Flag == 1)
            {
                g_TouchInfo.y[i] *= 0.8;   // 将1024*600分辨率的触摸坐标换算成800*480的分辨率
                g_TouchInfo.x[i] *= 0.78;  // 将1024*600分辨率的触摸坐标换算成800*480的分辨率
            }
            /*-------------------------------------------------------------------------------------------------*/
        }
        g_TouchInfo.flag = 1;  // 触摸标志位置1，代表有触摸动作发生
    }
    else
    {
        g_TouchInfo.flag = 0;  // 触摸标志位置0，无触摸动作
    }
}
/**
 * @brief  初始化IIC的GPIO口，推挽输出
 * @note   由于IIC通信速度不高，IO口速度配置为2M即可
 */

void Touch_IIC_GPIO_Config(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = { 0 };

    Touch_IIC_SCL_CLK_ENABLE;  // 初始化IO口时钟
    Touch_IIC_SDA_CLK_ENABLE;
    Touch_INT_CLK_ENABLE;
    Touch_RST_CLK_ENABLE;

    GPIO_InitStruct.Pin = Touch_IIC_SCL_PIN;      // SCL引脚
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD;   // 开漏输出
    GPIO_InitStruct.Pull = GPIO_NOPULL;           // 不带上下拉
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;  // 速度等级
    HAL_GPIO_Init(Touch_IIC_SCL_PORT, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = Touch_IIC_SDA_PIN;  // SDA引脚
    HAL_GPIO_Init(Touch_IIC_SDA_PORT, &GPIO_InitStruct);

    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;  // 推挽输出
    GPIO_InitStruct.Pull = GPIO_PULLUP;          // 上拉

    GPIO_InitStruct.Pin = Touch_INT_PIN;  //	INT
    HAL_GPIO_Init(Touch_INT_PORT, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = Touch_RST_PIN;  //	RST
    HAL_GPIO_Init(Touch_RST_PORT, &GPIO_InitStruct);

    HAL_GPIO_WritePin(Touch_IIC_SCL_PORT, Touch_IIC_SCL_PIN, GPIO_PIN_SET);  // SCL输出高电平
    HAL_GPIO_WritePin(Touch_IIC_SDA_PORT, Touch_IIC_SDA_PIN, GPIO_PIN_SET);  // SDA输出高电平
    HAL_GPIO_WritePin(Touch_INT_PORT, Touch_INT_PIN, GPIO_PIN_RESET);        // INT输出低电平
    HAL_GPIO_WritePin(Touch_RST_PORT, Touch_RST_PIN, GPIO_PIN_SET);          // RST输出高电平
}

/**
 * @brief  简单延时函数
 * @param  a 延时时间
 * @note   为了移植简便性且对延时精度要求不高，不使用定时器
 */

void Touch_IIC_Delay(uint32_t a)
{
    volatile uint16_t i;
    while (a--)
    {
        for (i = 0; i < 8; i++);
    }
}

/**
 * @brief  配置INT引脚为输出模式
 */

void Touch_INT_Out(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = { 0 };

    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;   // 输出模式
    GPIO_InitStruct.Pull = GPIO_PULLUP;           // 上拉
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;  // 速度等级
    GPIO_InitStruct.Pin = Touch_INT_PIN;          // 初始化 INT 引脚

    HAL_GPIO_Init(Touch_INT_PORT, &GPIO_InitStruct);
}

/**
 * @brief  配置INT引脚为输入模式
 */

void Touch_INT_In(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = { 0 };

    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;       // 输入模式
    GPIO_InitStruct.Pull = GPIO_NOPULL;           // 浮空
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;  // 速度等级
    GPIO_InitStruct.Pin = Touch_INT_PIN;          // 初始化 INT 引脚

    HAL_GPIO_Init(Touch_INT_PORT, &GPIO_InitStruct);
}

/**
 * @brief  IIC起始信号
 * @note   在SCL处于高电平期间，SDA由高到低跳变为起始信号
 */

void Touch_IIC_Start(void)
{
    Touch_IIC_SDA(1);
    Touch_IIC_SCL(1);
    Touch_IIC_Delay(IIC_DelayVaule);

    Touch_IIC_SDA(0);
    Touch_IIC_Delay(IIC_DelayVaule);
    Touch_IIC_SCL(0);
    Touch_IIC_Delay(IIC_DelayVaule);
}

/**
 * @brief  IIC停止信号
 * @note   在SCL处于高电平期间，SDA由低到高跳变为停止信号
 */

void Touch_IIC_Stop(void)
{
    Touch_IIC_SCL(0);
    Touch_IIC_Delay(IIC_DelayVaule);
    Touch_IIC_SDA(0);
    Touch_IIC_Delay(IIC_DelayVaule);

    Touch_IIC_SCL(1);
    Touch_IIC_Delay(IIC_DelayVaule);
    Touch_IIC_SDA(1);
    Touch_IIC_Delay(IIC_DelayVaule);
}

/**
 * @brief  IIC应答信号
 * @note   在SCL为高电平期间，SDA引脚输出低电平产生应答信号
 */

void Touch_IIC_ACK(void)
{
    Touch_IIC_SCL(0);
    Touch_IIC_Delay(IIC_DelayVaule);
    Touch_IIC_SDA(0);
    Touch_IIC_Delay(IIC_DelayVaule);
    Touch_IIC_SCL(1);
    Touch_IIC_Delay(IIC_DelayVaule);

    Touch_IIC_SCL(0);  // SCL输出低时，SDA应立即拉高，释放总线
    Touch_IIC_SDA(1);

    Touch_IIC_Delay(IIC_DelayVaule);
}

/**
 * @brief  IIC非应答信号
 * @note   在SCL为高电平期间，SDA引脚为高电平产生非应答信号
 */

void Touch_IIC_NoACK(void)
{
    Touch_IIC_SCL(0);
    Touch_IIC_Delay(IIC_DelayVaule);
    Touch_IIC_SDA(1);
    Touch_IIC_Delay(IIC_DelayVaule);
    Touch_IIC_SCL(1);
    Touch_IIC_Delay(IIC_DelayVaule);

    Touch_IIC_SCL(0);
    Touch_IIC_Delay(IIC_DelayVaule);
}

/**
 * @brief  等待接收设备发出应答信号
 * @retval  ACK_OK-应答正常, ACK_ERR-无应答
 * @note   在SCL为高电平期间，若检测到SDA引脚为低电平则接收设备响应正常
 */

uint8_t Touch_IIC_WaitACK(void)
{
    Touch_IIC_SDA(1);
    Touch_IIC_Delay(IIC_DelayVaule);
    Touch_IIC_SCL(1);
    Touch_IIC_Delay(IIC_DelayVaule);

    if (HAL_GPIO_ReadPin(Touch_IIC_SDA_PORT, Touch_IIC_SDA_PIN) != 0)  // 判断设备是否有做出响应
    {
        Touch_IIC_SCL(0);
        Touch_IIC_Delay(IIC_DelayVaule);
        return ACK_ERR;  // 无应答
    }
    else
    {
        Touch_IIC_SCL(0);
        Touch_IIC_Delay(IIC_DelayVaule);
        return ACK_OK;  // 应答正常
    }
}

/**
 * @brief  写一字节数据
 * @param  IIC_Data 要写入的8位数据
 * @retval  ACK_OK-设备响应正常, ACK_ERR-设备响应错误
 * @note   高位在前
 */

uint8_t Touch_IIC_WriteByte(uint8_t IIC_Data)
{
    uint8_t i;

    for (i = 0; i < 8; i++)
    {
        Touch_IIC_SDA(IIC_Data & 0x80);

        Touch_IIC_Delay(IIC_DelayVaule);
        Touch_IIC_SCL(1);
        Touch_IIC_Delay(IIC_DelayVaule);
        Touch_IIC_SCL(0);
        if (i == 7)
        {
            Touch_IIC_SDA(1);
        }
        IIC_Data <<= 1;
    }

    return Touch_IIC_WaitACK();  // 等待设备响应
}

/**
 * @brief  读一字节数据
 * @param  ACK_Mode 响应模式(1-发出应答信号, 0-发出非应答信号)
 * @retval  读取到的8位数据
 * @note   高位在前，应在主机接收最后一字节数据时发送非应答信号
 */

uint8_t Touch_IIC_ReadByte(uint8_t ACK_Mode)
{
    uint8_t IIC_Data = 0;
    uint8_t i = 0;

    for (i = 0; i < 8; i++)
    {
        IIC_Data <<= 1;

        Touch_IIC_SCL(1);
        Touch_IIC_Delay(IIC_DelayVaule);
        IIC_Data |= (HAL_GPIO_ReadPin(Touch_IIC_SDA_PORT, Touch_IIC_SDA_PIN) & 0x01);
        Touch_IIC_SCL(0);
        Touch_IIC_Delay(IIC_DelayVaule);
    }

    if (ACK_Mode == 1)  //	应答信号
        Touch_IIC_ACK();
    else
        Touch_IIC_NoACK();  // 非应答信号

    return IIC_Data;
}

/*************************************************************************************************************************************************************************************************FANKE****/
