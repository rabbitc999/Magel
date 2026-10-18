#include "bsp_system.h"
#include "bsp_bluetooth.h"
#include "bsp_periph.h"

static Bluetooth_T s_Bluetooth = {
    .State = BLUETOOTH_STATE_COMPLETE,
};

/**
 * @brief  设置蓝牙设备配置参数
 * @param  Name 设备名称字符串
 * @param  password 设备密码字符串
 * @param  role 设备角色字符串 ("MASTER", "SLAVE", "LOOPBACK")
 * @param  baudrate 波特率数值
 * @note   传NULL（或0）的参数保持原配置不变
 */
void Bluetooth_SetConfig(char *Name, char *password, char *role, uint32_t baudrate)
{
    if (Name != NULL) { s_Bluetooth.ConnectName = Name; }
    if (password != NULL) { s_Bluetooth.ConnectPassword = password; }
    if (role != NULL) { s_Bluetooth.ConnectRole = role; }
    if (baudrate > 0) { s_Bluetooth.BaudRate = baudrate; }
}

/**
 * @brief  蓝牙状态机处理函数（通过AT命令依次完成初始化、名称、密码、角色及串口参数配置）
 * @note   需周期调用以推进各配置状态，配置完成后不再处理
 */
void Bluetooth_StateMachine(void)
{
    // 如果已经完成初始化，则直接返回
    if (s_Bluetooth.State == BLUETOOTH_STATE_COMPLETE) { return; }

    // 检查是否接收到响应
    if (!s_Bluetooth.receiveFlag)
    {
        s_Bluetooth.receiveFlag = UART_IsReceivedString(COM4, "OK");
    }

    // 状态机处理
    switch (s_Bluetooth.State)
    {
    case BLUETOOTH_STATE_INIT:
        // 初始化蓝牙模块
        s_Bluetooth.lastTick = HAL_GetTick();
        s_Bluetooth.State = BLUETOOTH_STATE_INIT_WAIT;

        // 设置EN引脚高电平使能蓝牙模块AT命令模式
        HAL_GPIO_WritePin(BLUETOOTH_EN_GPIO_PORT, BLUETOOTH_EN_PIN, GPIO_PIN_SET);
        UART_SetBaudRate(COM4, 38400);
        UART_SetParity(COM4, UART_PARITY_NONE);  /* 无校验 */
        UART_SetStopBits(COM4, UART_STOPBITS_1); /* 1个停止位 */
        break;

    case BLUETOOTH_STATE_INIT_WAIT:
        if (Timer_GetDiffMs(s_Bluetooth.lastTick) > 1000)
        {
            s_Bluetooth.State = BLUETOOTH_STATE_SEND_AT;
        }
        break;

    case BLUETOOTH_STATE_SEND_AT:
        // 发送AT命令
        UART_Printf(COM4, "AT\r\n");
        UART_ClearRxFIFO(COM4);

        s_Bluetooth.receiveFlag = 0;
        s_Bluetooth.lastTick = HAL_GetTick();
        s_Bluetooth.State = BLUETOOTH_STATE_SEND_AT_WAIT;
        break;

    case BLUETOOTH_STATE_SEND_AT_WAIT:
        if (Timer_GetDiffMs(s_Bluetooth.lastTick) > 500 && s_Bluetooth.receiveFlag)
        {
            s_Bluetooth.State = BLUETOOTH_STATE_SET_NAME;
        }
        break;

    case BLUETOOTH_STATE_SET_NAME:
        // 设置设备名称
        UART_Printf(COM4, "AT+NAME=%s\r\n", s_Bluetooth.ConnectName);
        UART_ClearRxFIFO(COM4);

        s_Bluetooth.receiveFlag = 0;
        s_Bluetooth.lastTick = HAL_GetTick();
        s_Bluetooth.State = BLUETOOTH_STATE_SET_NAME_WAIT;
        break;

    case BLUETOOTH_STATE_SET_NAME_WAIT:
        if (Timer_GetDiffMs(s_Bluetooth.lastTick) > 500 && s_Bluetooth.receiveFlag)
        {
            s_Bluetooth.State = BLUETOOTH_STATE_SET_PASSWORD;
        }
        break;

    case BLUETOOTH_STATE_SET_PASSWORD:
        // 设置设备密码
        UART_Printf(COM4, "AT+PSWD=%s\r\n", s_Bluetooth.ConnectPassword);
        UART_ClearRxFIFO(COM4);

        s_Bluetooth.receiveFlag = 0;
        s_Bluetooth.lastTick = HAL_GetTick();
        s_Bluetooth.State = BLUETOOTH_STATE_SET_PASSWORD_WAIT;
        break;

    case BLUETOOTH_STATE_SET_PASSWORD_WAIT:
        if (Timer_GetDiffMs(s_Bluetooth.lastTick) > 500 && s_Bluetooth.receiveFlag)
        {
            s_Bluetooth.State = BLUETOOTH_STATE_SET_ROLE;
        }
        break;

    case BLUETOOTH_STATE_SET_ROLE:
        // 设置设备角色
        if (String_Compare(s_Bluetooth.ConnectRole, "MASTER", 6) == 0)
        {
            UART_Printf(COM4, "AT+ROLE=1\r\n"); /* 主模式 */
        }
        else if (String_Compare(s_Bluetooth.ConnectRole, "SLAVE", 5) == 0)
        {
            UART_Printf(COM4, "AT+ROLE=0\r\n"); /* 从模式 */
        }
        else if (String_Compare(s_Bluetooth.ConnectRole, "LOOPBACK", 8) == 0)
        {
            UART_Printf(COM4, "AT+ROLE=2\r\n"); /* 回环模式 */
        }

        s_Bluetooth.receiveFlag = 0;
        s_Bluetooth.lastTick = HAL_GetTick();
        s_Bluetooth.State = BLUETOOTH_STATE_SET_ROLE_WAIT;
        break;

    case BLUETOOTH_STATE_SET_ROLE_WAIT:
        if (Timer_GetDiffMs(s_Bluetooth.lastTick) > 500 && s_Bluetooth.receiveFlag)
        {
            s_Bluetooth.State = BLUETOOTH_STATE_SET_UART;
        }
        break;

    case BLUETOOTH_STATE_SET_UART:
        // 设置UART参数
        UART_Printf(COM4, "AT+UART=%lu,%d,%d\r\n", s_Bluetooth.BaudRate, s_Bluetooth.StopBits, s_Bluetooth.Parity);
        UART_ClearRxFIFO(COM4);

        s_Bluetooth.receiveFlag = 0;
        s_Bluetooth.lastTick = HAL_GetTick();
        s_Bluetooth.State = BLUETOOTH_STATE_SET_UART_WAIT;
        break;

    case BLUETOOTH_STATE_SET_UART_WAIT:
        if (Timer_GetDiffMs(s_Bluetooth.lastTick) > 500)
        {
            s_Bluetooth.State = BLUETOOTH_STATE_RESET;
        }
        break;
    case BLUETOOTH_STATE_RESET:
        // 重置蓝牙模块
        UART_Printf(COM4, "AT+RESET\r\n");
        UART_ClearRxFIFO(COM4);

        s_Bluetooth.receiveFlag = 0;
        s_Bluetooth.lastTick = HAL_GetTick();
        s_Bluetooth.State = BLUETOOTH_STATE_RESET_WAIT;
        break;

    case BLUETOOTH_STATE_RESET_WAIT:
        if (Timer_GetDiffMs(s_Bluetooth.lastTick) > 1000)
        {
            s_Bluetooth.State = BLUETOOTH_STATE_CONFIG;
        }
        break;
    case BLUETOOTH_STATE_CONFIG:
        // 其他配置
        HAL_GPIO_WritePin(BLUETOOTH_EN_GPIO_PORT, BLUETOOTH_EN_PIN, GPIO_PIN_SET);
        UART_SetBaudRate(COM4, s_Bluetooth.BaudRate);
        UART_ClearRxFIFO(COM4);

        s_Bluetooth.lastTick = HAL_GetTick();
        s_Bluetooth.State = BLUETOOTH_STATE_CONFIG_WAIT;
        break;

    case BLUETOOTH_STATE_CONFIG_WAIT:
        if (Timer_GetDiffMs(s_Bluetooth.lastTick) > 500)
        {
            s_Bluetooth.State = BLUETOOTH_STATE_COMPLETE;
        }
        break;
    default:
        // 配置完成
        break;
    }
}

/**
 * @brief  初始化蓝牙模块GPIO（EN使能引脚、STATE状态引脚）并复位状态机
 */
void Bluetooth_Init(void)
{
    BSP_GPIO_ClkEnable(BLUETOOTH_EN_GPIO_PORT);
    BSP_GPIO_ClkEnable(BLUETOOTH_STATE_GPIO_PORT);
    s_Bluetooth.State = BLUETOOTH_STATE_INIT;

    GPIO_InitTypeDef GPIO_InitStruct = { 0 };
    GPIO_InitStruct.Pin = BLUETOOTH_EN_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(BLUETOOTH_EN_GPIO_PORT, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = BLUETOOTH_STATE_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(BLUETOOTH_STATE_GPIO_PORT, &GPIO_InitStruct);
}

/**
 * @brief  检查蓝牙初始化是否完成
 * @retval 1-初始化完成；0-未完成
 */
uint8_t Bluetooth_IsInitComplete(void)
{
    return (s_Bluetooth.State == BLUETOOTH_STATE_COMPLETE);
}