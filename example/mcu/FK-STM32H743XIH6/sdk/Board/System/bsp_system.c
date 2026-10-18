#include "bsp_system.h"

/**
 * @brief  系统时钟配置函数（配置电源、HSE/PLL振荡器、总线时钟及外设时钟）
 */
void System_ClockConfig(void)
{
    RCC_OscInitTypeDef rccOscInitStruct = { 0 };
    RCC_ClkInitTypeDef rccClkInitStruct = { 0 };

    /* Supply configuration update enable */
    HAL_PWREx_ConfigSupply(PWR_LDO_SUPPLY);

    /* Configure the main internal regulator output voltage */
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

    while (!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY));

    __HAL_RCC_SYSCFG_CLK_ENABLE();
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE0);

    while (!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY));

    /* Configure LSE Drive Capability */
    HAL_PWR_EnableBkUpAccess();
    __HAL_RCC_LSEDRIVE_CONFIG(RCC_LSEDRIVE_LOW);

    /**
     * Initializes the RCC Oscillators according to the specified parameters
     * in the RCC_OscInitTypeDef structure.
     */
    rccOscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI48 | RCC_OSCILLATORTYPE_HSE
                                      | RCC_OSCILLATORTYPE_LSE;
    rccOscInitStruct.HSEState = RCC_HSE_ON;
    rccOscInitStruct.LSEState = RCC_LSE_ON;
    rccOscInitStruct.HSI48State = RCC_HSI48_ON;
    rccOscInitStruct.PLL.PLLState = RCC_PLL_ON;
    rccOscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    rccOscInitStruct.PLL.PLLM = 5;
    rccOscInitStruct.PLL.PLLN = 192;
    rccOscInitStruct.PLL.PLLP = 2;
    rccOscInitStruct.PLL.PLLQ = 4;
    rccOscInitStruct.PLL.PLLR = 2;
    rccOscInitStruct.PLL.PLLRGE = RCC_PLL1VCIRANGE_2;
    rccOscInitStruct.PLL.PLLVCOSEL = RCC_PLL1VCOWIDE;
    rccOscInitStruct.PLL.PLLFRACN = 0;
    if (HAL_RCC_OscConfig(&rccOscInitStruct) != HAL_OK)
    {
        Assert_Failed(__FILE__, __LINE__);
    }

    /* Initializes the CPU, AHB and APB buses clocks */
    rccClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
                                 | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2
                                 | RCC_CLOCKTYPE_D3PCLK1 | RCC_CLOCKTYPE_D1PCLK1;
    rccClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    rccClkInitStruct.SYSCLKDivider = RCC_SYSCLK_DIV1;
    rccClkInitStruct.AHBCLKDivider = RCC_HCLK_DIV2;
    rccClkInitStruct.APB3CLKDivider = RCC_APB3_DIV2;
    rccClkInitStruct.APB1CLKDivider = RCC_APB1_DIV2;
    rccClkInitStruct.APB2CLKDivider = RCC_APB2_DIV2;
    rccClkInitStruct.APB4CLKDivider = RCC_APB4_DIV2;
    if (HAL_RCC_ClockConfig(&rccClkInitStruct, FLASH_LATENCY_4) != HAL_OK)
    {
        Assert_Failed(__FILE__, __LINE__);
    }

    /* Initializes the peripherals clock */
    RCC_PeriphCLKInitTypeDef periphClkInitStruct = { 0 };
    periphClkInitStruct.PLL2.PLL2M = 25;
    periphClkInitStruct.PLL2.PLL2N = 330;
    periphClkInitStruct.PLL2.PLL2P = 3;
    periphClkInitStruct.PLL2.PLL2Q = 2;
    periphClkInitStruct.PLL2.PLL2R = 2;
    periphClkInitStruct.PLL2.PLL2RGE = RCC_PLL2VCIRANGE_0;
    periphClkInitStruct.PLL2.PLL2VCOSEL = RCC_PLL2VCOMEDIUM;
    periphClkInitStruct.PLL2.PLL2FRACN = 0;
    periphClkInitStruct.PLL3.PLL3M = 25;
    periphClkInitStruct.PLL3.PLL3N = 330;
    periphClkInitStruct.PLL3.PLL3P = 2;
    periphClkInitStruct.PLL3.PLL3Q = 2;
    periphClkInitStruct.PLL3.PLL3R = 10;
    periphClkInitStruct.PLL3.PLL3RGE = RCC_PLL3VCIRANGE_0;
    periphClkInitStruct.PLL3.PLL3VCOSEL = RCC_PLL3VCOMEDIUM;
    periphClkInitStruct.PLL3.PLL3FRACN = 0;
    periphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_FMC | RCC_PERIPHCLK_LTDC | RCC_PERIPHCLK_SPI2 | RCC_PERIPHCLK_SPI1;
    periphClkInitStruct.FmcClockSelection = RCC_FMCCLKSOURCE_D1HCLK;
    periphClkInitStruct.Spi123ClockSelection = RCC_SPI123CLKSOURCE_CLKP;
    if (HAL_RCCEx_PeriphCLKConfig(&periphClkInitStruct) != HAL_OK)
    {
        Assert_Failed(__FILE__, __LINE__);
    }
}

/**
 * @brief  配置MPU内存保护单元（设置XSPI HyperRAM区域的缓存属性）
 */
void MPU_Config(void)
{
    MPU_Region_InitTypeDef mpuInitStruct;

    HAL_MPU_Disable(); /* 先禁止MPU */

    /* 配置XSPI HyperRAM对应区域（0xC0000000~0xC1FFFFFF，32MB） */
    mpuInitStruct.Enable = MPU_REGION_ENABLE;
    mpuInitStruct.BaseAddress = SDRAM_BANK_ADDR;
    mpuInitStruct.Size = MPU_REGION_SIZE_32MB;
    mpuInitStruct.AccessPermission = MPU_REGION_FULL_ACCESS;
    mpuInitStruct.IsBufferable = MPU_ACCESS_NOT_BUFFERABLE;
    mpuInitStruct.IsCacheable = MPU_ACCESS_CACHEABLE;
    mpuInitStruct.IsShareable = MPU_ACCESS_NOT_SHAREABLE;
    mpuInitStruct.Number = MPU_REGION_NUMBER2;
    mpuInitStruct.TypeExtField = MPU_TEX_LEVEL0;
    mpuInitStruct.SubRegionDisable = 0x00;
    mpuInitStruct.DisableExec = MPU_INSTRUCTION_ACCESS_ENABLE;
    HAL_MPU_ConfigRegion(&mpuInitStruct);

    HAL_MPU_Enable(MPU_PRIVILEGED_DEFAULT); /* 使能MPU */
}

/**
 * @brief  断言失败处理函数，打印出错文件名和行号
 * @param  file 发生断言失败的文件名，以字符串形式传入
 * @param  line 发生断言失败的代码行号，无符号32位整数
 */
void Assert_Failed(char *file, uint32_t line)
{
    printf("Assert Failed: %s(%d)\r\n", file, line);
}

#define INIT_TITLE(TITLE) LCD_ShowString(0, 0, TITLE " Init", LCD_CYAN, LCD_BLACK)
#define INIT_INFO(INFO)   LCD_ShowString(0, (g_LCD.TftFlag ? 20 : 24) * (line++), INFO " Init...", LCD_YELLOW, LCD_BLACK)

/**
 * @brief  板级初始化（依次完成时钟、MPU、缓存及各外设驱动初始化并显示进度）
 */
void Board_Init(void)
{
    uint8_t line = 1;

    /* 顶层初始化 */
    MPU_Config();         /* MPU配置 */
    SCB_EnableICache();   /* 使能 ICache */
    SCB_EnableDCache();   /* 使能 DCache */
    HAL_Init();           /* 初始化HAL库 */
    System_ClockConfig(); /* 配置系统时钟 */

    /* 板级组件初始化 */
    Timer_Init();
    SDRAM_Init();
    DWT_Init();
    Memory_Init();
    UART_Init();
    LCD_Init();
    LCD_SetFontSize(g_LCD.TftFlag ? LCD_FontSize16 : LCD_FontSize24);
    INIT_TITLE("Board");
    INIT_INFO("Timer");
    INIT_INFO("Memory");
    INIT_INFO("Debug UART");
    INIT_INFO("LCD");
    RTC_Init();
    INIT_INFO("LED");
    WS2812_Init();
    LED_Init();
    INIT_INFO("ADC");
    Battery_Init();
    INIT_INFO("KEY");
    Key_Init();
    INIT_INFO("SoftI2C");
    SoftI2C_Init();
    INIT_INFO("PCA9685");
    PCA9685_Init();
    INIT_INFO("Encoder");
    Encoder_Init();
    INIT_INFO("Motor");
    Motor_Init();
    INIT_INFO("Servo");
    Servo_Init();
    INIT_INFO("Light");
    Light_Init();
    INIT_INFO("Beep");
    Beep_Init();
    INIT_INFO("CS100A");
    CS100A_Init();

    /* 等待W25QXX初始化完成 */
    INIT_INFO("W25QXX/SDRAM");
    while (W25QXX_Init() != W25QXX_OK)
    {
        LED1_Turn();
        HAL_Delay(400);
    }

    INIT_INFO("BEEP");
    Beep_Disable();
    LCD_ShowString(0, 0, "Board System OK! ", LCD_CYAN, LCD_BLACK);
    LCD_ShowString(0, 24 * (line++), "Wait To Next Init...", LCD_CYAN, LCD_BLACK);

    /* 等待3s完成板级初始化 */
    uint32_t startTick = HAL_GetTick();
    while (Timer_GetDiffMs(startTick) < 3000)
    {
        LED1_Turn();
        HAL_Delay(250);
    }

    LCD_Fill(0, 0, 800, 480, LCD_BLACK);
}

/**
 * @brief  内存拷贝函数（优化：4字节对齐访问，处理重叠）
 * @param  dst  目标内存区域
 * @param  src  源内存区域
 * @param  size 拷贝大小（字节数）
 */
void Memory_Copy(void *dst, const void *src, size_t size)
{
    uint8_t *pDst = (uint8_t *)dst;
    const uint8_t *pSrc = (const uint8_t *)src;

    if (size == 0) return;

    /* ---- 处理重叠：从后往前拷贝 ---- */
    if (pDst > pSrc && pDst < pSrc + size)
    {
        // 从后往前，按字节处理（重叠时无法安全使用对齐拷贝，因为可能破坏未拷贝数据）
        pDst += size;
        pSrc += size;
        while (size--)
        {
            *--pDst = *--pSrc;
        }
        return;
    }

    /* ---- 非重叠或正向拷贝，尝试 4 字节对齐 ---- */
    // 1. 先处理头部的未对齐字节（使 dst 对齐到 4 字节）
    while (size && ((uintptr_t)pDst & 3))
    {
        *pDst++ = *pSrc++;
        size--;
    }

    // 2. 主循环：以 4 字节为单位拷贝（要求 src 也对齐，否则仍用字节）
    if (size >= 4 && ((uintptr_t)pSrc & 3) == 0)
    {
        uint32_t *pDst32 = (uint32_t *)pDst;
        const uint32_t *pSrc32 = (const uint32_t *)pSrc;
        size_t count = size / 4;
        while (count--)
        {
            *pDst32++ = *pSrc32++;
        }
        pDst = (uint8_t *)pDst32;
        pSrc = (const uint8_t *)pSrc32;
        size %= 4;
    }
    else
    {
        // 若 src 未对齐，仍使用字节拷贝避免未对齐访问（部分平台会异常）
        // 但也可逐字节处理尾部，但这里直接字节拷贝剩余所有
        while (size--)
        {
            *pDst++ = *pSrc++;
        }
        return;
    }

    // 3. 处理剩余不足 4 字节的尾部
    while (size--)
    {
        *pDst++ = *pSrc++;
    }
}

/**
 * @brief  内存设置函数（优化：4字节对齐写入）
 * @param  dst   目标内存区域
 * @param  value 8位设置数值（低字节有效）
 * @param  size  设置大小（字节数）
 */
void Memory_Set(void *dst, uint8_t value, size_t size)
{
    uint8_t *pDst = (uint8_t *)dst;

    if (size == 0) return;

    // 1. 处理头部未对齐字节（使 pDst 对齐到 4 字节）
    while (size && ((uintptr_t)pDst & 3))
    {
        *pDst++ = value;
        size--;
    }

    // 2. 主循环：按 4 字节写入
    if (size >= 4)
    {
        // 将 value 扩展为 32 位重复值 (0x01010101 模式)
        uint32_t pattern = value | (value << 8) | (value << 16) | (value << 24);
        uint32_t *pDst32 = (uint32_t *)pDst;
        size_t count = size / 4;
        while (count--)
        {
            *pDst32++ = pattern;
        }
        pDst = (uint8_t *)pDst32;
        size %= 4;
    }

    // 3. 处理剩余不足 4 字节的尾部
    while (size--)
    {
        *pDst++ = value;
    }
}
