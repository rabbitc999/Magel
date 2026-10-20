#include "source.hpp"
#include <cctype>

namespace magel_macro
{
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
            for (const Memory::String keyword : { "$macro", "$match", "$regex", "$else" })
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
}  // namespace magel_macro
