/**
 * @file instruction_select.hpp
 * @brief 指令选择：把栈指令窗口按模式表压成一条目标指令。
 *
 * 目标指令仍然是"带原始 PC 跨度"的内部指令（`execution_detail::RegisterInstruction`），
 * 所以执行器可以用它一次分派完成整个窗口，同时把 PC、步数与源码位置原样保留——
 * 出错时的报错位置与逐步观察看到的仍是原始栈指令。
 *
 * 选择只做一件事：**证明这段窗口等价于一条指令**，模式表里的每条规则都对应
 * "语义完全相同、求值栈净变化为零"的窗口。任何不能证明的窗口都不选，交回逐条执行，
 * 因此选择结果只会变快，不会改变语义。
 */
#pragma once
#include "program_model.hpp"

namespace stackvm
{
namespace selection
{
/**
 * @brief 指令选择的统计，供 `--stats` 与回归断言使用。
 */
using Stats = execution_detail::SelectionStats;

/**
 * @brief 按模式表选择目标指令。
 * @param program 已组装的栈程序。
 * @param stats 可选的统计输出。
 * @return 目标指令序列，按起始 PC 递增。
 * @note 纯函数：同一程序永远得到同一序列。
 */
[[nodiscard]] Memory::Vector<execution_detail::RegisterInstruction> lower(const Program &program, Stats *stats = nullptr);

/**
 * @brief 最近一次运行的累计统计。
 * @return 统计。
 */
Stats lastStats();

/**
 * @brief 清零累计统计。
 */
void resetStats();

/**
 * @brief 把统计打印成稳定的一行文本。
 * @param stats 统计。
 * @return 文本，不含换行。
 */
[[nodiscard]] Memory::String describe(const Stats &stats);
}  // namespace selection
}  // namespace stackvm
