#pragma once
#include "allocator/magel_allocator.hpp"

namespace magel_macro
{
/**
 * @brief 宏条件的编译期值，支持整数、布尔和字符串比较。
 */
struct MacroConstant
{
    long long number = 0;  ///< 整数槽。
    Memory::String text;   ///< 字符串槽。
    bool string = false;   ///< 是否为字符串。
    /**
     * @brief 条件真值。
     * @return 是否为真。
     */
    bool truth() const { return string ? !text.empty() : number != 0; }
};
/**
 * @brief 求值文本宏与 const 泛型共用的编译期常量表达式。
 * @param source 需要完整消费的常量表达式。
 * @param values 当前可用的文本常量绑定。
 * @param error 失败时接收解析或运算诊断。
 * @return 表达式的整数或字符串常量；失败时由 error 指示原因。
 */
MacroConstant evaluateCondition(const Memory::String &source, const Memory::Map<Memory::String, Memory::String> &values, Memory::String &error);

/**
 * @brief 替换局部文本绑定，字符串与注释保持原样。
 * @param text 源文。
 * @param values 局部绑定。
 * @return 替换结果。
 */
Memory::String substituteMacroLocals(const Memory::String &text,
                                     const Memory::Map<Memory::String, Memory::String> &values);

/**
 * @brief 选择宏内条件分支并创建调用局部绑定。
 * @param body 宏体。
 * @param values 展开后的形参。
 * @param rawValues 原始形参。
 * @param argc 实参数量。
 * @param error 诊断。
 * @param variadicParam 尾参名称。
 * @param tail 尾参元素。
 * @param protect 是否保护各元素。
 * @param depth 嵌套深度。
 * @param groupTails 多括号宏中各组的独立尾参；为空时使用原有单尾参。
 * @return 是否成功。
 */
bool expandMacroControls(Memory::String &body,
                         Memory::Map<Memory::String, Memory::String> &values,
                         Memory::Map<Memory::String, Memory::String> &rawValues, std::size_t argc, Memory::String &error,
                         const Memory::String &variadicParam, const Memory::Vector<Memory::String> &tail, bool protect, unsigned depth = 0,
                         const Memory::Map<Memory::String, Memory::Vector<Memory::String>> *groupTails = nullptr);
}  // namespace magel_macro
