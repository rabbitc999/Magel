#ifndef PARSE_ENGINE_MCU_VM_CONSOLE_HPP
#define PARSE_ENGINE_MCU_VM_CONSOLE_HPP
/**
 * @file mcu_console.hpp
 * @brief 板端输入接入层：把"取一项程序输入"接到串口（或你自己那套控制台）。
 *
 * 与 `mcuFilesystem` / `mcuArenas` 一个风格：**注入一个读函数**，VM 不假设任何外设：
 *
 * ```cpp
 * mcuConsole::SetRead(MyUartRead, &huart1);   // 例：HAL_UART_Receive(&huart1, buffer, length, 0)
 * ```
 *
 * 语义跟桌面的 `--input` 一致：宿主给的输入项先用，用完了再从读函数按**空白切分**取一项
 * （`std::lang::inputLine()` / `inputInt()` 等共用同一个序号）。读函数返回 0 表示"暂时没数据 /
 * 到结尾"——原生函数照旧报"输入不够（第 N 项缺失）"，所以板端遇到串口超时时直接返回 0
 * 就行，不必阻塞等待。
 */

namespace mcuConsole
{
/**
 * @brief 读一段字节：返回实际读到的字节数，0 表示当前没有数据。
 */
using ReadFn = unsigned (*)(void *user, unsigned char *buffer, unsigned length);

/**
 * @brief 当前注入的读函数（`nullptr` = 没有输入源）。
 */
inline ReadFn g_read = nullptr;

/**
 * @brief 传给读函数的用户数据（板端一般放 UART 句柄）。
 */
inline void *g_user = nullptr;

/**
 * @brief 注入读函数。
 * @param reader 读函数；传 `nullptr` 恢复"没有输入源"。
 * @param user 传给读函数的用户数据。
 */
inline void SetRead(ReadFn reader, void *user)
{
    g_read = reader;
    g_user = user;
}

/**
 * @brief 有没有输入源。
 * @return 注入过读函数返回 true。
 */
inline bool HasInput() { return g_read != nullptr; }

/**
 * @brief 读一个字节。
 * @return 0..255 是读到的字节；-1 表示没有输入源或暂时没数据。
 */
inline int ReadByte()
{
    if (g_read == nullptr)
    {
        return -1;
    }
    unsigned char byte = 0;
    return g_read(g_user, &byte, 1u) == 1u ? static_cast<int>(byte) : -1;
}
}  // namespace mcuConsole

#endif
