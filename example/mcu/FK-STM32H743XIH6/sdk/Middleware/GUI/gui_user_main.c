#include "gui.h"
#include "car.h"
#include "imu.h"
#include "store.h"

#include "gui_user_main_define.h"

extern float g_FrontUltraSonicDistance, g_BackUltraSonicDistance;
extern uint8_t g_WirelessControlFlag, g_ServoControlFlag;
extern float g_BatteryVoltage;

/**
 * @brief 主界面回调函数
 */
uint16_t Main_Run(GUI_FuncItem_T *sender, GUI_MenuParam_T *param)
{
    /* 初始化界面 */
    if (param->InitFlag != 0)
    {
        Key_ClearAll();
        GUI_LCD_SetShowFont(GUI_FONT_TYPE_GB2312);
        GUI_LCD_FillColor(COL(0), B_ROW(0), GUI_WIDTH, GUI_HEIGHT, GUI_BLACK);

        /* 标题区域 */
        GUI_LCD_FillColor(COL(0), B_ROW(0), GUI_WIDTH, 20, GUI_BLUE);
        GUI_LCD_ShowString(COL(0), ROW(0), g_ParamData.info_guiMainTitle, GUI_WHITE, GUI_BLUE);
        GUI_LCD_ShowString(COL(0), ROW(1), "模式:          ", GUI_WHITE, GUI_BLACK);
        GUI_LCD_ShowString(COL(0), ROW(2), "控制:           ", GUI_WHITE, GUI_BLACK);

        /* 公共数据显示区域 */
        GUI_LCD_FillColor(COL(0), B_ROW(3), GUI_WIDTH, 20, GUI_RED);
        GUI_LCD_ShowString(COL(0), ROW(3), "系统状态信息                  ", GUI_WHITE, GUI_RED);
        GUI_LCD_ShowString(COL(0), ROW(4), "时钟:     -  -     :  :      ", GUI_WHITE, GUI_BLACK);
        GUI_LCD_ShowString(COL(0), ROW(5), "雷达:          ", GUI_WHITE, GUI_BLACK);
        GUI_LCD_ShowString(COL(0), ROW(6), "方向:       度  电压:       V ", GUI_WHITE, GUI_BLACK);
        GUI_LCD_ShowString(COL(0), ROW(7), "前障:       cm  后障:       cm", GUI_WHITE, GUI_BLACK);
        GUI_LCD_ShowString(COL(0), ROW(8), "云台:       度", GUI_WHITE, GUI_BLACK);
        GUI_LCD_ShowString(COL(0), ROW(9), "遥控:           灯光:       % ", GUI_WHITE, GUI_BLACK);

        /* 底部按键提示 */
        GUI_LCD_FillColor(COL(0), B_ROW(15), GUI_WIDTH, GUI_HEIGHT, GUI_BROWN);
        GUI_LCD_ShowString(COL(0), ROW(15), "进入-菜单 向上-U盘", GUI_WHITE, GUI_BROWN);

        param->userState = 0xFF;
    }

    /* 更新状态特定信息 */
    if (g_Car.state != CAR_MODE_STOP)
    {
        if (param->userState != g_Car.state)
        {
            GUI_LCD_ShowString(COL(6), ROW(1), g_Car.state == CAR_MODE_REMOTE ? "遥控模式" : "避障模式", g_Car.state == CAR_MODE_REMOTE ? GUI_MAGENTA : GUI_CYAN, GUI_BLACK);
            GUI_LCD_ShowString(COL(6), ROW(2), "串口控制", GUI_YELLOW, GUI_BLACK);
            GUI_LCD_FillColor(COL(0), B_ROW(10), GUI_WIDTH, 100, GUI_BLACK);
            GUI_LCD_FillColor(COL(0), B_ROW(10), GUI_WIDTH, 20, GUI_MAGENTA);
            GUI_LCD_ShowString(COL(0), ROW(10), "当前小车运动状态", GUI_WHITE, GUI_MAGENTA);
            GUI_LCD_ShowString(COL(0), ROW(11), "前进V_X=     m/s X=     m", GUI_WHITE, GUI_BLACK);
            GUI_LCD_ShowString(COL(0), ROW(12), "横向V_Y=     m/s Y=     m", GUI_WHITE, GUI_BLACK);
            GUI_LCD_ShowString(COL(0), ROW(13), "转向W_Z=       deg/s", GUI_WHITE, GUI_BLACK);
            GUI_LCD_ShowString(COL(0), ROW(14), "主频:     MHz 看门狗:         ", GUI_WHITE, GUI_BLACK);
            GUI_LCD_ShowNumber(COL(6), ROW(14), SystemCoreClock / 1000000, 3, GUI_YELLOW, GUI_BLACK, FALSE);
            GUI_LCD_ShowString(COL(22), ROW(14), SYSTEM_DEBUG_MODE ? "关闭" : "开启", GUI_YELLOW, GUI_BLACK);
            param->userState = g_Car.state;
        }

        GUI_LCD_ShowFloatNumber(COL(8), ROW(11), g_IMU.OdomSpeedX, 1, 3, GUI_YELLOW, GUI_BLACK, TRUE);
        GUI_LCD_ShowFloatNumber(COL(8), ROW(12), g_IMU.OdomSpeedY, 1, 3, GUI_YELLOW, GUI_BLACK, TRUE);
        GUI_LCD_ShowFloatNumber(COL(19), ROW(11), g_Car.remote.carLocationX, 3, 1, GUI_YELLOW, GUI_BLACK, TRUE);
        GUI_LCD_ShowFloatNumber(COL(19), ROW(12), g_Car.remote.carLocationY, 3, 1, GUI_YELLOW, GUI_BLACK, TRUE);
        GUI_LCD_ShowFloatNumber(COL(8), ROW(13), g_Car.remote.actualSpeedT, 3, 3, GUI_YELLOW, GUI_BLACK, TRUE);
    }
    else
    {
        if (param->userState != g_Car.state)
        {
            GUI_LCD_ShowString(COL(6), ROW(1), "停止模式", GUI_GREEN, GUI_BLACK);
            GUI_LCD_ShowString(COL(6), ROW(2), "系统待机", GUI_YELLOW, GUI_BLACK);
            GUI_LCD_FillColor(COL(0), B_ROW(10), GUI_WIDTH, 100, GUI_BLACK);
            GUI_LCD_FillColor(COL(0), B_ROW(10), GUI_WIDTH, 20, GUI_MAGENTA);
            GUI_LCD_ShowString(COL(0), ROW(10), "设备信息", GUI_WHITE, GUI_MAGENTA);
            GUI_LCD_ShowString(COL(0), ROW(11), g_ParamData.info_guiMainWinProjectTitle, GUI_WHITE, GUI_BLACK);
            GUI_LCD_ShowString(COL(6), ROW(11), g_ParamData.info_guiMainWinProjectName, GUI_YELLOW, GUI_BLACK);
            GUI_LCD_ShowString(COL(0), ROW(12), "主控: ", GUI_WHITE, GUI_BLACK);
            GUI_LCD_ShowString(COL(6), ROW(12), "STM32H743XIH6 TFBGA240", GUI_YELLOW, GUI_BLACK);
            GUI_LCD_ShowString(COL(0), ROW(13), g_ParamData.info_guiMainWinExtraTitle, GUI_WHITE, GUI_BLACK);
            GUI_LCD_ShowString(COL(6), ROW(13), g_ParamData.info_guiMainWinExtraName, GUI_YELLOW, GUI_BLACK);
            GUI_LCD_ShowString(COL(0), ROW(14), "主频:     MHz 看门狗:         ", GUI_WHITE, GUI_BLACK);
            GUI_LCD_ShowNumber(COL(6), ROW(14), SystemCoreClock / 1000000, 3, GUI_YELLOW, GUI_BLACK, FALSE);
            GUI_LCD_ShowString(COL(22), ROW(14), SYSTEM_DEBUG_MODE ? "关闭" : "开启", GUI_YELLOW, GUI_BLACK);
            param->userState = g_Car.state;
        }
    }

    /* 时间 */
    GUI_LCD_ShowNumber(COL(6), ROW(4), g_RTC_Data.year, 4, GUI_YELLOW, GUI_BLACK, FALSE);
    GUI_LCD_ShowNumber(COL(11), ROW(4), g_RTC_Data.month, 2, GUI_YELLOW, GUI_BLACK, FALSE);
    GUI_LCD_ShowNumber(COL(14), ROW(4), g_RTC_Data.day, 2, GUI_YELLOW, GUI_BLACK, FALSE);
    GUI_LCD_ShowNumber(COL(17), ROW(4), g_RTC_Data.hour, 2, GUI_YELLOW, GUI_BLACK, FALSE);
    GUI_LCD_ShowNumber(COL(20), ROW(4), g_RTC_Data.minute, 2, GUI_YELLOW, GUI_BLACK, FALSE);
    GUI_LCD_ShowNumber(COL(23), ROW(4), g_RTC_Data.second, 2, GUI_YELLOW, GUI_BLACK, FALSE);

    /* 设备信息 */
    GUI_LCD_ShowString(COL(6), ROW(5), s_LidarDevice[g_LidarState.currentLidarType], GUI_YELLOW, GUI_BLACK);

    /* 姿态角度/电池电压 */
    GUI_LCD_ShowFloatNumber(COL(6), ROW(6), g_Car.remote.carYaw, 3, 0, GUI_YELLOW, GUI_BLACK, TRUE);
    GUI_LCD_ShowFloatNumber(COL(22), ROW(6), g_BatteryVoltage, 2, 2, GUI_YELLOW, GUI_BLACK, FALSE);

    /* 距离信息 */
    GUI_LCD_ShowFloatNumber(COL(6), ROW(7), g_FrontUltraSonicDistance, 3, 1, GUI_YELLOW, GUI_BLACK, FALSE);
    GUI_LCD_ShowFloatNumber(COL(22), ROW(7), g_BackUltraSonicDistance, 3, 1, GUI_YELLOW, GUI_BLACK, FALSE);

    /* 云台/灯光 */
    GUI_LCD_ShowFloatNumber(COL(6), ROW(8), g_ServoAngle[SERVO_1], 3, 1, GUI_YELLOW, GUI_BLACK, FALSE);

    /* 遥控显示 */
    GUI_LCD_ShowString(COL(6), ROW(9), g_WirelessControlFlag > 0 ? "遥控器  " : "串口遥控", GUI_YELLOW, GUI_BLACK);
    GUI_LCD_ShowFloatNumber(COL(22), ROW(9), g_LightBrightness, 3, 1, GUI_YELLOW, GUI_BLACK, FALSE);

    /* KEY_ENTER 进入菜单 */
    if (Key_IsAction(KEY_ENTER))
    {
        Car_SetMode(CAR_MODE_STOP);
        Key_ClearAll();
        GUI_EnterMenu(&g_GUI_User.device, g_GUI_User.rootMenu);
        param->InitFlag = TRUE;
    }

    /* KEY_UP USB传输模式 */
    if (Key_IsLongPressed(KEY_UP))
    {
        RTC_WriteBKP(0x12341234);
        NVIC_SystemReset();
    }

    return param->userState;
}