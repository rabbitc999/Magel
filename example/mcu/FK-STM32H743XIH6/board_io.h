#pragma once
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/**
 * @brief 同步发送 USART1 调试字节。
 * @param bytes 待发送内容。
 * @param count 字节数。
 */
void FkSerialWrite(const void *bytes, uint16_t count);
/**
 * @brief 从 USART1 的接收 FIFO 读取当前可用字节。
 * @param buffer 接收缓冲。
 * @param length 缓冲容量。
 * @return 实际读取长度。
 */
unsigned FkSerialRead(unsigned char *buffer, unsigned length);
/**
 * @brief 处理 USART1 虚拟按键命令，并为 VM 保留其他串口输入。
 * @note 仅从前台调用；不在性能计时内核中调用。
 */
void FkPollCommands(void);
/**
 * @brief 消费 MODE 或 ENTER 的退出事件，或检测持续三十毫秒的实体按下。
 * @return 请求退出为一，否则为零。
 */
int FkExitRequested(void);
/**
 * @brief 清除全部按键事件和退出消抖状态，并等待实体按键释放后启用菜单。
 */
void FkResetKeys(void);
/**
 * @brief 等待全部实体按键连续释放三十毫秒，避免退出操作再次启动程序。
 * @return 菜单可以接收新按键时为一，否则为零。
 */
int FkMenuReady(void);
/**
 * @brief 读取 LCD 的水平像素数。
 * @return 屏幕宽度。
 */
uint16_t FkLcdWidth(void);
/**
 * @brief 读取 LCD 的垂直像素数。
 * @return 屏幕高度。
 */
uint16_t FkLcdHeight(void);
/**
 * @brief 通过 BSP 填充已检查边界的 LCD 矩形。
 * @param x 横坐标。
 * @param y 纵坐标。
 * @param width 宽度。
 * @param height 高度。
 * @param color RGB565 颜色。
 */
void FkLcdFill(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint16_t color);
/**
 * @brief 通过 BSP 绘制固定大小的 ASCII 字符。
 * @param x 横坐标。
 * @param y 纵坐标。
 * @param c ASCII 字符。
 * @param foreground 前景 RGB565 颜色。
 * @param background 背景 RGB565 颜色。
 */
void FkLcdChar(uint16_t x, uint16_t y, uint8_t c, uint16_t foreground, uint16_t background);
/**
 * @brief 查询已验证编号的物理按键电平。
 * @param key 按键编号。
 * @return 按下为一，否则为零。
 */
int FkKeyPressed(unsigned key);
/**
 * @brief 消费已验证编号按键的消抖单击事件。
 * @param key 按键编号。
 * @return 有单击为一，否则为零。
 */
int FkKeyClicked(unsigned key);
/**
 * @brief 在中断可运行的情况下等待指定毫秒数。
 * @param milliseconds 毫秒数。
 */
void FkDelay(uint32_t milliseconds);
/**
 * @brief 读取内核 DWT 周期计数器。
 * @return 当前 32 位计数值。
 */
uint32_t FkCycles(void);
/**
 * @brief 读取 TIM6 毫秒时基。
 * @return 启动以来的毫秒计数。
 */
uint32_t FkMillis(void);
/**
 * @brief 读取实际配置的内核时钟。
 * @return 时钟频率，单位 Hz。
 */
uint32_t FkClockHz(void);
/**
 * @brief 从板级外部 SDRAM 堆申请 VM 保留区。
 * @param bytes 容量字节数。
 * @return 失败为空，否则为对齐的外部 RAM 指针。
 */
void *FkSdramAllocate(uint32_t bytes);
/**
 * @brief 把对齐的测试快照写回 RAM，使 SWD 读取不受 DCache 脏行影响。
 * @param memory 按 32 字节对齐的快照地址。
 * @param bytes 按 32 字节取整的容量。
 */
void FkPublish(const volatile void *memory, uint32_t bytes);
#ifdef __cplusplus
}
#endif
