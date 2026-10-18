#include "bsp_system.h"
#include "bsp_esp8266.h"
#include "bsp_periph.h"
#include <string.h>

static ESP8266_T s_ESP8266 = {
    .State = ESP8266_STATE_COMPLETE,
};

/**
 * @brief  设置WiFi连接参数
 * @param  ssid WiFi名称
 * @param  password WiFi密码
 */
void ESP8266_SetWifiConfig(const char *ssid, const char *password)
{
    if (ssid != NULL) String_Copy(s_ESP8266.wifiSsid, ssid, sizeof(s_ESP8266.wifiSsid) - 1);
    if (password != NULL) String_Copy(s_ESP8266.wifiPassword, password, sizeof(s_ESP8266.wifiPassword) - 1);
}

/**
 * @brief  设置TCP配置
 * @param  serverIp 服务器IP
 * @param  serverPort 服务器端口
 */
void ESP8266_SetTcpConfig(const char *serverIp, const char *serverPort)
{
    strcpy(s_ESP8266.protocol, ESP8266_PROTOCOL_TCP);
    if (serverIp != NULL) String_Copy(s_ESP8266.serverIp, serverIp, sizeof(s_ESP8266.serverIp) - 1);
    if (serverPort != NULL) String_Copy(s_ESP8266.serverPort, serverPort, sizeof(s_ESP8266.serverPort) - 1);
}

/**
 * @brief  设置UDP配置
 * @param  serverIp 服务器IP
 * @param  serverPort 服务器端口
 * @param  localPort 本地端口
 */
void ESP8266_SetUdpConfig(const char *serverIp, const char *serverPort, const char *localPort)
{
    strcpy(s_ESP8266.protocol, ESP8266_PROTOCOL_UDP);
    if (serverIp != NULL) String_Copy(s_ESP8266.serverIp, serverIp, sizeof(s_ESP8266.serverIp) - 1);
    if (serverPort != NULL) String_Copy(s_ESP8266.serverPort, serverPort, sizeof(s_ESP8266.serverPort) - 1);
    if (localPort != NULL) String_Copy(s_ESP8266.localPort, localPort, sizeof(s_ESP8266.localPort) - 1);
}

/**
 * @brief  设置MQTT配置
 * @param  serverIp 服务器IP
 * @param  serverPort 服务器端口
 * @param  clientId 客户端ID
 * @param  user 用户名
 * @param  password 密码
 */
void ESP8266_SetMqttConfig(const char *serverIp, const char *serverPort,
                           const char *clientId, const char *user, const char *password)
{
    strcpy(s_ESP8266.protocol, ESP8266_PROTOCOL_MQTT);
    if (serverIp != NULL) String_Copy(s_ESP8266.serverIp, serverIp, sizeof(s_ESP8266.serverIp) - 1);
    if (serverPort != NULL) String_Copy(s_ESP8266.serverPort, serverPort, sizeof(s_ESP8266.serverPort) - 1);
    if (clientId != NULL) String_Copy(s_ESP8266.clientId, clientId, sizeof(s_ESP8266.clientId) - 1);
    if (user != NULL) String_Copy(s_ESP8266.mqttUser, user, sizeof(s_ESP8266.mqttUser) - 1);
    if (password != NULL) String_Copy(s_ESP8266.mqttPassword, password, sizeof(s_ESP8266.mqttPassword) - 1);
}

/**
 * @brief  设置SSL配置
 * @param  serverIp 服务器IP
 * @param  serverPort 服务器端口
 */
void ESP8266_SetSslConfig(const char *serverIp, const char *serverPort)
{
    strcpy(s_ESP8266.protocol, ESP8266_PROTOCOL_SSL);
    if (serverIp != NULL) String_Copy(s_ESP8266.serverIp, serverIp, sizeof(s_ESP8266.serverIp) - 1);
    if (serverPort != NULL) String_Copy(s_ESP8266.serverPort, serverPort, sizeof(s_ESP8266.serverPort) - 1);
}

/**
 * @brief  构建MQTT连接包
 * @param  buffer 输出缓冲区
 * @retval 数据包长度
 */
static uint16_t ESP8266_BuildMqttConnectPacket(uint8_t *buffer)
{
    uint16_t len = 0;

    /* MQTT固定头 */
    buffer[len++] = 0x10; /* CONNECT packet */

    /* 计算剩余长度 */
    uint16_t payloadLen = 0;

    /* Protocol Name */
    payloadLen += 2 + 4; /* "MQTT" length + string */

    /* Protocol Level */
    payloadLen += 1; /* 0x04 */

    /* Connect Flags */
    payloadLen += 1; /* 0xC2 (Clean Session, User/Pass) */

    /* Keep Alive */
    payloadLen += 2; /* 60 seconds */

    /* Client ID */
    payloadLen += 2 + strlen(s_ESP8266.clientId);

    /* Username */
    payloadLen += 2 + strlen(s_ESP8266.mqttUser);

    /* Password */
    payloadLen += 2 + strlen(s_ESP8266.mqttPassword);

    /* 编码剩余长度 */
    uint8_t remLen = payloadLen;
    buffer[len++] = remLen;

    /* Protocol Name: MQTT */
    buffer[len++] = 0x00;
    buffer[len++] = 0x04;
    buffer[len++] = 'M';
    buffer[len++] = 'Q';
    buffer[len++] = 'T';
    buffer[len++] = 'T';

    /* Protocol Level: 4 */
    buffer[len++] = 0x04;

    /* Connect Flags */
    buffer[len++] = 0xC2; /* Clean Session, Username, Password */

    /* Keep Alive: 60 seconds */
    buffer[len++] = 0x00;
    buffer[len++] = 0x3C;

    /* Client ID */
    uint16_t clientIdLen = strlen(s_ESP8266.clientId);
    buffer[len++] = (clientIdLen >> 8) & 0xFF;
    buffer[len++] = clientIdLen & 0xFF;
    memcpy(&buffer[len], s_ESP8266.clientId, clientIdLen);
    len += clientIdLen;

    /* Username */
    uint16_t userLen = strlen(s_ESP8266.mqttUser);
    buffer[len++] = (userLen >> 8) & 0xFF;
    buffer[len++] = userLen & 0xFF;
    memcpy(&buffer[len], s_ESP8266.mqttUser, userLen);
    len += userLen;

    /* Password */
    uint16_t passLen = strlen(s_ESP8266.mqttPassword);
    buffer[len++] = (passLen >> 8) & 0xFF;
    buffer[len++] = passLen & 0xFF;
    memcpy(&buffer[len], s_ESP8266.mqttPassword, passLen);
    len += passLen;

    return len;
}

/**
 * @brief  ESP8266状态机处理
 */
void ESP8266_StateMachine(void)
{
    /* 如果配置完成则直接返回 */
    if (s_ESP8266.State == ESP8266_STATE_COMPLETE) { return; }

    /* 检查是否收到响应 */
    if (!s_ESP8266.receiveFlag)
    {
        s_ESP8266.receiveFlag = UART_IsReceivedString(COM4, "OK");
    }

    /* 状态机处理 */
    switch (s_ESP8266.State)
    {
    case ESP8266_STATE_RESET:
        // 执行硬件复位
        HAL_GPIO_WritePin(ESP8266_RST_GPIO_PORT, ESP8266_RST_PIN, GPIO_PIN_RESET);
        s_ESP8266.lastTick = HAL_GetTick();
        s_ESP8266.State = ESP8266_STATE_RESET_WAIT;
        break;

    case ESP8266_STATE_RESET_WAIT:
        if (Timer_GetDiffMs(s_ESP8266.lastTick) > 100)
        {
            // 释放复位
            HAL_GPIO_WritePin(ESP8266_RST_GPIO_PORT, ESP8266_RST_PIN, GPIO_PIN_SET);
            s_ESP8266.lastTick = HAL_GetTick();
            s_ESP8266.State = ESP8266_STATE_INIT;
        }
        break;
    case ESP8266_STATE_INIT:
        // 等待模块启动完成（约2秒）
        if (Timer_GetDiffMs(s_ESP8266.lastTick) > 2000)
        {
            UART_SetBaudRate(COM4, 115200);
            s_ESP8266.lastTick = HAL_GetTick();
            s_ESP8266.State = ESP8266_STATE_INIT_WAIT;
        }
        break;
    case ESP8266_STATE_INIT_WAIT:
        if (Timer_GetDiffMs(s_ESP8266.lastTick) > 1000)
        {
            s_ESP8266.State = ESP8266_STATE_RESTORE;
        }
        break;
    case ESP8266_STATE_RESTORE:
        /* 恢复出厂设置 */
        UART_Printf(COM4, "AT+RESTORE\r\n");
        UART_ClearRxFIFO(COM4);

        s_ESP8266.receiveFlag = 0;
        s_ESP8266.lastTick = HAL_GetTick();
        s_ESP8266.State = ESP8266_STATE_RESTORE_WAIT;
        break;

    case ESP8266_STATE_RESTORE_WAIT:
        if (Timer_GetDiffMs(s_ESP8266.lastTick) > 2000)
        {
            s_ESP8266.State = ESP8266_STATE_SET_MODE;
        }
        break;

    case ESP8266_STATE_SET_MODE:
        /* 设置Station模式 */
        UART_Printf(COM4, "AT+CWMODE=1\r\n");
        UART_ClearRxFIFO(COM4);

        s_ESP8266.receiveFlag = 0;
        s_ESP8266.lastTick = HAL_GetTick();
        s_ESP8266.State = ESP8266_STATE_SET_MODE_WAIT;
        break;

    case ESP8266_STATE_SET_MODE_WAIT:
        if (Timer_GetDiffMs(s_ESP8266.lastTick) > 1000 && s_ESP8266.receiveFlag)
        {
            s_ESP8266.State = ESP8266_STATE_CONNECT_WIFI;
        }
        break;
    case ESP8266_STATE_CONNECT_WIFI:
        /* 连接WiFi */
        UART_Printf(COM4, "AT+CWJAP=\"%s\",\"%s\"\r\n", s_ESP8266.wifiSsid, s_ESP8266.wifiPassword);
        UART_ClearRxFIFO(COM4);

        s_ESP8266.receiveFlag = 0;
        s_ESP8266.lastTick = HAL_GetTick();
        s_ESP8266.State = ESP8266_STATE_CONNECT_WIFI_WAIT;
        break;
    case ESP8266_STATE_CONNECT_WIFI_WAIT:
        if (Timer_GetDiffMs(s_ESP8266.lastTick) > 10000 && s_ESP8266.receiveFlag)
        {
            s_ESP8266.State = ESP8266_STATE_SET_MUX;
        }
        break;
    case ESP8266_STATE_SET_MUX:
        /* 设置单连接模式 */
        UART_Printf(COM4, "AT+CIPMUX=0\r\n");
        UART_ClearRxFIFO(COM4);

        s_ESP8266.receiveFlag = 0;
        s_ESP8266.lastTick = HAL_GetTick();
        s_ESP8266.State = ESP8266_STATE_SET_MUX_WAIT;
        break;
    case ESP8266_STATE_SET_MUX_WAIT:
        if (Timer_GetDiffMs(s_ESP8266.lastTick) > 1000 && s_ESP8266.receiveFlag)
        {
            s_ESP8266.State = ESP8266_STATE_START_CONNECTION;
        }
        break;
    case ESP8266_STATE_START_CONNECTION:
        /* 建立连接 */
        if (strcmp(s_ESP8266.protocol, ESP8266_PROTOCOL_TCP) == 0)
        {
            UART_Printf(COM4, "AT+CIPSTART=\"TCP\",\"%s\",%s\r\n",
                        s_ESP8266.serverIp, s_ESP8266.serverPort);
        }
        else if (strcmp(s_ESP8266.protocol, ESP8266_PROTOCOL_UDP) == 0)
        {
            UART_Printf(COM4, "AT+CIPSTART=\"UDP\",\"%s\",%s,%s,2\r\n",
                        s_ESP8266.serverIp, s_ESP8266.serverPort, s_ESP8266.localPort);
        }
        else if (strcmp(s_ESP8266.protocol, ESP8266_PROTOCOL_SSL) == 0)
        {
            UART_Printf(COM4, "AT+CIPSTART=\"SSL\",\"%s\",%s\r\n",
                        s_ESP8266.serverIp, s_ESP8266.serverPort);
        }
        else if (strcmp(s_ESP8266.protocol, ESP8266_PROTOCOL_MQTT) == 0)
        {
            UART_Printf(COM4, "AT+CIPSTART=\"TCP\",\"%s\",%s\r\n",
                        s_ESP8266.serverIp, s_ESP8266.serverPort);
        }
        UART_ClearRxFIFO(COM4);

        s_ESP8266.receiveFlag = 0;
        s_ESP8266.lastTick = HAL_GetTick();
        s_ESP8266.State = ESP8266_STATE_START_CONNECTION_WAIT;
        break;
    case ESP8266_STATE_START_CONNECTION_WAIT:
        if (Timer_GetDiffMs(s_ESP8266.lastTick) > 3000 && s_ESP8266.receiveFlag)
        {
            if (strcmp(s_ESP8266.protocol, ESP8266_PROTOCOL_MQTT) == 0)
            {
                s_ESP8266.State = ESP8266_STATE_MQTT_CONNECT;
            }
            else
            {
                s_ESP8266.State = ESP8266_STATE_START_TRANSMISSION;
            }
        }
        break;
    case ESP8266_STATE_MQTT_CONNECT:
    {
        uint8_t mqttPacket[128];
        uint16_t packetLen = ESP8266_BuildMqttConnectPacket(mqttPacket);
        UART_Printf(COM4, "AT+CIPSEND=%d\r\n", packetLen);
        s_ESP8266.receiveFlag = 0;
        s_ESP8266.lastTick = HAL_GetTick();
        s_ESP8266.State = ESP8266_STATE_MQTT_CONNECT_WAIT1;
        break;
    }

    case ESP8266_STATE_MQTT_CONNECT_WAIT1:
        if (Timer_GetDiffMs(s_ESP8266.lastTick) > 1000 && s_ESP8266.receiveFlag)
        {
            uint8_t mqttPacket[128];
            uint16_t packetLen = ESP8266_BuildMqttConnectPacket(mqttPacket);
            s_ESP8266.State = ESP8266_STATE_COMPLETE;
            UART_SendArray(COM4, mqttPacket, packetLen);
            s_ESP8266.State = ESP8266_STATE_MQTT_CONNECT_WAIT2;
        }
        break;

    case ESP8266_STATE_MQTT_CONNECT_WAIT2:
        if (Timer_GetDiffMs(s_ESP8266.lastTick) > 1000 && s_ESP8266.receiveFlag)
        {
            s_ESP8266.State = ESP8266_STATE_COMPLETE;
        }
        break;
    case ESP8266_STATE_START_TRANSMISSION:
        /* 启动透传模式 */
        UART_Printf(COM4, "AT+CIPMODE=1\r\n");
        UART_ClearRxFIFO(COM4);

        s_ESP8266.receiveFlag = 0;
        s_ESP8266.lastTick = HAL_GetTick();
        s_ESP8266.State = ESP8266_STATE_START_TRANSMISSION_WAIT;
        break;
    case ESP8266_STATE_START_TRANSMISSION_WAIT:
        if (Timer_GetDiffMs(s_ESP8266.lastTick) > 1000 && s_ESP8266.receiveFlag)
        {
            UART_Printf(COM4, "AT+CIPSEND\r\n");
            s_ESP8266.State = ESP8266_STATE_COMPLETE;
        }
        break;
    default:
        /* 默认处理 */
        break;
    }
}

/**
 * @brief  初始化ESP8266模块GPIO状态
 */
void ESP8266_Init(void)
{
    // 使能GPIO时钟
    BSP_GPIO_ClkEnable(ESP8266_RST_GPIO_PORT);  // RST引脚所在的GPIO端口
    BSP_GPIO_ClkEnable(ESP8266_IO0_GPIO_PORT);  // IO0引脚所在的GPIO端口

    s_ESP8266.State = ESP8266_STATE_RESET;

    GPIO_InitTypeDef GPIO_InitStruct = { 0 };

    // 配置RST引脚为输出
    GPIO_InitStruct.Pin = ESP8266_RST_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(ESP8266_RST_GPIO_PORT, &GPIO_InitStruct);

    // 配置IO0引脚为输出
    GPIO_InitStruct.Pin = ESP8266_IO0_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(ESP8266_IO0_GPIO_PORT, &GPIO_InitStruct);

    // 设置初始状态：IO0为高（正常运行模式）
    HAL_GPIO_WritePin(ESP8266_IO0_GPIO_PORT, ESP8266_IO0_PIN, GPIO_PIN_SET);
}

/**
 * @brief  检查初始化是否完成
 * @retval 完成返回1，未完成返回0
 */
uint8_t ESP8266_IsInitComplete(void)
{
    return (s_ESP8266.State == ESP8266_STATE_COMPLETE);
}