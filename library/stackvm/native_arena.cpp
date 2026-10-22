/**
 * @file native_arena.cpp
 * @brief arena 分配统计及看门狗配置服务。
 */
#include "runtime_internal.hpp"

namespace stackvm
{
using namespace runtimeDetail;
namespace runtimeDetail
{
/**
 * @brief 查询当前存活分配占用的字节数。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @retval 封装为原生值的运行时结果。
 */
Slot memoryLiveBytes(NativeContext &ctx, const Memory::Vector<Slot> &)
{
    return Slot::fromInt(ctx.heap == nullptr ? 0 : static_cast<std::int64_t>(ctx.heap->liveBytes()));
}

/**
 * @brief `__std_watchdog_enable(watermarkBytes) -> int64`：程序侧打开软重启看门狗。
 *
 * 对应 ChaLi 的 `std::watchdog::enable(watermarkBytes)`。默认关闭：不调它就
 * `__std_request_restart()` 只报 `NotEnabled`、不产生重启。水位由宿主每轮量（VM 只记下来）。
 * @param ctx 环境。
 * @param args `[水位字节数]`。
 * @return 0 = 成功；1 = 水位为负（参数非法）。
 */
Slot watchdogEnable(NativeContext &ctx, const Memory::Vector<Slot> &args)
{
    (void)ctx;
    const std::int64_t watermark = args.empty() ? 0 : nativeInt(args[0]);
    if (watermark < 0) return Slot::fromInt(1);
    Memory::restartSignal().enable(static_cast<std::size_t>(watermark));
    return Slot::fromInt(0);
}

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
Slot requestRestart(NativeContext &ctx, const Memory::Vector<Slot> &args)
{
    (void)ctx;
    (void)args;
    return Slot::fromInt(Memory::restartSignal().requestRestart() ? 0 : 1);
}

/**
 * @brief 查询当前存活分配的块数。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @retval 封装为原生值的运行时结果。
 */
Slot memoryLiveBlocks(NativeContext &ctx, const Memory::Vector<Slot> &)
{
    return Slot::fromInt(ctx.heap == nullptr ? 0 : static_cast<std::int64_t>(ctx.heap->liveBlocks()));
}

/**
 * @brief 查询内存分配的峰值字节数。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @retval 封装为原生值的运行时结果。
 */
Slot memoryPeakBytes(NativeContext &ctx, const Memory::Vector<Slot> &)
{
    return Slot::fromInt(ctx.heap == nullptr ? 0 : static_cast<std::int64_t>(ctx.heap->peakBytes()));
}

/**
 * @brief 创建运行时 arena 对象。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @retval 封装为原生值的运行时结果。
 */
Slot arenaCreate(NativeContext &ctx, const Memory::Vector<Slot> &)
{
    return nativeNewObject(ctx, ObjectKind::Arena);
}

/**
 * @brief 从指定 arena 分配运行时内存。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
Slot arenaAllocate(NativeContext &ctx, const Memory::Vector<Slot> &args)
{
    RuntimeObject *arena = nativeObject(ctx, args[0], ObjectKind::Arena, "std::lang::Arena.allocate");
    if (arena == nullptr || ctx.heap == nullptr) return Slot::fromInt(0);
    const std::int64_t requested = nativeInt(args[1]);
    if (requested < 0)
    {
        ctx.error = "std::lang::Arena.allocate 的字节数不能为负";
        return Slot::fromInt(0);
    }
    if (requested == 0) return Slot::fromInt(0);
    const std::size_t address = nativeAllocate(ctx, static_cast<std::size_t>(requested));
    if (address == 0u)
    {
        ctx.error = "std::lang::Arena.allocate 分配失败";
        return Slot::fromInt(0);
    }
    arena->arenaBlocks.push_back(address);
    return Slot::fromInt(static_cast<std::int64_t>(address));
}

/**
 * @brief 释放 arena 管理的全部分配并重置状态。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param arena 待操作的运行时 arena。
 * @retval 满足检查条件或操作完成时为 true，检查失败或无法完成时为 false。
 */
bool resetArena(NativeContext &ctx, RuntimeObject *arena)
{
    if (arena == nullptr || ctx.heap == nullptr) return false;
    for (const std::size_t address : arena->arenaBlocks)
    {
        Memory::String ignored;
        ctx.heap->release(address, &ignored);
    }
    arena->arenaBlocks.clear();
    return true;
}

/**
 * @brief 重置原生调用参数指定的 arena。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
Slot arenaReset(NativeContext &ctx, const Memory::Vector<Slot> &args)
{
    RuntimeObject *arena = nativeObject(ctx, args[0], ObjectKind::Arena, "std::lang::Arena.reset");
    resetArena(ctx, arena);
    return Slot::fromInt(0);
}

/**
 * @brief 销毁 arena 及其持有的分配。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
Slot arenaDrop(NativeContext &ctx, const Memory::Vector<Slot> &args)
{
    if (ctx.objects == nullptr) return Slot::fromInt(0);
    const std::int64_t handle = nativeInt(args[0]);
    RuntimeObject *arena = ctx.objects->find(handle);
    if (arena == nullptr) return Slot::fromInt(0); /* 值副本重复析构时保持幂等。 */
    if (arena->kind != ObjectKind::Arena)
    {
        ctx.error = "std::lang::Arena.drop 收到了错误种类的对象句柄";
        return Slot::fromInt(0);
    }
    resetArena(ctx, arena);
    ctx.objects->release(handle);
    return Slot::fromInt(0);
}

/* ---- 装箱格子（obj）----
 *
 * 盒子就是一个"值 + 类型号"：类型号由**前端在编译期**算好（`nameof(类型)` 给的也是它），
 * 所以 `obj.name()` 可以和 `nameof(...)` 直接比 —— 泛型分支就是这么写的。
 *
 * 它同时是**接口指针的运行时表示**：接口指针（`Shape mut*`）是"胖指针"，除了对象的
 * 地址还带着那个对象的具体类型号，调 `q.area()` 时正是靠它挑到具体实现的。
 */
}  // namespace runtimeDetail
}  // namespace stackvm
