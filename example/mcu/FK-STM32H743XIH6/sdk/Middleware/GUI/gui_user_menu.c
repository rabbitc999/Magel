#include "gui.h"

#include "car.h"
#include "cmd.h"
#include "imu.h"
#include "store.h"
#include "gui_user_menu_value.h"
#include "gui_user_menu_operator.h"

uint16_t Main_Run(GUI_FuncItem_T *sender, GUI_MenuParam_T *param);
uint16_t ConfigManager_Function(GUI_FuncItem_T *sender, GUI_MenuParam_T *param);
uint16_t FileBrowser_Function(GUI_FuncItem_T *sender, GUI_MenuParam_T *param);
uint16_t EncoderView_Function(GUI_FuncItem_T *sender, GUI_MenuParam_T *param);

extern float g_BatteryVoltage;

/**
 * @brief 菜单初始化函数
 */
void GUI_MenuInit(void)
{
    /* 局部变量定义 */
    GUI_Menu_T *root, *imuSet, *remote, *remoteYaw, *remoteSlip, *motorMapper, *motorSpeed, *otherSetting, *author, *uart, *uartCom[COM_COUNT], *avoidanceSetting;
    GUI_Device_T *device = &g_GUI_User.device;

    /* 设备初始化 */
    GUI_MenuDeviceInit(device, Main_Run);

    /* 初始化菜单 */
    root = GUI_CreateMenu(device, "主菜单");
    imuSet = GUI_CreateMenu(device, "陀螺仪调试");
    remote = GUI_CreateMenu(device, "小车控制调试");
    remoteSlip = GUI_CreateMenu(device, "打滑检测");
    motorMapper = GUI_CreateMenu(device, "映射设置");
    motorSpeed = GUI_CreateMenu(device, "速度环PID调试");
    remoteYaw = GUI_CreateMenu(device, "偏航角PID调试");
    otherSetting = GUI_CreateMenu(device, "其他设置");
    uart = GUI_CreateMenu(device, "串口设置");
    uartCom[COM2] = GUI_CreateMenu(device, "串口COM2设置");
    uartCom[COM4] = GUI_CreateMenu(device, "串口COM4设置");
    uartCom[COM6] = GUI_CreateMenu(device, "串口COM6设置");
    uartCom[COM7] = GUI_CreateMenu(device, "串口COM7设置");
    avoidanceSetting = GUI_CreateMenu(device, "自动避障设置");
    author = GUI_CreateMenu(device, "作者信息");

    /* 赋值根菜单 */
    g_GUI_User.rootMenu = root;

    /* 主菜单 */
    GUI_CreateTextItem(device, root, "基本调试");
    GUI_CreateJumpItemF(device, root, "小车控制调试", remote, StartRemoteDebugOperator, StopRemoteDebugOperator);
    GUI_CreateJumpItemF(device, root, "自动避障设置", avoidanceSetting, StartShowLidarOperator, StopShowLidarOperator);
    GUI_CreateJumpItemF(device, root, "陀螺仪调试", imuSet, StartImuDebugOperator, StopImuDebugOperator);
    GUI_CreateTextItem(device, root, "基本设置");
    GUI_CreateJumpItem(device, root, "串口设置", uart);
    GUI_CreateJumpItem(device, root, "其他设置", otherSetting);
    GUI_CreateTextItem(device, root, "其他功能");
    if (!g_LCD.TftFlag) { GUI_CreateFuncItem(device, root, "使用新UI界面", UseNewUIOperator); }
    GUI_CreateFuncItem(device, root, "USB文件传输模式", USB_FileOperator);
    GUI_CreatePageItem(device, root, "配置文件管理", ConfigManager_Function);
    GUI_CreatePageItem(device, root, "文件浏览器", FileBrowser_Function);
    GUI_CreateFuncItem(device, root, "系统重启", ResetSystemOperator);
    GUI_CreateJumpItemF(device, root, "作者信息", author, StartDetailOperator, StopDetailOperator);

    /* 陀螺仪调试 */
    GUI_CreateTextItem(device, imuSet, "校准操作");
    GUI_CreateFuncItem(device, imuSet, "陀螺仪姿态重置", ImuResetOrientationOperator);
    GUI_CreateTextItem(device, imuSet, "欧拉角解算");
    GUI_CreateFloatItemR(device, imuSet, "俯仰角(deg)", &g_IMU.Pitch);
    GUI_CreateFloatItemR(device, imuSet, "横滚角(deg)", &g_IMU.Roll);
    GUI_CreateFloatItemR(device, imuSet, "偏航角(deg)", &g_IMU.Yaw);
    GUI_CreateTextItem(device, imuSet, "陀螺仪数据显示");
    GUI_CreateFloatItemR(device, imuSet, "X轴角速度(deg/s)", &g_IMU.GyroX);
    GUI_CreateFloatItemR(device, imuSet, "Y轴角速度(deg/s)", &g_IMU.GyroY);
    GUI_CreateFloatItemR(device, imuSet, "Z轴角速度(deg/s)", &g_IMU.GyroZ);
    GUI_CreateTextItem(device, imuSet, "加速度计数据显示");
    GUI_CreateFloatItemR(device, imuSet, "X轴加速度(g)", &g_IMU.AccelX);
    GUI_CreateFloatItemR(device, imuSet, "Y轴加速度(g)", &g_IMU.AccelY);
    GUI_CreateFloatItemR(device, imuSet, "Z轴加速度(g)", &g_IMU.AccelZ);

    /* 小车控制调试 */
    GUI_CreateTextItem(device, remote, "图像说明");
    GUI_CreateFuncItem(device, remote, "T:表示目标值", NULL);
    GUI_CreateFuncItem(device, remote, "A:表示解算后的实际值", NULL);
    GUI_CreateTextItem(device, remote, "基础设置");
    GUI_CreateJumpItemF(device, remote, "电机/编码器映射设置", motorMapper, StartShowMapperPlotDebugOperator, StopShowMapperPlotDebugOperator);
    GUI_CreateJumpItemF(device, remote, "打滑检测设置", remoteSlip, StartShowSilpPlotDebugOperator, StopShowSilpPlotDebugOperator);
    GUI_CreateTextItem(device, remote, "小车底盘类型选择");
    GUI_CreateModeItem8(device, remote, "类型", s_ChassisMode, (uint8_t *)&g_ParamData.car_chassisType);
    GUI_CreateFuncItem(device, remote, "括号内为底盘对应的电机索引", NULL);
    GUI_CreateTextItem(device, remote, "小车基础数据");
    GUI_CreateFloatItem(device, remote, "轮距(cm)", &g_Car.remote.wheelBase, 0, 360);
    GUI_CreateFloatItem(device, remote, "轮径(cm)", &g_Car.remote.wheelDiameter, 0, 120);
    GUI_CreateUintItem32(device, remote, "单圈编码器数值增量", &g_Car.remote.encoderPerRev, 0, 10000);
    GUI_CreatePageItem(device, remote, "电机编码器数值查看", EncoderView_Function);
    GUI_CreateTextItem(device, remote, "小车控制速度设置");
    GUI_CreateFloatItem(device, remote, "最大前进速度(m/s)", &g_Car.remote.remoteMaxSpeedX, 0, 1.0);
    GUI_CreateFloatItem(device, remote, "最大旋转速度(deg/s)", &g_Car.remote.remoteMaxSpeedT, 0, 300);
    GUI_CreateTextItem(device, remote, "控制器PID调试");
    GUI_CreateJumpItem(device, remote, "速度环PID调试", motorSpeed);
    GUI_CreateJumpItem(device, remote, "偏航角PID调试", remoteYaw);
    GUI_CreateTextItem(device, remote, "电机PID附加调试");
    GUI_CreateFloatItem(device, remote, "输入死区(百分比)", &g_Car.remote.inputDeadZone, 0, 100);
    GUI_CreateTextItem(device, remote, "保存与恢复设置");
    GUI_CreateFuncItem(device, remote, "保存当前设置", SaveCarControlOperator);
    GUI_CreateFuncItem(device, remote, "恢复默认设置", LoadCarControlOperator);

    GUI_CreateTextItem(device, remoteYaw, "小车控制设置");
    GUI_CreateFloatItemF(device, remoteYaw, "前进速度(m/s)", &s_SpeedX, -1.0f, 1.0f, NULL, SetSpeedOperator);
    GUI_CreateFloatItemF(device, remoteYaw, "旋转速度(deg/s)", &s_SpeedT, -360, 360, NULL, SetSpeedOperator);
    GUI_CreateTextItem(device, remoteYaw, "角度环PID设置");
    GUI_CreateFloatItem(device, remoteYaw, "比例系数Kp", &g_Car.remote.pidYaw.Kp, 0, 1000);
    GUI_CreateFloatItem(device, remoteYaw, "积分系数Ki", &g_Car.remote.pidYaw.Ki, 0, 1000);
    GUI_CreateFloatItem(device, remoteYaw, "微分系数Kd", &g_Car.remote.pidYaw.Kd, 0, 1000);
    GUI_CreateFloatItem(device, remoteYaw, "输出限幅", &g_Car.remote.pidYaw.OutputLimit, 0, 360);
    GUI_CreateFloatItem(device, remoteYaw, "积分限幅", &g_Car.remote.pidYaw.IntegralLimit, 0, 10000);
    GUI_CreateTextItem(device, remote, "保存与恢复设置");
    GUI_CreateFuncItem(device, remote, "保存当前设置", SavePIDYawOperator);
    GUI_CreateFuncItem(device, remote, "恢复默认设置", LoadPIDYawOperator);

    GUI_CreateTextItem(device, remoteSlip, "小车控制设置");
    GUI_CreateFloatItemF(device, remoteSlip, "前进速度(m/s)", &s_SpeedX, -0.5f, 0.5f, NULL, SetSpeedOperator);
    GUI_CreateFloatItemF(device, remoteSlip, "旋转速度(deg/s)", &s_SpeedT, -360, 360, NULL, SetSpeedOperator);
    GUI_CreateTextItem(device, remoteSlip, "系数设置");
    GUI_CreateFloatItem(device, remoteSlip, "固定阈值Theta(x100)", &g_Car.remote.slipTheta, 0, 1);
    GUI_CreateFloatItem(device, remoteSlip, "轮子打滑系数K1(x100)", &g_Car.remote.slipK1, 0, 2);
    GUI_CreateFloatItem(device, remoteSlip, "轮间打滑系数K2(x100)", &g_Car.remote.slipK2, 0, 2);
    GUI_CreateTextItem(device, remoteSlip, "保存与恢复设置");
    GUI_CreateFuncItem(device, remoteSlip, "保存当前设置", SaveSlipDetectOperator);
    GUI_CreateFuncItem(device, remoteSlip, "恢复默认设置", LoadSlipDetectOperator);

    GUI_CreateTextItem(device, motorSpeed, "小车控制设置");
    GUI_CreateFloatItem(device, motorSpeed, "前进速度(m/s)", &g_Car.remote.targetSpeedX, -1.0f, 1.0f);
    GUI_CreateFloatItem(device, motorSpeed, "旋转速度(deg/s)", &g_Car.remote.targetSpeedT, -360, 360);
    GUI_CreateTextItem(device, motorSpeed, "电机速度环PID设置");
    GUI_CreateFloatItem(device, motorSpeed, "比例系数Kp", &g_Car.remote.pidMotorParam.Kp, 0, 1000);
    GUI_CreateFloatItem(device, motorSpeed, "积分系数Ki", &g_Car.remote.pidMotorParam.Ki, 0, 1000);
    GUI_CreateFloatItem(device, motorSpeed, "微分系数Kd", &g_Car.remote.pidMotorParam.Kd, 0, 1000);
    GUI_CreateFloatItem(device, motorSpeed, "输出限幅", &g_Car.remote.pidMotorParam.OutputLimit, 0, 1000);
    GUI_CreateFloatItem(device, motorSpeed, "积分限幅", &g_Car.remote.pidMotorParam.IntegralLimit, 0, 10000);
    GUI_CreateTextItem(device, motorSpeed, "保存与恢复设置");
    GUI_CreateFuncItem(device, motorSpeed, "保存当前设置", SavePIDMotorOperator);
    GUI_CreateFuncItem(device, motorSpeed, "恢复默认设置", LoadPIDMotorOperator);

    GUI_CreateTextItem(device, motorMapper, "图像说明");
    GUI_CreateFuncItem(device, motorMapper, "O:表示对应电机PWM输出", NULL);
    GUI_CreateFuncItem(device, motorMapper, "E:表示对应电机编码器输入", NULL);
    GUI_CreateTextItem(device, motorMapper, "控制设置");
    GUI_CreateModeItem8(device, motorMapper, "电机控制模式", s_MotorControlMode, (uint8_t *)&g_ParamData.motor_controlMode);
    GUI_CreateTextItem(device, motorMapper, "动力设置");
    GUI_CreateIntItem16(device, motorMapper, "电机M1", &g_Car.stopMotorSpeed[CAR_INDEX_M1], -1000, 1000);
    GUI_CreateIntItem16(device, motorMapper, "电机M2", &g_Car.stopMotorSpeed[CAR_INDEX_M2], -1000, 1000);
    GUI_CreateIntItem16(device, motorMapper, "电机M3", &g_Car.stopMotorSpeed[CAR_INDEX_M3], -1000, 1000);
    GUI_CreateIntItem16(device, motorMapper, "电机M4", &g_Car.stopMotorSpeed[CAR_INDEX_M4], -1000, 1000);
    GUI_CreateTextItem(device, motorMapper, "电机映射设置");
    GUI_CreateModeItem8F(device, motorMapper, "电机M1", s_MotorMapperMode, (uint8_t *)&g_ParamData.car_motorMapper[CAR_INDEX_M1], MapperMotorOperator);
    GUI_CreateModeItem8F(device, motorMapper, "电机M2", s_MotorMapperMode, (uint8_t *)&g_ParamData.car_motorMapper[CAR_INDEX_M2], MapperMotorOperator);
    GUI_CreateModeItem8F(device, motorMapper, "电机M3", s_MotorMapperMode, (uint8_t *)&g_ParamData.car_motorMapper[CAR_INDEX_M3], MapperMotorOperator);
    GUI_CreateModeItem8F(device, motorMapper, "电机M4", s_MotorMapperMode, (uint8_t *)&g_ParamData.car_motorMapper[CAR_INDEX_M4], MapperMotorOperator);
    GUI_CreateTextItem(device, motorMapper, "对应编码器映射设置");
    GUI_CreateModeItem8F(device, motorMapper, "电机M1", s_EncoderMapperMode, (uint8_t *)&g_ParamData.car_encoderMapper[CAR_INDEX_M1], MapperEncoderOperator);
    GUI_CreateModeItem8F(device, motorMapper, "电机M2", s_EncoderMapperMode, (uint8_t *)&g_ParamData.car_encoderMapper[CAR_INDEX_M2], MapperEncoderOperator);
    GUI_CreateModeItem8F(device, motorMapper, "电机M3", s_EncoderMapperMode, (uint8_t *)&g_ParamData.car_encoderMapper[CAR_INDEX_M3], MapperEncoderOperator);
    GUI_CreateModeItem8F(device, motorMapper, "电机M4", s_EncoderMapperMode, (uint8_t *)&g_ParamData.car_encoderMapper[CAR_INDEX_M4], MapperEncoderOperator);
    GUI_CreateTextItem(device, motorMapper, "保存与恢复设置");
    GUI_CreateFuncItem(device, motorMapper, "保存当前设置", SaveMotorMapperOperator);
    GUI_CreateFuncItem(device, motorMapper, "恢复默认设置", LoadMotorMapperOperator);

    /* 避障模式设置 */
    GUI_CreateTextItem(device, avoidanceSetting, "运动方向设置");
    GUI_CreateFloatItem(device, avoidanceSetting, "目标X方向", &g_Car.avoidance.ctrlSpeedX, 0, 1);
    GUI_CreateFloatItem(device, avoidanceSetting, "目标Y方向", &g_Car.avoidance.ctrlSpeedT, 0, 1);
    GUI_CreateTextItem(device, avoidanceSetting, "小车控制设置");
    GUI_CreateFloatItemF(device, avoidanceSetting, "前进速度(m/s)", &s_SpeedX, -1.0f, 1.0f, NULL, SetSpeedOperator);
    GUI_CreateFloatItemF(device, avoidanceSetting, "转向速度(deg/s)", &s_SpeedT, -360, 360, NULL, SetSpeedOperator);
    GUI_CreateTextItem(device, avoidanceSetting, "激光雷达基础设置");
    GUI_CreateFloatItem(device, avoidanceSetting, "前后角度切换阈值(deg)", &g_LidarState.frontBackAngleCheckThreshold, 0, 90);
    GUI_CreateFloatItem(device, avoidanceSetting, "左右角度切换阈值(deg)", &g_LidarState.sideAngleCheckThreshold, 0, 90);
    GUI_CreateTextItem(device, avoidanceSetting, "避障模式基础设置");
    GUI_CreateFloatItem(device, avoidanceSetting, "前方避障角度(deg)", &g_Car.avoidance.frontAngleRange, 0, 85);
    GUI_CreateFloatItem(device, avoidanceSetting, "避障距离阈值(mm)", &g_Car.avoidance.obstacleDistanceThreshold, 0, 1000);
    GUI_CreateFloatItem(device, avoidanceSetting, "超声波避障阈值(cm)", &g_Car.avoidance.ultraSonicThreshold, 0, 1000);
    GUI_CreateFloatItem(device, avoidanceSetting, "转向速度增益", &g_Car.avoidance.ctrlSpeedTGain, 0, 10);
    GUI_CreateTextItem(device, avoidanceSetting, "保存与恢复设置");
    GUI_CreateFuncItem(device, avoidanceSetting, "保存当前设置", SaveAvoidanceOperator);
    GUI_CreateFuncItem(device, avoidanceSetting, "恢复默认设置", LoadAvoidanceOperator);

    /* 其他设置 */
    GUI_CreateTextItem(device, otherSetting, "蜂鸣器设置");
    GUI_CreateCheckItem(device, otherSetting, "蜂鸣器使能开关", &g_ParamData.system_BeepEnableFlag, 0);
    GUI_CreateCheckItem(device, otherSetting, "电源低压报警", &g_ParamData.system_VoltageBeepFlag, 0);
    GUI_CreateTextItem(device, otherSetting, "电池设置");
    GUI_CreateFloatItemR(device, otherSetting, "电压值", &g_BatteryVoltage);
    GUI_CreateIntItem32(device, otherSetting, "修正系数", (int32_t *)&g_ParamData.system_VoltageCoff, 0, 1000);
    GUI_CreateFloatItem(device, otherSetting, "低压报警阈值", &g_ParamData.system_BeepVoltage, 0, 16);
    GUI_CreateTextItem(device, otherSetting, "目标设备设置");
    GUI_CreateCheckItem(device, otherSetting, "使用无线模块(NRF24L01)", &g_ParamData.app_UseDevice, 1);
    GUI_CreateCheckItem(device, otherSetting, "使用网络模块(ESP8266)", &g_ParamData.app_UseDevice, 2);
    GUI_CreateCheckItem(device, otherSetting, "使用蓝牙模块(HC03/04)", &g_ParamData.app_UseDevice, 3);
    GUI_CreateTextItem(device, otherSetting, "待机时间设置");
    GUI_CreateModeItem(device, otherSetting, "待机时间设置", s_PwrTimeMode, &g_ParamData.pwr_standbyTime);
    GUI_CreateTextItem(device, otherSetting, "日期设置");
    GUI_CreateIntItem16F(device, otherSetting, "年份", &g_RTC_Data.year, 2000, 2100, StartSetTimeOperator, StopSetTimeOperator);
    GUI_CreateIntItem8F(device, otherSetting, "月份", &g_RTC_Data.month, 1, 12, StartSetTimeOperator, StopSetTimeOperator);
    GUI_CreateIntItem8F(device, otherSetting, "日期", &g_RTC_Data.day, 1, 31, StartSetTimeOperator, StopSetTimeOperator);
    GUI_CreateTextItem(device, otherSetting, "时间设置");
    GUI_CreateIntItem8F(device, otherSetting, "小时", &g_RTC_Data.hour, 0, 23, StartSetTimeOperator, StopSetTimeOperator);
    GUI_CreateIntItem8F(device, otherSetting, "分钟", &g_RTC_Data.minute, 0, 59, StartSetTimeOperator, StopSetTimeOperator);
    GUI_CreateIntItem8ZF(device, otherSetting, "秒数", &g_RTC_Data.second, 0, SetSecondOperator);
    GUI_CreateTextItem(device, otherSetting, "保存与恢复设置");
    GUI_CreateFuncItem(device, otherSetting, "保存当前设置", SaveSystemOperator);
    GUI_CreateFuncItem(device, otherSetting, "恢复默认设置", LoadSystemOperator);

    /* 串口数据 */
    GUI_CreateTextItem(device, uart, "串口设置选择");
    GUI_CreateJumpItem(device, uart, "串口COM2设置", uartCom[COM2]);
    GUI_CreateJumpItem(device, uart, "串口COM4设置", uartCom[COM4]);
    GUI_CreateJumpItem(device, uart, "串口COM6设置", uartCom[COM6]);
    GUI_CreateJumpItem(device, uart, "串口COM7设置", uartCom[COM7]);
    GUI_CreateTextItem(device, uart, "串口端口说明");
    GUI_CreateFuncItem(device, uart, "COM1作为调试串口", NULL);
    GUI_CreateFuncItem(device, uart, "COM2使用PD05和PD06端口", NULL);
    GUI_CreateFuncItem(device, uart, "COM3作为激光雷达串口", NULL);
    GUI_CreateFuncItem(device, uart, "COM4使用PH13和PH14端口", NULL);
    GUI_CreateFuncItem(device, uart, "COM5作为陀螺仪串口", NULL);
    GUI_CreateFuncItem(device, uart, "COM6使用PG9端口(仅接收)", NULL);
    GUI_CreateFuncItem(device, uart, "COM7使用PA8端口(仅接收)", NULL);

    /* 添加菜单项并初始化串口 */
    for (uint8_t comX = COM1; comX < COM_COUNT; comX++)
    {
        /* COM1/3/5 不支持设置 */
        if (comX == COM1 || comX == COM3 || comX == COM5) { continue; }

        GUI_CreateTextItem(device, uartCom[comX], "接口设置");
        GUI_CreateModeItemF(device, uartCom[comX], "波特率", s_UartBaudRate, &g_ParamData.uart_baudRate[comX], BaudRateSetOperator);
        GUI_CreateModeItemF(device, uartCom[comX], "校验位", s_UartParity, &g_ParamData.uart_parity[comX], ParitySetOperator);
        GUI_CreateModeItemF(device, uartCom[comX], "停止位", s_UartStopBits, &g_ParamData.uart_stopBits[comX], StopBitsSetOperator);
        GUI_CreateTextItem(device, uartCom[comX], "接收设置");
        GUI_CreateCheckItem(device, uartCom[comX], "开启命令接收模式", &g_CMD.receiveMode, comX);
        GUI_CreateCheckItem(device, uartCom[comX], "HEX命令校验模式", &g_CMD.hexStrictMode, comX);

        /* COM6/7 不支持发送 */
        if (comX != COM7 && comX != COM6)
        {
            GUI_CreateTextItem(device, uartCom[comX], "数据广播使能");
            GUI_CreateCheckItem(device, uartCom[comX], "是否开启", &g_CMD.comBoardCast, comX);
            GUI_CreateUintItem16(device, uartCom[comX], "广播使能间隔时间", &g_CMD.comBoardCastDeltaTime[comX], 0, 10000);
            GUI_CreateTextItem(device, uartCom[comX], "调试输出使能");
            GUI_CreateCheckItem(device, uartCom[comX], "欧拉角数据", &g_CMD.dataOutputShowFlag[comX], 0);
            GUI_CreateCheckItem(device, uartCom[comX], "小车速度和位置数据", &g_CMD.dataOutputShowFlag[comX], 2);
            GUI_CreateCheckItem(device, uartCom[comX], "电池电压和蜂鸣器状态", &g_CMD.dataOutputShowFlag[comX], 3);
            GUI_CreateUintItem8Z(device, uartCom[comX], "重置串口输出", &g_CMD.dataOutputShowFlag[comX], 0)->disableShowNumberFlag = TRUE;
        }

        GUI_CreateTextItem(device, uartCom[comX], "保存与恢复设置");
        GUI_CreateFuncItem(device, uartCom[comX], "保存当前设置", SaveCommandOperator);
        GUI_CreateFuncItem(device, uartCom[comX], "恢复默认设置", LoadCommandOperator);
    }

    /* 作者信息 */
    GUI_CreateTextItem(device, author, "作者信息");
    GUI_CreateFuncItem(device, author, g_ParamData.info_guiMenuShowAnother, NULL);
    GUI_CreateFuncItem(device, author, g_ParamData.info_guiMenuShowProject, NULL);
    GUI_CreateFuncItem(device, author, g_ParamData.info_guiMenuShowVerision, NULL);
    GUI_CreateFuncItem(device, author, g_ParamData.info_guiMenuShowOrganization1, NULL);
    GUI_CreateFuncItem(device, author, g_ParamData.info_guiMenuShowOrganization2, NULL);
    GUI_CreateFuncItem(device, author, g_ParamData.info_guiMenuShowExtraInfo, NULL);

    /* 结束初始化 */
    GUI_MenuDeviceInit(device, Main_Run);
}
