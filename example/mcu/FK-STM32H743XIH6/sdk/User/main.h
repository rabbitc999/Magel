#ifndef __MAIN_H
#define __MAIN_H

#include "stm32h7xx_hal.h"

#include "bsp_system.h"
#include "file.h"
#include "cmd.h"
#include "imu.h"
#include "car.h"
#include "store.h"
#include "run_tasks.h"

void Program_PrintInfo(void);
void IMU_ProgramInit(void);
void Bluetooth_ReadInfo(void);
void ESP8266_ReadInfo(void);

void USB_DetectMode(void);

#define INIT_TITLE(TITLE) LCD_ShowString(0, 0, TITLE " Init", LCD_CYAN, LCD_BLACK)
#define INIT_INFO(INFO)   LCD_ShowString(0, (g_LCD.TftFlag ? 20 : 24) * (line++), INFO " Init...", LCD_YELLOW, LCD_BLACK)

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
