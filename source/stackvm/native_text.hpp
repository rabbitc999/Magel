/**
 * @file native_text.hpp
 * @brief VM 内部 native text 模块的类型与接口。
 */
#pragma once
#include "runtime_objects.hpp"
#include "value_transfer.hpp"

namespace stackvm::runtimeDetail
{
/**
 * @brief `__std_str_len(地址) -> 长度`。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
Slot strLen(NativeContext &ctx, const Memory::Vector<Slot> &args);

/**
 * `__std_str_sub(串, 起, 止) -> 新串地址`：半开区间 `[起, 止)`，**越界自动夹到范围内**；
 * `止` 传负数表示"到末尾"。
 */
/**
 * @brief 这个字节位置是不是 UTF-8 字符的边界（`String` 的下标与切片都按字节算，但端点落在
 * 多字节字符中间就会切出半个字符——那时报错，而不是静默产出坏字节）。
 * @param text 字符串。
 * @param index 字节位置（等于长度也算合法边界）。
 * @return 是字符边界（或正好在末尾）返回 true。
 */
bool utf8Boundary(const Memory::String &text, std::size_t index);

/**
 * @brief `__std_str_fmt(格式串, 实参…) -> 新串地址`。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
Slot strFormat(NativeContext &ctx, const Memory::Vector<Slot> &args);

/**
 * @brief 输出一段文本：挂了下沉就先实时推给宿主，`keepText=false` 时不再进文本桶。
 * @param ctx 环境。
 * @param bytes 字节。
 * @param count 字节数。
 */
void nativeEmitText(NativeContext &ctx, const char *bytes, std::size_t count);

/**
 * @brief `__std_print(格式串, 实参…) -> 0`：把格式化结果追加到文本输出桶（`printf` 用它）。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
Slot strPrint(NativeContext &ctx, const Memory::Vector<Slot> &args);

/**
 * @brief 输出一个字符。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
Slot putChar(NativeContext &ctx, const Memory::Vector<Slot> &args);

/**
 * @brief `__std_str_set(串, 下标, 字符码) -> 0`：往字符串的第 `下标` 个字节写一个字符。
 *
 * 字符串在堆上是**可写**的（字面量当值用时已经复制成自己的那块），越界就报错 ——
 * 和数组下标一个脾气。写进去的是低 8 位。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
Slot strSet(NativeContext &ctx, const Memory::Vector<Slot> &args);

/**
 * @brief `__std_str_at(串, 下标) -> 字符码 / -1`：按下标取一个字符。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
Slot strAt(NativeContext &ctx, const Memory::Vector<Slot> &args);

/**
 * @brief 验证下标后取得字符串字符地址，范围与普通字符串下标一致。
 * @param ctx 提供堆块和数据区地址范围的运行环境。
 * @param args 字符串地址和按 UTF-8 字节计数的下标。
 * @return 对应字节地址；读取失败或越界时返回零并登记运行时错误。
 * @note 堆串跳过长度头；数据区字面量直接从字节起始地址取值。
 */
Slot strRef(NativeContext &ctx, const Memory::Vector<Slot> &args);

/**
 * @brief `__std_memcpy(目标, 源, 字节数) -> 0`：逐字节搬一段（C++ 里就是 `memcpy`）。
 *
 * 源与目标整段必须**各自落在一块已分配内存里**（堆块 / 数据区符号），否则报错 ——
 * 以前语言侧是"在目标地址上读 8 字节 → 只改最低那个字节 → 写回"，那么读堆串最后一
 * 个字节时那 8 字节会越过块尾（现在字符串在堆上，块是紧的）。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
Slot memCopy(NativeContext &ctx, const Memory::Vector<Slot> &args);

/**
 * @brief `__std_memset(地址, 字节值, 字节数) -> 0`：把一段内存填成同一个字节。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
Slot memSet(NativeContext &ctx, const Memory::Vector<Slot> &args);
}  // namespace stackvm::runtimeDetail
