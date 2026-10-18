#include "gui.h"
#include "gui_runtime.h"
#include "gui_capture.h"

#include "store.h"
#include "file.h"
#include "car.h"
#include "cmd.h"

#include "simple_task.h"

/**
 * @brief LCD设备信息结构体实例
 * @details 初始化LCD设备的所有回调函数和参数，适配240x320分辨率的LCD屏幕
 */
GUI_DeviceInfo_T g_LCD_DeviceInfo = {
    .deviceInit = NULL,                              /* 设备初始化回调，此处为空 */
    .drawBufferCallback = GUI_LCD_DrawBufferAdapter, /* 缓冲区绘制回调 */
    .drawMonoImageCallback = NULL,                   /* 单色图像绘制回调 */
    .drawPointCallback = GUI_LCD_DrawPointAdapter,   /* 点绘制回调 */
    .fillColorCallback = GUI_LCD_FillAdapter,        /* 区域填充回调 */
    .InitFlag = 1,                                   /* 设备初始化标志 */
    .fillMode = GUI_PAINTER_FILL_COLOR,              /* 覆盖绘图 */
    .swapFlag = 1,                                   /* 需要根据具体的LCD设置 */
    .tagInfo = NULL                                  /* 附加信息指针 */
};

extern LidarDataList_T g_LidarDataList;

// TFT显示屏的临时缓存
#define LCD_TFT_CACHE_MEM ((uint16_t *)SDRAM_BANK_ADDR)  // 缓存显存的起始地址

/**
 * @brief 菜单按键的基本操作函数 (用户实现)
 * @param baseUi 指向菜单设备的指针
 * @return None
 */
void GUI_MenuKeyBasicOperator(GUI_Device_T *baseUi)
{
    /* KEY_ENTER: 进入功能 */
    if (Key_IsAction(KEY_ENTER))
    {
        GUI_MenuEnterEvent(baseUi);
    }

    /* KEY_MODE: 单击点击退出功能(仅针对于值模式) */
    if (Key_IsClicked(KEY_MODE) && baseUi->valueFlag)
    {
        GUI_MenuExitEvent(baseUi);
    }

    /* KEY_UP: 向上操作功能 */
    if (Key_IsAction(KEY_UP))
    {
        if (baseUi->valueFlag)
        {
            GUI_MenuValueChangeEvent(baseUi, 1);
        }
        else
        {
            GUI_MenuItemChangeEvent(baseUi, -1);
        }
    }

    /* KEY_DOWN: 向下操作功能 */
    if (Key_IsAction(KEY_DOWN))
    {
        if (baseUi->valueFlag)
        {
            GUI_MenuValueChangeEvent(baseUi, -1);
        }
        else
        {
            GUI_MenuItemChangeEvent(baseUi, 1);
        }
    }
}

/**
 * @brief LCD点绘制适配器函数实现
 * @details 调用底层LCD驱动的点绘制函数
 *
 * @param deviceInfo 设备信息结构体指针
 * @param x 点的X坐标
 * @param y 点的Y坐标
 * @param color 点的颜色（32位RGB格式）
 */
void GUI_LCD_DrawPointAdapter(GUI_DeviceInfo_T *deviceInfo, uint16_t x, uint16_t y, uint32_t color)
{
    LCD_TFT_CACHE_MEM[y * GUI_SCREEN_WIDTH + x] = color;
}

/**
 * @brief LCD区域填充适配器函数实现
 * @details 调用底层LCD驱动的区域填充函数
 *
 * @param deviceInfo 设备信息结构体指针
 * @param x 区域左上角X坐标
 * @param y 区域左上角Y坐标
 * @param Width 区域宽度
 * @param Height 区域高度
 * @param color 填充颜色
 */
void GUI_LCD_FillAdapter(GUI_DeviceInfo_T *deviceInfo, uint16_t x, uint16_t y, uint16_t Width, uint16_t Height, uint32_t color)
{
    DMA2D_MemFill(x, y, Width, Height, (color << 8) | (color >> 8), (uint32_t *)LCD_TFT_CACHE_MEM, GUI_SCREEN_WIDTH);
}

/**
 * @brief LCD缓冲区绘制适配器函数实现
 * @details 调用底层LCD驱动的图像显示函数
 *
 * @param deviceInfo 设备信息结构体指针
 * @param x 图像左上角X坐标
 * @param y 图像左上角Y坐标
 * @param buffer 图像数据缓冲区
 * @param Width 图像宽度
 * @param Height 图像高度
 */
void GUI_LCD_DrawBufferAdapter(GUI_DeviceInfo_T *deviceInfo, uint16_t x, uint16_t y, const uint8_t *buffer, uint16_t Width, uint16_t Height)
{
    DMA2D_MemCopy(x, y, Width, Height, (uint32_t *)buffer, (uint32_t *)LCD_TFT_CACHE_MEM, GUI_SCREEN_WIDTH);
}

/**
 * @brief 分配指定大小的内存空间
 * @param size 需要分配的内存大小
 * @return 分配的内存指针
 */
void *GUI_MenuMalloc(size_t size)
{
    return GUI_RuntimeAllocate(size);
}

/**
 * @brief 释放内存
 * @param block 需要释放的内存块指针
 * @return None
 */
void GUI_MenuFree(void *block)
{
    GUI_RuntimeFree(block);
}

uint32_t GUI_GetTick(void)
{
    return HAL_GetTick();
}

uint32_t GUI_GetDiffMs(uint32_t current)
{
    return Timer_GetDiffMs(current);
}

/**
 * @brief 释放内存
 * @param block 需要释放的内存块指针
 * @return None
 */
uint32_t GUI_MenuGetMallocTotal(void)
{
    return GUI_RuntimeCapacity();
}

/**
 * @brief 释放内存
 * @param block 需要释放的内存块指针
 * @return None
 */
uint32_t GUI_MenuGetMallocUsed(void)
{
    return GUI_RuntimeUsed();
}

/**
 * @brief 用户界面循环
 */
void GUI_Loop(void)
{
    /* 检测新华的龙脉的点击 */
    if (g_TouchInfo.flag && g_GUI_User.dragonClicked)
    {
        if (g_TouchInfo.x[0] > 360 && g_TouchInfo.x[0] <= 800)
        {
            Beep_Play(100, 400, 2);
        }
    }

    /* 绘制图像 */
    if (g_GUI_User.showPlotIndex == 1 && g_GUI_User.showPlotMotorPidIndex < CAR_MAX_INDEX)
    {
        GUI_CoordinateGraphAddData(&g_SlipGraph, g_Car.remote.slipAlpha, g_Car.remote.slipBeta, g_Car.remote.slipGamma);
        GUI_CoordinateGraphDraw(&g_SlipGraph);
    }
    if (g_GUI_User.showPlotIndex == 2)
    {
        GUI_CoordinateGraphAddData(&g_ImuGraph, g_IMU.Pitch, g_IMU.Roll, g_IMU.Yaw);
        GUI_CoordinateGraphDraw(&g_ImuGraph);
    }
    if (g_GUI_User.showPlotIndex == 3)
    {
        static float x = 0.0f;
        float sinx = sinf(x);
        float cosx = cosf(x);
        GUI_CoordinateGraphAddData(&g_DetailGraph, sinx, cosx);
        GUI_CoordinateGraphDraw(&g_DetailGraph);
        x += 0.1;
    }
    if (g_GUI_User.showPlotIndex == 4)
    {
        GUI_CoordinateGraphAddData(&g_CarGraph, g_Car.remote.actualSpeedX, g_Car.remote.actualSpeedT);
        GUI_CoordinateGraphDraw(&g_CarGraph);
    }
    if (g_GUI_User.showPlotIndex == 5)
    {
        GUI_CoordinateGraphAddData(&g_MotorMapperGraph, (double)g_Car.stopMotorSpeed[CAR_INDEX_M1], (double)g_Car.stopMotorEncoder[CAR_INDEX_M1],
                                   (double)g_Car.stopMotorSpeed[CAR_INDEX_M2], (double)g_Car.stopMotorEncoder[CAR_INDEX_M2],
                                   (double)g_Car.stopMotorSpeed[CAR_INDEX_M3], (double)g_Car.stopMotorEncoder[CAR_INDEX_M3],
                                   (double)g_Car.stopMotorSpeed[CAR_INDEX_M4], (double)g_Car.stopMotorEncoder[CAR_INDEX_M4]);
        GUI_CoordinateGraphDraw(&g_MotorMapperGraph);
    }
    if (g_GUI_User.showPlotIndex == 6)
    {
        uint32_t maxRadius = g_Car.avoidance.obstacleDistanceThreshold;
        s_polarGraph.fixedBaseRadius = maxRadius + 100;

        /* 添加激光雷达数据点 */
        if (g_LidarDataList.count <= PACKET_LIDAR_MAX_SAMPLES && g_LidarDataList.count > 0)
        {
            /* 清空原来的数据点 */
            GUI_PolarGraphClearPoints(&s_polarGraph);

            for (uint16_t i = 0; i < g_LidarDataList.count; i++)
            {
                GUI_PolarGraphAddPoint(&s_polarGraph, g_LidarDataList.buffer[i].angle + 90, g_LidarDataList.buffer[i].distance, GUI_MAGENTA);
            }
        }
        /* 绘制极坐标图 */
        GUI_PolarGraphDraw(&s_polarGraph);
    }

    /* 主界面运行 */
    GUI_MenuLoop(&g_GUI_User.device);
}

/**
 * @brief GUI初始化函数
 */
static uint64_t s_GuiArena[64 * 1024 / sizeof(uint64_t)];

void GUI_Init(void)
{
    /* Small menu objects stay in AXI SRAM; only frame history uses SDRAM. */
    GUI_RuntimeInit(s_GuiArena, sizeof(s_GuiArena), (void *)0xC0400000, 1024 * 1024);
    /* 初始化字库 */
    GUI_FontInitAll();
    GUI_LCD_SetShowFont(GUI_FONT_TYPE_GB2312);

    /* 设置菜单显示区域 */
    g_LCD_DeviceInfo.Width = GUI_SCREEN_WIDTH;
    g_LCD_DeviceInfo.Height = GUI_SCREEN_HEIGHT;
    g_GUI_User.device.menuPainter.Width = GUI_SCREEN_WIDTH;
    g_GUI_User.device.menuPainter.Height = GUI_SCREEN_HEIGHT;
    g_GUI_User.dragonClicked = 1;

    /* 菜单初始化 */
    GUI_MenuInit();
}

volatile uint32_t g_GUIFrameCount = 0;
volatile uint32_t g_GUILastFrameUs = 0;
volatile uint32_t g_GUIMaxFrameUs = 0;

volatile uint32_t g_GUILastServiceUs = 0;
volatile uint32_t g_GUIMaxServiceUs = 0;

static int GUI_WriteBand(uint16_t y, uint16_t width, uint16_t rows, const uint8_t *pixels)
{
    LCD_ShowImage(0, y, width, rows, pixels);
    return g_LCD.FinishFlag != 0;
}

TaskRet_T GUI_Task(TaskParam_T *param)
{
    static uint32_t nextFrame;
    static uint8_t flushing;
    if (T_STA(param) == 0)
    {
        GUI_Init();
        nextFrame = HAL_GetTick();
        return Task_Return(1, 10);
    }
    uint64_t serviceStart = Timer_GetTicks();
    if (flushing)
    {
        flushing = GUI_RuntimeService(GUI_WriteBand);
    }
    else if ((int32_t)(HAL_GetTick() - nextFrame) >= 0)
    {
        uint64_t frameStart = Timer_GetTicks();
        GUI_Loop();
        /* DMA2D bypasses the CPU cache. Refresh it before comparing pixels. */
        SCB_InvalidateDCache_by_Addr((uint32_t *)LCD_TFT_CACHE_MEM, GUI_SCREEN_WIDTH * GUI_SCREEN_HEIGHT * 2);
        if (g_LCD.TftFlag)
            flushing = GUI_RuntimePresent(LCD_TFT_CACHE_MEM, GUI_SCREEN_WIDTH, GUI_SCREEN_HEIGHT);
        else
            LCD_ShowImage(0, 0, GUI_SCREEN_WIDTH, GUI_SCREEN_HEIGHT, (uint8_t *)LCD_TFT_CACHE_MEM);
        g_GUILastFrameUs = (uint32_t)(Timer_GetTicks() - frameStart);
        if (g_GUILastFrameUs > g_GUIMaxFrameUs) g_GUIMaxFrameUs = g_GUILastFrameUs;
        if (++g_GUIFrameCount % 10 == 0) LED1_Turn();
        nextFrame = HAL_GetTick() + 33;
    }
    GUI_CaptureService();
    g_GUILastServiceUs = (uint32_t)(Timer_GetTicks() - serviceStart);
    if (g_GUILastServiceUs > g_GUIMaxServiceUs) g_GUIMaxServiceUs = g_GUILastServiceUs;
    return Task_Return(1, 1);
}
