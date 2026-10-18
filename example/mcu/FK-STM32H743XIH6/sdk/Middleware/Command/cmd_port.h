#ifndef __CMD_PORT_H
#define __CMD_PORT_H

#include "bsp_uart.h"

#define CMD_RECEIVE_DATA_BUFFER_SIZE 512

#ifdef __cplusplus
extern "C" {
#endif

/* 命令模式控制块 */
typedef struct __CMD_T
{
    uint8_t dataOutputShowFlag[COM_COUNT];                     /* 数据输出标志位 */
    char receiveData[COM_COUNT][CMD_RECEIVE_DATA_BUFFER_SIZE]; /* 串口接收数据缓冲区 */
    uint16_t comBoardCastDeltaTime[COM_COUNT];                 /* 数据广播间隔时间 */
    uint8_t comBoardCast;                                      /* 数据广播标志位 */
    uint8_t hexStrictMode;                                     /* HEX严格模式标志位 */
    uint8_t receiveMode;                                       /* 命令接收模式标志位 */
    uint8_t lidarReceiveCom;                                   /* 雷达接收串口（默认为255） */
} CMD_T;

extern CMD_T g_CMD;

void CMD_SendArray(uint16_t comX, uint8_t *array, uint16_t length);
void CMD_Printf(uint16_t comX, char *format, ...);
void CMD_OutputData(uint8_t comX);
void CMD_IDLE_Read(UartPort_T comX, char ch);
void UART_RawDataProtocolHandler(UartPort_T comX, uint8_t *array, uint32_t size);

#ifdef __cplusplus
}
#endif

#endif
