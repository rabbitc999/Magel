#ifndef __BSP_ENCODER_H
#define __BSP_ENCODER_H

#include "bsp_system.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum __EncoderSelect_T
{
    ENCODER_A,
    ENCODER_B,
    ENCODER_C,
    ENCODER_D,
    ENCODER_COUNT
} EncoderSelect_T;

void Encoder_Init(void);
void Encoder_Clear(void);
int16_t Encoder_GetCounter(EncoderSelect_T index);

#ifdef __cplusplus
}
#endif

#endif