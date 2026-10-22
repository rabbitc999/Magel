#ifndef CHALI_SYNTAX_SOURCE_TEXT_HPP
#define CHALI_SYNTAX_SOURCE_TEXT_HPP
#include "allocator/chali_allocator.hpp"

namespace chali_syntax
{
/**
 * @brief 判断字符能否开始普通标识符。
 * @param ch 待检查的字符。
 * @return 字母或下划线返回真。
 */
bool identStart(char ch);
/**
 * @brief 判断字符能否继续组成普通标识符。
 * @param ch 待检查的字符。
 * @return 字母、数字或下划线返回真。
 */
bool identPart(char ch);
/**
 * @brief 跳过源码扫描中的空白。
 * @param source 原始文本。
 * @param at 输入并更新扫描位置。
 */
void spaces(const Memory::String &source, std::size_t &at);
/**
 * @brief 移除首尾空白，保留所有内部文本。
 * @param text 原始文本。
 * @return 去除两端空白后的文本。
 */
Memory::String trim(const Memory::String &text);
/**
 * @brief 查询四种左括号对应的结束字符。
 * @param ch 左括号字符。
 * @return 对应右括号；不是左括号时返回零。
 */
char groupClose(char ch);
/**
 * @brief 在最外层逗号处切分平衡分组，字符串和注释中的标点保持原样。
 * @param text 完整原文。
 * @param open 左括号位置。
 * @param parts 接收原始片段。
 * @param end 接收结束括号后一位置。
 * @param allowIncomplete 是否允许编辑器读取尚未闭合的组。
 * @param closed 可选输出，记录括号是否已闭合。
 * @return 分组有效或允许未完成输入时返回真。
 */
bool groupArguments(const Memory::String &text, std::size_t open,
                    Memory::Vector<Memory::String> &parts, std::size_t &end,
                    bool allowIncomplete = false, bool *closed = nullptr);
}  // namespace chali_syntax
#endif
