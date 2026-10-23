/**
 * @file native_arena.hpp
 * @brief VM 内部 native arena 模块的类型与接口。
 */
#pragma once
#include "runtime_objects.hpp"
#include "value_transfer.hpp"

namespace stackvm::runtimeDetail
{
/**
 * @brief 查询当前存活分配占用的字节数。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @retval 封装为原生值的运行时结果。
 */
Slot memoryLiveBytes(NativeContext &ctx, const Memory::Vector<Slot> &);

/**
 * @brief `__std_watchdog_enable(watermarkBytes) -> int64`：程序侧打开软重启看门狗。
 *
 * 对应 ChaLi 的 `std::watchdog::enable(watermarkBytes)`。默认关闭：不调它就
 * `__std_request_restart()` 只报 `NotEnabled`、不产生重启。水位由宿主每轮量（VM 只记下来）。
 * @param ctx 环境。
 * @param args `[水位字节数]`。
 * @return 0 = 成功；1 = 水位为负（参数非法）。
 */
Slot watchdogEnable(NativeContext &ctx, const Memory::Vector<Slot> &args);

/**
 * @brief `__std_request_restart() -> int64`：程序显式请求软重启（写标志 + 原因）。
 *
 * 对应 ChaLi 的 `std::watchdog::requestRestart()`。**未 `enable()` 时不产生重启**，
 * 返回 1（`NotEnabled`）；已开启则写 `WatchdogReason::ExplicitRequest`，VM 在下一条
 * 指令边界（安全点）以"可恢复终止"结束 `run()`。
 * @param ctx 环境。
 * @param args 无。
 * @return 0 = 已记录；1 = 没有 `enable()` 过。
 */
Slot requestRestart(NativeContext &ctx, const Memory::Vector<Slot> &args);

/**
 * @brief 查询当前存活分配的块数。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @retval 封装为原生值的运行时结果。
 */
Slot memoryLiveBlocks(NativeContext &ctx, const Memory::Vector<Slot> &);

/**
 * @brief 查询内存分配的峰值字节数。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @retval 封装为原生值的运行时结果。
 */
Slot memoryPeakBytes(NativeContext &ctx, const Memory::Vector<Slot> &);

/**
 * @brief 创建运行时 arena 对象。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @retval 封装为原生值的运行时结果。
 */
Slot arenaCreate(NativeContext &ctx, const Memory::Vector<Slot> &);

/**
 * @brief 从指定 arena 分配运行时内存。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
Slot arenaAllocate(NativeContext &ctx, const Memory::Vector<Slot> &args);

/**
 * @brief 释放 arena 管理的全部分配并重置状态。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param arena 待操作的运行时 arena。
 * @retval 满足检查条件或操作完成时为 true，检查失败或无法完成时为 false。
 */
bool resetArena(NativeContext &ctx, RuntimeObject *arena);

/**
 * @brief 重置原生调用参数指定的 arena。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
Slot arenaReset(NativeContext &ctx, const Memory::Vector<Slot> &args);

/**
 * @brief 销毁 arena 及其持有的分配。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
Slot arenaDrop(NativeContext &ctx, const Memory::Vector<Slot> &args);
}  // namespace stackvm::runtimeDetail
