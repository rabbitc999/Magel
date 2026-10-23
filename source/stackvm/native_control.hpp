/**
 * @file native_control.hpp
 * @brief VM 内部 native control 模块的类型与接口。
 */
#pragma once
#include "runtime_objects.hpp"
#include "value_transfer.hpp"

namespace stackvm::runtimeDetail
{
/**
 * @brief 设置程序退出码并终止当前执行。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
Slot programExit(NativeContext &ctx, const Memory::Vector<Slot> &args);

/**
 * @brief 校验运行时断言，失败时报告错误。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
Slot programAssert(NativeContext &ctx, const Memory::Vector<Slot> &args);

/**
 * @brief 触发程序 panic 并保存诊断信息。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
Slot programPanic(NativeContext &ctx, const Memory::Vector<Slot> &args);

/**
 * @brief 配置当前执行上下文的看门狗截止时间，不登记用户回调。
 * @param ctx 本次执行的原生环境。
 * @param args 正超时单位，零表示关闭超时。
 * @return 配置成功返回零，无效参数返回一。
 */
Slot watchdogTimeout(NativeContext &ctx, const Memory::Vector<Slot> &args);

/**
 * @brief 推迟当前执行的内部看门狗截止时间。
 * @param ctx 当前看门狗状态。
 * @param args 无实参。
 * @return 已启用超时返回零，否则返回一。
 */
Slot watchdogKick(NativeContext &ctx, const Memory::Vector<Slot> &args);
}  // namespace stackvm::runtimeDetail
