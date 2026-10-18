#ifndef __PCA9685_H
#define __PCA9685_H

#include "bsp_system.h"

#ifdef __cplusplus
extern "C" {
#endif

void PCA9685_Init(void);
void PCA9685_Write(uint8_t addr, uint8_t data);
uint8_t PCA9685_Read(uint8_t addr);
void PCA9685_SetPWM(uint8_t num, uint32_t on, uint32_t off);
void PCA9685_SetFreq(float freq);

#ifdef __cplusplus
}
#endif

#endif