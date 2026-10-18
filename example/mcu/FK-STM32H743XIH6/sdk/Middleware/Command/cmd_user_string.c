#include "arm_control.h"
#include "cmd_string.h"
#include "cmd_config.h"
#include "cmd_port.h"

#include "bsp_system.h"
#include "file.h"
#include "storage.h"
#include "logger.h"
#include "car.h"
#include "imu.h"
#include "expression.h"
#include "condition_task.h"
#include "simple_task.h"

#define CMD_COMPARE_WITHOUT_CASE(str1, str2) String_CompareWithoutCase(str1, str2, sizeof(str2) - 1)
#define CMD_STR_ADD(STR, FUN, NUM, DETAIL)   assert(CMD_StrAdd(STR, FUN, NUM, DETAIL) == CMD_NO_ERROR)

extern uint8_t g_WirelessControlFlag;

/**
 * @brief 蜂鸣器控制命令
 * @param comX: 串口号
 * @param vars: 变量参数
 * @param count: 变量数量
 * @return 命令执行结果
 */
static uint16_t CMD_Beep(uint8_t comX, CmdStringDataPacket_T *packet)
{
    /* 1. 停止命令: beep off */
    if (CMD_COMPARE_WITHOUT_CASE(packet->varList[0].string, "off") == 0 && packet->count == 1)
    {
        Beep_Stop();
        UART_Printf(comX, "蜂鸣器已停止\r\n");
        return CMD_NO_ERROR;
    }

    /* 2. 蜂鸣命令: beep on_time off_time cycles */
    uint32_t onTime = packet->varList[0].unsignedNumber;
    uint32_t offTime = packet->varList[1].unsignedNumber;
    uint32_t cycle = packet->varList[2].unsignedNumber;
    Beep_Play(onTime, offTime, cycle);
    UART_Printf(comX, "蜂鸣器已设置: 开启%ums, 关闭%ums, 循环%u次\r\n", onTime, offTime, cycle);

    return CMD_NO_ERROR;
}

/**
 * @brief RTC控制命令
 * @param comX: 串口号
 * @param vars: 变量参数
 * @param count: 变量数量
 * @return 命令执行结果
 */
static uint16_t CMD_RTC(uint8_t comX, CmdStringDataPacket_T *packet)
{
    if (packet->count < 1)
    {
        // 无参数时显示当前时间
        RTC_ReadTime();
        UART_Printf(comX, "当前时间为: %04d年%02d月%02d日 %02d时%02d分%02d秒\r\n",
                    g_RTC_Data.year, g_RTC_Data.month, g_RTC_Data.day,
                    g_RTC_Data.hour, g_RTC_Data.minute, g_RTC_Data.second);
        return CMD_NO_ERROR;
    }

    uint32_t rtcTime = packet->varList[0].unsignedNumber;
    if (rtcTime < 946656000) /* 2000-01-01 UTC */
    {
        UART_Printf(comX, "错误: 时间戳必须 >= 946656000 (2000-01-01 UTC)!\r\n");
        return CMD_ERROR_PARAMENER_FORMAT;
    }

    if (rtcTime - RTC_GetTimestamp() > 60) /* 原始时间戳与实时时间差大于60秒，为设置时间时 */
    {
        RTC_SetTimestamp(rtcTime);
        UART_Printf(comX, "RTC时间已设置为: %u (Unix时间戳)\r\n", rtcTime);
    }
    else
    {
        UART_Printf(comX, "时间差小于60秒，未修改RTC时间\r\n");
    }

    return CMD_NO_ERROR;
}

/**
 * @brief 按键控制命令
 * @param comX: 串口号
 * @param vars: 变量参数
 * @param count: 变量数量
 * @return 命令执行结果
 */
static uint16_t CMD_SetKey(uint8_t comX, CmdStringDataPacket_T *packet)
{
    KeyEvent_T ev = KEY_EVENT_CLICK;
    char *key = packet->varList[0].string;
    const char *eventStr = "点击";

    /* Process additional parameter */
    if (packet->count > 1)
    {
        char *event = packet->varList[1].string;
        if (CMD_COMPARE_WITHOUT_CASE(event, "long") == 0)
        {
            ev = KEY_EVENT_LONG_PRESS;
            eventStr = "长按";
        }
        else if (CMD_COMPARE_WITHOUT_CASE(event, "repeat") == 0)
        {
            ev = KEY_EVENT_REPEAT;
            eventStr = "重复";
        }
        else if (CMD_COMPARE_WITHOUT_CASE(event, "double") == 0)
        {
            ev = KEY_EVENT_DOUBLE_CLICK;
            eventStr = "双击";
        }
        else
        {
            ev = KEY_EVENT_CLICK;
            eventStr = "点击";
        }
    }

    /* Handle event */
    if (CMD_COMPARE_WITHOUT_CASE(key, "enter") == 0)
    {
        Key_SetEvent(KEY_ENTER, ev);
        UART_Printf(comX, "已触发确认按钮%s事件\r\n", eventStr);
    }
    else if (CMD_COMPARE_WITHOUT_CASE(key, "mode") == 0)
    {
        Key_SetEvent(KEY_MODE, ev);
        UART_Printf(comX, "已触发模式按钮%s事件\r\n", eventStr);
    }
    else if (CMD_COMPARE_WITHOUT_CASE(key, "up") == 0)
    {
        Key_SetEvent(KEY_UP, ev);
        UART_Printf(comX, "已触发上按钮%s事件\r\n", eventStr);
    }
    else if (CMD_COMPARE_WITHOUT_CASE(key, "down") == 0)
    {
        Key_SetEvent(KEY_DOWN, ev);
        UART_Printf(comX, "已触发下按钮%s事件\r\n", eventStr);
    }
    else
    {
        UART_Printf(comX, "错误: 未知按钮 '%s'!\r\n", key);
        return CMD_ERROR_PARAMENER_FORMAT;
    }

    return CMD_NO_ERROR;
}

/**
 * @brief 内存使用率查询命令
 * @param comX: 串口号
 * @param vars: 变量参数
 * @param count: 变量数量
 * @return 命令执行结果
 */
static uint16_t CMD_GetMemory(uint8_t comX, CmdStringDataPacket_T *packet)
{
    float total, used;
    Memory_GetInfo(MP_ROM_IN, &total, &used);
    UART_Printf(comX, "内部ROM内存使用: %.2f/%.2f KB (%.2f%%)\r\n", used, total, used / total * 100);
#if SRAM_BANK >= 2
    Memory_GetInfo(MP_RAM_IN, &total, &used);
    UART_Printf(comX, "内部RAM内存使用: %.2f/%.2f KB (%.2f%%)\r\n", used, total, used / total * 100);
#endif
    Memory_GetInfo(MP_RAM_EX, &total, &used);
    UART_Printf(comX, "外部RAM内存使用: %.2f/%.2f MB (%.2f%%)\r\n", used / 1024.0f, total / 1024.0f, used / total * 100);

    return CMD_NO_ERROR;
}

/**
 * @brief 模式切换命令
 * @param comX: 串口号
 * @param vars: 变量参数
 * @param count: 变量数量
 * @return 命令执行结果
 */
static uint16_t CMD_Mode(uint8_t comX, CmdStringDataPacket_T *packet)
{
    char *mode = packet->varList[0].string;

    if (CMD_COMPARE_WITHOUT_CASE(mode, "stop") == 0)
    {
        Car_SetMode(CAR_MODE_STOP);
        UART_Printf(comX, "已切换到停止模式\r\n");
    }
    else if (CMD_COMPARE_WITHOUT_CASE(mode, "remote") == 0)
    {
        Car_SetMode(CAR_MODE_REMOTE);
        UART_Printf(comX, "已切换到遥控模式\r\n");
    }
    else if (CMD_COMPARE_WITHOUT_CASE(mode, "avoidance") == 0)
    {
        Car_SetMode(CAR_MODE_REMOTE);
        UART_Printf(comX, "已切换到避障模式\r\n");
    }
    else if (CMD_COMPARE_WITHOUT_CASE(mode, "line") == 0)
    {
        Car_SetMode(CAR_MODE_REMOTE);
        UART_Printf(comX, "已切换到巡线模式\r\n");
    }
    else
    {
        UART_Printf(comX, "错误: 无效模式 '%s'\r\n", mode);
        return CMD_ERROR_PARAMENER_FORMAT;
    }

    return CMD_NO_ERROR;
}

/**
 * @brief 数据输出控制命令
 * @param comX: 串口号
 * @param vars: 变量参数
 * @param count: 变量数量
 * @return 命令执行结果
 */
static uint16_t CMD_Show(uint8_t comX, CmdStringDataPacket_T *packet)
{
    /* Process multiple parameters */
    char *option = packet->varList[0].string;

    if (CMD_COMPARE_WITHOUT_CASE(option, "off") == 0)
    {
        g_CMD.dataOutputShowFlag[comX] = 0;
        UART_Printf(comX, "数据输出已禁用\r\n");
    }
    else if (CMD_COMPARE_WITHOUT_CASE(option, "euler") == 0)
    {
        g_CMD.dataOutputShowFlag[comX] |= 0x01;
        UART_Printf(comX, "欧拉角数据输出已启用\r\n");
    }
    else if (CMD_COMPARE_WITHOUT_CASE(option, "mpu") == 0)
    {
        g_CMD.dataOutputShowFlag[comX] |= 0x02;
        UART_Printf(comX, "MPU原始数据输出已启用\r\n");
    }
    else if (CMD_COMPARE_WITHOUT_CASE(option, "car") == 0)
    {
        g_CMD.dataOutputShowFlag[comX] |= 0x04;
        UART_Printf(comX, "小车数据输出已启用\r\n");
    }
    else if (CMD_COMPARE_WITHOUT_CASE(option, "battery") == 0)
    {
        g_CMD.dataOutputShowFlag[comX] |= 0x08;
        UART_Printf(comX, "电池数据输出已启用\r\n");
    }
    else if (CMD_COMPARE_WITHOUT_CASE(option, "state") == 0)
    {
        const char *modeStr = "未知";
        switch (g_Car.state)
        {
        case CAR_MODE_STOP: modeStr = "停止模式"; break;
        case CAR_MODE_REMOTE: modeStr = "遥控模式"; break;
        case CAR_MODE_AVOIDANCE: modeStr = "避障模式"; break;
        default: modeStr = "未知模式"; break;
        }

        UART_Printf(comX, "当前模式: %s\r\n", modeStr);
    }
    else
    {
        UART_Printf(comX, "错误: 未知选项 '%s'\r\n", option);
        return CMD_ERROR_PARAMENER_FORMAT;
    }

    return CMD_NO_ERROR;
}

/**
 * @brief 遥控命令
 * @param comX: 串口号
 * @param vars: 变量参数
 * @param count: 变量数量
 * @return 命令执行结果
 */
static uint16_t CMD_Remote(uint8_t comX, CmdStringDataPacket_T *packet)
{
    if (g_WirelessControlFlag > 0)
    {
        UART_Printf(comX, "使用遥控器遥控时，不能使用串口遥控设置遥控内容\r\n");
        return CMD_ERROR_PARAMENER_FORMAT;
    }

    if (g_Car.state != CAR_MODE_REMOTE)
    {
        Car_SetMode(CAR_MODE_REMOTE);
    }

    if (CMD_COMPARE_WITHOUT_CASE(packet->varList[0].string, "stop") == 0)
    {
        CarControl_Stop(&g_Car.remote);
        UART_Printf(comX, "遥控小车已停止\r\n");
        return CMD_NO_ERROR;
    }
    else
    {
        float speedX = packet->varList[0].floatNumber;
        float speedT = packet->varList[1].floatNumber;
        Car_SetSpeed(speedX, speedT);
        UART_Printf(comX, "速度已设置: 前进=%.2f m/s, 旋转=%.2f deg/s\r\n", speedX, speedT);
        return CMD_NO_ERROR;
    }
}

/**
 * @brief 小车停止命令
 * @param comX: 串口号
 * @param vars: 变量参数
 * @param count: 变量数量
 * @return 命令执行结果
 */
static uint16_t CMD_Stop(uint8_t comX, CmdStringDataPacket_T *packet)
{
    Car_SetMode(CAR_MODE_STOP);
    UART_Printf(comX, "小车已停止\r\n");
    return CMD_NO_ERROR;
}

/**
 * @brief 统一的设置命令处理器
 * @param comX: 串口编号
 * @param packet: 数据包
 * @return 执行结果
 * 用法: set <子命令> [参数1] [参数2] ...
 * 示例:
 *   set yaw 90.5                 # 设置偏航角
 *   set euler 10.2 0.5 90.0     # 设置欧拉角
 *   set location 1.5 2.3        # 设置位置
 *   set rtc 1735689600          # 设置RTC时间
 *   set inter_ctrl              # 启用内部控制
 *   set light 50                # 设置灯光亮度
 *   set servo A 90              # 设置舵机角度
 *   set help                    # 显示帮助
 */
static uint16_t CMD_Set(uint8_t comX, CmdStringDataPacket_T *packet)
{
    /* 检查是否有子命令参数 */
    if (packet->count < 1)
    {
        UART_SendString(comX, "错误: 需要子命令。输入 'set help' 查看用法。\r\n");
        return CMD_ERROR_PARAMENER_FORMAT;
    }

    char *subCmd = packet->varList[0].string;

    /* 处理 'help' 子命令 */
    if (CMD_COMPARE_WITHOUT_CASE(subCmd, "help") == 0 || CMD_COMPARE_WITHOUT_CASE(subCmd, "?") == 0)
    {
        const char *helpText =
            "设置命令使用说明:\r\n"
            "================ 姿态控制 ================\r\n"
            "  set yaw <偏航角>                  - 设置当前偏航角(度)\r\n"
            "  set euler <俯仰> <横滚> <偏航>   - 设置欧拉角(度)\r\n"
            "  set location <X> <Y>             - 设置小车位置(m)\r\n"
            "  set inter_ctrl                   - 启用内部位置解算控制\r\n"
            "================ 时间系统 ================\r\n"
            "  set rtc <Unix时间戳>             - 设置RTC时间\r\n"
            "     注意: 时间戳 >= 946627200 (2000-01-01 UTC)!\r\n"
            "================ 硬件控制 ================\r\n"
            "  set light <PWM值>                - 设置灯光亮度(0-100)\r\n"
            "  set servo <舵机号> <角度>        - 设置舵机角度(0-180度)\r\n"
            "      舵机号: A, B, C... (对应 SERVO_A, SERVO_B...)\r\n"
            "================ 系统帮助 ================\r\n"
            "  set help                         - 显示此帮助信息\r\n"
            "示例:\r\n"
            "  set yaw 180                     # 设置偏航角180度\r\n"
            "  set location 2.5 1.8            # 设置位置X=2.5m, Y=1.8m\r\n"
            "  set rtc 1735689600              # 设置时间为2024-12-31 00:00:00\r\n"
            "  set light 75                    # 设置灯光亮度75%\r\n"
            "  set servo B 120                 # 设置B舵机为120度\r\n";
        UART_SendString(comX, helpText);
        return CMD_NO_ERROR;
    }

    /* 处理 'yaw' 子命令 (设置偏航角) */
    if (CMD_COMPARE_WITHOUT_CASE(subCmd, "yaw") == 0)
    {
        if (packet->count < 2)
        {
            UART_SendString(comX, "错误: 需要偏航角参数。用法: set yaw <偏航角>\r\n");
            return CMD_ERROR_PARAMENER_FORMAT;
        }

        g_Car.remote.useIntervalControl = 0;  // 取消使用内部控制
        g_Car.remote.carPitch = g_IMU.Pitch;
        g_Car.remote.carRoll = g_IMU.Roll;
        g_Car.remote.carYaw = packet->varList[1].floatNumber;
        UART_Printf(comX, "偏航角已设为: %.2f 度\r\n", packet->varList[1].floatNumber);
        return CMD_NO_ERROR;
    }

    /* 处理 'euler' 子命令 (设置欧拉角) */
    if (CMD_COMPARE_WITHOUT_CASE(subCmd, "euler") == 0)
    {
        if (packet->count < 4)
        {
            UART_SendString(comX, "错误: 需要3个角度参数。用法: set euler <俯仰> <横滚> <偏航>\r\n");
            return CMD_ERROR_PARAMENER_FORMAT;
        }

        g_Car.remote.useIntervalControl = 0;  // 取消使用内部控制
        g_Car.remote.carPitch = packet->varList[1].floatNumber;
        g_Car.remote.carRoll = packet->varList[2].floatNumber;
        g_Car.remote.carYaw = packet->varList[3].floatNumber;
        UART_Printf(comX, "欧拉角已设为: 俯仰=%.2f, 横滚=%.2f, 偏航=%.2f 度\r\n",
                    packet->varList[1].floatNumber, packet->varList[2].floatNumber, packet->varList[3].floatNumber);
        return CMD_NO_ERROR;
    }

    /* 处理 'location' 子命令 (设置位置) */
    if (CMD_COMPARE_WITHOUT_CASE(subCmd, "location") == 0 || CMD_COMPARE_WITHOUT_CASE(subCmd, "pos") == 0)
    {
        if (packet->count < 3)
        {
            UART_SendString(comX, "错误: 需要X,Y坐标参数。用法: set location <X> <Y>\r\n");
            return CMD_ERROR_PARAMENER_FORMAT;
        }

        g_Car.remote.useIntervalControl = 0;  // 取消使用内部控制
        g_Car.remote.carLocationX = packet->varList[1].floatNumber;
        g_Car.remote.carLocationY = packet->varList[2].floatNumber;
        UART_Printf(comX, "小车位置已设为: X=%.2fm, Y=%.2fm\r\n",
                    packet->varList[1].floatNumber, packet->varList[2].floatNumber);
        return CMD_NO_ERROR;
    }

    /* 处理 'rtc' 子命令 (设置RTC时间) */
    if (CMD_COMPARE_WITHOUT_CASE(subCmd, "rtc") == 0 || CMD_COMPARE_WITHOUT_CASE(subCmd, "time") == 0)
    {
        if (packet->count < 2)
        {
            UART_SendString(comX, "错误: 需要RTC时间戳。用法: rtc <Unix时间戳>\r\n");
            return CMD_ERROR_PARAMENER_FORMAT;
        }

        uint32_t rtcTime = packet->varList[1].unsignedNumber;
        if (rtcTime < 946656000) /* 2000-01-01 UTC */
        {
            UART_Printf(comX, "错误: 时间戳必须 >= 946656000 (2000-01-01 UTC)!\r\n");
            return CMD_ERROR_PARAMENER_FORMAT;
        }

        if (rtcTime - RTC_GetTimestamp() > 60) /* 原始时间戳与实时时间差大于60秒，为设置时间时 */
        {
            RTC_SetTimestamp(rtcTime);
            UART_Printf(comX, "RTC时间已设置为: %u (Unix时间戳)\r\n", rtcTime);
        }
        else
        {
            UART_Printf(comX, "时间差小于60秒，未修改RTC时间\r\n");
        }

        return CMD_NO_ERROR;
    }

    /* 处理 'inter_ctrl' 子命令 (设置内部控制) - 特别注意: 这是一个单词的子命令 */
    if (CMD_COMPARE_WITHOUT_CASE(subCmd, "inter_ctrl") == 0)
    {
        g_Car.remote.useIntervalControl = 1;  // 启用使用内部控制
        UART_Printf(comX, "已设置为内部控制模式\r\n");
        return CMD_NO_ERROR;
    }

    /* 处理 'light' 子命令 (设置灯光) */
    if (CMD_COMPARE_WITHOUT_CASE(subCmd, "light") == 0 || CMD_COMPARE_WITHOUT_CASE(subCmd, "led") == 0)
    {
        if (packet->count < 2)
        {
            UART_SendString(comX, "错误: 需要PWM值参数。用法: set light <PWM值>\r\n");
            return CMD_ERROR_PARAMENER_FORMAT;
        }

        int32_t pwmValue = packet->varList[1].intNumber;
        if (pwmValue > 100) { pwmValue = 100; }
        if (pwmValue < 0) { pwmValue = 0; }
        Light_SetBrightness(pwmValue);
        UART_Printf(comX, "灯光亮度已设为: %d%%\r\n", pwmValue);
        return CMD_NO_ERROR;
    }

    /* 处理 'servo' 子命令 (设置舵机) */
    if (CMD_COMPARE_WITHOUT_CASE(subCmd, "servo") == 0)
    {
        if (packet->count < 3)
        {
            UART_SendString(comX, "错误: 需要舵机编号和角度参数。用法: set servo <舵机号> <角度>\r\n");
            UART_SendString(comX, "舵机号: 数字 (1-8)\r\n");
            return CMD_ERROR_PARAMENER_FORMAT;
        }

        int32_t angle = packet->varList[2].floatNumber;
        if (angle > 180) { angle = 180; }
        if (angle < 0) { angle = 0; }

        char servoChar = packet->varList[1].string[0];
        int32_t selectServo;

        if (servoChar >= '1' && servoChar <= '8')
        {
            selectServo = servoChar - '1' + SERVO_1;
        }
        else
        {
            UART_Printf(comX, "错误: 舵机号必须是数字 (1-8)\r\n");
            return CMD_ERROR_PARAMENER_FORMAT;
        }

        if (!ArmControl_IsEnabled()) { Servo_SetAngle(selectServo, angle); }
        UART_Printf(comX, "舵机 %c 角度已设为: %d 度\r\n", servoChar, angle);
        return CMD_NO_ERROR;
    }

    /* 未知子命令 */
    UART_Printf(comX, "错误: 未知的子命令 '%s'。输入 'set help' 查看用法。\r\n", subCmd);
    return CMD_ERROR_PARAMENER_FORMAT;
}

/**
 * @brief 从其他串口设备接收数据
 * @param comX: 串口号
 * @param vars: 变量参数
 * @param count: 变量数量
 * @return 命令执行结果
 */
static uint16_t CMD_Receive(uint8_t comX, CmdStringDataPacket_T *packet)
{
    /* 解析串口号 */
    char *comStr = packet->varList[0].string;
    uint8_t targetCom;

    if (CMD_COMPARE_WITHOUT_CASE(comStr, "com1") == 0)
    {
        targetCom = COM1;
    }
    else if (CMD_COMPARE_WITHOUT_CASE(comStr, "com2") == 0)
    {
        targetCom = COM2;
    }
    else if (CMD_COMPARE_WITHOUT_CASE(comStr, "com3") == 0)
    {
        targetCom = COM3;
    }
    else if (CMD_COMPARE_WITHOUT_CASE(comStr, "com4") == 0)
    {
        targetCom = COM4;
    }
    else if (CMD_COMPARE_WITHOUT_CASE(comStr, "com5") == 0)
    {
        targetCom = COM5;
    }
    else if (CMD_COMPARE_WITHOUT_CASE(comStr, "com6") == 0)
    {
        targetCom = COM6;
    }
    else if (CMD_COMPARE_WITHOUT_CASE(comStr, "com7") == 0)
    {
        targetCom = COM7;
    }
    else
    {
        UART_Printf(comX, "错误: 未知串口 '%s'\n", comStr);
        return CMD_ERROR_PARAMENER_FORMAT;
    }

    /* 从预定义的接收数据缓冲区读取数据 */
    if (strnlen(g_CMD.receiveData[targetCom], UART_BUFFER_SIZE) > 0)
    {
        UART_Printf(comX, "从 %s 接收到的数据: %s\n", comStr, g_CMD.receiveData[targetCom]);
        g_CMD.receiveData[targetCom][0] = 0;
    }
    else
    {
        UART_Printf(comX, "从 %s 未接收到数据\n", comStr);
    }

    return CMD_NO_ERROR;
}

/**
 * @brief 发送命令到其他串口设备
 * @param comX: 串口号
 * @param vars: 变量参数
 * @param count: 变量数量
 * @return 命令执行结果
 */
static uint16_t CMD_Send(uint8_t comX, CmdStringDataPacket_T *packet)
{
    /* 解析串口号 */
    char *comStr = packet->varList[0].string;
    uint8_t targetCom;

    if (CMD_COMPARE_WITHOUT_CASE(comStr, "com1") == 0)
    {
        targetCom = COM1;
    }
    else if (CMD_COMPARE_WITHOUT_CASE(comStr, "com2") == 0)
    {
        targetCom = COM2;
    }
    else if (CMD_COMPARE_WITHOUT_CASE(comStr, "com3") == 0)
    {
        targetCom = COM3;
    }
    else if (CMD_COMPARE_WITHOUT_CASE(comStr, "com4") == 0)
    {
        targetCom = COM4;
    }
    else if (CMD_COMPARE_WITHOUT_CASE(comStr, "com5") == 0)
    {
        targetCom = COM5;
    }
    else if (CMD_COMPARE_WITHOUT_CASE(comStr, "com6") == 0)
    {
        targetCom = COM6;
    }
    else if (CMD_COMPARE_WITHOUT_CASE(comStr, "com7") == 0)
    {
        targetCom = COM7;
    }
    else
    {
        UART_Printf(comX, "错误: 未知串口 '%s'\r\n", comStr);
        return CMD_ERROR_PARAMENER_FORMAT;
    }

    /* 发送数据 */
    UART_SendString(targetCom, packet->varList[1].string);
    UART_Printf(comX, "已向 %s 发送数据: %s\r\n", comStr, packet->varList[1].string);

    return CMD_NO_ERROR;
}

/**
 * @brief 任务列表命令
 * @param comX: 串口号
 * @param packet: 命令参数包
 * @return 命令执行结果
 */
static uint16_t CMD_TaskInfo(uint8_t comX, CmdStringDataPacket_T *packet)
{
    const uint32_t TASK_STRING_LEN = 2048;
    char *tempBuffer = Memory_Malloc(MP_RAM_EX, TASK_STRING_LEN);

    if (tempBuffer == NULL)
    {
        UART_SendString(comX, "错误: 内存分配失败，无法获取任务信息\r\n");
        return CMD_ERROR_EXCUTE;
    }
    Task_GetInfo(tempBuffer, TASK_STRING_LEN);
    UART_Printf(comX, "%s\r\n", tempBuffer);
    Memory_Free(MP_RAM_EX, tempBuffer);
    return CMD_NO_ERROR;
}

/**
 * @brief 重启命令
 * @param comX: 串口号
 * @param vars: 变量参数
 * @param count: 变量数量
 * @return 命令执行结果
 */
static uint16_t CMD_Reboot(uint8_t comX, CmdStringDataPacket_T *packet)
{
    UART_SendStringImm(comX, "系统已重启，将重新初始化系统...\r\n");
    HAL_Delay(1000);
    HAL_NVIC_SystemReset();
    return CMD_NO_ERROR;
}

/**
 * @brief 陀螺仪姿态重置命令
 * @param comX: 串口号
 * @param vars: 变量参数
 * @param count: 变量数量
 * @return 命令执行结果
 */
static uint16_t CMD_ImuResetOrientation(uint8_t comX, CmdStringDataPacket_T *packet)
{
    UART_SendString(comX, "陀螺仪已姿态清零...\r\n");
    IMU_ResetOrientation();
    return CMD_NO_ERROR;
}

/**
 * @brief 表达式测试
 * @param comX: 串口号
 * @param vars: 变量参数
 * @param count: 变量数量
 * @return 命令执行结果
 */
/**
 * @brief 统一的表达式系统命令处理器 (调整后版本)
 * @param comX: 串口编号
 * @param packet: 数据包
 * @return 执行结果
 * 用法: exp <子命令> [参数1] [参数2] ...
 * 示例:
 *   exp test "a+b*2"         # 测试表达式
 *   exp set myVar 3.14       # 设置变量
 *   exp show var             # 显示所有变量
 *   exp show func            # 显示所有函数
 *   exp help                 # 显示帮助
 */
static uint16_t CMD_Exp(uint8_t comX, CmdStringDataPacket_T *packet)
{
    /* 检查是否有子命令参数 */
    if (packet->count < 1)
    {
        UART_SendString(comX, "错误: 需要子命令。输入 'exp help' 查看用法。\r\n");
        return CMD_ERROR_PARAMENER_FORMAT;
    }

    char *subCmd = packet->varList[0].string;

    /* 处理 'help' 子命令 */
    if (CMD_COMPARE_WITHOUT_CASE(subCmd, "help") == 0 || CMD_COMPARE_WITHOUT_CASE(subCmd, "?") == 0)
    {
        const char *helpText =
            "表达式系统命令使用说明:\r\n"
            "  exp test <表达式字符串>            - 解析并计算表达式，显示语法树和结果\r\n"
            "  exp set <变量名> <数值>            - 设置表达式变量的值\r\n"
            "  exp show var                      - 显示当前所有已定义的变量及其值\r\n"
            "  exp show func                     - 显示表达式系统支持的所有函数列表\r\n"
            "  exp help                           - 显示此帮助信息\r\n"
            "示例:\r\n"
            "  exp test \"sin(30*pi/180) + 5\"     # 测试数学表达式\r\n"
            "  exp set velocity 2.5              # 设置变量velocity为2.5\r\n"
            "  exp show var                      # 列出所有变量\r\n"
            "  exp show func                     # 列出所有函数\r\n";
        UART_SendString(comX, helpText);
        return CMD_NO_ERROR;
    }

    /* 处理 'test' 子命令 (测试表达式) */
    if (CMD_COMPARE_WITHOUT_CASE(subCmd, "test") == 0)
    {
        if (packet->count < 2)
        {
            UART_SendString(comX, "错误: 需要表达式字符串。用法: exp test <表达式>\r\n");
            return CMD_ERROR_PARAMENER_FORMAT;
        }

        const uint32_t TASK_STRING_LEN = 2048;
        uint8_t tempFlag = 0;
        char *tempBuffer = Memory_Malloc(MP_RAM_EX, TASK_STRING_LEN);

        if (tempBuffer == NULL)
        {
            UART_SendString(comX, "错误: 内存不足，无法获取表达式信息\r\n");
            return CMD_ERROR_EXCUTE;
        }

        const char *exprStr = packet->varList[1].string;
        uint64_t startTick = Timer_GetTicks();
        Node_T *root = Expr_EvaluateExpression(exprStr, &tempFlag, tempBuffer, TASK_STRING_LEN);
        uint32_t diffUs = Timer_GetDiffUs(startTick);

        UART_Printf(comX, "测试表达式: \"%s\"\r\n", exprStr);
        UART_Printf(comX, "解析耗时=%dus\r\n", diffUs);
        UART_SendString(comX, tempBuffer);  // 输出评估结果

        if (!tempFlag)
        {
            UART_SendString(comX, "表达式语法树:\r\n");
            Expr_PrintTree(root, 0, tempBuffer, TASK_STRING_LEN);
            UART_Printf(comX, "%s\r\n", tempBuffer);
        }

        Memory_Free(MP_RAM_EX, tempBuffer);
        return (tempFlag) ? CMD_ERROR_EXCUTE : CMD_NO_ERROR;
    }

    /* 处理 'set' 子命令 (设置变量) */
    if (CMD_COMPARE_WITHOUT_CASE(subCmd, "set") == 0)
    {
        if (packet->count < 3)
        {
            UART_SendString(comX, "错误: 参数不足。用法: exp set <变量名> <数值>\r\n");
            return CMD_ERROR_PARAMENER_FORMAT;
        }

        const char *varName = packet->varList[1].string;
        float value = packet->varList[2].floatNumber;

        if (!Expr_SetVariable(varName, value))
        {
            UART_Printf(comX, "变量 %s 已设置为 %.2f\r\n", varName, value);
            return CMD_NO_ERROR;
        }
        else
        {
            UART_Printf(comX, "错误: 设置变量 %s 失败！\r\n", varName);
            return CMD_ERROR_EXCUTE;
        }
    }

    /* 处理 'show' 子命令 (显示变量或函数) */
    if (CMD_COMPARE_WITHOUT_CASE(subCmd, "show") == 0)
    {
        if (packet->count < 2)
        {
            UART_SendString(comX, "错误: 需要指定显示内容。用法: exp show <var/func>\r\n");
            UART_SendString(comX, "  exp show var   - 显示所有变量\r\n");
            UART_SendString(comX, "  exp show func  - 显示所有函数\r\n");
            return CMD_ERROR_PARAMENER_FORMAT;
        }

        const uint32_t TASK_STRING_LEN = 2048;
        char *tempBuffer = Memory_Malloc(MP_RAM_EX, TASK_STRING_LEN);

        if (tempBuffer == NULL)
        {
            UART_SendString(comX, "错误: 内存不足，无法获取信息\r\n");
            return CMD_ERROR_EXCUTE;
        }

        char *showWhat = packet->varList[1].string;

        /* 显示变量 */
        if (CMD_COMPARE_WITHOUT_CASE(showWhat, "var") == 0 || CMD_COMPARE_WITHOUT_CASE(showWhat, "vars") == 0)
        {
            Expr_PrintVariables(tempBuffer, TASK_STRING_LEN);
            UART_SendString(comX, tempBuffer);
            Memory_Free(MP_RAM_EX, tempBuffer);
            return CMD_NO_ERROR;
        }
        /* 显示函数 */
        else if (CMD_COMPARE_WITHOUT_CASE(showWhat, "func") == 0 || CMD_COMPARE_WITHOUT_CASE(showWhat, "funcs") == 0)
        {
            Expr_PrintFunctions(tempBuffer, TASK_STRING_LEN);
            UART_SendString(comX, tempBuffer);
            Memory_Free(MP_RAM_EX, tempBuffer);
            return CMD_NO_ERROR;
        }
        else
        {
            UART_Printf(comX, "错误: 未知的显示选项 '%s'。使用 'var' 或 'func'。\r\n", showWhat);
            Memory_Free(MP_RAM_EX, tempBuffer);
            return CMD_ERROR_PARAMENER_FORMAT;
        }
    }

    /* 未知子命令 */
    UART_Printf(comX, "错误: 未知的子命令 '%s'。输入 'exp help' 查看用法。\r\n", subCmd);
    return CMD_ERROR_PARAMENER_FORMAT;
}

/**
 * @brief 统一的条件域命令处理器
 * @param comX: 串口编号
 * @param packet: 数据包
 * @return 执行结果
 * 用法: cond <子命令> [参数1] [参数2] ...
 * 示例:
 *   cond set 0 "a<1" "b=2"        # 配置ID为0的条件域
 *   cond show                     # 显示所有条件域状态
 *   cond del 0                    # 删除ID为0的条件域
 *   cond help                    # 显示帮助
 */
static uint16_t CMD_Cond(uint8_t comX, CmdStringDataPacket_T *packet)
{
    /* 检查是否有子命令参数 */
    if (packet->count < 1)
    {
        UART_SendString(comX, "错误: 需要子命令。输入 'cond help' 查看用法。\r\n");
        return CMD_ERROR_PARAMENER_FORMAT;
    }

    char *subCmd = packet->varList[0].string;

    /* 处理 'help' 子命令 */
    if (CMD_COMPARE_WITHOUT_CASE(subCmd, "help") == 0 || CMD_COMPARE_WITHOUT_CASE(subCmd, "?") == 0)
    {
        const char *helpText =
            "条件域命令使用说明:\r\n"
            "  cond set <ID> <条件表达式> <触发赋值表达式> [持续时间]  - 配置条件域\r\n"
            "  cond del <ID>                                           - 删除/禁用条件域\r\n"
            "  cond show                                               - 显示所有条件域状态\r\n"
            "  cond test <ID>                                          - 测试指定条件域触发状态\r\n"
            "  cond reset <ID>                                         - 重置指定条件域状态\r\n"
            "  cond reset all                                          - 重置所有条件域\r\n"
            "  cond cancel <0/1>                                       - 设置遥控模式下条件域报警取消(1:是, 0:否)\r\n"
            "  cond help                                               - 显示此帮助信息\r\n"
            "示例:\r\n"
            "  cond set 0 \"distance<0.5\" \"speed=0\" 5000  # 5秒后自动销毁\r\n"
            "  cond test 0\r\n"
            "  cond del 0\r\n";
        UART_SendString(comX, helpText);
        return CMD_NO_ERROR;
    }

    /* 处理 'set' 子命令 (配置条件域) */
    if (CMD_COMPARE_WITHOUT_CASE(subCmd, "set") == 0)
    {
        if (packet->count < 4)
        {
            UART_SendString(comX, "错误: 参数不足。用法: cond set <ID> <条件表达式> <触发赋值表达式> [持续时间]\r\n");
            return CMD_ERROR_PARAMENER_FORMAT;
        }

        uint8_t domainId = (uint8_t)packet->varList[1].floatNumber;
        if (domainId >= CONDITION_DOMAIN_COUNT)
        {
            UART_Printf(comX, "错误: ID需在0-%d之间\r\n", CONDITION_DOMAIN_COUNT - 1);
            return CMD_ERROR_EXCUTE;
        }

        const char *condExpr = packet->varList[2].string;
        const char *triggerExpr = packet->varList[3].string;
        uint32_t durationMs = CONDITION_DOMAIN_INFINITE;

        if (packet->count >= 5)
        {
            durationMs = (uint32_t)packet->varList[4].floatNumber;
        }

        if (Condition_Configure(domainId, condExpr, triggerExpr, durationMs) && Condition_Enable(domainId))
        {
            UART_Printf(comX, "条件域 %d 配置成功: \"%s\" -> \"%s\"\r\n", domainId, condExpr, triggerExpr);
            if (durationMs != CONDITION_DOMAIN_INFINITE)
            {
                UART_Printf(comX, "销毁时间: %lu ms\r\n", durationMs);
            }
            return CMD_NO_ERROR;
        }
        UART_Printf(comX, "错误: 条件域 %d 配置失败\r\n", domainId);
        return CMD_ERROR_EXCUTE;
    }

    /* 处理 'del' 子命令 (删除/禁用条件域) */
    if (CMD_COMPARE_WITHOUT_CASE(subCmd, "del") == 0 || CMD_COMPARE_WITHOUT_CASE(subCmd, "disable") == 0)
    {
        if (packet->count < 2)
        {
            UART_SendString(comX, "错误: 需要条件域ID。用法: cond del <ID>\r\n");
            return CMD_ERROR_PARAMENER_FORMAT;
        }
        uint8_t domainId = (uint8_t)packet->varList[1].floatNumber;
        if (domainId >= CONDITION_DOMAIN_COUNT)
        {
            UART_Printf(comX, "错误: ID需在0-%d之间\r\n", CONDITION_DOMAIN_COUNT - 1);
            return CMD_ERROR_EXCUTE;
        }
        if (Condition_Disable(domainId))
        {
            UART_Printf(comX, "条件域 %d 已删除/禁用\r\n", domainId);
            return CMD_NO_ERROR;
        }
        UART_Printf(comX, "条件域 %d 删除失败或未配置\r\n", domainId);
        return CMD_ERROR_EXCUTE;
    }

    /* 处理 'show' 子命令 (显示状态) */
    if (CMD_COMPARE_WITHOUT_CASE(subCmd, "show") == 0 || CMD_COMPARE_WITHOUT_CASE(subCmd, "status") == 0)
    {
        const uint32_t BUFFER_SIZE = 2048;
        char *tempBuffer = Memory_Malloc(MP_RAM_EX, BUFFER_SIZE);
        if (tempBuffer == NULL)
        {
            UART_SendString(comX, "错误: 内存不足\r\n");
            return CMD_ERROR_EXCUTE;
        }
        Condition_PrintStatus(tempBuffer, BUFFER_SIZE);
        UART_SendString(comX, tempBuffer);
        Memory_Free(MP_RAM_EX, tempBuffer);
        return CMD_NO_ERROR;
    }

    /* 处理 'test' 子命令 (测试触发) */
    if (CMD_COMPARE_WITHOUT_CASE(subCmd, "test") == 0)
    {
        if (packet->count < 2)
        {
            UART_SendString(comX, "错误: 需要条件域ID。用法: cond test <ID>\r\n");
            return CMD_ERROR_PARAMENER_FORMAT;
        }
        uint8_t domainId = (uint8_t)packet->varList[1].floatNumber;
        if (domainId >= CONDITION_DOMAIN_COUNT)
        {
            UART_Printf(comX, "错误: ID需在0-%d之间\r\n", CONDITION_DOMAIN_COUNT - 1);
            return CMD_ERROR_EXCUTE;
        }
        Condition_UpdateAllDomains();
        if (Condition_IsTriggered(domainId))
        {
            UART_Printf(comX, "条件域 %d: 已触发\r\n", domainId);
            float dx, dy, dz, dyaw, dt;
            if (Condition_GetTriggerInfo(domainId, &dx, &dy, &dz, &dyaw, &dt))
            {
                UART_Printf(comX, "  位移: dx=%.3f, dy=%.3f, dz=%.3f\r\n", dx, dy, dz);
                UART_Printf(comX, "  角度: dyaw=%.1f, 时间: dt=%.1fs\r\n", dyaw, dt);
            }
        }
        else
        {
            UART_Printf(comX, "条件域 %d: 未触发\r\n", domainId);
        }
        return CMD_NO_ERROR;
    }

    /* 处理 'reset' 子命令 (重置单个) */
    if (CMD_COMPARE_WITHOUT_CASE(subCmd, "reset") == 0)
    {
        if (packet->count < 2)
        {
            UART_SendString(comX, "错误: 需要条件域ID。用法: cond reset <ID>\r\n");
            return CMD_ERROR_PARAMENER_FORMAT;
        }
        if (CMD_COMPARE_WITHOUT_CASE(packet->varList[1].string, "all") == 0)
        {
            for (uint8_t i = 0; i < CONDITION_DOMAIN_COUNT; i++)
            {
                Condition_Reset(i);
                Condition_Disable(i);
            }
            UART_SendString(comX, "所有条件域已重置\r\n");
            return CMD_NO_ERROR;
        }
        uint8_t domainId = (uint8_t)packet->varList[1].floatNumber;
        if (domainId >= CONDITION_DOMAIN_COUNT)
        {
            UART_Printf(comX, "错误: ID需在0-%d之间\r\n", CONDITION_DOMAIN_COUNT - 1);
            return CMD_ERROR_EXCUTE;
        }
        if (Condition_Reset(domainId))
        {
            UART_Printf(comX, "条件域 %d 已重置\r\n", domainId);
            return CMD_NO_ERROR;
        }
        UART_Printf(comX, "条件域 %d 重置失败\r\n", domainId);
        return CMD_ERROR_EXCUTE;
    }

    /* 处理 'cancel' 子命令 (设置取消标志) */
    if (CMD_COMPARE_WITHOUT_CASE(subCmd, "cancel") == 0)
    {
        if (packet->count < 2)
        {
            UART_SendString(comX, "错误: 需要设置值(0或1)。用法: cond cancel <0/1>\r\n");
            return CMD_ERROR_PARAMENER_FORMAT;
        }
        extern uint8_t g_ConditionCancelFlag;
        g_ConditionCancelFlag = !!packet->varList[1].intNumber;
        UART_Printf(comX, "遥控模式条件域报警取消已设置为: %d (1:取消, 0:保持)\r\n", g_ConditionCancelFlag);
        return CMD_NO_ERROR;
    }

    /* 未知子命令 */
    UART_Printf(comX, "错误: 未知的子命令 '%s'。输入 'cond help' 查看用法。\r\n", subCmd);
    return CMD_ERROR_PARAMENER_FORMAT;
}

/**
 * @brief 查看I2C设备地址
 * @param comX: 串口号
 * @param packet: 命令数据包
 * @return 命令执行结果
 */
static uint16_t CMD_SoftI2CDevice(uint8_t comX, CmdStringDataPacket_T *packet)
{
    uint8_t deviceCount = 0;

    for (uint8_t devAddr = 0x01; devAddr < 0x7F; devAddr++)
    {
        if (SoftI2C_ProbeDevice(devAddr << 1) == SOFT_I2C_RESULT_OK)
        {
            if (deviceCount == 0)
            {
                UART_SendString(comX, "检测到I2C设备\r\n");
            }
            UART_Printf(comX, "0x%X, ", devAddr << 1);

            if (++deviceCount % 8 == 0)
            {
                UART_SendString(comX, "\r\n");
            }
        }
    }

    if (deviceCount == 0)
    {
        UART_SendString(comX, "未检测到I2C设备\r\n");
        return CMD_ERROR_EXCUTE;
    }
    else
    {
        UART_SendString(comX, "\r\n");
        return CMD_NO_ERROR;
    }
}

/**
 * @brief 文件读取命令
 * @param comX: 串口号
 * @param packet: 命令参数包
 * @return 命令执行结果
 */
static uint16_t Cmd_FileRead(uint8_t comX, CmdStringDataPacket_T *packet)
{
    char *fileName = packet->varList[0].string;
    uint32_t size = (packet->count > 1) ? packet->varList[1].unsignedNumber : 0;
    uint8_t *buffer;

    if (size == 0)
    {
        // 读取整个文件
        uint32_t fileSize = FatFS_GetFileSize(fileName);
        if (fileSize == 0)
        {
            UART_Printf(comX, "错误: 文件不存在或为空\r\n");
            return CMD_ERROR_PARAMENER_FORMAT;
        }

        buffer = (uint8_t *)Memory_Malloc(MP_RAM_IN, fileSize + 1);
        if (!buffer)
        {
            UART_Printf(comX, "错误: 内存分配失败\r\n");
            return CMD_ERROR_PARAMENER_FORMAT;
        }

        uint32_t bytesRead = FatFS_ReadFile(fileName, buffer, fileSize);
        if (bytesRead > 0)
        {
            buffer[bytesRead] = '\0';
            UART_Printf(comX, "文件内容 (%u 字节):\r\n%s\r\n", bytesRead, buffer);
        }
    }
    else
    {
        // 读取指定大小
        buffer = (uint8_t *)Memory_Malloc(MP_RAM_IN, size + 1);
        if (!buffer)
        {
            UART_Printf(comX, "错误: 内存分配失败\r\n");
            return CMD_ERROR_PARAMENER_FORMAT;
        }

        uint32_t bytesRead = FatFS_ReadFile(fileName, buffer, size);
        if (bytesRead > 0)
        {
            buffer[bytesRead] = '\0';
            UART_Printf(comX, "读取 %u 字节:\r\n%s\r\n", bytesRead, buffer);
        }
    }

    Memory_Free(MP_RAM_IN, buffer);
    return CMD_NO_ERROR;
}

/**
 * @brief 文件写入命令
 * @param comX: 串口号
 * @param packet: 命令参数包
 * @return 命令执行结果
 */
static uint16_t Cmd_FileWrite(uint8_t comX, CmdStringDataPacket_T *packet)
{
    char *fileName = packet->varList[0].string;
    char *data = packet->varList[1].string;

    uint32_t bytesWritten = FatFS_WriteFile(fileName, (uint8_t *)data, strlen(data));
    if (bytesWritten > 0)
    {
        UART_Printf(comX, "成功写入 %u 字节到 %s\r\n", bytesWritten, fileName);
        return CMD_NO_ERROR;
    }

    UART_Printf(comX, "错误: 文件写入失败\r\n");
    return CMD_ERROR_PARAMENER_FORMAT;
}

/**
 * @brief 文件追加命令
 * @param comX: 串口号
 * @param packet: 命令参数包
 * @return 命令执行结果
 */
static uint16_t Cmd_FileAppend(uint8_t comX, CmdStringDataPacket_T *packet)
{
    char *fileName = packet->varList[0].string;
    char *data = packet->varList[1].string;

    uint32_t bytesWritten = FatFS_AppendFile(fileName, (uint8_t *)data, strlen(data));
    if (bytesWritten > 0)
    {
        UART_Printf(comX, "成功追加 %u 字节到 %s\r\n", bytesWritten, fileName);
        return CMD_NO_ERROR;
    }

    UART_Printf(comX, "错误: 文件追加失败\r\n");
    return CMD_ERROR_PARAMENER_FORMAT;
}

/**
 * @brief 文件删除命令
 * @param comX: 串口号
 * @param packet: 命令参数包
 * @return 命令执行结果
 */
static uint16_t Cmd_FileDelete(uint8_t comX, CmdStringDataPacket_T *packet)
{
    char *fileName = packet->varList[0].string;

    FatFsStatus_T status = FatFS_DeleteFile(fileName);
    if (status == FATFS_RET_OK)
    {
        UART_Printf(comX, "文件删除成功: %s\r\n", fileName);
        return CMD_NO_ERROR;
    }

    UART_Printf(comX, "错误: 文件删除失败 (代码: %d)\r\n", status);
    return CMD_ERROR_PARAMENER_FORMAT;
}

/**
 * @brief 目录创建命令
 * @param comX: 串口号
 * @param packet: 命令参数包
 * @return 命令执行结果
 */
static uint16_t Cmd_FileMkdir(uint8_t comX, CmdStringDataPacket_T *packet)
{
    char *dirPath = packet->varList[0].string;

    FatFsStatus_T status = FatFS_CreateDirectory(dirPath);
    if (status == FATFS_RET_OK)
    {
        UART_Printf(comX, "目录创建成功: %s\r\n", dirPath);
        return CMD_NO_ERROR;
    }

    UART_Printf(comX, "错误: 目录创建失败 (代码: %d)\r\n", status);
    return CMD_ERROR_PARAMENER_FORMAT;
}

/**
 * @brief 目录删除命令
 * @param comX: 串口号
 * @param packet: 命令参数包
 * @return 命令执行结果
 */
static uint16_t Cmd_FileRmdir(uint8_t comX, CmdStringDataPacket_T *packet)
{
    char *dirPath = packet->varList[0].string;

    FatFsStatus_T status = FatFS_DeleteDirectory(dirPath);
    if (status == FATFS_RET_OK)
    {
        UART_Printf(comX, "目录删除成功: %s\r\n", dirPath);
        return CMD_NO_ERROR;
    }

    UART_Printf(comX, "错误: 目录删除失败 (代码: %d)\r\n", status);
    return CMD_ERROR_PARAMENER_FORMAT;
}

/**
 * @brief 目录列表命令
 * @param comX: 串口号
 * @param packet: 命令参数包
 * @return 命令执行结果
 */
static uint16_t Cmd_FileLs(uint8_t comX, CmdStringDataPacket_T *packet)
{
    char *path = packet->varList[0].string;
    char *pattern = (packet->count > 1) ? packet->varList[1].string : "*";
    FatFsFileList_T list;

    FatFsStatus_T status = FatFS_GetFileList(path, pattern, &list, FALSE);
    if (status == FATFS_RET_OK)
    {
        UART_Printf(comX, "目录 %s 中的文件 (匹配 %s):\r\n", path, pattern);
        UART_Printf(comX, "%-20s %-8s %-12s %-8s %s\r\n", "文件名", "类型", "大小", "后缀", "完整路径");
        UART_Printf(comX, "%-20s %-8s %-12s %-8s %s\r\n", "--------------------", "--------", "------------", "--------", "--------------------");

        for (uint16_t i = 0; i < list.count; i++)
        {
            const char *typeStr = (list.files[i].type == FATFS_TYPE_DIRECTORY) ? "目录" : "文件";
            const char *extStr = (list.files[i].extension != NULL) ? list.files[i].extension : "(无)";

            UART_Printf(comX, "%-20s %-8s %-12u %-8s %s\r\n",
                        list.files[i].Name,
                        typeStr,
                        list.files[i].size,
                        extStr,
                        list.files[i].fullPath);
        }
        UART_Printf(comX, "\r\n总计: %d 个项目\r\n", list.count);
        FatFS_FreeFileList(&list);
        return CMD_NO_ERROR;
    }

    UART_Printf(comX, "错误: 目录列表获取失败 (代码: %d)\r\n", status);
    return CMD_ERROR_PARAMENER_FORMAT;
}

/**
 * @brief 小车设置舵机角度命令
 * @param comX: 串口号
 * @param vars: 变量参数
 * @param count: 变量数量
 * @return 命令执行结果
 */
static uint16_t CMD_SetConditionCancelFlag(uint8_t comX, CmdStringDataPacket_T *packet)
{
    extern uint8_t g_ConditionCancelFlag;
    g_ConditionCancelFlag = !!packet->varList[1].intNumber;  // 转换为0/1数值
    return CMD_NO_ERROR;
}

/**
 * @brief 初始化字符串命令
 */
static uint16_t CMD_Storage(uint8_t comX, CmdStringDataPacket_T *packet)
{
    if (packet->varList[0].type != CMD_STR_VAR_STRING) { return CMD_ERROR_PARAMENER_FORMAT; }
    if (packet->count > 1 && packet->varList[1].type != CMD_STR_VAR_STRING) { return CMD_ERROR_PARAMENER_FORMAT; }
    const char *path = packet->count > 1 ? packet->varList[1].string : NULL;
    return Storage_Command(comX, packet->varList[0].string, path) ? CMD_NO_ERROR : CMD_ERROR_PARAMENER_FORMAT;
}

static uint16_t CMD_Log(uint8_t port, CmdStringDataPacket_T *packet)
{
    if (packet->varList[0].type != CMD_STR_VAR_STRING) return CMD_ERROR_PARAMENER_FORMAT;
    return Log_Command(port, packet->varList[0].string) ? CMD_NO_ERROR : CMD_ERROR_PARAMENER_FORMAT;
}

void CMD_StringProcInit(void)
{
    CMD_StrBaseInit();
    CMD_STR_ADD("log", CMD_Log, 1, "log on|off|status|debug|info|warn|error|quiet|test");
    CMD_STR_ADD("storage", CMD_Storage, 1, "storage info|bounds|guard");
    CMD_STR_ADD("storage", CMD_Storage, 2, "storage test|verify 0:/new-file.bin");

    CMD_STR_ADD("task_info", CMD_TaskInfo, 0, "task_info - 列出任务运行情况");
    CMD_STR_ADD("beep", CMD_Beep, 3, "beep [开启时间] [关闭时间] [循环次数] - 蜂鸣器循环鸣响");
    CMD_STR_ADD("beep", CMD_Beep, 1, "beep off - 关闭蜂鸣器");
    CMD_STR_ADD("rtc", CMD_RTC, 0, "rtc - 获取时间");
    CMD_STR_ADD("rtc", CMD_RTC, 1, "rtc - 设置时间 (兼容命令)");
    CMD_STR_ADD("exp", CMD_Exp, CMD_STR_VARIABLE_PARAMS, "exp <子命令> [参数...] - 表达式系统操作 (输入'exp help'查看子命令)");
    CMD_STR_ADD("cond", CMD_Cond, CMD_STR_VARIABLE_PARAMS, "cond <子命令> [参数...] - 条件域操作 (输入'cond help'查看子命令)");
    CMD_STR_ADD("memory", CMD_GetMemory, 0, "memory - 查看系统内存使用情况");
    CMD_STR_ADD("remote", CMD_Remote, 1, "remote stop - 在遥控模式下停止小车（三个方向的速度都设置为0）");
    CMD_STR_ADD("remote", CMD_Remote, 2, "remote [前进] [平移] [旋转] - 小车遥控控制");
    CMD_STR_ADD("stop", CMD_Stop, 0, "stop - 停止小车");
    CMD_STR_ADD("imu_reset", CMD_ImuResetOrientation, 0, "imu_reset - 陀螺仪姿态清零");
    CMD_STR_ADD("set", CMD_Set, CMD_STR_VARIABLE_PARAMS, "set <子命令> [参数...] - 设置系统参数 (输入'set help'查看子命令)");
    CMD_STR_ADD("receive", CMD_Receive, 1, "receive [串口] - 从其他串口设备接收命令数据\r\n示例: receive com1");
    CMD_STR_ADD("send", CMD_Send, 2, "send [串口] [数据] - 发送命令数据到其他串口设备\r\n 示例: send com2 \"期待你的回复\"");
    CMD_STR_ADD("reboot", CMD_Reboot, 0, "reboot - 重启系统");
    CMD_STR_ADD("i2c_device", CMD_SoftI2CDevice, 0, "i2c_device - 获取所有I2C设备列表");
    CMD_STR_ADD("key", CMD_SetKey, 1, "key [按钮] - 以点击模式按下按键\r\n按钮:\r\n   enter     - 确认按钮\r\n   mode      - 模式按钮\r\n   up        - 向上按钮\r\n   down      - 向上按钮");
    CMD_STR_ADD("key", CMD_SetKey, 2, "key [按钮] [模式] - 以指定模式按下按键\r\n模式:\r\n   long      - 长按\r\n   repeat    - 重复模式\r\n   (其他)    - 点击模式");
    CMD_STR_ADD("show", CMD_Show, 1, "show [选项]\r\n选项:\r\n  euler       - 欧拉角数据(Pitch, Roll, Yaw)\r\n  mpu         - 原始MPU数据(陀螺仪+加速度计+温度)\r\n  car         - 小车速度和位置数据\r\n  m1           - 电机M1数据(实际值/目标值/输出值)\r\n  m2           - 电机M2数据(实际值/目标值/输出值)\r\n  m3           - 电机M3数据(实际值/目标值/输出值)\r\n  m4           - 电机M4数据(实际值/目标值/输出值)\r\n  battery     - 电池电压和蜂鸣器状态\r\n  state       - 系统运行状态\r\n  off         - 禁用所有数据输出\r\n");
    CMD_STR_ADD("read", Cmd_FileRead, 1, "read [文件名] - 读取整个文件内容");
    CMD_STR_ADD("read", Cmd_FileRead, 2, "read [文件名] [大小] - 读取指定大小的文件内容");
    CMD_STR_ADD("write", Cmd_FileWrite, 2, "write [文件名] [数据] - 写入数据到文件");
    CMD_STR_ADD("append", Cmd_FileAppend, 2, "append [文件名] [数据] - 追加数据到文件");
    CMD_STR_ADD("rm", Cmd_FileDelete, 1, "rm [文件名] - 删除文件");
    CMD_STR_ADD("mkdir", Cmd_FileMkdir, 1, "mkdir [目录路径] - 创建目录");
    CMD_STR_ADD("rmdir", Cmd_FileRmdir, 1, "rmdir [目录路径] - 删除目录");
    CMD_STR_ADD("ls", Cmd_FileLs, 1, "ls [路径] - 列出目录内容");
    CMD_STR_ADD("ls", Cmd_FileLs, 2, "ls [路径] [匹配模式] - 列出匹配模式的文件");
}
