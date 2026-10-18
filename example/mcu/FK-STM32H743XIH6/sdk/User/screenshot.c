#include "screenshot.h"
#include "tiny_jpeg.h"
#include "gui.h"
#include "remote_gui.h"
#include "file.h"

/**
 * @brief 提取 RGB565 像素的分量
 *
 * RGB565 格式 (16-bit):
 * - R: bits[15:11] (5 bits)
 * - G: bits[10:5]  (6 bits)
 * - B: bits[4:0]   (5 bits)
 */
#define RGB565_R(pixel)         (((pixel) >> 11) & 0x1F)
#define RGB565_G(pixel)         (((pixel) >> 5) & 0x3F)
#define RGB565_B(pixel)         ((pixel) & 0x1F)

/**
 * @brief 将 RGB565 分量扩展到 8-bit
 *
 * R/B: 5-bit -> 8-bit: 左移3位或乘以255/31
 * G:   6-bit -> 8-bit: 左移2位或乘以255/63
 *
 * 这里使用优化的位扩展方法
 */
#define R5_TO_R8(r)             (((r) << 3) | ((r) >> 2))                                                                          /* 5-bit 扩展到 8-bit */
#define G6_TO_G8(g)             (((g) << 2) | ((g) >> 4))                                                                          /* 6-bit 扩展到 8-bit */
#define B5_TO_B8(b)             (((b) << 3) | ((b) >> 2))                                                                          /* 5-bit 扩展到 8-bit */
#define RGB565_TO_RGB888(pixel) ((B5_TO_B8(RGB565_B(pixel)) << 16) | (G6_TO_G8(RGB565_G(pixel)) << 8) | R5_TO_R8(RGB565_R(pixel))) /* 快速替换宏 */

/* LCD绘图缓冲区 */
static uint32_t *s_LCD_VRAM = NULL;
static uint32_t *s_OLED_VRAM = NULL;
extern SemaphoreHandle_t xGuiMutex;

void GUI_OLED_DrawPointAdapter(GUI_DeviceInfo_T *deviceInfo, uint16_t x, uint16_t y, uint32_t color);

/**
 * @brief LCD设备信息结构体实例
 * @details 初始化LCD设备的所有回调函数和参数，适配128x64分辨率的OLED屏幕
 */
GUI_DeviceInfo_T g_OLED_DeviceInfo = {
    .deviceInit = NULL,                             /* 设备初始化回调，此处为空 */
    .drawBufferCallback = NULL,                     /* 缓冲区绘制回调 */
    .drawMonoImageCallback = NULL,                  /* 单色图像绘制回调 */
    .drawPointCallback = GUI_OLED_DrawPointAdapter, /* 点绘制回调 */
    .fillColorCallback = NULL,                      /* 区域填充回调 */
    .InitFlag = 1,                                  /* 设备初始化标志 */
    .Width = 128,                                   /* OLED宽度 */
    .Height = 64,                                   /* OLED高度 */
    .fillMode = GUI_PAINTER_FILL_COLOR,             /* 覆盖绘图 */
    .swapFlag = 1,                                  /* 需要根据具体的LCD设置 */
    .tagInfo = NULL                                 /* 附加信息指针 */
};

/**
 * @brief  LCD点绘制适配器函数实现
 * @param  deviceInfo 设备信息结构体指针
 * @param  x 点的X坐标
 * @param  y 点的Y坐标
 * @param  color 点的颜色（32位RGB格式）
 * @note   调用底层LCD驱动的点绘制函数
 */
void GUI_OLED_DrawPointAdapter(GUI_DeviceInfo_T *deviceInfo, uint16_t x, uint16_t y, uint32_t color)
{
    static uint16_t Width = 0, Height = 0;

    if (x >= g_OLED_DeviceInfo.Width || y >= g_OLED_DeviceInfo.Height) return;

    /* 开机初始化图像信息 */
    if (s_OLED_VRAM == NULL)
    {
        s_OLED_VRAM = (uint32_t *)0xC0232800;
        Width = g_OLED_DeviceInfo.Width;
        Height = g_OLED_DeviceInfo.Height;
        g_OLED_DeviceInfo.tagInfo = s_OLED_VRAM;
        Memory_Set(s_OLED_VRAM, 0, 32768);
    }

    /* 判断图像信息，如果信息发生变化，则重新读取 */
    if (Width != g_OLED_DeviceInfo.Width || Height != g_OLED_DeviceInfo.Height)
    {
        Memory_Set(s_OLED_VRAM, 0, 32768);
    }

    uint32_t index = y * g_OLED_DeviceInfo.Width + x;
    uint32_t pixel = g_OLED_DeviceInfo.swapFlag ? (uint16_t)GUI_SwapBytes(color) : (uint16_t)(color);
    uint32_t rgb888 = RGB565_TO_RGB888(pixel);
    s_OLED_VRAM[index] = rgb888;
}

/**
 * @brief  内存设备绘制点回调函数
 * @param  x 目标点X坐标
 * @param  y 目标点Y坐标
 * @param  color 绘制颜色值
 * @note   将点绘制到内存图形缓冲区，自动进行边界检查
 */
void Screenshot_BufferDrawPoint(uint16_t x, uint16_t y, uint32_t color)
{
    static uint16_t Width = 0, Height = 0;

    if (x >= g_LCD_DeviceInfo.Width || y >= g_LCD_DeviceInfo.Height) return;

    /* 开机初始化图像信息 */
    if (s_LCD_VRAM == NULL)
    {
        s_LCD_VRAM = (uint32_t *)0xC00BB800;
        Width = g_LCD_DeviceInfo.Width;
        Height = g_LCD_DeviceInfo.Height;
        Memory_Set(s_LCD_VRAM, 0, LCD_HEIGHT * LCD_WIDTH * 4);
    }

    /* 判断图像信息，如果信息发生变化，则重新读取 */
    if (Width != g_LCD_DeviceInfo.Width || Height != g_LCD_DeviceInfo.Height)
    {
        Memory_Set(s_LCD_VRAM, 0, LCD_HEIGHT * LCD_WIDTH * 4);
    }

    uint32_t index = y * g_LCD_DeviceInfo.Width + x;
    uint32_t pixel = g_LCD_DeviceInfo.swapFlag ? (uint16_t)GUI_SwapBytes(color) : (uint16_t)(color);
    uint32_t rgb888 = RGB565_TO_RGB888(pixel);
    s_LCD_VRAM[index] = rgb888;
}

/**
 * @brief  内存设备区域填充回调函数
 * @param  x 起始X坐标
 * @param  y 起始Y坐标
 * @param  Width 填充区域宽度
 * @param  Height 填充区域高度
 * @param  color 填充颜色值
 * @note   自动裁剪填充区域至缓冲区边界内
 */
void Screenshot_BufferFillColor(uint16_t x, uint16_t y, uint16_t Width, uint16_t Height, uint32_t color)
{
    int32_t i, j;

    for (j = y; j < y + Height; j++)
    {
        for (i = x; i < x + Width; i++)
        {
            Screenshot_BufferDrawPoint(i, j, g_LCD_DeviceInfo.swapFlag ? (uint16_t)GUI_SwapBytes(color) : (uint16_t)(color));
        }
    }
}

/**
 * @brief  内存设备绘制缓冲区回调函数
 * @param  x 起始X坐标
 * @param  y 起始Y坐标
 * @param  buffer 图像数据缓冲区指针
 * @param  Width 绘制区域宽度
 * @param  Height 绘制区域高度
 * @note   自动裁剪绘制区域至缓冲区边界内
 */
void Screenshot_BufferDrawBuffer(uint16_t x, uint16_t y, const uint8_t *buffer, uint16_t Width, uint16_t Height)
{
    uint16_t i = 0, j = 0, *ptr = (uint16_t *)buffer;

    for (j = y; j < y + Height; j++)
    {
        for (i = x; i < x + Width; i++)
        {
            Screenshot_BufferDrawPoint(i, j, *ptr++);
        }
    }
}

/**
 * @brief  生成基于时间的文件名
 * @param  filename 输出文件名缓冲区
 * @param  size 缓冲区大小
 */
static void Screenshot_GenerateFilename(char *filename, uint32_t size)
{
    RTC_ReadTime();

    /* 生成文件名 */
    snprintf(filename, size, "0:/screenshot/%04d年%02d月%02d日 %02d时%02d分%02d秒.jpg",
             g_RTC_Data.year, g_RTC_Data.month, g_RTC_Data.day,
             g_RTC_Data.hour, g_RTC_Data.minute, g_RTC_Data.second);
}

/**
 * @brief tiny_jpeg 写入上下文
 */
typedef struct tagTJE_WriteContext_T
{
    uint8_t *buffer;
    int size;
    int capacity;
} TJE_WriteContext_T;

/**
 * @brief  tiny_jpeg 写入内存回调
 * @param  context 写入上下文指针
 * @param  data 待写入数据指针
 * @param  size 待写入数据大小
 */
static void TJE_WriteMemory(void *context, void *data, int size)
{
    TJE_WriteContext_T *ctx = (TJE_WriteContext_T *)context;

    if (ctx->size + size > ctx->capacity)
    {
        return;
    }

    memcpy(ctx->buffer + ctx->size, data, size);
    ctx->size += size;
}

/**
 * @brief  截取LCD/OLED当前屏幕信息
 * @param  filename 输出文件名缓冲区
 * @param  filenameSize 输出文件名缓冲区大小
 * @param  selected 选择的屏幕输出
 * @retval 0 转换失败
 * @retval 非零值 转换成功，返回写入文件的大小
 */
uint32_t Screenshot_Run(char *filename, uint32_t filenameSize, uint8_t selected)
{
    uint32_t writeBytes = 0;
    uint16_t Width = g_LCD_DeviceInfo.Width, Height = g_LCD_DeviceInfo.Height;  // LCD 大小
    uint8_t *buffer = (uint8_t *)s_LCD_VRAM;

    /* 处理其他屏幕的数据 */
    if (selected == SCREEN_SELECT_OLED_WINDOWS_1 || selected == SCREEN_SELECT_OLED_WINDOWS_2)
    {
        Width = g_OLED_DeviceInfo.Width, Height = g_OLED_DeviceInfo.Height;  // OLED 大小
        buffer = (uint8_t *)s_OLED_VRAM;
        OLED_Clear();
        if (selected == SCREEN_SELECT_OLED_WINDOWS_1) { RemoteGUI_DisplayMainScreen(); }
        if (selected == SCREEN_SELECT_OLED_WINDOWS_2) { RemoteGUI_DisplayRemoteScreen(); }
    }

    uint32_t max_jpeg_capacity = 3 * Width * Height;
    uint8_t *jpeg_buf = (uint8_t *)Memory_Malloc(MP_RAM_EX, sizeof(uint8_t) * max_jpeg_capacity);

    if (jpeg_buf == NULL || filename == NULL)
    {
        if (jpeg_buf != NULL) Memory_Free(MP_RAM_EX, jpeg_buf);
        if (filename != NULL) Memory_Free(MP_RAM_EX, filename);
        return 0;
    }

    FatFS_CreateDirectory("0:/screenshot");  // 确保路径存在
    Screenshot_GenerateFilename(filename, filenameSize);

    // ===== JPEG 编码 =====
    TJE_WriteContext_T writeCTX = {
        .buffer = jpeg_buf,
        .size = 0,
        .capacity = max_jpeg_capacity
    };
    int result = 0;

    if (xSemaphoreTake(xGuiMutex, portMAX_DELAY) != pdTRUE)
    {
        result = tje_encode_with_func(
            TJE_WriteMemory,
            &writeCTX,
            3,  // quality: 3 = 最高的质量
            Width,
            Height,
            4,                 // num_components: 4 = ARGB
            (uint8_t *)buffer  // RGB888 数据
        );
    }

    if (result != 0)
    {
        Screenshot_GenerateFilename(filename, filenameSize);
        writeBytes = FatFS_WriteFile(filename, writeCTX.buffer, writeCTX.size);
    }

    return writeBytes;
}
