/**
 * @file quad_optimizer_internal.hpp
 * @brief VM 内部 quad optimizer internal 模块的类型与接口。
 */
#pragma once
#include "stackvm.hpp"

namespace stackvm::runtimeDetail
{
// ---------------------------------------------------------------------------
//  执行
// ---------------------------------------------------------------------------

/**
 * @brief 读一个字面量。
 * @param token 原文。
 * @param isNumber 输出：是不是数字。
 * @param isFloat 输出：是不是浮点。
 * @param intValue 输出：整数值。
 * @param doubleValue 输出：浮点值。
 * @param text 输出：字符串内容。
 * @return 是字面量返回 true。
 */
bool readLiteral(const Memory::String &token, bool &isNumber, bool &isFloat,
                 std::int64_t &intValue, double &doubleValue, Memory::String &text);

/**
 * @brief 一条四元式读到的变量（用于死代码分析）。
 * @param quad 四元式。
 * @return 变量名列表（不含字面量）。
 */
Memory::Vector<Memory::String> readOperands(const Quad &quad);

/**
 * @brief 这条四元式会不会写变量（写的是第几个参数）。
 * @param base 基础操作符。
 * @return 写入的参数下标；不写返回 -1。
 */
int writesTo(const Memory::String &base);

/**
 * @brief 返回真正读取值的操作数下标，元数据及标签不参与传播。
 * @param base 基础操作码。
 * @return 读操作数下标。
 */
std::array<std::size_t, 2> optimizationReads(const Memory::String &base);

/**
 * @brief 识别可消除复制的无声明编译器临时量。
 * @param name 名字。
 * @return 是否 t 加数字。
 */
bool optimizationTemporary(const Memory::String &name);

/**
 * @brief 基本块内传播同类型复制与消除同类型转换，不跨过调用或控制流边界。
 * @param code 四元式。
 * @param declared 声明类型。
 * @param report 优化统计。
 * @return 是否改变。
 */
bool optimizeLocalCopies(Memory::Vector<Quad> &code, const Memory::Map<Memory::String, TypeInfo> &declared,
                         OptimizationReport &report);

/**
 * @brief 在控制流合流及循环回边上求可用整数常量的不动点。
 * @param code 四元式序列。
 * @param declared 已声明类型。
 * @param report 优化统计。
 * @return 是否替换了跨块常量或重复表达式。
 * @note 调用及内存副作用清空状态；分析未收敛时不应用常量结果。
 */
bool optimizeControlFlow(Memory::Vector<Quad> &code, const Memory::Map<Memory::String, TypeInfo> &declared,
                         OptimizationReport &report);
}  // namespace stackvm::runtimeDetail
