#include "allocator/chali_allocator.hpp"
#ifndef CHALI_LAMBDA_HPP
#define CHALI_LAMBDA_HPP

#include "preprocess.hpp"
#include "chali_generic_closure.hpp"

namespace chali_lambda
{

/**
 * @brief 内部 lambda 名称对应的显式捕获列表，编译开始时清空。
 */
extern Memory::Map<Memory::String, Memory::Map<Memory::String, int>> captureSpecifications;

/**
 * @brief 移除文本两端的空白并保留内部文本。
 * @param value 原始文本。
 * @return 去除首尾空白的文本；全为空白时返回空串。
 */
Memory::String trim(const Memory::String &value);

/**
 * @brief 判断字符是否属于闭包源码中的标识符。
 * @param value 待检查的字符。
 * @return 字母、数字或下划线返回 true。
 */
bool ident(char value);

/**
 * @brief 遮蔽注释和字面量内容，同时保持偏移和换行不变。
 * @param text 待分析的原始源码。
 * @return 与源码等长且只暴露代码结构的掩码。
 */
Memory::String codeMask(const Memory::String &text);

/**
 * @brief 跳过注释和字面量，查找成对分隔符的结束位置。
 * @param text 完整源码。
 * @param open 起始分隔符的位置。
 * @param left 左分隔符。
 * @param right 右分隔符。
 * @param close 成功时接收匹配的右分隔符位置。
 * @return 嵌套分隔符完整闭合时返回 true。
 */
bool balanced(const Memory::String &text, std::size_t open, char left, char right,
              std::size_t &close);

/**
 * @brief 仅在最外层逗号处分割闭包调用实参。
 * @param text 可能包含嵌套调用的参数文本。
 * @return 各个实参的文本列表。
 */
Memory::Vector<Memory::String> split(const Memory::String &text);

/**
 * @brief 查找具有完整标识符边界的单词。
 * @param text 待查询的源码文本。
 * @param word 需要匹配的完整单词。
 * @return 至少存在一个边界完整的匹配时返回 true。
 */
bool hasWord(const Memory::String &text, const Memory::String &word);

/**
 * @brief 从已知变量或基础字面量推断闭包调用实参类型。
 * @param argument 调用实参的原始文本。
 * @param variables 当前可见变量名及类型拼写。
 * @return 已知的类型拼写；文本阶段无法确定时返回空串。
 */
Memory::String inferArgument(const Memory::String &argument,
                             const Memory::Map<Memory::String, Memory::String> &variables);

/**
 * @brief 保留模块与行映射并提升待推导 lambda。
 * @param result 源码及原行映射。
 * @param error 失败说明。
 * @return 成功返回 true。
 */
bool run(preprocess::Result &result, Memory::String &error);

}  // namespace chali_lambda

#endif
