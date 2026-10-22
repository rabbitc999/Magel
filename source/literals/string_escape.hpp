#ifndef CHALI_LITERAL_STRING_ESCAPE_HPP
#define CHALI_LITERAL_STRING_ESCAPE_HPP
#include <string>
#include <string_view>

namespace chali_literal
{
/**
 * @brief 按语言字符串规则解码一次转义，供普通字面量和编译期宏共用。
 * @param raw 去掉外层引号后仍保留反斜杠的内容。
 * @param output 接收解码后的字节，失败时保留错误之前的内容。
 * @param error 接收错误，成功时清空。
 * @return 所有转义合法时返回真。
 */
bool decodeString(std::string_view raw, std::string &output, std::string &error);
}  // namespace chali_literal
#endif
