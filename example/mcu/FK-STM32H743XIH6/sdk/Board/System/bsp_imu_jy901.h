#ifndef __BSP_IMU_DEVICE_H
#define __BSP_IMU_DEVICE_H

#include "bsp_system.h"

#ifdef __cplusplus
extern "C" {
#endif

#define WIT_REGSIZE              0x90

#define WIT_SAVE                 0x00
#define WIT_CALSW                0x01
#define WIT_RSW                  0x02
#define WIT_RRATE                0x03
#define WIT_BAUD                 0x04
#define WIT_AXOFFSET             0x05
#define WIT_AYOFFSET             0x06
#define WIT_AZOFFSET             0x07
#define WIT_GXOFFSET             0x08
#define WIT_GYOFFSET             0x09
#define WIT_GZOFFSET             0x0a
#define WIT_HXOFFSET             0x0b
#define WIT_HYOFFSET             0x0c
#define WIT_HZOFFSET             0x0d
#define WIT_D0MODE               0x0e
#define WIT_D1MODE               0x0f
#define WIT_D2MODE               0x10
#define WIT_D3MODE               0x11
#define WIT_D0PWMH               0x12
#define WIT_D1PWMH               0x13
#define WIT_D2PWMH               0x14
#define WIT_D3PWMH               0x15
#define WIT_D0PWMT               0x16
#define WIT_D1PWMT               0x17
#define WIT_D2PWMT               0x18
#define WIT_D3PWMT               0x19
#define WIT_IICADDR              0x1a
#define WIT_LEDOFF               0x1b
#define WIT_MAGRANGX             0x1c
#define WIT_MAGRANGY             0x1d
#define WIT_MAGRANGZ             0x1e
#define WIT_BANDWIDTH            0x1f
#define WIT_GYRORANGE            0x20
#define WIT_ACCRANGE             0x21
#define WIT_SLEEP                0x22
#define WIT_ORIENT               0x23
#define WIT_AXIS6                0x24
#define WIT_FILTK                0x25
#define WIT_GPSBAUD              0x26
#define WIT_READADDR             0x27
#define WIT_BWSCALE              0x28
#define WIT_MOVETHR              0x28
#define WIT_MOVESTA              0x29
#define WIT_ACCFILT              0x2A
#define WIT_GYROFILT             0x2b
#define WIT_MAGFILT              0x2c
#define WIT_POWONSEND            0x2d
#define WIT_VERSION              0x2e
#define WIT_CCBW                 0x2f
#define WIT_YYMM                 0x30
#define WIT_DDHH                 0x31
#define WIT_MMSS                 0x32
#define WIT_MS                   0x33
#define WIT_AX                   0x34
#define WIT_AY                   0x35
#define WIT_AZ                   0x36
#define WIT_GX                   0x37
#define WIT_GY                   0x38
#define WIT_GZ                   0x39
#define WIT_HX                   0x3a
#define WIT_HY                   0x3b
#define WIT_HZ                   0x3c
#define WIT_Roll                 0x3d
#define WIT_Pitch                0x3e
#define WIT_Yaw                  0x3f
#define WIT_TEMP                 0x40

/* High precision */
#define WIT_LRoll                0x3d
#define WIT_HRoll                0x3e
#define WIT_LPitch               0x3f
#define WIT_HPitch               0x40
#define WIT_LYaw                 0x41
#define WIT_HYaw                 0x42
#define WIT_TEMP905x             0x43

#define WIT_D0Status             0x41
#define WIT_D1Status             0x42
#define WIT_D2Status             0x43
#define WIT_D3Status             0x44
#define WIT_PressureL            0x45
#define WIT_PressureH            0x46
#define WIT_HeightL              0x47
#define WIT_HeightH              0x48
#define WIT_LonL                 0x49
#define WIT_LonH                 0x4a
#define WIT_LatL                 0x4b
#define WIT_LatH                 0x4c
#define WIT_GPSHeight            0x4d
#define WIT_GPSYAW               0x4e
#define WIT_GPSVL                0x4f
#define WIT_GPSVH                0x50
#define WIT_Q0                   0x51
#define WIT_Q1                   0x52
#define WIT_Q2                   0x53
#define WIT_Q3                   0x54
#define WIT_SVNUM                0x55
#define WIT_PDOP                 0x56
#define WIT_HDOP                 0x57
#define WIT_VDOP                 0x58
#define WIT_DELAYT               0x59
#define WIT_XMIN                 0x5a
#define WIT_XMAX                 0x5b
#define WIT_BATVAL               0x5c
#define WIT_ALARMPIN             0x5d
#define WIT_YMIN                 0x5e
#define WIT_YMAX                 0x5f
#define WIT_GYROZSCALE           0x60
#define WIT_GYROCALITHR          0x61
#define WIT_ALARMLEVEL           0x62
#define WIT_GYROCALTIME          0x63
#define WIT_REFROLL              0x64
#define WIT_REFPITCH             0x65
#define WIT_REFYAW               0x66
#define WIT_GPSTYPE              0x67
#define WIT_TRIGTIME             0x68
#define WIT_KEY                  0x69
#define WIT_WERROR               0x6a
#define WIT_TIMEZONE             0x6b
#define WIT_CALICNT              0x6c
#define WIT_WZCNT                0x6d
#define WIT_WZTIME               0x6e
#define WIT_WZSTATIC             0x6f
#define WIT_ACCSENSOR            0x70
#define WIT_GYROSENSOR           0x71
#define WIT_MAGSENSOR            0x72
#define WIT_PRESSENSOR           0x73
#define WIT_MODDELAY             0x74

#define WIT_ANGLEAXIS            0x75
#define WIT_XRSCALE              0x76
#define WIT_YRSCALE              0x77
#define WIT_ZRSCALE              0x78

#define WIT_XREFROLL             0x79
#define WIT_YREFPITCH            0x7a
#define WIT_ZREFYAW              0x7b

#define WIT_ANGXOFFSET           0x7c
#define WIT_ANGYOFFSET           0x7d
#define WIT_ANGZOFFSET           0x7e

#define WIT_NUMBERID1            0x7f
#define WIT_NUMBERID2            0x80
#define WIT_NUMBERID3            0x81
#define WIT_NUMBERID4            0x82
#define WIT_NUMBERID5            0x83
#define WIT_NUMBERID6            0x84

#define WIT_XA85PSCALE           0x85
#define WIT_XA85NSCALE           0x86
#define WIT_YA85PSCALE           0x87
#define WIT_YA85NSCALE           0x88
#define WIT_XA30PSCALE           0x89
#define WIT_XA30NSCALE           0x8a
#define WIT_YA30PSCALE           0x8b
#define WIT_YA30NSCALE           0x8c

#define WIT_CHIPIDL              0x8D
#define WIT_CHIPIDH              0x8E
#define WIT_REGINITFLAG          REGSIZE - 1

/* AXIS6 */
#define WIT_ALGRITHM9            0
#define WIT_ALGRITHM6            1

/** **********CALSW**************/
#define WIT_NORMAL               0x00
#define WIT_CALGYROACC           0x01
#define WIT_CALMAG               0x02
#define WIT_CALALTITUDE          0x03
#define WIT_CALANGLEZ            0x04
#define WIT_CALACCL              0x05
#define WIT_CALACCR              0x06
#define WIT_CALMAGMM             0x07
#define WIT_CALREFANGLE          0x08
#define WIT_CALMAG2STEP          0x09
// #define WIT_CALACCX         0x09
// #define WIT_ACC45PRX        0x0A
// #define WIT_ACC45NRX        0x0B
// #define WIT_CALACCY         0x0C
// #define WIT_ACC45PRY        0x0D
// #define WIT_ACC45NRY     	  0x0E
// #define WIT_CALREFANGLER    0x0F
// #define WIT_CALACCINIT      0x10
// #define WIT_CALREFANGLEINIT 0x11
#define WIT_CALHEXAHEDRON        0x12

/************OUTPUTHEAD**************/
#define WIT_TIME                 0x50
#define WIT_ACC                  0x51
#define WIT_GYRO                 0x52
#define WIT_ANGLE                0x53
#define WIT_MAGNETIC             0x54
#define WIT_DPORT                0x55
#define WIT_PRESS                0x56
#define WIT_GPS                  0x57
#define WIT_VELOCITY             0x58
#define WIT_QUATER               0x59
#define WIT_GSA                  0x5A
#define WIT_REGVALUE             0x5F

/************RSW**************/
#define WIT_RSW_TIME             0x01
#define WIT_RSW_ACC              0x02
#define WIT_RSW_GYRO             0x04
#define WIT_RSW_ANGLE            0x08
#define WIT_RSW_MAG              0x10
#define WIT_RSW_PORT             0x20
#define WIT_RSW_PRESS            0x40
#define WIT_RSW_GPS              0x80
#define WIT_RSW_V                0x100
#define WIT_RSW_Q                0x200
#define WIT_RSW_GSA              0x400
#define WIT_RSW_MASK             0xfff

/**RRATE*****/
#define WIT_RRATE_NONE           0x0d
#define WIT_RRATE_02HZ           0x01
#define WIT_RRATE_05HZ           0x02
#define WIT_RRATE_1HZ            0x03
#define WIT_RRATE_2HZ            0x04
#define WIT_RRATE_5HZ            0x05
#define WIT_RRATE_10HZ           0x06
#define WIT_RRATE_20HZ           0x07
#define WIT_RRATE_50HZ           0x08
#define WIT_RRATE_100HZ          0x09
#define WIT_RRATE_125HZ          0x0a  // only WT931
#define WIT_RRATE_200HZ          0x0b
#define WIT_RRATE_ONCE           0x0c

/* BAUD */
#define WIT_BAUD_4800            1
#define WIT_BAUD_9600            2
#define WIT_BAUD_19200           3
#define WIT_BAUD_38400           4
#define WIT_BAUD_57600           5
#define WIT_BAUD_115200          6
#define WIT_BAUD_230400          7
#define WIT_BAUD_460800          8
#define WIT_BAUD_921600          9

/*CAN BAUD*/
#define WIT_CAN_BAUD_1000000     0
#define WIT_CAN_BAUD_800000      1
#define WIT_CAN_BAUD_500000      2
#define WIT_CAN_BAUD_400000      3
#define WIT_CAN_BAUD_250000      4
#define WIT_CAN_BAUD_200000      5
#define WIT_CAN_BAUD_125000      6
#define WIT_CAN_BAUD_100000      7
#define WIT_CAN_BAUD_80000       8
#define WIT_CAN_BAUD_50000       9
#define WIT_CAN_BAUD_40000       10
#define WIT_CAN_BAUD_20000       11
#define WIT_CAN_BAUD_10000       12
#define WIT_CAN_BAUD_5000        13
#define WIT_CAN_BAUD_3000        14

/* KEY */
#define WIT_KEY_UNLOCK           0xB588

/* SAVE */
#define WIT_SAVE_PARAM           0x00
#define WIT_SAVE_SWRST           0xFF

/* ORIENT */
#define WIT_ORIENT_HERIZONE      0
#define WIT_ORIENT_VERTICLE      1

/* BANDWIDTH */
#define WIT_BANDWIDTH_256HZ      0
#define WIT_BANDWIDTH_184HZ      1
#define WIT_BANDWIDTH_94HZ       2
#define WIT_BANDWIDTH_44HZ       3
#define WIT_BANDWIDTH_21HZ       4
#define WIT_BANDWIDTH_10HZ       5
#define WIT_BANDWIDTH_5HZ        6

#define WIT_HAL_OK               (0)  /**< There is no error */
#define WIT_HAL_BUSY             (-1) /**< Busy */
#define WIT_HAL_TIMEOUT          (-2) /**< Timed out */
#define WIT_HAL_ERROR            (-3) /**< A generic error happens */
#define WIT_HAL_NOMEM            (-4) /**< No memory */
#define WIT_HAL_EMPTY            (-5) /**< The resource is empty */
#define WIT_HAL_INVAL            (-6) /**< Invalid argument */

#define WIT_DATA_BUFF_SIZE       256

#define WIT_PROTOCOL_NORMAL      0
#define WIT_PROTOCOL_MODBUS      1
#define WIT_PROTOCOL_CAN         2
#define WIT_PROTOCOL_I2C         3
#define WIT_PROTOCOL_JY61        4
#define WIT_PROTOCOL_905x_MODBUS 5
#define WIT_PROTOCOL_905x_CAN    6

/* serial function */
typedef void (*SerialWrite)(uint8_t *p_ucData, uint32_t uiLen);
int32_t WIT_SerialWriteRegister(SerialWrite write_func);
void WIT_SerialDataIn(uint8_t ucData);

/* iic function */

/*
    i2c write function example

    int32_t WIT_I2cWrite(uint8_t ucAddr, uint8_t ucReg, uint8_t *p_ucVal, uint32_t uiLen)
    {
        i2c_start();
        i2c_send(ucAddr);
        if(i2c_wait_ask() != SUCCESS)return 0;
        i2c_send(ucReg);
        if(i2c_wait_ask() != SUCCESS)return 0;
        for(uint32_t i = 0; i < uiLen; i++)
        {
            i2c_send(*p_ucVal++);
            if(i2c_wait_ask() != SUCCESS)return 0;
        }
        i2c_stop();
        return 1;
    }
 */
typedef int32_t (*WIT_I2cWrite)(uint8_t ucAddr, uint8_t ucReg, uint8_t *p_ucVal, uint32_t uiLen);
/*
    i2c read function example

    int32_t WIT_I2cRead(uint8_t ucAddr, uint8_t ucReg, uint8_t *p_ucVal, uint32_t uiLen)
    {
        i2c_start();
        i2c_send(ucAddr);
        if(i2c_wait_ask() != SUCCESS)return 0;
        i2c_send(ucReg);
        if(i2c_wait_ask() != SUCCESS)return 0;

        i2c_start();
        i2c_send(ucAddr+1);
        for(uint32_t i = 0; i < uiLen; i++)
        {
            if(i+1 == uiLen)*p_ucVal++ = i2c_read(0);  //last byte no ask
            else *p_ucVal++ = i2c_read(1);  //  ask
        }
        i2c_stop();
        return 1;
    }
*/
typedef int32_t (*WIT_I2cRead)(uint8_t ucAddr, uint8_t ucReg, uint8_t *p_ucVal, uint32_t uiLen);
int32_t WIT_I2cFuncRegister(WIT_I2cWrite write_func, WIT_I2cRead read_func);

/* can function */
typedef void (*WIT_CanWrite)(uint8_t ucStdId, uint8_t *p_ucData, uint32_t uiLen);
int32_t WIT_CanWriteRegister(WIT_CanWrite write_func);

/* Delayms function */
typedef void (*WIT_DelaymsCb)(uint16_t ucMs);
int32_t WIT_DelayMsRegister(WIT_DelaymsCb delayms_func);

void WIT_CanDataIn(uint8_t ucData[8], uint8_t ucLen);

typedef void (*WIT_RegUpdateCb)(uint32_t uiReg, uint32_t uiRegNum);
int32_t WIT_RegisterCallBack(WIT_RegUpdateCb update_func);
int32_t WIT_WriteReg(uint32_t uiReg, uint16_t usData);
int32_t WIT_ReadReg(uint32_t uiReg, uint32_t uiReadNum);
int32_t WIT_Init(uint32_t uiProtocol, uint8_t ucAddr);
void WIT_DeInit(void);

/**
 * *****************************************************************************
 * @file    wit_c_sdk.h
 * @author  WIT_
 * @version V1.0
 * @date    05-May-2022
 * @brief   This file provides all Configure sensor function.
 * *****************************************************************************
 * @attention
 *
 *        http://wit-motion.cn/
 *
 * *****************************************************************************
 */
int32_t WIT_StartAccCali(void);
int32_t WIT_StopAccCali(void);
int32_t WIT_StartMagCali(void);
int32_t WIT_StopMagCali(void);
int32_t WIT_SetUartBaud(int32_t uiBaudIndex);
int32_t WIT_SetBandwidth(int32_t uiBaudWidth);
int32_t WIT_SetOutputRate(int32_t uiRate);
int32_t WIT_SetContent(int32_t uiRsw);
int32_t WIT_SetCanBaud(int32_t uiBaudIndex);
int32_t WIT_SaveParameter(void);
int32_t WIT_SetForReset(void);
int32_t WIT_CaliRefAngle(void);

extern int16_t WIT_Reg[WIT_REGSIZE];

#ifdef __cplusplus
}
#endif

#endif
