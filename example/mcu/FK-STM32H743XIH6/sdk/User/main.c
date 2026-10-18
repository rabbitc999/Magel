#include "main.h"
#include "simple_task.h"
#include "logger.h"

volatile uint32_t g_RunTimerTicks = 0UL;
static uint8_t line = 1;

/**
 * @brief  主程序入口
 * @retval 正常情况下不会返回
 */
int main(void)
{
    /* 失能全局中断 */
    BSP_DISABLE_INT();

    /* 板级初始化 */
    Board_Init();
    Log_Init();
    Log_Write(LOG_INFO, "BOOT", "board initialized");

    /* 使能全局中断 */
    BSP_ENABLE_INT();
#ifdef CAR_SERVO_ZERO
    /* 舵机归中专用固件：PCA9685仅0~5共6路，加PB14/PB15两路，共8路。
     * Board_Init已完成归中；屏幕常驻Servo Zero，不启动遥控和小车任务，
     * 防止后续控制指令覆盖90度中位。TIM6中断继续运行并喂看门狗。
     */
    LCD_Fill(0, 0, g_LCD.Width, g_LCD.Height, LCD_BLACK);
    LCD_ShowString(20, 40, "Servo Zero", LCD_CYAN, LCD_BLACK);
    while (1)
    {
        __WFI();
    }
#endif

    /* 判断是否使能USB文件接收模式 */
    USB_DetectMode();

    /* 应用初始化 */
    INIT_TITLE("Software");
    INIT_INFO("FatFS");
    FatFS_Init();
    INIT_INFO("Param");
    Param_Init();
    INIT_INFO("Car");
    Car_Init();
    INIT_INFO("Command");
    CMD_Init();
    Math_Init();

    /* 判断是否使能NRF24L01无线模块接收 */
    if ((g_ParamData.app_UseDevice & 0x03) == 0x02)
    {
        INIT_INFO("NRF24L01");
        NRF24L01_Init();
    }

    /* 判断是否使能ESP8266无线模块接收 */
    if ((g_ParamData.app_UseDevice & 0x0F) == 0x04)
    {
        INIT_INFO("ESP8266");
        ESP8266_ReadInfo();
        ESP8266_Init();
    }

    /* 判断是否使能蓝牙无线模块接收 */
    if (g_ParamData.app_UseDevice & 0x08)
    {
        INIT_INFO("Bluetooth");
        Bluetooth_ReadInfo();
        Bluetooth_Init();
    }

    /* 初始化GUI和运行任务 */
    INIT_INFO("Program Task");

    ProgramTask_Init();

    /* 开启看门狗 */
    IWDG_Init(IWDG_PRESCALER_64, 500);

    /* 显示程序运行信息 */
    Program_PrintInfo();

    /* 启动调度器 */
    while (1)
    {
        Task_Loop();
    }
}

/**
 * @brief  检测USB启动模式
 */
void USB_DetectMode(void)
{
    if ((RTC_ReadBKP() & 0x0000FFFF) != 0x00001234) return;

    USB_Init();
    uint32_t State = RTC_ReadBKP() >> 16;
    LCD_Fill(0, 0, 240, 320, LCD_BLACK);

    switch (State)
    {
    case 0x1234:
    {
        LCD_ShowString(0, 0, "USB File Copy Mode!!!", LCD_YELLOW, LCD_BLACK);
        LCD_ShowString(0, 30, "Press ANY Key To Reset!!!", LCD_MAGENTA, LCD_BLACK);
        break;
    }
    case 0x1111:
    {
        LCD_ShowString(0, 0, "Wait Chip Erase!!!", LCD_WHITE, LCD_BLACK);
        W25QXX_ChipErase();
        LCD_ShowString(0, 0, "FATFS Faild!!!    ", LCD_WHITE, LCD_BLACK);
        LCD_ShowString(0, 30, "USE USB TO FORMAT!!!", LCD_WHITE, LCD_BLACK);
        LCD_ShowString(0, 300, "Press ANY Key To Reset!!!", LCD_YELLOW, LCD_BLACK);
        break;
    }
    case 0x2212:
    {
        LCD_ShowString(0, 0, "Cannot Find Chinese Font!!!", LCD_WHITE, LCD_BLACK);
        LCD_ShowString(0, 30, "USE USB TO COPY!!!", LCD_WHITE, LCD_BLACK);
        LCD_ShowString(0, 60, "COPY FILE:", LCD_WHITE, LCD_BLACK);
        LCD_ShowString(0, 90, "0:/font/CH_Font_12.bin", LCD_CYAN, LCD_BLACK);
        LCD_ShowString(0, 120, "Press ANY Key To Reset!!!", LCD_YELLOW, LCD_BLACK);
        break;
    }
    case 0x2216:
    {
        LCD_ShowString(0, 0, "Cannot Find Chinese Font!!!", LCD_WHITE, LCD_BLACK);
        LCD_ShowString(0, 30, "USE USB TO COPY!!!", LCD_WHITE, LCD_BLACK);
        LCD_ShowString(0, 60, "COPY FILE:", LCD_WHITE, LCD_BLACK);
        LCD_ShowString(0, 90, "0:/font/CH_Font_16.bin", LCD_CYAN, LCD_BLACK);
        LCD_ShowString(0, 120, "Press ANY Key To Reset!!!", LCD_YELLOW, LCD_BLACK);
        break;
    }
    case 0x2224:
    {
        LCD_ShowString(0, 0, "Cannot Find Chinese Font!!!", LCD_WHITE, LCD_BLACK);
        LCD_ShowString(0, 30, "USE USB TO COPY!!!", LCD_WHITE, LCD_BLACK);
        LCD_ShowString(0, 60, "COPY FILE:", LCD_WHITE, LCD_BLACK);
        LCD_ShowString(0, 90, "0:/font/CH_Font_24.bin", LCD_CYAN, LCD_BLACK);
        LCD_ShowString(0, 120, "Press ANY Key To Reset!!!", LCD_YELLOW, LCD_BLACK);
        break;
    }
    case 0x2232:
    {
        LCD_ShowString(0, 0, "Cannot Find Chinese Font!!!", LCD_WHITE, LCD_BLACK);
        LCD_ShowString(0, 30, "USE USB TO COPY!!!", LCD_WHITE, LCD_BLACK);
        LCD_ShowString(0, 60, "COPY FILE:", LCD_WHITE, LCD_BLACK);
        LCD_ShowString(0, 90, "0:/font/CH_Font_32.bin", LCD_CYAN, LCD_BLACK);
        LCD_ShowString(0, 120, "Press ANY Key To Reset!!!", LCD_YELLOW, LCD_BLACK);
        break;
    }
    case 0x3324:
    {
        LCD_ShowString(0, 0, "Cannot Find Chinese Font!!!", LCD_WHITE, LCD_BLACK);
        LCD_ShowString(0, 30, "USE USB TO COPY!!!", LCD_WHITE, LCD_BLACK);
        LCD_ShowString(0, 60, "COPY FILE:", LCD_WHITE, LCD_BLACK);
        LCD_ShowString(0, 90, "0:/font/GB2312_24.FON", LCD_CYAN, LCD_BLACK);
        LCD_ShowString(0, 120, "Press ANY Key To Reset!!!", LCD_YELLOW, LCD_BLACK);
        break;
    }
    case 0x3333:
    {
        LCD_ShowString(0, 0, "Cannot Find Image File!!!", LCD_WHITE, LCD_BLACK);
        LCD_ShowString(0, 30, "USE USB TO COPY!!!", LCD_WHITE, LCD_BLACK);
        LCD_ShowString(0, 60, "COPY FILE:", LCD_WHITE, LCD_BLACK);
        LCD_ShowString(0, 90, "0:/img/OLD_BACKGROUND.BIN", LCD_CYAN, LCD_BLACK);
        LCD_ShowString(0, 120, "Press ANY Key To Reset!!!", LCD_YELLOW, LCD_BLACK);
        break;
    }
    }

    /* 任意键：系统重启 */
    while (!Key_IsAnyKeyPressed())
    {
        /* 闪灯提示 */
        LED1_Turn();
        HAL_Delay(500);
    }

    RTC_WriteBKP(0x00000000);
    NVIC_SystemReset();
}

/**
 * @brief  定时器6中断回调函数
 */
void TIM6_DAC_IRQHandler(void)
{
    static uint16_t EventCount = 0;

    if (LL_TIM_IsActiveFlag_UPDATE(TIM6))
    {
        /* 调度处理 */
        Task_Tick();

        /* 每20ms处理一次按键 */
        if (++EventCount > 20)
        {
            EventCount = 0;
            Key_Tick();
            Beep_Tick();
            IWDG_Feed();
        }

        g_RunTimerTicks++;
        LL_TIM_ClearFlag_UPDATE(TIM6);
    }
}

/**
 * @brief  定时器7中断回调函数
 */
void TIM7_IRQHandler(void)
{
    Car_Tick(); /* 小车运动控制 */

    LL_TIM_ClearFlag_UPDATE(TIM7);
}

/**
 * @brief  程序运行信息显示
 */
void Program_PrintInfo(void)
{
    DEBUG_PRINTF("CPU: STM32H743XIH6, BGA240, 频率=%dMHz\r\n", SystemCoreClock / 1000000);
    DEBUG_PRINTF("UID = %08X %08X %08X\r\n", g_CPU_Serial[2], g_CPU_Serial[1], g_CPU_Serial[0]);
    DEBUG_PRINTF("RTC = %d\r\n", RTC_GetTimestamp());

    FatFS_ShowInfo();
}
