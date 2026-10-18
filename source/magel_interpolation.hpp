#ifndef MAGEL_INTERPOLATION_HPP
#define MAGEL_INTERPOLATION_HPP
#include "preprocess.hpp"
#include <cctype>
#include <cstdlib>

/**
 * @brief 插值字符串的词法归一化；表达式仍完整交给生成解析器。
 */
namespace magel_interpolation
{
/**
 * @brief 扫描字符串与插值边界，保留普通代码与注释。
 */
struct Scanner
{
    const Memory::String &source;  ///< 原始源码。
    Memory::String &error;         ///< 词法诊断。
    std::size_t at = 0;            ///< 当前字符位置。
    unsigned nesting = 0;          ///< 嵌套插值深度。

    /**
     * @brief 复制普通字符串或字符。
     * @return 含引号的原文。
     */
    Memory::String quoted()
    {
        const auto begin = at;
        const char quote = source[at++];
        while (at < source.size())
        {
            const char ch = source[at++];
            if (ch == '\\' && at < source.size())
                ++at;
            else if (ch == quote)
                return source.substr(begin, at - begin);
        }
        error = "字符串缺少结束引号";
        return {};
    }
    /**
     * @brief 复制注释，不解释其中的插值标记。
     * @return 注释原文。
     */
    Memory::String comment()
    {
        const auto begin = at;
        const bool line = source[at + 1] == '/';
        const auto end = source.find(line ? "\n" : "*/", at + 2);
        at = end == Memory::String::npos ? source.size() : end + (line ? 0u : 2u);
        return source.substr(begin, at - begin);
    }
    /**
     * @brief 读取完整表达式的词法边界，嵌套字符串由内到外归一化。
     * @param interpolation 是否以右大括号结束。
     * @return 原文或归一化的表达式。
     */
    Memory::String code(bool interpolation)
    {
        Memory::String out;
        int braces = 0;
        while (at < source.size() && error.empty())
        {
            const char ch = source[at];
            if (ch == 'f' && at + 1 < source.size() && source[at + 1] == '"' && (at == 0 || !(std::isalnum(static_cast<unsigned char>(source[at - 1])) || source[at - 1] == '_' || source[at - 1] == '$')))
            {
                out += string();
            }
            else if (ch == '"' || ch == '\'')
                out += quoted();
            else if (ch == '/' && at + 1 < source.size() && (source[at + 1] == '/' || source[at + 1] == '*'))
                out += comment();
            else if (interpolation && ch == '}' && braces == 0)
            {
                ++at;
                return out;
            }
            else
            {
                if (ch == '{')
                    ++braces;
                else if (ch == '}')
                    --braces;
                out += source[at++];
            }
        }
        if (interpolation && error.empty()) error = "插值表达式缺少 `}`";
        return out;
    }
    /**
     * @brief 将插值片段编码为格式串和完整实参列表。
     * @return 交给文法解析的内部字符串表达式。
     */
    Memory::String string()
    {
        if (++nesting > 64)
        {
            error = "插值字符串嵌套超过 64 层";
            return {};
        }
        const auto begin = at;
        at += 2;
        Memory::String format = "\"";
        Memory::Vector<Memory::String> expressions;
        bool closed = false;
        std::size_t padding = 0;
        while (at < source.size() && error.empty())
        {
            const char ch = source[at++];
            if (ch == '"')
            {
                closed = true;
                break;
            }
            if (ch == '\\' && at < source.size())
            {
                const char escaped = source[at++];
                if (escaped == '{' || escaped == '}')
                {
                    format += escaped;
                    format += escaped;
                }
                else
                {
                    format += '\\';
                    format += escaped;
                }
            }
            else if (ch == '{')
            {
                auto expression = code(true);
                if (expression.find_first_not_of(" \t\r\n") == Memory::String::npos)
                {
                    error = "插值 `{}` 中必须写表达式";
                    break;
                }
                expressions.push_back(std::move(expression));
                format += "{}";
            }
            else if (ch == '}')
            {
                error = "插值中的字面 `}` 必须写成 `\\}`";
                break;
            }
            else if (ch == '\n')
            {
                format += "\\n";
                ++padding;
            }
            else
                format += ch;
        }
        --nesting;
        if (!closed && error.empty()) error = "插值字符串缺少结束引号";
        format += '"';
        Memory::String out = "__magel_fstring(" + format;
        for (const auto &expression : expressions) out += ",(" + expression + ")";
        out += ")";
        const char *digits = "0123456789abcdef";
        Memory::String raw;
        for (const unsigned char ch : source.substr(begin, at - begin))
        {
            raw += digits[ch >> 4];
            raw += digits[ch & 15];
        }
        return "/*__magel_fraw:" + Memory::toString(padding) + ":" + raw + "__*/" + out + Memory::String(padding, '\n');
    }
};
/**
 * @brief 恢复宏实参的插值原文，保证 stringify 沿用原始实参语义。
 * @param text 已归一化的文本。
 * @return 恢复原始插值的文本。
 */
inline Memory::String rawText(const Memory::String &text)
{
    Memory::String output;
    for (std::size_t at = 0; at < text.size();)
    {
        if (text.compare(at, 15, "/*__magel_fraw:") != 0)
        {
            output += text[at++];
            continue;
        }
        const auto colon = text.find(':', at + 15), markerEnd = text.find("__*/", at + 15);
        if (colon == Memory::String::npos || markerEnd == Memory::String::npos)
        {
            output += text[at++];
            continue;
        }
        const auto padding = std::strtoul(text.substr(at + 15, colon - at - 15).c_str(), nullptr, 10);
        std::size_t begin = markerEnd + 4;
        if (text.compare(begin, 16, "__magel_fstring(") != 0)
        {
            output += text[at++];
            continue;
        }
        std::size_t end = begin + 16;
        int parentheses = 1;
        char quote = 0;
        for (; end < text.size() && parentheses; ++end)
        {
            const char ch = text[end];
            if (quote)
            {
                if (ch == '\\')
                    ++end;
                else if (ch == quote)
                    quote = 0;
            }
            else if (text.compare(end, 2, "/*") == 0)
            {
                const auto close = text.find("*/", end + 2);
                if (close == Memory::String::npos) break;
                end = close + 1;
            }
            else if (text.compare(end, 2, "//") == 0)
            {
                const auto close = text.find('\n', end + 2);
                if (close == Memory::String::npos) break;
                end = close;
            }
            else if (ch == '"' || ch == '\'')
                quote = ch;
            else if (ch == '(')
                ++parentheses;
            else if (ch == ')')
                --parentheses;
        }
        if (parentheses)
        {
            output += text[at++];
            continue;
        }
        const auto digit = [](char ch) { return ch >= '0' && ch <= '9' ? ch - '0' : ch >= 'a' && ch <= 'f' ? ch - 'a' + 10 :
                                                                                                             -1; };
        Memory::String raw;
        for (auto i = colon + 1; i + 1 < markerEnd; i += 2)
        {
            const int high = digit(text[i]), low = digit(text[i + 1]);
            if (high < 0 || low < 0)
            {
                raw.clear();
                break;
            }
            raw += static_cast<char>((high << 4) | low);
        }
        if (raw.empty())
        {
            output += text[at++];
            continue;
        }
        for (std::size_t i = 0; i < padding && end < text.size() && text[end] == '\n'; ++i) ++end;
        output += raw;
        at = end;
    }
    return output;
}
/**
 * @brief 归一化插值并重新映射行来源。
 * @param result 源码与位置。
 * @param error 诊断。
 * @return 是否成功。
 */
inline bool run(preprocess::Result &result, Memory::String &error)
{
    Scanner scanner{ result.text, error };
    auto output = scanner.code(false);
    if (!error.empty()) return false;
    // 插值格式串中的换行仍在原位置，表达式新增的换行逐行沿用起始源码位置。
    Memory::Vector<preprocess::LineOrigin> origins;
    std::size_t oldLine = 0;
    for (const char ch : output)
        if (ch == '\n')
        {
            origins.push_back(oldLine < result.lineMap.size() ? result.lineMap[oldLine++] : preprocess::LineOrigin{});
        }
    if (oldLine < result.lineMap.size()) origins.push_back(result.lineMap[oldLine]);
    result.text = std::move(output);
    result.lineMap = std::move(origins);
    return true;
}
}  // namespace magel_interpolation
#endif
