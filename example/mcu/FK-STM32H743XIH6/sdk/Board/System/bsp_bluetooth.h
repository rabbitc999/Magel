#ifndef _BSP_BLUETOOTH_H
#define _BSP_BLUETOOTH_H

#include "stm32h7xx_hal.h"

#ifdef __cplusplus
extern "C" {
#endif

#define BLUETOOTH_EN_PIN          GPIO_PIN_3
#define BLUETOOTH_EN_GPIO_PORT    GPIOE
#define BLUETOOTH_STATE_PIN       GPIO_PIN_4
#define BLUETOOTH_STATE_GPIO_PORT GPIOD

typedef enum __BluetoothState_T
{
    BLUETOOTH_STATE_INIT,
    BLUETOOTH_STATE_INIT_WAIT,
    BLUETOOTH_STATE_SEND_AT,
    BLUETOOTH_STATE_SEND_AT_WAIT,
    BLUETOOTH_STATE_SET_NAME,
    BLUETOOTH_STATE_SET_NAME_WAIT,
    BLUETOOTH_STATE_SET_PASSWORD,
    BLUETOOTH_STATE_SET_PASSWORD_WAIT,
    BLUETOOTH_STATE_SET_ROLE,
    BLUETOOTH_STATE_SET_ROLE_WAIT,
    BLUETOOTH_STATE_SET_UART,
    BLUETOOTH_STATE_SET_UART_WAIT,
    BLUETOOTH_STATE_RESET,
    BLUETOOTH_STATE_RESET_WAIT,
    BLUETOOTH_STATE_CONFIG,
    BLUETOOTH_STATE_CONFIG_WAIT,
    BLUETOOTH_STATE_COMPLETE,
} BluetoothState_T;

typedef struct __Bluetooth_T
{
    char *ConnectName;       // 设备连接名称
    char *ConnectPassword;   // 设备连接密码
    char *ConnectRole;       // 设备连接角色
    uint32_t BaudRate;       // 波特率
    uint8_t StopBits;        // 停止位
    uint8_t Parity;          // 校验位
    uint32_t lastTick;       // 上次时间
    uint8_t receiveFlag;     // 接收标志
    BluetoothState_T State;  // 当前状态
} Bluetooth_T;

void Bluetooth_SetConfig(char *Name, char *password, char *role, uint32_t baudrate);
void Bluetooth_StateMachine(void);
uint8_t Bluetooth_IsInitComplete(void);
void Bluetooth_Init(void);

#ifdef __cplusplus
}
#endif

#endif /* _BSP_BLUETOOTH_H */