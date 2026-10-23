/**
 * @file native_control.cpp
 * @brief 程序退出、断言、panic 与看门狗截止时间。
 */
#include "runtime_internal.hpp"

namespace stackvm
{
using namespace runtimeDetail;
namespace runtimeDetail
{
/**
 * @brief 设置程序退出码并终止当前执行。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
Slot programExit(NativeContext &ctx, const Memory::Vector<Slot> &args)
{
    if (ctx.exitRequested != nullptr) *ctx.exitRequested = true;
    if (ctx.exitCode != nullptr) *ctx.exitCode = static_cast<int>(nativeInt(args[0]));
    return Slot::fromInt(0);
}

/**
 * @brief 校验运行时断言，失败时报告错误。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
Slot programAssert(NativeContext &ctx, const Memory::Vector<Slot> &args)
{
    if (nativeInt(args[0]) == 0) ctx.error = "断言失败：assert(x) 的 x 为假";
    return Slot::fromInt(0);
}

/**
 * @brief 触发程序 panic 并保存诊断信息。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
Slot programPanic(NativeContext &ctx, const Memory::Vector<Slot> &args)
{
    const std::size_t address = static_cast<std::size_t>(nativeInt(args[0]));
    Memory::String message;
    if (!nativeReadCString(ctx, address, &message))
    {
        ctx.error = "panic 读不到错误信息字符串（地址 " + Memory::toString(address) + "）";
        return Slot::fromInt(0);
    }
    ctx.error = "panic：" + message;
    return Slot::fromInt(0);
}

/**
 * @brief 配置当前执行上下文的看门狗截止时间，不登记用户回调。
 * @param ctx 本次执行的原生环境。
 * @param args 正超时单位，零表示关闭超时。
 * @return 配置成功返回零，无效参数返回一。
 */
Slot watchdogTimeout(NativeContext &ctx, const Memory::Vector<Slot> &args)
{
    const auto period = nativeInt(args[0]);
    if (!ctx.watchdog || period < 0) return Slot::fromInt(1);
    ctx.watchdog->period = static_cast<std::uint64_t>(period);
    ctx.watchdog->deadline = 0;
    return Slot::fromInt(0);
}
/**
 * @brief 推迟当前执行的内部看门狗截止时间。
 * @param ctx 当前看门狗状态。
 * @param args 无实参。
 * @return 已启用超时返回零，否则返回一。
 */
Slot watchdogKick(NativeContext &ctx, const Memory::Vector<Slot> &args)
{
    (void)args;
    if (!ctx.watchdog || ctx.watchdog->period == 0) return Slot::fromInt(1);
    ctx.watchdog->deadline = ctx.watchdog->period > std::numeric_limits<std::uint64_t>::max() - ctx.watchdog->now ? std::numeric_limits<std::uint64_t>::max() : ctx.watchdog->now + ctx.watchdog->period;
    return Slot::fromInt(0);
}
}  // namespace runtimeDetail
}  // namespace stackvm
