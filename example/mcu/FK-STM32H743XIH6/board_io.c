#include "bsp_system.h"
#include "board_io.h"
#include <ctype.h>
#include <stdio.h>
#include <string.h>

static unsigned char serialInput[1024];
static unsigned inputHead, inputCount;
static char commandLine[128];
static unsigned commandLength;
static int passInput, dropCommand;
static uint32_t exitPressTime[2], menuReleaseTime;
static unsigned exitDown[2], menuReleasing, menuReady;

/**
 * @brief 把非命令字节保留到 VM 输入队列，容量不足时丢弃新增字节。
 * @param byte 串口输入的单个原始字节。
 */
static void SaveInput(unsigned char byte)
{
    if (inputCount < sizeof(serialInput))
        serialInput[(inputHead + inputCount++) % sizeof(serialInput)] = byte;
}

/**
 * @brief 解释原工程兼容的文本虚拟按键命令，拒绝未知按键和事件。
 * @note 只注册按键操作，未启用原小车的电机及文件修改命令。
 */
static void HandleKeyCommand(void)
{
    char verb[16], name[16], mode[16] = "click", extra;
    KeyEnum_T key;
    KeyEvent_T event;
    for (unsigned i = 0; i < commandLength; ++i)
        commandLine[i] = (char)tolower((unsigned char)commandLine[i]);
    commandLine[commandLength] = 0;
    int fields = sscanf(commandLine, "%15s %15s %15s %c", verb, name, mode, &extra);
    if (fields < 2 || fields > 3 || strcmp(verb, "key") != 0) goto invalid;
    if (strcmp(name, "mode") == 0)
        key = KEY_MODE;
    else if (strcmp(name, "enter") == 0)
        key = KEY_ENTER;
    else if (strcmp(name, "up") == 0)
        key = KEY_UP;
    else if (strcmp(name, "down") == 0)
        key = KEY_DOWN;
    else
        goto invalid;
    if (strcmp(mode, "click") == 0)
        event = KEY_EVENT_CLICK;
    else if (strcmp(mode, "long") == 0)
        event = KEY_EVENT_LONG_PRESS;
    else if (strcmp(mode, "double") == 0)
        event = KEY_EVENT_DOUBLE_CLICK;
    else if (strcmp(mode, "repeat") == 0)
        event = KEY_EVENT_REPEAT;
    else
        goto invalid;
    uint32_t irq = __get_PRIMASK();
    __disable_irq();
    Key_SetEvent(key, event);
    __set_PRIMASK(irq);
    char response[64];
    int length = snprintf(response, sizeof(response), "KEY_OK %s %s\r\n", name, mode);
    FkSerialWrite(response, (uint16_t)length);
    return;
invalid:
    FkSerialWrite("KEY_ERROR\r\n", 11);
}

/**
 * @brief 分流以 key 空格开头的命令，其他输入按原字节转交 VM。
 * @param byte 当前串口输入字节。
 * @note 超长命令整行丢弃，不执行截断命令；允许命令跨多次 FIFO 读取。
 */
static void RouteByte(unsigned char byte)
{
    if (passInput)
    {
        SaveInput(byte);
        if (byte == '\n') passInput = 0;
        return;
    }
    if (commandLength < 4 && tolower(byte) != "key "[commandLength])
    {
        for (unsigned i = 0; i < commandLength; ++i) SaveInput((unsigned char)commandLine[i]);
        commandLength = 0;
        SaveInput(byte);
        passInput = byte != '\n';
        return;
    }
    if (byte == '\n')
    {
        if (dropCommand)
            FkSerialWrite("KEY_ERROR\r\n", 11);
        else
            HandleKeyCommand();
        commandLength = 0;
        dropCommand = 0;
    }
    else if (commandLength + 1 < sizeof(commandLine))
        commandLine[commandLength++] = (char)byte;
    else
        dropCommand = 1;
}

/**
 * @brief 处理 USART1 虚拟按键命令，并为 VM 保留其他串口输入。
 * @note 每次最多处理二百五十六字节，仅从前台调用。
 */
void FkPollCommands(void)
{
    UartBlock_T *port = UART_FindBlockByPort(COM1);
    if (!port) return;
    unsigned char buffer[64];
    for (unsigned batch = 0; batch < 4; ++batch)
    {
        unsigned count = FIFO_ReadBuffer(&port->RxFIFO, buffer, sizeof(buffer));
        for (unsigned i = 0; i < count; ++i) RouteByte(buffer[i]);
        if (count < sizeof(buffer)) break;
    }
}

/**
 * @brief 同步发送 USART1 调试字节。
 * @param bytes 待发送内容。
 * @param count 字节数。
 */
void FkSerialWrite(const void *bytes, uint16_t count) { UART_SendArrayImm(COM1, (uint8_t *)bytes, count); }
/**
 * @brief 从 USART1 的接收 FIFO 读取当前可用字节。
 * @param buffer 接收缓冲。
 * @param length 缓冲容量。
 * @return 实际读取长度。
 */
unsigned FkSerialRead(unsigned char *buffer, unsigned length)
{
    FkPollCommands();
    unsigned count = 0;
    while (count < length && inputCount)
    {
        buffer[count++] = serialInput[inputHead];
        inputHead = (inputHead + 1) % sizeof(serialInput);
        --inputCount;
    }
    return count;
}
/**
 * @brief 读取 LCD 的水平像素数。
 * @return 屏幕宽度。
 */
uint16_t FkLcdWidth(void) { return g_LCD.Width; }
/**
 * @brief 读取 LCD 的垂直像素数。
 * @return 屏幕高度。
 */
uint16_t FkLcdHeight(void) { return g_LCD.Height; }
/**
 * @brief 通过 BSP 填充已检查边界的 LCD 矩形。
 * @param x 横坐标。
 * @param y 纵坐标。
 * @param width 宽度。
 * @param height 高度。
 * @param color RGB565 颜色。
 */
void FkLcdFill(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint16_t color) { LCD_Fill(x, y, width, height, color); }
/**
 * @brief 通过 BSP 绘制固定大小的 ASCII 字符。
 * @param x 横坐标。
 * @param y 纵坐标。
 * @param c ASCII 字符。
 * @param foreground 前景 RGB565 颜色。
 * @param background 背景 RGB565 颜色。
 */
void FkLcdChar(uint16_t x, uint16_t y, uint8_t c, uint16_t foreground, uint16_t background) { LCD_ShowChar(x, y, c, foreground, background); }
/**
 * @brief 查询已验证编号的物理按键电平。
 * @param key 按键编号。
 * @return 按下为一，否则为零。
 */
int FkKeyPressed(unsigned key) { return Key_GetVoltage((KeyEnum_T)key) == KEY_VOLATGE_PRESSED; }
/**
 * @brief 消费已验证编号按键的消抖单击事件。
 * @param key 按键编号。
 * @return 有单击为一，否则为零。
 */
int FkKeyClicked(unsigned key)
{
    uint32_t irq = __get_PRIMASK();
    __disable_irq();
    int clicked = Key_IsClicked((KeyEnum_T)key);
    __set_PRIMASK(irq);
    return clicked;
}
/**
 * @brief 消费 MODE 或 ENTER 的有效退出事件，并独立消抖实体按下电平。
 * @return 存在退出请求为一，否则为零。
 */
int FkExitRequested(void)
{
    FkPollCommands();
    int requested = 0;
    const uint32_t now = HAL_GetTick();
    const KeyEvent_T actions = KEY_EVENT_CLICK | KEY_EVENT_DOUBLE_CLICK | KEY_EVENT_LONG_PRESS | KEY_EVENT_REPEAT;
    for (unsigned key = 0; key < 2; ++key)
    {
        uint32_t irq = __get_PRIMASK();
        __disable_irq();
        if (Key_HasEvent((KeyEnum_T)key, actions))
        {
            Key_ClearEvent((KeyEnum_T)key, KEY_EVENT_ALL);
            requested = 1;
        }
        __set_PRIMASK(irq);
        if (!FkKeyPressed(key))
            exitDown[key] = 0;
        else if (!exitDown[key])
        {
            exitDown[key] = 1;
            exitPressTime[key] = now;
        }
        else if ((uint32_t)(now - exitPressTime[key]) >= 30u)
            requested = 1;
    }
    return requested;
}
/**
 * @brief 清除所有按键状态，并重新建立菜单释放屏障。
 */
void FkResetKeys(void)
{
    uint32_t irq = __get_PRIMASK();
    __disable_irq();
    Key_ClearAll();
    __set_PRIMASK(irq);
    memset(exitDown, 0, sizeof(exitDown));
    menuReleasing = menuReady = 0;
}
/**
 * @brief 全部实体按键释放三十毫秒后启用菜单，清理退出键产生的后续事件。
 * @return 菜单可响应新按键时为一，否则为零。
 */
int FkMenuReady(void)
{
    if (menuReady) return 1;
    for (unsigned key = 0; key < 4; ++key)
        if (FkKeyPressed(key))
        {
            menuReleasing = 0;
            return 0;
        }
    uint32_t now = HAL_GetTick();
    if (!menuReleasing)
    {
        menuReleasing = 1;
        menuReleaseTime = now;
    }
    if ((uint32_t)(now - menuReleaseTime) < 30u) return 0;
    FkResetKeys();
    menuReady = 1;
    return 1;
}
/**
 * @brief 在中断可运行的情况下等待指定毫秒数。
 * @param milliseconds 毫秒数。
 */
void FkDelay(uint32_t milliseconds)
{
    FkPollCommands();
    while (milliseconds)
    {
        uint32_t part = milliseconds > 10u ? 10u : milliseconds;
        HAL_Delay(part);
        milliseconds -= part;
        FkPollCommands();
    }
}
/**
 * @brief 读取内核 DWT 周期计数器。
 * @return 当前 32 位计数值。
 */
uint32_t FkCycles(void) { return DWT->CYCCNT; }
/**
 * @brief 读取 TIM6 毫秒时基。
 * @return 启动以来的毫秒计数。
 */
uint32_t FkMillis(void) { return HAL_GetTick(); }
/**
 * @brief 读取实际配置的内核时钟。
 * @return 时钟频率，单位 Hz。
 */
uint32_t FkClockHz(void) { return SystemCoreClock; }
/**
 * @brief 从板级外部 SDRAM 堆申请 VM 保留区。
 * @param bytes 容量字节数。
 * @return 失败为空，否则为对齐的外部 RAM 指针。
 */
void *FkSdramAllocate(uint32_t bytes) { return Memory_Malloc(MP_RAM_EX, bytes); }
/**
 * @brief 把对齐的测试快照写回 RAM，使 SWD 读取不受 DCache 脏行影响。
 * @param memory 按 32 字节对齐的快照地址。
 * @param bytes 按 32 字节取整的容量。
 */
void FkPublish(const volatile void *memory, uint32_t bytes)
{
    __DMB();
    SCB_CleanDCache_by_Addr((uint32_t *)memory, bytes);
    __DSB();
}
