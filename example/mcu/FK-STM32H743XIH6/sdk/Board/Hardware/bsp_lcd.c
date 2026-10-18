#include "bsp_lcd.h"
#include "bsp_timer.h"
#include "bsp_memory.h"
#include "bsp_periph.h"

#include "stm32h7xx_hal_dma2d.h"

LCD_T g_LCD;

void (*LCD_SetDirection)(LCD_Direction_T Direction);
void (*LCD_DrawPoint)(uint16_t x, uint16_t y, uint16_t color);
void (*LCD_ShowImage)(uint16_t x, uint16_t y, uint16_t Width, uint16_t Height, const uint8_t *image);
void (*LCD_Fill)(uint16_t StartX, uint16_t StartY, uint16_t Width, uint16_t Height, uint16_t color);
void (*LCD_ChangeMemoryAccess)(void);
void (*LCD_ShowMonoImage)(uint16_t X, uint16_t Y, uint16_t Width, uint16_t Height, const uint8_t *Image, uint16_t FontColor, uint16_t BlackColor);
void (*LCD_ON)(void);
void (*LCD_OFF)(void);

// 1. LTDC时钟在 main.c 文件中 SystemClock_Config() 函数设置，例程默认为33MHz，
//    即刷新率在60帧左右，过高的时钟会加大对SDRAM的占用，容易造成花屏，并且屏
//    幕本身不支持这么高的刷新率，还会导致屏幕闪烁显示异常等，过低的时钟会导
//    致刷新率太低，屏幕会有明显的闪烁
//
// 2. 只开单层显示，16位色、RGB888以及ARGB8888模式下，LTDC时钟可以设置为33M

/*---------------------------------------------------------- 函数声明 -------------------------------------------------------*/
#define HBP                           80  // 根据屏幕的手册进行设置
#define VBP                           40
#define HSW                           1
#define VSW                           1
#define HFP                           200
#define VFP                           22

// FK750M5-XBH6 核心板 使用的是外部SDRAM作为显存，起始地址0xC0000000
// 显存所需空间 = 分辨率 * 每个像素所占字节数，例如 800*480的屏，使用16位色（RGB565或者AEGB1555），需要显存 800*480*2 = 768000 字节
#define LCD_WIDTH                     800              // LCD的像素长度
#define LCD_HEIGHT                    480              // LCD的像素宽度
#define LCD_MEM_ADDR                  SDRAM_BANK_ADDR  // 显存的起始地址

/*-------------------------------------------------------- LCD背光引脚 -------------------------------------------------------*/
#define LCD_BACKLIGHT_PIN             GPIO_PIN_6
#define LCD_BACKLIGHT_PORT            GPIOH
#define GPIO_LDC_Backlight_CLK_ENABLE __HAL_RCC_GPIOH_CLK_ENABLE()

/* 反客科技内部引脚定义 */
#define LCD_SPI                       SPI5
#define LCD_SPI_AF                    GPIO_AF5_SPI5
#define LCD_SPI_REQUEST               DMA_REQUEST_SPI5_TX
#define LCD_RES_PIN                   GPIO_PIN_12
#define LCD_RES_PORT                  GPIOG
#define LCD_DC_PIN                    GPIO_PIN_11
#define LCD_DC_PORT                   GPIOJ
#define LCD_BLK_PIN                   GPIO_PIN_6
#define LCD_BLK_PORT                  GPIOH
#define LCD_CS_PIN                    GPIO_PIN_5
#define LCD_CS_PORT                   GPIOH
#define LCD_CLK_PIN                   GPIO_PIN_10
#define LCD_CLK_PORT                  GPIOJ
#define LCD_MOSI_PIN                  GPIO_PIN_0
#define LCD_MOSI_PORT                 GPIOK

/* TFT屏幕 */
#define LCD_DEVICE_HEIGHT             240
#define LCD_DEVICE_WIDTH              320

LTDC_HandleTypeDef hltdc;  // LTDC句柄

/**
 * @brief  判断指定点是否在指定多边形内部
 * @param  nvert 多边形顶点数
 * @param  vertx 顶点X坐标数组
 * @param  verty 顶点Y坐标数组
 * @param  testx 测试点X坐标
 * @param  testy 测试点Y坐标
 * @retval 1表示在多边形内部，0表示在外部
 */
uint8_t LCD_IsPointInPoly(uint8_t nvert, uint16_t *vertx, uint16_t *verty, uint16_t testx, uint16_t testy)
{
    uint16_t i, j, c = 0;

    /* 此算法由W. Randolph Franklin提出 */
    /* 参考链接：https://wrfranklin.org/Research/Short_Notes/pnpoly.html */
    for (i = 0, j = nvert - 1; i < nvert; j = i++)
    {
        if (((verty[i] > testy) != (verty[j] > testy)) && (testx < (vertx[j] - vertx[i]) * (testy - verty[i]) / (verty[j] - verty[i]) + vertx[i]))
        {
            c = !c;
        }
    }
    return c;
}

/**
 * @brief  获取数组中的最大值
 * @param  Array 待查找的数组
 * @param  n 数组元素个数
 * @retval 数组中的最大值
 */
int16_t LCD_GetMax(uint16_t Array[], int n)
{
    uint16_t Max = Array[0];
    uint8_t i;

    for (i = 1; i < n; i++)
    {
        if (Array[i] > Max)
        {
            Max = Array[i];
        }
    }

    return Max;
}

/**
 * @brief  获取数组中的最小值
 * @param  Array 待查找的数组
 * @param  n 数组元素个数
 * @retval 数组中的最小值
 */
int16_t LCD_GetMin(uint16_t Array[], int n)
{
    uint16_t Min = Array[0];
    uint8_t i;

    for (i = 1; i < n; i++)
    {
        if (Array[i] < Min)
        {
            Min = Array[i];
        }
    }

    return Min;
}

/**
 * @brief  判断指定点是否在指定角度范围内
 * @param  X 测试点X坐标
 * @param  Y 测试点Y坐标
 * @param  StartAngle 起始角度
 * @param  EndAngle 结束角度
 * @retval 1表示在角度范围内，0表示不在
 */
uint8_t LCD_IsInAngle(uint16_t X, uint16_t Y, int16_t StartAngle, int16_t EndAngle)
{
    uint16_t PointAngle;
    PointAngle = atan2(Y, X) / 3.14 * 180;  // 计算指定点的弧度，并转换为角度表示
    if (StartAngle < EndAngle)              // 起始角度小于终止角度的情况
    {
        /* 如果指定角度在起始终止角度之间，则判定指定点在指定角度 */
        if (PointAngle >= StartAngle && PointAngle <= EndAngle)
        {
            return 1;
        }
    }
    else  // 起始角度大于于终止角度的情况
    {
        /* 如果指定角度大于起始角度或者小于终止角度，则判定指定点在指定角度 */
        if (PointAngle >= StartAngle || PointAngle <= EndAngle)
        {
            return 1;
        }
    }
    return 0;  // 不满足以上条件，则判断判定指定点不在指定角度
}

/**
 * @brief  计算绝对值
 * @param  X 输入值
 * @retval 输入值的绝对值
 */
int32_t LCD_ABS(int32_t X)
{
    return X > 0 ? X : -X;  // 不满足以上条件，则判断判定指定点不在指定角度
}

/**
 * @brief  计算X的Y次幂
 * @param  X 底数
 * @param  Y 指数
 * @retval X的Y次幂
 */
uint32_t LCD_Pow(uint32_t X, uint32_t Y)
{
    uint32_t Result = 1;  // 结果默认为1
    while (Y--)           // 累乘Y次
    {
        Result *= X;  // 每次把X累乘到结果上
    }
    return Result;
}

/**
 * @brief  LCD绘制单个点
 * @param  x 点的X坐标
 * @param  y 点的Y坐标
 * @param  color 点的颜色，16位RGB565格式
 */
void LCD_RGB_DrawPoint(uint16_t x, uint16_t y, uint16_t color)
{
    *(__IO uint16_t *)(LCD_MEM_ADDR + 2 * (x + y * LCD_WIDTH)) = color;
}

/**
 * @brief  LTDC_MspInit函数
 * @param  hltdc LTDC句柄
 */
void HAL_LTDC_MspInit(LTDC_HandleTypeDef *hltdc)
{
    uint8_t Modify_Flag = 0;  // 旧面板识别标志位

    GPIO_InitTypeDef GPIO_InitStruct = { 0 };
    if (hltdc->Instance == LTDC)
    {
        __HAL_RCC_LTDC_CLK_ENABLE();  // 使能LTDC时钟

        __HAL_RCC_GPIOK_CLK_ENABLE();  // 使能IO口时钟
        __HAL_RCC_GPIOG_CLK_ENABLE();
        __HAL_RCC_GPIOJ_CLK_ENABLE();
        __HAL_RCC_GPIOH_CLK_ENABLE();
        __HAL_RCC_GPIOI_CLK_ENABLE();

        GPIO_LDC_Backlight_CLK_ENABLE;  // 使能背光引脚IO口时钟

        /*------------------------------LTDC GPIO Configuration--------------------------

            PI15    ------> LTDC_R0		PJ7     ------> LTDC_G0			 PJ12    ------> LTDC_B0
            PJ0     ------> LTDC_R1	   PJ8     ------> LTDC_G1        PJ13    ------> LTDC_B1
            PJ1     ------> LTDC_R2      PJ9     ------> LTDC_G2	       PJ14    ------> LTDC_B2
            PJ2     ------> LTDC_R3      PG10    ------> LTDC_G3	       PJ15    ------> LTDC_B3
            PJ3     ------> LTDC_R4      PH15    ------> LTDC_G4	       PK3     ------> LTDC_B4
            PJ4     ------> LTDC_R5      PH4     ------> LTDC_G5        PK4     ------> LTDC_B5
            PJ5     ------> LTDC_R6	   PK1     ------> LTDC_G6	       PK5     ------> LTDC_B6
            PJ6     ------> LTDC_R7	   PK2     ------> LTDC_G7	       PK6     ------> LTDC_B7

            PI12     ------> LTDC_HSYNC
            PI13     ------> LTDC_VSYNC
            PK7      ------> LTDC_DE
            PI14     ------> LTDC_CLK

        */

        /*-----------------------------------------------------------------------*/

        GPIO_InitStruct.Pin = GPIO_PIN_5 | GPIO_PIN_4 | GPIO_PIN_6 | GPIO_PIN_3
                              | GPIO_PIN_7 | GPIO_PIN_2 | GPIO_PIN_1;
        GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
        GPIO_InitStruct.Alternate = GPIO_AF14_LTDC;
        HAL_GPIO_Init(GPIOK, &GPIO_InitStruct);

        GPIO_InitStruct.Pin = GPIO_PIN_10;
        GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
        GPIO_InitStruct.Alternate = GPIO_AF9_LTDC;
        HAL_GPIO_Init(GPIOG, &GPIO_InitStruct);

        GPIO_InitStruct.Pin = GPIO_PIN_15 | GPIO_PIN_14 | GPIO_PIN_12 | GPIO_PIN_13
                              | GPIO_PIN_9 | GPIO_PIN_0 | GPIO_PIN_8 | GPIO_PIN_7
                              | GPIO_PIN_6 | GPIO_PIN_1 | GPIO_PIN_5 | GPIO_PIN_3 | GPIO_PIN_2
                              | GPIO_PIN_4;
        GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
        GPIO_InitStruct.Alternate = GPIO_AF14_LTDC;
        HAL_GPIO_Init(GPIOJ, &GPIO_InitStruct);

        GPIO_InitStruct.Pin = GPIO_PIN_15;
        GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
        GPIO_InitStruct.Alternate = GPIO_AF14_LTDC;
        HAL_GPIO_Init(GPIOH, &GPIO_InitStruct);

        GPIO_InitStruct.Pin = GPIO_PIN_12 | GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_15;
        GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
        GPIO_InitStruct.Alternate = GPIO_AF14_LTDC;
        HAL_GPIO_Init(GPIOI, &GPIO_InitStruct);

        GPIO_InitStruct.Pin = GPIO_PIN_4;
        GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
        GPIO_InitStruct.Alternate = GPIO_AF9_LTDC;
        HAL_GPIO_Init(GPIOH, &GPIO_InitStruct);

        // 初始化背光引脚
        GPIO_InitStruct.Pin = LCD_BACKLIGHT_PIN;              // 背光引脚
        GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;           // 推挽输出模式
        GPIO_InitStruct.Pull = GPIO_NOPULL;                   // 无上下拉
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;          // 速度等级低
        HAL_GPIO_Init(LCD_BACKLIGHT_PORT, &GPIO_InitStruct);  // 初始化

        LCD_OFF();  // 先关闭背光引脚，初始化LTDC之后再开启
    }
}

/**
 * @brief  初始化LCD（基于LTDC），调用MX_LTDC_Init初始化硬件，并设置默认显示参数
 * @note   此函数替代了原SPI初始化流程
 */
void LCD_RGB_Init(void)
{
    LTDC_LayerCfgTypeDef pLayerCfg = { 0 };  // layer0 相关参数

    hltdc.Instance = LTDC;
    hltdc.Init.HSPolarity = LTDC_HSPOLARITY_AL;   // 低电平有效
    hltdc.Init.VSPolarity = LTDC_VSPOLARITY_AL;   // 低电平有效
    hltdc.Init.DEPolarity = LTDC_DEPOLARITY_AL;   // 低电平有效，要注意的是，很多面板都是高电平有效，但是750需要设置成低电平才能正常显示
    hltdc.Init.PCPolarity = LTDC_PCPOLARITY_IPC;  // 正常时钟信号

    hltdc.Init.HorizontalSync = HSW - 1;  // 根据屏幕设置参数即可
    hltdc.Init.VerticalSync = VSW - 1;
    hltdc.Init.AccumulatedHBP = HBP + HSW - 1;
    hltdc.Init.AccumulatedVBP = VBP + VSW - 1;
    hltdc.Init.AccumulatedActiveW = LCD_WIDTH + HSW + HBP - 1;
    hltdc.Init.AccumulatedActiveH = LCD_HEIGHT + VSW + VBP - 1;
    hltdc.Init.TotalWidth = LCD_WIDTH + HSW + HBP + HFP - 1;
    hltdc.Init.TotalHeigh = LCD_HEIGHT + VSW + VBP + VFP - 1;

    hltdc.Init.Backcolor.Red = 0;    // 初始背景色，R
    hltdc.Init.Backcolor.Green = 0;  // 初始背景色，G
    hltdc.Init.Backcolor.Blue = 0;   // 初始背景色，B

    HAL_LTDC_Init(&hltdc);  // 初始化LTDC参数

    /*---------------------------------- layer0 显示配置 --------------------------------*/
    pLayerCfg.WindowX0 = 0;                            // 水平起点
    pLayerCfg.WindowX1 = LCD_WIDTH;                    // 水平终点
    pLayerCfg.WindowY0 = 0;                            // 垂直起点
    pLayerCfg.WindowY1 = LCD_HEIGHT;                   // 垂直终点
    pLayerCfg.ImageWidth = LCD_WIDTH;                  // 显示区域宽度
    pLayerCfg.ImageHeight = LCD_HEIGHT;                // 显示区域高度
    pLayerCfg.PixelFormat = LTDC_PIXEL_FORMAT_RGB565;  // 颜色格式

    // 配置 layer0 的恒定透明度，最终写入 LTDC_LxCACR 寄存器
    //	需要注意的是，这个参数是直接配置整个 layer0 的透明度，这里设置为255即不透明
    pLayerCfg.Alpha = 255;  // 取值范围0~255，255表示不透明，0表示完全透明

    // 设置 layer0 的层混合系数，最终写入 LTDC_LxBFCR 寄存器
    // 该参数用于设置 layer0 和 底层背景 之间的颜色混合系数，计算公式为 ：
    // 混合后的颜色 =  BF1 * layer0的颜色 + BF2 * 底层背景的颜色
    // 如果 layer0 使用了透明色，则必须配置成 LTDC_BLENDING_FACTOR1_PAxCA 和 LTDC_BLENDING_FACTOR2_PAxCA，否则ARGB中的A通道不起作用
    //	更多的介绍可以查阅 参考手册关于 LTDC_LxBFCR 寄存器的介绍
    pLayerCfg.BlendingFactor1 = LTDC_BLENDING_FACTOR1_CA;  // 混合系数1
    pLayerCfg.BlendingFactor2 = LTDC_BLENDING_FACTOR2_CA;  // 混合系数2

    pLayerCfg.FBStartAdress = LCD_MEM_ADDR;  // 显存地址

    // 配置 layer0 的初始默认颜色，包括A,R,G,B 的值 ，最终写入 LTDC_LxDCCR 寄存器
    pLayerCfg.Alpha0 = 0;           // 初始颜色，A
    pLayerCfg.Backcolor.Blue = 0;   //	初始颜色，R
    pLayerCfg.Backcolor.Green = 0;  //	初始颜色，G
    pLayerCfg.Backcolor.Red = 0;    //	初始颜色，B

    HAL_LTDC_ConfigLayer(&hltdc, &pLayerCfg, 0);  // 配置layer0

    // 因为750每个通道的低位都是采用伪随机抖动输出，如果不开启颜色抖动，则无法正常显示24位或者32位色
    HAL_LTDC_EnableDither(&hltdc);  // 开启颜色抖动

    /*---------------------------------- 初始化一些默认配置 --------------------------------*/
    LCD_SetFontSize(LCD_FontSize16);
    LCD_SetDirection(LCD_HORIZONTAL);
    LCD_Fill(0, 0, LCD_WIDTH, LCD_HEIGHT, LCD_BLACK); /* 清空多余显示 */

    BSP_GPIO_WritePin(LCD_BACKLIGHT_PORT, LCD_BACKLIGHT_PIN, 1);  // 开启背光}

    // 配置DMA2D NVIC，设置中断优先级并使能
    HAL_NVIC_SetPriority(DMA2D_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(DMA2D_IRQn);
}

/**
 * @brief  设置LCD显示方向（废弃接口，保留以维持API兼容）
 * @param  Direction 显示方向（水平/垂直及其反转）
 */
void LCD_RGB_SetDirection(LCD_Direction_T Direction)
{
    g_LCD.Width = LCD_WIDTH;
    g_LCD.Height = LCD_HEIGHT;
}

/**
 * @brief  显示RGB565格式图像
 * @param  x 起始X坐标
 * @param  y 起始Y坐标
 * @param  Width 图像宽度
 * @param  Height 图像高度
 * @param  image 指向RGB565数据的指针（字节序：高位在前）
 * @note   数据无需额外转换，LTDC直接读取RGB565格式
 */
void LCD_RGB_ShowImage(uint16_t x, uint16_t y, uint16_t Width, uint16_t Height, const uint8_t *image)
{
    DMA2D->CR &= ~(DMA2D_CR_START);             //	停止DMA2D
    DMA2D->CR = DMA2D_M2M;                      //	存储器到存储器
    DMA2D->FGPFCCR = LTDC_PIXEL_FORMAT_RGB565;  //	设置颜色格式
    DMA2D->FGOR = 0;
    DMA2D->FGMAR = (uint32_t)image;
    DMA2D->OOR = LCD_WIDTH - Width;                        //	设置行偏移
    DMA2D->OMAR = LCD_MEM_ADDR + 2 * (LCD_WIDTH * y + x);  // 地址;
    DMA2D->NLR = (Width << 16) | (Height);                 //	设定长度和宽度
    DMA2D->CR |= DMA2D_CR_START;                           //	启动DMA2D

    while (DMA2D->CR & DMA2D_CR_START);  // 等待传输完成
}

/**
 * @brief  填充指定矩形区域
 * @param  StartX 起始X坐标
 * @param  StartY 起始Y坐标
 * @param  Width 区域宽度
 * @param  Height 区域高度
 * @param  color RGB565填充颜色
 */
void LCD_RGB_Fill(uint16_t StartX, uint16_t StartY, uint16_t Width, uint16_t Height, uint16_t color)
{
    DMA2D->CR &= ~(DMA2D_CR_START);                                  // 停止DMA2D
    DMA2D->CR = DMA2D_R2M;                                           // 寄存器到SDRAM
    DMA2D->OPFCCR = LTDC_PIXEL_FORMAT_RGB565;                        // 设置颜色格式
    DMA2D->OCOLR = color;                                            // 颜色
    DMA2D->OOR = LCD_WIDTH - Width;                                  // 设置行偏移
    DMA2D->OMAR = LCD_MEM_ADDR + 2 * (LCD_WIDTH * StartY + StartX);  // 地址;
    DMA2D->NLR = (Width << 16) | (Height);                           // 设定长度和宽度
    DMA2D->CR |= DMA2D_CR_START;                                     //	启动DMA2D

    while (DMA2D->CR & DMA2D_CR_START);  // 等待传输完成
}

/**
 * @brief  更改内存访问模式（废弃接口，保留以维持API兼容）
 * @note   LTDC驱动中无对应功能，此函数为空操作。
 */
void LCD_RGB_ChangeMemoryAccess(void)
{
    // 原始bsp_lcd中该函数未实现，推测为遗留接口
    // 当前驱动基于显存，无需主动控制总线访问
}

/**
 * @brief  显示单色图像（如图标）
 * @param  X 起始X坐标
 * @param  Y 起始Y坐标
 * @param  Width 图像宽度
 * @param  Height 图像高度
 * @param  Image 位图数据指针（1位每像素）
 * @param  FontColor 前景色(RGB565)
 * @param  BlackColor 背景色(RGB565)
 * @note   通过逐位判断并调用DrawPoint实现，适用于小图标
 */
void LCD_RGB_ShowMonoImage(uint16_t X, uint16_t Y, uint16_t Width, uint16_t Height, const uint8_t *Image, uint16_t FontColor, uint16_t BlackColor)
{
    uint8_t disChar;        // 字模的值
    uint16_t Xaddress = X;  // 水平坐标
    uint16_t i = 0, j = 0, m = 0;

    for (i = 0; i < Height; i++)
    {
        for (j = 0; j < (float)Width / 8; j++)
        {
            disChar = *Image;

            for (m = 0; m < 8; m++)
            {
                if (disChar & 0x01)
                {
                    LCD_DrawPoint(Xaddress, Y, FontColor);  // 当前模值不为0时，使用画笔色绘点
                }
                else
                {
                    LCD_DrawPoint(Xaddress, Y, BlackColor);  // 否则使用背景色绘制点
                }
                disChar >>= 1;
                Xaddress++;  // 水平坐标自加

                if ((Xaddress - X) == Width)  // 如果水平坐标达到了字符宽度，则退出当前循环
                {                             // 进入下一行的绘制
                    Xaddress = X;
                    Y++;
                    break;
                }
            }
            Image++;
        }
    }
}

/**
 * @brief  开启LCD背光
 */
void LCD_RGB_ON(void)
{
    BSP_GPIO_WritePin(LCD_BACKLIGHT_PORT, LCD_BACKLIGHT_PIN, 1);
}

/**
 * @brief  关闭LCD背光
 */
void LCD_RGB_OFF(void)
{
    BSP_GPIO_WritePin(LCD_BACKLIGHT_PORT, LCD_BACKLIGHT_PIN, 0);
}

void LCD_SPI_TransmitData16(const uint16_t *pData, int32_t size, int8_t mode);
void LCD_SPI_TransmitData8(const uint8_t *pData, int32_t size);

/**
 * @brief  写入复位引脚状态
 * @param  data 引脚状态
 */
__STATIC_INLINE void LCD_TFT_WR_RES(uint8_t data)
{
    HAL_GPIO_WritePin(LCD_RES_PORT, LCD_RES_PIN, data);
}
/**
 * @brief  写入数据/命令选择引脚状态
 * @param  data 引脚状态
 */
__STATIC_INLINE void LCD_TFT_WR_DC(uint8_t data)
{
    HAL_GPIO_WritePin(LCD_DC_PORT, LCD_DC_PIN, data);
}
/**
 * @brief  写入片选引脚状态
 * @param  data 引脚状态
 */
__STATIC_INLINE void LCD_TFT_WR_CS(uint8_t data)
{
    HAL_GPIO_WritePin(LCD_CS_PORT, LCD_CS_PIN, data);
}
/**
 * @brief  写入背光引脚状态
 * @param  data 引脚状态
 */
__STATIC_INLINE void LCD_TFT_WR_BLK(uint8_t data)
{
    HAL_GPIO_WritePin(LCD_BLK_PORT, LCD_BLK_PIN, data);
}

/**
 * @brief  写入8位数据到SPI总线
 * @param  data 待写入的数据
 */
__STATIC_INLINE void LCD_TFT_WR_BUS(uint8_t data)
{
    LCD_SPI_TransmitData8(&data, 1);
}

/**
 * @brief  写入16位数据到SPI总线
 * @param  data 待写入的数据
 */
__STATIC_INLINE void LCD_TFT_WR_BUS16(uint16_t data)
{
    uint16_t swap = (data >> 8) | (data << 8);
    LCD_SPI_TransmitData8((uint8_t *)&swap, 2);
}

/**
 * @brief  写8位数据
 * @param  data 待写入的数据
 */
__STATIC_INLINE void LCD_TFT_WR_DATA8(uint8_t data)
{
    LCD_TFT_WR_DC(1);     /* 设置为数据模式 */
    LCD_TFT_WR_CS(0);     /* 拉低CS引脚 */
    LCD_TFT_WR_BUS(data); /* 写入数据 */
    LCD_TFT_WR_CS(1);     /* 拉高CS引脚 */
}

/**
 * @brief  写16位数据
 * @param  data 待写入的数据
 */
__STATIC_INLINE void LCD_TFT_WR_DATA16(uint16_t data)
{
    LCD_TFT_WR_DC(1);       /* 设置为数据模式 */
    LCD_TFT_WR_CS(0);       /* 拉低CS引脚 */
    LCD_TFT_WR_BUS16(data); /* 写入数据 */
    LCD_TFT_WR_CS(1);       /* 拉高CS引脚 */
}

/**
 * @brief  写命令
 * @param  data 命令值
 */
__STATIC_INLINE void LCD_TFT_WR_REG(uint8_t data)
{
    LCD_TFT_WR_DC(0);     /* 设置为命令模式 */
    LCD_TFT_WR_CS(0);     /* 拉低CS引脚 */
    LCD_TFT_WR_BUS(data); /* 写入命令 */
    LCD_TFT_WR_CS(1);     /* 拉高CS引脚 */
}

/**
 * @brief  初始化LCD TFT的GPIO引脚
 */
void LCD_TFT_GPIO_Init(void)
{
    /* 开启时钟 */
    BSP_GPIO_ClkEnable(LCD_CLK_PORT);
    BSP_GPIO_ClkEnable(LCD_MOSI_PORT);
    BSP_GPIO_ClkEnable(LCD_CS_PORT);
    BSP_GPIO_ClkEnable(LCD_DC_PORT);
    BSP_GPIO_ClkEnable(LCD_BLK_PORT);
    BSP_GPIO_ClkEnable(LCD_RES_PORT);

    /* 初始化GPIO */
    GPIO_InitTypeDef gpioInitStruct = { 0 };
    gpioInitStruct.Mode = GPIO_MODE_AF_PP;       /* 复用推挽输出 */
    gpioInitStruct.Pull = GPIO_NOPULL;           /* 无上下拉 */
    gpioInitStruct.Speed = GPIO_SPEED_FREQ_HIGH; /* 最高速度等级 */
    gpioInitStruct.Alternate = LCD_SPI_AF;
    gpioInitStruct.Pin = LCD_CLK_PIN;
    HAL_GPIO_Init(LCD_CLK_PORT, &gpioInitStruct); /* 初始化GPIO */
    gpioInitStruct.Pin = LCD_MOSI_PIN;
    HAL_GPIO_Init(LCD_MOSI_PORT, &gpioInitStruct); /* 初始化GPIO */

    gpioInitStruct.Mode = GPIO_MODE_OUTPUT_PP;  /* 复用推挽输出 */
    gpioInitStruct.Speed = GPIO_SPEED_FREQ_LOW; /* 最低速度等级 */
    gpioInitStruct.Pin = LCD_CS_PIN;
    HAL_GPIO_Init(LCD_CS_PORT, &gpioInitStruct); /* 初始化GPIO */
    gpioInitStruct.Pin = LCD_DC_PIN;
    HAL_GPIO_Init(LCD_DC_PORT, &gpioInitStruct); /* 初始化GPIO */
    gpioInitStruct.Pin = LCD_BLK_PIN;
    HAL_GPIO_Init(LCD_BLK_PORT, &gpioInitStruct); /* 初始化GPIO */
    gpioInitStruct.Pin = LCD_RES_PIN;
    HAL_GPIO_Init(LCD_RES_PORT, &gpioInitStruct); /* 初始化GPIO */
}

/**
 * @brief  初始化SPI接口
 */
void LCD_TFT_SPI_Init(void)
{
    static SPI_HandleTypeDef spi = { 0 };

    /* 使能时钟 */
    BSP_SPI_ClkEnable(LCD_SPI);

    /* 配置SPI */
    spi.Instance = LCD_SPI;
    spi.Init.Mode = SPI_MODE_MASTER;                                               /* 主机模式 */
    spi.Init.Direction = SPI_DIRECTION_1LINE;                                      /* 单线 */
    spi.Init.DataSize = SPI_DATASIZE_8BIT;                                         /* 8位数据宽度 */
    spi.Init.CLKPolarity = SPI_POLARITY_LOW;                                       /* CLK空闲时保持低电平 */
    spi.Init.CLKPhase = SPI_PHASE_1EDGE;                                           /* 数据在CLK第一个边沿有效 */
    spi.Init.NSS = SPI_NSS_HARD_OUTPUT;                                            /* 使用硬件片选 */
    spi.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_2;                          /* SPI的内核时钟设置为120M，再经过2分频得到60M的SCK驱动时钟 */
    spi.Init.FirstBit = SPI_FIRSTBIT_MSB;                                          /* 高位在先 */
    spi.Init.TIMode = SPI_TIMODE_DISABLE;                                          /* 禁止TI模式 */
    spi.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;                          /* 禁止CRC */
    spi.Init.CRCPolynomial = 0x0;                                                  /* CRC校验项，这里用不到 */
    spi.Init.NSSPMode = SPI_NSS_PULSE_ENABLE;                                      /* 使用片选脉冲模式 */
    spi.Init.NSSPolarity = SPI_NSS_POLARITY_LOW;                                   /* 片选低电平有效 */
    spi.Init.FifoThreshold = SPI_FIFO_THRESHOLD_01DATA;                            /* FIFO阈值 */
    spi.Init.TxCRCInitializationPattern = SPI_CRC_INITIALIZATION_ALL_ZERO_PATTERN; /* 发送端CRC初始化模式，这里用不到 */
    spi.Init.RxCRCInitializationPattern = SPI_CRC_INITIALIZATION_ALL_ZERO_PATTERN; /* 接收端CRC初始化模式，这里用不到 */
    spi.Init.MasterSSIdleness = SPI_MASTER_SS_IDLENESS_00CYCLE;                    /* 额外延迟周期为0 */
    spi.Init.MasterInterDataIdleness = SPI_MASTER_INTERDATA_IDLENESS_00CYCLE;      /* 主机模式下，两个数据帧之间的延迟周期 */
    spi.Init.MasterReceiverAutoSusp = SPI_MASTER_RX_AUTOSUSP_DISABLE;              /* 禁止自动接收管理 */
    spi.Init.MasterKeepIOState = SPI_MASTER_KEEP_IO_STATE_DISABLE;                 /* 主机模式下，禁止SPI保持当前引脚状态 */
    spi.Init.IOSwap = SPI_IO_SWAP_DISABLE;                                         /* 不交换MOSI和MISO */
    if (HAL_SPI_Init(&spi) != HAL_OK)                                              /* 初始化SPI */
    {
        Assert_Failed(__FILE__, __LINE__);
    }

    g_LCD.SPI = spi;

    RCC_PeriphCLKInitTypeDef periphClkInitStruct = { 0 };
    periphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_SPI1;
    periphClkInitStruct.Spi123ClockSelection = RCC_SPI123CLKSOURCE_PLL; /* SPI5 内核时钟120M */
    if (HAL_RCCEx_PeriphCLKConfig(&periphClkInitStruct) != HAL_OK)
    {
        Assert_Failed(__FILE__, __LINE__);
    }

    HAL_NVIC_SetPriority(DMA2_Stream0_IRQn, 0, 1);
    HAL_NVIC_EnableIRQ(DMA2_Stream0_IRQn);
#if LCD_PIN_SELECT == 0
    HAL_NVIC_SetPriority(SPI5_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(SPI5_IRQn);
#else
    HAL_NVIC_SetPriority(SPI1_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(SPI1_IRQn);
#endif
}

/**
 * @brief  初始化SPI接口DMA(普通模式)
 */
void LCD_TFT_DMA_NormalInit(void)
{
    DMA_HandleTypeDef *DMA = &g_LCD.DMA;

    /* 使能时钟 */
    BSP_DMA_ClkEnable(DMA2);

    /* 配置DMA */
    HAL_DMA_DeInit(DMA);
    DMA->Instance = DMA2_Stream0;
    DMA->Init.Request = LCD_SPI_REQUEST;
    DMA->Init.Direction = DMA_MEMORY_TO_PERIPH;
    DMA->Init.PeriphInc = DMA_PINC_DISABLE;
    DMA->Init.MemInc = DMA_MINC_ENABLE;
    DMA->Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
    DMA->Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
    DMA->Init.Mode = DMA_NORMAL;
    DMA->Init.Priority = DMA_PRIORITY_HIGH;
    DMA->Init.FIFOMode = DMA_FIFOMODE_DISABLE;
    if (HAL_DMA_Init(&g_LCD.DMA) != HAL_OK)
    {
        Assert_Failed(__FILE__, __LINE__);
    }

    __HAL_LINKDMA(&g_LCD.SPI, hdmatx, g_LCD.DMA);
}

/**
 * @brief  设置显示区域
 * @param  StartX 起始X坐标
 * @param  StartY 起始Y坐标
 * @param  endX 结束X坐标
 * @param  endY 结束Y坐标
 */
void LCD_TFT_AddressSet(uint16_t StartX, uint16_t StartY, uint16_t endX, uint16_t endY)
{
    LCD_TFT_WR_REG(0x2a); /* 列地址设置 */
    LCD_TFT_WR_DATA16(StartX);
    LCD_TFT_WR_DATA16(endX);
    LCD_TFT_WR_REG(0x2b); /* 行地址设置 */
    LCD_TFT_WR_DATA16(StartY);
    LCD_TFT_WR_DATA16(endY);
    LCD_TFT_WR_REG(0x2c); /* 储存器写 */
}

/**
 * @brief  LCD绘制单个点
 * @param  x 点的X坐标
 * @param  y 点的Y坐标
 * @param  color 点的颜色，16位RGB565格式
 */
void LCD_TFT_DrawPoint(uint16_t x, uint16_t y, uint16_t color)
{
    if (x >= g_LCD.Width || y >= g_LCD.Height)
        return;
    LCD_TFT_AddressSet(x, y, x, y); /* 设置光标位置 */
    LCD_TFT_WR_DATA16(color);       /* 写入颜色数据 */
}

/**
 * @brief  初始化LCD（基于TFT），初始化GPIO和SPI，并设置默认显示参数
 * @note   此函数替代了原SPI初始化流程
 */
void LCD_TFT_Init(void)
{
    LCD_TFT_GPIO_Init(); /* 初始化GPIO */
    LCD_TFT_SPI_Init();  /* 初始化SPI1 */

    LCD_TFT_WR_BLK(1); /* 打开背光 */
    LCD_TFT_WR_RES(0); /* 复位 */
    HAL_Delay(100);    /* 延时 */
    LCD_TFT_WR_RES(1); /* 释放复位 */
    HAL_Delay(10);
    LCD_TFT_WR_REG(0x01); /* 软件复位 */
    HAL_Delay(150);
    LCD_TFT_WR_REG(0x11); /* 睡眠模式关闭 */
    HAL_Delay(1200);      /* 延时 */

    /*------------------------------------ST7735S Frame Rate-----------------------------------------*/
    LCD_TFT_WR_REG(0xB1);
    LCD_TFT_WR_DATA8(0x05);
    LCD_TFT_WR_DATA8(0x3C);
    LCD_TFT_WR_DATA8(0x3C);
    LCD_TFT_WR_REG(0xB2);
    LCD_TFT_WR_DATA8(0x05);
    LCD_TFT_WR_DATA8(0x3C);
    LCD_TFT_WR_DATA8(0x3C);
    LCD_TFT_WR_REG(0xB3);
    LCD_TFT_WR_DATA8(0x05);
    LCD_TFT_WR_DATA8(0x3C);
    LCD_TFT_WR_DATA8(0x3C);
    LCD_TFT_WR_DATA8(0x05);
    LCD_TFT_WR_DATA8(0x3C);
    LCD_TFT_WR_DATA8(0x3C);
    /*------------------------------------End ST7735S Frame Rate---------------------------------*/
    LCD_TFT_WR_REG(0xB4); /* Dot inversion */
    LCD_TFT_WR_DATA8(0x03);
    /*------------------------------------ST7735S Power Sequence---------------------------------*/
    LCD_TFT_WR_REG(0xC0);
    LCD_TFT_WR_DATA8(0x28);
    LCD_TFT_WR_DATA8(0x08);
    LCD_TFT_WR_DATA8(0x04);
    LCD_TFT_WR_REG(0xC1);
    LCD_TFT_WR_DATA8(0XC0);
    LCD_TFT_WR_REG(0xC2);
    LCD_TFT_WR_DATA8(0x0D);
    LCD_TFT_WR_DATA8(0x00);
    LCD_TFT_WR_REG(0xC3);
    LCD_TFT_WR_DATA8(0x8D);
    LCD_TFT_WR_DATA8(0x2A);
    LCD_TFT_WR_REG(0xC4);
    LCD_TFT_WR_DATA8(0x8D);
    LCD_TFT_WR_DATA8(0xEE);

    /*---------------------------------End ST7735S Power Sequence-------------------------------------*/
    LCD_TFT_WR_REG(0xC5); /* VCOM */
    LCD_TFT_WR_DATA8(0x1A);

    /*------------------------------------ST7735S Gamma Sequence---------------------------------*/
    LCD_TFT_WR_REG(0xE0);   /* 设置正伽马控制 */
    LCD_TFT_WR_DATA8(0xD0); /* 设置正伽马控制参数 */
    LCD_TFT_WR_DATA8(0x04);
    LCD_TFT_WR_DATA8(0x0D);
    LCD_TFT_WR_DATA8(0x11);
    LCD_TFT_WR_DATA8(0x13);
    LCD_TFT_WR_DATA8(0x2B);
    LCD_TFT_WR_DATA8(0x3F);
    LCD_TFT_WR_DATA8(0x54);
    LCD_TFT_WR_DATA8(0x4C);
    LCD_TFT_WR_DATA8(0x18);
    LCD_TFT_WR_DATA8(0x0D);
    LCD_TFT_WR_DATA8(0x0B);
    LCD_TFT_WR_DATA8(0x1F);
    LCD_TFT_WR_DATA8(0x23);

    LCD_TFT_WR_REG(0xE1);   /* 设置负伽马控制 */
    LCD_TFT_WR_DATA8(0xD0); /* 设置负伽马控制参数 */
    LCD_TFT_WR_DATA8(0x04);
    LCD_TFT_WR_DATA8(0x0C);
    LCD_TFT_WR_DATA8(0x11);
    LCD_TFT_WR_DATA8(0x13);
    LCD_TFT_WR_DATA8(0x2C);
    LCD_TFT_WR_DATA8(0x3F);
    LCD_TFT_WR_DATA8(0x44);
    LCD_TFT_WR_DATA8(0x51);
    LCD_TFT_WR_DATA8(0x2F);
    LCD_TFT_WR_DATA8(0x1F);
    LCD_TFT_WR_DATA8(0x1F);
    LCD_TFT_WR_DATA8(0x20);
    LCD_TFT_WR_DATA8(0x23);
    /*------------------------------------End ST7735S Gamma Sequence-----------------------------*/

    LCD_TFT_WR_REG(0x3A); /* 65k mode */
    LCD_TFT_WR_DATA8(0x05);
    LCD_TFT_WR_REG(0x29); /* Display on */
    LCD_TFT_WR_REG(0x21); /* 设置内存访问方向 */

    LCD_SetDirection(LCD_REV_HORIZONTAL); /* 设置显示方向为垂直 */

    LCD_Fill(0, 0, g_LCD.Width, g_LCD.Height, 0x0000); /* 清空多余显示 */

    /* 简单的状态初始化 */
    LCD_SetFontSize(LCD_FontSize16);
}

/**
 * @brief  设置LCD显示方向
 * @param  Direction 显示方向（水平/垂直及其反转）
 */
void LCD_TFT_SetDirection(LCD_Direction_T Direction)
{
    g_LCD.Direction = Direction; /* 更新方向参数 */

    LCD_TFT_WR_REG(0x36); /* 写入方向控制寄存器 */

    if (g_LCD.Direction == LCD_HORIZONTAL)
    {
        LCD_TFT_WR_DATA8(0x00); /* 水平方向，正常显示 */
        g_LCD.Height = LCD_DEVICE_WIDTH;
        g_LCD.Width = LCD_DEVICE_HEIGHT;
    }
    else if (g_LCD.Direction == LCD_REV_HORIZONTAL)
    {
        LCD_TFT_WR_DATA8(0xC0); /* 水平方向，反转显示 */
        g_LCD.Height = LCD_DEVICE_WIDTH;
        g_LCD.Width = LCD_DEVICE_HEIGHT;
    }
    else if (g_LCD.Direction == LCD_VERTICAL)
    {
        LCD_TFT_WR_DATA8(0x70); /* 垂直方向，正常显示 */
        g_LCD.Height = LCD_DEVICE_HEIGHT;
        g_LCD.Width = LCD_DEVICE_WIDTH;
    }
    else
    {
        LCD_TFT_WR_DATA8(0xA0); /* 垂直方向，反转显示 */
        g_LCD.Height = LCD_DEVICE_HEIGHT;
        g_LCD.Width = LCD_DEVICE_WIDTH;
    }
}

/**
 * @brief  显示RGB565格式图像
 * @param  x 起始X坐标
 * @param  y 起始Y坐标
 * @param  Width 图像宽度
 * @param  Height 图像高度
 * @param  image 指向RGB565数据的指针（字节序：高位在前）
 * @note   数据无需额外转换，LTDC直接读取RGB565格式
 */
void LCD_TFT_ShowImage(uint16_t x, uint16_t y, uint16_t Width, uint16_t Height, const uint8_t *image)
{
    if (Width == 0 || Height == 0 || x + Width > g_LCD.Width || y + Height > g_LCD.Height) { return; }

    if (Width == 0 || Height == 0 || x + Width > g_LCD.Width || y + Height > g_LCD.Height) { return; }

    uint32_t temp = 120 * Width, currentHeight = 0;
    uint32_t size = (uint32_t)Width * Height * 2; /* 计算填充区域的像素数 */
    uint32_t Timer = HAL_GetTick();
    LCD_TFT_AddressSet(x, y, x + Width - 1, y + Height - 1); /* 设置显示区域 */

    LCD_TFT_WR_CS(0); /* 拉低CS引脚 */
    LCD_TFT_WR_DC(1); /* 设置为数据模式 */
    LCD_TFT_DMA_NormalInit();

    while (currentHeight < Height)
    {
        uint16_t tmpSize = size > temp ? temp : size;
        g_LCD.FinishFlag = 0;
        Timer = HAL_GetTick();
        if (HAL_SPI_Transmit_DMA(&g_LCD.SPI, image, tmpSize) != HAL_OK)
        {
            HAL_SPI_DMAStop(&g_LCD.SPI);
            LCD_TFT_WR_CS(1);
            return;
        }
        while (g_LCD.FinishFlag == 0 && Timer_GetDiffMs(Timer) <= 20);
        if (!g_LCD.FinishFlag)
        {
            HAL_SPI_DMAStop(&g_LCD.SPI);
            LCD_TFT_WR_CS(1);
            return;
        }
        HAL_SPI_DMAStop(&g_LCD.SPI);
        size -= tmpSize;
        image += tmpSize;
        currentHeight += (tmpSize + 2 * Width - 1) / Width / 2;
    }

    LCD_TFT_WR_CS(1); /* 拉高CS引脚 */
}

/**
 * @brief  填充指定矩形区域
 * @param  x 起始X坐标
 * @param  y 起始Y坐标
 * @param  Width 区域宽度
 * @param  Height 区域高度
 * @param  color RGB565填充颜色
 */
void LCD_TFT_Fill(uint16_t x, uint16_t y, uint16_t Width, uint16_t Height, uint16_t color)
{
    if (x + Width > g_LCD.Width) { Width = g_LCD.Width - x; }
    if (y + Height > g_LCD.Height) { Height = g_LCD.Height - y; }
    if (Width == 0 || Height == 0) { return; }

    uint32_t size = Width * Height + 3;                      /* 计算填充区域的像素数 */
    LCD_TFT_AddressSet(x, y, x + Width - 1, y + Height - 1); /* 设置显示区域 */
    LCD_TFT_WR_CS(0);                                        /* 拉低CS引脚 */
    LCD_TFT_WR_DC(1);                                        /* 设置为数据模式 */
    LCD_SPI_TransmitData16(&color, size, 0);
    LCD_TFT_WR_CS(1); /* 拉高CS引脚 */
}

/**
 * @brief  更改内存访问模式
 */
void LCD_TFT_ChangeMemoryAccess(void)
{
    static uint8_t memoryAccess = 0;

    if (memoryAccess == 0)
    {
        memoryAccess = 1;
        LCD_TFT_WR_REG(0x20); /* 设置内存访问方向 */
    }
    else
    {
        memoryAccess = 0;
        LCD_TFT_WR_REG(0x21); /* 设置内存访问方向 */
    }
}

/**
 * @brief  显示单色图像（如图标）
 * @param  X 起始X坐标
 * @param  Y 起始Y坐标
 * @param  Width 图像宽度
 * @param  Height 图像高度
 * @param  Image 位图数据指针（1位每像素）
 * @param  FontColor 前景色(RGB565)
 * @param  BlackColor 背景色(RGB565)
 */
void LCD_TFT_ShowMonoImage(uint16_t X, uint16_t Y, uint16_t Width, uint16_t Height, const uint8_t *Image, uint16_t FontColor, uint16_t BlackColor)
{
    uint8_t mask = 0x01, i, j = 0, OffsetX = 0, OffsetY = 0;
    static uint16_t buffer[256];  // 创建最大32767像素的缓冲区 (65534字节)

    if (X + Width > g_LCD.Width || Y + Height > g_LCD.Height)
        return;

    uint32_t tmpWidth = Width % 8 == 0 ? Width : (Width - Width % 8 + 8);
    uint32_t Size = (uint32_t)tmpWidth * Height / 8;  // 计算填充区域的像素数

    /* 覆盖模式优化处理 */
    if (g_LCD.MonoFill == LCD_Cover)
    {
        LCD_TFT_AddressSet(X, Y, X + Width - 1, Y + Height - 1);
        LCD_TFT_WR_CS(0);
        LCD_TFT_WR_DC(1);

        // 使用16位数据传输
        g_LCD.SPI.Init.DataSize = SPI_DATASIZE_16BIT;  // 16位数据宽度
        HAL_SPI_Init(&g_LCD.SPI);

        uint32_t bufferIndex = 0;
        uint32_t bytesProcessed = 0;

        while (bytesProcessed < Size)
        {
            mask = 0x01;  // 重置掩码
            uint8_t currentByte = *Image;

            // 处理当前字节的8个位
            for (i = 0; i < 8 && j < Width; i++)
            {
                // 直接存储16位颜色值
                buffer[bufferIndex++] = (currentByte & mask) ? FontColor : BlackColor;

                mask <<= 1;
                j++;

                // 缓冲区满时立即发送
                if (bufferIndex >= 512)
                {
                    HAL_SPI_Transmit(&g_LCD.SPI, (uint8_t *)buffer, bufferIndex, HAL_MAX_DELAY);
                    bufferIndex = 0;
                }
            }

            // 移动到下一个数据字节
            Image++;
            bytesProcessed++;

            // 行结束处理
            if (j >= Width)
            {
                j = 0;
                OffsetY++;

                // 跳过当前行剩余的数据
                uint32_t remainingBytes = (tmpWidth - Width) / 8;
                Image += remainingBytes;
                bytesProcessed += remainingBytes;
            }
        }

        // 发送缓冲区剩余数据
        if (bufferIndex > 0)
        {
            HAL_SPI_Transmit(&g_LCD.SPI, (uint8_t *)buffer, bufferIndex, HAL_MAX_DELAY);
        }

        LCD_TFT_WR_CS(1);

        // 切换为8位数据显示
        g_LCD.SPI.Init.DataSize = SPI_DATASIZE_8BIT;  // 8位数据宽度
        HAL_SPI_Init(&g_LCD.SPI);
    }
    /* 非覆盖模式（透明模式）*/
    else
    {
        while (Size--)
        {
            mask = 0x01;

            // 处理当前字节的8个位
            for (i = 0; i < 8 && j < Width; i++)
            {
                if (*Image & mask)
                {
                    OffsetX = j;
                    LCD_DrawPoint(X + OffsetX, Y + OffsetY, FontColor);
                }

                mask <<= 1;
                j++;
            }

            // 行结束处理
            if (j >= Width)
            {
                j = 0;
                OffsetY++;
            }

            Image++;
        }
    }
}

/**
 * @brief  开启LCD背光
 */
void LCD_TFT_ON(void)
{
    LCD_TFT_WR_DATA8(0x29);
    LCD_TFT_WR_BLK(1); /* 打开背光 */
}

/**
 * @brief  关闭LCD背光
 */
void LCD_TFT_OFF(void)
{
    LCD_TFT_WR_DATA8(0x28);
    LCD_TFT_WR_BLK(0); /* 关闭背光 */
}

/**
 * @brief  初始化LCD，自动识别屏幕类型并选择对应驱动
 */
void LCD_Init(void)
{
    __HAL_RCC_DMA2D_CLK_ENABLE();  // 使能DMA2D时钟

    if (Touch_Init() == SUCCESS)
    {
        g_LCD.TftFlag = 0;
        LCD_SetDirection = LCD_RGB_SetDirection;
        LCD_DrawPoint = LCD_RGB_DrawPoint;
        LCD_ShowImage = LCD_RGB_ShowImage;
        LCD_Fill = LCD_RGB_Fill;
        LCD_ChangeMemoryAccess = LCD_RGB_ChangeMemoryAccess;
        LCD_ShowMonoImage = LCD_RGB_ShowMonoImage;
        LCD_ON = LCD_RGB_ON;
        LCD_OFF = LCD_RGB_OFF;
        LCD_RGB_Init();
    }
    else
    {
        g_LCD.TftFlag = 1;
        LCD_SetDirection = LCD_TFT_SetDirection;
        LCD_DrawPoint = LCD_TFT_DrawPoint;
        LCD_ShowImage = LCD_TFT_ShowImage;
        LCD_Fill = LCD_TFT_Fill;
        LCD_ChangeMemoryAccess = LCD_TFT_ChangeMemoryAccess;
        LCD_ShowMonoImage = LCD_TFT_ShowMonoImage;
        LCD_ON = LCD_TFT_ON;
        LCD_OFF = LCD_TFT_OFF;
        LCD_TFT_Init();
    }
}

/**
 * @brief  设置LCD字体大小
 * @param  Size 字体大小
 */
void LCD_SetFontSize(LCD_FontSize_T Size)
{
    g_LCD.FontSize = Size;  // 更新字体大小
}

/**
 * @brief  设置LCD单色图像填充模式
 * @param  Fill 填充模式
 */
void LCD_SetMonoFill(LCD_MonoFill_T Fill)
{
    g_LCD.MonoFill = Fill;  // 更新绘图颜色
}

/**
 * @brief  清空屏幕为指定颜色
 * @param  Color 清屏颜色
 */
void LCD_Clear(uint16_t Color)
{
    LCD_Fill(0, 0, g_LCD.Width, g_LCD.Height, Color);  // 使能SPI
}

/**
 * @brief  显示ASCII字符
 * @param  X 起始X坐标
 * @param  Y 起始Y坐标
 * @param  ShowChar 待显示的字符
 * @param  FontColor 前景色
 * @param  BackColor 背景色
 */
void LCD_ShowChar(uint16_t X, uint16_t Y, uint8_t ShowChar, uint16_t FontColor, uint16_t BackColor)
{
    if (g_LCD.FontSize == LCD_FontSize12)
        LCD_ShowMonoImage(X, Y, 6, 12, LCD_F12X06[ShowChar - ' '], FontColor, BackColor);
    if (g_LCD.FontSize == LCD_FontSize16)
        LCD_ShowMonoImage(X, Y, 8, 16, LCD_F16X08[ShowChar - ' '], FontColor, BackColor);
    if (g_LCD.FontSize == LCD_FontSize24)
        LCD_ShowMonoImage(X, Y, 12, 24, LCD_F24X12[ShowChar - ' '], FontColor, BackColor);
    if (g_LCD.FontSize == LCD_FontSize32)
        LCD_ShowMonoImage(X, Y, 16, 32, LCD_F32X16[ShowChar - ' '], FontColor, BackColor);
}

/**
 * @brief  显示字符串
 * @param  X 起始X坐标
 * @param  Y 起始Y坐标
 * @param  String 待显示的字符串
 * @param  FontColor 前景色
 * @param  BackColor 背景色
 */
void LCD_ShowString(uint16_t X, uint16_t Y, const char *String, uint16_t FontColor, uint16_t BackColor)
{
    uint16_t i = 0;
    uint8_t CharLength = 0;
    uint16_t XOffset = 0;
    uint16_t FontSize = (uint16_t)g_LCD.FontSize / 2;  // 字体大小

    while (String[i] != '\0' && X + XOffset < g_LCD.Width)  // 遍历字符串
    {
        /* 使用LCD_ShowChar显示此字符 */
        if (String[i] >= ' ' && String[i] <= '~') { LCD_ShowChar(X + XOffset, Y, String[i], FontColor, BackColor); }
        else
        {
            LCD_ShowChar(X + XOffset, Y, ' ', FontColor, BackColor);
        }
        XOffset += FontSize, i++;
    }
}

/**
 * @brief  显示整数
 * @param  X 起始X坐标
 * @param  Y 起始Y坐标
 * @param  Number 待显示的整数
 * @param  Length 整数位数
 * @param  FontColor 前景色
 * @param  BackColor 背景色
 * @param  ShowSign 是否显示符号
 */
void LCD_ShowNumber(uint16_t X, uint16_t Y, int32_t Number, uint8_t Length, uint16_t FontColor, uint16_t BackColor, uint8_t ShowSign)
{
    uint8_t i, FontSize = g_LCD.FontSize >> 1;

    if (ShowSign != FALSE)
    {
        if (Number >= 0)  // 数字大于等于0
        {
            LCD_ShowChar(X, Y, '+', FontColor, BackColor);  // 显示+号
        }
        else  // 数字小于0
        {
            LCD_ShowChar(X, Y, '-', FontColor, BackColor);  // 显示-号
            Number = -Number;                               // Number1等于Number取负
        }

        X += FontSize;
    }

    for (i = 0; i < Length; i++)  // 遍历数字的每一位
    {
        /* 调用LCD_ShowChar函数，依次显示每个数字 */
        /* Number / LCD_Pow(10, Length - i - 1) % 10 可以十进制提取数字的每一位 */
        /* + '0' 可将数字转换为字符格式 */
        LCD_ShowChar(X + i * FontSize, Y, Number / LCD_Pow(10, Length - i - 1) % 10 + '0', FontColor, BackColor);
    }
}

/**
 * @brief  显示十六进制数字
 * @param  X 起始X坐标
 * @param  Y 起始Y坐标
 * @param  Number 待显示的数字
 * @param  Length 显示位数
 * @param  FontColor 前景色
 * @param  BackColor 背景色
 */
void LCD_ShowHexNumber(int16_t X, int16_t Y, uint32_t Number, uint8_t Length, uint16_t FontColor, uint16_t BackColor)
{
    uint8_t i, SingleNumber, FontSize = g_LCD.FontSize >> 1;

    for (i = 0; i < Length; i++) /* 遍历数字的每一位 */
    {
        /* 以十六进制提取数字的每一位 */
        SingleNumber = Number / LCD_Pow(16, Length - i - 1) % 16;

        if (SingleNumber < 10)  // 单个数字小于10
        {
            /* 调用LCD_ShowChar函数，显示此数字 */
            /* + '0' 可将数字转换为字符格式 */
            LCD_ShowChar(X + i * FontSize, Y, SingleNumber + '0', FontColor, BackColor);
        }
        else  // 单个数字大于10
        {
            /* 调用LCD_ShowChar函数，显示此数字 */
            /* + 'A' 可将数字转换为从A开始的十六进制字符 */
            LCD_ShowChar(X + i * FontSize, Y, SingleNumber - 10 + 'A', FontColor, BackColor);
        }
    }
}

/**
 * @brief  显示二进制数字
 * @param  X 起始X坐标
 * @param  Y 起始Y坐标
 * @param  Number 待显示的数字
 * @param  Length 显示位数
 * @param  FontColor 前景色
 * @param  BackColor 背景色
 */
void LCD_ShowBinNumber(int16_t X, int16_t Y, uint32_t Number, uint8_t Length, uint16_t FontColor, uint16_t BackColor)
{
    uint8_t i, FontSize = g_LCD.FontSize >> 1;

    for (i = 0; i < Length; i++)  // 遍历数字的每一位
    {
        /* 调用LCD_ShowChar函数，依次显示每个数字 */
        /* Number / LCD_Pow(2, Length - i - 1) % 2 可以二进制提取数字的每一位 */
        /* + '0' 可将数字转换为字符格式 */
        LCD_ShowChar(X + i * FontSize, Y, Number / LCD_Pow(2, Length - i - 1) % 2 + '0', FontColor, BackColor);
    }
}

/**
 * @brief  显示浮点数
 * @param  X 起始X坐标
 * @param  Y 起始Y坐标
 * @param  Number 待显示的浮点数
 * @param  IntLength 整数部分位数
 * @param  FraLength 小数部分位数
 * @param  FontColor 前景色
 * @param  BackColor 背景色
 * @param  ShowSign 是否显示符号
 */
void LCD_ShowFloatNumber(int16_t X, int16_t Y, double Number, uint8_t IntLength, uint8_t FraLength, uint16_t FontColor, uint16_t BackColor, uint8_t ShowSign)
{
    uint32_t PowNum, IntNum, FraNum, FontSize = g_LCD.FontSize >> 1;

    if (ShowSign != FALSE)
    {
        if (Number >= 0)  // 数字大于等于0
        {
            LCD_ShowChar(X, Y, '+', FontColor, BackColor);  // 显示+号
        }
        else  // 数字小于0
        {
            LCD_ShowChar(X, Y, '-', FontColor, BackColor);  // 显示-号
            Number = -Number;                               // Number取负
        }
    }

    /* 提取整数部分和小数部分 */
    IntNum = Number;                   // 直接赋值给整型变量，提取整数
    Number -= IntNum;                  // 将Number的整数减掉，防止之后将小数乘到整数时因数过大造成错误
    PowNum = LCD_Pow(10, FraLength);   // 根据指定小数的位数，确定乘数
    FraNum = roundf(Number * PowNum);  // 将小数乘到整数，同时四舍五入，避免显示误差
    IntNum += FraNum / PowNum;         // 若四舍五入造成了进位，则需要再加给整数

    /* 显示整数部分 */
    LCD_ShowNumber(X + FontSize, Y, IntNum, IntLength, FontColor, BackColor, FALSE);

    if (FraLength > 0)
    {
        /* 显示小数点 */
        LCD_ShowChar(X + (IntLength + 1) * FontSize, Y, '.', FontColor, BackColor);

        /* 显示小数部分 */
        LCD_ShowNumber(X + (IntLength + 2) * FontSize, Y, FraNum, FraLength, FontColor, BackColor, FALSE);
    }
}

/**
 * @brief  打印格式化字符串
 * @param  X 起始X坐标
 * @param  Y 起始Y坐标
 * @param  FontColor 前景色
 * @param  BackColor 背景色
 * @param  String 格式化字符串
 */
void LCD_Printf(uint16_t X, uint16_t Y, uint16_t FontColor, uint16_t BackColor, const char *String, ...)
{
    static char Buffer[100];        // 定义字符数组
    va_list arg;                    // 定义可变参数列表
    va_start(arg, String);          // 初始化可变参数列表
    vsprintf(Buffer, String, arg);  // 格式化字符串
    va_end(arg);                    // 结束可变参数列表
    LCD_ShowString(X, Y, Buffer, FontColor, BackColor);
}

/**
 * @brief  画直线
 * @param  X0 起点X坐标
 * @param  Y0 起点Y坐标
 * @param  X1 终点X坐标
 * @param  Y1 终点Y坐标
 * @param  Color 直线颜色
 */
void LCD_DrawLine(uint16_t X0, uint16_t Y0, uint16_t X1, uint16_t Y1, uint16_t Color)
{
    int16_t x, y, dx, dy, d, incrE, incrNE, temp;
    int16_t x0 = X0, y0 = Y0, x1 = X1, y1 = Y1;
    uint8_t yflag = 0, xyflag = 0;

    if (y0 == y1)  // 横线单独处理
    {
        /* 0号点X坐标大于1号点X坐标，则交换两点X坐标 */
        if (x0 > x1)
        {
            temp = x0;
            x0 = x1;
            x1 = temp;
        }

        /* 遍历X坐标 */
        LCD_Fill(x0, y0, x1, y0 + 1, Color);
    }
    else if (x0 == x1)  // 竖线单独处理
    {
        /*0号点Y坐标大于1号点Y坐标，则交换两点Y坐标*/
        if (y0 > y1)
        {
            temp = y0;
            y0 = y1;
            y1 = temp;
        }

        /*遍历Y坐标*/
        LCD_Fill(x0, y0, x0 + 1, y1, Color);
    }
    else  // 斜线
    {
        /* 使用Bresenham算法画直线，可以避免耗时的浮点运算，效率更高 */
        /* 参考文档：https://www.cs.montana.edu/courses/spring2009/425/dslectures/Bresenham.pdf */
        /* 参考教程：https://www.bilibili.com/video/BV1364y1d7Lo */

        if (x0 > x1)  // 0号点X坐标大于1号点X坐标
        {
            temp = x0;
            x0 = x1;
            x1 = temp;
            temp = y0;
            y0 = y1;
            y1 = temp;
        }

        if (y0 > y1)  // 0号点Y坐标大于1号点Y坐标
        {
            y0 = -y0;
            y1 = -y1;
            yflag = 1;
        }

        if (y1 - y0 > x1 - x0)  // 画线斜率大于1
        {
            temp = x0;
            x0 = y0;
            y0 = temp;
            temp = x1;
            x1 = y1;
            y1 = temp;
            xyflag = 1;
        }

        dx = x1 - x0;
        dy = y1 - y0;
        incrE = 2 * dy;
        incrNE = 2 * (dy - dx);
        d = 2 * dy - dx;
        x = x0;
        y = y0;

        if (yflag && xyflag)
        {
            LCD_DrawPoint(y, -x, Color);
        }
        else if (yflag)
        {
            LCD_DrawPoint(x, -y, Color);
        }
        else if (xyflag)
        {
            LCD_DrawPoint(y, x, Color);
        }
        else
        {
            LCD_DrawPoint(x, y, Color);
        }

        while (x < x1)  // 遍历X轴的每个点
        {
            x++;
            if (d < 0)  // 下一个点在当前点东方
            {
                d += incrE;
            }
            else  // 下一个点在当前点东北方
            {
                y++;
                d += incrNE;
            }

            /* 画每一个点，同时判断标志位，将坐标换回来 */
            if (yflag && xyflag)
            {
                LCD_DrawPoint(y, -x, Color);
            }
            else if (yflag)
            {
                LCD_DrawPoint(x, -y, Color);
            }
            else if (xyflag)
            {
                LCD_DrawPoint(y, x, Color);
            }
            else
            {
                LCD_DrawPoint(x, y, Color);
            }
        }
    }
}

/**
 * @brief  画矩形
 * @param  X 起始X坐标
 * @param  Y 起始Y坐标
 * @param  Width 矩形宽度
 * @param  Height 矩形高度
 * @param  Color 矩形颜色
 */
void LCD_DrawRectangle(uint16_t X, uint16_t Y, uint16_t Width, uint16_t Height, uint16_t Color)
{
    /* 判断是否为填充模式 */
    if (g_LCD.MonoFill == LCD_Cover)
    {
        LCD_Fill(X, Y, X + Width, Y + Height, Color);
    }
    else
    {
        LCD_DrawLine(X, Y, X + Width - 1, Y, Color);                            // 上边
        LCD_DrawLine(X, Y, X, Y + Height - 1, Color);                           // 左边
        LCD_DrawLine(X + Width - 1, Y, X + Width - 1, Y + Height - 1, Color);   // 右边
        LCD_DrawLine(X, Y + Height - 1, X + Width - 1, Y + Height - 1, Color);  // 下边
    }
}

/**
 * @brief  画三角形
 * @param  X0 第一个顶点X坐标
 * @param  Y0 第一个顶点Y坐标
 * @param  X1 第二个顶点X坐标
 * @param  Y1 第二个顶点Y坐标
 * @param  X2 第三个顶点X坐标
 * @param  Y2 第三个顶点Y坐标
 * @param  Color 三角形颜色
 */
void LCD_DrawTriangle(uint16_t X0, uint16_t Y0, uint16_t X1, uint16_t Y1, uint16_t X2, uint16_t Y2, uint16_t Color)
{
    uint16_t minx, miny, maxx, maxy;
    uint16_t i, j;
    uint16_t vx[] = { X0, X1, X2 };
    uint16_t vy[] = { Y0, Y1, Y2 };

    if (g_LCD.MonoFill == LCD_Transparent)  // 指定三角形不填充
    {
        LCD_DrawLine(X0, Y0, X1, Y1, Color);
        LCD_DrawLine(X0, Y0, X2, Y2, Color);
        LCD_DrawLine(X1, Y1, X2, Y2, Color);
    }
    else  // 指定三角形填充
    {
        minx = LCD_GetMin(vx, 3);
        miny = LCD_GetMin(vy, 3);
        maxx = LCD_GetMax(vx, 3);
        maxy = LCD_GetMax(vy, 3);

        for (i = minx; i <= maxx; i++)
        {
            for (j = miny; j <= maxy; j++)
            {
                if (LCD_IsPointInPoly(3, vx, vy, i, j))
                {
                    LCD_DrawPoint(i, j, Color);
                }
            }
        }
    }
}

/**
 * @brief  画多边形
 * @param  vx 顶点X坐标数组
 * @param  vy 顶点Y坐标数组
 * @param  n 顶点数
 * @param  Color 多边形颜色
 */
void LCD_DrawPoly(uint16_t vx[], uint16_t vy[], int n, uint16_t Color)
{
    uint16_t minx, miny, maxx, maxy;
    uint16_t i, j;

    if (g_LCD.MonoFill == LCD_Transparent)  // 指定填充
    {
        /* 调用画线函数，将各个点用直线连接 */
        for (i = 0; i < n - 1; i++)
        {
            LCD_DrawLine(vx[i], vy[i], vx[i + 1], vy[i + 1], Color);
        }
        LCD_DrawLine(vx[0], vx[0], vx[n - 1], vx[n - 1], Color);
    }
    else  // 指定填充
    {
        /* 找到最小的X、Y坐标 */
        minx = LCD_GetMin(vx, n);
        miny = LCD_GetMin(vy, n);

        /* 找到最大的X、Y坐标 */
        maxx = LCD_GetMax(vx, n);
        maxy = LCD_GetMax(vy, n);

        for (i = minx; i <= maxx; i++)
        {
            for (j = miny; j <= maxy; j++)
            {
                if (LCD_IsPointInPoly(n, vx, vy, i, j))
                {
                    LCD_DrawPoint(i, j, Color);
                }
            }
        }
    }
}

/**
 * @brief  绘制圆形
 * @param  x 圆心X坐标
 * @param  y 圆心Y坐标
 * @param  radius 圆半径
 * @param  color 圆形颜色
 * @note   使用中点圆算法绘制圆形
 */
void LCD_DrawCircle(uint16_t x, uint16_t y, uint16_t radius, uint16_t color)
{
    if (g_LCD.MonoFill == LCD_Cover)
    {
        /* 填充圆 */
        for (int16_t dy = -radius; dy <= radius; dy++)
        {
            for (int16_t dx = -radius; dx <= radius; dx++)
            {
                if (dx * dx + dy * dy <= radius * radius)
                {
                    LCD_DrawPoint(x + dx, y + dy, color);
                }
            }
        }
    }
    else
    {
        /* 绘制圆边框 */
        int16_t x0 = 0;
        int16_t y0 = radius;
        int16_t d = 3 - 2 * radius;

        while (x0 <= y0)
        {
            LCD_DrawPoint(x + x0, y + y0, color);
            LCD_DrawPoint(x - x0, y + y0, color);
            LCD_DrawPoint(x + x0, y - y0, color);
            LCD_DrawPoint(x - x0, y - y0, color);
            LCD_DrawPoint(x + y0, y + x0, color);
            LCD_DrawPoint(x - y0, y + x0, color);
            LCD_DrawPoint(x + y0, y - x0, color);
            LCD_DrawPoint(x - y0, y - x0, color);

            if (d < 0)
            {
                d += 4 * x0 + 6;
            }
            else
            {
                d += 4 * (x0 - y0) + 10;
                y0--;
            }
            x0++;
        }
    }
}

/**
 * @brief  画椭圆
 * @param  X 椭圆中心X坐标
 * @param  Y 椭圆中心Y坐标
 * @param  A 椭圆长轴
 * @param  B 椭圆短轴
 * @param  Color 椭圆颜色
 */
void LCD_DrawEllipse(uint16_t X, uint16_t Y, uint16_t A, uint16_t B, uint16_t Color)
{
    uint16_t x, y, j;
    uint16_t a = A, b = B;
    float d1, d2;

    /* 使用Bresenham算法画椭圆，可以避免部分耗时的浮点运算，效率更高 */
    /* 参考链接：https://blog.csdn.net/myf_666/article/details/128167392 */

    x = 0;
    y = b;
    d1 = b * b + a * a * (-b + 0.5);

    if (g_LCD.MonoFill == LCD_Cover)  // 指定椭圆填充
    {
        /* 遍历起始点Y坐标 */
        for (j = -y; j < y; j++)
        {
            /* 在指定区域画点，填充部分椭圆 */
            LCD_DrawPoint(X, Y + j, Color);
            LCD_DrawPoint(X, Y + j, Color);
        }
        LCD_Fill(X, Y - y, X + 1, Y + y, Color);
    }

    /* 画椭圆弧的起始点 */
    LCD_DrawPoint(X + x, Y + y, Color);
    LCD_DrawPoint(X - x, Y - y, Color);
    LCD_DrawPoint(X - x, Y + y, Color);
    LCD_DrawPoint(X + x, Y - y, Color);

    /* 画椭圆中间部分 */
    while (b * b * (x + 1) < a * a * (y - 0.5))
    {
        if (d1 <= 0) /* 下一个点在当前点东方 */
        {
            d1 += b * b * (2 * x + 3);
        }
        else /* 下一个点在当前点东南方 */
        {
            d1 += b * b * (2 * x + 3) + a * a * (-2 * y + 2);
            y--;
        }
        x++;

        if (g_LCD.MonoFill == LCD_Cover) /* 指定椭圆填充 */
        {
            /* 遍历中间部分 */
            LCD_Fill(X + x, Y - y, X + x + 1, Y + y, Color);
            LCD_Fill(X - x, Y - y, X - x + 1, Y + y, Color);
        }

        /* 画椭圆中间部分圆弧 */
        LCD_DrawPoint(X + x, Y + y, Color);
        LCD_DrawPoint(X - x, Y - y, Color);
        LCD_DrawPoint(X - x, Y + y, Color);
        LCD_DrawPoint(X + x, Y - y, Color);
    }

    /* 画椭圆两侧部分 */
    d2 = b * b * (x + 0.5) * (x + 0.5) + a * a * (y - 1) * (y - 1) - a * a * b * b;

    while (y > 0)
    {
        if (d2 <= 0)  // 下一个点在当前点东方
        {
            d2 += b * b * (2 * x + 2) + a * a * (-2 * y + 3);
            x++;
        }
        else  // 下一个点在当前点东南方
        {
            d2 += a * a * (-2 * y + 3);
        }
        y--;

        if (g_LCD.MonoFill == LCD_Cover)  // 指定椭圆填充
        {
            /* 遍历两侧部分 */
            LCD_Fill(X + x, Y - y, X + x + 1, Y + y, Color);
            LCD_Fill(X - x, Y - y, X - x + 1, Y + y, Color);
        }

        /* 画椭圆两侧部分圆弧 */
        LCD_DrawPoint(X + x, Y + y, Color);
        LCD_DrawPoint(X - x, Y - y, Color);
        LCD_DrawPoint(X - x, Y + y, Color);
        LCD_DrawPoint(X + x, Y - y, Color);
    }
}

/**
 * @brief  画粗直线
 * @param  X0 起点X坐标
 * @param  Y0 起点Y坐标
 * @param  X1 终点X坐标
 * @param  Y1 终点Y坐标
 * @param  thickness 直线粗细
 * @param  Color 直线颜色
 */
void LCD_DrawThickLine(uint16_t X0, uint16_t Y0, uint16_t X1, uint16_t Y1, uint16_t thickness, uint16_t Color)
{
    int16_t x, y, dx, dy, d, incrE, incrNE, temp;
    int16_t x0 = X0, y0 = Y0, x1 = X1, y1 = Y1;
    uint8_t yflag = 0, xyflag = 0;

    if (y0 == y1)  // 横线单独处理
    {
        /* 0号点X坐标大于1号点X坐标，则交换两点X坐标 */
        if (x0 > x1)
        {
            temp = x0;
            x0 = x1;
            x1 = temp;
        }

        /* 遍历X坐标 */
        LCD_Fill(x0, y0 - thickness, x1, y0 + thickness, Color);
        LCD_DrawCircle(x0, y0, thickness, Color);
        LCD_DrawCircle(x1, y0, thickness, Color);
    }
    else if (x0 == x1)  // 竖线单独处理
    {
        /*0号点Y坐标大于1号点Y坐标，则交换两点Y坐标*/
        if (y0 > y1)
        {
            temp = y0;
            y0 = y1;
            y1 = temp;
        }

        /*遍历Y坐标*/
        LCD_Fill(x0 - thickness, y0, x0 + thickness, y1, Color);
        LCD_DrawCircle(x0, y0, thickness, Color);
        LCD_DrawCircle(x1, y0, thickness, Color);
    }
    else  // 斜线
    {
        /* 使用Bresenham算法画直线，可以避免耗时的浮点运算，效率更高 */
        /* 参考文档：https://www.cs.montana.edu/courses/spring2009/425/dslectures/Bresenham.pdf */
        /* 参考教程：https://www.bilibili.com/video/BV1364y1d7Lo */

        if (x0 > x1)  // 0号点X坐标大于1号点X坐标
        {
            temp = x0;
            x0 = x1;
            x1 = temp;
            temp = y0;
            y0 = y1;
            y1 = temp;
        }

        if (y0 > y1)  // 0号点Y坐标大于1号点Y坐标
        {
            y0 = -y0;
            y1 = -y1;
            yflag = 1;
        }

        if (y1 - y0 > x1 - x0)  // 画线斜率大于1
        {
            temp = x0;
            x0 = y0;
            y0 = temp;
            temp = x1;
            x1 = y1;
            y1 = temp;
            xyflag = 1;
        }

        dx = x1 - x0;
        dy = y1 - y0;
        incrE = 2 * dy;
        incrNE = 2 * (dy - dx);
        d = 2 * dy - dx;
        x = x0;
        y = y0;

        if (yflag && xyflag)
        {
            LCD_DrawCircle(y, -x, thickness, Color);
        }
        else if (yflag)
        {
            LCD_DrawCircle(x, -y, thickness, Color);
        }
        else if (xyflag)
        {
            LCD_DrawCircle(y, x, thickness, Color);
        }
        else
        {
            LCD_DrawCircle(x, y, thickness, Color);
        }

        while (x < x1)  // 遍历X轴的每个点
        {
            x++;
            if (d < 0)  // 下一个点在当前点东方
            {
                d += incrE;
            }
            else  // 下一个点在当前点东北方
            {
                y++;
                d += incrNE;
            }

            /* 画每一个点，同时判断标志位，将坐标换回来 */
            if (yflag && xyflag)
            {
                LCD_DrawCircle(y, -x, thickness, Color);
            }
            else if (yflag)
            {
                LCD_DrawCircle(x, -y, thickness, Color);
            }
            else if (xyflag)
            {
                LCD_DrawCircle(y, x, thickness, Color);
            }
            else
            {
                LCD_DrawCircle(x, y, thickness, Color);
            }
        }
    }
}

/**
 * @brief  启动SPI传输
 * @param  dataWidth 数据宽度
 */
void LCD_SPI_StartTransmit(uint32_t dataWidth)
{
    /* 设置基本信息 */
    LL_SPI_SetDataWidth(LCD_SPI, dataWidth);
    LL_SPI_SetHalfDuplexDirection(LCD_SPI, LL_SPI_HALF_DUPLEX_TX);
    LL_SPI_SetTransferSize(LCD_SPI, 0);
    LL_SPI_Enable(LCD_SPI);

    /* 主机传输开始 */
    LL_SPI_StartMasterTransfer(LCD_SPI);
}

/**
 * @brief  结束SPI传输
 */
void LCD_SPI_EndTransmit(void)
{
    /* 等待Tx(和CRC)数据发送完成 */
    volatile uint32_t timeout = 1000; /* 超时时间 */
    while (!LL_SPI_IsActiveFlag_TXC(LCD_SPI) && timeout > 0UL)
    {
        timeout--;
    }

    /* 清除所有相关的标志位 */
    LL_SPI_ClearFlag_EOT(LCD_SPI);
    LL_SPI_ClearFlag_TXTF(LCD_SPI);

    /* 禁用SPI外设 */
    LL_SPI_Disable(LCD_SPI);

    /* 退出处理 */
    LL_SPI_DisableDMAReq_TX(LCD_SPI);
    LL_SPI_ClearFlag_OVR(LCD_SPI);
    LL_SPI_ClearFlag_CRCERR(LCD_SPI);
    LL_SPI_ClearFlag_MODF(LCD_SPI);
    LL_SPI_ClearFlag_FRE(LCD_SPI);
}

/**
 * @brief  通过SPI发送16位数据
 * @param  data 待发送的16位数据
 */
void LCD_SPI_SendData16(uint16_t data)
{
    while (!LL_SPI_IsActiveFlag_TXP(LCD_SPI));
    LL_SPI_TransmitData16(LCD_SPI, data);
}

/**
 * @brief  通过SPI以16位模式传输数据
 * @param  pData 数据指针
 * @param  size 数据大小
 * @param  mode 模式选择（非0时每次发送后指针递增）
 */
void LCD_SPI_TransmitData16(const uint16_t *pData, int32_t size, int8_t mode)
{
    /* 设置基本信息 */
    LCD_SPI_StartTransmit(LL_SPI_DATAWIDTH_16BIT);

    /* 以16位模式传输数据 */
    while (size > 0)
    {
        /* 等待TXP标志置位以发送数据 */
        LCD_SPI_SendData16(*pData);
        size--;
        if (mode) { pData++; }
    }

    /* 发送完成处理 */
    LCD_SPI_EndTransmit();
}

/**
 * @brief  通过SPI以8位模式传输数据
 * @param  pData 数据指针
 * @param  size 数据大小
 */
void LCD_SPI_TransmitData8(const uint8_t *pData, int32_t size)
{
    /* Pixel fills use 16-bit LL transfers; commands always use 8-bit HAL transfers. */
    LL_SPI_Disable(LCD_SPI);
    LL_SPI_SetDataWidth(LCD_SPI, LL_SPI_DATAWIDTH_8BIT);
    /* Finish every command/address byte before CS or DC changes. */
    while (size > 0)
    {
        uint16_t count = size > 65535 ? 65535 : (uint16_t)size;
        if (HAL_SPI_Transmit(&g_LCD.SPI, (uint8_t *)pData, count, 20) != HAL_OK) { return; }
        pData += count;
        size -= count;
    }
}

// ==================== 底层引擎配置宏 ====================
#define DMA2D_WAIT()        while (DMA2D->CR & DMA2D_CR_START)
#define DMA2D_STOP()        DMA2D->CR &= ~DMA2D_CR_START
#define DMA2D_SET_NLR(h, w) DMA2D->NLR = ((w) << 16) | (h)  // 标准写法：高为行数，宽为像素数

/**
 * @brief  使用DMA2D在内存之间复制一个矩形区域的RGB565图像数据
 * @param  x 目标区域起始X坐标
 * @param  y 目标区域起始Y坐标
 * @param  w 矩形区域宽度
 * @param  h 矩形区域高度
 * @param  src_ptr 源数据缓冲区指针
 * @param  dst_ptr 目标显示缓存指针
 * @param  lcd_width 目标缓存的行宽度
 * @note   1. 源数据要求每行紧密连续（无额外偏移），源行偏移（FGOR）固定为0。
 * 2. 目标行偏移自动计算为 (lcd_width - w)，确保只更新目标矩形区域。
 * 3. 格式固定为RGB565，不进行任何颜色转换，纯粹搬运。
 * 4. NLR寄存器采用标准配置：高16位为高度(h)，低16位为宽度(w)。
 * 5. 该函数为阻塞型，直到DMA2D传输完成才返回。
 * 6. 若源和目标内存有重叠，DMA2D不支持自动处理，需保证不重叠或使用CPU搬运。
 * 7. 确保 src_ptr 和 dst_ptr 指向的内存已分配且可访问，大小至少覆盖所需区域。
 * 8. 目标地址计算方式：dst_ptr + 2*(lcd_width*y + x)，每个像素占2字节。
 */
void DMA2D_MemCopy(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint32_t *src_ptr, uint32_t *dst_ptr, uint16_t lcd_width)
{
    DMA2D_STOP();
    DMA2D->CR = DMA2D_M2M;
    DMA2D->FGPFCCR = LTDC_PIXEL_FORMAT_RGB565;
    DMA2D->FGMAR = (uint32_t)src_ptr;
    DMA2D->FGOR = 0;

    DMA2D->OPFCCR = LTDC_PIXEL_FORMAT_RGB565;
    DMA2D->OMAR = (uint32_t)dst_ptr + 2 * (lcd_width * y + x);
    DMA2D->OOR = lcd_width - w;
    DMA2D_SET_NLR(h, w);
    DMA2D->CR |= DMA2D_CR_START;
    DMA2D_WAIT();
}

/**
 * @brief  使用DMA2D在目标缓存中填充一个指定矩形区域为纯色（RGB565）
 * @param  x 填充区域起始X坐标
 * @param  y 填充区域起始Y坐标
 * @param  w 填充区域宽度
 * @param  h 填充区域高度
 * @param  color 填充颜色值（RGB565格式）
 * @param  dst_ptr 目标显示缓存指针
 * @param  lcd_width 目标缓存的行宽度
 * @note   1. 目标行偏移自动计算为 (lcd_width - w)，只填充指定矩形，不影响同行的其他像素。
 * 2. 格式固定为RGB565，填充颜色直接写入，无需转换。
 * 3. NLR寄存器采用标准配置：高16位为高度(h)，低16位为宽度(w)。
 * 4. 该函数为阻塞型，直到DMA2D传输完成才返回。
 * 5. 典型应用：清屏、绘制背景色块、绘制纯色矩形控件。
 * 6. 确保 dst_ptr 指向的内存已分配且可访问，大小至少覆盖所需区域。
 */
void DMA2D_MemFill(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color, uint32_t *dst_ptr, uint16_t lcd_width)
{
    while (DMA2D->CR & DMA2D_CR_START);  // 等待传输完成
    DMA2D_STOP();
    DMA2D->CR = DMA2D_R2M;
    DMA2D->OPFCCR = LTDC_PIXEL_FORMAT_RGB565;
    DMA2D->OCOLR = color;
    DMA2D->OMAR = (uint32_t)dst_ptr + 2 * (lcd_width * y + x);
    DMA2D->OOR = lcd_width - w;
    DMA2D_SET_NLR(h, w);
    DMA2D->CR |= DMA2D_CR_START;
    DMA2D_WAIT();
}

/**
 * @brief  SPI发送完成回调函数
 */
void HAL_SPI_TxCpltCallback(SPI_HandleTypeDef *hspi)
{
    if (hspi == &g_LCD.SPI) g_LCD.FinishFlag = 1;
}

/**
 * @brief  DMA2_Stream0全局中断处理函数
 */
void DMA2_Stream0_IRQHandler(void)
{
    HAL_DMA_IRQHandler(&g_LCD.DMA);
}

/**
 * @brief  SPI5全局中断处理函数
 */
void SPI5_IRQHandler(void)
{
    HAL_SPI_IRQHandler(&g_LCD.SPI);
}