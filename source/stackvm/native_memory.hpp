/**
 * @file native_memory.hpp
 * @brief VM 内部 native memory 模块的类型与接口。
 */
#pragma once
#include "runtime_objects.hpp"
#include "value_transfer.hpp"

namespace stackvm::runtimeDetail
{
/**
 * @brief 造一个对象并返回句柄。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param kind 待处理对象的类别。
 * @retval 封装为原生值的运行时结果。
 */
Slot nativeNewObject(NativeContext &ctx, ObjectKind kind);

/* ---- 容器（vec / set / map）---- */

/**
 * @brief 按一个字节读取 char 引用，适用于字符串字节及八字节标量槽。
 */
Slot charLoad(NativeContext &ctx, const Memory::Vector<Slot> &args);

/**
 * @brief 按一个字节写入 char 引用，避免覆盖相邻字符。
 */
Slot charStore(NativeContext &ctx, const Memory::Vector<Slot> &args);

/**
 * @brief 复制结构体状态到拥有型箱，接口借用共享稳定单元，值复制另建状态。
 * @param ctx 运行上下文。
 * @param args 状态地址、叶子布局、引用类型号和类型名。
 * @return 新状态箱句柄；失败记录运行错误。
 */
Slot iterBox(NativeContext &ctx, const Memory::Vector<Slot> &args);

/**
 * @brief 分配内存后复位地址复用的生命周期标记。
 * @param ctx 环境。
 * @param bytes 字节数。
 * @return 分配地址。
 */
std::size_t nativeAllocate(NativeContext &ctx, std::size_t bytes);

/**
 * @brief 查询逻辑分配是否有效，供编译器清理守卫避免读取失效载荷。
 * @param ctx 当前堆。
 * @param args 块首地址。
 * @return 活跃分配返回一。
 */
Slot heapLive(NativeContext &ctx, const Memory::Vector<Slot> &args);

/**
 * @brief 显式删除前严格检查块首，阻止对失效身份再次析构。
 * @param ctx 当前堆。
 * @param args 块首。
 * @return 活跃时为一；失效时记录诊断并返回零。
 */
Slot heapDropLive(NativeContext &ctx, const Memory::Vector<Slot> &args);

/**
 * @brief 查询、结束或重新开始 move 管理的存储生命周期。
 * @param ctx 本次运行的对象表；标记不跨虚拟机实例共享。
 * @param args 存储地址、字节数和子对象类型号（开始生命周期只需前两项）。
 * @return 查询返回存活标志；重复移动报告运行错误。
 */
template <int Operation>
Slot moveLifetime(NativeContext &ctx, const Memory::Vector<Slot> &args);
}  // namespace stackvm::runtimeDetail
