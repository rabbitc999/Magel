/**
 * @file native_input.hpp
 * @brief VM 内部 native input 模块的类型与接口。
 */
#pragma once
#include "runtime_objects.hpp"
#include "value_transfer.hpp"

namespace stackvm::runtimeDetail
{
/**
 * @brief `CHALI_NO_STDIN_INPUT=1` 时不做标准输入兜底。
 *
 * 门禁（`check.sh` 等）靠它保证确定性：样例缺输入时必须照旧报"输入不够"，
 * 而不是停下来等标准输入。
 * @return 关掉兜底返回 true。
 */
bool stdinInputDisabled();

/**
 * @brief 取下一项输入：先用 `--input "a b c"` 给的项，用完了就**从标准输入读一项**。
 *
 * 于是调试时可以直接敲，也可以 `echo "abc 3" | chali run x.chl` 喂进去；两项之间按空白
 * 切分，跟 `--input` 一个口径。读到 EOF 返回 false，调用方照旧报"输入不够"。
 *
 * @param ctx 原生调用上下文。
 * @param out 输出：读到的这一项。
 * @return 取到返回 true。
 */
/**
 * @brief 从当前平台输入源取一个字节。
 * @return 字节或 EOF。
 */
int nativeInputByte();

/**
 * @brief 从输入流提取下一个文本字段。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param out 接收处理结果的输出对象。
 * @retval 满足检查条件或操作完成时为 true，检查失败或无法完成时为 false。
 */
bool nextInputText(NativeContext &ctx, Memory::String *out);

/**
 * @brief 读取原生输入所需的文本数据。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @retval 封装为原生值的运行时结果。
 */
Slot readText(NativeContext &ctx, const Memory::Vector<Slot> &);

/**
 * @brief 从宿主输入读取完整行，保留空格；显式输入项各视作一行。
 * @param ctx 调用上下文。
 * @param line 输出行，不含 LF/CRLF。
 * @return 是否读到一行，EOF 返回 false。
 */
bool nextInputLine(NativeContext &ctx, Memory::String &line);

/**
 * @brief 读取一行标准输入，EOF 返回空串。
 * @param ctx 调用上下文。
 * @return 新字符串槽。
 */
Slot inputReadLine(NativeContext &ctx, const Memory::Vector<Slot> &);

/**
 * @brief 最多读取指定行数，以 LF 连接，EOF 提前结束。
 * @param ctx 上下文。
 * @param args 非负行数。
 * @return 新字符串槽；负数或零字节报告运行错误。
 */
Slot inputReadLines(NativeContext &ctx, const Memory::Vector<Slot> &args);

/**
 * @brief 标准输入是不是交互式终端——决定 `__std_pause` 要不要真的停下来等。
 * @retval 满足检查条件或操作完成时为 true，检查失败或无法完成时为 false。
 */
bool standardInputIsInteractive();

/**
 * @brief `__std_pause() -> 0`：调试用"临时停一下"，等到一行输入再继续（`std::lang::pause()` 降到它）。
 *
 * **只在交互式终端里真的停**：标准输入不是终端（重定向、管道、CI、嵌入式宿主）时立刻返回，
 * 所以回归样例和自动化跑批不会被它挂住。停下时提示走 stderr，不污染程序的 stdout；
 * 读到的内容直接丢弃（它是"按回车继续"，不是数据输入——要数据请用 `std::lang::inputLine`）。
 * @retval 封装为原生值的运行时结果。
 */
Slot pauseForDebug(NativeContext &, const Memory::Vector<Slot> &);

/**
 * @brief 取得下一段可供类型转换的输入文本。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param name 名称文本。
 * @param value 待转换、保存或参与计算的值。
 * @retval 满足检查条件或操作完成时为 true，检查失败或无法完成时为 false。
 */
bool takeInput(NativeContext &ctx, const char *name, Memory::String *value);

/**
 * @brief 读取输入并转换为有符号整数。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @retval 封装为原生值的运行时结果。
 */
Slot inputInt(NativeContext &ctx, const Memory::Vector<Slot> &);

/**
 * @brief 读取输入并转换为无符号整数。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @retval 封装为原生值的运行时结果。
 */
Slot inputUInt(NativeContext &ctx, const Memory::Vector<Slot> &);

/**
 * @brief 读取输入并转换为浮点数。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @retval 封装为原生值的运行时结果。
 */
Slot inputFloat(NativeContext &ctx, const Memory::Vector<Slot> &);

/**
 * @brief 读取输入并转换为布尔值。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @retval 封装为原生值的运行时结果。
 */
Slot inputBool(NativeContext &ctx, const Memory::Vector<Slot> &);

/**
 * @brief 从输入中读取一个字符。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @retval 封装为原生值的运行时结果。
 */
Slot inputChar(NativeContext &ctx, const Memory::Vector<Slot> &);
}  // namespace stackvm::runtimeDetail
