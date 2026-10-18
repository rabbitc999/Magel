#ifndef MAGEL_DAP_SERVER_HPP
#define MAGEL_DAP_SERVER_HPP

/* DAP（Debug Adapter Protocol）调试服务端。
 *
 * VS Code 侧用 stdio 起一个 `magel --dap` 进程，双方按 DAP 收发 JSON。
 * VM 侧本来就带断点与单步（`Machine::setBreakpoints` / `setSingleStep`），
 * 调试回调返回 `DebugAction` 决定下一步，所以"停在断点上等命令"是直接支持的：
 * 每次停下时发 `stopped` 事件，然后**嵌套读**下一条命令再返回动作。
 *
 * 注意：DAP 用 stdout 传协议，所以 DAP 模式下程序自己的输出不能再写 stdout，
 * 要走 `emitOutput()` 转成 `output` 事件，否则协议流会被冲垮。
 */
#include "../../library/stackvm/stackvm.hpp"

#include <functional>
#include <string>

namespace dap
{

/**
 * @brief 挂上即将运行的 VM 与调试表。
 * @param machine 执行用的虚拟机；断点会直接写进它。
 * @param info 组装得到的调试表（指令 → 源码位置）；用于把源码行反查成指令下标。
 */
void attach(stackvm::Machine *machine, const stackvm::DebugInfo *info);

/**
 * @brief VM 停在断点 / 单步时调用。
 * @param event 本次停下的快照（位置、调用栈、是否断点停）。
 * @return 下一步动作：继续跑到下一断点、单步、或中止。
 */
stackvm::DebugAction onStop(const stackvm::DebugEvent &event);

/**
 * @brief DAP 主循环；`launch` 时调用 `compileAndRun`（编译 + 执行）。
 * @param compileAndRun 编译并执行一次，返回编译器退出码。
 * @return 进程退出码。
 */
int serve(const std::function<int()> &compileAndRun);

/**
 * @brief 转发程序输出为 `output` 事件。
 * @param text 程序写到标准输出 / 标准错误的内容。
 * @param category DAP 的 category：`stdout` / `stderr` / `console`。
 */
void emitOutput(const std::string &text, const char *category = "stdout");

/**
 * @brief 程序是否已经跑完（供执行路径判断要不要再停下）。
 */
bool finished();

}  // namespace dap

#endif  // MAGEL_DAP_SERVER_HPP
