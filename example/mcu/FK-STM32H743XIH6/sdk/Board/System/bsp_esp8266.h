#ifndef _BSP_ESP8266_H
#define _BSP_ESP8266_H

#include "stm32h7xx_hal.h"

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32h7xx_hal.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ESP8266_IO0_PIN       GPIO_PIN_12
#define ESP8266_IO0_GPIO_PORT GPIOA
#define ESP8266_RST_PIN       GPIO_PIN_9
#define ESP8266_RST_GPIO_PORT GPIOH

/* 协议类型 */
#define ESP8266_PROTOCOL_TCP  "TCP"
#define ESP8266_PROTOCOL_UDP  "UDP"
#define ESP8266_PROTOCOL_MQTT "MQTT"
#define ESP8266_PROTOCOL_SSL  "SSL"

typedef enum __Esp8266State_T
{
    ESP8266_STATE_RESET,
    ESP8266_STATE_RESET_WAIT,
    ESP8266_STATE_INIT,
    ESP8266_STATE_INIT_WAIT,
    ESP8266_STATE_RESTORE,
    ESP8266_STATE_RESTORE_WAIT,
    ESP8266_STATE_SET_MODE,
    ESP8266_STATE_SET_MODE_WAIT,
    ESP8266_STATE_CONNECT_WIFI,
    ESP8266_STATE_CONNECT_WIFI_WAIT,
    ESP8266_STATE_SET_MUX,
    ESP8266_STATE_SET_MUX_WAIT,
    ESP8266_STATE_SET_PROTOCOL,
    ESP8266_STATE_SET_PROTOCOL_WAIT,
    ESP8266_STATE_START_CONNECTION,
    ESP8266_STATE_START_CONNECTION_WAIT,
    ESP8266_STATE_MQTT_CONNECT,
    ESP8266_STATE_MQTT_CONNECT_WAIT1,
    ESP8266_STATE_MQTT_CONNECT_WAIT2,
    ESP8266_STATE_START_TRANSMISSION,
    ESP8266_STATE_START_TRANSMISSION_WAIT,
    ESP8266_STATE_COMPLETE,
} Esp8266State_T;

typedef struct __ESP8266_T
{
    char wifiSsid[32];     /* WiFi名称 */
    char wifiPassword[64]; /* WiFi密码 */
    char protocol[8];      /* 协议类型 */
    char serverIp[16];     /* 服务器IP */
    char serverPort[8];    /* 服务器端口 */
    char localPort[8];     /* 本地端口(UDP) */
    char clientId[32];     /* 客户端ID(MQTT) */
    char mqttUser[32];     /* MQTT用户名 */
    char mqttPassword[32]; /* MQTT密码 */
    uint32_t lastTick;     /* 上次时间戳 */
    uint8_t receiveFlag;   /* 接收标志 */
    Esp8266State_T State;  /* 当前状态 */
} ESP8266_T;

void ESP8266_SetWifiConfig(const char *ssid, const char *password);
void ESP8266_SetTcpConfig(const char *serverIp, const char *serverPort);
void ESP8266_SetUdpConfig(const char *serverIp, const char *serverPort, const char *localPort);
void ESP8266_SetMqttConfig(const char *serverIp, const char *serverPort,
                           const char *clientId, const char *user, const char *password);
void ESP8266_SetSslConfig(const char *serverIp, const char *serverPort);
void ESP8266_StateMachine(void);
uint8_t ESP8266_IsInitComplete(void);
void ESP8266_Init(void);

#ifdef __cplusplus
}
#endif

#endif /* _BSP_ESP8266_H */