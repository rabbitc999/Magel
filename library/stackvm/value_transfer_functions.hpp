/**
 * @file value_transfer_functions.hpp
 * @brief VM 内部 value transfer functions 模块的类型与接口。
 */
#pragma once
#include "runtime_objects.hpp"
#include "value_transfer.hpp"

namespace stackvm::runtimeDetail
{
/**
 * @brief 捕获可跨运行时传递的对象值。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param value 待转换、保存或参与计算的值。
 * @param packet 待处理的 JSON-RPC 消息对象。
 * @param active 当前正在处理的节点集合。
 * @retval 满足检查条件或操作完成时为 true，检查失败或无法完成时为 false。
 */
bool captureTransferValue(NativeContext &ctx, const ObjectValue &value,
                          TransferPacket &packet, Memory::Set<std::int64_t> &active);

/**
 * @brief 捕获运行时对象的可传输快照。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param handle 待操作的运行时句柄。
 * @param packet 待处理的 JSON-RPC 消息对象。
 * @param active 当前正在处理的节点集合。
 * @retval 满足检查条件或操作完成时为 true，检查失败或无法完成时为 false。
 */
bool captureTransferObject(NativeContext &ctx, std::int64_t handle, TransferPacket &packet,
                           Memory::Set<std::int64_t> &active);

/**
 * @brief 将原生参数捕获为可传输的值。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param slot 待转换或传递的虚拟机值槽。
 * @param kind 待处理对象的类别。
 * @param layout 传输值的布局描述。
 * @param type 类型描述。
 * @param out 接收处理结果的输出对象。
 * @retval 满足检查条件或操作完成时为 true，检查失败或无法完成时为 false。
 */
bool captureTransfer(NativeContext &ctx, const Slot &slot, int kind,
                     std::int64_t layout, const Memory::String &type, TransferPacket *out);

/**
 * @brief 在目标运行时恢复传输的对象值。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param packet 待处理的 JSON-RPC 消息对象。
 * @param value 待转换、保存或参与计算的值。
 * @param active 当前正在处理的节点集合。
 * @param created 已恢复对象的句柄映射，避免重复恢复。
 * @retval 满足检查条件或操作完成时为 true，检查失败或无法完成时为 false。
 */
bool restoreTransferValue(NativeContext &ctx, const TransferPacket &packet,
                          ObjectValue *value, Memory::Set<std::int64_t> &active,
                          Memory::Vector<std::int64_t> &created);

/**
 * @brief 在目标运行时恢复传输的对象快照。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param packet 待处理的 JSON-RPC 消息对象。
 * @param oldHandle 原运行时中的对象句柄。
 * @param active 当前正在处理的节点集合。
 * @param created 已恢复对象的句柄映射，避免重复恢复。
 * @param newHandle 目标运行时中恢复的对象句柄。
 * @retval 满足检查条件或操作完成时为 true，检查失败或无法完成时为 false。
 */
bool restoreTransferObject(NativeContext &ctx, const TransferPacket &packet,
                           std::int64_t oldHandle, Memory::Set<std::int64_t> &active,
                           Memory::Vector<std::int64_t> &created, std::int64_t *newHandle);

/**
 * @brief 将传输值恢复为目标运行时的原生值。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param packet 待处理的 JSON-RPC 消息对象。
 * @param out 接收处理结果的输出对象。
 * @retval 满足检查条件或操作完成时为 true，检查失败或无法完成时为 false。
 */
bool restoreTransfer(NativeContext &ctx, const TransferPacket &packet, Slot *out);
}  // namespace stackvm::runtimeDetail
