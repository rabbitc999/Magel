#include "arm_control.h"
#include "wireless_task.h"
#include "simple_task.h"

extern uint8_t g_LidarDataReadyFlag, g_ConditionBeepFlag, g_ConditionCancelFlag;
extern volatile float g_FrontUltraSonicDistance, g_BackUltraSonicDistance;
extern float g_BatteryVoltage;
uint8_t g_WirelessControlFlag = 0, g_AvoidanceAutoFlag = 0;

#define LIMIT_UNSIGN(x) ((x) > 1 ? 1 : ((x) < 0 ? 0 : (x)))
#define LIMIT_SIGNED(x) ((x) > 1 ? 1 : ((x) < -1 ? -1 : (x)))

/**
 * @brief  遥控器控制解算任务
 * @param  pvParameters FreeRTOS任务参数（未使用）
 */
TaskRet_T NRF24L01_Task(TaskParam_T *param)
{
    /* 设置模式 */
    static uint8_t useFlag = 0;

    /* 如果没有使能NRF24L01，则删除该任务 (跟初始化时的状态相关) */
    if ((g_ParamData.app_UseDevice & 0x03) == 0x02) { useFlag = 1; }

    /* 无线模块 */
    if (NRF24L01_Receive() == 1 && NRF24L01_RxPacket[0] == 0xAA && NRF24L01_RxPacket[13] == 0x55)  // 判断是否收到无线模块数据包
    {
        /* 得到遥控器数据 */
        int8_t S1_DELTA = NRF24L01_RxPacket[1];
        int8_t S2_DELTA = NRF24L01_RxPacket[2];
        int8_t L1_DELTA = NRF24L01_RxPacket[3];  // 远光灯
        int8_t L2_DELTA = NRF24L01_RxPacket[4];  // 近光灯
        int8_t SX_DELTA = NRF24L01_RxPacket[5];  // 速度X
        int8_t ST_DELTA = NRF24L01_RxPacket[6];  // 转动角度
        int8_t REMOTE_MODE = NRF24L01_RxPacket[7];
        int8_t KEY_EV_MODE = NRF24L01_RxPacket[8];
        int8_t KEY_EV_ENTER = NRF24L01_RxPacket[9];
        int8_t KEY_EV_UP = NRF24L01_RxPacket[10];
        int8_t KEY_EV_DOWN = NRF24L01_RxPacket[11];
        int8_t CAR_AUTO = NRF24L01_RxPacket[12];

        if (!ArmControl_IsEnabled())
        {
            if (g_ServoAngle[SERVO_1] < 90) { Servo_SetAngle(SERVO_1, g_ServoAngle[SERVO_1] + 10 * (float)S1_DELTA / 100); }
            else
            {
                Servo_SetAngle(SERVO_1, 90);
            }
            Servo_SetAngle(SERVO_2, g_ServoAngle[SERVO_2] + 20 * (float)S2_DELTA / 100);
        }
        Light_SetBrightness(g_LightBrightness - 5 * (float)L1_DELTA / 100);

        /* 根据不同的设置模式设置相应传感器 */
        float SpeedX = SX_DELTA / 100.0f * g_Car.remote.remoteMaxSpeedX;
        float SpeedT = -ST_DELTA / 100.0f * g_Car.remote.remoteMaxSpeedT;
        Car_SetSpeed(SpeedX, SpeedT);

        if (KEY_EV_MODE != 0) { Key_SetEvent(KEY_MODE, KEY_EV_MODE); }
        if (KEY_EV_ENTER != 0) { Key_SetEvent(KEY_ENTER, KEY_EV_ENTER); }
        if (KEY_EV_UP != 0) { Key_SetEvent(KEY_UP, KEY_EV_UP); }
        if (KEY_EV_DOWN != 0) { Key_SetEvent(KEY_DOWN, KEY_EV_DOWN); }

        /* 右边的按键分布
                K5
            K7      K8
                K6
        */
        if (REMOTE_MODE >= 1)  // 遥控器K5键按下
        {
            uint8_t CurrentMode = Car_GetMode();
            CurrentMode = (CurrentMode + 1) % CAR_MODE_COUNT;
            Car_SetMode(CurrentMode);
        }
        if (CAR_AUTO == 1)  // 遥控器K10键按下
        {
            g_AvoidanceAutoFlag ^= 1;

            /* 设置永真条件用于条件域触发 */
            if (g_AvoidanceAutoFlag == 1 && g_Car.state == CAR_MODE_AVOIDANCE) { Condition_Configure(0, "1=1", "min_x:=0.15", CONDITION_DOMAIN_INFINITE); }
            else
            {
                Condition_Disable(0);
                g_AvoidanceAutoFlag = 0;
            }
        }

        /* 设置标志为50，也就是50次未连接后，小车认为没有连接上无线遥控器 */
        g_WirelessControlFlag = 50;
        PWR_RecordTime();
    }
    else
    {
        if (g_WirelessControlFlag > 0)
        {
            g_WirelessControlFlag -= 1;
        }
    }

    /* 写入相关的状态信息 */
    //    if (g_WirelessControlFlag > 0)
    {
        uint8_t sendFlag = ((g_Car.state & 0x03) << 2) | ((g_ConditionBeepFlag & 0x01) << 4) | ((g_ConditionCancelFlag & 0x01) << 5);

        /* 灯光 / 标志位 */
        NRF24L01_TxPacket[0] = g_LightBrightness;  // 0
        NRF24L01_TxPacket[1] = sendFlag;           // 1
        NRF24L01_TxPacket[2] = 0;                  // 2

        /* 以下数据小端方式传输 */
        NRF24L01_TxPacket[3] = LIMIT_UNSIGN(g_BatteryVoltage / 24) * 255.0f;                            // 3
        *(int16_t *)&NRF24L01_TxPacket[4] = LIMIT_UNSIGN(g_ServoAngle[SERVO_1] / 180.0f) * 32767;       // 4 5
        *(int16_t *)&NRF24L01_TxPacket[6] = LIMIT_UNSIGN(g_ServoAngle[SERVO_2] / 180.0f) * 32767;       // 6 7
        *(int16_t *)&NRF24L01_TxPacket[8] = LIMIT_SIGNED(g_IMU.Yaw / 180.0f) * 32767;                   // 8 9
        *(int16_t *)&NRF24L01_TxPacket[10] = LIMIT_SIGNED(g_IMU.Pitch / 90.0f) * 32767;                 // 10 11
        *(int16_t *)&NRF24L01_TxPacket[12] = LIMIT_SIGNED(g_IMU.Roll / 180.0f) * 32767;                 // 12 13
        *(int16_t *)&NRF24L01_TxPacket[14] = LIMIT_SIGNED(g_Car.remote.targetSpeedT / 300.0f) * 32767;  // 14 15
        *(int16_t *)&NRF24L01_TxPacket[16] = LIMIT_SIGNED(g_Car.remote.targetSpeedX / 2.0f) * 32767;    // 16 17

        *(float *)&NRF24L01_TxPacket[18] = g_IMU.LocationX;       // 18 19 20 21
        *(float *)&NRF24L01_TxPacket[22] = g_IMU.LocationY;       // 22 23 24 25
        *(int32_t *)&NRF24L01_TxPacket[26] = RTC_GetTimestamp();  // 26 27 28 29

        NRF24L01_TxPacket[30] = LIMIT_UNSIGN(g_FrontUltraSonicDistance / 300) * 255.0f;  // 30
        NRF24L01_TxPacket[31] = LIMIT_UNSIGN(g_BackUltraSonicDistance / 300) * 255.0f;   // 31

        /* 发送状态信息给遥控器，以便在遥控器上观察小车状态 */
        NRF24L01_Send();
    }

    return Task_Delay(100);
}