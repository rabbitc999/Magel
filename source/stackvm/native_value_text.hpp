/**
 * @file native_value_text.hpp
 * @brief VM 内部 native value text 模块的类型与接口。
 */
#pragma once
#include "runtime_objects.hpp"

namespace stackvm::runtimeDetail
{
/**
 * @brief 根据运行时文本布局将标量或聚合转换为字符串。
 * @param ctx 当前原生执行环境。
 * @param args 值、类型及布局参数。
 * @return 新字符串在 VM 内存中的地址。
 */
Slot valueToString(NativeContext &ctx, const Memory::Vector<Slot> &args);
}  // namespace stackvm::runtimeDetail
