#ifndef __GUI_USER_MENU_VALUE_H
#define __GUI_USER_MENU_VALUE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "gui.h"
#include "car.h"
#include "imu.h"

static const GUI_ModeItemElement_T s_ChassisMode[] = {
    { "  两轮差速底盘(M1-M2)", CHASSIS_TYPE_DIFFERENTIAL_2 },
    { "  四轮差速底盘(M1-M4)", CHASSIS_TYPE_DIFFERENTIAL_4 },
    { NULL, 0 }
};

static const GUI_ModeItemElement_T s_MotorMapperMode[] = {
    { " 不驱动任何电机", CAR_MAPPER_NONE },
    { "电机通道1(正向)", CAR_MAPPER_MA_POS },
    { "电机通道1(反向)", CAR_MAPPER_MA_NEG },
    { "电机通道2(正向)", CAR_MAPPER_MB_POS },
    { "电机通道2(反向)", CAR_MAPPER_MB_NEG },
    { "电机通道3(正向)", CAR_MAPPER_MC_POS },
    { "电机通道3(反向)", CAR_MAPPER_MC_NEG },
    { "电机通道4(正向)", CAR_MAPPER_MD_POS },
    { "电机通道4(反向)", CAR_MAPPER_MD_NEG },
    { NULL, 0 }
};

static const GUI_ModeItemElement_T s_EncoderMapperMode[] = {
    { " 不选择任何编码器", CAR_MAPPER_NONE },
    { "编码器通道1(正向)", CAR_MAPPER_MA_POS },
    { "编码器通道1(反向)", CAR_MAPPER_MA_NEG },
    { "编码器通道2(正向)", CAR_MAPPER_MB_POS },
    { "编码器通道2(反向)", CAR_MAPPER_MB_NEG },
    { "编码器通道3(正向)", CAR_MAPPER_MC_POS },
    { "编码器通道3(反向)", CAR_MAPPER_MC_NEG },
    { "编码器通道4(正向)", CAR_MAPPER_MD_POS },
    { "编码器通道4(反向)", CAR_MAPPER_MD_NEG },
    { NULL, 0 }
};

static const GUI_ModeItemElement_T s_MotorControlMode[] = {
    { "单极性控制(无使能)", MOTOR_MODE_UNIPOLAR_NO_EN },
    { "单极性控制(带使能)", MOTOR_MODE_UNIPOLAR_WITH_EN },
    { " 双输入控制/TB6612", MOTOR_MODE_DUAL_INPUT },
    { "        双极性控制", MOTOR_MODE_BIPOLAR },
    { NULL, 0 }
};

static const GUI_ModeItemElement_T s_UartParity[] = {
    { "无校验", UART_PARITY_NONE },
    { "奇校验", UART_PARITY_EVEN },
    { "偶校验", UART_PARITY_ODD },
    { NULL, 0 }
};

static const GUI_ModeItemElement_T s_UartStopBits[] = {
    { "0.5位", UART_STOPBITS_0_5 },
    { "  1位", UART_STOPBITS_1 },
    { "1.5位", UART_STOPBITS_1_5 },
    { "  2位", UART_STOPBITS_2 },
    { NULL, 0 }
};

static const GUI_ModeItemElement_T s_UartBaudRate[] = {
    { "  4800", 4800 },
    { "  9600", 9600 },
    { " 19200", 19200 },
    { " 38400", 38400 },
    { " 57600", 57600 },
    { "115200", 115200 },
    { "230400", 230400 },
    { "460800", 460800 },
    { "921600", 921600 },
    { NULL, 0 }
};

static const GUI_ModeItemElement_T s_LidarComNum[] = {
    { "COM2", COM2 },
    { "COM3", COM3 },
    { "禁用", COM_COUNT },
    { NULL, 0 }
};

static const GUI_ModeItemElement_T s_PwrTimeMode[] = {
    { "  30秒", 30 },
    { " 1分钟", 60 },
    { " 2分钟", 120 },
    { " 5分钟", 300 },
    { "10分钟", 600 },
    { "30分钟", 1800 },
    { " 1小时", 3600 },
    { "不待机", 0xFFFFFFFF },
    { NULL, 0 }
};

#ifdef __cplusplus
}
#endif

#endif
