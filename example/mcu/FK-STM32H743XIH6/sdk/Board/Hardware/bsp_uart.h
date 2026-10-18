#ifndef __BSP_UART_H
#define __BSP_UART_H

#include "bsp_system.h"
#include "bsp_fifo.h"

#include <stdio.h>
#include <stdint.h>

#if STSTEM_USE_RTOS
/* 互斥量声明 */
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"
#endif

/* 缓存大小设置 */
#define UART_BUFFER_SIZE   256
#define UART_TX_USE_BUFFER 1

/* 超时时间设置 */
#define UART_RX_TIMEOUT    3000 /* 接收超时时间 */

/* 串口调试设置 */
#define UART_DEBUG         COM1
#define DEBUG_PRINTF(...)  UART_Printf(UART_DEBUG, __VA_ARGS__)

#ifdef __cplusplus
extern "C" {
#endif

/* USART端口 */
typedef enum __UartPort_T
{
    COM1,
    COM2,
    COM3,
    COM4,
    COM5,
    COM6,
    COM7,
    COM_COUNT
} UartPort_T;

/* USART控制块主结构体 */
typedef struct __UartBlock_T
{
    /* FIFO循环缓冲区 */
    FIFO_T RxFIFO;
#if UART_TX_USE_BUFFER
    FIFO_T TxFIFO;
#endif

    /* 临时缓冲区 */
    uint8_t *RxTempBuffer;
    uint8_t *TxTempBuffer;
    uint8_t InitFlag;
    const uint32_t RxTempBufferSize;
    const uint32_t TxTempBufferSize;

    /* 硬件句柄 */
    UART_HandleTypeDef UART_Handle; /* STM32 HAL库UART句柄 */
    const IRQn_Type UART_IRQn;      /* 串口中断线 */

    DMA_HandleTypeDef RxDMA_IRQnHandle; /* 接收DMA句柄 */
    const IRQn_Type RxDMA_IRQnIrqn;     /* 串口中断线 */
    const uint16_t RxDMA_Request;       /* 串口DMA请求 */

    /* 其他成员 */
    DMA_HandleTypeDef TxDMA_IRQnHandle;
    IRQn_Type TxDMA_IRQnIrqn;
    uint16_t TxDMA_Request;
    volatile uint32_t TxBytes, TxErrors, TxDroppedBytes, TxCompleteEvents;
    uint16_t TxPendingLength;

    volatile uint32_t RxDmaPosition;
    volatile uint32_t RxBytes, RxDroppedBytes, RxErrors;
    volatile uint32_t RxIdleEvents, RxHalfEvents, RxFullEvents;
    uint32_t RxSendTick;   /* 接收时间 */
    UartPort_T PortNumber; /* 目标端口号 */
#if UART_TX_USE_BUFFER
    uint32_t TxSendTick;         /* 发送时间 */
    volatile uint8_t TxBusyFlag; /* 串口发送空闲标志 */
#endif
    const uint8_t RxEnable; /* 接收使能 */
    const uint8_t TxEnable; /* 发送使能 */

    /* 接收完成回调函数 */
    void (*receiveFinishCallback)(UartPort_T comX, uint8_t *buffer, uint32_t length);
    void (*transmitStartCallback)(UartPort_T comX);
    void (*transmitFinishCallback)(UartPort_T comX);

#if STSTEM_USE_RTOS
    SemaphoreHandle_t TxMutex; /* 串口发送互斥锁 */
#endif
} UartBlock_T;

/* 发送状态 */
typedef enum __UartTxState_T
{
    UART_TX_STATE_COMPLETE, /* 数据发送完成，FIFO为空 */
    UART_TX_STATE_BUSY,     /* 串口正在发送中 */
    UART_TX_STATE_HAS_DATA, /* FIFO中还有数据未发送 */
    UART_TX_STATE_ERROR     /* 错误状态（如串口未初始化） */
} UartTxState_T;

/* 发送状态 */
typedef enum __UartRxState_T
{
    UART_RX_STATE_COMPLETE, /* 数据发送完成，FIFO为空 */
    UART_RX_STATE_BUSY,     /* 串口正在发送中 */
    UART_RX_STATE_HAS_DATA, /* FIFO中还有数据未发送 */
    UART_RX_STATE_ERROR     /* 错误状态（如串口未初始化） */
} UartRxState_T;

/* 函数定义 */
void UART_Init(void);
#if UART_TX_USE_BUFFER
UartTxState_T UART_ProcessTx(UartPort_T comX);
#endif
UartRxState_T UART_ProcessRx(UartPort_T comX);
void UART_SendArray(UartPort_T comX, uint8_t *array, uint16_t size);
void UART_SendString(UartPort_T comX, const char *string);
void UART_Printf(UartPort_T comX, const char *format, ...);
void UART_SendArrayImm(UartPort_T comX, uint8_t *array, uint16_t size);  // Immediately 的缩写
void UART_SendStringImm(UartPort_T comX, const char *string);
void UART_PrintfImm(UartPort_T comX, const char *format, ...);
uint8_t UART_IsReceivedArray(UartPort_T comX, uint8_t *array, uint16_t size);
uint8_t UART_IsReceivedString(UartPort_T comX, const char *string);
void UART_ClearRxFIFO(UartPort_T comX);
void UART_ClearTxFIFO(UartPort_T comX);
void UART_VaListPrintf(UartPort_T comX, const char *format, va_list arg);
void UART_SetBaudRate(UartPort_T comX, uint32_t newBaud);
void UART_SetParity(UartPort_T comX, uint32_t parity);
void UART_SetStopBits(UartPort_T comX, uint32_t stopBits);
UartBlock_T *UART_FindBlockByPort(UartPort_T comX);

/* 回调函数 */
void UART_RxCpltCallback(UartPort_T comX, uint8_t *buffer, uint32_t length);
void UART_SetReceiveFinishCallback(UartPort_T comX, void (*finishCallback)(UartPort_T comX, uint8_t *buffer, uint32_t length));
void UART_SetTransmitStartCallback(UartPort_T comX, void (*finishCallback)(UartPort_T comX));
void UART_SetTransmitFinishCallback(UartPort_T comX, void (*finishCallback)(UartPort_T comX));

#ifdef __cplusplus
}
#endif

#endif