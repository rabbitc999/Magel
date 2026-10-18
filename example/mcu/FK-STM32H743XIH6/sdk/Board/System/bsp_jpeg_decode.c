#include "bsp_jpeg_decode.h"

JPEG_HandleTypeDef g_hJpeg;          /* JPEG句柄 */
extern DMA2D_HandleTypeDef g_hdma2d; /* DMA2D句柄 声明 ，在 lcd_rgb.c 定义 */

#define CHUNK_SIZE_IN  ((uint32_t)(64 * 1024)) /* 单次解码输入数据最大长度 */
#define CHUNK_SIZE_OUT ((uint32_t)(64 * 1024)) /* 单次解码输出数据最大长度 */

__IO uint8_t s_JpegHwDecodingState = 0; /* 解码状态标志，为 1 时表示完成解码 */

uint32_t g_frameBufferAddress; /* 解码输出缓冲区地址 */
uint32_t g_jpegSourceAddress;  /* JPG图片源地址 */
uint32_t g_inputFrameSize;     /* 当前输入缓冲区地址 */
uint32_t g_inputFrameIndex;    /* 地址索引 */

/**
 * @brief  JPEG底层初始化、MDMA初始化以及中断配置
 * @param  hJpeg JPEG句柄
 * @note   配置JPEG硬件相关的时钟、中断和MDMA传输
 */
void Jpeg_MspInit(JPEG_HandleTypeDef *hJpeg)
{
    static MDMA_HandleTypeDef s_hmdmaIn;  /* Input MDMA */
    static MDMA_HandleTypeDef s_hmdmaOut; /* Output MDMA */

    __HAL_RCC_JPGDECEN_CLK_ENABLE(); /* 使能JPEG解码器时钟 */
    __HAL_RCC_MDMA_CLK_ENABLE();     /* 使能MDMA时钟 */

    HAL_NVIC_SetPriority(JPEG_IRQn, 0x07, 0x0F); /* 设置中断优先级 */
    HAL_NVIC_EnableIRQ(JPEG_IRQn);               /* 使能中断 */

    /* Input MDMA  设置 */
    s_hmdmaIn.Init.Priority = MDMA_PRIORITY_HIGH;                /* 优先级高 */
    s_hmdmaIn.Init.Endianness = MDMA_LITTLE_ENDIANNESS_PRESERVE; /* 小端字节格式，不使用交换 */

    /* 若使用 emWin小工具 "Bin2C.exe" 进行转换图片转换，得到的图片数组是按字节存储的，因此相应的MDMA设置需要设置成8位 */
    s_hmdmaIn.Init.SourceInc = MDMA_SRC_INC_BYTE;              /* 源地址按照全字递增(8 bits) */
    s_hmdmaIn.Init.SourceDataSize = MDMA_SRC_DATASIZE_BYTE;    /* 源地址数据宽度为全字(8 bits) */
    s_hmdmaIn.Init.SourceBurst = MDMA_SOURCE_BURST_32BEATS;    /* 源数据单次突发传输 */
    s_hmdmaIn.Init.SourceBlockAddressOffset = 0;               /* 数据偏移为0 */
    s_hmdmaIn.Init.DestinationInc = MDMA_DEST_INC_DISABLE;     /* 禁止目标地址自增 */
    s_hmdmaIn.Init.DestDataSize = MDMA_DEST_DATASIZE_WORD;     /* 目标地址数据宽度 */
    s_hmdmaIn.Init.DestBurst = MDMA_DEST_BURST_16BEATS;        /* 目标数据单次突发传输 */
    s_hmdmaIn.Init.DestBlockAddressOffset = 0;                 /* 数据偏移为0 */
    s_hmdmaIn.Init.DataAlignment = MDMA_DATAALIGN_PACKENABLE;  /* 所有字节右对齐，使用小端格式 */
    s_hmdmaIn.Init.Request = MDMA_REQUEST_JPEG_INFIFO_TH;      /* 使用 JPEG FIFO 阈值触发MDMA */
    s_hmdmaIn.Init.TransferTriggerMode = MDMA_BUFFER_TRANSFER; /* 使用缓冲区传输 */
    s_hmdmaIn.Init.BufferTransferLength = 32;                  /* 缓冲区单次传输数据长度 */
    s_hmdmaIn.Instance = MDMA_Channel7;                        /* 使用通道17 */

    HAL_MDMA_DeInit(&s_hmdmaIn);             /* 恢复默认设置 */
    HAL_MDMA_Init(&s_hmdmaIn);               /* 根据上面的配置进行初始化 */
    __HAL_LINKDMA(hJpeg, hdmain, s_hmdmaIn); /* 关联MDMA句柄 */

    /* Output MDMA  设置 */
    s_hmdmaOut.Init.Priority = MDMA_PRIORITY_VERY_HIGH;           /* 优先级最高 */
    s_hmdmaOut.Init.Endianness = MDMA_LITTLE_ENDIANNESS_PRESERVE; /* 小端字节格式，不使用交换 */
    s_hmdmaOut.Init.SourceInc = MDMA_SRC_INC_DISABLE;             /* 源地址禁止递增 */
    s_hmdmaOut.Init.SourceDataSize = MDMA_SRC_DATASIZE_WORD;      /* 源地址数据宽度为全字(32 bits) */
    s_hmdmaOut.Init.SourceBurst = MDMA_SOURCE_BURST_32BEATS;      /* 源数据单次突发传输 */
    s_hmdmaOut.Init.SourceBlockAddressOffset = 0;                 /* 数据偏移为0 */
    s_hmdmaOut.Init.DestinationInc = MDMA_DEST_INC_HALFWORD;      /* 目标地址按照半字递增(16 bits) */
    s_hmdmaOut.Init.DestDataSize = MDMA_DEST_DATASIZE_HALFWORD;   /* 目标地址数据宽度半字(16 bits) */
    s_hmdmaOut.Init.DestBurst = MDMA_DEST_BURST_32BEATS;          /* 目标数据单次突发传输 */
    s_hmdmaOut.Init.DestBlockAddressOffset = 0;                   /* 数据偏移为0 */
    s_hmdmaOut.Init.DataAlignment = MDMA_DATAALIGN_PACKENABLE;    /* 所有字节右对齐，使用小端格式 */
    s_hmdmaOut.Init.Request = MDMA_REQUEST_JPEG_OUTFIFO_TH;       /* 使用 JPEG FIFO 阈值触发MDMA */
    s_hmdmaOut.Init.TransferTriggerMode = MDMA_BUFFER_TRANSFER;   /* 使用缓冲区传输 */
    s_hmdmaOut.Init.BufferTransferLength = 32;                    /* 缓冲区单次传输数据长度 */
    s_hmdmaOut.Instance = MDMA_Channel6;                          /* 使用通道16 */

    HAL_MDMA_DeInit(&s_hmdmaOut);              /* 恢复默认设置 */
    HAL_MDMA_Init(&s_hmdmaOut);                /* 根据上面的配置进行初始化 */
    __HAL_LINKDMA(hJpeg, hdmaout, s_hmdmaOut); /* 关联MDMA句柄 */

    HAL_NVIC_SetPriority(MDMA_IRQn, 0x08, 0x0F); /* 设置MDMA中断 */
    HAL_NVIC_EnableIRQ(MDMA_IRQn);               /* 使能MDMA中断 */
}

/**
 * @brief  JPEG解码器初始化
 * @note   初始化JPEG硬件解码器
 */
void Jpeg_Init(void)
{
    g_hJpeg.Instance = JPEG;

    HAL_JPEG_Init(&g_hJpeg); /* 初始化JPEG解码器 */
}

/**
 * @brief  开始JPEG硬件解码，使用DMA传输数据
 * @param  sourceAddress JPG图片源地址
 * @param  frameSize JPG图片大小
 * @param  destAddress 解码输出区地址
 * @note   启动JPEG硬件解码过程，配置输入输出缓冲区
 */
void Jpeg_StartDecode(uint32_t sourceAddress, uint32_t frameSize, uint32_t destAddress)
{
    g_jpegSourceAddress = sourceAddress; /* 获取 JPG图片源地址 */
    g_frameBufferAddress = destAddress;  /* 获取 解码输出区地址 */
    g_inputFrameSize = frameSize;        /* 获取 JPG图片大小 */

    g_inputFrameIndex = 0;                     /* 当前地址索引归0 */
    s_JpegHwDecodingState = JPEG_DECODE_START; /* 解码开始标志 */

    /* 开始解码 */
    HAL_JPEG_Decode_DMA(&g_hJpeg, (uint8_t *)g_jpegSourceAddress, CHUNK_SIZE_IN, (uint8_t *)g_frameBufferAddress, CHUNK_SIZE_OUT);
}

/**
 * @brief  等待硬件JPEG解码结束
 * @retval JPEG_DECODE_COMPLETE - 解码结束
 * @note   阻塞等待直到解码完成
 */
uint8_t Jpeg_WaitDecodeEnd(void)
{
    while (s_JpegHwDecodingState == JPEG_DECODE_START); /* 等待解码完成 */

    return JPEG_DECODE_COMPLETE; /* 返回解码完成标志 */
}

/**
 * @brief  数据获取回调函数，当图片数据过多时，需要分包读取
 * @param  hJpeg JPEG句柄
 * @param  nbDecodedData 要操作的数据长度
 * @note   处理输入数据缓冲区的配置
 */
void Jpeg_GetDataCallback(JPEG_HandleTypeDef *hJpeg, uint32_t nbDecodedData)
{
    uint32_t inDataLength;
    g_inputFrameIndex += nbDecodedData; /* 每次读取长度 nbDecodedData */

    if (g_inputFrameIndex < g_inputFrameSize) /* 如果单次读取的长度小于 jpg图片的大小 */
    {
        g_jpegSourceAddress = g_jpegSourceAddress + nbDecodedData; /* 计算地址偏移 */

        /* 如果单次读取的数据长度超过了定义的 CHUNK_SIZE_IN */
        if ((g_inputFrameSize - g_inputFrameIndex) >= CHUNK_SIZE_IN)
        {
            inDataLength = CHUNK_SIZE_IN;
        }
        else
        {
            inDataLength = g_inputFrameSize - g_inputFrameIndex;
        }
    }
    else
    {
        inDataLength = 0;
    }
    HAL_JPEG_ConfigInputBuffer(hJpeg, (uint8_t *)g_jpegSourceAddress, inDataLength); /* 配置输入缓冲 */
}

/**
 * @brief  数据更新回调函数，因为每次最大输出长度为 CHUNK_SIZE_OUT ，当图片数据过多时，需要分多次解码
 * @param  hJpeg JPEG句柄
 * @param  pDataOut 输出缓冲区地址
 * @param  outDataLength 要操作的数据长度
 * @note   处理输出数据缓冲区的配置
 */
void Jpeg_DataReadyCallback(JPEG_HandleTypeDef *hJpeg, uint8_t *pDataOut, uint32_t outDataLength)
{
    g_frameBufferAddress += outDataLength;                                               /* 更新输出地址 */
    HAL_JPEG_ConfigOutputBuffer(hJpeg, (uint8_t *)g_frameBufferAddress, CHUNK_SIZE_OUT); /* 配置输出缓冲 */
}

/**
 * @brief  解码出错回调函数，当解码发生错误时，会进入该函数
 * @param  hJpeg JPEG句柄
 * @note   处理JPEG解码过程中的错误
 */
void Jpeg_ErrorCallback(JPEG_HandleTypeDef *hJpeg)
{
    if (HAL_JPEG_GetError(hJpeg) == HAL_JPEG_ERROR_DMA) /* DMA数据传输错误 */
    {
        printf("DMA传输错误！！错误代码:%d\r\n", HAL_JPEG_GetError(hJpeg));
        printf("当前解码输出地址：0X%x,\r\n", g_frameBufferAddress);
    }
    printf("错误！！错误代码:%d\r\n", HAL_JPEG_GetError(hJpeg));
}

/**
 * @brief  JPEG解码结束回调函数，解码结束后，会进入该中断
 * @param  hJpeg JPEG句柄
 * @note   标记解码完成状态
 */
void Jpeg_DecodeCpltCallback(JPEG_HandleTypeDef *hJpeg)
{
    s_JpegHwDecodingState = JPEG_DECODE_COMPLETE; /* 解码结束 */
}

/**
 * @brief  JPEG中断处理函数
 * @note   处理JPEG硬件中断
 */
void JPEG_IRQHandler(void)
{
    HAL_JPEG_IRQHandler(&g_hJpeg);
}

/**
 * @brief  MDMA中断处理函数
 * @note   处理MDMA传输中断
 */
void MDMA_IRQHandler()
{
    /* Check the interrupt and clear flag */
    HAL_MDMA_IRQHandler(g_hJpeg.hdmain);
    HAL_MDMA_IRQHandler(g_hJpeg.hdmaout);
}
