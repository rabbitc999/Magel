#include "source.hpp"
#include <cctype>
#include <algorithm>

namespace chali_macro
{
/**
 * @brief 屏蔽注释和字面量以定位编译期指令，不把屏蔽结果交给语言解析器。
 * @param source 保留原文的输入。
 * @param lineComments 非空时收集真实行注释的开始位置。
 * @return 长度和换行完全相同的词法扫描掩码。
 */
static Memory::String directiveMask(const Memory::String &source, std::vector<std::size_t> *lineComments = nullptr)
{
    auto result = source;
    char quote = 0;
    bool block = false, line = false;
    for (std::size_t at = 0; at < source.size(); ++at)
    {
        const char ch = source[at];
        if (line || block || quote)
        {
            if (ch != '\n' && ch != '\r') result[at] = ' ';
            if (line && ch == '\n')
                line = false;
            else if (block && source.compare(at, 2, "*/") == 0)
            {
                result[++at] = ' ';
                block = false;
            }
            else if (quote && ch == '\\' && at + 1 < source.size())
            {
                if (source[at + 1] != '\n') result[++at] = ' ';
            }
            else if (quote && ch == quote)
                quote = 0;
            continue;
        }
        if (ch == '"' || ch == '\'')
        {
            quote = ch;
            result[at] = ' ';
        }
        else if (source.compare(at, 2, "//") == 0)
        {
            if (lineComments) lineComments->push_back(at);
            line = true;
            result[at] = ' ';
            result[++at] = ' ';
        }
        else if (source.compare(at, 2, "/*") == 0)
        {
            block = true;
            result[at] = ' ';
            result[++at] = ' ';
        }
    }
    return result;
}

/**
 * @brief 给单行宏插入词法边界，不展开正文也不为表达式补括号。
 * @param source 待处理源码。
 * @param error 接收无正文或头部括号错误。
 * @return 边界标记成功时返回真。
 */
bool markReplacementBoundaries(Memory::String &source, std::string &error)
{
    error.clear();
    std::vector<std::size_t> comments;
    const auto mask = directiveMask(source, &comments);
    std::vector<std::pair<std::size_t, Memory::String>> insertions;
    for (std::size_t at = 0; at < mask.size(); ++at)
    {
        if (mask.compare(at, 8, "$replace") != 0 || (at + 8 < mask.size() && (std::isalnum(static_cast<unsigned char>(mask[at + 8])) || mask[at + 8] == '_'))) continue;
        auto end = mask.find('\n', at);
        if (end == Memory::String::npos) end = mask.size();
        auto header = at + 8;
        const auto spaces = [&] { while (header < end && std::isspace(static_cast<unsigned char>(source[header]))) ++header; };
        spaces();
        if (header == end || !(std::isalpha(static_cast<unsigned char>(mask[header])) || mask[header] == '_'))
        {
            error = "单行宏缺少宏名称";
            return false;
        }
        while (header < end && (std::isalnum(static_cast<unsigned char>(mask[header])) || mask[header] == '_')) ++header;
        spaces();
        bool generic = true;
        while (header < end && (mask[header] == '(' || mask[header] == '[' || mask[header] == '{' || (generic && mask[header] == '<')))
        {
            const char opening = mask[header];
            const char closing = opening == '(' ? ')' : opening == '[' ? ']' :
                                                    opening == '{'     ? '}' :
                                                                         '>';
            const auto close = mask.find(closing, header + 1);
            if (close == Memory::String::npos || close >= end)
            {
                error = "单行宏的形参组缺少结束括号";
                return false;
            }
            bool parameters = true;
            for (auto index = header + 1; index < close; ++index)
                if (!(std::isalnum(static_cast<unsigned char>(mask[index])) || mask[index] == '_' || mask[index] == ',' || mask[index] == '.' || std::isspace(static_cast<unsigned char>(mask[index])) || (opening == '<' && mask[index] == ':'))) parameters = false;
            if (!parameters) break;
            header = close + 1;
            generic = false;
            spaces();
        }
        if (header >= end)
        {
            error = "单行宏必须在同一行提供正文";
            return false;
        }
        // 行尾注释必须留在结束标记之后；字面量中的斜线已被掩码屏蔽。
        auto bodyEnd = end;
        const auto comment = std::lower_bound(comments.begin(), comments.end(), header);
        if (comment != comments.end() && *comment < end) bodyEnd = *comment;
        insertions.push_back({ header, " @!macrobody " });
        insertions.push_back({ bodyEnd, " $end_macro " });
        at = end;
    }
    for (auto item = insertions.rbegin(); item != insertions.rend(); ++item) source.insert(item->first, item->second);
    return true;
}
/**
 * @brief 扫描注释与字符串之外的宏定义及分支头，为解析器保留换行边界。
 * @param source 原地添加内部头部结束标记的源码。
 */
void markHeaderBoundaries(Memory::String &source)
{
    bool blockComment = false;
    bool lineComment = false;
    char quote = 0;
    bool header = false;
    std::size_t headerComment = Memory::String::npos;
    for (std::size_t at = 0; at < source.size(); ++at)
    {
        const char ch = source[at];
        if (ch == '\n')
        {
            lineComment = false;
            if (header && !quote && !blockComment)
            {
                source.insert(headerComment == Memory::String::npos ? at : headerComment, " @!macrobody ");
                at += 13;
                header = false;
            }
            headerComment = Memory::String::npos;
            continue;
        }
        if (lineComment) continue;
        if (blockComment)
        {
            if (source.compare(at, 2, "*/") == 0)
            {
                blockComment = false;
                ++at;
            }
            continue;
        }
        if (quote)
        {
            if (ch == '\\')
                ++at;
            else if (ch == quote)
                quote = 0;
            continue;
        }
        if (ch == '\'' || ch == '"')
            quote = ch;
        else if (source.compare(at, 2, "//") == 0)
        {
            lineComment = true;
            if (header) headerComment = at;
        }
        else if (source.compare(at, 2, "/*") == 0)
            blockComment = true;
        else if (ch == '$')
        {
            for (const Memory::String keyword : { "$macro", "$match", "$regex", "$else", "$for" })
            {
                const auto end = at + keyword.size();
                if (source.compare(at, keyword.size(), keyword) == 0 && (end == source.size() || !(std::isalnum(static_cast<unsigned char>(source[end])) || source[end] == '_')))
                {
                    header = true;
                    break;
                }
            }
        }
    }
    if (header) source += " @!macrobody";
}

/**
 * @brief 隐藏字符串和注释之外的完整宏定义，保持宏体原文及原有换行。
 * @param source 含定义的源码。
 * @param error 接收宏结构错误。
 * @return 定义全部闭合且不嵌套时返回真。
 */
bool ProtectedDefinitions::hide(Memory::String &source, std::string &error)
{
    definitions_.clear();
    error.clear();
    std::size_t begin = Memory::String::npos;
    char quote = 0;
    bool block = false, line = false;
    for (std::size_t at = 0; at < source.size(); ++at)
    {
        const char ch = source[at];
        if (line)
        {
            if (ch == '\n') line = false;
            continue;
        }
        if (block)
        {
            if (source.compare(at, 2, "*/") == 0)
            {
                block = false;
                ++at;
            }
            continue;
        }
        if (quote)
        {
            if (ch == '\\')
                ++at;
            else if (ch == quote)
                quote = 0;
            continue;
        }
        if (ch == '"' || ch == '\'')
        {
            quote = ch;
            continue;
        }
        if (source.compare(at, 2, "//") == 0)
        {
            line = true;
            ++at;
            continue;
        }
        if (source.compare(at, 2, "/*") == 0)
        {
            block = true;
            ++at;
            continue;
        }
        if (ch != '$') continue;
        auto end = at + 1;
        while (end < source.size() && (std::isalnum(static_cast<unsigned char>(source[end])) || source[end] == '_')) ++end;
        const auto word = source.substr(at, end - at);
        if (word == "$macro" || word == "$replace")
        {
            if (begin != Memory::String::npos)
            {
                error = "不支持嵌套宏定义";
                return false;
            }
            begin = at;
        }
        else if (word == "$end_macro")
        {
            if (begin == Memory::String::npos)
            {
                error = "$end_macro 没有对应的宏定义";
                return false;
            }
            const auto marker = Memory::String("/*@!m") + Memory::toString(definitions_.size()) + "*/";
            const auto original = source.substr(begin, end - begin);
            definitions_.push_back({ marker, original });
            for (auto index = begin; index < end; ++index)
                if (source[index] != '\n' && source[index] != '\r') source[index] = ' ';
            source.insert(begin, marker);
            end += marker.size();
            begin = Memory::String::npos;
        }
        at = end - 1;
    }
    if (begin != Memory::String::npos)
    {
        error = "宏定义缺少 $end_macro";
        return false;
    }
    return true;
}

/**
 * @brief 根据唯一注释占位恢复模板，不重新生成宏体或改写其内容。
 * @param source 普通闭包及泛型归一化后的源码。
 * @param error 接收丢失占位的错误。
 * @return 每个定义恢复成功时返回真。
 */
bool ProtectedDefinitions::restore(Memory::String &source, std::string &error) const
{
    error.clear();
    for (const auto &definition : definitions_)
    {
        const auto at = source.find(definition.first);
        if (at == Memory::String::npos || source.size() - at < definition.second.size() + definition.first.size())
        {
            error = "普通源码归一化丢失了宏定义占位";
            return false;
        }
        source.replace(at, definition.second.size() + definition.first.size(), definition.second);
    }
    return true;
}
}  // namespace chali_macro
