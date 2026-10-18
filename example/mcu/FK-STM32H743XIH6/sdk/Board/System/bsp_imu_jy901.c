/**
 * ***************************************************************************************************
 * @file        atk_ms901m.c
 * @author      正点原子团队(ALIENTEK)
 * @version     V1.0
 * @date        2022-06-21
 * @brief       ATK-MS901M模块驱动代码
 * @license     Copyright (c) 2020-2032, 广州市星翼电子科技有限公司
 * ***************************************************************************************************
 * @attention
 *
 * 实验平台:正点原子 阿波罗 H743开发板
 * 在线视频:www.yuanzige.com
 * 技术论坛:www.openedv.com
 * 公司网址:www.alientek.com
 * 购买地址:openedv.taobao.com
 *
 * ***************************************************************************************************
 */

#include "bsp_imu_jy901.h"
#include "bsp_system.h"
#include "bsp_periph.h"
#include "stm32h7xx_ll_spi.h"
#include "stm32h7xx_ll_gpio.h"
#include <string.h>

static SerialWrite p_WIT_SerialWriteFunc = NULL;
static WIT_I2cWrite p_WIT_I2cWriteFunc = NULL;
static WIT_I2cRead p_WIT_I2cReadFunc = NULL;
static WIT_CanWrite p_WIT_CanWriteFunc = NULL;
static WIT_RegUpdateCb p_WIT_RegUpdateCbFunc = NULL;
static WIT_DelaymsCb p_WIT_DelaymsFunc = NULL;

static uint8_t s_ucAddr = 0xff;
static uint8_t s_ucWIT_DataBuff[WIT_DATA_BUFF_SIZE];
static uint32_t s_uiWIT_DataCnt = 0, s_uiProtoclo = 0, s_uiReadRegIndex = 0;
int16_t WIT_Reg[WIT_REGSIZE];

#define WIT_FUNC_WR 0x06
#define WIT_FUNC_RD 0x03

static const uint8_t __auchCRCHi[256] = {
    0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81,
    0x40, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0,
    0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01,
    0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41,
    0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81,
    0x40, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0,
    0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01,
    0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40,
    0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81,
    0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0,
    0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01,
    0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41,
    0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81,
    0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0,
    0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01,
    0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41,
    0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81,
    0x40
};
static const uint8_t __auchCRCLo[256] = {
    0x00, 0xC0, 0xC1, 0x01, 0xC3, 0x03, 0x02, 0xC2, 0xC6, 0x06, 0x07, 0xC7, 0x05, 0xC5, 0xC4,
    0x04, 0xCC, 0x0C, 0x0D, 0xCD, 0x0F, 0xCF, 0xCE, 0x0E, 0x0A, 0xCA, 0xCB, 0x0B, 0xC9, 0x09,
    0x08, 0xC8, 0xD8, 0x18, 0x19, 0xD9, 0x1B, 0xDB, 0xDA, 0x1A, 0x1E, 0xDE, 0xDF, 0x1F, 0xDD,
    0x1D, 0x1C, 0xDC, 0x14, 0xD4, 0xD5, 0x15, 0xD7, 0x17, 0x16, 0xD6, 0xD2, 0x12, 0x13, 0xD3,
    0x11, 0xD1, 0xD0, 0x10, 0xF0, 0x30, 0x31, 0xF1, 0x33, 0xF3, 0xF2, 0x32, 0x36, 0xF6, 0xF7,
    0x37, 0xF5, 0x35, 0x34, 0xF4, 0x3C, 0xFC, 0xFD, 0x3D, 0xFF, 0x3F, 0x3E, 0xFE, 0xFA, 0x3A,
    0x3B, 0xFB, 0x39, 0xF9, 0xF8, 0x38, 0x28, 0xE8, 0xE9, 0x29, 0xEB, 0x2B, 0x2A, 0xEA, 0xEE,
    0x2E, 0x2F, 0xEF, 0x2D, 0xED, 0xEC, 0x2C, 0xE4, 0x24, 0x25, 0xE5, 0x27, 0xE7, 0xE6, 0x26,
    0x22, 0xE2, 0xE3, 0x23, 0xE1, 0x21, 0x20, 0xE0, 0xA0, 0x60, 0x61, 0xA1, 0x63, 0xA3, 0xA2,
    0x62, 0x66, 0xA6, 0xA7, 0x67, 0xA5, 0x65, 0x64, 0xA4, 0x6C, 0xAC, 0xAD, 0x6D, 0xAF, 0x6F,
    0x6E, 0xAE, 0xAA, 0x6A, 0x6B, 0xAB, 0x69, 0xA9, 0xA8, 0x68, 0x78, 0xB8, 0xB9, 0x79, 0xBB,
    0x7B, 0x7A, 0xBA, 0xBE, 0x7E, 0x7F, 0xBF, 0x7D, 0xBD, 0xBC, 0x7C, 0xB4, 0x74, 0x75, 0xB5,
    0x77, 0xB7, 0xB6, 0x76, 0x72, 0xB2, 0xB3, 0x73, 0xB1, 0x71, 0x70, 0xB0, 0x50, 0x90, 0x91,
    0x51, 0x93, 0x53, 0x52, 0x92, 0x96, 0x56, 0x57, 0x97, 0x55, 0x95, 0x94, 0x54, 0x9C, 0x5C,
    0x5D, 0x9D, 0x5F, 0x9F, 0x9E, 0x5E, 0x5A, 0x9A, 0x9B, 0x5B, 0x99, 0x59, 0x58, 0x98, 0x88,
    0x48, 0x49, 0x89, 0x4B, 0x8B, 0x8A, 0x4A, 0x4E, 0x8E, 0x8F, 0x4F, 0x8D, 0x4D, 0x4C, 0x8C,
    0x44, 0x84, 0x85, 0x45, 0x87, 0x47, 0x46, 0x86, 0x82, 0x42, 0x43, 0x83, 0x41, 0x81, 0x80,
    0x40
};

/**
 * @brief  CRC16校验函数
 * @param  puchMsg 待校验数据指针
 * @param  usDataLen 待校验数据长度
 * @retval CRC16校验结果
 */
static uint16_t __CRC16(uint8_t *puchMsg, uint16_t usDataLen)
{
    uint8_t uchCRCHi = 0xFF;
    uint8_t uchCRCLo = 0xFF;
    uint8_t uIndex;
    int i = 0;
    uchCRCHi = 0xFF;
    uchCRCLo = 0xFF;
    for (; i < usDataLen; i++)
    {
        uIndex = uchCRCHi ^ puchMsg[i];
        uchCRCHi = uchCRCLo ^ __auchCRCHi[uIndex];
        uchCRCLo = __auchCRCLo[uIndex];
    }
    return (uint16_t)(((uint16_t)uchCRCHi << 8) | (uint16_t)uchCRCLo);
}

/**
 * @brief  校验和计算函数
 * @param  data 待校验数据指针
 * @param  len 待校验数据长度
 * @retval 校验和结果
 */
static uint8_t __CaliSum(uint8_t *data, uint32_t len)
{
    uint32_t i;
    uint8_t ucCheck = 0;
    for (i = 0; i < len; i++) ucCheck += *(data + i);
    return ucCheck;
}

/**
 * @brief  串口发送函数注册
 * @param  Write_func 串口发送函数指针
 * @retval 0:成功;其他:失败
 */
int32_t WIT_SerialWriteRegister(SerialWrite Write_func)
{
    if (!Write_func) return WIT_HAL_INVAL;
    p_WIT_SerialWriteFunc = Write_func;
    return WIT_HAL_OK;
}

/**
 * @brief  数据处理函数
 * @param  ucIndex 数据索引
 * @param  p_data 数据指针
 * @param  uiLen 数据长度
 * @note   该函数用于处理接收到的数据，根据数据索引将数据存储
 */
static void CopeWIT_Data(uint8_t ucIndex, uint16_t *p_data, uint32_t uiLen)
{
    uint32_t uiReg1 = 0, uiReg2 = 0, uiReg1Len = 0, uiReg2Len = 0;
    uint16_t *p_usReg1Val = p_data;
    uint16_t *p_usReg2Val = p_data + 3;

    uiReg1Len = 4;
    switch (ucIndex)
    {
    case WIT_ACC:
        uiReg1 = WIT_AX;
        uiReg1Len = 3;
        uiReg2 = WIT_TEMP;
        uiReg2Len = 1;
        break;
    case WIT_ANGLE:
        uiReg1 = WIT_Roll;
        uiReg1Len = 3;
        uiReg2 = WIT_VERSION;
        uiReg2Len = 1;
        break;
    case WIT_TIME: uiReg1 = WIT_YYMM; break;
    case WIT_GYRO:
        uiReg1 = WIT_GX;
        uiLen = 3;
        break;
    case WIT_MAGNETIC:
        uiReg1 = WIT_HX;
        uiLen = 3;
        break;
    case WIT_DPORT: uiReg1 = WIT_D0Status; break;
    case WIT_PRESS: uiReg1 = WIT_PressureL; break;
    case WIT_GPS: uiReg1 = WIT_LonL; break;
    case WIT_VELOCITY: uiReg1 = WIT_GPSHeight; break;
    case WIT_QUATER: uiReg1 = WIT_Q0; break;
    case WIT_GSA: uiReg1 = WIT_SVNUM; break;
    case WIT_REGVALUE: uiReg1 = s_uiReadRegIndex; break;
    default:
        return;
    }
    if (uiLen == 3)
    {
        uiReg1Len = 3;
        uiReg2Len = 0;
    }
    if (uiReg1Len)
    {
        memcpy(&WIT_Reg[uiReg1], p_usReg1Val, uiReg1Len << 1);
        p_WIT_RegUpdateCbFunc(uiReg1, uiReg1Len);
    }
    if (uiReg2Len)
    {
        memcpy(&WIT_Reg[uiReg2], p_usReg2Val, uiReg2Len << 1);
        p_WIT_RegUpdateCbFunc(uiReg2, uiReg2Len);
    }
}

/**
 * @brief  串口数据接收处理函数
 * @param  ucData 接收到的数据字节
 * @note   该函数用于处理串口接收到的数据，根据协议解析数据并调用数据处理函数
 */
void WIT_SerialDataIn(uint8_t ucData)
{
    uint16_t usCRC16, usTemp, i, usData[4];
    uint8_t ucSum;

    if (p_WIT_RegUpdateCbFunc == NULL) return;
    s_ucWIT_DataBuff[s_uiWIT_DataCnt++] = ucData;
    switch (s_uiProtoclo)
    {
    case WIT_PROTOCOL_JY61:
    case WIT_PROTOCOL_NORMAL:
        if (s_ucWIT_DataBuff[0] != 0x55)
        {
            s_uiWIT_DataCnt--;
            memcpy(s_ucWIT_DataBuff, &s_ucWIT_DataBuff[1], s_uiWIT_DataCnt);
            return;
        }
        if (s_uiWIT_DataCnt >= 11)
        {
            ucSum = __CaliSum(s_ucWIT_DataBuff, 10);
            if (ucSum != s_ucWIT_DataBuff[10])
            {
                s_uiWIT_DataCnt--;
                memcpy(s_ucWIT_DataBuff, &s_ucWIT_DataBuff[1], s_uiWIT_DataCnt);
                return;
            }
            usData[0] = ((uint16_t)s_ucWIT_DataBuff[3] << 8) | (uint16_t)s_ucWIT_DataBuff[2];
            usData[1] = ((uint16_t)s_ucWIT_DataBuff[5] << 8) | (uint16_t)s_ucWIT_DataBuff[4];
            usData[2] = ((uint16_t)s_ucWIT_DataBuff[7] << 8) | (uint16_t)s_ucWIT_DataBuff[6];
            usData[3] = ((uint16_t)s_ucWIT_DataBuff[9] << 8) | (uint16_t)s_ucWIT_DataBuff[8];
            CopeWIT_Data(s_ucWIT_DataBuff[1], usData, 4);
            s_uiWIT_DataCnt = 0;
        }
        break;
    case WIT_PROTOCOL_905x_MODBUS:
    case WIT_PROTOCOL_MODBUS:
        if (s_uiWIT_DataCnt > 2)
        {
            if (s_ucWIT_DataBuff[1] != WIT_FUNC_RD)
            {
                s_uiWIT_DataCnt--;
                memcpy(s_ucWIT_DataBuff, &s_ucWIT_DataBuff[1], s_uiWIT_DataCnt);
                return;
            }
            if (s_uiWIT_DataCnt < (s_ucWIT_DataBuff[2] + 5)) return;
            usTemp = ((uint16_t)s_ucWIT_DataBuff[s_uiWIT_DataCnt - 2] << 8) | s_ucWIT_DataBuff[s_uiWIT_DataCnt - 1];
            usCRC16 = __CRC16(s_ucWIT_DataBuff, s_uiWIT_DataCnt - 2);
            if (usTemp != usCRC16)
            {
                s_uiWIT_DataCnt--;
                memcpy(s_ucWIT_DataBuff, &s_ucWIT_DataBuff[1], s_uiWIT_DataCnt);
                return;
            }
            usTemp = s_ucWIT_DataBuff[2] >> 1;
            for (i = 0; i < usTemp; i++)
            {
                WIT_Reg[i + s_uiReadRegIndex] = ((uint16_t)s_ucWIT_DataBuff[(i << 1) + 3] << 8) | s_ucWIT_DataBuff[(i << 1) + 4];
            }
            p_WIT_RegUpdateCbFunc(s_uiReadRegIndex, usTemp);
            s_uiWIT_DataCnt = 0;
        }
        break;
    case WIT_PROTOCOL_905x_CAN:
    case WIT_PROTOCOL_CAN:
    case WIT_PROTOCOL_I2C:
        s_uiWIT_DataCnt = 0;
        break;
    }
    if (s_uiWIT_DataCnt == WIT_DATA_BUFF_SIZE) s_uiWIT_DataCnt = 0;
}

/**
 * @brief  I2C读写函数注册
 * @param  write_func I2C写函数指针
 * @param  read_func I2C读函数指针
 * @retval 0:成功;其他:失败
 */
int32_t WIT_I2cFuncRegister(WIT_I2cWrite write_func, WIT_I2cRead read_func)
{
    if (!write_func) return WIT_HAL_INVAL;
    if (!read_func) return WIT_HAL_INVAL;
    p_WIT_I2cWriteFunc = write_func;
    p_WIT_I2cReadFunc = read_func;
    return WIT_HAL_OK;
}

/**
 * @brief  CAN写函数注册
 * @param  Write_func CAN写函数指针
 * @retval 0:成功;其他:失败
 */
int32_t WIT_CanWriteRegister(WIT_CanWrite Write_func)
{
    if (!Write_func) return WIT_HAL_INVAL;
    p_WIT_CanWriteFunc = Write_func;
    return WIT_HAL_OK;
}

/**
 * @brief  CAN数据接收处理函数
 * @param  ucData 接收到的数据字节数组
 * @param  ucLen 数据长度
 * @note   该函数用于处理CAN接收到的数据，根据协议解析数据并调用数据处理函数
 */
void WIT_CanDataIn(uint8_t ucData[8], uint8_t ucLen)
{
    uint16_t usData[3];
    uint8_t ucTemp;

    if (p_WIT_RegUpdateCbFunc == NULL) return;
    if (ucLen < 8) return;
    switch (s_uiProtoclo)
    {
    case WIT_PROTOCOL_905x_CAN:
        if (ucData[0] != 0x55) return;
        if (ucData[1] == 0x53)
        {
            switch (ucData[2])
            {
            case 0X01: ucTemp = WIT_LRoll; break;
            case 0X02: ucTemp = WIT_LPitch; break;
            case 0X03: ucTemp = WIT_LYaw; break;
            }
            WIT_Reg[ucTemp] = ((uint16_t)ucData[5] << 8) | ucData[4];
            WIT_Reg[ucTemp + 1] = ((uint16_t)ucData[7] << 8) | ucData[6];
            p_WIT_RegUpdateCbFunc(ucTemp, 2);
            break;
        }
    case WIT_PROTOCOL_CAN:
        if (ucData[0] != 0x55) return;
        usData[0] = ((uint16_t)ucData[3] << 8) | ucData[2];
        usData[1] = ((uint16_t)ucData[5] << 8) | ucData[4];
        usData[2] = ((uint16_t)ucData[7] << 8) | ucData[6];
        CopeWIT_Data(ucData[1], usData, 3);
        break;

    case WIT_PROTOCOL_NORMAL:
    case WIT_PROTOCOL_905x_MODBUS:
    case WIT_PROTOCOL_MODBUS:
    case WIT_PROTOCOL_I2C:
        break;
    }
}

/**
 * @brief  寄存器更新回调函数注册
 * @param  update_func 寄存器更新回调函数指针
 * @retval 0:成功;其他:失败
 */
int32_t WIT_RegisterCallBack(WIT_RegUpdateCb update_func)
{
    if (!update_func) return WIT_HAL_INVAL;
    p_WIT_RegUpdateCbFunc = update_func;
    return WIT_HAL_OK;
}

/**
 * @brief  写寄存器值
 * @param  uiReg 寄存器地址
 * @param  usData 写入的数据
 * @retval 0:成功;其他:失败
 */
int32_t WIT_WriteReg(uint32_t uiReg, uint16_t usData)
{
    uint16_t usCRC;
    uint8_t ucBuff[8];
    if (uiReg >= WIT_REGSIZE) return WIT_HAL_INVAL;
    switch (s_uiProtoclo)
    {
    case WIT_PROTOCOL_JY61:
        return WIT_HAL_INVAL;
    case WIT_PROTOCOL_NORMAL:
        if (p_WIT_SerialWriteFunc == NULL) return WIT_HAL_EMPTY;
        ucBuff[0] = 0xFF;
        ucBuff[1] = 0xAA;
        ucBuff[2] = uiReg & 0xFF;
        ucBuff[3] = usData & 0xff;
        ucBuff[4] = usData >> 8;
        p_WIT_SerialWriteFunc(ucBuff, 5);
        break;
    case WIT_PROTOCOL_905x_MODBUS:
    case WIT_PROTOCOL_MODBUS:
        if (p_WIT_SerialWriteFunc == NULL) return WIT_HAL_EMPTY;
        ucBuff[0] = s_ucAddr;
        ucBuff[1] = WIT_FUNC_WR;
        ucBuff[2] = uiReg >> 8;
        ucBuff[3] = uiReg & 0xFF;
        ucBuff[4] = usData >> 8;
        ucBuff[5] = usData & 0xff;
        usCRC = __CRC16(ucBuff, 6);
        ucBuff[6] = usCRC >> 8;
        ucBuff[7] = usCRC & 0xff;
        p_WIT_SerialWriteFunc(ucBuff, 8);
        break;
    case WIT_PROTOCOL_905x_CAN:
    case WIT_PROTOCOL_CAN:
        if (p_WIT_CanWriteFunc == NULL) return WIT_HAL_EMPTY;
        ucBuff[0] = 0xFF;
        ucBuff[1] = 0xAA;
        ucBuff[2] = uiReg & 0xFF;
        ucBuff[3] = usData & 0xff;
        ucBuff[4] = usData >> 8;
        p_WIT_CanWriteFunc(s_ucAddr, ucBuff, 5);
        break;
    case WIT_PROTOCOL_I2C:
        if (p_WIT_I2cWriteFunc == NULL) return WIT_HAL_EMPTY;
        ucBuff[0] = usData & 0xff;
        ucBuff[1] = usData >> 8;
        if (p_WIT_I2cWriteFunc(s_ucAddr << 1, uiReg, ucBuff, 2) != 1)
        {
            return WIT_HAL_ERROR;
        }
        break;
    default:
        return WIT_HAL_INVAL;
    }
    return WIT_HAL_OK;
}

/**
 * @brief  读取寄存器值
 * @param  uiReg 寄存器地址
 * @param  uiReadNum 读取寄存器数量
 * @retval 0:成功;其他:失败
 */
int32_t WIT_ReadReg(uint32_t uiReg, uint32_t uiReadNum)
{
    uint16_t usTemp, i;
    uint8_t ucBuff[8];
    if ((uiReg + uiReadNum) >= WIT_REGSIZE) return WIT_HAL_INVAL;
    switch (s_uiProtoclo)
    {
    case WIT_PROTOCOL_JY61:
        return WIT_HAL_INVAL;
    case WIT_PROTOCOL_NORMAL:
        if (uiReadNum > 4) return WIT_HAL_INVAL;
        if (p_WIT_SerialWriteFunc == NULL) return WIT_HAL_EMPTY;
        ucBuff[0] = 0xFF;
        ucBuff[1] = 0xAA;
        ucBuff[2] = 0x27;
        ucBuff[3] = uiReg & 0xff;
        ucBuff[4] = uiReg >> 8;
        p_WIT_SerialWriteFunc(ucBuff, 5);
        break;
    case WIT_PROTOCOL_905x_MODBUS:
    case WIT_PROTOCOL_MODBUS:
        if (p_WIT_SerialWriteFunc == NULL) return WIT_HAL_EMPTY;
        usTemp = uiReadNum << 1;
        if ((usTemp + 5) > WIT_DATA_BUFF_SIZE) return WIT_HAL_NOMEM;
        ucBuff[0] = s_ucAddr;
        ucBuff[1] = WIT_FUNC_RD;
        ucBuff[2] = uiReg >> 8;
        ucBuff[3] = uiReg & 0xFF;
        ucBuff[4] = uiReadNum >> 8;
        ucBuff[5] = uiReadNum & 0xff;
        usTemp = __CRC16(ucBuff, 6);
        ucBuff[6] = usTemp >> 8;
        ucBuff[7] = usTemp & 0xff;
        p_WIT_SerialWriteFunc(ucBuff, 8);
        break;
    case WIT_PROTOCOL_905x_CAN:
    case WIT_PROTOCOL_CAN:
        if (uiReadNum > 3) return WIT_HAL_INVAL;
        if (p_WIT_CanWriteFunc == NULL) return WIT_HAL_EMPTY;
        ucBuff[0] = 0xFF;
        ucBuff[1] = 0xAA;
        ucBuff[2] = 0x27;
        ucBuff[3] = uiReg & 0xff;
        ucBuff[4] = uiReg >> 8;
        p_WIT_CanWriteFunc(s_ucAddr, ucBuff, 5);
        break;
    case WIT_PROTOCOL_I2C:
        if (p_WIT_I2cReadFunc == NULL) return WIT_HAL_EMPTY;
        usTemp = uiReadNum << 1;
        if (WIT_DATA_BUFF_SIZE < usTemp) return WIT_HAL_NOMEM;
        if (p_WIT_I2cReadFunc(s_ucAddr << 1, uiReg, s_ucWIT_DataBuff, usTemp) == 1)
        {
            if (p_WIT_RegUpdateCbFunc == NULL) return WIT_HAL_EMPTY;
            for (i = 0; i < uiReadNum; i++)
            {
                WIT_Reg[i + uiReg] = ((uint16_t)s_ucWIT_DataBuff[(i << 1) + 1] << 8) | s_ucWIT_DataBuff[i << 1];
            }
            p_WIT_RegUpdateCbFunc(uiReg, uiReadNum);
        }
        break;
    default:
        return WIT_HAL_INVAL;
    }
    s_uiReadRegIndex = uiReg;

    return WIT_HAL_OK;
}

/**
 * @brief  初始化函数
 * @param  uiProtocol 通信协议类型
 * @param  ucAddr 设备地址
 * @retval 0:成功;其他:失败
 */
int32_t WIT_Init(uint32_t uiProtocol, uint8_t ucAddr)
{
    if (uiProtocol > WIT_PROTOCOL_905x_CAN) return WIT_HAL_INVAL;
    s_uiProtoclo = uiProtocol;
    s_ucAddr = ucAddr;
    s_uiWIT_DataCnt = 0;
    return WIT_HAL_OK;
}

/**
 * @brief  反初始化函数
 * @note   该函数用于清除注册的函数指针和重置相关变量
 */
void WIT_DeInit(void)
{
    p_WIT_SerialWriteFunc = NULL;
    p_WIT_I2cWriteFunc = NULL;
    p_WIT_I2cReadFunc = NULL;
    p_WIT_CanWriteFunc = NULL;
    p_WIT_RegUpdateCbFunc = NULL;
    p_WIT_DelaymsFunc = NULL;
    s_ucAddr = 0xff;
    s_uiWIT_DataCnt = 0;
    s_uiProtoclo = 0;
}

/**
 * @brief  延时函数注册
 * @param  delayms_func 延时函数指针
 * @retval 0:成功;其他:失败
 */
int32_t WIT_DelayMsRegister(WIT_DelaymsCb delayms_func)
{
    if (!delayms_func) return WIT_HAL_INVAL;
    p_WIT_DelaymsFunc = delayms_func;
    return WIT_HAL_OK;
}

/**
 * @brief  检查数值是否在指定范围内
 * @param  sTemp 待检查的数值
 * @param  sMin 最小值
 * @param  sMax 最大值
 * @retval 1:在范围内;0:不在范围内
 */
static char CheckRange(short sTemp, short sMin, short sMax)
{
    if ((sTemp >= sMin) && (sTemp <= sMax))
        return 1;
    else
        return 0;
}

/**
 * @brief  加速度计校准函数
 * @note   该函数用于启动加速度计的校准过程，要求设备水平放置，并发送相应的命令或设置寄存器以进入校准模式
 * @retval 0:成功;其他:失败
 */
int32_t WIT_StartAccCali(void)
{
    /*
        First place the equipment horizontally, and then perform the following operations
    */
    uint8_t ucBuff[3];
    if (s_uiProtoclo == WIT_PROTOCOL_JY61)
    {
        if (p_WIT_SerialWriteFunc == NULL) return WIT_HAL_EMPTY;
        ucBuff[0] = 0xFF;
        ucBuff[1] = 0xAA;
        ucBuff[2] = 0x67;
        p_WIT_SerialWriteFunc(ucBuff, 3);
        return WIT_HAL_OK;
    }
    if (WIT_WriteReg(WIT_KEY, WIT_KEY_UNLOCK) != WIT_HAL_OK) return WIT_HAL_ERROR;
    if (p_WIT_DelaymsFunc != NULL)
    {
        p_WIT_DelaymsFunc(20);
    }
    else
        return WIT_HAL_EMPTY;
    if (WIT_WriteReg(WIT_CALSW, WIT_CALGYROACC) != WIT_HAL_OK) return WIT_HAL_ERROR;
    return WIT_HAL_OK;
}

/**
 * @brief  停止加速度计校准函数
 * @note   该函数用于停止加速度计的校准过程，发送相应的命令或设置寄存器以退出校准模式
 * @retval 0:成功;其他:失败
 */
int32_t WIT_StopAccCali(void)
{
    if (s_uiProtoclo == WIT_PROTOCOL_JY61) return WIT_HAL_INVAL;
    if (WIT_WriteReg(WIT_KEY, WIT_KEY_UNLOCK) != WIT_HAL_OK) return WIT_HAL_ERROR;
    if (p_WIT_DelaymsFunc != NULL)
    {
        p_WIT_DelaymsFunc(20);
    }
    else
        return WIT_HAL_EMPTY;
    if (WIT_WriteReg(WIT_CALSW, WIT_NORMAL) != WIT_HAL_OK) return WIT_HAL_ERROR;
    return WIT_HAL_OK;
}

/**
 * @brief  磁力计校准函数
 * @note   该函数用于启动磁力计的校准过程，要求设备水平放置，并发送相应的命令或设置寄存器以进入校准模式
 * @retval 0:成功;其他:失败
 */
int32_t WIT_StartMagCali(void)
{
    if (s_uiProtoclo == WIT_PROTOCOL_JY61) return WIT_HAL_INVAL;
    if (WIT_WriteReg(WIT_KEY, WIT_KEY_UNLOCK) != WIT_HAL_OK) return WIT_HAL_ERROR;
    if (p_WIT_DelaymsFunc != NULL)
    {
        p_WIT_DelaymsFunc(20);
    }
    else
        return WIT_HAL_EMPTY;
    if (WIT_WriteReg(WIT_CALSW, WIT_CALMAGMM) != WIT_HAL_OK) return WIT_HAL_ERROR;
    return WIT_HAL_OK;
}

/**
 * @brief  停止磁力计校准函数
 * @note   该函数用于停止磁力计的校准过程，发送相应的命令或设置寄存器以退出校准模式
 * @retval 0:成功;其他:失败
 */
int32_t WIT_StopMagCali(void)
{
    if (s_uiProtoclo == WIT_PROTOCOL_JY61) return WIT_HAL_INVAL;
    if (WIT_WriteReg(WIT_KEY, WIT_KEY_UNLOCK) != WIT_HAL_OK) return WIT_HAL_ERROR;
    if (p_WIT_DelaymsFunc != NULL)
    {
        p_WIT_DelaymsFunc(20);
    }
    else
        return WIT_HAL_EMPTY;
    if (WIT_WriteReg(WIT_CALSW, WIT_NORMAL) != WIT_HAL_OK) return WIT_HAL_ERROR;
    return WIT_HAL_OK;
}

/**
 * @brief  设置串口波特率函数
 * @param  uiBaudIndex 波特率索引，必须在指定范围内
 * @note   该函数用于设置串口通信的波特率，根据协议类型发送相应的命令或设置寄存器以更改波特率
 * @retval 0:成功;其他:失败
 */
int32_t WIT_SetUartBaud(int32_t uiBaudIndex)
{
    uint8_t ucBuff[3];
    if (!CheckRange(uiBaudIndex, WIT_BAUD_4800, WIT_BAUD_230400))
    {
        return WIT_HAL_INVAL;
    }
    if (s_uiProtoclo == WIT_PROTOCOL_JY61)
    {
        if ((uiBaudIndex == WIT_BAUD_115200) || (uiBaudIndex == WIT_BAUD_9600))
        {
            if (p_WIT_SerialWriteFunc == NULL) return WIT_HAL_EMPTY;
            ucBuff[0] = 0xFF;
            ucBuff[1] = 0xAA;
            if (uiBaudIndex == WIT_BAUD_115200)
                ucBuff[2] = 0x63;
            else if (uiBaudIndex == WIT_BAUD_9600)
                ucBuff[2] = 0x64;
            p_WIT_SerialWriteFunc(ucBuff, 3);
            return WIT_HAL_OK;
        }
        else
            return WIT_HAL_INVAL;
    }
    if (WIT_WriteReg(WIT_KEY, WIT_KEY_UNLOCK) != WIT_HAL_OK) return WIT_HAL_ERROR;
    if (p_WIT_DelaymsFunc != NULL)
    {
        p_WIT_DelaymsFunc(20);
    }
    else
        return WIT_HAL_EMPTY;
    if (WIT_WriteReg(WIT_BAUD, uiBaudIndex) != WIT_HAL_OK) return WIT_HAL_ERROR;
    return WIT_HAL_OK;
}

/**
 * @brief  设置CAN波特率函数
 * @param  uiBaudIndex 波特率索引，必须在指定范围内
 * @note   该函数用于设置CAN通信的波特率，根据协议类型发送相应的命令或设置寄存器以更改波特率
 * @retval 0:成功;其他:失败
 */
int32_t WIT_SetCanBaud(int32_t uiBaudIndex)
{
    if (!(s_uiProtoclo == WIT_PROTOCOL_CAN || s_uiProtoclo == WIT_PROTOCOL_905x_CAN)) return WIT_HAL_INVAL;
    if (!CheckRange(uiBaudIndex, WIT_CAN_BAUD_1000000, WIT_CAN_BAUD_3000))
    {
        return WIT_HAL_INVAL;
    }
    if (WIT_WriteReg(WIT_KEY, WIT_KEY_UNLOCK) != WIT_HAL_OK) return WIT_HAL_ERROR;
    if (p_WIT_DelaymsFunc != NULL)
    {
        p_WIT_DelaymsFunc(20);
    }
    else
        return WIT_HAL_EMPTY;
    if (WIT_WriteReg(WIT_BAUD, uiBaudIndex) != WIT_HAL_OK) return WIT_HAL_ERROR;
    return WIT_HAL_OK;
}

/**
 * @brief  设置数据带宽函数
 * @param  uiBaudWidth 带宽索引，必须在指定范围内
 * @note   该函数用于设置数据的带宽，根据协议类型发送相应的命令或设置寄存器以更改数据带宽
 * @retval 0:成功;其他:失败
 */
int32_t WIT_SetBandwidth(int32_t uiBaudWidth)
{
    if (s_uiProtoclo == WIT_PROTOCOL_JY61) return WIT_HAL_INVAL;
    if (!CheckRange(uiBaudWidth, WIT_BANDWIDTH_256HZ, WIT_BANDWIDTH_5HZ))
    {
        return WIT_HAL_INVAL;
    }
    if (p_WIT_DelaymsFunc != NULL)
    {
        p_WIT_DelaymsFunc(20);
    }
    else
        return WIT_HAL_EMPTY;
    if (WIT_WriteReg(WIT_BANDWIDTH, uiBaudWidth) != WIT_HAL_OK) return WIT_HAL_ERROR;
    return WIT_HAL_OK;
}
/**
 * @brief  设置输出频率函数
 * @param  uiRate 输出频率索引，必须在指定范围内
 * @note   该函数用于设置数据的输出频率，根据协议类型发送相应的命令或设置寄存器以更改输出频率
 * @retval 0:成功;其他:失败
 */
int32_t WIT_SetOutputRate(int32_t uiRate)
{
    if (s_uiProtoclo == WIT_PROTOCOL_JY61) return WIT_HAL_INVAL;
    if (!CheckRange(uiRate, WIT_RRATE_02HZ, WIT_RRATE_NONE))
    {
        return WIT_HAL_INVAL;
    }
    if (WIT_WriteReg(WIT_KEY, WIT_KEY_UNLOCK) != WIT_HAL_OK) return WIT_HAL_ERROR;
    if (p_WIT_DelaymsFunc != NULL)
    {
        p_WIT_DelaymsFunc(20);
    }
    else
        return WIT_HAL_EMPTY;
    if (WIT_WriteReg(WIT_RRATE, uiRate) != WIT_HAL_OK) return WIT_HAL_ERROR;
    return WIT_HAL_OK;
}

/**
 * @brief  设置内容函数
 * @param  uiRsw 内容索引，必须在指定范围内
 * @note   该函数用于设置数据的内容，根据协议类型发送相应的命令或设置寄存器以更改内容
 * @retval 0:成功;其他:失败
 */
int32_t WIT_SetContent(int32_t uiRsw)
{
    if (s_uiProtoclo == WIT_PROTOCOL_JY61) return WIT_HAL_INVAL;
    if (!CheckRange(uiRsw, WIT_RSW_TIME, WIT_RSW_MASK))
    {
        return WIT_HAL_INVAL;
    }
    if (WIT_WriteReg(WIT_KEY, WIT_KEY_UNLOCK) != WIT_HAL_OK) return WIT_HAL_ERROR;
    if (p_WIT_DelaymsFunc != NULL)
    {
        p_WIT_DelaymsFunc(20);
    }
    else
        return WIT_HAL_EMPTY;
    if (WIT_WriteReg(WIT_RSW, uiRsw) != WIT_HAL_OK) return WIT_HAL_ERROR;
    return WIT_HAL_OK;
}

/**
 * @brief  保存参数函数
 * @note   该函数用于保存当前的参数设置，发送相应的命令或设置寄存器以将参数保存到设备中
 * @retval 0:成功;其他:失败
 */
int32_t WIT_SaveParameter()
{
    if (s_uiProtoclo == WIT_PROTOCOL_JY61) return WIT_HAL_INVAL;
    if (WIT_WriteReg(WIT_KEY, WIT_KEY_UNLOCK) != WIT_HAL_OK) return WIT_HAL_ERROR;
    if (p_WIT_DelaymsFunc != NULL)
    {
        p_WIT_DelaymsFunc(20);
    }
    else
        return WIT_HAL_EMPTY;
    if (WIT_WriteReg(WIT_SAVE, WIT_SAVE_PARAM) != WIT_HAL_OK) return WIT_HAL_ERROR;
    return WIT_HAL_OK;
}

/**
 * @brief  恢复出厂设置函数
 * @note   该函数用于将设备恢复到出厂设置，发送相应的命令或设置寄存器以执行恢复操作
 * @retval 0:成功;其他:失败
 */
int32_t WIT_SetForReset()
{
    if (s_uiProtoclo == WIT_PROTOCOL_JY61) return WIT_HAL_INVAL;
    if (WIT_WriteReg(WIT_KEY, WIT_KEY_UNLOCK) != WIT_HAL_OK) return WIT_HAL_ERROR;
    if (p_WIT_DelaymsFunc != NULL)
    {
        p_WIT_DelaymsFunc(20);
    }
    else
        return WIT_HAL_EMPTY;
    if (WIT_WriteReg(WIT_SAVE, WIT_SAVE_SWRST) != WIT_HAL_OK) return WIT_HAL_ERROR;
    return WIT_HAL_OK;
}

/**
 * @brief  参考角度校准函数
 * @note   该函数用于校准设备的参考角度，要求设备水平放置，并发送相应的命令或设置寄存器以进入参考角度校准模式
 * @retval 0:成功;其他:失败
 */
int32_t WIT_CaliRefAngle(void)
{
    if (s_uiProtoclo == WIT_PROTOCOL_JY61) return WIT_HAL_INVAL;
    if (WIT_WriteReg(WIT_KEY, WIT_KEY_UNLOCK) != WIT_HAL_OK) return WIT_HAL_ERROR;
    if (p_WIT_DelaymsFunc != NULL)
    {
        p_WIT_DelaymsFunc(20);
    }
    else
        return WIT_HAL_EMPTY;
    if (WIT_WriteReg(WIT_CALSW, WIT_CALREFANGLE) != WIT_HAL_OK) return WIT_HAL_ERROR;
    return WIT_HAL_OK;
}
