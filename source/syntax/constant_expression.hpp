#ifndef CHALI_SYNTAX_CONSTANT_EXPRESSION_HPP
#define CHALI_SYNTAX_CONSTANT_EXPRESSION_HPP
#include "source_text.hpp"

namespace chali_syntax
{
/**
 * @brief 普通 const 泛型归一化使用的整数或字符串常量，不执行语言函数。
 */
struct CompileTimeConstant
{
    long long number = 0;
    Memory::String text;
    bool string = false;
    /**
     * @brief 将常量转换为条件真值。
     * @return 字符串非空或整数非零时返回真。
     */
    bool truth() const { return string ? !text.empty() : number != 0; }
};
/**
 * @brief 完整求值 const 泛型的受限常量表达式。
 * @param source 常量表达式原文。
 * @param values 当前可用的常量绑定。
 * @param error 接收语法、溢出及无效运算诊断。
 * @return 求得的常量，失败时由 error 表示原因。
 */
CompileTimeConstant evaluateConstant(const Memory::String &source,
                                     const Memory::Map<Memory::String, Memory::String> &values,
                                     Memory::String &error);
}  // namespace chali_syntax
#endif
