#include "bsp_uart.h"
/* Current scheduler serializes UART producers. Keep that ownership in the RTOS port. */
uint8_t Log_PortSend(uint8_t port, const char *data, uint16_t length)
{
    UartBlock_T *block = UART_FindBlockByPort((UartPort_T)port);
    if (!block || !block->TxEnable || FIFO_GetRemain(&block->TxFIFO) < length) return 0;
    UART_SendArray((UartPort_T)port, (uint8_t *)data, length);
    return 1;
}
void Log_PortReply(uint8_t port, const char *text)
{
    UART_SendString((UartPort_T)port, text);
}
