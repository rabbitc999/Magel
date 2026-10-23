/**
 * @file object_values.hpp
 * @brief VM 内部 object values 模块的类型与接口。
 */
#pragma once
#include "runtime_objects.hpp"
#include "value_transfer.hpp"

namespace stackvm::runtimeDetail
{
/**
 * @brief 根据原生调用参数查找运行时对象。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param handle 待操作的运行时句柄。
 * @param kind 待处理对象的类别。
 * @param who 诊断信息中使用的操作名称。
 * @retval 匹配项的指针，未找到时为空指针。
 */
RuntimeObject *nativeObject(NativeContext &ctx, const Slot &handle, ObjectKind kind,
                            const char *who);

/**
 * @brief 一条实参 → 容器里的值 / 键。
 * @param ctx 环境。
 * @param value 值（数值就是 Slot 本身；字符串是它的地址）。
 * @param isText true = 按字符串读（**读成副本**）。
 * @param isObject true = 这个整数是一份由调用者持有的对象句柄。
 * @param copyObject true = 存入容器 / obj 时复制一份，false = 只用于查找比较。
 * @param out 结果。
 * @return 成功返回 true；字符串读不到时写 `ctx.error` 并返回 false。
 */
bool nativeValue(NativeContext &ctx, const Slot &value, bool isText, std::int64_t objectFlag,
                 bool copyObject, ObjectValue *out);

/**
 * @brief 容器里的值 / 键 → 一个 Slot（字符串会在堆上复制一份新的）。
 * @param ctx 环境。
 * @param value 值。
 * @param out 结果。
 * @return 成功返回 true。
 */
bool nativeSlot(NativeContext &ctx, const ObjectValue &value, Slot *out);

/**
 * @brief 从活跃元素单元读取当前值；聚合单元直接保存摊平后的字段。
 */
bool nativeViewValue(NativeContext &ctx, const RuntimeObject::ElementView &view, ObjectValue *out);

/**
 * @brief 将活跃引用中的当前值同步到列表快照，所有列表读取与值复制共用该入口。
 */
bool nativeRefreshViews(NativeContext &ctx, RuntimeObject &object);

/**
 * @brief 释放元素单元中当前拥有的字符串与对象叶子，不释放单元本身。
 */
void nativeReleaseViewContents(NativeContext &ctx, const RuntimeObject::ElementView &view);

/**
 * @brief 将容器快照复制到引用单元；字符串与对象叶子由单元独占。
 */
bool nativeWriteView(NativeContext &ctx, const RuntimeObject::ElementView &view, const ObjectValue &value);
}  // namespace stackvm::runtimeDetail
