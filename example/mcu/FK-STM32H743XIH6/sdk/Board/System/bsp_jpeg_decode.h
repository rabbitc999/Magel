#ifndef __BSP_JPEG_DECODE_H
#define __BSP_JPEG_DECODE_H

/* 头文件引用 */
#include "stm32h7xx_hal.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 宏定义 */
#define CHUNK_SIZE_IN  ((uint32_t)(64 * 1024)) /* 单次解码输入数据最大长度 */
#define CHUNK_SIZE_OUT ((uint32_t)(64 * 1024)) /* 单次解码输出数据最大长度 */

/* 枚举类型 */
typedef enum tagJpegDecodeStatus_T
{
    JPEG_DECODE_START = 0,   /* 解码开始 */
    JPEG_DECODE_COMPLETE = 1 /* 解码完成 */
} JpegDecodeStatus_T;

/* 函数声明 */
void Jpeg_MspInit(JPEG_HandleTypeDef *hJpeg);
void Jpeg_Init(void);
void Jpeg_StartDecode(uint32_t sourceAddress, uint32_t frameSize, uint32_t destAddress);
uint8_t Jpeg_WaitDecodeEnd(void);
void Jpeg_GetDataCallback(JPEG_HandleTypeDef *hJpeg, uint32_t nbDecodedData);
void Jpeg_DataReadyCallback(JPEG_HandleTypeDef *hJpeg, uint8_t *pDataOut, uint32_t outDataLength);
void Jpeg_ErrorCallback(JPEG_HandleTypeDef *hJpeg);
void Jpeg_DecodeCpltCallback(JPEG_HandleTypeDef *hJpeg);
void JPEG_IRQHandler(void);
void MDMA_IRQHandler(void);

/* 外部变量声明 */
extern JPEG_HandleTypeDef g_hJpeg;    /* JPEG句柄 */
extern uint32_t g_frameBufferAddress; /* 解码输出缓冲区地址 */
extern uint32_t g_jpegSourceAddress;  /* JPG图片源地址 */
extern uint32_t g_inputFrameSize;     /* 当前输入缓冲区地址 */
extern uint32_t g_inputFrameIndex;    /* 地址索引 */

#ifdef __cplusplus
}
#endif

#endif /* __BSP_JPEG_DECODE_H */
