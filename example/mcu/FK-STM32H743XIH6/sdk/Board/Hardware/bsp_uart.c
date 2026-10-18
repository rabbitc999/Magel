#include "bsp_uart.h"
#include "bsp_periph.h"

/* 串口使能定义 */
#define UART1_ENABLE 1
#define UART2_ENABLE 1
#define UART3_ENABLE 1
#define UART4_ENABLE 1
#define UART5_ENABLE 1
#define UART6_ENABLE 1
#define UART7_ENABLE 1

/* 串口信息定义 */
#if defined(UART1_ENABLE) && UART1_ENABLE != 0
#define UART1_RX_BUFFER_SIZE      2 * 1024 /* 缓冲区大小 */
#define UART1_RX_TEMP_BUFFER_SIZE 1 * 1024 /* 临时缓冲区大小 */
#define UART1_TX_BUFFER_SIZE      2 * 1024 /* 缓冲区大小 */
#define UART1_TX_TEMP_BUFFER_SIZE 1 * 1024 /* 临时缓冲区大小 */
#define UART1_TX_PORT             GPIOA
#define UART1_TX_PIN              GPIO_PIN_9
#define UART1_RX_PORT             GPIOA
#define UART1_RX_PIN              GPIO_PIN_10
#endif

#if defined(UART2_ENABLE) && UART2_ENABLE != 0
#define UART2_RX_BUFFER_SIZE      2 * 1024 /* 缓冲区大小 */
#define UART2_RX_TEMP_BUFFER_SIZE 1 * 1024 /* 临时缓冲区大小 */
#define UART2_TX_BUFFER_SIZE      2 * 1024 /* 缓冲区大小 */
#define UART2_TX_TEMP_BUFFER_SIZE 1 * 1024 /* 临时缓冲区大小 */
#define UART2_TX_PORT             GPIOD
#define UART2_TX_PIN              GPIO_PIN_5
#define UART2_RX_PORT             GPIOD
#define UART2_RX_PIN              GPIO_PIN_6
#endif

#if defined(UART3_ENABLE) && UART3_ENABLE != 0
#define UART3_RX_BUFFER_SIZE      4 * 1024 /* 缓冲区大小 */
#define UART3_RX_TEMP_BUFFER_SIZE 2 * 1024 /* 临时缓冲区大小 */
#define UART3_TX_BUFFER_SIZE      256      /* 缓冲区大小 */
#define UART3_TX_TEMP_BUFFER_SIZE 256      /* 临时缓冲区大小 */
#define UART3_TX_PORT             GPIOB
#define UART3_TX_PIN              GPIO_PIN_10
#define UART3_RX_PORT             GPIOB
#define UART3_RX_PIN              GPIO_PIN_11
#endif

#if defined(UART4_ENABLE) && UART4_ENABLE != 0
#define UART4_RX_BUFFER_SIZE      2 * 1024 /* 缓冲区大小 */
#define UART4_RX_TEMP_BUFFER_SIZE 1 * 1024 /* 临时缓冲区大小 */
#define UART4_TX_BUFFER_SIZE      2 * 1024 /* 缓冲区大小 */
#define UART4_TX_TEMP_BUFFER_SIZE 1 * 1024 /* 临时缓冲区大小 */
#define UART4_TX_PORT             GPIOH
#define UART4_TX_PIN              GPIO_PIN_13
#define UART4_RX_PORT             GPIOH
#define UART4_RX_PIN              GPIO_PIN_14
#endif

#if defined(UART5_ENABLE) && UART5_ENABLE != 0
#define UART5_RX_BUFFER_SIZE      (8 * 1024) /* 缓冲区大小 */
#define UART5_RX_TEMP_BUFFER_SIZE 1024       /* 临时缓冲区大小 */
#define UART5_TX_BUFFER_SIZE      1024       /* 缓冲区大小 */
#define UART5_TX_TEMP_BUFFER_SIZE 1024       /* 临时缓冲区大小 */
#define UART5_TX_PORT             GPIOC
#define UART5_TX_PIN              GPIO_PIN_12
#define UART5_RX_PORT             GPIOB
#define UART5_RX_PIN              GPIO_PIN_12
#endif

#if defined(UART6_ENABLE) && UART6_ENABLE != 0
#define UART6_RX_BUFFER_SIZE      2 * 1024 /* 缓冲区大小 */
#define UART6_RX_TEMP_BUFFER_SIZE 1 * 1024 /* 临时缓冲区大小 */
#define UART6_TX_BUFFER_SIZE      0        /* 缓冲区大小 */
#define UART6_TX_TEMP_BUFFER_SIZE 0        /* 临时缓冲区大小 */
#define UART6_RX_PORT             GPIOG
#define UART6_RX_PIN              GPIO_PIN_9
#endif

#if defined(UART7_ENABLE) && UART7_ENABLE != 0
#define UART7_RX_BUFFER_SIZE      2 * 1024 /* 缓冲区大小 */
#define UART7_RX_TEMP_BUFFER_SIZE 1 * 1024 /* 临时缓冲区大小 */
#define UART7_TX_BUFFER_SIZE      0        /* 缓冲区大小 */
#define UART7_TX_TEMP_BUFFER_SIZE 0        /* 临时缓冲区大小 */
#define UART7_RX_PORT             GPIOA
#define UART7_RX_PIN              GPIO_PIN_8
#endif

#if defined(UART1_ENABLE) && UART1_ENABLE != 0
static uint8_t s_Uart1RxBuffer[UART1_RX_BUFFER_SIZE];
static uint8_t s_Uart1RxTempBuffer[UART1_RX_TEMP_BUFFER_SIZE] __attribute__((aligned(32)));
#if UART_TX_USE_BUFFER
static uint8_t s_Uart1TxBuffer[UART1_TX_BUFFER_SIZE];
static uint8_t s_Uart1TxTempBuffer[UART1_TX_TEMP_BUFFER_SIZE] __attribute__((aligned(32)));
#endif
static UartBlock_T s_Uart1Block = {
    .UART_Handle.Instance = USART1,
    .TxDMA_IRQnHandle.Instance = DMA1_Stream5,
    .TxDMA_IRQnIrqn = DMA1_Stream5_IRQn,
    .TxDMA_Request = DMA_REQUEST_USART1_TX,
    .RxDMA_IRQnHandle.Instance = DMA1_Stream0,
    .RxDMA_IRQnIrqn = DMA1_Stream0_IRQn,
    .RxDMA_Request = DMA_REQUEST_USART1_RX,
    .PortNumber = COM1,
    .RxFIFO.buffer = s_Uart1RxBuffer,
    .RxFIFO.size = UART1_RX_BUFFER_SIZE,
    .RxTempBuffer = s_Uart1RxTempBuffer,
    .RxTempBufferSize = UART1_RX_TEMP_BUFFER_SIZE,
#if UART_TX_USE_BUFFER
    .TxTempBuffer = s_Uart1TxTempBuffer,
    .TxTempBufferSize = UART1_TX_TEMP_BUFFER_SIZE,
    .TxFIFO.buffer = s_Uart1TxBuffer,
    .TxFIFO.size = UART1_TX_BUFFER_SIZE,
    .TxFIFO.esize = 1,
#endif
    .RxFIFO.esize = 1,
    .UART_IRQn = USART1_IRQn,
    .TxEnable = TRUE,
    .RxEnable = TRUE,
};
#endif

#if defined(UART2_ENABLE) && UART2_ENABLE != 0
static uint8_t s_Uart2RxBuffer[UART2_RX_BUFFER_SIZE];
static uint8_t s_Uart2RxTempBuffer[UART2_RX_TEMP_BUFFER_SIZE] __attribute__((aligned(32)));
#if UART_TX_USE_BUFFER
static uint8_t s_Uart2TxBuffer[UART2_TX_BUFFER_SIZE];
static uint8_t s_Uart2TxTempBuffer[UART2_TX_TEMP_BUFFER_SIZE] __attribute__((aligned(32)));
#endif
static UartBlock_T s_Uart2Block = {
    .UART_Handle.Instance = USART2,
    .TxDMA_IRQnHandle.Instance = DMA1_Stream6,
    .TxDMA_IRQnIrqn = DMA1_Stream6_IRQn,
    .TxDMA_Request = DMA_REQUEST_USART2_TX,
    .RxDMA_IRQnHandle.Instance = DMA1_Stream1,
    .RxDMA_IRQnIrqn = DMA1_Stream1_IRQn,
    .RxDMA_Request = DMA_REQUEST_USART2_RX,
    .PortNumber = COM2,
    .RxFIFO.buffer = s_Uart2RxBuffer,
    .RxFIFO.size = UART2_RX_BUFFER_SIZE,
    .RxTempBuffer = s_Uart2RxTempBuffer,
    .RxTempBufferSize = UART2_RX_TEMP_BUFFER_SIZE,
#if UART_TX_USE_BUFFER
    .TxTempBuffer = s_Uart2TxTempBuffer,
    .TxTempBufferSize = UART2_TX_TEMP_BUFFER_SIZE,
    .TxFIFO.buffer = s_Uart2TxBuffer,
    .TxFIFO.size = UART2_TX_BUFFER_SIZE,
    .TxFIFO.esize = 1,
#endif
    .RxFIFO.esize = 1,
    .UART_IRQn = USART2_IRQn,
    .TxEnable = TRUE,
    .RxEnable = TRUE,
};
#endif

#if defined(UART3_ENABLE) && UART3_ENABLE != 0
static uint8_t s_Uart3RxBuffer[UART3_RX_BUFFER_SIZE];
static uint8_t s_Uart3RxTempBuffer[UART3_RX_TEMP_BUFFER_SIZE] __attribute__((aligned(32)));
#if UART_TX_USE_BUFFER
static uint8_t s_Uart3TxBuffer[UART3_TX_BUFFER_SIZE];
static uint8_t s_Uart3TxTempBuffer[UART3_TX_TEMP_BUFFER_SIZE] __attribute__((aligned(32)));
#endif
// static UartBlock_T s_Uart3Block = {
UartBlock_T s_Uart3Block = {
    .UART_Handle.Instance = USART3,
    .TxDMA_IRQnHandle.Instance = DMA1_Stream7,
    .TxDMA_IRQnIrqn = DMA1_Stream7_IRQn,
    .TxDMA_Request = DMA_REQUEST_USART3_TX,
    .RxDMA_Request = DMA_REQUEST_USART3_RX,
    .RxDMA_IRQnHandle.Instance = DMA1_Stream2,
    .RxDMA_IRQnIrqn = DMA1_Stream2_IRQn,
    .PortNumber = COM3,
    .RxFIFO.buffer = s_Uart3RxBuffer,
    .RxFIFO.size = UART3_RX_BUFFER_SIZE,
    .RxTempBuffer = s_Uart3RxTempBuffer,
    .RxTempBufferSize = UART3_RX_TEMP_BUFFER_SIZE,
#if UART_TX_USE_BUFFER
    .TxTempBuffer = s_Uart3TxTempBuffer,
    .TxTempBufferSize = UART3_TX_TEMP_BUFFER_SIZE,
    .TxFIFO.buffer = s_Uart3TxBuffer,
    .TxFIFO.size = UART3_TX_BUFFER_SIZE,
    .TxFIFO.esize = 1,
#endif
    .RxFIFO.esize = 1,
    .UART_IRQn = USART3_IRQn,
    .TxEnable = TRUE,
    .RxEnable = TRUE,
};
#endif

#if defined(UART4_ENABLE) && UART4_ENABLE != 0
static uint8_t s_Uart4RxBuffer[UART4_RX_BUFFER_SIZE];
static uint8_t s_Uart4RxTempBuffer[UART4_RX_TEMP_BUFFER_SIZE] __attribute__((aligned(32)));
#if UART_TX_USE_BUFFER
static uint8_t s_Uart4TxBuffer[UART4_TX_BUFFER_SIZE];
static uint8_t s_Uart4TxTempBuffer[UART4_TX_TEMP_BUFFER_SIZE] __attribute__((aligned(32)));
#endif
static UartBlock_T s_Uart4Block = {
    .UART_Handle.Instance = UART4,
    .TxDMA_IRQnHandle.Instance = DMA2_Stream1,
    .TxDMA_IRQnIrqn = DMA2_Stream1_IRQn,
    .TxDMA_Request = DMA_REQUEST_UART4_TX,
    .RxDMA_Request = DMA_REQUEST_UART4_RX,
    .RxDMA_IRQnHandle.Instance = DMA1_Stream3,
    .RxDMA_IRQnIrqn = DMA1_Stream3_IRQn,
    .PortNumber = COM4,
    .RxFIFO.buffer = s_Uart4RxBuffer,
    .RxFIFO.size = UART4_RX_BUFFER_SIZE,
    .RxTempBuffer = s_Uart4RxTempBuffer,
    .RxTempBufferSize = UART4_RX_TEMP_BUFFER_SIZE,
#if UART_TX_USE_BUFFER
    .TxTempBuffer = s_Uart4TxTempBuffer,
    .TxTempBufferSize = UART4_TX_TEMP_BUFFER_SIZE,
    .TxFIFO.buffer = s_Uart4TxBuffer,
    .TxFIFO.size = UART4_TX_BUFFER_SIZE,
    .TxFIFO.esize = 1,
#endif
    .RxFIFO.esize = 1,
    .UART_IRQn = UART4_IRQn,
    .TxEnable = TRUE,
    .RxEnable = TRUE,
};
#endif

#if defined(UART5_ENABLE) && UART5_ENABLE != 0
static uint8_t s_Uart5RxBuffer[UART5_RX_BUFFER_SIZE];
static uint8_t s_Uart5RxTempBuffer[UART5_RX_TEMP_BUFFER_SIZE] __attribute__((aligned(32)));
#if UART_TX_USE_BUFFER
static uint8_t s_Uart5TxBuffer[UART5_TX_BUFFER_SIZE];
static uint8_t s_Uart5TxTempBuffer[UART5_TX_TEMP_BUFFER_SIZE] __attribute__((aligned(32)));
#endif
// static UartBlock_T s_Uart5Block = {
UartBlock_T s_Uart5Block = {
    .UART_Handle.Instance = UART5,
    .TxDMA_IRQnHandle.Instance = DMA2_Stream2,
    .TxDMA_IRQnIrqn = DMA2_Stream2_IRQn,
    .TxDMA_Request = DMA_REQUEST_UART5_TX,
    .RxDMA_Request = DMA_REQUEST_UART5_RX,
    .RxDMA_IRQnHandle.Instance = DMA1_Stream4,
    .RxDMA_IRQnIrqn = DMA1_Stream4_IRQn,
    .PortNumber = COM5,
    .RxFIFO.buffer = s_Uart5RxBuffer,
    .RxFIFO.size = UART5_RX_BUFFER_SIZE,
    .RxTempBuffer = s_Uart5RxTempBuffer,
    .RxTempBufferSize = UART5_RX_TEMP_BUFFER_SIZE,
#if UART_TX_USE_BUFFER
    .TxTempBuffer = s_Uart5TxTempBuffer,
    .TxTempBufferSize = UART5_TX_TEMP_BUFFER_SIZE,
    .TxFIFO.buffer = s_Uart5TxBuffer,
    .TxFIFO.size = UART5_TX_BUFFER_SIZE,
    .TxFIFO.esize = 1,
#endif
    .RxFIFO.esize = 1,
    .UART_IRQn = UART5_IRQn,
    .TxEnable = TRUE,
    .RxEnable = TRUE,
};
#endif

#if defined(UART6_ENABLE) && UART6_ENABLE != 0
static uint8_t s_Uart6RxBuffer[UART6_RX_BUFFER_SIZE];
static uint8_t s_Uart6RxTempBuffer[UART6_RX_TEMP_BUFFER_SIZE] __attribute__((aligned(32)));
#if UART_TX_USE_BUFFER
static uint8_t s_Uart6TxBuffer[UART6_TX_BUFFER_SIZE];
static uint8_t s_Uart6TxTempBuffer[UART5_TX_TEMP_BUFFER_SIZE];
#endif
static UartBlock_T s_Uart6Block = {
    .UART_Handle.Instance = USART6,
    .RxDMA_IRQnHandle.Instance = DMA2_Stream3,
    .RxDMA_IRQnIrqn = DMA2_Stream3_IRQn,
    .RxDMA_Request = DMA_REQUEST_USART6_RX,
    .PortNumber = COM6,
    .RxFIFO.buffer = s_Uart6RxBuffer,
    .RxFIFO.size = UART6_RX_BUFFER_SIZE,
    .RxTempBuffer = s_Uart6RxTempBuffer,
    .RxTempBufferSize = UART6_RX_TEMP_BUFFER_SIZE,
#if UART_TX_USE_BUFFER
    .TxTempBuffer = s_Uart6TxTempBuffer,
    .TxTempBufferSize = UART6_TX_TEMP_BUFFER_SIZE,
    .TxFIFO.buffer = s_Uart6TxBuffer,
    .TxFIFO.size = UART6_TX_BUFFER_SIZE,
    .TxFIFO.esize = 1,
#endif
    .RxFIFO.esize = 1,
    .UART_IRQn = USART6_IRQn,
    .TxEnable = FALSE,
    .RxEnable = TRUE,
};
#endif

#if defined(UART7_ENABLE) && UART7_ENABLE != 0
static uint8_t s_Uart7RxBuffer[UART7_RX_BUFFER_SIZE];
static uint8_t s_Uart7RxTempBuffer[UART7_RX_TEMP_BUFFER_SIZE] __attribute__((aligned(32)));
#if UART_TX_USE_BUFFER
static uint8_t s_Uart7TxBuffer[UART7_TX_BUFFER_SIZE];
static uint8_t s_Uart7TxTempBuffer[UART5_TX_TEMP_BUFFER_SIZE];
#endif
static UartBlock_T s_Uart7Block = {
    .UART_Handle.Instance = UART7,
    .RxDMA_IRQnHandle.Instance = DMA2_Stream4,
    .RxDMA_IRQnIrqn = DMA2_Stream4_IRQn,
    .RxDMA_Request = DMA_REQUEST_UART7_RX,
    .PortNumber = COM7,
    .RxFIFO.buffer = s_Uart7RxBuffer,
    .RxFIFO.size = UART7_RX_BUFFER_SIZE,
    .RxTempBuffer = s_Uart7RxTempBuffer,
    .RxTempBufferSize = UART7_RX_TEMP_BUFFER_SIZE,
#if UART_TX_USE_BUFFER
    .TxTempBuffer = s_Uart7TxTempBuffer,
    .TxTempBufferSize = UART7_TX_TEMP_BUFFER_SIZE,
    .TxFIFO.buffer = s_Uart7TxBuffer,
    .TxFIFO.size = UART7_TX_BUFFER_SIZE,
    .TxFIFO.esize = 1,
#endif
    .RxFIFO.esize = 1,
    .UART_IRQn = UART7_IRQn,
    .TxEnable = FALSE,
    .RxEnable = TRUE,
};
#endif

/* 串口接收缓存 */
static uint8_t s_UartBuffer[UART_BUFFER_SIZE];
static uint8_t s_UartRxBuffer[UART_BUFFER_SIZE];

/**
 * @brief  根据串口号查找对应的控制块结构体
 * @param  comX 目标串口号
 * @retval 返回对应的串口控制块指针，若找不到返回NULL
 */
UartBlock_T *UART_FindBlockByPort(UartPort_T comX)
{
    /* 寻找具体串口的控制块 */
#if defined(UART1_ENABLE) && UART1_ENABLE != 0
    if (comX == COM1)
    {
        return &s_Uart1Block;
    }
#endif
#if defined(UART2_ENABLE) && UART2_ENABLE != 0
    if (comX == COM2)
    {
        return &s_Uart2Block;
    }
#endif
#if defined(UART3_ENABLE) && UART3_ENABLE != 0
    if (comX == COM3)
    {
        return &s_Uart3Block;
    }
#endif
#if defined(UART4_ENABLE) && UART4_ENABLE != 0
    if (comX == COM4)
    {
        return &s_Uart4Block;
    }
#endif
#if defined(UART5_ENABLE) && UART5_ENABLE != 0
    if (comX == COM5)
    {
        return &s_Uart5Block;
    }
#endif
#if defined(UART6_ENABLE) && UART6_ENABLE != 0
    if (comX == COM6)
    {
        return &s_Uart6Block;
    }
#endif
#if defined(UART7_ENABLE) && UART7_ENABLE != 0
    if (comX == COM7)
    {
        return &s_Uart7Block;
    }
#endif
    return NULL;
}

/**
 * @brief  串口接收数据
 * @param  block 串口控制块指针
 */
static void UART_ReceiveToIdle(UartBlock_T *block)
{
    if (block == NULL || block->UART_Handle.RxState != HAL_UART_STATE_READY) { return; }
    block->RxDmaPosition = 0;
    block->RxSendTick = HAL_GetTick();
    if (block->RxDMA_IRQnHandle.Instance == NULL)
    {
        HAL_UARTEx_ReceiveToIdle_IT(&block->UART_Handle, block->RxTempBuffer, block->RxTempBufferSize);
    }
    else
    {
        /* Dedicated, aligned RX storage: DMA must not see dirty cache lines. */
        SCB_CleanInvalidateDCache_by_Addr((uint32_t *)block->RxTempBuffer, block->RxTempBufferSize);
        if (HAL_UARTEx_ReceiveToIdle_DMA(&block->UART_Handle, block->RxTempBuffer,
                                         block->RxTempBufferSize)
            != HAL_OK) { block->RxErrors++; }
        /* Keep HT and TC: a continuous stream may never generate IDLE. */
    }
}

/**
 * @brief  设置指定UART端口的波特率
 * @param  comX 目标UART端口号
 * @param  newBaud 期望设置的波特率值
 * @note   该函数会先禁用目标UART外设，重新计算并设置波特率参数，同时固定停止位为1位，最后重新启用UART外设。
 * 1. USART1/6使用独立的时钟源，其他USART使用另一组时钟源
 * 2. 设置前会自动读取当前UART的预分频器(Prescaler)配置
 * 3. 函数内部会自动禁用/启用UART，无需外部操作
 * 4. 波特率设置期间UART无法收发数据，建议在空闲时调用
 */
void UART_SetBaudRate(UartPort_T comX, uint32_t newBaud)
{
    uint32_t usartClk = 0;
    UartBlock_T *block = UART_FindBlockByPort(comX);
    if (block == NULL) { return; }
    if (block->RxDMA_IRQnHandle.Instance != NULL) { HAL_UART_AbortReceive(&block->UART_Handle); }
    USART_TypeDef *USARTx = block->UART_Handle.Instance;

    /* 获取USART时钟源频率 */
    if (USARTx == USART1 || USARTx == USART6)
    {
        usartClk = LL_RCC_GetUSARTClockFreq(LL_RCC_USART16_CLKSOURCE);
    }
    else
    {
        usartClk = LL_RCC_GetUSARTClockFreq(LL_RCC_USART234578_CLKSOURCE);
    }

    /* 获取当前Prescaler */
    uint32_t prescaler = LL_USART_GetPrescaler(USARTx);

    /* 必须先Disable */
    LL_USART_Disable(USARTx);

    /* 设置新波特率 */
    block->UART_Handle.Init.BaudRate = newBaud;
    LL_USART_SetBaudRate(USARTx, usartClk, prescaler, LL_USART_OVERSAMPLING_16, newBaud);
    LL_USART_Enable(USARTx);

    /* 重新启动接收 */
    UART_ReceiveToIdle(block);
}

/**
 * @brief  设置指定UART端口的校验位模式
 * @param  comX 目标UART端口号
 * @param  parity 期望设置的校验位类型（UART_PARITY_ODD/EVEN/NONE）
 * @note   该函数会先禁用目标UART外设，根据指定的校验位类型更新配置，并同步更新句柄中的校验位配置，最后重新启用UART外设。
 * 1. 校验位设置会影响UART帧格式，需与通信对端保持一致
 * 2. 函数内部会自动禁用/启用UART，无需外部操作
 * 3. 非法的校验位参数会被忽略，保持原有配置
 * 4. 校验位更改期间UART无法收发数据
 * 5. 启用校验位后，数据位会自动调整为7位，无校验时为8位(硬件默认)
 */
void UART_SetParity(UartPort_T comX, uint32_t parity)
{
    UartBlock_T *block = UART_FindBlockByPort(comX);
    if (block == NULL) { return; }
    if (block->RxDMA_IRQnHandle.Instance != NULL) { HAL_UART_AbortReceive(&block->UART_Handle); }
    USART_TypeDef *USARTx = block->UART_Handle.Instance;

    /* 必须先Disable */
    LL_USART_Disable(USARTx);
    block->UART_Handle.Init.Parity = parity;
    switch (parity)
    {
    case UART_PARITY_ODD: LL_USART_SetParity(USARTx, LL_USART_PARITY_ODD); break;
    case UART_PARITY_EVEN: LL_USART_SetParity(USARTx, LL_USART_PARITY_EVEN); break;
    case UART_PARITY_NONE: LL_USART_SetParity(USARTx, LL_USART_PARITY_NONE); break;
    default: break;
    }
    LL_USART_Enable(USARTx);

    /* 重新启动接收 */
    UART_ReceiveToIdle(block);
}

/**
 * @brief  设置指定UART端口的停止位长度
 * @param  comX 目标UART端口号
 * @param  stopBits 期望设置的停止位长度（UART_STOPBITS_0_5/1/1_5/2）
 * @note   该函数会先禁用目标UART外设，根据指定的停止位长度更新配置，并同步更新句柄中的停止位配置，最后重新启用UART外设。
 * 1. 停止位长度需与通信对端保持一致，否则会导致数据解析错误
 * 2. 函数内部会自动禁用/启用UART，无需外部操作
 * 3. 非法的停止位参数会被忽略，保持原有配置
 * 4. 停止位更改期间UART无法收发数据
 * 5. 0.5/1.5位停止位仅在USART工作在智能卡模式下有效(常规串口不推荐使用)
 */
void UART_SetStopBits(UartPort_T comX, uint32_t stopBits)
{
    UartBlock_T *block = UART_FindBlockByPort(comX);
    if (block == NULL) { return; }
    if (block->RxDMA_IRQnHandle.Instance != NULL) { HAL_UART_AbortReceive(&block->UART_Handle); }
    USART_TypeDef *USARTx = block->UART_Handle.Instance;

    /* 必须先Disable */
    LL_USART_Disable(USARTx);
    block->UART_Handle.Init.StopBits = stopBits;
    switch (stopBits)
    {
    case UART_STOPBITS_0_5: LL_USART_SetStopBitsLength(USARTx, LL_USART_STOPBITS_0_5); break;
    case UART_STOPBITS_1: LL_USART_SetStopBitsLength(USARTx, LL_USART_STOPBITS_1); break;
    case UART_STOPBITS_1_5: LL_USART_SetStopBitsLength(USARTx, LL_USART_STOPBITS_1_5); break;
    case UART_STOPBITS_2: LL_USART_SetStopBitsLength(USARTx, LL_USART_STOPBITS_2); break;
    default: break;
    }
    LL_USART_Enable(USARTx);

    /* 重新启动接收 */
    UART_ReceiveToIdle(block);
}

/**
 * @brief  根据串口外设地址查找对应的控制块结构体
 * @param  usartX 目标串口外设地址
 * @retval 返回对应的串口控制块指针，若找不到返回NULL
 */
static UartBlock_T *UART_FindBlockByDevice(USART_TypeDef *usartX)
{
    /* 寻找具体串口的控制块 */
    if (usartX == USART1)
    {
#if defined(UART1_ENABLE) && UART1_ENABLE != 0
        return &s_Uart1Block;
#else
        return NULL;
#endif
    }
    if (usartX == USART2)
    {
#if defined(UART2_ENABLE) && UART2_ENABLE != 0
        return &s_Uart2Block;
#else
        return NULL;
#endif
    }
    if (usartX == USART3)
    {
#if defined(UART3_ENABLE) && UART3_ENABLE != 0
        return &s_Uart3Block;
#else
        return NULL;
#endif
    }
    if (usartX == UART4)
    {
#if defined(UART4_ENABLE) && UART4_ENABLE != 0
        return &s_Uart4Block;
#else
        return NULL;
#endif
    }
    if (usartX == UART5)
    {
#if defined(UART5_ENABLE) && UART5_ENABLE != 0
        return &s_Uart5Block;
#else
        return NULL;
#endif
    }
    if (usartX == USART6)
    {
#if defined(UART6_ENABLE) && UART6_ENABLE != 0
        return &s_Uart6Block;
#else
        return NULL;
#endif
    }
    if (usartX == UART7)
    {
#if defined(UART7_ENABLE) && UART7_ENABLE != 0
        return &s_Uart7Block;
#else
        return NULL;
#endif
    }
    return NULL;
}

/**
 * @brief  设置UART开始接收回调函数
 * @param  comX UART端口号
 * @param  finishCallback 接收完成回调函数指针
 */
void UART_SetTransmitStartCallback(UartPort_T comX, void (*finishCallback)(UartPort_T comX))
{
    UartBlock_T *block = UART_FindBlockByPort(comX);
    if (block == NULL) { return; }
    block->transmitStartCallback = finishCallback;
}

/**
 * @brief  设置UART接收完成回调函数
 * @param  comX UART端口号
 * @param  finishCallback 接收完成回调函数指针
 */
void UART_SetReceiveFinishCallback(UartPort_T comX, void (*finishCallback)(UartPort_T comX, uint8_t *buffer, uint32_t length))
{
    UartBlock_T *block = UART_FindBlockByPort(comX);
    if (block == NULL) { return; }
    block->receiveFinishCallback = finishCallback;
}

/**
 * @brief  设置UART发送完成回调函数
 * @param  comX UART端口号
 * @param  finishCallback 发送完成回调函数指针
 * @note   回调函数会在中断上下文中执行，应保持简短，避免耗时操作。传入NULL可取消之前设置的回调函数。
 */
void UART_SetTransmitFinishCallback(UartPort_T comX, void (*finishCallback)(UartPort_T comX))
{
    UartBlock_T *block = UART_FindBlockByPort(comX);
    if (block == NULL) { return; }
    block->transmitFinishCallback = finishCallback;
}

/**
 * @brief  发送字节数组
 * @param  comX 目标串口号
 * @param  array 待发送数组指针
 * @param  size 数据大小(字节)
 */
void UART_SendArray(UartPort_T comX, uint8_t *array, uint16_t size)
{
    UartBlock_T *block = UART_FindBlockByPort(comX);
    if (block == NULL) { return; }
    if (!block->TxEnable) { return; }

#if UART_TX_USE_BUFFER
    /* 处理写入缓冲区 */
    if (FIFO_WriteBuffer(&block->TxFIFO, array, size) != FIFO_OK)
    {
        block->TxDroppedBytes += size;
    }

    /* 调用串口接收回调函数 */
    if (block->transmitStartCallback != NULL)
    {
        block->transmitStartCallback(comX);
    }
#else
    USART_TypeDef *USARTx = block->UART_Handle.Instance;

    /* 调用串口接收回调函数 */
    if (block->transmitStartCallback != NULL)
    {
        block->transmitStartCallback(comX);
    }

    /* 使用LL库传输串口内容 */
    for (uint16_t i = 0; i < size; i++)
    {
        LL_USART_TransmitData8(USARTx, array[i]);

        /* Wait for transmission complete */
        while (LL_USART_IsActiveFlag_TC(USARTx) == 0);
    }

    /* 调用串口接收回调函数 */
    if (block->transmitFinishCallback != NULL)
    {
        block->transmitFinishCallback(comX);
    }
#endif
}

/**
 * @brief  发送C字符串
 * @param  comX 目标串口号
 * @param  string 待发送的C字符串(以'\0'结尾)
 * @note   自动计算字符串长度(不包含终止符)
 */
void UART_SendString(UartPort_T comX, const char *string)
{
    uint32_t length = strnlen(string, UART_BUFFER_SIZE); /* 获取有效字符长度 */
    UART_SendArray(comX, (uint8_t *)string, length);     /* 类型转换后发送 */
}

/**
 * @brief  格式化输出（类似printf）
 * @param  comX 目标串口号
 * @param  format 格式化字符串
 */
void UART_Printf(UartPort_T comX, const char *format, ...)
{
    /* 创建临时缓冲区(足够大的静态缓冲区) */
    va_list arg;           /* 可变参数列表对象 */
    va_start(arg, format); /* 初始化可变参数 */
    uint16_t sendLength = vsnprintf((char *)s_UartBuffer, UART_BUFFER_SIZE, format, arg);
    if (sendLength > 0 && sendLength < UART_BUFFER_SIZE)
    {
        UART_SendArray(comX, s_UartBuffer, sendLength);
    }
    va_end(arg); /* 清理可变参数 */
}

/**
 * @brief  发送字节数组（立即发送，不使用缓冲区）
 * @param  comX 目标串口号
 * @param  array 待发送数组指针
 * @param  size 数据大小(字节)
 */
void UART_SendArrayImm(UartPort_T comX, uint8_t *array, uint16_t size)
{
    BSP_DISABLE_INT();

    // 1. 清除可能挂起的中断和标志
    UartBlock_T *block = UART_FindBlockByPort(comX);
    LL_USART_ClearFlag_TC(block->UART_Handle.Instance);
    FIFO_Clear(&block->TxFIFO);

    // 2. 确保USART使能并处于活动状态
    LL_USART_Enable(block->UART_Handle.Instance);

    // 3. 发送缓冲区数据
    for (uint8_t i = 0; i < size; i++)
    {
        LL_USART_TransmitData8(block->UART_Handle.Instance, array[i]);
        while (LL_USART_IsActiveFlag_TC(block->UART_Handle.Instance) == 0);
    }
    BSP_ENABLE_INT();
}

/**
 * @brief  发送C字符串（立即发送，不使用缓冲区）
 * @param  comX 目标串口号
 * @param  string 待发送的C字符串(以'\0'结尾)
 * @note   自动计算字符串长度(不包含终止符)
 */
void UART_SendStringImm(UartPort_T comX, const char *string)
{
    uint32_t length = strnlen(string, UART_BUFFER_SIZE); /* 获取有效字符长度 */
    UART_SendArrayImm(comX, (uint8_t *)string, length);  /* 类型转换后发送 */
}

/**
 * @brief  格式化输出（立即发送，不使用缓冲区）
 * @param  comX 目标串口号
 * @param  format 格式化字符串
 */
void UART_PrintfImm(UartPort_T comX, const char *format, ...)
{
    /* 创建临时缓冲区(足够大的静态缓冲区) */
    va_list arg;           /* 可变参数列表对象 */
    va_start(arg, format); /* 初始化可变参数 */
    uint16_t sendLength = vsnprintf((char *)s_UartBuffer, UART_BUFFER_SIZE, format, arg);
    if (sendLength > 0 && sendLength < UART_BUFFER_SIZE)
    {
        UART_SendArrayImm(comX, s_UartBuffer, sendLength);
    }
    va_end(arg); /* 清理可变参数 */
}

/**
 * @brief  格式化输出(可提供参数列表)
 * @param  comX 目标串口号
 * @param  format 格式化字符串
 * @param  arg 可变参数列表
 */
void UART_VaListPrintf(UartPort_T comX, const char *format, va_list arg)
{
    /* 创建临时缓冲区(足够大的静态缓冲区) */
    uint16_t sendLength = vsnprintf((char *)s_UartBuffer, UART_BUFFER_SIZE, format, arg);
    if (sendLength > 0 && sendLength < UART_BUFFER_SIZE)
    {
        UART_SendArray(comX, s_UartBuffer, sendLength);
    }
}

/**
 * @brief  清空指定串口的接收FIFO
 * @param  comX 目标串口号
 */
void UART_ClearRxFIFO(UartPort_T comX)
{
    /* 获取串口控制块 */
    UartBlock_T *block = UART_FindBlockByPort(comX);
    if (block == NULL || block->RxFIFO.size == 0) { return; }

    FIFO_Clear(&block->RxFIFO);
}

/**
 * @brief  清空指定串口的发送FIFO
 * @param  comX 目标串口号
 */
void UART_ClearTxFIFO(UartPort_T comX)
{
    /* 获取串口控制块 */
    UartBlock_T *block = UART_FindBlockByPort(comX);
    if (block == NULL || block->RxFIFO.size == 0) { return; }

    FIFO_Clear(&block->TxFIFO);
}

#if UART_TX_USE_BUFFER
/**
 * @brief  处理发送的数据
 * @param  comX 目标串口号
 * @retval 发送状态
 */
UartTxState_T UART_ProcessTx(UartPort_T comX)
{
    /* 获取串口控制块 */
    UartBlock_T *block = UART_FindBlockByPort(comX);
    if (block == NULL || block->RxFIFO.size == 0) { return UART_TX_STATE_ERROR; }

    /* 检查串口是否正在发送 */
    if (block->TxBusyFlag)
    {
        /* 串口忙，但检查FIFO中是否还有数据 */
        if (FIFO_GetLength(&block->TxFIFO) > 0)
        {
            return UART_TX_STATE_HAS_DATA; /* 还有数据等待发送 */
        }
        return UART_TX_STATE_BUSY; /* 仅忙，无待发数据 */
    }

    /* 尝试从FIFO读取数据 */
    if (block->TxPendingLength == 0)
    {
        block->TxPendingLength = FIFO_ReadBuffer(&block->TxFIFO, block->TxTempBuffer, block->TxTempBufferSize);
    }
    uint16_t dataLength = block->TxPendingLength;

    if (dataLength > 0)
    {
        /* 有数据需要发送，启动发送 */
        block->TxBusyFlag = TRUE;
        SCB_CleanDCache_by_Addr((uint32_t *)block->TxTempBuffer, (dataLength + 31U) & ~31U);
        HAL_StatusTypeDef status = HAL_UART_Transmit_DMA(&block->UART_Handle, block->TxTempBuffer, dataLength);

        /* 发送完成处理 */
        if (status == HAL_OK)
        {
            block->TxBytes += dataLength;
            block->TxPendingLength = 0;
            __HAL_DMA_DISABLE_IT(&block->TxDMA_IRQnHandle, DMA_IT_HT);
            /* 检查FIFO中是否还有剩余数据 */
            if (FIFO_GetLength(&block->TxFIFO) > 0)
            {
                return UART_TX_STATE_HAS_DATA; /* 本次发送已启动，但还有更多数据 */
            }
            return UART_TX_STATE_BUSY; /* 本次发送已启动，无更多数据 */
        }
        else
        {
            /* 发送启动失败，重置标志 */
            block->TxBusyFlag = FALSE;
            block->TxErrors++;
            return UART_TX_STATE_HAS_DATA;
        }
    }
    else
    {
        /* 调用串口接收回调函数 */
        if (block->transmitFinishCallback != NULL)
        {
            block->transmitFinishCallback(comX);
        }

        /* FIFO中没有数据 */
        return UART_TX_STATE_COMPLETE;
    }
}
#endif

/**
 * @brief  处理接收的数据
 * @param  comX 目标串口号
 * @retval 接收状态
 */
UartRxState_T UART_ProcessRx(UartPort_T comX)
{
    /* 获取串口控制块 */
    UartBlock_T *block = UART_FindBlockByPort(comX);
    if (block == NULL || block->RxFIFO.size == 0) { return UART_RX_STATE_ERROR; }

    /* 判断数据长度，只有在有数据时才进行接收 */
    uint32_t rxPrimask = __get_PRIMASK();
    __disable_irq();
    uint16_t dataLength = FIFO_ReadBuffer(&block->RxFIFO, s_UartRxBuffer, UART_BUFFER_SIZE);
    __set_PRIMASK(rxPrimask);

    /* 调用原始数据回调 */
    if (dataLength > 0)
    {
        UART_RxCpltCallback(block->PortNumber, s_UartRxBuffer, dataLength);
        if (FIFO_GetLength(&block->RxFIFO) > 0) { return UART_RX_STATE_HAS_DATA; }
        else
        {
            return UART_RX_STATE_COMPLETE;
        }
    }

    /* 未接收到数据，判断接收超时时间，如果超时则重新接收 */
    if ((Timer_GetDiffMs(block->RxSendTick)) > UART_RX_TIMEOUT)
    {
        UART_ReceiveToIdle(block);
        return UART_RX_STATE_ERROR;
    }

    return UART_RX_STATE_COMPLETE;
}

/**
 * @brief  检查接收FIFO中是否包含指定字节数组
 * @param  comX 目标串口号
 * @param  array 待查找的字节数组指针
 * @param  size 数组长度
 * @retval 1表示已收到，0表示未收到
 */
uint8_t UART_IsReceivedArray(UartPort_T comX, uint8_t *array, uint16_t size)
{
    /* 获取串口控制块 */
    UartBlock_T *block = UART_FindBlockByPort(comX);
    if (block == NULL || block->RxFIFO.size == 0) { return 0; }

    /* 使用KMP算法查找字节串 */
    return FIFO_IsContains(&block->RxFIFO, array, size);
}

/**
 * @brief  检查接收FIFO中是否包含指定字符串
 * @param  comX 目标串口号
 * @param  string 待查找的C字符串
 * @retval 1表示已收到，0表示未收到
 */
uint8_t UART_IsReceivedString(UartPort_T comX, const char *string)
{
    uint32_t length = strnlen(string, UART_BUFFER_SIZE);
    return UART_IsReceivedArray(comX, (uint8_t *)string, length);
}

/**
 * @brief  串口基础初始化函数
 * @param  comX 目标串口号
 */
void UART_BaseInit(UartPort_T comX)
{
    UartBlock_T *block = UART_FindBlockByPort(comX);
    if (block == NULL) { return; }

    /* 停止串口中断 */
    HAL_NVIC_DisableIRQ(block->UART_IRQn);
    HAL_NVIC_DisableIRQ(block->RxDMA_IRQnIrqn);

    /* 停止当前传输 */
    HAL_UART_Abort(&block->UART_Handle);
    block->TxBusyFlag = FALSE;
    block->TxPendingLength = 0;
    HAL_UART_Abort(&block->UART_Handle);

    /* 反初始化 */
    HAL_UART_DeInit(&block->UART_Handle);

    /* USART硬件参数配置 */
    block->UART_Handle.Instance = block->UART_Handle.Instance;
    block->UART_Handle.Init.BaudRate = 115200;
    block->UART_Handle.Init.WordLength = UART_WORDLENGTH_8B;
    block->UART_Handle.Init.StopBits = UART_STOPBITS_1;
    block->UART_Handle.Init.Parity = UART_PARITY_NONE;

    if (block->RxEnable && !block->TxEnable)
    {
        block->UART_Handle.Init.Mode = UART_MODE_RX;
    }
    else if (block->TxEnable && !block->RxEnable)
    {
        block->UART_Handle.Init.Mode = UART_MODE_TX;
    }
    else
    {
        block->UART_Handle.Init.Mode = UART_MODE_TX_RX;
    }

    block->UART_Handle.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    block->UART_Handle.Init.OverSampling = UART_OVERSAMPLING_16;
    block->UART_Handle.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
    block->UART_Handle.Init.ClockPrescaler = UART_PRESCALER_DIV1;
    block->UART_Handle.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
    block->receiveFinishCallback = NULL;
    if (HAL_UART_Init(&block->UART_Handle) != HAL_OK)
    {
        Assert_Failed(__FILE__, __LINE__);
    }
    if (HAL_UARTEx_SetTxFifoThreshold(&block->UART_Handle, UART_TXFIFO_THRESHOLD_1_8) != HAL_OK)
    {
        Assert_Failed(__FILE__, __LINE__);
    }
    if (HAL_UARTEx_SetRxFifoThreshold(&block->UART_Handle, UART_RXFIFO_THRESHOLD_1_8) != HAL_OK)
    {
        Assert_Failed(__FILE__, __LINE__);
    }
    if (HAL_UARTEx_DisableFifoMode(&block->UART_Handle) != HAL_OK)
    {
        Assert_Failed(__FILE__, __LINE__);
    }

    /* 使能中断 */
    LL_USART_EnableIT_IDLE(block->UART_Handle.Instance);

    /* TX uses normal DMA: only queued bytes are sent, with no repetition. */
    if (block->TxEnable && block->TxDMA_IRQnHandle.Instance != NULL)
    {
        __HAL_RCC_DMA1_CLK_ENABLE();
        __HAL_RCC_DMA2_CLK_ENABLE();
        DMA_HandleTypeDef *dma = &block->TxDMA_IRQnHandle;
        dma->Init.Request = block->TxDMA_Request;
        dma->Init.Direction = DMA_MEMORY_TO_PERIPH;
        dma->Init.PeriphInc = DMA_PINC_DISABLE;
        dma->Init.MemInc = DMA_MINC_ENABLE;
        dma->Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
        dma->Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
        dma->Init.Mode = DMA_NORMAL;
        dma->Init.Priority = DMA_PRIORITY_LOW;
        dma->Init.FIFOMode = DMA_FIFOMODE_DISABLE;
        if (HAL_DMA_Init(dma) != HAL_OK) Assert_Failed(__FILE__, __LINE__);
        __HAL_LINKDMA(&block->UART_Handle, hdmatx, block->TxDMA_IRQnHandle);
        BSP_NVIC_Init(block->TxDMA_IRQnIrqn, 7, 0);
    }

    /* RX DMA初始化 */
    if (block->RxDMA_IRQnHandle.Instance != NULL)
    {
        __HAL_RCC_DMA1_CLK_ENABLE();
        __HAL_RCC_DMA2_CLK_ENABLE();
        HAL_DMA_DeInit(&block->RxDMA_IRQnHandle);

        block->RxDMA_IRQnHandle.Init.Request = block->RxDMA_Request;
        block->RxDMA_IRQnHandle.Init.Direction = DMA_PERIPH_TO_MEMORY;
        block->RxDMA_IRQnHandle.Init.PeriphInc = DMA_PINC_DISABLE;
        block->RxDMA_IRQnHandle.Init.MemInc = DMA_MINC_ENABLE;
        block->RxDMA_IRQnHandle.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
        block->RxDMA_IRQnHandle.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
        block->RxDMA_IRQnHandle.Init.Mode = DMA_CIRCULAR;
        block->RxDMA_IRQnHandle.Init.Priority = DMA_PRIORITY_VERY_HIGH;
        block->RxDMA_IRQnHandle.Init.FIFOMode = DMA_FIFOMODE_DISABLE;

        if (HAL_DMA_Init(&block->RxDMA_IRQnHandle) != HAL_OK)
        {
            Assert_Failed(__FILE__, __LINE__);
        }
        __HAL_LINKDMA(&block->UART_Handle, hdmarx, block->RxDMA_IRQnHandle);

        BSP_NVIC_Init(block->RxDMA_IRQnIrqn, 7, 0);
    }

    block->UART_Handle.Instance->ICR = 0xFFFFFFFF;

    /* 开启串口中断 */
    BSP_NVIC_Init(block->UART_IRQn, 7, 0);

    /* 接收串口数据 */
    UART_ReceiveToIdle(block);
    block->InitFlag = 1;
}

/**
 * @brief  初始化所有串口
 */
void UART_Init(void)
{
    RCC_PeriphCLKInitTypeDef periphClkInitStruct = { 0 };

    __HAL_RCC_DMA1_CLK_ENABLE();
    __HAL_RCC_USART1_CLK_ENABLE();
    __HAL_RCC_USART2_CLK_ENABLE();
    __HAL_RCC_USART3_CLK_ENABLE();
    __HAL_RCC_UART4_CLK_ENABLE();
    __HAL_RCC_UART5_CLK_ENABLE();
    __HAL_RCC_USART6_CLK_ENABLE();
    __HAL_RCC_UART7_CLK_ENABLE();

    periphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_USART1 | RCC_PERIPHCLK_USART2 | RCC_PERIPHCLK_USART3 | RCC_PERIPHCLK_UART4 | RCC_PERIPHCLK_UART5 | RCC_PERIPHCLK_USART6 | RCC_PERIPHCLK_UART7;
    periphClkInitStruct.Usart16ClockSelection = RCC_USART16CLKSOURCE_D2PCLK2;
    periphClkInitStruct.Usart234578ClockSelection = RCC_USART234578CLKSOURCE_D2PCLK1;
    if (HAL_RCCEx_PeriphCLKConfig(&periphClkInitStruct) != HAL_OK)
    {
        Assert_Failed(__FILE__, __LINE__);
    }

    periphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_USART1;
    periphClkInitStruct.Usart16ClockSelection = RCC_USART16CLKSOURCE_D2PCLK2;
    if (HAL_RCCEx_PeriphCLKConfig(&periphClkInitStruct) != HAL_OK)
    {
        Assert_Failed(__FILE__, __LINE__);
    }

#if defined(UART1_ENABLE) && UART1_ENABLE != 0
    BSP_GPIO_InitEx(UART1_TX_PORT, UART1_TX_PIN, OUT_AF_PP, GPIO_AF7_USART1);
    BSP_GPIO_InitEx(UART1_RX_PORT, UART1_RX_PIN, OUT_AF_PP, GPIO_AF7_USART1);
    UART_BaseInit(COM1);
#endif

#if defined(UART2_ENABLE) && UART2_ENABLE != 0
    BSP_GPIO_InitEx(UART2_TX_PORT, UART2_TX_PIN, OUT_AF_PP, GPIO_AF7_USART2);
    BSP_GPIO_InitEx(UART2_RX_PORT, UART2_RX_PIN, OUT_AF_PP, GPIO_AF7_USART2);
    UART_BaseInit(COM2);
#endif

#if defined(UART3_ENABLE) && UART3_ENABLE != 0
    BSP_GPIO_InitEx(UART3_TX_PORT, UART3_TX_PIN, OUT_AF_PP, GPIO_AF7_USART3);
    BSP_GPIO_InitEx(UART3_RX_PORT, UART3_RX_PIN, OUT_AF_PP, GPIO_AF7_USART3);
    UART_BaseInit(COM3);
#endif

#if defined(UART4_ENABLE) && UART4_ENABLE != 0
    BSP_GPIO_InitEx(UART4_TX_PORT, UART4_TX_PIN, OUT_AF_PP, GPIO_AF8_UART4);
    BSP_GPIO_InitEx(UART4_RX_PORT, UART4_RX_PIN, OUT_AF_PP, GPIO_AF8_UART4);
    UART_BaseInit(COM4);
#endif

#if defined(UART5_ENABLE) && UART5_ENABLE != 0
    BSP_GPIO_InitEx(UART5_TX_PORT, UART5_TX_PIN, OUT_AF_PP, GPIO_AF8_UART5);
    BSP_GPIO_InitEx(UART5_RX_PORT, UART5_RX_PIN, OUT_AF_PP, GPIO_AF14_UART5);
    UART_BaseInit(COM5);
#endif

#if defined(UART6_ENABLE) && UART6_ENABLE != 0
    BSP_GPIO_InitEx(UART6_RX_PORT, UART6_RX_PIN, OUT_AF_PP, GPIO_AF7_USART6);
    UART_BaseInit(COM6);
#endif

#if defined(UART7_ENABLE) && UART7_ENABLE != 0
    BSP_GPIO_InitEx(UART7_RX_PORT, UART7_RX_PIN, OUT_AF_PP, GPIO_AF11_UART7);
    UART_BaseInit(COM7);
#endif
}

/**
 * @brief  接收事件回调函数(在使用高级接收服务后调用的Rx事件通知)
 * @param  huart UART句柄
 * @param  size 应用接收缓冲区中可用的数据量
 */
static void UART_PublishRx(UartBlock_T *block, uint32_t offset, uint32_t length)
{
    if (length == 0) { return; }
    uint8_t *data = block->RxTempBuffer + offset;
    if (block->RxDMA_IRQnHandle.Instance != NULL)
    {
        uintptr_t first = (uintptr_t)data & ~(uintptr_t)31;
        uintptr_t end = ((uintptr_t)data + length + 31) & ~(uintptr_t)31;
        SCB_InvalidateDCache_by_Addr((uint32_t *)first, end - first);
    }
    if (FIFO_WriteBuffer(&block->RxFIFO, data, length) != FIFO_OK)
    {
        block->RxDroppedBytes += length;
        return;
    }
    block->RxBytes += length;
    if (block->receiveFinishCallback != NULL)
    {
        block->receiveFinishCallback(block->PortNumber, data, length);
    }
}

void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t size)
{
    UartBlock_T *block = UART_FindBlockByDevice(huart->Instance);
    if (block == NULL) { return; }
    uint32_t primask = __get_PRIMASK();
    __disable_irq();
    HAL_UART_RxEventTypeTypeDef event = HAL_UARTEx_GetRxEventType(huart);
    if (event == HAL_UART_RXEVENT_IDLE) { block->RxIdleEvents++; }
    else if (event == HAL_UART_RXEVENT_HT) { block->RxHalfEvents++; }
    else if (event == HAL_UART_RXEVENT_TC) { block->RxFullEvents++; }
    block->RxSendTick = HAL_GetTick();
    if (block->RxDMA_IRQnHandle.Instance != NULL)
    {
        /* Read the live cursor so a delayed HT IRQ cannot replay an older IDLE span. */
        uint32_t capacity = block->RxTempBufferSize;
        uint32_t position = capacity - __HAL_DMA_GET_COUNTER(&block->RxDMA_IRQnHandle);
        if (position == capacity) { position = 0; }
        uint32_t previous = block->RxDmaPosition;
        if (position < previous)
        {
            UART_PublishRx(block, previous, capacity - previous);
            previous = 0;
        }
        if (position > previous) { UART_PublishRx(block, previous, position - previous); }
        block->RxDmaPosition = position;
        /* Circular DMA stays enabled across HT, TC and IDLE. */
    }
    else
    {
        UART_PublishRx(block, 0, size);
        UART_ReceiveToIdle(block);
    }
    __set_PRIMASK(primask);
}

#if UART_TX_USE_BUFFER
/**
 * @brief  发送事件回调函数(在使用发送服务后调用的Tx事件通知)
 * @param  huart UART句柄
 */
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
    UartBlock_T *block = UART_FindBlockByDevice(huart->Instance);
    if (block == NULL) { return; }
    block->TxCompleteEvents++;
    block->TxBusyFlag = FALSE;
}
#endif

/**
 * @brief  事件错误回调函数
 * @param  huart UART句柄
 */
void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    UartBlock_T *block = UART_FindBlockByDevice(huart->Instance);
    if (block == NULL) { return; }
    block->RxErrors++;
    if ((huart->ErrorCode & HAL_UART_ERROR_DMA) != 0U && huart->gState == HAL_UART_STATE_READY)
    {
        block->TxErrors++;
        block->TxBusyFlag = FALSE;
    }
    __HAL_UART_CLEAR_FLAG(huart, UART_CLEAR_OREF | UART_CLEAR_NEF | UART_CLEAR_FEF | UART_CLEAR_PEF);
    if (huart->RxState == HAL_UART_STATE_READY) { UART_ReceiveToIdle(block); }
}

/**
 * @brief  清空标志位
 * @param  huart UART句柄
 */
void UART_ClearFlagIrq(UART_HandleTypeDef *huart)
{
    LL_USART_ClearFlag_TXFE(huart->Instance);
}

/* 以下为中断处理函数 */
#if defined(UART1_ENABLE) && UART1_ENABLE != 0
/**
 * @brief  USART1全局中断处理函数
 */
void USART1_IRQHandler(void)
{
    HAL_UART_IRQHandler(&s_Uart1Block.UART_Handle);
    UART_ClearFlagIrq(&s_Uart1Block.UART_Handle);
}

/**
 * @brief  DMA1_Stream0全局中断处理函数
 */
void DMA1_Stream0_IRQHandler(void)
{
    HAL_DMA_IRQHandler(&s_Uart1Block.RxDMA_IRQnHandle);
}
#endif

#if defined(UART2_ENABLE) && UART2_ENABLE != 0
/**
 * @brief  USART2全局中断处理函数
 */
void USART2_IRQHandler(void)
{
    HAL_UART_IRQHandler(&s_Uart2Block.UART_Handle);
    UART_ClearFlagIrq(&s_Uart2Block.UART_Handle);
}

/**
 * @brief  DMA1_Stream1全局中断处理函数
 */
void DMA1_Stream1_IRQHandler(void)
{
    HAL_DMA_IRQHandler(&s_Uart2Block.RxDMA_IRQnHandle);
}
#endif

#if defined(UART3_ENABLE) && UART3_ENABLE != 0
/**
 * @brief  USART3全局中断处理函数
 */
void USART3_IRQHandler(void)
{
    HAL_UART_IRQHandler(&s_Uart3Block.UART_Handle);
    UART_ClearFlagIrq(&s_Uart3Block.UART_Handle);
}

/**
 * @brief  DMA1_Stream2全局中断处理函数
 */
void DMA1_Stream2_IRQHandler(void)
{
    HAL_DMA_IRQHandler(&s_Uart3Block.RxDMA_IRQnHandle);
}
#endif

#if defined(UART4_ENABLE) && UART4_ENABLE != 0
/**
 * @brief  UART4全局中断处理函数
 */
void UART4_IRQHandler(void)
{
    HAL_UART_IRQHandler(&s_Uart4Block.UART_Handle);
    UART_ClearFlagIrq(&s_Uart4Block.UART_Handle);
}

/**
 * @brief  DMA1_Stream3全局中断处理函数
 */
void DMA1_Stream3_IRQHandler(void)
{
    HAL_DMA_IRQHandler(&s_Uart4Block.RxDMA_IRQnHandle);
}
#endif

#if defined(UART5_ENABLE) && UART5_ENABLE != 0
/**
 * @brief  UART5全局中断处理函数
 */
void UART5_IRQHandler(void)
{
    HAL_UART_IRQHandler(&s_Uart5Block.UART_Handle);
    UART_ClearFlagIrq(&s_Uart5Block.UART_Handle);
}

/**
 * @brief  DMA1_Stream4全局中断处理函数
 */
void DMA1_Stream4_IRQHandler(void)
{
    HAL_DMA_IRQHandler(&s_Uart5Block.RxDMA_IRQnHandle);
}
#endif

#if defined(UART6_ENABLE) && UART6_ENABLE != 0
/**
 * @brief  USART6全局中断处理函数
 */
void USART6_IRQHandler(void)
{
    HAL_UART_IRQHandler(&s_Uart6Block.UART_Handle);
    UART_ClearFlagIrq(&s_Uart6Block.UART_Handle);
}
#endif

#if defined(UART7_ENABLE) && UART7_ENABLE != 0
/**
 * @brief  UART7全局中断处理函数
 */
void UART7_IRQHandler(void)
{
    HAL_UART_IRQHandler(&s_Uart7Block.UART_Handle);
    UART_ClearFlagIrq(&s_Uart7Block.UART_Handle);
}
#endif

/**
 * @brief   接收完成回调函数
 */
__WEAK void UART_RxCpltCallback(UartPort_T comX, uint8_t *buffer, uint32_t length)
{
}
#if defined(UART1_ENABLE) && UART1_ENABLE != 0
void DMA1_Stream5_IRQHandler(void)
{
    HAL_DMA_IRQHandler(&s_Uart1Block.TxDMA_IRQnHandle);
}
#endif

#if defined(UART2_ENABLE) && UART2_ENABLE != 0
void DMA1_Stream6_IRQHandler(void)
{
    HAL_DMA_IRQHandler(&s_Uart2Block.TxDMA_IRQnHandle);
}
#endif

#if defined(UART3_ENABLE) && UART3_ENABLE != 0
void DMA1_Stream7_IRQHandler(void)
{
    HAL_DMA_IRQHandler(&s_Uart3Block.TxDMA_IRQnHandle);
}
#endif

#if defined(UART4_ENABLE) && UART4_ENABLE != 0
void DMA2_Stream1_IRQHandler(void)
{
    HAL_DMA_IRQHandler(&s_Uart4Block.TxDMA_IRQnHandle);
}
#endif

#if defined(UART5_ENABLE) && UART5_ENABLE != 0
void DMA2_Stream2_IRQHandler(void)
{
    HAL_DMA_IRQHandler(&s_Uart5Block.TxDMA_IRQnHandle);
}
#endif

#if defined(UART6_ENABLE) && UART6_ENABLE != 0
void DMA2_Stream3_IRQHandler(void)
{
    HAL_DMA_IRQHandler(&s_Uart6Block.RxDMA_IRQnHandle);
}
#endif

#if defined(UART7_ENABLE) && UART7_ENABLE != 0
void DMA2_Stream4_IRQHandler(void)
{
    HAL_DMA_IRQHandler(&s_Uart7Block.RxDMA_IRQnHandle);
}
#endif
