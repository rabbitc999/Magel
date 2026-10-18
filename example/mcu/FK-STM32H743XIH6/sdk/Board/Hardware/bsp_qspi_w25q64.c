#include "bsp_qspi_w25q64.h"
#include "bsp_periph.h"

#define QSPI_CLK_PIN      GPIO_PIN_10      /* QSPI_CLK 引脚 */
#define QSPI_CLK_PORT     GPIOF            /* QSPI_CLK 引脚端口 */
#define QSPI_CLK_AF       GPIO_AF9_QUADSPI /* QSPI_CLK IO口复用 */

#define QSPI_BK1_NCS_PIN  GPIO_PIN_6        /* QSPI_BK1_NCS 引脚 */
#define QSPI_BK1_NCS_PORT GPIOG             /* QSPI_BK1_NCS 引脚端口 */
#define QSPI_BK1_NCS_AF   GPIO_AF10_QUADSPI /* QSPI_BK1_NCS IO口复用 */

#define QSPI_BK1_IO0_PIN  GPIO_PIN_8        /* QSPI_BK1_IO0 引脚 */
#define QSPI_BK1_IO0_PORT GPIOF             /* QSPI_BK1_IO0 引脚端口 */
#define QSPI_BK1_IO0_AF   GPIO_AF10_QUADSPI /* QSPI_BK1_IO0 IO口复用 */

#define QSPI_BK1_IO1_PIN  GPIO_PIN_9        /* QSPI_BK1_IO1 引脚 */
#define QSPI_BK1_IO1_PORT GPIOF             /* QSPI_BK1_IO1 引脚端口 */
#define QSPI_BK1_IO1_AF   GPIO_AF10_QUADSPI /* QSPI_BK1_IO1 IO口复用 */

#define QSPI_BK1_IO2_PIN  GPIO_PIN_7       /* QSPI_BK1_IO2 引脚 */
#define QSPI_BK1_IO2_PORT GPIOF            /* QSPI_BK1_IO2 引脚端口 */
#define QSPI_BK1_IO2_AF   GPIO_AF9_QUADSPI /* QSPI_BK1_IO2 IO口复用 */

#define QSPI_BK1_IO3_PIN  GPIO_PIN_6       /* QSPI_BK1_IO3 引脚 */
#define QSPI_BK1_IO3_PORT GPIOF            /* QSPI_BK1_IO3 引脚端口 */
#define QSPI_BK1_IO3_AF   GPIO_AF9_QUADSPI /* QSPI_BK1_IO3 IO口复用 */

static QSPI_HandleTypeDef s_QspiHandle; /* 定义QSPI句柄，这里保留使用cubeMX生成的变量命名，方便用户参考和移植 */

/**
 * @brief  初始化 QSPI 配置
 */
void W25QXX_QSPI_Init(void)
{
    s_QspiHandle.Instance = QUADSPI; /* QSPI外设 */

    GPIO_InitTypeDef gpioInitStruct = { 0 };
    __HAL_RCC_QSPI_CLK_ENABLE(); /* 使能QSPI时钟 */

    BSP_GPIO_ClkEnable(QSPI_CLK_PORT);     /* 使能 QSPI_CLK IO口时钟 */
    BSP_GPIO_ClkEnable(QSPI_BK1_NCS_PORT); /* 使能 QSPI_BK1_NCS IO口时钟 */
    BSP_GPIO_ClkEnable(QSPI_BK1_IO0_PORT); /* 使能 QSPI_BK1_IO0 IO口时钟 */
    BSP_GPIO_ClkEnable(QSPI_BK1_IO1_PORT); /* 使能 QSPI_BK1_IO1 IO口时钟 */
    BSP_GPIO_ClkEnable(QSPI_BK1_IO2_PORT); /* 使能 QSPI_BK1_IO2 IO口时钟 */
    BSP_GPIO_ClkEnable(QSPI_BK1_IO3_PORT); /* 使能 QSPI_BK1_IO3 IO口时钟 */

    /**     ****************************************************
        PB2  -- -- --> QSPI_CLK
        PB6  -- -- --> QSPI_BK1_NCS
        PD11 -- -- --> QSPI_BK1_IO0
        PD12 -- -- --> QSPI_BK1_IO1
        PE2  -- -- --> QSPI_BK1_IO2
        PD13 -- -- --> QSPI_BK1_IO3
     ******************************************************/
    gpioInitStruct.Mode = GPIO_MODE_AF_PP;            /* 复用推挽输出模式 */
    gpioInitStruct.Pull = GPIO_NOPULL;                /* 无上下拉 */
    gpioInitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH; /* 超高速IO口速度 */

    gpioInitStruct.Pin = QSPI_CLK_PIN;             /* QSPI_CLK 引脚 */
    gpioInitStruct.Alternate = QSPI_CLK_AF;        /* QSPI_CLK 复用 */
    HAL_GPIO_Init(QSPI_CLK_PORT, &gpioInitStruct); /* 初始化 QSPI_CLK 引脚 */

    gpioInitStruct.Pin = QSPI_BK1_NCS_PIN;             /* QSPI_BK1_NCS 引脚 */
    gpioInitStruct.Alternate = QSPI_BK1_NCS_AF;        /* QSPI_BK1_NCS 复用 */
    HAL_GPIO_Init(QSPI_BK1_NCS_PORT, &gpioInitStruct); /* 初始化 QSPI_BK1_NCS 引脚 */

    gpioInitStruct.Pin = QSPI_BK1_IO0_PIN;             /* QSPI_BK1_IO0 引脚 */
    gpioInitStruct.Alternate = QSPI_BK1_IO0_AF;        /* QSPI_BK1_IO0 复用 */
    HAL_GPIO_Init(QSPI_BK1_IO0_PORT, &gpioInitStruct); /* 初始化 QSPI_BK1_IO0 引脚 */

    gpioInitStruct.Pin = QSPI_BK1_IO1_PIN;             /* QSPI_BK1_IO1 引脚 */
    gpioInitStruct.Alternate = QSPI_BK1_IO1_AF;        /* QSPI_BK1_IO1 复用 */
    HAL_GPIO_Init(QSPI_BK1_IO1_PORT, &gpioInitStruct); /* 初始化 QSPI_BK1_IO1 引脚 */

    gpioInitStruct.Pin = QSPI_BK1_IO2_PIN;             /* QSPI_BK1_IO2 引脚 */
    gpioInitStruct.Alternate = QSPI_BK1_IO2_AF;        /* QSPI_BK1_IO2 复用 */
    HAL_GPIO_Init(QSPI_BK1_IO2_PORT, &gpioInitStruct); /* 初始化 QSPI_BK1_IO2 引脚 */

    gpioInitStruct.Pin = QSPI_BK1_IO3_PIN;             /* QSPI_BK1_IO3 引脚 */
    gpioInitStruct.Alternate = QSPI_BK1_IO3_AF;        /* QSPI_BK1_IO3 复用 */
    HAL_GPIO_Init(QSPI_BK1_IO3_PORT, &gpioInitStruct); /* 初始化 QSPI_BK1_IO3 引脚 */

    RCC_PeriphCLKInitTypeDef periphClkInitStruct = { 0 };
    periphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_QSPI;
    periphClkInitStruct.QspiClockSelection = RCC_QSPICLKSOURCE_D1HCLK;
    if (HAL_RCCEx_PeriphCLKConfig(&periphClkInitStruct) != HAL_OK)
    {
        Assert_Failed(__FILE__, __LINE__);
    }

    HAL_QSPI_DeInit(&s_QspiHandle); /* 复位QSPI */

    s_QspiHandle.Init.ClockPrescaler = 1;                              /* 时钟分频值，将QSPI内核时钟进行 1+1 分频得到QSPI通信驱动时钟 */
    s_QspiHandle.Init.FifoThreshold = 32;                              /* FIFO阈值 */
    s_QspiHandle.Init.SampleShifting = QSPI_SAMPLE_SHIFTING_HALFCYCLE; /* 半个CLK周期之后进行采样 */
    s_QspiHandle.Init.FlashSize = 22;                                  /* flash大小，FLASH 中的字节数 = 2^[FSIZE+1]，核心板采用是8M字节的W25Q64，这里设置为22 */
    s_QspiHandle.Init.ChipSelectHighTime = QSPI_CS_HIGH_TIME_1_CYCLE;  /* 片选保持高电平的时间 */
    s_QspiHandle.Init.ClockMode = QSPI_CLOCK_MODE_3;                   /* 模式3 */
    s_QspiHandle.Init.FlashID = QSPI_FLASH_ID_1;                       /* 使用QSPI1 */
    s_QspiHandle.Init.DualFlash = QSPI_DUALFLASH_DISABLE;              /* 禁止双闪存模式 */
    if (HAL_QSPI_Init(&s_QspiHandle) != HAL_OK)                        /* 初始化配置 */
    {
        Assert_Failed(__FILE__, __LINE__);
    }
}

/**
 * @brief  初始化W25Q64
 * @retval W25QXX_OK-初始化成功, W25QXX_ERROR_INIT-初始化错误
 */
int8_t W25QXX_Init(void)
{
    W25QXX_QSPI_Init(); /* 初始化 QSPI 配置 */
    W25QXX_Reset();     /* 复位器件 */

    if (W25QXX_ReadId() == W25QXX_FLASH_ID) /* 进行匹配 */
    {
        return W25QXX_OK; /* 返回成功标志 */
    }
    else
    {
        return W25QXX_ERROR_INIT; /* 返回错误标志 */
    }
}

/**
 * @brief  自动轮询等待内存准备就绪
 * @retval W25QXX_OK-通信正常结束, W25QXX_ERROR_AUTOPOLLING-轮询等待无响应
 * @note   每一次通信都应该调用此函数，等待通信结束，避免错误的操作
 */
int8_t W25QXX_AutoPollingMemReady(void)
{
    QSPI_CommandTypeDef sCommand;    /* QSPI传输配置 */
    QSPI_AutoPollingTypeDef sConfig; /* 轮询比较相关配置参数 */

    sCommand.InstructionMode = QSPI_INSTRUCTION_1_LINE;     /* 1线指令模式 */
    sCommand.AddressMode = QSPI_ADDRESS_NONE;               /* 无地址模式 */
    sCommand.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE; /* 无交替字节 */
    sCommand.DdrMode = QSPI_DDR_MODE_DISABLE;               /* 禁止DDR模式 */
    sCommand.DdrHoldHalfCycle = QSPI_DDR_HHC_ANALOG_DELAY;  /* DDR模式中数据延迟，这里用不到 */
    sCommand.SIOOMode = QSPI_SIOO_INST_EVERY_CMD;           /* 每次传输数据都发送指令 */
    sCommand.DataMode = QSPI_DATA_1_LINE;                   /* 1线数据模式 */
    sCommand.DummyCycles = 0;                               /* 空周期个数 */
    sCommand.Instruction = W25QXX_CMD_READ_STATUS_REG1;     /* 读状态信息寄存器 */

    /* 不停的查询 W25QXX_CMD_READ_STATUS_REG1 寄存器，将读取到的状态字节中的
     * W25QXX_STATUS_REG1_BUSY 不停的与0作比较 */
    /* 读状态寄存器1的第0位(只读)，Busy标志位，当正在擦除/写入数据/写命令时会被置1，空闲或通信结束为0
     */

    sConfig.Match = 0;                                  /* 匹配值 */
    sConfig.MatchMode = QSPI_MATCH_MODE_AND;            /* 与运算 */
    sConfig.Interval = 0x10;                            /* 轮询间隔 */
    sConfig.AutomaticStop = QSPI_AUTOMATIC_STOP_ENABLE; /* 自动停止模式 */
    sConfig.StatusBytesSize = 1;                        /* 状态字节数 */
    sConfig.Mask = W25QXX_STATUS_REG1_BUSY;             /* 对在轮询模式下接收的状态字节进行屏蔽，只比较需要用到的位
                                                         */

    /* 发送轮询等待命令 */
    if (HAL_QSPI_AutoPolling(&s_QspiHandle, &sCommand, &sConfig, HAL_QSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
    {
        return W25QXX_ERROR_AUTOPOLLING; /* 轮询等待无响应 */
    }
    return W25QXX_OK; /* 通信正常结束 */
}

/**
 * @brief  复位 W25Q64
 * @retval W25QXX_OK-复位成功, 其他-复位失败
 */
int8_t W25QXX_Reset(void)
{
    QSPI_CommandTypeDef sCommand; /* QSPI传输配置 */

    sCommand.InstructionMode = QSPI_INSTRUCTION_1_LINE;     /* 1线指令模式 */
    sCommand.AddressMode = QSPI_ADDRESS_NONE;               /* 无地址模式 */
    sCommand.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE; /* 无交替字节 */
    sCommand.DdrMode = QSPI_DDR_MODE_DISABLE;               /* 禁止DDR模式 */
    sCommand.DdrHoldHalfCycle = QSPI_DDR_HHC_ANALOG_DELAY;  /* DDR模式中数据延迟，这里用不到 */
    sCommand.SIOOMode = QSPI_SIOO_INST_EVERY_CMD;           /* 每次传输数据都发送指令 */
    sCommand.DataMode = QSPI_DATA_NONE;                     /* 无数据模式 */
    sCommand.DummyCycles = 0;                               /* 空周期个数 */
    sCommand.Instruction = W25QXX_CMD_ENABLE_RESET;         /* 执行复位使能命令 */

    /* 发送复位使能命令 */
    if (HAL_QSPI_Command(&s_QspiHandle, &sCommand, HAL_QSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
    {
        return W25QXX_ERROR_INIT; /* 如果发送失败，返回错误信息 */
    }
    /* 使用自动轮询标志位，等待通信结束 */
    if (W25QXX_AutoPollingMemReady() != W25QXX_OK)
    {
        return W25QXX_ERROR_AUTOPOLLING; /* 轮询等待无响应 */
    }

    sCommand.Instruction = W25QXX_CMD_RESET_DEVICE; /* 复位器件命令 */

    /* 发送复位器件命令 */
    if (HAL_QSPI_Command(&s_QspiHandle, &sCommand, HAL_QSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
    {
        return W25QXX_ERROR_INIT; /* 如果发送失败，返回错误信息 */
    }
    /* 使用自动轮询标志位，等待通信结束 */
    if (W25QXX_AutoPollingMemReady() != W25QXX_OK)
    {
        return W25QXX_ERROR_AUTOPOLLING; /* 轮询等待无响应 */
    }
    return W25QXX_OK; /* 复位成功 */
}

/**
 * @brief  读取 W25Q64 器件ID
 * @retval 读取到的器件ID, 其他-读取失败
 */
uint32_t W25QXX_ReadId(void)
{
    QSPI_CommandTypeDef sCommand; /* QSPI传输配置 */
    uint8_t qspiReceiveBuff[3];   /* 存储QSPI读到的数据 */
    uint32_t w25q64Id;            /* 器件的ID */

    sCommand.InstructionMode = QSPI_INSTRUCTION_1_LINE;     /* 1线指令模式 */
    sCommand.AddressSize = QSPI_ADDRESS_24_BITS;            /* 24位地址 */
    sCommand.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE; /* 无交替字节 */
    sCommand.DdrMode = QSPI_DDR_MODE_DISABLE;               /* 禁止DDR模式 */
    sCommand.DdrHoldHalfCycle = QSPI_DDR_HHC_ANALOG_DELAY;  /* DDR模式中数据延迟，这里用不到 */
    sCommand.SIOOMode = QSPI_SIOO_INST_EVERY_CMD;           /* 每次传输数据都发送指令 */
    sCommand.AddressMode = QSPI_ADDRESS_NONE;               /* 无地址模式 */
    sCommand.DataMode = QSPI_DATA_1_LINE;                   /* 1线数据模式 */
    sCommand.DummyCycles = 0;                               /* 空周期个数 */
    sCommand.NbData = 3;                                    /* 传输数据的长度 */
    sCommand.Instruction = W25QXX_CMD_JEDEC_ID;             /* 执行读器件ID命令 */

    /* 发送指令 */
    if (HAL_QSPI_Command(&s_QspiHandle, &sCommand, HAL_QSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
    {
        return W25QXX_ERROR_INIT; /* 如果发送失败，返回错误信息 */
    }
    /* 接收数据 */
    if (HAL_QSPI_Receive(&s_QspiHandle, qspiReceiveBuff, HAL_QSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
    {
        return W25QXX_ERROR_TRANSMIT; /* 如果接收失败，返回错误信息 */
    }
    /* 将得到的数据组合成ID */
    w25q64Id = (qspiReceiveBuff[0] << 16) | (qspiReceiveBuff[1] << 8) | qspiReceiveBuff[2];

    return w25q64Id; /* 返回ID */
}

/**
 * @brief  将QSPI设置为内存映射模式
 * @retval W25QXX_OK-设置成功, 其他-设置失败
 * @note   设置为内存映射模式时，只能读，不能写！！！
 */
int8_t W25QXX_MemoryMappedMode(void)
{
    QSPI_CommandTypeDef sCommand;           /* QSPI传输配置 */
    QSPI_MemoryMappedTypeDef sMemMappedCfg; /* 内存映射访问参数 */

    sCommand.InstructionMode = QSPI_INSTRUCTION_1_LINE;     /* 1线指令模式 */
    sCommand.AddressSize = QSPI_ADDRESS_24_BITS;            /* 24位地址 */
    sCommand.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE; /* 无交替字节 */
    sCommand.DdrMode = QSPI_DDR_MODE_DISABLE;               /* 禁止DDR模式 */
    sCommand.DdrHoldHalfCycle = QSPI_DDR_HHC_ANALOG_DELAY;  /* DDR模式中数据延迟，这里用不到 */
    sCommand.SIOOMode = QSPI_SIOO_INST_EVERY_CMD;           /* 每次传输数据都发送指令 */
    sCommand.AddressMode = QSPI_ADDRESS_4_LINES;            /* 4线地址模式 */
    sCommand.DataMode = QSPI_DATA_4_LINES;                  /* 4线数据模式 */
    sCommand.DummyCycles = 6;                               /* 空周期个数 */
    sCommand.Instruction = W25QXX_CMD_FAST_READ_QUAD_IO;    /* 1-4-4模式下(1线指令4线地址4线数据)，快速读取指令
                                                             */

    sMemMappedCfg.TimeOutActivation = QSPI_TIMEOUT_COUNTER_DISABLE; /* 禁用超时计数器, nCS 保持激活状态 */
    sMemMappedCfg.TimeOutPeriod = 0;                                /* 超时判断周期 */

    W25QXX_Reset(); /* 复位W25Qxx */

    if (HAL_QSPI_MemoryMapped(&s_QspiHandle, &sCommand, &sMemMappedCfg) != HAL_OK) /* 进行配置 */
    {
        return W25QXX_ERROR_MEMORT_MAPPED; /* 设置内存映射模式错误 */
    }

    return W25QXX_OK; /* 配置成功 */
}

/**
 * @brief  发送写使能命令
 * @retval W25QXX_OK-写使能成功, W25QXX_ERROR_WRITE_ENABLE-写使能失败
 */
int8_t W25QXX_WriteEnable(void)
{
    QSPI_CommandTypeDef sCommand;    /* QSPI传输配置 */
    QSPI_AutoPollingTypeDef sConfig; /* 轮询比较相关配置参数 */

    sCommand.InstructionMode = QSPI_INSTRUCTION_1_LINE;     /* 1线指令模式 */
    sCommand.AddressMode = QSPI_ADDRESS_NONE;               /* 无地址模式 */
    sCommand.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE; /* 无交替字节 */
    sCommand.DdrMode = QSPI_DDR_MODE_DISABLE;               /* 禁止DDR模式 */
    sCommand.DdrHoldHalfCycle = QSPI_DDR_HHC_ANALOG_DELAY;  /* DDR模式中数据延迟，这里用不到 */
    sCommand.SIOOMode = QSPI_SIOO_INST_EVERY_CMD;           /* 每次传输数据都发送指令 */
    sCommand.DataMode = QSPI_DATA_NONE;                     /* 无数据模式 */
    sCommand.DummyCycles = 0;                               /* 空周期个数 */
    sCommand.Instruction = W25QXX_CMD_WRITE_ENABLE;         /* 发送写使能命令 */

    /* 发送写使能命令 */
    if (HAL_QSPI_Command(&s_QspiHandle, &sCommand, HAL_QSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
    {
        return W25QXX_ERROR_WRITE_ENABLE; /* */
    }

    /* 不停的查询 W25QXX_CMD_READ_STATUS_REG1 寄存器，将读取到的状态字节中的
     * W25QXX_STATUS_REG1_WEL 不停的与 0x02 作比较 */
    /* 读状态寄存器1的第1位(只读)，WEL写使能标志位，该标志位为1时，代表可以进行写操作
     */

    sConfig.Match = 0x02;                               /* 匹配值 */
    sConfig.Mask = W25QXX_STATUS_REG1_WEL;              /* 读状态寄存器1的第1位(只读)，WEL写使能标志位，该标志位为1时，代表可以进行写操作 */
    sConfig.MatchMode = QSPI_MATCH_MODE_AND;            /* 与运算 */
    sConfig.StatusBytesSize = 1;                        /* 状态字节数 */
    sConfig.Interval = 0x10;                            /* 轮询间隔 */
    sConfig.AutomaticStop = QSPI_AUTOMATIC_STOP_ENABLE; /* 自动停止模式 */

    sCommand.Instruction = W25QXX_CMD_READ_STATUS_REG1; /* 读状态信息寄存器 */
    sCommand.DataMode = QSPI_DATA_1_LINE;               /* 1线数据模式 */
    sCommand.NbData = 1;                                /* 数据长度 */

    /* 发送轮询等待命令 */
    if (HAL_QSPI_AutoPolling(&s_QspiHandle, &sCommand, &sConfig, HAL_QSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
    {
        return W25QXX_ERROR_AUTOPOLLING; /* 轮询等待无响应 */
    }
    return W25QXX_OK; /* 通信正常结束 */
}

/**
 * @brief  扇区擦除，每次擦除4K字节
 * @param  sectorAddress 要擦除的扇区地址
 * @retval W25QXX_OK-擦除成功, W25QXX_ERROR_ERASE-擦除失败, W25QXX_ERROR_AUTOPOLLING-轮询等待无响应
 * @note   擦除操作是不可逆的，请谨慎使用
 * @note   按照 W25Q64JV 数据手册给出的擦除参考时间，典型值为45ms，最大值为400ms
 * @note   实际的擦除速度可能大于45ms，也可能小于45ms
 * @note   flash使用的时间越长，擦除所需时间也会越长
 */
int8_t W25QXX_SectorErase(uint32_t sectorAddress)
{
    QSPI_CommandTypeDef sCommand; /* QSPI传输配置 */

    sCommand.InstructionMode = QSPI_INSTRUCTION_1_LINE;     /* 1线指令模式 */
    sCommand.AddressSize = QSPI_ADDRESS_24_BITS;            /* 24位地址模式 */
    sCommand.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE; /* 无交替字节 */
    sCommand.DdrMode = QSPI_DDR_MODE_DISABLE;               /* 禁止DDR模式 */
    sCommand.DdrHoldHalfCycle = QSPI_DDR_HHC_ANALOG_DELAY;  /* DDR模式中数据延迟，这里用不到 */
    sCommand.SIOOMode = QSPI_SIOO_INST_EVERY_CMD;           /* 每次传输数据都发送指令 */
    sCommand.AddressMode = QSPI_ADDRESS_1_LINE;             /* 1线地址模式 */
    sCommand.DataMode = QSPI_DATA_NONE;                     /* 无数据 */
    sCommand.DummyCycles = 0;                               /* 空周期个数 */
    sCommand.Address = sectorAddress;                       /* 要擦除的地址 */
    sCommand.Instruction = W25QXX_CMD_SECTOR_ERASE;         /* 扇区擦除命令 */

    /* 发送写使能 */
    if (W25QXX_WriteEnable() != W25QXX_OK)
    {
        return W25QXX_ERROR_WRITE_ENABLE; /* 写使能失败 */
    }
    /* 发出擦除命令 */
    if (HAL_QSPI_Command(&s_QspiHandle, &sCommand, HAL_QSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
    {
        return W25QXX_ERROR_ERASE; /* 擦除失败 */
    }
    /* 使用自动轮询标志位，等待擦除的结束 */
    if (W25QXX_AutoPollingMemReady() != W25QXX_OK)
    {
        return W25QXX_ERROR_AUTOPOLLING; /* 轮询等待无响应 */
    }
    return W25QXX_OK; /* 擦除成功 */
}

/**
 * @brief  块擦除，每次擦除32K字节
 * @param  sectorAddress 要擦除的扇区地址
 * @retval W25QXX_OK-擦除成功, W25QXX_ERROR_ERASE-擦除失败, W25QXX_ERROR_AUTOPOLLING-轮询等待无响应
 * @note   擦除操作是不可逆的，请谨慎使用
 * @note   按照 W25Q64JV 数据手册给出的擦除参考时间，典型值为120ms，最大值为1600ms
 * @note   实际的擦除速度可能大于120ms，也可能小于120ms
 * @note   flash使用的时间越长，擦除所需时间也会越长
 */
int8_t W25QXX_BlockErase32k(uint32_t sectorAddress)
{
    QSPI_CommandTypeDef sCommand; /* QSPI传输配置 */

    sCommand.InstructionMode = QSPI_INSTRUCTION_1_LINE;     /* 1线指令模式 */
    sCommand.AddressSize = QSPI_ADDRESS_24_BITS;            /* 24位地址模式 */
    sCommand.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE; /* 无交替字节 */
    sCommand.DdrMode = QSPI_DDR_MODE_DISABLE;               /* 禁止DDR模式 */
    sCommand.DdrHoldHalfCycle = QSPI_DDR_HHC_ANALOG_DELAY;  /* DDR模式中数据延迟，这里用不到 */
    sCommand.SIOOMode = QSPI_SIOO_INST_EVERY_CMD;           /* 每次传输数据都发送指令 */
    sCommand.AddressMode = QSPI_ADDRESS_1_LINE;             /* 1线地址模式 */
    sCommand.DataMode = QSPI_DATA_NONE;                     /* 无数据 */
    sCommand.DummyCycles = 0;                               /* 空周期个数 */
    sCommand.Address = sectorAddress;                       /* 要擦除的地址 */
    sCommand.Instruction = W25QXX_CMD_BLOCK_ERASE_32K;      /* 块擦除命令，每次擦除32K字节 */

    /* 发送写使能 */
    if (W25QXX_WriteEnable() != W25QXX_OK)
    {
        return W25QXX_ERROR_WRITE_ENABLE; /* 写使能失败 */
    }
    /* 发出擦除命令 */
    if (HAL_QSPI_Command(&s_QspiHandle, &sCommand, HAL_QSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
    {
        return W25QXX_ERROR_ERASE; /* 擦除失败 */
    }
    /* 使用自动轮询标志位，等待擦除的结束 */
    if (W25QXX_AutoPollingMemReady() != W25QXX_OK)
    {
        return W25QXX_ERROR_AUTOPOLLING; /* 轮询等待无响应 */
    }
    return W25QXX_OK; /* 擦除成功 */
}

/**
 * @brief  块擦除，每次擦除64K字节
 * @param  sectorAddress 要擦除的扇区地址
 * @retval W25QXX_OK-擦除成功, W25QXX_ERROR_ERASE-擦除失败, W25QXX_ERROR_AUTOPOLLING-轮询等待无响应
 * @note   擦除操作是不可逆的，请谨慎使用
 * @note   按照 W25Q64JV 数据手册给出的擦除参考时间，典型值为150ms，最大值为2000ms
 * @note   实际的擦除速度可能大于150ms，也可能小于150ms
 * @note   flash使用的时间越长，擦除所需时间也会越长
 */
int8_t W25QXX_BlockErase64k(uint32_t sectorAddress)
{
    QSPI_CommandTypeDef sCommand; /* QSPI传输配置 */

    sCommand.InstructionMode = QSPI_INSTRUCTION_1_LINE;     /* 1线指令模式 */
    sCommand.AddressSize = QSPI_ADDRESS_24_BITS;            /* 24位地址模式 */
    sCommand.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE; /* 无交替字节 */
    sCommand.DdrMode = QSPI_DDR_MODE_DISABLE;               /* 禁止DDR模式 */
    sCommand.DdrHoldHalfCycle = QSPI_DDR_HHC_ANALOG_DELAY;  /* DDR模式中数据延迟，这里用不到 */
    sCommand.SIOOMode = QSPI_SIOO_INST_EVERY_CMD;           /* 每次传输数据都发送指令 */
    sCommand.AddressMode = QSPI_ADDRESS_1_LINE;             /* 1线地址模式 */
    sCommand.DataMode = QSPI_DATA_NONE;                     /* 无数据 */
    sCommand.DummyCycles = 0;                               /* 空周期个数 */
    sCommand.Address = sectorAddress;                       /* 要擦除的地址 */
    sCommand.Instruction = W25QXX_CMD_BLOCK_ERASE_64K;      /* 块擦除命令，每次擦除64K字节 */

    /* 发送写使能 */
    if (W25QXX_WriteEnable() != W25QXX_OK)
    {
        return W25QXX_ERROR_WRITE_ENABLE; /* 写使能失败 */
    }
    /* 发出擦除命令 */
    if (HAL_QSPI_Command(&s_QspiHandle, &sCommand, HAL_QSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
    {
        return W25QXX_ERROR_ERASE; /* 擦除失败 */
    }
    /* 使用自动轮询标志位，等待擦除的结束 */
    if (W25QXX_AutoPollingMemReady() != W25QXX_OK)
    {
        return W25QXX_ERROR_AUTOPOLLING; /* 轮询等待无响应 */
    }
    return W25QXX_OK; /* 擦除成功 */
}

/**
 * @brief  整片擦除
 * @retval W25QXX_OK-擦除成功, W25QXX_ERROR_ERASE-擦除失败, W25QXX_ERROR_AUTOPOLLING-轮询等待无响应
 * @note   擦除操作是不可逆的，请谨慎使用
 * @note   按照 W25Q64JV 数据手册给出的擦除参考时间，典型值为20s，最大值为100s
 * @note   实际的擦除速度可能大于20s，也可能小于20s
 * @note   flash使用的时间越长，擦除所需时间也会越长
 */
int8_t W25QXX_ChipErase(void)
{
    QSPI_CommandTypeDef sCommand;    /* QSPI传输配置 */
    QSPI_AutoPollingTypeDef sConfig; /* 轮询等待配置参数 */

    sCommand.InstructionMode = QSPI_INSTRUCTION_1_LINE;     /* 1线指令模式 */
    sCommand.AddressSize = QSPI_ADDRESS_24_BITS;            /* 24位地址模式 */
    sCommand.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE; /* 无交替字节 */
    sCommand.DdrMode = QSPI_DDR_MODE_DISABLE;               /* 禁止DDR模式 */
    sCommand.DdrHoldHalfCycle = QSPI_DDR_HHC_ANALOG_DELAY;  /* DDR模式中数据延迟，这里用不到 */
    sCommand.SIOOMode = QSPI_SIOO_INST_EVERY_CMD;           /* 每次传输数据都发送指令 */
    sCommand.AddressMode = QSPI_ADDRESS_NONE;               /* 无地址 */
    sCommand.DataMode = QSPI_DATA_NONE;                     /* 无数据 */
    sCommand.DummyCycles = 0;                               /* 空周期个数 */
    sCommand.Instruction = W25QXX_CMD_CHIP_ERASE;           /* 擦除命令，进行整片擦除 */

    /* 发送写使能 */
    if (W25QXX_WriteEnable() != W25QXX_OK)
    {
        return W25QXX_ERROR_WRITE_ENABLE; /* 写使能失败 */
    }
    /* 发出擦除命令 */
    if (HAL_QSPI_Command(&s_QspiHandle, &sCommand, HAL_QSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
    {
        return W25QXX_ERROR_ERASE; /* 擦除失败 */
    }

    /* 不停的查询 W25QXX_CMD_READ_STATUS_REG1 寄存器，将读取到的状态字节中的
     * W25QXX_STATUS_REG1_BUSY 不停的与0作比较 */
    /* 读状态寄存器1的第0位(只读)，Busy标志位，当正在擦除/写入数据/写命令时会被置1，空闲或通信结束为0
     */

    sConfig.Match = 0;                                  /* 匹配值 */
    sConfig.MatchMode = QSPI_MATCH_MODE_AND;            /* 与运算 */
    sConfig.Interval = 0x10;                            /* 轮询间隔 */
    sConfig.AutomaticStop = QSPI_AUTOMATIC_STOP_ENABLE; /* 自动停止模式 */
    sConfig.StatusBytesSize = 1;                        /* 状态字节数 */
    sConfig.Mask = W25QXX_STATUS_REG1_BUSY;             /* 对在轮询模式下接收的状态字节进行屏蔽，只比较需要用到的位
                                                         */

    sCommand.Instruction = W25QXX_CMD_READ_STATUS_REG1; /* 读状态信息寄存器 */
    sCommand.DataMode = QSPI_DATA_1_LINE;               /* 1线数据模式 */
    sCommand.NbData = 1;                                /* 数据长度 */

    /* W25Q64整片擦除的典型参考时间为20s，最大时间为100S，这里的超时等待值
     * W25QXX_CHIP_ERASE_TIMEOUT_MAX 为 100S */
    if (HAL_QSPI_AutoPolling(&s_QspiHandle, &sCommand, &sConfig, W25QXX_CHIP_ERASE_TIMEOUT_MAX) != HAL_OK)
    {
        return W25QXX_ERROR_AUTOPOLLING; /* 轮询等待无响应 */
    }
    return W25QXX_OK;
}

/**
 * @brief  按页写入数据
 * @param  pBuffer 要写入的数据
 * @param  writeAddr 要写入的地址
 * @param  numByteToWrite 要写入的数据长度
 * @retval W25QXX_OK-写数据成功, W25QXX_ERROR_WRITE_ENABLE-写使能失败, W25QXX_ERROR_TRANSMIT-传输失败, W25QXX_ERROR_AUTOPOLLING-轮询等待无响应
 * @note   在数据写入之前，请务必完成擦除操作
 * @note   Flash的写入时间和擦除时间一样，是限定的，并不是说QSPI驱动时钟133M就可以以这个速度进行写入
 * @note   按照 W25Q64JV 数据手册给出的页(256字节)写入参考时间，典型值为0.4ms，最大值为3ms
 * @note   实际的写入速度可能大于0.4ms，也可能小于0.4ms
 * @note   Flash使用的时间越长，写入所需时间也会越长
 */
int8_t W25QXX_WritePage(uint8_t *pBuffer, uint32_t writeAddr,
                        uint16_t numByteToWrite)
{
    QSPI_CommandTypeDef sCommand; /* QSPI传输配置 */

    sCommand.InstructionMode = QSPI_INSTRUCTION_1_LINE;        /* 1线指令模式 */
    sCommand.AddressSize = QSPI_ADDRESS_24_BITS;               /* 24位地址 */
    sCommand.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;    /* 无交替字节 */
    sCommand.DdrMode = QSPI_DDR_MODE_DISABLE;                  /* 禁止DDR模式 */
    sCommand.DdrHoldHalfCycle = QSPI_DDR_HHC_ANALOG_DELAY;     /* DDR模式中数据延迟，这里用不到 */
    sCommand.SIOOMode = QSPI_SIOO_INST_EVERY_CMD;              /* 每次传输数据都发送指令 */
    sCommand.AddressMode = QSPI_ADDRESS_1_LINE;                /* 1线地址模式 */
    sCommand.DataMode = QSPI_DATA_4_LINES;                     /* 4线数据模式 */
    sCommand.DummyCycles = 0;                                  /* 空周期个数 */
    sCommand.NbData = numByteToWrite;                          /* 数据长度，最大只能256字节 */
    sCommand.Address = writeAddr;                              /* 要写入 W25Qxx 的地址 */
    sCommand.Instruction = W25QXX_CMD_QUAD_INPUT_PAGE_PROGRAM; /* 1-1-4模式下(1线指令1线地址4线数据)，页编程指令
                                                                */

    /* 写使能 */
    if (W25QXX_WriteEnable() != W25QXX_OK)
    {
        return W25QXX_ERROR_WRITE_ENABLE; /* 写使能失败 */
    }
    /* 写命令 */
    if (HAL_QSPI_Command(&s_QspiHandle, &sCommand, HAL_QSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
    {
        return W25QXX_ERROR_TRANSMIT; /* 传输数据错误 */
    }
    /* 开始传输数据 */
    if (HAL_QSPI_Transmit(&s_QspiHandle, pBuffer, HAL_QSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
    {
        return W25QXX_ERROR_TRANSMIT; /* 传输数据错误 */
    }
    /* 使用自动轮询标志位，等待写入的结束 */
    if (W25QXX_AutoPollingMemReady() != W25QXX_OK)
    {
        return W25QXX_ERROR_AUTOPOLLING; /* 轮询等待无响应 */
    }

    return W25QXX_OK; /* 写数据成功 */
}

/**
 * @brief  写入数据
 * @param  pBuffer 要写入的数据
 * @param  writeAddr 要写入W25Qxx的地址
 * @param  size 数据长度，最大不能超过flash芯片的大小
 * @retval W25QXX_OK-写数据成功, W25QXX_ERROR_WRITE_ENABLE-写使能失败, W25QXX_ERROR_TRANSMIT-传输失败, W25QXX_ERROR_AUTOPOLLING-轮询等待无响应
 * @note   在数据写入之前，请务必完成擦除操作
 * @note   Flash的写入时间和擦除时间一样，是限定的，并不是说QSPI驱动时钟133M就可以以这个速度进行写入
 * @note   按照 W25Q64JV 数据手册给出的页(256字节)写入参考时间，典型值为0.4ms，最大值为3ms
 * @note   实际的写入速度可能大于0.4ms，也可能小于0.4ms
 * @note   Flash使用的时间越长，写入所需时间也会越长
 */
static int8_t __W25QXX_WriteBuffer(uint8_t *pBuffer, uint32_t writeAddr, uint32_t size)
{
    uint32_t endAddr, currentSize, currentAddr;
    uint8_t *writeData; /* 要写入的数据 */

    currentSize = W25QXX_PAGE_SIZE - (writeAddr % W25QXX_PAGE_SIZE); /* 计算当前页还剩余的空间 */

    if (currentSize > size) /* 判断当前页剩余的空间是否足够写入所有数据 */
    {
        currentSize = size; /* 如果足够，则直接获取当前长度 */
    }

    currentAddr = writeAddr;    /* 获取要写入的地址 */
    endAddr = writeAddr + size; /* 计算结束地址 */
    writeData = pBuffer;        /* 获取要写入的数据存储区地址 */

    do
    {
        /* 按页写入数据 */
        if (W25QXX_WritePage(writeData, currentAddr, currentSize) != W25QXX_OK)
        {
            return W25QXX_ERROR_TRANSMIT;
        }

        else /* 按页写入数据成功，进行下一次写数据的准备工作 */
        {
            currentAddr += currentSize; /* 计算下一次要写入的地址 */
            writeData += currentSize;   /* 获取下一次要写入的数据存储区地址 */
            /* 计算下一次写数据的长度 */
            currentSize = ((currentAddr + W25QXX_PAGE_SIZE) > endAddr) ? (endAddr - currentAddr) : W25QXX_PAGE_SIZE;
        }
    } while (currentAddr < endAddr); /* 判断数据是否全部写入完毕 */

    return W25QXX_OK; /* 写入数据成功 */
}

/**
 * @brief  读取数据
 * @param  pBuffer 要读取的数据
 * @param  readAddr 要读取W25Qxx的地址
 * @param  numByteToRead 数据长度，最大不能超过flash芯片的大小
 * @retval W25QXX_OK-读数据成功, W25QXX_ERROR_TRANSMIT-传输失败, W25QXX_ERROR_AUTOPOLLING-轮询等待无响应
 * @note   Flash的读取速度取决于QSPI的通信时钟，最大不能超过133M
 * @note   这里使用的是1-4-4模式下(1线指令4线地址4线数据)，快速读取指令Fast Read Quad I/O
 * @note   使用快速读取指令是有空周期的，具体参考W25Q64JV的手册Fast Read Quad I/O (0xEB)指令
 * @note   实际使用中，是否使用DMA、编译器的优化等级以及数据存储区的位置(内部TCM SRAM或者AXI SRAM)都会影响读取的速度
 * @note   在本例程中，使用的是库函数进行直接读写，keil版本5.30，编译器AC6.14，编译等级Oz image size，读取速度为7M字节/S，数据放在TCM SRAM或者AXI SRAM都是差不多的结果
 * @note   直接使用HAL库进行读写，速度很慢，使用MDMA进行读取，可以达到50M字节/S以上
 * @note   W25Q64JV所允许的最高驱动频率为133MHz，743的QSPI最高驱动频率也是133MHz，但是对于HAL库函数直接读取而言，驱动时钟超过15M已经不会对性能有提升，对速度要求高的场合可以用MDMA的方式
 */
int8_t W25QXX_ReadBuffer(uint8_t *pBuffer, uint32_t readAddr,
                         uint32_t numByteToRead)
{
    QSPI_CommandTypeDef sCommand; /* QSPI传输配置 */

    sCommand.InstructionMode = QSPI_INSTRUCTION_1_LINE;     /* 1线指令模式 */
    sCommand.AddressSize = QSPI_ADDRESS_24_BITS;            /* 24位地址 */
    sCommand.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE; /* 无交替字节 */
    sCommand.DdrMode = QSPI_DDR_MODE_DISABLE;               /* 禁止DDR模式 */
    sCommand.DdrHoldHalfCycle = QSPI_DDR_HHC_ANALOG_DELAY;  /* DDR模式中数据延迟，这里用不到 */
    sCommand.SIOOMode = QSPI_SIOO_INST_EVERY_CMD;           /* 每次传输数据都发送指令 */
    sCommand.AddressMode = QSPI_ADDRESS_4_LINES;            /* 4线地址模式 */
    sCommand.DataMode = QSPI_DATA_4_LINES;                  /* 4线数据模式 */
    sCommand.DummyCycles = 6;                               /* 空周期个数 */
    sCommand.NbData = numByteToRead;                        /* 数据长度，最大不能超过flash芯片的大小 */
    sCommand.Address = readAddr;                            /* 要读取 W25Qxx 的地址 */
    sCommand.Instruction = W25QXX_CMD_FAST_READ_QUAD_IO;    /* 1-4-4模式下(1线指令4线地址4线数据)，快速读取指令
                                                             */

    /* 发送读取命令 */
    if (HAL_QSPI_Command(&s_QspiHandle, &sCommand, HAL_QSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
    {
        return W25QXX_ERROR_TRANSMIT; /* 传输数据错误 */
    }

    /* 接收数据 */
    if (HAL_QSPI_Receive(&s_QspiHandle, pBuffer, HAL_QSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
    {
        return W25QXX_ERROR_TRANSMIT; /* 传输数据错误 */
    }

    /* 使用自动轮询标志位，等待接收的结束 */
    if (W25QXX_AutoPollingMemReady() != W25QXX_OK)
    {
        return W25QXX_ERROR_AUTOPOLLING; /* 轮询等待无响应 */
    }
    return W25QXX_OK; /* 读取数据成功 */
}

/**
 * @brief  按扇区号和数量进行范围擦除（智能加速版）
 * @param  startSector 起始扇区号(以4KB为1个扇区单位)
 * @param  sectorCount 需要擦除的扇区数量
 * @retval W25QXX_OK-成功, 其他-错误码
 * @note   策略: 优先64K大块擦除->其次32K中块擦除->最后4K小扇区擦除，速度最快，且兼容任意起始位置和任意长度
 */
int8_t W25QXX_EraseBySectorRange(uint32_t startSector, uint32_t sectorCount)
{
    uint32_t currentSector = startSector;
    uint32_t remainingSectors = sectorCount;
    int8_t status = W25QXX_OK;

// 定义常量，方便计算
#define SECTOR_4K_SIZE    0x1000  // 4KB = 4096 bytes
#define BLOCK_32K_SECTORS 8       // 32KB / 4KB = 8 个扇区
#define BLOCK_64K_SECTORS 16      // 64KB / 4KB = 16 个扇区

    while (remainingSectors > 0)
    {
        // 1. 如果剩余不足 8 个扇区，只能用 4KB 擦除
        if (remainingSectors < BLOCK_32K_SECTORS)
        {
            status = W25QXX_SectorErase(currentSector * SECTOR_4K_SIZE);
            if (status != W25QXX_OK) return status;

            currentSector++;
            remainingSectors--;
        }
        // 2. 如果剩余 >= 16 个扇区，且起始位置对齐 64KB 边界 (16扇区)，使用 64KB 擦除
        else if (remainingSectors >= BLOCK_64K_SECTORS && (currentSector % BLOCK_64K_SECTORS) == 0)
        {
            status = W25QXX_BlockErase64k(currentSector * SECTOR_4K_SIZE);
            if (status != W25QXX_OK) return status;

            currentSector += BLOCK_64K_SECTORS;
            remainingSectors -= BLOCK_64K_SECTORS;
        }
        // 3. 如果剩余 >= 8 个扇区，且起始位置对齐 32KB 边界 (8扇区)，使用 32KB 擦除
        else if (remainingSectors >= BLOCK_32K_SECTORS && (currentSector % BLOCK_32K_SECTORS) == 0)
        {
            status = W25QXX_BlockErase32k(currentSector * SECTOR_4K_SIZE);
            if (status != W25QXX_OK) return status;

            currentSector += BLOCK_32K_SECTORS;
            remainingSectors -= BLOCK_32K_SECTORS;
        }
        // 4. 如果未对齐边界，先用 4KB 擦除直到对齐
        else
        {
            status = W25QXX_SectorErase(currentSector * SECTOR_4K_SIZE);
            if (status != W25QXX_OK) return status;

            currentSector++;
            remainingSectors--;
        }
    }

    return W25QXX_OK;
}

/**
 * @brief  智能自动写入数据（包含擦除逻辑）
 * @param  pData 要写入的数据指针
 * @param  writeAddr 写入的起始物理地址
 * @param  size 要写入的字节数
 * @retval W25QXX_OK-成功, 其他-错误码
 * @note   内部逻辑流程：1.参数检查(空指针、地址越界) 2.计算写入范围覆盖的"逻辑扇区"(4KB为单位) 3.调用W25QXX_EraseBySectorRange进行智能擦除(64K/32K/4K) 4.调用W25QXX_WriteBuffer写入新数据
 */
int8_t W25QXX_WriteBuffer(uint8_t *pData, uint32_t writeAddr, uint32_t size)
{
    // 1. 参数合法性检查
    if (pData == NULL)
    {
        return W25QXX_ERROR_TRANSMIT;
    }

    // 检查地址是否超出 W25Q64 的容量 (8MBytes)
    if (writeAddr > W25QXX_FLASH_SIZE || size > W25QXX_FLASH_SIZE - writeAddr)
    {
        return W25QXX_ERROR_TRANSMIT;
    }

    if (size == 0) { return W25QXX_OK; }

// 2. 计算需要擦除的逻辑扇区范围
// 定义常量
#define SECTOR_4K_SIZE 0x1000       // 4KB 扇区大小
#define INVALID_ADDR   0xFFFFFFFFU  // 无效地址标记

    uint32_t startSector = INVALID_ADDR;
    uint32_t endSector = INVALID_ADDR;
    uint32_t sectorCount = 0;

    // 计算起始扇区号 (地址除以 4096)
    startSector = writeAddr / SECTOR_4K_SIZE;

    // 计算结束扇区号 (考虑整除情况)
    // 如果 (writeAddr + size) 刚好是 4096 的整数倍，则结束扇区为 (writeAddr+size)/4096 - 1
    // 否则为 (writeAddr+size)/4096
    if (((writeAddr + size) % SECTOR_4K_SIZE) == 0)
    {
        endSector = (writeAddr + size) / SECTOR_4K_SIZE - 1;
    }
    else
    {
        endSector = (writeAddr + size) / SECTOR_4K_SIZE;
    }

    // 计算总共涉及多少个扇区
    sectorCount = endSector - startSector + 1;

    // 3. 执行智能擦除
    // 调用之前写好的智能擦除函数，它会自动选择 64K/32K/4K 擦除指令
    int8_t eraseStatus = W25QXX_EraseBySectorRange(startSector, sectorCount);
    if (eraseStatus != W25QXX_OK)
    {
        return eraseStatus;  // 如果擦除失败，直接返回错误码
    }

    // 4. 执行写入操作
    // 调用底层的 WriteBuffer 函数，该函数内部会处理分页写入 (Page Program)
    int8_t writeStatus = __W25QXX_WriteBuffer(pData, writeAddr, size);
    if (writeStatus != W25QXX_OK)
    {
        return writeStatus;  // 如果写入失败，返回错误码
    }

    // 5. 全部成功
    return W25QXX_OK;
}
