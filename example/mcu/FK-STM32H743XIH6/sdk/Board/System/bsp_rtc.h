#ifndef __BSP_RTC_H
#define __BSP_RTC_H

#include "bsp_system.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum __RtcWeekDays_T
{
    SUNDAY = 0,
    MONDAY,
    TUESDAY,
    WEDNESDAY,
    THURSDAY,
    FRIDAY,
    SATURDAY
} RtcWeekDays_T;

typedef struct __RtcDateTime_T
{
    int16_t year;
    int8_t month;
    int8_t day;
    int8_t hour;
    int8_t minute;
    int8_t second;
    RtcWeekDays_T weekDay;
} RTC_DateTime_T;

extern RTC_DateTime_T g_RTC_Data;

void RTC_Init(void);
void RTC_SetTime(void);
void RTC_ReadTime(void);
void RTC_SetTimestamp(uint32_t timestamp);
uint32_t RTC_GetTimestamp(void);

uint32_t RTC_ReadBKP();
void RTC_WriteBKP(uint32_t number);

#ifdef __cplusplus
}
#endif

#endif
