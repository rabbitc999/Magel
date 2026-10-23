/**
 * @file native_closure.hpp
 * @brief VM 内部 native closure 模块的类型与接口。
 */
#pragma once
#include "runtime_objects.hpp"
#include "value_transfer.hpp"

namespace stackvm::runtimeDetail
{
/**
 * @brief 解析闭包或旧式无环境函数地址。
 * @param objects 对象表。
 * @param pointer 函数值。
 * @param environment 返回需要隐式传入的环境句柄。
 * @return 标签地址，非法闭包返回零。
 */
std::int64_t closureTarget(ObjectStore &objects, std::int64_t pointer, std::int64_t &environment);

/**
 * @brief 建立函数值；捕获环境由后续操作逐项登记借用地址。
 */
Slot closureNew(NativeContext &ctx, const Memory::Vector<Slot> &args);

/**
 * @brief 登记已校验的捕获项；借用捕获以非拥有整数地址保存。
 */
Slot closureCapture(NativeContext &ctx, const Memory::Vector<Slot> &args);

/**
 * @brief 登记独立的值捕获。
 * @param ctx 原生上下文。
 * @param args 环境、值与存储布局。
 * @return 环境句柄。
 */
Slot closureCaptureCopy(NativeContext &ctx, const Memory::Vector<Slot> &args);

/**
 * @brief 转移源存储到闭包，清空源槽而不复制资源。
 * @param ctx 上下文。
 * @param args 环境、地址、布局、浮点标志和字节数。
 * @return 环境句柄。
 */
Slot closureCaptureMove(NativeContext &ctx, const Memory::Vector<Slot> &args);

/**
 * @brief 登记聚合捕获的用户析构入口。
 * @param ctx 上下文。
 * @param args 环境、索引和函数地址。
 * @return 环境句柄。
 */
Slot closureDestructor(NativeContext &ctx, const Memory::Vector<Slot> &args);

/**
 * @brief 取得 copy 捕获的可写稳定单元。
 * @param ctx 原生上下文。
 * @param args 环境、索引及布局。
 * @return 捕获副本的地址。
 */
Slot closureRef(NativeContext &ctx, const Memory::Vector<Slot> &args);

/**
 * @brief 取出捕获项；借用地址由来源所有者管理，不在调用块释放。
 */
Slot closureGet(NativeContext &ctx, const Memory::Vector<Slot> &args);

/**
 * @brief 为运行时对象创建独立副本。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
Slot objectCopy(NativeContext &ctx, const Memory::Vector<Slot> &args);

/**
 * @brief 静态函数地址无需复制；带捕获函数值深拷贝环境。
 */
Slot functionCopy(NativeContext &ctx, const Memory::Vector<Slot> &args);

/**
 * @brief `__std_object_free(句柄) -> 0`：把对象还回去（`delete` 用；不是句柄就报错）。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
Slot objectFree(NativeContext &ctx, const Memory::Vector<Slot> &args);
}  // namespace stackvm::runtimeDetail
