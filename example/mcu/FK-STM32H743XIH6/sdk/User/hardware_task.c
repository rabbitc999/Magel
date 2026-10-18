#include "uart_task.h"
#include "simple_task.h"
#include "store.h"

extern MedianFilt_T g_FrontFilt, g_BackFilt;
extern volatile float g_FrontUltraSonicDistance, g_BackUltraSonicDistance;

/**
 * @brief  读取蓝牙配置信息
 */
void Bluetooth_ReadInfo(void)
{
    /* 读取蓝牙配置参数 */
    uint32_t baudRate = g_ParamData.uart_baudRate[COM4]; /* 从存储数据获取波特率 */

    /* 设置蓝牙配置 */
    Bluetooth_SetConfig((char *)g_ParamData.bluetooth_name, (char *)g_ParamData.bluetooth_pin, (char *)g_ParamData.bluetooth_role, baudRate);
}

/**
 * @brief  读取ESP8266配置信息
 */
void ESP8266_ReadInfo(void)
{
    /* 设置WiFi配置 */
    ESP8266_SetWifiConfig(g_ParamData.esp8266_wifiSsid, g_ParamData.esp8266_wifiPassword);

    /* 根据协议类型设置网络配置 */
    if (strcmp(g_ParamData.esp8266_protocol, "TCP") == 0)
    {
        ESP8266_SetTcpConfig(g_ParamData.esp8266_serverIp, g_ParamData.esp8266_serverPort);
    }
    else if (strcmp(g_ParamData.esp8266_protocol, "UDP") == 0)
    {
        ESP8266_SetUdpConfig(g_ParamData.esp8266_serverIp, g_ParamData.esp8266_serverPort, g_ParamData.esp8266_serverLocalPort);
    }
    else if (strcmp(g_ParamData.esp8266_protocol, "MQTT") == 0)
    {
        ESP8266_SetMqttConfig(g_ParamData.esp8266_serverIp, g_ParamData.esp8266_serverPort, g_ParamData.esp8266_clientId, g_ParamData.esp8266_mqttUser, g_ParamData.esp8266_mqttPass);
    }
    else if (strcmp(g_ParamData.esp8266_protocol, "SSL") == 0)
    {
        ESP8266_SetSslConfig(g_ParamData.esp8266_serverIp, g_ParamData.esp8266_serverPort);
    }
}

/**
 * @brief  蓝牙初始化任务
 * @param  pvParameters FreeRTOS任务参数（未使用）
 */
TaskRet_T BluetoothInit_Task(TaskParam_T *param)
{
    Bluetooth_StateMachine();

    return Task_Delay(100);
}

/**
 * @brief  ESP8266初始化任务
 * @param  pvParameters FreeRTOS任务参数（未使用）
 */
TaskRet_T ESP8266Init_Task(TaskParam_T *param)
{
    ESP8266_StateMachine();

    return Task_Delay(100);
}

/**
 * @brief  数据输出任务
 * @param  pvParameters FreeRTOS任务参数（未使用）
 */
TaskRet_T CS200A_Task(TaskParam_T *param)
{
    /* 开始新的测量 */
    CS100A_SendTrigger();

    if (g_CS100A_FrontDistance < 12.0f) { g_CS100A_FrontDistance = 12.0f; }
    if (g_CS100A_BackDistance < 12.0f) { g_CS100A_BackDistance = 12.0f; }

    /* 滤波得到新的测量距离 */
    g_FrontUltraSonicDistance = MedianFilt_Update(&g_FrontFilt, g_CS100A_FrontDistance);
    g_BackUltraSonicDistance = MedianFilt_Update(&g_BackFilt, g_CS100A_BackDistance);

    return Task_Delay(20);
}
