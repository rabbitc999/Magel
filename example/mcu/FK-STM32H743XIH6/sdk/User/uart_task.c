#include "uart_task.h"
#include "simple_task.h"

extern Task_T *g_UartRxTask, *g_UartTxTask;

/**
 * @brief  通用串口接收处理任务
 * @param  pvParameters FreeRTOS任务参数（未使用）
 */
TaskRet_T UartRx_Task(TaskParam_T *param)
{
    T_STA(param) = 1;
    for (UartPort_T port = COM1; port < COM_COUNT; port++)
    {
        /* 如果是激光雷达的串口，应跳过处理 */
        if (port == COM3 || port == COM5) { continue; }

        /* 若蓝牙或ESP8266未初始化，则忽略该串口帧 */
        if (port == COM4 && (!Bluetooth_IsInitComplete() || !ESP8266_IsInitComplete())) { continue; }

        UartRxState_T state = UART_ProcessRx(port);
        if (state == UART_RX_STATE_BUSY || state == UART_RX_STATE_HAS_DATA)
        {
            T_STA(param) = 0;
        }
    }
    if (T_STA(param) == 0) { return Task_Delay(5); }
    else
    {
        return Task_Exit();
    }
}

#if UART_TX_USE_BUFFER
/**
 * @brief  串口发送任务
 * @param  pvParameters FreeRTOS任务参数（未使用）
 */
TaskRet_T UartTx_Task(TaskParam_T *param)
{
    T_STA(param) = 1;
    for (UartPort_T port = COM1; port < COM_COUNT; port++)
    {
        /* 处理串口接收 */
        UartTxState_T state = UART_ProcessTx(port);
        if (state == UART_TX_STATE_BUSY || state == UART_TX_STATE_HAS_DATA)
        {
            T_STA(param) = 0;
        }
    }
    if (T_STA(param) == 0) { return Task_Delay(5); }
    else
    {
        return Task_Exit();
    }
}
#endif

/**
 * @brief  串口接收完成回调函数
 * @param  comX 串口端口号
 * @param  buffer 接收数据缓冲区指针
 * @param  length 接收数据长度
 * @note   当串口接收完成时调用，释放接收信号量以唤醒接收任务
 */
void UART_ReceiveFinishCallback(UartPort_T comX, uint8_t *buffer, uint32_t length)
{
    // 激光雷达和陀螺仪接收不参与信号量
    if (comX == COM3 || comX == COM5) { return; }

    /* 如果启动发送任务未运行，且不用于激光雷达，此时直接使用命令系统 */
    Task_Enable(g_UartRxTask);
}

#if UART_TX_USE_BUFFER
/**
 * @brief  串口发送开始回调函数
 * @param  comX 串口端口号
 * @note   当有数据写入FIFO时调用，释放发送信号量以启动发送任务
 */
void UART_TransmitStartCallback(UartPort_T comX)
{
    /* 如果发送任务未运行，则启动它 */
    Task_Enable(g_UartTxTask);
}
#endif

/**
 * @brief  数据输出任务
 * @param  pvParameters FreeRTOS任务参数（未使用）
 */
TaskRet_T DataOutput_Task(TaskParam_T *param)
{
    static uint32_t boardCastTime[COM_COUNT] = { 0 };
    static uint16_t castTimeArray[COM_COUNT] = { 0 };
    uint8_t castCount = 0;

    /* 串口输出 */
    for (UartPort_T port = COM1; port < COM_COUNT; port++)
    {
        /* 如果是激光雷达的串口，应跳过处理 */
        if (port == g_CMD.lidarReceiveCom) { continue; }

        /* 若蓝牙或ESP8266未初始化，应跳过处理 */
        if (port == COM4 && (!Bluetooth_IsInitComplete() || !ESP8266_IsInitComplete())) { continue; }

        /* 输出串口数据 */
        if ((g_CMD.comBoardCast & (0x01 << port)) != 0 || g_CMD.dataOutputShowFlag[port] != 0)
        {
            if (Timer_GetDiffMs(boardCastTime[port]) >= g_CMD.comBoardCastDeltaTime[port])
            {
                CMD_OutputData(port);
            }
            castTimeArray[castCount++] = g_CMD.comBoardCastDeltaTime[port];
        }
    }

    if (castCount != 0)
    {
        /* 计算最大公约数，用作任务延时时间 */
        uint16_t delayTime = GCD_Array(castTimeArray, castCount);
        /* 设置最小任务延时时间，保证其他任务得到调度 */
        delayTime = delayTime > 5 ? delayTime : 5;
        return Task_Delay(delayTime);
    }
    else
    {
        return Task_Exit();
    }
}
