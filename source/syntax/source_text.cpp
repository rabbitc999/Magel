#include "source_text.hpp"
#include <cctype>

namespace chali_syntax
{
/**
 * @brief 判断字符能否开始一个普通标识符。
 * @param ch 待检查的字符。
 * @return 字母或下划线返回 true。
 */
bool identStart(char ch)
{
    return std::isalpha(static_cast<unsigned char>(ch)) || ch == '_';
}

/**
 * @brief 判断字符能否继续组成普通标识符。
 * @param ch 待检查的字符。
 * @return 字母、数字或下划线返回 true。
 */
bool identPart(char ch)
{
    return std::isalnum(static_cast<unsigned char>(ch)) || ch == '_';
}

/**
 * @brief 跳过当前位置开始的源码文本空白。
 * @param s 待扫描的源码文本。
 * @param i 输入并更新扫描位置。
 */
void spaces(const Memory::String &s, std::size_t &i)
{
    while (i < s.size() && std::isspace(static_cast<unsigned char>(s[i]))) ++i;
}

/**
 * @brief 移除文本两端的空白并保留内部文本。
 * @param text 原始文本。
 * @return 去除首尾空白的文本；全为空白时返回空串。
 */
Memory::String trim(const Memory::String &text)
{
    std::size_t begin = 0;
    std::size_t end = text.size();
    while (begin < end && std::isspace(static_cast<unsigned char>(text[begin]))) ++begin;
    while (end > begin && std::isspace(static_cast<unsigned char>(text[end - 1]))) --end;
    return text.substr(begin, end - begin);
}

/**
 * @brief 返回文本分组左括号对应的右括号。
 * @param ch 待检查字符。
 * @retval 右括号；非左括号返回零。
 */
char groupClose(char ch)
{
    switch (ch)
    {
    case '(': return ')';
    case '[': return ']';
    case '{': return '}';
    case '<': return '>';
    default: return 0;
    }
}

/**
 * @brief 按字符栈切分一组文本，不解释运算符或类型。
 * @param text 原始文本。
 * @param open 左括号位置。
 * @param parts 输出当前层的逗号分隔片段。
 * @param end 输出右括号后一位置。
 * @param allowIncomplete 是否允许编辑器读取尚未闭合的末尾分组；编译默认关闭。
 * @param closed 可选输出，指出分组是否已完整闭合。
 * @retval 成功返回 true；缺少或交叉闭合括号返回 false。
 * @note 字符串和注释中的括号不参与分组，连续右尖括号逐字符处理。
 */
bool groupArguments(const Memory::String &text, std::size_t open,
                    Memory::Vector<Memory::String> &parts, std::size_t &end,
                    bool allowIncomplete, bool *closed)
{
    if (closed) *closed = false;
    if (open >= text.size() || !groupClose(text[open])) return false;
    Memory::Vector<char> closes{ groupClose(text[open]) };
    std::size_t begin = open + 1;
    for (std::size_t i = begin; i < text.size(); ++i)
    {
        const char ch = text[i];
        if (ch == '\"' || ch == '\'')
        {
            const char quote = ch;
            while (++i < text.size())
            {
                if (text[i] == '\\' && i + 1 < text.size())
                    ++i;
                else if (text[i] == quote)
                    break;
            }
            continue;
        }
        if (text.compare(i, 2, "//") == 0)
        {
            i = text.find('\n', i + 2);
            if (i == Memory::String::npos) break;
            continue;
        }
        if (text.compare(i, 2, "/*") == 0)
        {
            i = text.find("*/", i + 2);
            if (i == Memory::String::npos) break;
            ++i;
            continue;
        }
        if (ch == '<' && closes.back() != '>')
        {
            // 非尖括号组中的未配对尖括号按普通文本保留，比较表达式无需语言解析。
            Memory::Vector<Memory::String> ignored;
            std::size_t angleEnd = i;
            if (!groupArguments(text, i, ignored, angleEnd)) continue;
        }
        if (ch == '>' && closes.back() != '>') continue;
        if (const char close = groupClose(ch))
            closes.push_back(close);
        else if (ch == ')' || ch == ']' || ch == '}' || ch == '>')
        {
            if (closes.back() != ch) return false;
            closes.pop_back();
            if (closes.empty())
            {
                const auto part = text.substr(begin, i - begin);
                if (!parts.empty() || !trim(part).empty()) parts.push_back(part);
                end = i + 1;
                if (closed) *closed = true;
                return true;
            }
        }
        else if (ch == ',' && closes.size() == 1)
        {
            parts.push_back(text.substr(begin, i - begin));
            begin = i + 1;
        }
    }
    if (allowIncomplete)
    {
        parts.push_back(text.substr(begin));
        end = text.size();
        return true;
    }
    return false;
}

}  // namespace chali_syntax
