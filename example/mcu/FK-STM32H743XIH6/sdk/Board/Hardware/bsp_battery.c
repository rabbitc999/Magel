#include "bsp_battery.h"
#include "bsp_periph.h"

/* 电池检测引脚配置宏 */
#define BATTERY_ADC_INSTANCE ADC1
#define BATTERY_ADC_CHANNEL  ADC_CHANNEL_4
#define BATTERY_ADC_PORT     GPIOC
#define BATTERY_ADC_PIN      GPIO_PIN_4

static ADC_HandleTypeDef s_AdcBatteryHandle;

// 3S锂电池电压-SOC特征表（可根据电芯手册/实测校准）
typedef struct __BatterySOC_Table
{
    float voltage;  // 电池电压（V）
    uint8_t soc;    // 剩余电量（%）
} BatterySOC_Table;

BatterySOC_Table SOC_Table[] = {
    { 12.30f, 100 },  // 满电
    { 12.00f, 90 },
    { 11.85f, 80 },
    { 11.70f, 70 },
    { 11.50f, 60 },
    { 11.40f, 50 },
    { 11.25f, 30 },
    { 11.10f, 20 },  // 标称11.1V对应20% SOC
    { 10.80f, 10 },
    { 10.50f, 0 }  // 欠压截止
};

#define SOC_TABLE_LEN (sizeof(SOC_Table) / sizeof(BatterySOC_Table))

/**
 * @brief  初始化电池电压检测所需的ADC模块
 */
void Battery_Init(void)
{
    GPIO_InitTypeDef gpioInitStruct = { 0 };
    RCC_PeriphCLKInitTypeDef periphClkInitStruct = { 0 };
    ADC_MultiModeTypeDef multimode = { 0 };
    ADC_ChannelConfTypeDef sConfig = { 0 };

    /* 使能时钟 */
    __HAL_RCC_ADC12_CLK_ENABLE();

    /* ADC1 GPIO配置
       PC4     ------> ADC1_INP4
    */
    BSP_GPIO_Init(BATTERY_ADC_PORT, BATTERY_ADC_PIN, ANALOG);

    /* 设备时钟配置 */
    periphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_ADC;
    periphClkInitStruct.PLL2.PLL2M = 5;
    periphClkInitStruct.PLL2.PLL2N = 192;
    periphClkInitStruct.PLL2.PLL2P = 12;
    periphClkInitStruct.PLL2.PLL2Q = 2;
    periphClkInitStruct.PLL2.PLL2R = 2;
    periphClkInitStruct.PLL2.PLL2RGE = RCC_PLL2VCIRANGE_2;
    periphClkInitStruct.PLL2.PLL2VCOSEL = RCC_PLL2VCOWIDE;
    periphClkInitStruct.PLL2.PLL2FRACN = 0;
    periphClkInitStruct.AdcClockSelection = RCC_ADCCLKSOURCE_PLL2;
    HAL_RCCEx_PeriphCLKConfig(&periphClkInitStruct);

    /* ADC配置 */
    s_AdcBatteryHandle.Instance = BATTERY_ADC_INSTANCE;
    s_AdcBatteryHandle.Init.ClockPrescaler = ADC_CLOCK_ASYNC_DIV1;
    s_AdcBatteryHandle.Init.Resolution = ADC_RESOLUTION_16B;
    s_AdcBatteryHandle.Init.ScanConvMode = ADC_SCAN_DISABLE;
    s_AdcBatteryHandle.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
    s_AdcBatteryHandle.Init.LowPowerAutoWait = DISABLE;
    s_AdcBatteryHandle.Init.ContinuousConvMode = DISABLE;
    s_AdcBatteryHandle.Init.NbrOfConversion = 1;
    s_AdcBatteryHandle.Init.DiscontinuousConvMode = DISABLE;
    s_AdcBatteryHandle.Init.ExternalTrigConv = ADC_SOFTWARE_START;
    s_AdcBatteryHandle.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
    s_AdcBatteryHandle.Init.ConversionDataManagement = ADC_CONVERSIONDATA_DR;
    s_AdcBatteryHandle.Init.Overrun = ADC_OVR_DATA_PRESERVED;
    s_AdcBatteryHandle.Init.LeftBitShift = ADC_LEFTBITSHIFT_NONE;
    s_AdcBatteryHandle.Init.OversamplingMode = DISABLE;
    if (HAL_ADC_Init(&s_AdcBatteryHandle) != HAL_OK)
    {
        Assert_Failed(__FILE__, __LINE__);
    }

    /* 设置ADC附加描述 */
    multimode.Mode = ADC_MODE_INDEPENDENT;
    if (HAL_ADCEx_MultiModeConfigChannel(&s_AdcBatteryHandle, &multimode) != HAL_OK)
    {
        Assert_Failed(__FILE__, __LINE__);
    }

    /* 配置规则组 */
    sConfig.Channel = BATTERY_ADC_CHANNEL;
    sConfig.Rank = ADC_REGULAR_RANK_1;
    sConfig.SamplingTime = ADC_SAMPLETIME_1CYCLE_5;
    sConfig.SingleDiff = ADC_SINGLE_ENDED;
    sConfig.OffsetNumber = ADC_OFFSET_NONE;
    sConfig.Offset = 0;
    sConfig.OffsetSignedSaturation = DISABLE;
    if (HAL_ADC_ConfigChannel(&s_AdcBatteryHandle, &sConfig) != HAL_OK)
    {
        Assert_Failed(__FILE__, __LINE__);
    }
}

/**
 * @brief  获取电池电压值（启动一次ADC转换并读取）
 * @retval ADC采样换算后的电压值（V）
 */
float Battery_GetVoltage(void)
{
    /* 开始ADC转换 */
    HAL_ADC_Start(&s_AdcBatteryHandle);
    HAL_ADC_PollForConversion(&s_AdcBatteryHandle, HAL_MAX_DELAY);
    uint16_t value = HAL_ADC_GetValue(&s_AdcBatteryHandle);

    return value / 65536.0f * 3.3f; /* 将ADC数值转换为ADC实际的电压值 */
}

/**
 * @brief  根据电压计算SOC（电池电量百分比），采用电压-SOC特征表线性插值
 * @param  voltage 电池电压
 * @retval 电池电量百分比（0-100）
 */
uint8_t Battery_GetPercent(float voltage)
{
    // 边界处理：超过满电/低于欠压
    if (voltage >= SOC_Table[0].voltage) return 100;
    if (voltage <= SOC_Table[SOC_TABLE_LEN - 1].voltage) return 0;

    // 查找电压所在区间，线性插值计算SOC
    for (uint8_t i = 0; i < SOC_TABLE_LEN - 1; i++)
    {
        if (voltage >= SOC_Table[i + 1].voltage && voltage <= SOC_Table[i].voltage)
        {
            // 线性插值公式：SOC = SOC1 + (V-V1)*(SOC0-SOC1)/(V0-V1)
            float delta_v = SOC_Table[i].voltage - SOC_Table[i + 1].voltage;
            float delta_soc = SOC_Table[i].soc - SOC_Table[i + 1].soc;
            float soc = SOC_Table[i + 1].soc + (voltage - SOC_Table[i + 1].voltage) * delta_soc / delta_v;
            return (uint8_t)soc;
        }
    }
    return 0;
}
