#include "arm_control.h"
#include "cmd.h"
#include "cmd_user_hex.h"
#include "cmd_file_transfer.h"
#include "gui_capture.h"
#include "bsp_system.h"
#include "car.h"
#include "imu.h"
#include "wireless_task.h"

extern uint8_t g_LidarDataReadyFlag, g_ConditionBeepFlag, g_ConditionCancelFlag, g_ServoControlFlag;
extern volatile float g_FrontUltraSonicDistance, g_BackUltraSonicDistance;
extern float g_BatteryVoltage;
extern uint8_t g_WirelessControlFlag, g_AvoidanceAutoFlag;
CmdHexTable_T g_CMDHexTable = { 0 };
static uint8_t s_HexSendArray[256] = { 0 };

/**
 * @brief 蜂鸣器控制命令（二进制）
 * @param comX: 串口号
 * @param packet: HEX数据包
 * @note
 *
 * 格式1: 停止蜂鸣器
 *  数据长度: 0字节
 *  数据: 无
 *  说明: 测试回传指令
 */
static void CMD_HexTest(uint16_t comX, CmdHexDataPacket_T *packet)
{
    CMD_HexTransmitOk(comX, packet->commandId);
}

/**
 * @brief 蜂鸣器控制命令（二进制）
 * @param comX: 串口号
 * @param packet: HEX数据包
 * @note
 *
 * 格式1: 停止蜂鸣器
 *  数据长度: 0字节
 *  功能码: 0x00
 *  数据: [无]
 *  说明: 立即停止蜂鸣器
 *
 * 格式2: 播放蜂鸣
 *  数据长度: 5字节
 *  功能码: 0x01
 *  数据: [开启时间(2字节)] + [关闭时间(2字节)] + [循环次数(1字节)]
 *  说明:
 *    - 开启时间: uint16_t, 毫秒
 *    - 关闭时间: uint16_t, 毫秒
 *    - 循环次数: uint8_t
 *  示例: 01 64 00 32 00 0A
 *        表示: 开启100ms, 关闭50ms, 循环10次
 */
static void CMD_HexBeep(uint16_t comX, CmdHexDataPacket_T *packet)
{
    if (packet->funcId == 0)
    {
        /* 停止蜂鸣器: 0x00 */
        Beep_Stop();
        CMD_HexTransmitOk(comX, packet->commandId);
    }
    else if (packet->dataLength >= 5)
    {
        /* 蜂鸣命令: onTime(2) + offTime(2) + cycles(1) */
        uint16_t onTime = CMD_HexReadUint16(packet, 0);
        uint16_t offTime = CMD_HexReadUint16(packet, 2);
        uint8_t cycles = CMD_HexReadUint8(packet, 4);

        Beep_Play(onTime, offTime, cycles);
        CMD_HexTransmitOk(comX, packet->commandId);
    }
    else
    {
        CMD_HexTransmitError(comX, packet->commandId, CMD_HEX_ERROR_DATA_LENGTH);
    }
}

/* Command 0x02: 64-bit milliseconds in the frame timestamp, empty payload. */
static void CMD_HexRtc(uint16_t comX, CmdHexDataPacket_T *packet)
{
    if (packet->funcId != 0 || packet->dataLength != 0)
    {
        CMD_HexTransmitError(comX, packet->commandId, CMD_HEX_ERROR_DATA_LENGTH);
        return;
    }
    Timer_SetTimestampMs(packet->timestampMs);
    CMD_HexTransmitOk(comX, packet->commandId);
}

/**
 * @brief 模式切换命令（二进制）
 * @param comX: 串口号
 * @param packet: HEX数据包
 * @note
 *
 * 格式: 切换小车模式
 *  数据长度: 0字节
 *  数据: [无]
 *  功能码:
 *    - 0x00: 停止模式
 *    - 0x01: 遥控模式
 *    - 0x02: 避障模式
 *  示例: 0x01 表示切换到遥控模式
 */
static void CMD_HexMode(uint16_t comX, CmdHexDataPacket_T *packet)
{
    switch (packet->funcId)
    {
    case 0: /* 停止模式 */
        Car_SetMode(CAR_MODE_STOP);
        CMD_HexTransmitOk(comX, packet->commandId);
        break;
    case 1: /* 遥控模式 */
        Car_SetMode(CAR_MODE_REMOTE);
        CMD_HexTransmitOk(comX, packet->commandId);
        break;
    case 2: /* 避障模式 */
        Car_SetMode(CAR_MODE_AVOIDANCE);
        CMD_HexTransmitOk(comX, packet->commandId);
        break;
    default:
        CMD_HexTransmitError(comX, packet->commandId, CMD_HEX_ERROR_DATA_FORMAT);
        break;
    }
}

/**
 * @brief 数据输出控制命令（二进制）
 * @param comX: 串口号
 * @param packet: HEX数据包
 * @note
 *
 * 格式: 设置为输出模式
 *  数据长度: 0字节
 *  数据: [无]
 *
 * @note 数据输出将IMU和小车状态数据打包成32字节的二进制数据发送 (数据包大小：60+N字节)
 */
void CMD_HexShow(uint16_t comX, CmdHexDataPacket_T *packet)
{
#define LIMIT_UNSIGN(x) ((x) > 32767 ? 32767 : ((x) < 0 ? 0 : (x)))
    CmdHexDataPacket_T transmitPacket = {
        .commandId = packet->commandId,
        .funcId = packet->funcId,
        .data = s_HexSendArray,
        .dataLength = 72
    };

    uint8_t sendFlag1 = (g_WirelessControlFlag & 0x01) | ((g_Car.state & 0x03) << 2) | ((g_ConditionCancelFlag & 0x01) << 4) | ((g_ConditionBeepFlag & 0x01) << 5);
    uint8_t sendFlag2 = 0xFF; /* 后8位保留给巡线控制的状态 */
    uint8_t idx = 0;

    /* 1. 陀螺仪数据 (6字节) */
    /* 范围: ±500 DPS, 精度: 0.01526度/秒 */
    CMD_HexWriteInt16(&transmitPacket, idx, g_IMU.GyroX * 65.534f), idx += 2;
    CMD_HexWriteInt16(&transmitPacket, idx, g_IMU.GyroY * 65.534f), idx += 2;
    CMD_HexWriteInt16(&transmitPacket, idx, g_IMU.GyroZ * 65.534f), idx += 2;

    /* 2. 加速度数据 (6字节) */
    /* 范围: ±2G, 精度: 0.000061G */
    CMD_HexWriteInt16(&transmitPacket, idx, g_IMU.AccelX * 16383.5f), idx += 2;
    CMD_HexWriteInt16(&transmitPacket, idx, g_IMU.AccelY * 16383.5f), idx += 2;
    CMD_HexWriteInt16(&transmitPacket, idx, g_IMU.AccelZ * 16383.5f), idx += 2;

    /* 3. 四元数数据 (8字节) */
    /* 范围: [-1, 1], 精度: 0.0000305 */
    CMD_HexWriteInt16(&transmitPacket, idx, g_IMU.Q0 * 32767.0f), idx += 2;
    CMD_HexWriteInt16(&transmitPacket, idx, g_IMU.Q1 * 32767.0f), idx += 2;
    CMD_HexWriteInt16(&transmitPacket, idx, g_IMU.Q2 * 32767.0f), idx += 2;
    CMD_HexWriteInt16(&transmitPacket, idx, g_IMU.Q3 * 32767.0f), idx += 2;

    /* 4. 速度数据 (4字节) */
    CMD_HexWriteInt16(&transmitPacket, idx, g_Car.remote.actualSpeedX * 32767.0f), idx += 2;
    CMD_HexWriteInt16(&transmitPacket, idx, g_Car.remote.actualSpeedT * 163.835f), idx += 2;

    /* 5. 状态标志 (2字节) */
    CMD_HexWriteInt8(&transmitPacket, idx, sendFlag1), idx += 1;  // 后4位预留巡线状态
    CMD_HexWriteInt8(&transmitPacket, idx, sendFlag2), idx += 1;

    /* 6. 完整位置数据 (8字节，保持浮点数) */
    CMD_HexWriteFloat(&transmitPacket, idx, g_IMU.LocationX), idx += 4;
    CMD_HexWriteFloat(&transmitPacket, idx, g_IMU.LocationY), idx += 4;

    /* 7. 完整角度数据 (6字节) */
    CMD_HexWriteInt16(&transmitPacket, idx, g_IMU.Pitch * 364.0778f), idx += 2;
    CMD_HexWriteInt16(&transmitPacket, idx, g_IMU.Roll * 182.0389f), idx += 2;
    CMD_HexWriteInt16(&transmitPacket, idx, g_IMU.Yaw * 182.0389f), idx += 2;

    /* 8. 电压数据 (4字节) */
    CMD_HexWriteInt16(&transmitPacket, idx, g_BatteryVoltage * 2730.625f), idx += 2;
    CMD_HexWriteInt16(&transmitPacket, idx, g_ParamData.system_BeepVoltage * 2730.625f), idx += 2;

    /* 8. 电压数据 (4字节) */
    CMD_HexWriteInt16(&transmitPacket, idx, 0 * 2730.625f), idx += 2;
    CMD_HexWriteInt16(&transmitPacket, idx, 0 * 2730.625f), idx += 2;

    /* 9. 前后超声波数据 (4字节) */
    CMD_HexWriteInt16(&transmitPacket, idx, LIMIT_UNSIGN(g_FrontUltraSonicDistance * 72.8156f)), idx += 2;
    CMD_HexWriteInt16(&transmitPacket, idx, LIMIT_UNSIGN(g_BackUltraSonicDistance * 72.8156f)), idx += 2;

    /* 10. 巡线传感器数据 (保留) */
    CMD_HexWriteInt8(&transmitPacket, idx, 0x00), idx += 1;  // 0为白色，1为黑色，共8位

    /* 11. 前照灯光PWM数值 (1-100) */
    CMD_HexWriteInt8(&transmitPacket, idx, g_LightBrightness), idx += 1;  // 0为白色，1为黑色

    /* 12. 舵机角度数据 (4字节) */
    CMD_HexWriteInt16(&transmitPacket, idx, LIMIT_UNSIGN(g_ServoAngle[SERVO_1] * 182.0389f)), idx += 2;
    CMD_HexWriteInt16(&transmitPacket, idx, LIMIT_UNSIGN(g_ServoAngle[SERVO_2] * 182.0389f)), idx += 2;

    /* 13. RTC时间 (4字节) */
    CMD_HexWriteInt32(&transmitPacket, idx, RTC_GetTimestamp()), idx += 4;

    /* 14. 设置数据长度 */
    transmitPacket.dataLength = idx;

    CMD_HexTransmitData(comX, &transmitPacket);
#undef LIMIT_UNSIGN
}

/**
 * @brief 遥控命令（二进制）
 * @param comX: 串口号
 * @param packet: HEX数据包
 * @note
 *
 * 如果小车不处于遥控模式或避障遥控模式，则会返回错误信息0x77
 *
 * 格式1: 停止小车
 *  功能码: 0x00
 *  数据长度: 1字节
 *  数据: [0x00]
 *  说明: 在遥控模式下停止小车运动
 *
 * 格式2: 速度控制
 *  功能码: 0x01
 *  数据长度: 12字节
 *  数据: [X速度(4字节)] + [角度速度(4字节)]
 *  说明:
 *    - X速度: float, 前进/后退速度(m/s)
 *    - 角度速度: float, 旋转速度(deg/s)
 *  示例: 0x00 0x00 0x80 0x3F 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00
 *        表示: X速度=1.0, Y速度=0, 角度速度=0
 * 格式3: 速度控制(简单数据包)
 *  功能码: 0x02
 *  数据长度: 6字节
 *  数据: [X速度(2字节)] + [角度速度(2字节)]
 *  说明:
 *    - X速度: int16_t, 前进/后退速度，-32767表示最低速度（-1.0m/s），32767表示最高速度（1.0m/s）
 *    - 角度速度: int16_t, 旋转速度(deg/s)，-32767表示最低速度（-200deg/s），32767表示最高速度（200deg/s）
 *  示例: 0x7F 0xFF 0x00 0x00 0x00 0x00
 *        表示: X速度=1.0, Y速度=0, 角度速度=0
 */
static void CMD_HexCarControl(uint16_t comX, CmdHexDataPacket_T *packet)
{
    if (packet->funcId == 0)
    {
        /* 停止命令: 0x00 */
        Car_SetSpeed(0, 0);
        CMD_HexTransmitOk(comX, packet->commandId);
    }
    else if (packet->funcId == 1 && packet->dataLength >= 12)
    {
        /* 兼容前面版本的命令 */
        float speedX = CMD_HexReadFloat(packet, 0);
        float speedT = CMD_HexReadFloat(packet, 8);
        Car_SetSpeed(speedX, speedT);
        CMD_HexTransmitOk(comX, packet->commandId);
    }
    else if (packet->funcId == 1 && packet->dataLength >= 8 && packet->dataLength < 12)
    {
        /* 现版本的命令 */
        float speedX = CMD_HexReadFloat(packet, 0);
        float speedT = CMD_HexReadFloat(packet, 4);
        Car_SetSpeed(speedX, speedT);
        CMD_HexTransmitOk(comX, packet->commandId);
    }
    else if (packet->funcId == 2 && packet->dataLength >= 4)
    {
        float speedX = CMD_HexReadInt16(packet, 0) / 32767.0f;
        float speedT = CMD_HexReadInt16(packet, 2) / 163.835f;
        Car_SetSpeed(speedX, speedT);
        CMD_HexTransmitOk(comX, packet->commandId);
    }
    else
    {
        CMD_HexTransmitError(comX, packet->commandId, CMD_HEX_ERROR_DATA_FORMAT);
    }
}

/**
 * @brief 小车停止命令（二进制）
 * @param comX: 串口号
 * @param packet: HEX数据包
 * @note
 *
 * 格式: 立即停止
 *  功能码: 任意指定
 *  数据长度: 0字节
 *  数据: 无
 *  说明: 立即停止小车所有运动，不受模式限制
 */
static void CMD_HexStop(uint16_t comX, CmdHexDataPacket_T *packet)
{
    Car_SetMode(CAR_MODE_STOP);
    CMD_HexTransmitOk(comX, packet->commandId);
}

/**
 * @brief 小车欧拉角设置（二进制）
 * @param comX: 串口号
 * @param packet: HEX数据包
 * @note
 *
 * 格式1: 设置为偏航角，俯仰角，滚转角数值
 *  功能码: 0x01
 *  数据长度: 12字节
 *  数据: [俯仰角(4字节), 滚转角(4字节), 偏航角(4字节)]
 *  说明: 使用float格式，角度制
 * 格式2: 设置为偏航角，俯仰角，滚转角数值
 *  功能码: 0x02
 *  数据长度: 6字节
 *  数据: [俯仰角(2字节), 滚转角(2字节), 偏航角(2字节)]
 *  说明: 使用int16格式
 * 格式3: 设置为四元数数值
 *  功能码: 0x03
 *  数据长度: 8字节
 *  数据: [q0(4字节), q1(4字节), q2(4字节), q3(4字节)]
 *  说明: 使用float格式
 * 格式4: 设置为四元数数值
 *  功能码: 0x04
 *  数据长度: 8字节
 *  数据: [q0(2字节), q1(2字节), q2(2字节), q3(2字节)]
 *  说明: 使用int16格式
 */
static void CMD_HexSetEulerAngle(uint16_t comX, CmdHexDataPacket_T *packet)
{
    g_Car.remote.useIntervalControl = 0;  // 取消使用内部解算的数据

    if (packet->funcId == 1 && packet->dataLength == 12)
    {
        // 格式1: float 欧拉角 (pitch, roll, yaw) —— 单位：度
        g_Car.remote.carPitch = CMD_HexReadFloat(packet, 0);
        g_Car.remote.carRoll = CMD_HexReadFloat(packet, 4);
        g_Car.remote.carYaw = CMD_HexReadFloat(packet, 8);
        CMD_HexTransmitOk(comX, packet->commandId);
    }
    else if (packet->funcId == 2 && packet->dataLength == 6)
    {
        // 格式2: int16 欧拉角
        int16_t pitch_raw = CMD_HexReadInt16(packet, 0);
        int16_t roll_raw = CMD_HexReadInt16(packet, 2);
        int16_t yaw_raw = CMD_HexReadInt16(packet, 4);
        g_Car.remote.carPitch = (pitch_raw / 32767.0f) * 90.0f;
        g_Car.remote.carRoll = (roll_raw / 32767.0f) * 180.0f;
        g_Car.remote.carYaw = (yaw_raw / 32767.0f) * 180.0f;
        CMD_HexTransmitOk(comX, packet->commandId);
    }
    else if (packet->funcId == 3 && packet->dataLength == 16)
    {
        // 格式3: float 四元数 [q0, q1, q2, q3]
        float q0 = CMD_HexReadFloat(packet, 0);
        float q1 = CMD_HexReadFloat(packet, 4);
        float q2 = CMD_HexReadFloat(packet, 8);
        float q3 = CMD_HexReadFloat(packet, 12);
        g_Car.remote.carRoll = Math_Asin(2 * (q0 * q2 - q1 * q3));
        g_Car.remote.carPitch = Math_Atan2(2 * (q2 * q3 + q0 * q1), 1 - 2 * q1 * q1 - 2 * q2 * q2);
        g_Car.remote.carYaw = Math_Atan2(2 * (q1 * q2 + q0 * q3), 1 - 2 * q2 * q2 - 2 * q3 * q3);
        CMD_HexTransmitOk(comX, packet->commandId);
    }
    else if (packet->funcId == 4 && packet->dataLength == 8)
    {
        // 格式4: int16 四元数 → [-1.0, 1.0]
        float q0 = CMD_HexReadInt16(packet, 0) / 32767.0f;
        float q1 = CMD_HexReadInt16(packet, 2) / 32767.0f;
        float q2 = CMD_HexReadInt16(packet, 4) / 32767.0f;
        float q3 = CMD_HexReadInt16(packet, 6) / 32767.0f;
        g_Car.remote.carRoll = Math_Asin(2 * (q0 * q2 - q1 * q3));
        g_Car.remote.carPitch = Math_Atan2(2 * (q2 * q3 + q0 * q1), 1 - 2 * q1 * q1 - 2 * q2 * q2);
        g_Car.remote.carYaw = Math_Atan2(2 * (q1 * q2 + q0 * q3), 1 - 2 * q2 * q2 - 2 * q3 * q3);
        CMD_HexTransmitOk(comX, packet->commandId);
    }
    else
    {
        g_Car.remote.useIntervalControl = 1;  // 解析失败，使用内部解算的数据
        CMD_HexTransmitError(comX, packet->commandId, CMD_HEX_ERROR_DATA_LENGTH);
    }
}

/**
 * @brief 小车位置设置（二进制）
 * @param comX: 串口号
 * @param packet: HEX数据包
 * @note
 *
 * 格式1: 设置为小车内部解算位置
 *  功能码: 0x01
 *  数据长度: 8字节
 *  数据: [X位置(4字节), Y位置(4字节)]
 *  说明: 使用float格式
 *
 * 格式2: 设置为小车内部解算位置+偏航角
 *  功能码: 0x02
 *  数据长度: 12字节
 *  数据: [X位置(4字节), Y位置(4字节), 偏航角(4字节)]
 *  说明: 使用float格式，角度制
 *
 * 格式3: 设置为小车内部解算位置+偏航角
 *  功能码: 0x03
 *  数据长度: 10字节
 *  数据: [X位置(4字节), Y位置(4字节), 偏航角(2字节)]
 *  说明: 使用float+int16格式，角度制
 *
 * 格式4: 设置为小车内部解算位置+欧拉角
 *  功能码: 0x04
 *  数据长度: 20字节
 *  数据: [X位置(4字节), Y位置(4字节), 俯仰角(4字节), 滚转角(4字节), 偏航角(4字节)]
 *  说明: 使用float格式，角度制
 *
 * 格式5: 设置为小车内部解算位置+欧拉角
 *  功能码: 0x05
 *  数据长度: 14字节
 *  数据: [X位置(4字节), Y位置(4字节), 俯仰角(2字节), 滚转角(2字节), 偏航角(2字节)]
 *  说明: 使用float+int16格式，角度制
 *
 * 格式6: 设置为小车内部解算位置+四元数数值
 *  功能码: 0x06
 *  数据长度: 24字节
 *  数据: [X位置(4字节), Y位置(4字节), q0(4字节), q1(4字节), q2(4字节), q3(4字节)]
 *  说明: 使用float格式
 *
 * 格式7: 设置为小车内部解算位置+四元数数值
 *  功能码: 0x07
 *  数据长度: 16字节
 *  数据: [X位置(4字节), Y位置(4字节), q0(2字节), q1(2字节), q2(2字节), q3(2字节)]
 *  说明: 使用int16格式
 */
static void CMD_HexSetLocation(uint16_t comX, CmdHexDataPacket_T *packet)
{
    g_Car.remote.useIntervalControl = 0;  // 取消使用内部解算的数据

    if (packet->funcId == 1 && packet->dataLength == 8)
    {
        g_Car.remote.carLocationX = CMD_HexReadFloat(packet, 0);
        g_Car.remote.carLocationY = CMD_HexReadFloat(packet, 4);
        g_Car.remote.carYaw = g_IMU.Yaw;
        g_Car.remote.carPitch = g_IMU.Pitch;
        g_Car.remote.carRoll = g_IMU.Roll;
        CMD_HexTransmitOk(comX, packet->commandId);
    }
    else if (packet->funcId == 2 && packet->dataLength == 12)
    {
        g_Car.remote.carLocationX = CMD_HexReadFloat(packet, 0);
        g_Car.remote.carLocationY = CMD_HexReadFloat(packet, 4);
        g_Car.remote.carYaw = CMD_HexReadFloat(packet, 8);
        g_Car.remote.carPitch = g_IMU.Pitch;
        g_Car.remote.carRoll = g_IMU.Roll;
        CMD_HexTransmitOk(comX, packet->commandId);
    }
    else if (packet->funcId == 3 && packet->dataLength == 10)
    {
        g_Car.remote.carLocationX = CMD_HexReadFloat(packet, 0);
        g_Car.remote.carLocationY = CMD_HexReadFloat(packet, 4);
        int16_t yaw_raw = CMD_HexReadInt16(packet, 8);
        g_Car.remote.carYaw = (yaw_raw / 32767.0f) * 180.0f;
        g_Car.remote.carPitch = g_IMU.Pitch;
        g_Car.remote.carRoll = g_IMU.Roll;
        CMD_HexTransmitOk(comX, packet->commandId);
    }
    else if (packet->funcId == 4 && packet->dataLength == 20)
    {
        g_Car.remote.carLocationX = CMD_HexReadFloat(packet, 0);
        g_Car.remote.carLocationY = CMD_HexReadFloat(packet, 4);
        g_Car.remote.carPitch = CMD_HexReadFloat(packet, 8);
        g_Car.remote.carRoll = CMD_HexReadFloat(packet, 12);
        g_Car.remote.carYaw = CMD_HexReadFloat(packet, 16);
        CMD_HexTransmitOk(comX, packet->commandId);
    }
    else if (packet->funcId == 5 && packet->dataLength == 14)
    {
        g_Car.remote.carLocationX = CMD_HexReadFloat(packet, 0);
        g_Car.remote.carLocationY = CMD_HexReadFloat(packet, 4);
        int16_t pitch_raw = CMD_HexReadInt16(packet, 8);
        int16_t roll_raw = CMD_HexReadInt16(packet, 10);
        int16_t yaw_raw = CMD_HexReadInt16(packet, 12);
        g_Car.remote.carPitch = (pitch_raw / 32767.0f) * 90.0f;
        g_Car.remote.carRoll = (roll_raw / 32767.0f) * 180.0f;
        g_Car.remote.carYaw = (yaw_raw / 32767.0f) * 180.0f;
        CMD_HexTransmitOk(comX, packet->commandId);
    }
    else if (packet->funcId == 6 && packet->dataLength == 24)
    {
        g_Car.remote.carLocationX = CMD_HexReadFloat(packet, 0);
        g_Car.remote.carLocationY = CMD_HexReadFloat(packet, 4);
        float q0 = CMD_HexReadFloat(packet, 8);
        float q1 = CMD_HexReadFloat(packet, 12);
        float q2 = CMD_HexReadFloat(packet, 16);
        float q3 = CMD_HexReadFloat(packet, 20);
        g_Car.remote.carRoll = Math_Asin(2 * (q0 * q2 - q1 * q3));
        g_Car.remote.carPitch = Math_Atan2(2 * (q2 * q3 + q0 * q1), 1 - 2 * q1 * q1 - 2 * q2 * q2);
        g_Car.remote.carYaw = Math_Atan2(2 * (q1 * q2 + q0 * q3), 1 - 2 * q2 * q2 - 2 * q3 * q3);
        CMD_HexTransmitOk(comX, packet->commandId);
    }
    else if (packet->funcId == 7 && packet->dataLength == 16)
    {
        g_Car.remote.carLocationX = CMD_HexReadFloat(packet, 0);
        g_Car.remote.carLocationY = CMD_HexReadFloat(packet, 4);
        float q0 = CMD_HexReadInt16(packet, 8) / 32767.0f;
        float q1 = CMD_HexReadInt16(packet, 10) / 32767.0f;
        float q2 = CMD_HexReadInt16(packet, 12) / 32767.0f;
        float q3 = CMD_HexReadInt16(packet, 14) / 32767.0f;
        g_Car.remote.carRoll = Math_Asin(2 * (q0 * q2 - q1 * q3));
        g_Car.remote.carPitch = Math_Atan2(2 * (q2 * q3 + q0 * q1), 1 - 2 * q1 * q1 - 2 * q2 * q2);
        g_Car.remote.carYaw = Math_Atan2(2 * (q1 * q2 + q0 * q3), 1 - 2 * q2 * q2 - 2 * q3 * q3);
        CMD_HexTransmitOk(comX, packet->commandId);
    }
    else
    {
        g_Car.remote.useIntervalControl = 1;  // 解析失败，使用内部解算的数据
        CMD_HexTransmitError(comX, packet->commandId, CMD_HEX_ERROR_DATA_LENGTH);
    }
}

/**
 * @brief 小车设置内部解算命令
 * @param comX: 串口号
 * @param packet: HEX数据包
 * @note
 *
 * 格式: 设置内部解算命令
 *  功能码: 任意指定
 *  数据长度: 0字节
 *  数据: 无
 *  说明: 立即停止小车所有运动，不受模式限制
 */
static void CMD_HexSetIntervalControl(uint16_t comX, CmdHexDataPacket_T *packet)
{
    Car_SetMode(CAR_MODE_STOP);
    CMD_HexTransmitOk(comX, packet->commandId);
}

/**
 * @brief 小车虚拟按键设置（二进制）
 * @param comX: 串口号
 * @param packet: HEX数据包
 * @note
 *
 * 格式: 虚拟按键设置设置
 *  数据长度: 1字节
 *  数据: [按键号(1字节)]
 *  说明: 按键号: uint8_t, 当前设置的按键
 *  功能码: 0为按下事件；1为长按事件；2为连击事件；3为双击事件
 */
static void CMD_HexSetKey(uint16_t comX, CmdHexDataPacket_T *packet)
{
    /* 判断数据长度的合法性 */
    if (packet->dataLength < 1)
    {
        CMD_HexTransmitError(comX, packet->commandId, CMD_HEX_ERROR_DATA_LENGTH);
        return;
    }

    /* 获取按键事件 */
    KeyEvent_T ev = KEY_EVENT_CLICK;
    KeyEnum_T key = KEY_ENTER;

    /* 按到功能ID识别按键按下模式 */
    switch (packet->funcId)
    {
    case 0: ev = KEY_EVENT_CLICK; break;
    case 1: ev = KEY_EVENT_LONG_PRESS; break;
    case 2: ev = KEY_EVENT_REPEAT; break;
    case 3: ev = KEY_EVENT_DOUBLE_CLICK; break;
    default: CMD_HexTransmitError(comX, packet->commandId, CMD_HEX_ERROR_DATA_FORMAT); return;
    }

    /* 按到数据识别按键 */
    switch (packet->data[0])
    {
    case 0: key = KEY_ENTER; break;
    case 1: key = KEY_MODE; break;
    case 2: key = KEY_UP; break;
    case 3: key = KEY_DOWN; break;
    default: CMD_HexTransmitError(comX, packet->commandId, CMD_HEX_ERROR_DATA_FORMAT); return;
    }

    /* 按键适配识别 */
    Key_SetEvent(key, ev);
}

/**
 * @brief 串口数据接收命令（二进制）
 * @param comX: 串口号
 * @param packet: HEX数据包
 * @note
 *
 * 格式: 获取指定串口的接收数据
 *  功能码: 0x00
 *  数据长度: 1字节
 *  数据: [串口号(1字节)]
 *  说明:
 *    - 串口号: uint8_t, 0=com1, 1=com2, 2=com3, 3=com4
 *  示例: 0x01 表示从com2获取数据
 */
static void CMD_HexReceive(uint16_t comX, CmdHexDataPacket_T *packet)
{
    if (packet->dataLength < 1)
    {
        CMD_HexTransmitError(comX, packet->commandId, CMD_HEX_ERROR_DATA_LENGTH);
        return;
    }

    uint8_t targetCom = packet->data[0];
    if (targetCom >= COM_COUNT)
    {
        CMD_HexTransmitError(comX, packet->commandId, CMD_HEX_ERROR_DATA_FORMAT);
        return;
    }

    /* 准备响应数据包 */
    CmdHexDataPacket_T transmitPacket = {
        .commandId = packet->commandId,
        .funcId = packet->funcId,
        .data = s_HexSendArray,
        .dataLength = 0
    };

    /* 从预定义的接收数据缓冲区读取数据 */
    uint32_t dataLen = strnlen(g_CMD.receiveData[targetCom], sizeof(s_HexSendArray));
    if (dataLen > 0)
    {
        if (dataLen > sizeof(s_HexSendArray) - 1)
        {
            dataLen = sizeof(s_HexSendArray) - 1;
        }

        /* 复制数据到发送缓冲区 */
        Memory_Copy(s_HexSendArray, g_CMD.receiveData[targetCom], dataLen);
        s_HexSendArray[dataLen] = '\0';          /* 添加字符串结束符 */
        transmitPacket.dataLength = dataLen + 1; /* 包含结束符 */

        /* 清空接收缓冲区 */
        g_CMD.receiveData[targetCom][0] = 0;
    }
    else
    {
        /* 无数据时发送空响应 */
        s_HexSendArray[0] = '\0';
        transmitPacket.dataLength = 1;
    }

    CMD_HexTransmitData(comX, &transmitPacket);
}

/**
 * @brief 串口数据转发命令（二进制）
 * @param comX: 串口号
 * @param packet: HEX数据包
 * @note
 *
 * 格式: 发送数据到指定串口
 *  功能码: 0x00
 *  数据长度: 3+N字节
 *  数据: [串口号(1字节)] + [数据内容(N字节)]
 *  说明:
 *    - 串口号: uint8_t, 0=com1, 1=com2, 2=com3, 3=com4
 *    - 数据内容: 可变长度字符串
 *  示例: 0x01 48 65 6C 6C 6F
 *        表示: 向com2发送"Hello"
 */
static void CMD_HexSend(uint16_t comX, CmdHexDataPacket_T *packet)
{
    if (packet->dataLength < 1)
    {
        CMD_HexTransmitError(comX, packet->commandId, CMD_HEX_ERROR_DATA_LENGTH);
        return;
    }

    uint8_t targetCom = packet->data[0];
    if (targetCom >= COM_COUNT)
    {
        CMD_HexTransmitError(comX, packet->commandId, CMD_HEX_ERROR_DATA_FORMAT);
        return;
    }

    /* 发送数据到目标串口 */
    if (packet->dataLength > 1)
    {
        /* 确保数据以null结尾 */
        uint16_t sendLen = packet->dataLength - 1;
        if (sendLen > sizeof(s_HexSendArray) - 1)
        {
            sendLen = sizeof(s_HexSendArray) - 1;
        }

        Memory_Copy(s_HexSendArray, packet->data + 1, sendLen);
        s_HexSendArray[sendLen] = '\0';

        UART_SendString(targetCom, (char *)s_HexSendArray);

        /* 返回发送确认 */
        CmdHexDataPacket_T transmitPacket = {
            .commandId = packet->commandId,
            .funcId = packet->funcId,
            .data = s_HexSendArray,
            .dataLength = sendLen + 1 /* 包含结束符 */
        };

        CMD_HexTransmitData(comX, &transmitPacket);
    }
    else
    {
        /* 无数据内容 */
        s_HexSendArray[0] = '\0';
        CmdHexDataPacket_T transmitPacket = {
            .commandId = packet->commandId,
            .funcId = packet->funcId,
            .data = s_HexSendArray,
            .dataLength = 1
        };

        CMD_HexTransmitData(comX, &transmitPacket);
    }
}

/**
 * @brief 串口命令转发（二进制）
 * @param comX: 串口号
 * @param packet: HEX数据包
 * @note
 *
 * 格式: 转发命令到指定串口
 *  功能码: 0x00
 *  数据长度: 3+N字节
 *  数据: [目标串口号(1字节)] + [命令ID(1字节)] + [功能码(1字节)] + [数据内容(N字节)]
 *  说明:
 *    - 目标串口号: uint8_t, 0=com1, 1=com2, 2=com3, 3=com4
 *    - 命令ID: 要转发的命令ID
 *    - 功能码: 要转发的功能码
 *    - 数据内容: 可变长度数据
 *  示例: 0x01 0x10 0x01 48 65 6C 6C 6F
 *        表示: 向com2转发命令0x10，功能码0x01，数据"Hello"
 */
static void CMD_HexSendCmd(uint16_t comX, CmdHexDataPacket_T *packet)
{
    /* 检查最小数据长度：目标串口 + 命令ID + 功能码 */
    if (packet->dataLength < 3)
    {
        CMD_HexTransmitError(comX, packet->commandId, CMD_HEX_ERROR_DATA_LENGTH);
        return;
    }

    uint8_t targetCom = packet->data[0];
    uint8_t forwardCommandId = packet->data[1];
    uint8_t forwardFuncId = packet->data[2];

    if (targetCom >= COM_COUNT)
    {
        CMD_HexTransmitError(comX, packet->commandId, CMD_HEX_ERROR_DATA_FORMAT);
        return;
    }

    /* 构建转发的数据包 */
    CmdHexDataPacket_T forwardPacket;
    forwardPacket.commandId = forwardCommandId;
    forwardPacket.funcId = forwardFuncId;

    /* 处理数据内容 */
    if (packet->dataLength > 3)
    {
        /* 有数据内容需要转发 */
        uint16_t dataLen = packet->dataLength - 3;
        if (dataLen > sizeof(s_HexSendArray))
        {
            dataLen = sizeof(s_HexSendArray);
        }

        Memory_Copy(s_HexSendArray, packet->data + 3, dataLen);
        forwardPacket.data = s_HexSendArray;
        forwardPacket.dataLength = dataLen;
    }
    else
    {
        /* 无数据内容 */
        forwardPacket.data = NULL;
        forwardPacket.dataLength = 0;
    }

    /* 转发命令到目标串口 */
    CMD_SendHexData(targetCom, &forwardPacket);

    /* 构建确认响应包 */
    uint8_t responseBuffer[4]; /* 目标串口 + 命令ID + 功能码 + 状态 */
    responseBuffer[0] = targetCom;
    responseBuffer[1] = forwardCommandId;
    responseBuffer[2] = forwardFuncId;
    responseBuffer[3] = 0x00; /* 0x00表示转发成功 */

    CmdHexDataPacket_T transmitPacket = {
        .commandId = packet->commandId,
        .funcId = packet->funcId,
        .data = responseBuffer,
        .dataLength = 4
    };

    CMD_HexTransmitData(comX, &transmitPacket);
}

/**
 * @brief 设置灯光亮度HEX命令
 * @param comX: 串口号
 * @param packet: HEX数据包
 * @note
 *
 * 格式: 设置灯光亮度
 *  命令ID: 0x0E
 *  数据长度: 1字节
 *  功能ID: 0x00
 *  数据: [亮度值(1字节)]
 *  说明: 亮度值范围0-100，0为关闭，100为最亮
 *  示例: 0x64 表示设置亮度为100%
 */
static void CMD_HexSetLightPWM(uint16_t comX, CmdHexDataPacket_T *packet)
{
    if (packet->dataLength < 1)
    {
        CMD_HexTransmitError(comX, packet->commandId, CMD_HEX_ERROR_DATA_LENGTH);
        return;
    }

    uint8_t pwmValue = CMD_HexReadUint8(packet, 0);
    if (pwmValue > 100)
    {
        pwmValue = 100;
    }

    Light_SetBrightness(pwmValue);
    CMD_HexTransmitOk(comX, packet->commandId);
}

/**
 * @brief 设置舵机角度HEX命令
 * @param comX: 串口号
 * @param packet: HEX数据包
 * @note
 *
 * 格式1: 设置单个舵机角度
 *  命令ID: 0x0F
 *  数据长度: 2字节
 *  功能ID: 0x00
 *  数据: [舵机编号(1字节)] + [角度值(1字节)]
 *  说明:
 *    - 舵机编号: 0=SERVO_1, 1=SERVO_2
 *    - 角度值: 范围0-180度
 *  示例: 0x00 0x5A 表示设置SERVO_1舵机到90度
 *
 * 格式2: 设置两个舵机角度(float精度)
 *  命令ID: 0x0F
 *  数据长度: 8字节
 *  功能ID: 0x01
 *  数据: [舵机A角度(4字节)] + [舵机B角度(4字节)]
 *  说明: 使用float格式，精度更高
 *  示例: 0x42 0x34 0x00 0x00 0x42 0x48 0x00 0x00 表示设置A=45.0度, B=50.0度
 */
static void CMD_HexSetServoAngle(uint16_t comX, CmdHexDataPacket_T *packet)
{
    if (packet->funcId == 0 && packet->dataLength == 5)
    {
        /* 格式1: 单字节角度设置 */
        uint8_t servoId = CMD_HexReadUint8(packet, 0);
        uint8_t angle = CMD_HexReadFloat(packet, 1);

        if (servoId >= SERVO_COUNT)
        {
            CMD_HexTransmitError(comX, packet->commandId, CMD_HEX_ERROR_DATA_FORMAT);
            return;
        }

        if (angle > 180) angle = 180;

        if (!ArmControl_IsEnabled()) { Servo_SetAngle(servoId, angle); }
        CMD_HexTransmitOk(comX, packet->commandId);
    }
    else if (packet->funcId == 1 && packet->dataLength == 8)
    {
        /* 格式2: float精度角度设置 */
        float angleA = CMD_HexReadFloat(packet, 0);
        float angleB = CMD_HexReadFloat(packet, 4);

        if (angleA > 180.0f) angleA = 180.0f;
        if (angleA < 0.0f) angleA = 0.0f;
        if (angleB > 180.0f) angleB = 180.0f;
        if (angleB < 0.0f) angleB = 0.0f;

        if (!ArmControl_IsEnabled()) { Servo_SetAngle(SERVO_1, angleA); }
        if (!ArmControl_IsEnabled()) { Servo_SetAngle(SERVO_2, angleB); }
        CMD_HexTransmitOk(comX, packet->commandId);
    }
    else
    {
        CMD_HexTransmitError(comX, packet->commandId, CMD_HEX_ERROR_DATA_FORMAT);
    }
}

/**
 * @brief 设置条件取消标志HEX命令
 * @param comX: 串口号
 * @param packet: HEX数据包
 * @note
 *
 * 格式: 设置条件取消标志
 *  命令ID: 0x10
 *  数据长度: 1字节
 *  功能ID: 0x00
 *  数据: [标志值(1字节)]
 *  说明:
 *    - 0x00: 清除标志
 *    - 0x01: 设置标志
 *    - 其他值: 视为错误
 *  示例: 0x01 表示设置条件取消标志
 */
static void CMD_HexSetConditionCancelFlag(uint16_t comX, CmdHexDataPacket_T *packet)
{
    if (packet->dataLength < 1)
    {
        CMD_HexTransmitError(comX, packet->commandId, CMD_HEX_ERROR_DATA_LENGTH);
        return;
    }

    uint8_t flagValue = CMD_HexReadUint8(packet, 0);

    if (flagValue == 0x00 || flagValue == 0x01)
    {
        g_ConditionCancelFlag = flagValue;
        CMD_HexTransmitOk(comX, packet->commandId);
    }
    else
    {
        CMD_HexTransmitError(comX, packet->commandId, CMD_HEX_ERROR_DATA_FORMAT);
    }
}

/**
 * @brief 初始化二进制命令处理
 */
static void CMD_HexScreenshot(uint16_t comX, CmdHexDataPacket_T *packet)
{
    CmdHexDataPacket_T response = { 0 };
    response.commandId = packet->commandId;
    response.funcId = packet->funcId;
    response.data = s_HexSendArray;
    if (packet->funcId <= 1)
    {
        if (packet->dataLength != 0)
        {
            CMD_HexTransmitError(comX, packet->commandId, CMD_HEX_ERROR_DATA_LENGTH);
            return;
        }
        if (packet->funcId == 0) { GUI_CaptureRequest(); }
        response.dataLength = 22;
        response.data[0] = g_GUICaptureState;
        CMD_HexWriteUint16(&response, 1, g_GUICaptureWidth);
        CMD_HexWriteUint16(&response, 3, g_GUICaptureHeight);
        response.data[5] = 1; /* RGB565, high byte first, row-major top to bottom. */
        CMD_HexWriteUint32(&response, 6, g_GUICaptureBytes);
        CMD_HexWriteUint32(&response, 10, g_GUICaptureId);
        for (uint8_t i = 0; i < 8; i++)
        {
            response.data[14 + i] = (uint8_t)(g_GUICaptureTimestampMs >> (56 - 8 * i));
        }
    }
    else if (packet->funcId == 2)
    {
        if (packet->dataLength != 10)
        {
            CMD_HexTransmitError(comX, packet->commandId, CMD_HEX_ERROR_DATA_LENGTH);
            return;
        }
        uint32_t id = CMD_HexReadUint32(packet, 0);
        uint32_t offset = CMD_HexReadUint32(packet, 4);
        uint16_t count = CMD_HexReadUint16(packet, 8);
        if (g_GUICaptureState != 2 || id != g_GUICaptureId || count == 0 || count > GUI_CAPTURE_CHUNK_MAX || offset > g_GUICaptureBytes || count > g_GUICaptureBytes - offset)
        {
            CMD_HexTransmitError(comX, packet->commandId, CMD_HEX_ERROR_DATA_FORMAT);
            return;
        }
        response.dataLength = count + 8;
        CMD_HexWriteUint32(&response, 0, id);
        CMD_HexWriteUint32(&response, 4, offset);
        memcpy(response.data + 8, GUI_CAPTURE_BUFFER + offset, count);
    }
    else
    {
        CMD_HexTransmitError(comX, packet->commandId, CMD_HEX_ERROR_DATA_FORMAT);
        return;
    }
    CMD_HexTransmitData(comX, &response);
}

void CMD_HexProcInit(void)
{
    /* 分配命令表内存 */
    static const CmdHexNode_T s_HexCmdTable[] = {
        { 0x00, CMD_HexTest },                   /* 测试回传指令 */
        { 0x01, CMD_HexBeep },                   /* 蜂鸣器控制 */
        { 0x02, CMD_HexRtc },                    /* RTC设置 */
        { 0x03, CMD_HexMode },                   /* 模式切换 */
        { 0x04, CMD_HexShow },                   /* 数据输出控制 */
        { 0x05, CMD_HexCarControl },             /* 小车控制 */
        { 0x06, CMD_HexStop },                   /* 小车停止 */
        { 0x07, CMD_HexSetEulerAngle },          /* 小车欧拉角设置 */
        { 0x08, CMD_HexSetKey },                 /* 小车按键设置 */
        { 0x09, CMD_HexReceive },                /* 串口数据接收 */
        { 0x0A, CMD_HexSend },                   /* 串口数据发送 */
        { 0x0B, CMD_HexSendCmd },                /* 串口HEX命令发送 */
        { 0x0C, CMD_HexSetLocation },            /* 小车位置设置 */
        { 0x0D, CMD_HexSetIntervalControl },     /* 小车设置内部解算命令 */
        { 0x0E, CMD_HexSetLightPWM },            /* 设置灯光亮度 */
        { 0x0F, CMD_HexSetServoAngle },          /* 设置舵机角度 */
        { 0x12, CMD_FileTransfer },              /* Binary file transfer. */
        { 0x11, CMD_HexScreenshot },             /* Buffered screenshot request/status/chunk. */
        { 0x10, CMD_HexSetConditionCancelFlag }, /* 设置条件取消标志 */
    };

    /* 初始化命令表 */
    g_CMDHexTable.table = s_HexCmdTable;
    g_CMDHexTable.count = sizeof(s_HexCmdTable) / sizeof(s_HexCmdTable[0]);
}
