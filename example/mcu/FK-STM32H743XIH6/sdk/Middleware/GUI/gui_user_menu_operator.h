#ifndef __GUI_USER_MENU_OPERATOR_H
#define __GUI_USER_MENU_OPERATOR_H

#ifdef __cplusplus
extern "C" {
#endif

/* 设置使用的中间变量 */
float s_SpeedX, s_SpeedT;

#include "car.h"
#include "imu.h"
#include "cmd.h"
#include "gui.h"
#include "store.h"

static void SetSpeedOperator(GUI_BaseItem_T *sender)
{
    CarControl_SetSpeed(&g_Car.remote, s_SpeedX, s_SpeedT);
}

static void SavePIDLocationOperator(GUI_BaseItem_T *sender)
{
    Param_Save(PARAM_SAVE_LOC_X_PID_DATA | PARAM_SAVE_LOC_Y_PID_DATA);
}

static void LoadPIDLocationOperator(GUI_BaseItem_T *sender)
{
    Param_LoadDefault(PARAM_SAVE_LOC_X_PID_DATA | PARAM_SAVE_LOC_Y_PID_DATA);
}

static void LoadPIDYawOperator(GUI_BaseItem_T *sender)
{
    Param_LoadDefault(PARAM_SAVE_YAW_PID_DATA);
}

static void SavePIDYawOperator(GUI_BaseItem_T *sender)
{
    Param_Save(PARAM_SAVE_YAW_PID_DATA);
}

static void LoadPIDMotorOperator(GUI_BaseItem_T *sender)
{
    Param_LoadDefault(PARAM_SAVE_MOTOR_PID_DATA);
}

static void SavePIDMotorOperator(GUI_BaseItem_T *sender)
{
    Param_Save(PARAM_SAVE_MOTOR_PID_DATA);
}

static void LoadCarControlOperator(GUI_BaseItem_T *sender)
{
    Param_LoadDefault(PARAM_SAVE_CAR_CTRL_DATA);
}

static void SaveCarControlOperator(GUI_BaseItem_T *sender)
{
    Param_Save(PARAM_SAVE_CAR_CTRL_DATA);
}

static void LoadSlipDetectOperator(GUI_BaseItem_T *sender)
{
    Param_LoadDefault(PARAM_SAVE_SLIP_DET_DATA);
}

static void SaveSlipDetectOperator(GUI_BaseItem_T *sender)
{
    Param_Save(PARAM_SAVE_SLIP_DET_DATA);
}

static void LoadMotorMapperOperator(GUI_BaseItem_T *sender)
{
    Param_LoadDefault(PARAM_SAVE_MOTOR_MAPPER_DATA);
}

static void SaveMotorMapperOperator(GUI_BaseItem_T *sender)
{
    Param_Save(PARAM_SAVE_MOTOR_MAPPER_DATA);
}

static void LoadCommandOperator(GUI_BaseItem_T *sender)
{
    Param_LoadDefault(PARAM_SAVE_COMMAND_DATA);
}

static void SaveCommandOperator(GUI_BaseItem_T *sender)
{
    Param_Save(PARAM_SAVE_COMMAND_DATA);
}

static void LoadAvoidanceOperator(GUI_BaseItem_T *sender)
{
    Param_LoadDefault(PARAM_SAVE_AVOIDANCE_DATA);
}

static void SaveAvoidanceOperator(GUI_BaseItem_T *sender)
{
    Param_Save(PARAM_SAVE_AVOIDANCE_DATA);
}

static void LoadSystemOperator(GUI_BaseItem_T *sender)
{
    Param_LoadDefault(PARAM_SAVE_AVOIDANCE_DATA);
}

static void SaveSystemOperator(GUI_BaseItem_T *sender)
{
    Param_Save(PARAM_SAVE_AVOIDANCE_DATA);
}

static void UseNewUIOperator(GUI_BaseItem_T *sender)
{
    g_ParamData.gui_useNewUI = 1;
    Param_Save(PARAM_SAVE_GUI_DATA);
    NVIC_SystemReset();
}

static void USB_FileOperator(GUI_BaseItem_T *sender)
{
    RTC_WriteBKP(0x12341234);
    NVIC_SystemReset();
}

static void MapperMotorOperator(GUI_BaseItem_T *sender)
{
    /* 寻找对应电机，并给一定初速度方便调试 */
    uint8_t index = sender->title[5] - '1' + CAR_INDEX_M1;
    g_Car.stopMotorSpeed[index] = 200;
}

static void BaudRateSetOperator(GUI_BaseItem_T *sender)
{
    uint8_t comX = sender->baseMenu->title[7] - '1' + COM1;
    UART_SetBaudRate(comX, g_ParamData.uart_baudRate[comX]);
}

static void ParitySetOperator(GUI_BaseItem_T *sender)
{
    uint8_t comX = sender->baseMenu->title[7] - '1' + COM1;
    UART_SetParity(comX, g_ParamData.uart_parity[comX]);
}

static void StopBitsSetOperator(GUI_BaseItem_T *sender)
{
    uint8_t comX = sender->baseMenu->title[7] - '1' + COM1;
    UART_SetStopBits(comX, g_ParamData.uart_stopBits[comX]);
}

static void MapperEncoderOperator(GUI_BaseItem_T *sender)
{
    Encoder_Clear();
}

static void ResetSystemOperator(GUI_BaseItem_T *sender)
{
    HAL_NVIC_SystemReset();
}

static void StartSetTimeOperator(GUI_BaseItem_T *sender)
{
    PWR_RecordTime();
    g_GUI_User.rtcSetFlag = 1;
}

static void StopSetTimeOperator(GUI_BaseItem_T *sender)
{
    PWR_RecordTime();
    RTC_SetTime();
    g_GUI_User.rtcSetFlag = 0;
}

static void SetSecondOperator(GUI_BaseItem_T *sender)
{
    PWR_RecordTime();
    g_GUI_User.rtcSetFlag = 1;
    g_RTC_Data.second = 0;
    RTC_SetTime();
    g_GUI_User.rtcSetFlag = 0;
}

static void ImuResetOrientationOperator(GUI_BaseItem_T *sender)
{
    IMU_ResetOrientation();
}

static void StartShowLidarOperator(GUI_BaseItem_T *sender)
{
    /* 绘图设置 */
    g_GUI_User.device.menuPainter.Height = 180;
    g_GUI_User.device.menuPainter.Width = 240;
    g_GUI_User.device.menuPainter.StartY = 140;
    g_GUI_User.device.updateAllFlag = TRUE;

    /* 绘图设置 */
    s_polarGraph.StartX = 0;
    s_polarGraph.StartY = 0;
    s_polarGraph.Width = 240;
    s_polarGraph.Height = 140;

    g_GUI_User.showPlotIndex = 6; /* 激光雷达点云图像 */

    /* 设置环 */
    float ringRadii[] = { 0.25f, 0.5f, 0.75f, 1.0f };
    uint16_t ringColors[] = { GUI_GRAY, GUI_GRAY, GUI_GRAY, GUI_WHITE };
    const char *ringLabels[] = { "0.25", "0.5", "0.75", "1.0" };

    GUI_PolarGraphSetRings(&s_polarGraph, ringRadii, ringColors, ringLabels, 4);

    /* 初始化电机图表缓冲区 */
    GUI_PolarGraphInit(&s_polarGraph);

    /* 清空曲线区域 */
    GUI_LCD_FillColor(0, 0, g_LCD.Width, g_LCD.Height, GUI_BLACK);

    /* 设置为避障模式 */
    Car_SetMode(CAR_MODE_AVOIDANCE);

    /* 在图像区域显示提示信息 */
    GUI_LCD_ShowString(0, 0, "正在初始化激光雷达点云...", GUI_WHITE, GUI_BLACK);
    g_GUI_User.dragonClicked = 0;
}

static void StopShowLidarOperator(GUI_BaseItem_T *sender)
{
    /* 反初始化电机图表缓冲区 */
    GUI_PolarGraphDeinit(&s_polarGraph);

    /* 清空整个屏幕 */
    GUI_LCD_FillColor(0, 0, g_LCD.Width, g_LCD.Height, GUI_BLACK);

    /* 设置为停止模式 */
    Car_SetMode(CAR_MODE_STOP);

    /* 恢复设置 */
    g_GUI_User.device.menuPainter.Height = 320;
    g_GUI_User.device.menuPainter.StartY = 0;
    g_GUI_User.device.updateAllFlag = TRUE;
    g_GUI_User.showPlotIndex = 0;
    g_GUI_User.dragonClicked = 1;
}

void StartRemoteDebugOperator(GUI_BaseItem_T *sender)
{
    /* 设置小车状态 */
    Car_SetMode(CAR_MODE_REMOTE);

    /* 绘图设置 */
    g_GUI_User.device.menuPainter.Height = 180;
    g_GUI_User.device.menuPainter.Width = 240;
    g_GUI_User.device.menuPainter.StartY = 140;
    g_GUI_User.device.updateAllFlag = TRUE;

    /* 绘图设置 */
    g_CarGraph.StartX = 0;
    g_CarGraph.StartY = 0;
    g_CarGraph.Width = 240;
    g_CarGraph.Height = 140;

    /* 绘图设置 */
    g_GUI_User.showPlotIndex = 4; /* 小车遥控控制图像 */

    /* 初始化电机图表缓冲区 */
    GUI_CoordinateGraphInit(&g_CarGraph);

    /* 清空曲线区域（菜单下方120像素区域） */
    GUI_LCD_FillColor(0, 0, g_LCD.Width, g_LCD.Height, GUI_BLACK);

    /* 在图像区域显示提示信息 */
    GUI_LCD_ShowString(0, 0, "正在初始化小车遥控曲线...", GUI_WHITE, GUI_BLACK);
    g_GUI_User.dragonClicked = 0;
}

void StopRemoteDebugOperator(GUI_BaseItem_T *sender)
{
    /* 反初始化电机图表缓冲区 */
    GUI_CoordinateGraphDeinit(&g_CarGraph);

    /* 清空整个屏幕 */
    GUI_LCD_FillColor(0, 0, g_LCD.Width, g_LCD.Height, GUI_BLACK);

    /* 设置小车状态 */
    Car_SetMode(CAR_MODE_STOP);

    /* 恢复设置 */
    g_GUI_User.device.menuPainter.Height = 320;
    g_GUI_User.device.menuPainter.StartY = 0;
    g_GUI_User.device.updateAllFlag = TRUE;
    g_GUI_User.showPlotIndex = 0;
    g_GUI_User.dragonClicked = 1;
}

static void StartShowMapperPlotDebugOperator(GUI_BaseItem_T *sender)
{
    /* 绘图设置 */
    g_MotorMapperGraph.StartX = 0;
    g_MotorMapperGraph.StartY = 0;
    g_MotorMapperGraph.Width = 240;
    g_MotorMapperGraph.Height = 140;

    g_GUI_User.showPlotIndex = 5; /* 电机图像 */
    g_Car.readMotorEncoderFlag = 1;

    /* 设置小车状态 */
    Car_SetMode(CAR_MODE_STOP);

    /* 初始化电机图表缓冲区 */
    GUI_CoordinateGraphDeinit(&g_CarGraph);
    GUI_CoordinateGraphInit(&g_MotorMapperGraph);

    /* 清空曲线区域（菜单下方120像素区域） */
    GUI_LCD_FillColor(0, 0, g_LCD.Width, g_LCD.Height, GUI_BLACK);

    /* 在图像区域显示提示信息 */
    GUI_LCD_ShowString(0, 0, "正在初始化映射曲线...", GUI_WHITE, GUI_BLACK);
}

static void StopShowMapperPlotDebugOperator(GUI_BaseItem_T *sender)
{
    /* 反初始化电机图表缓冲区 */
    GUI_CoordinateGraphDeinit(&g_MotorMapperGraph);
    GUI_CoordinateGraphInit(&g_CarGraph);

    /* 设置小车状态 */
    Car_SetMode(CAR_MODE_REMOTE);

    /* 绘图设置 */
    g_CarGraph.StartX = 0;
    g_CarGraph.StartY = 0;
    g_CarGraph.Width = 240;
    g_CarGraph.Height = 140;

    g_GUI_User.showPlotIndex = 4; /* 电机图像 */
    g_Car.readMotorEncoderFlag = 0;

    /* 清空速度 */
    for (uint8_t i = 0; i < CAR_MAX_INDEX; i++)
    {
        g_Car.stopMotorSpeed[i] = 0;
    }

    /* 清空整个屏幕 */
    GUI_LCD_FillColor(0, 0, g_LCD.Width, g_LCD.Height, GUI_BLACK);
}

static void StartShowSilpPlotDebugOperator(GUI_BaseItem_T *sender)
{
    /* 绘图设置 */
    g_SlipGraph.StartX = 0;
    g_SlipGraph.StartY = 0;
    g_SlipGraph.Width = 240;
    g_SlipGraph.Height = 140;

    g_GUI_User.showPlotIndex = 1; /* 打滑度图像 */
    g_Car.readMotorEncoderFlag = 1;

    /* 初始化电机图表缓冲区 */
    GUI_CoordinateGraphDeinit(&g_CarGraph);
    GUI_CoordinateGraphInit(&g_SlipGraph);

    /* 清空曲线区域（菜单下方120像素区域） */
    GUI_LCD_FillColor(0, 0, g_LCD.Width, g_LCD.Height, GUI_BLACK);

    /* 在图像区域显示提示信息 */
    GUI_LCD_ShowString(0, 0, "正在初始化打滑度曲线...", GUI_WHITE, GUI_BLACK);
}

static void StopShowSilpPlotDebugOperator(GUI_BaseItem_T *sender)
{
    /* 反初始化电机图表缓冲区 */
    GUI_CoordinateGraphDeinit(&g_SlipGraph);
    GUI_CoordinateGraphInit(&g_CarGraph);

    /* 绘图设置 */
    g_CarGraph.StartX = 0;
    g_CarGraph.StartY = 0;
    g_CarGraph.Width = 240;
    g_CarGraph.Height = 140;
    g_GUI_User.showPlotIndex = 4; /* 电机图像 */
    g_Car.readMotorEncoderFlag = 0;

    /* 清空速度 */
    for (uint8_t i = 0; i < CAR_MAX_INDEX; i++)
    {
        g_Car.stopMotorSpeed[i] = 0;
    }

    /* 清空整个屏幕 */
    GUI_LCD_FillColor(0, 0, g_LCD.Width, g_LCD.Height, GUI_BLACK);
}

static void StartImuDebugOperator(GUI_BaseItem_T *sender)
{
    /* 绘图设置 */
    g_GUI_User.device.menuPainter.Height = 180;
    g_GUI_User.device.menuPainter.Width = 240;
    g_GUI_User.device.menuPainter.StartY = 140;
    g_GUI_User.device.updateAllFlag = TRUE;

    /* 绘图设置 */
    g_ImuGraph.StartX = 0;
    g_ImuGraph.StartY = 0;
    g_ImuGraph.Width = 240;
    g_ImuGraph.Height = 140;

    g_GUI_User.showPlotIndex = 2; /* IMU图像 */

    /* 初始化电机图表缓冲区 */
    GUI_CoordinateGraphInit(&g_ImuGraph);

    /* 清空曲线区域（菜单下方120像素区域） */
    GUI_LCD_FillColor(0, 0, g_LCD.Width, g_LCD.Height, GUI_BLACK);

    /* 在图像区域显示提示信息 */
    GUI_LCD_ShowString(0, 0, "正在初始化陀螺仪曲线...", GUI_WHITE, GUI_BLACK);
    g_GUI_User.dragonClicked = 0;
}

static void StopImuDebugOperator(GUI_BaseItem_T *sender)
{
    /* 恢复设置 */
    g_GUI_User.device.menuPainter.Height = 320;
    g_GUI_User.device.menuPainter.StartY = 0;
    g_GUI_User.device.updateAllFlag = TRUE;
    g_GUI_User.showPlotIndex = 0;

    /* 反初始化电机图表缓冲区 */
    GUI_CoordinateGraphDeinit(&g_ImuGraph);

    /* 清空整个屏幕 */
    GUI_LCD_FillColor(0, 0, g_LCD.Width, g_LCD.Height, GUI_BLACK);
    g_GUI_User.dragonClicked = 1;
}

static void StartDetailOperator(GUI_BaseItem_T *sender)
{
    g_GUI_User.showPlotIndex = 3; /* 详情图像 */

    /* 初始化电机图表缓冲区 */
    GUI_CoordinateGraphInit(&g_DetailGraph);

    /* 清空曲线区域（菜单下方120像素区域） */
    GUI_LCD_FillColor(0, 0, g_LCD.Width, g_LCD.Height, GUI_BLACK);

    /* 在图像区域显示提示信息 */
    GUI_LCD_ShowString(0, 0, "正在初始化电机曲线...", GUI_WHITE, GUI_BLACK);
    g_GUI_User.dragonClicked = 0;
}

static void StopDetailOperator(GUI_BaseItem_T *sender)
{
    /* 恢复设置 */
    g_GUI_User.device.menuPainter.Height = 320;
    g_GUI_User.device.menuPainter.StartY = 0;
    g_GUI_User.device.updateAllFlag = TRUE;
    g_GUI_User.showPlotIndex = 0;

    /* 反初始化电机图表缓冲区 */
    GUI_CoordinateGraphDeinit(&g_DetailGraph);

    /* 清空整个屏幕 */
    GUI_LCD_FillColor(0, 0, g_LCD.Width, g_LCD.Height, GUI_BLACK);
    g_GUI_User.dragonClicked = 1;
}

#ifdef __cplusplus
}
#endif

#endif
