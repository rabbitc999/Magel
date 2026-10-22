#include "string_escape.hpp"

namespace chali_literal
{
/**
 * @brief 完整解码语言支持的控制字符、引号及反斜杠转义。
 * @param raw 未解码内容。
 * @param output 接收解码结果。
 * @param error 接收错误。
 * @return 解码成功返回真。
 */
bool decodeString(std::string_view raw, std::string &output, std::string &error)
{
    output.clear();
    error.clear();
    for (std::size_t index = 0; index < raw.size(); ++index)
    {
        if (raw[index] != '\\')
        {
            output += raw[index];
            continue;
        }
        if (++index == raw.size())
        {
            error = "字符串以反斜杠结尾（写 `\\\\` 表示一个反斜杠）";
            return false;
        }
        switch (raw[index])
        {
        case 'n': output += '\n'; break;
        case 't': output += '\t'; break;
        case 'r': output += '\r'; break;
        case '0': output += '\0'; break;
        case 'a': output += '\a'; break;
        case 'b': output += '\b'; break;
        case 'f': output += '\f'; break;
        case 'v': output += '\v'; break;
        case '\\': output += '\\'; break;
        case '"': output += '"'; break;
        case '\'': output += '\''; break;
        default:
            error = std::string("字符串里不认识的转义：\\") + raw[index] + "（支持 \\n \\t \\r \\0 \\\\ \\\" \\' ）";
            return false;
        }
    }
    return true;
}
}  // namespace chali_literal
