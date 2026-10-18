#include "bsp_sdram.h"

static SDRAM_HandleTypeDef s_SdramHandle;  /* SDRAM_HandleTypeDef 结构体变量 */
static FMC_SDRAM_CommandTypeDef s_Command; /* 控制指令 */

/**
 * @brief  初始化sdram引脚
 */
static void HAL_FMC_MspInit(void)
{
    GPIO_InitTypeDef gpioInitStruct = { 0 };
    /* Peripheral clock enable */
    __HAL_RCC_FMC_CLK_ENABLE();

    /* GPIO Ports Clock Enable */
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();
    __HAL_RCC_GPIOE_CLK_ENABLE();
    __HAL_RCC_GPIOF_CLK_ENABLE();
    __HAL_RCC_GPIOG_CLK_ENABLE();
    __HAL_RCC_GPIOH_CLK_ENABLE();

    /** FMC GPIO Configuration
        PF0   ------> FMC_A0        PD14   ------> FMC_D0         PC0    ------> FMC_SDNWE
        PF1   ------> FMC_A1        PD15   ------> FMC_D1         PG15   ------> FMC_SDNCAS
        PF2   ------> FMC_A2        PD0    ------> FMC_D2         PF11   ------> FMC_SDNRAS
        PF3   ------> FMC_A3        PD1    ------> FMC_D3         PH3    ------> FMC_SDNE0
        PF4   ------> FMC_A4        PE7    ------> FMC_D4         PG4    ------> FMC_BA0
        PF5   ------> FMC_A5        PE8    ------> FMC_D5         PG5    ------> FMC_BA1
        PF12  ------> FMC_A6        PE9    ------> FMC_D6         PH2    ------> FMC_SDCKE0
        PF13  ------> FMC_A7        PE10   ------> FMC_D7         PG8    ------> FMC_SDCLK
        PF14  ------> FMC_A8        PE11   ------> FMC_D8         PE1    ------> FMC_NBL1
        PF15  ------> FMC_A9        PE12   ------> FMC_D9         PE0    ------> FMC_NBL0
        PG0   ------> FMC_A10       PE13   ------> FMC_D10
        PG1   ------> FMC_A11       PE14   ------> FMC_D11
        PG2   ------> FMC_A12       PE15   ------> FMC_D12
                                    PD8    ------> FMC_D13
                                    PD9    ------> FMC_D14
                                    PD10   ------> FMC_D15
     */
    gpioInitStruct.Pin = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3 | GPIO_PIN_4 | GPIO_PIN_5 | GPIO_PIN_11 | GPIO_PIN_12 | GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_15;
    gpioInitStruct.Mode = GPIO_MODE_AF_PP;
    gpioInitStruct.Pull = GPIO_NOPULL;
    gpioInitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    gpioInitStruct.Alternate = GPIO_AF12_FMC;
    HAL_GPIO_Init(GPIOF, &gpioInitStruct);

    gpioInitStruct.Pin = GPIO_PIN_2 | GPIO_PIN_3;
    gpioInitStruct.Mode = GPIO_MODE_AF_PP;
    gpioInitStruct.Pull = GPIO_NOPULL;
    gpioInitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    gpioInitStruct.Alternate = GPIO_AF12_FMC;
    HAL_GPIO_Init(GPIOH, &gpioInitStruct);

    gpioInitStruct.Pin = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_4 | GPIO_PIN_5 | GPIO_PIN_8 | GPIO_PIN_15;
    gpioInitStruct.Mode = GPIO_MODE_AF_PP;
    gpioInitStruct.Pull = GPIO_NOPULL;
    gpioInitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    gpioInitStruct.Alternate = GPIO_AF12_FMC;
    HAL_GPIO_Init(GPIOG, &gpioInitStruct);

    gpioInitStruct.Pin = GPIO_PIN_7 | GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_10 | GPIO_PIN_11 | GPIO_PIN_12 | GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_15 | GPIO_PIN_0 | GPIO_PIN_1;
    gpioInitStruct.Mode = GPIO_MODE_AF_PP;
    gpioInitStruct.Pull = GPIO_NOPULL;
    gpioInitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    gpioInitStruct.Alternate = GPIO_AF12_FMC;
    HAL_GPIO_Init(GPIOE, &gpioInitStruct);

    gpioInitStruct.Pin = GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_10 | GPIO_PIN_14 | GPIO_PIN_15 | GPIO_PIN_0 | GPIO_PIN_1;
    gpioInitStruct.Mode = GPIO_MODE_AF_PP;
    gpioInitStruct.Pull = GPIO_NOPULL;
    gpioInitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    gpioInitStruct.Alternate = GPIO_AF12_FMC;
    HAL_GPIO_Init(GPIOD, &gpioInitStruct);

    gpioInitStruct.Pin = GPIO_PIN_0;
    gpioInitStruct.Mode = GPIO_MODE_AF_PP;
    gpioInitStruct.Pull = GPIO_NOPULL;
    gpioInitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    gpioInitStruct.Alternate = GPIO_AF12_FMC;
    HAL_GPIO_Init(GPIOC, &gpioInitStruct);
}

/**
 * @brief  SDRAM MSP初始化
 * @param  hsdram SDRAM_HandleTypeDef定义的变量，即表示定义的sdram
 */
void HAL_SDRAM_MspInit(SDRAM_HandleTypeDef *hsdram)
{
    HAL_FMC_MspInit();
}

/**
 * @brief  SDRAM 初始化序列
 * @param  hsdram SDRAM_HandleTypeDef定义的变量，即表示定义的sdram
 * @param  command 控制指令
 */
void SDRAM_InitializationSequence(SDRAM_HandleTypeDef *hsdram, FMC_SDRAM_CommandTypeDef *command)
{
    __IO uint32_t tmpmrd = 0;
    volatile uint32_t timeout = 0xFFFF;

    /* Configure a clock configuration enable command */
    command->CommandMode = FMC_SDRAM_CMD_CLK_ENABLE;  /* 开启SDRAM时钟 */
    command->CommandTarget = FMC_COMMAND_TARGET_BANK; /* 选择要控制的区域 */
    command->AutoRefreshNumber = 1;
    command->ModeRegisterDefinition = 0;

    HAL_SDRAM_SendCommand(hsdram, command, SDRAM_TIMEOUT); /* 发送控制指令 */
    while (timeout--);                                     /* 延时等待 */

    /* Configure a PALL (precharge all) command */
    command->CommandMode = FMC_SDRAM_CMD_PALL;        /* 预充电命令 */
    command->CommandTarget = FMC_COMMAND_TARGET_BANK; /* 选择要控制的区域 */
    command->AutoRefreshNumber = 1;
    command->ModeRegisterDefinition = 0;

    HAL_SDRAM_SendCommand(hsdram, command, SDRAM_TIMEOUT); /* 发送控制指令 */

    /* Configure a Auto-Refresh command */
    command->CommandMode = FMC_SDRAM_CMD_AUTOREFRESH_MODE; /* 使用自动刷新 */
    command->CommandTarget = FMC_COMMAND_TARGET_BANK;      /* 选择要控制的区域 */
    command->AutoRefreshNumber = 8;                        /* 自动刷新次数 */
    command->ModeRegisterDefinition = 0;

    HAL_SDRAM_SendCommand(hsdram, command, SDRAM_TIMEOUT); /* 发送控制指令 */

    /* Program the external memory mode register */
    tmpmrd = (uint32_t)SDRAM_MODEREG_BURST_LENGTH_2 | SDRAM_MODEREG_BURST_TYPE_SEQUENTIAL | SDRAM_MODEREG_CAS_LATENCY_3 | SDRAM_MODEREG_OPERATING_MODE_STANDARD | SDRAM_MODEREG_WRITEBURST_MODE_SINGLE;

    command->CommandMode = FMC_SDRAM_CMD_LOAD_MODE;   /* 加载模式寄存器命令 */
    command->CommandTarget = FMC_COMMAND_TARGET_BANK; /* 选择要控制的区域 */
    command->AutoRefreshNumber = 1;
    command->ModeRegisterDefinition = tmpmrd;

    HAL_SDRAM_SendCommand(hsdram, command, SDRAM_TIMEOUT); /* 发送控制指令 */

    HAL_SDRAM_ProgramRefreshRate(hsdram, 918); /* 配置刷新率 */
}

/**
 * @brief  SDRAM 初始化
 */
void SDRAM_Init(void)
{
    FMC_SDRAM_TimingTypeDef sdramTiming = { 0 };

    s_SdramHandle.Instance = FMC_SDRAM_DEVICE;                               /* s_SdramHandle.Init */
    s_SdramHandle.Init.SDBank = FMC_SDRAM_BANK1;                             /* 选择BANK区 */
    s_SdramHandle.Init.ColumnBitsNumber = FMC_SDRAM_COLUMN_BITS_NUM_9;       /* 行地址宽度 */
    s_SdramHandle.Init.RowBitsNumber = FMC_SDRAM_ROW_BITS_NUM_13;            /* 列地址线宽度 */
    s_SdramHandle.Init.MemoryDataWidth = FMC_SDRAM_MEM_BUS_WIDTH_16;         /* 数据宽度 */
    s_SdramHandle.Init.InternalBankNumber = FMC_SDRAM_INTERN_BANKS_NUM_4;    /* bank数量 */
    s_SdramHandle.Init.CASLatency = FMC_SDRAM_CAS_LATENCY_3;                 /* CAS */
    s_SdramHandle.Init.WriteProtection = FMC_SDRAM_WRITE_PROTECTION_DISABLE; /* 禁止写保护 */
    s_SdramHandle.Init.SDClockPeriod = FMC_SDRAM_CLOCK_PERIOD_2;             /* 分频 */
    s_SdramHandle.Init.ReadBurst = FMC_SDRAM_RBURST_ENABLE;                  /* 突发模式 */
    s_SdramHandle.Init.ReadPipeDelay = FMC_SDRAM_RPIPE_DELAY_1;              /* 读延迟 */

    /* sdramTiming */
    sdramTiming.LoadToActiveDelay = 2;
    sdramTiming.ExitSelfRefreshDelay = 7;
    sdramTiming.SelfRefreshTime = 4;
    sdramTiming.RowCycleDelay = 7;
    sdramTiming.WriteRecoveryTime = 2;
    sdramTiming.RPDelay = 2;
    sdramTiming.RCDDelay = 2;

    /* 外设时钟配置 */
    RCC_PeriphCLKInitTypeDef periphClkInitStruct = { 0 };
    periphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_FMC;
    periphClkInitStruct.Usart16ClockSelection = RCC_USART16CLKSOURCE_D2PCLK2;
    periphClkInitStruct.FmcClockSelection = RCC_FMCCLKSOURCE_D1HCLK;
    HAL_RCCEx_PeriphCLKConfig(&periphClkInitStruct);

    /* 初始化FMC接口 */
    if (HAL_SDRAM_Init(&s_SdramHandle, &sdramTiming) != HAL_OK)
    {
        Assert_Failed(__FILE__, __LINE__);
    }

    /* 配置SDRAM */
    SDRAM_InitializationSequence(&s_SdramHandle, &s_Command);
}
