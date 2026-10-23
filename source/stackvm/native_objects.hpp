/**
 * @file native_objects.hpp
 * @brief VM 内部 native objects 模块的类型与接口。
 */
#pragma once
#include "runtime_objects.hpp"
#include "value_transfer.hpp"

namespace stackvm::runtimeDetail
{
/**
 * @brief `__std_obj_new() -> obj`：造一个空格子（类型号 0 = 还没 set 过）。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @retval 封装为原生值的运行时结果。
 */
Slot objNew(NativeContext &ctx, const Memory::Vector<Slot> &);

/**
 * @brief `__std_obj_set(obj, 值, 类型号, 值是不是字符串, 值是不是对象句柄, 类型名地址) -> 0`：把值装进去。
 *
 * 原来的值直接丢掉（格子只有一个），类型号与类型名一起换成新的。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
Slot objSet(NativeContext &ctx, const Memory::Vector<Slot> &args);

/**
 * @brief `obj.get` / `obj.get` 取指针 共用的**类型校验**。
 *
 * 前端把"这个位置上允许出现的类型号"全部算好跟在后面（`args[4..]`）：
 * 目标是具体类型时就是它自己，目标是接口指针时是"所有实现了这个接口的结构体指针"。
 * 对不上就写 `ctx.error`（`run()` 会转成运行期错误）。
 *
 * @param ctx 环境。
 * @param object 盒子。
 * @param args 实参：`[1]` 要取的类型的名字地址、`[2]`/`[3]`/`[4]` 形状标志、`[5..]` 允许的类型号。
 * @param who 谁在取（诊断文案）。
 * @return 允许返回 true。
 */
bool objCheckTag(NativeContext &ctx, const RuntimeObject *object, const Memory::Vector<Slot> &args,
                 const char *who);

/**
 * @brief `__std_obj_get(obj, 要取的类型的名字地址, 是不是字符串, 是不是浮点, 是不是对象句柄, 允许的类型号…) -> 值`。
 *
 * 取出来的是**裸值**（整数 / 浮点 / 堆上的新串 / 指针的地址）。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
Slot objGet(NativeContext &ctx, const Memory::Vector<Slot> &args);

/**
 * @brief 判断对象是否带有指定类型标记。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
Slot objHasTag(NativeContext &ctx, const Memory::Vector<Slot> &args);

/**
 * @brief `__std_obj_get_ptr(obj, 要取的类型的名字地址, 0, 0, 1, 允许的类型号…) -> 新的 obj（胖指针）`。
 *
 * 接口指针走这一条：校验过关后**复制一份胖指针**（值语义），交给变量持有 ——
 * 于是"两个接口指针"各拿各的类型号，谁先出块被释放都不会把对方弄悬空。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
Slot objGetPointer(NativeContext &ctx, const Memory::Vector<Slot> &args);

/**
 * @brief `__std_obj_name(obj) -> 类型号`：和 `nameof(类型)` 是同一个编号（空格子给 0）。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
Slot objName(NativeContext &ctx, const Memory::Vector<Slot> &args);

/**
 * @brief 验证 CHALI 引用非空，阻止未绑定存储暴露为有效引用。
 * @param ctx 运行上下文。
 * @param args 单个引用地址或接口句柄。
 * @return 原引用；零地址时设置运行错误。
 */
Slot requireReference(NativeContext &ctx, const Memory::Vector<Slot> &args);

/**
 * @brief `__std_obj_addr(obj) -> 地址`：胖指针里那个对象的地址（接口分派把它当 `self` 传下去）。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
Slot objAddress(NativeContext &ctx, const Memory::Vector<Slot> &args);

/**
 * @brief `__std_iface_fail(类型号, 接口.方法 名地址) -> 0`：`q.area()` 分派不到实现时的报错出口。
 *
 * 两种情形都落到这里：接口指针是空的（类型号 0），或者它指着的对象没实现这个接口。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
Slot ifaceFail(NativeContext &ctx, const Memory::Vector<Slot> &args);
}  // namespace stackvm::runtimeDetail
