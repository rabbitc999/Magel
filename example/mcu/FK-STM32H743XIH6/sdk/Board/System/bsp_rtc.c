#include "bsp_rtc.h"
#include <time.h>

RTC_DateTime_T g_RTC_Data = { 2025, 6, 29, 21, 30, 0, SUNDAY }; /* 定义全局的时间数组，数组内容分别为年、月、日、时、分、秒、星期 */
RTC_HandleTypeDef g_hRtc;

void RTC_SetTime(void); /* 函数声明 */

/**
 * @brief  初始化RTC(实时时钟)模块
 * @note   使用LSE作为RTC时钟源，24小时制
 */
void RTC_Init(void)
{
    /* RTC使用LSE时钟 */
    RCC_PeriphCLKInitTypeDef periphClkInitStruct = { 0 };
    periphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_RTC;
    periphClkInitStruct.RTCClockSelection = RCC_RTCCLKSOURCE_LSE;
    HAL_RCCEx_PeriphCLKConfig(&periphClkInitStruct);

    /* 使能RTC的时钟，确保RTC模块能够正常工作 */
    __HAL_RCC_RTC_ENABLE();

    /* 指定RTC实例，使用硬件RTC模块 */
    g_hRtc.Instance = RTC;
    g_hRtc.Init.HourFormat = RTC_HOURFORMAT_24;
    g_hRtc.Init.AsynchPrediv = 127;
    g_hRtc.Init.SynchPrediv = 255;
    g_hRtc.Init.OutPut = RTC_OUTPUT_DISABLE;
    g_hRtc.Init.OutPutPolarity = RTC_OUTPUT_POLARITY_HIGH;
    g_hRtc.Init.OutPutType = RTC_OUTPUT_TYPE_OPENDRAIN;

    /* 调用HAL库函数初始化RTC，传入RTC句柄指针 */
    if (HAL_RTC_Init(&g_hRtc) != HAL_OK)
    {
        Assert_Failed(__FILE__, __LINE__);
    }
}

/**
 * @brief  设置RTC(实时时钟)的时间和日期（以全局变量g_RTC_Data为数据源）
 */
void RTC_SetTime(void)
{
    RTC_TimeTypeDef rtcTimeStructure = { 0 };
    RTC_DateTypeDef rtcDateStructure = { 0 };

    rtcDateStructure.Year = g_RTC_Data.year - 2000;
    rtcDateStructure.Month = g_RTC_Data.month;
    rtcDateStructure.Date = g_RTC_Data.day;

    rtcTimeStructure.Hours = g_RTC_Data.hour;
    rtcTimeStructure.Minutes = g_RTC_Data.minute;
    rtcTimeStructure.Seconds = g_RTC_Data.second;

    rtcDateStructure.WeekDay = g_RTC_Data.weekDay == 0 ? RTC_WEEKDAY_SUNDAY : g_RTC_Data.weekDay;

    HAL_RTC_SetTime(&g_hRtc, &rtcTimeStructure, RTC_FORMAT_BIN);
    HAL_RTC_SetDate(&g_hRtc, &rtcDateStructure, RTC_FORMAT_BIN);
}

/**
 * @brief  从RTC读取当前时间和日期，并存储到全局变量g_RTC_Data中
 */
void RTC_ReadTime(void)
{
    RTC_TimeTypeDef rtcTimeStructure = { 0 };
    RTC_DateTypeDef rtcDateStructure = { 0 };

    HAL_RTC_GetTime(&g_hRtc, &rtcTimeStructure, RTC_FORMAT_BIN);
    HAL_RTC_GetDate(&g_hRtc, &rtcDateStructure, RTC_FORMAT_BIN);

    g_RTC_Data.year = rtcDateStructure.Year + 2000;
    g_RTC_Data.month = rtcDateStructure.Month;
    g_RTC_Data.day = rtcDateStructure.Date;
    g_RTC_Data.hour = rtcTimeStructure.Hours;
    g_RTC_Data.minute = rtcTimeStructure.Minutes;
    g_RTC_Data.second = rtcTimeStructure.Seconds;
    g_RTC_Data.weekDay = rtcDateStructure.WeekDay % 7;
}

/**
 * @brief  根据时间戳设置RTC(实时时钟)的时间和日期
 * @param  timestamp 要设置的时间戳(秒)
 * @note   将UTC时间戳转换为东八区时间后写入RTC，并回读更新g_RTC_Data
 */
void RTC_SetTimestamp(uint32_t timestamp)
{
    /* 将传入的时间戳加上8小时的秒数，转换为东八区时间 */
    timestamp += 8 * 60 * 60;

    /* 使用localtime函数将时间戳转换为本地时间结构体 */
    struct tm time = *localtime((time_t *)&timestamp);

    RTC_TimeTypeDef rtcTimeStructure = { 0 };
    RTC_DateTypeDef rtcDateStructure = { 0 };

    rtcDateStructure.Year = time.tm_year - 100;
    rtcDateStructure.Month = time.tm_mon + 1;
    rtcDateStructure.Date = time.tm_mday;
    rtcTimeStructure.Hours = time.tm_hour;
    rtcTimeStructure.Minutes = time.tm_min;
    rtcTimeStructure.Seconds = time.tm_sec;
    rtcDateStructure.WeekDay = time.tm_wday == 0 ? RTC_WEEKDAY_SUNDAY : time.tm_wday;

    HAL_RTC_SetTime(&g_hRtc, &rtcTimeStructure, RTC_FORMAT_BIN);
    HAL_RTC_SetDate(&g_hRtc, &rtcDateStructure, RTC_FORMAT_BIN);

    RTC_ReadTime();
}

/**
 * @brief  获取当前时间的时间戳(UTC时间)
 * @retval 当前时间的时间戳(从1970-01-01 00:00:00 UTC开始的秒数)
 * @note   读取g_RTC_Data中的东八区时间，转换为UTC时间戳返回
 */
uint32_t RTC_GetTimestamp(void)
{
    /* 将g_RTC_Data转换为struct tm结构体 */
    struct tm timeInfo = { 0 };
    timeInfo.tm_year = g_RTC_Data.year - 1900; /* 年份偏移：实际年份-1900 */
    timeInfo.tm_mon = g_RTC_Data.month - 1;    /* 月份范围：0-11 */
    timeInfo.tm_mday = g_RTC_Data.day;
    timeInfo.tm_hour = g_RTC_Data.hour;
    timeInfo.tm_min = g_RTC_Data.minute;
    timeInfo.tm_sec = g_RTC_Data.second;
    timeInfo.tm_isdst = 0; /* 禁用夏令时 */

    /* 转换为本地时间的时间戳(东八区时间) */
    time_t timestamp = mktime(&timeInfo);

    /* 减去8小时时区偏移，得到UTC时间戳 */
    timestamp -= 8 * 60 * 60;

    return (uint32_t)timestamp;
}

/**
 * @brief  从RTC备份寄存器DR0中读取数据
 * @retval 读到的备份寄存器数据
 */
uint32_t RTC_ReadBKP()
{
    return HAL_RTCEx_BKUPRead(&g_hRtc, RTC_BKP_DR0);
}

/**
 * @brief  向RTC备份寄存器DR0写入数据
 * @param  number 要写入的数据
 */
void RTC_WriteBKP(uint32_t number)
{
    HAL_RTCEx_BKUPWrite(&g_hRtc, RTC_BKP_DR0, number);
}
