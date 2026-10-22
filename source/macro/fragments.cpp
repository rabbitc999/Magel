#include "fragments.hpp"
#include <algorithm>
#include <cctype>
#include <stdexcept>

namespace chali_macro
{
SourceFragments sourceFragments;

/**
 * @brief 清除既有片段，防止跨编译沿用编号。
 */
void SourceFragments::clear()
{
    invocations_.clear();
    activeBegin_ = 0;
    active_ = false;
}

/**
 * @brief 关闭词素查找，避免后续生成源码误用上一输入的相同行列。
 */
void SourceFragments::finishParsing()
{
    active_ = false;
}

/**
 * @brief 返回按捕获顺序保存的调用。
 * @param index 调用编号。
 * @return 对应调用记录。
 */
const SourceInvocation &SourceFragments::at(std::size_t index) const
{
    return invocations_.at(index);
}

/**
 * @brief 在当前捕获批次中定位原始调用，避免递归片段解析误用其他输入的坐标。
 * @param line 原始行号。
 * @param column 原始列号。
 * @return 调用编号，不存在返回 size_t 最大值。
 */
std::size_t SourceFragments::find(std::size_t line, std::size_t column) const
{
    if (!active_) return static_cast<std::size_t>(-1);
    for (std::size_t index = activeBegin_; index < invocations_.size(); ++index)
        if (invocations_[index].line == line && invocations_[index].column == column) return index;
    return static_cast<std::size_t>(-1);
}

/**
 * @brief 判断 ASCII 字符是否属于标识符。
 * @param ch 待分类字符。
 * @return 字母、数字或下划线返回真。
 */
static bool identifierChar(char ch)
{
    return std::isalnum(static_cast<unsigned char>(ch)) || ch == '_';
}

/**
 * @brief 跳过一个字符串、字符常量或注释，避免把其中的括号和美元符号当语法。
 * @param text 原始源码。
 * @param at 当前字符位置，成功时移到片段之后。
 * @return 当前字符是否开始引用或注释片段。
 */
static bool skipQuoted(const std::string &text, std::size_t &at)
{
    if (text[at] == '"' || text[at] == '\'')
    {
        const char quote = text[at++];
        while (at < text.size())
        {
            const char ch = text[at++];
            if (ch == '\\' && at < text.size())
                ++at;
            else if (ch == quote)
                break;
        }
        return true;
    }
    if (text.compare(at, 2, "//") == 0)
    {
        at = text.find('\n', at);
        if (at == std::string::npos) at = text.size();
        return true;
    }
    if (text.compare(at, 2, "/*") == 0)
    {
        const auto end = text.find("*/", at + 2);
        at = end == std::string::npos ? text.size() : end + 2;
        return true;
    }
    return false;
}

/**
 * @brief 识别嵌套宏泛型或已经归一化的具名泛型参数起点，避免误分其逗号。
 * @param text 源码。
 * @param at 小于号所在位置。
 * @return 属于具名泛型参数列表时返回真。
 */
static bool typeAngle(const std::string &text, std::size_t at)
{
    while (at && std::isspace(static_cast<unsigned char>(text[at - 1]))) --at;
    if (at && text[at - 1] == '@') return true;
    const auto end = at;
    while (at && identifierChar(text[at - 1])) --at;
    return at < end && ((at && text[at - 1] == '$') || text.substr(at, end - at) == "fn" || std::isupper(static_cast<unsigned char>(text[at])));
}

/**
 * @brief 检查区间是否含有空白及注释之外的词素，区分空组与空实参。
 * @param text 完整源码。
 * @param begin 区间起点。
 * @param end 区间终点。
 * @return 含有实际词素时返回真。
 */
static bool hasCode(const std::string &text, std::size_t begin, std::size_t end)
{
    while (begin < end)
    {
        if (std::isspace(static_cast<unsigned char>(text[begin])))
            ++begin;
        else if (text.compare(begin, 2, "//") == 0 || text.compare(begin, 2, "/*") == 0)
            skipQuoted(text, begin);
        else
            return true;
    }
    return false;
}

/**
 * @brief 读取一个平衡括号组并在最外层逗号处划分实参。
 * @param text 完整原文。
 * @param at 组起点，成功时移到结束括号之后。
 * @param ranges 接收每个实参的半开区间。
 * @param error 接收不配对错误。
 * @return 读取成功返回真。
 */
static bool readGroup(const std::string &text, std::size_t &at, std::vector<std::pair<std::size_t, std::size_t>> &ranges, std::string &error)
{
    const auto closing = [](char ch) { return ch == '(' ? ')' : ch == '[' ? ']' :
                                                            ch == '{'     ? '}' :
                                                                            '>'; };
    std::vector<char> stack{ closing(text[at++]) };
    std::size_t begin = at;
    while (at < text.size())
    {
        if (skipQuoted(text, at)) continue;
        const char ch = text[at];
        if (ch == '(' || ch == '[' || ch == '{' || (ch == '<' && typeAngle(text, at)))
            stack.push_back(closing(ch));
        else if (ch == stack.back())
        {
            stack.pop_back();
            if (stack.empty())
            {
                if (hasCode(text, begin, at) || !ranges.empty()) ranges.emplace_back(begin, at);
                ++at;
                return true;
            }
        }
        else if (ch == ')' || ch == ']' || ch == '}')
        {
            error = "宏实参的括号不配对";
            return false;
        }
        else if (ch == ',' && stack.size() == 1)
        {
            ranges.emplace_back(begin, at);
            begin = at + 1;
        }
        ++at;
    }
    error = "宏实参组缺少结束括号";
    return false;
}

/**
 * @brief 捕获宏调用的词法组，不展开宏体，不把原文作为生成代码替换。
 * @param source 待归一化源码。
 * @param error 接收错误。
 * @param firstLine 原文首行。
 * @param firstColumn 原文首列。
 * @param anchored 是否保留可被模板复制的内部调用编号。
 * @return 捕获成功返回真。
 */
bool SourceFragments::capture(Memory::String &source, std::string &error, std::size_t firstLine, std::size_t firstColumn, bool anchored)
{
    const std::string text(source.data(), source.size());
    std::vector<std::size_t> lines{ 0 };
    for (std::size_t index = 0; index < text.size(); ++index)
        if (text[index] == '\n') lines.push_back(index + 1);
    const auto origin = [&](std::size_t at) {
        const auto row = static_cast<std::size_t>(std::upper_bound(lines.begin(), lines.end(), at) - lines.begin() - 1);
        return std::make_pair(firstLine + row, at - lines[row] + (row ? 1 : firstColumn));
    };
    const auto fragment = [&](std::pair<std::size_t, std::size_t> range) {
        const auto location = origin(range.first);
        return SourceFragment{ text.substr(range.first, range.second - range.first), location.first, location.second };
    };
    std::string output;
    error.clear();
    const auto saved = invocations_.size();
    activeBegin_ = saved;
    active_ = true;
    for (std::size_t at = 0; at < text.size();)
    {
        const auto start = at;
        if (skipQuoted(text, at))
        {
            output.append(text, start, at - start);
            continue;
        }
        if (text[at] != '$' || at + 1 == text.size() || !(std::isalpha(static_cast<unsigned char>(text[at + 1])) || text[at + 1] == '_'))
        {
            output += text[at++];
            continue;
        }
        ++at;
        while (at < text.size() && identifierChar(text[at])) ++at;
        /* 限定名宏调用（`$std::lang::assert`）：模块路径要一起吃进来。否则片段捕获把
         * `$std` 当成完整名字、在它后面插 `@!编号`，`::lang::assert` 留在原文里挨着，
         * 词法只能切成「宏名 + 作用域」两个 token，解析必然报错。 */
        while (at + 1 < text.size() && text[at] == ':' && text[at + 1] == ':' &&
               (std::isalpha(static_cast<unsigned char>(text[at + 2])) || text[at + 2] == '_'))
        {
            at += 2;
            while (at < text.size() && identifierChar(text[at])) ++at;
        }
        const auto name = text.substr(start + 1, at - start - 1);
        if (text.compare(at, 2, "@!") == 0)
        {
            at += 2;
            while (at < text.size() && std::isdigit(static_cast<unsigned char>(text[at]))) ++at;
            output.append(text, start, at - start);
            continue;
        }
        const bool control = name == "macro" || name == "replace" || name == "end_macro" || name == "match" || name == "else" || name == "end_match" || name == "regex" || name == "end_regex" || name == "for" || name == "end_for";
        if (control)
        {
            output.append(text, start, at - start);
            continue;
        }
        SourceInvocation call;
        call.name = name;
        const auto location = origin(start);
        call.line = location.first;
        call.column = location.second;
        /**
         * @brief 保存公开括号的词法范围，供生成解析器读取真实的组结构。
         */
        struct GroupSegment
        {
            std::size_t begin, end, count;
            char opening;
        };
        std::vector<GroupSegment> segments;
        for (;;)
        {
            auto next = at;
            for (;;)
            {
                while (next < text.size() && std::isspace(static_cast<unsigned char>(text[next]))) ++next;
                if (text.compare(next, 2, "//") != 0 && text.compare(next, 2, "/*") != 0) break;
                skipQuoted(text, next);
            }
            if (next == text.size()) break;
            const char opening = text[next];
            const bool types = opening == '<' && call.groups.empty() && !call.explicitTypes;
            if (!types && opening != '(' && opening != '[' && opening != '{') break;
            std::vector<std::pair<std::size_t, std::size_t>> ranges;
            const auto groupBegin = next;
            if (!readGroup(text, next, ranges, error))
            {
                invocations_.resize(saved);
                active_ = false;
                return false;
            }
            segments.push_back({ groupBegin, next, ranges.size(), opening });
            if (types)
            {
                call.explicitTypes = true;
                for (const auto range : ranges) call.types.push_back(fragment(range));
            }
            else
            {
                SourceGroup group;
                group.opening = opening;
                for (const auto range : ranges) group.arguments.push_back(fragment(range));
                call.groups.push_back(std::move(group));
            }
            at = next;
        }
        invocations_.push_back(std::move(call));
        const auto endName = start + name.size() + 1;
        output.append(text, start, endName - start);
        if (anchored)
        {
            const auto reference = std::to_string(invocations_.size() - 1);
            output += "@!" + reference;
            std::size_t previous = endName, argument = 0;
            for (const auto &segment : segments)
            {
                output.append(text, previous, segment.begin - previous);
                output += segment.opening;
                for (std::size_t index = 0; index < segment.count; ++index)
                {
                    if (index) output += ',';
                    output += "@!macrofragment" + reference + "_" + std::to_string(argument++);
                }
                output += segment.opening == '(' ? ')' : segment.opening == '[' ? ']' :
                                                     segment.opening == '{'     ? '}' :
                                                                                  '>';
                for (auto index = segment.begin; index < segment.end; ++index)
                    if (text[index] == '\n' || text[index] == '\r') output += text[index];
                previous = segment.end;
            }
        }
        else
            for (auto index = endName; index < at; ++index)
                output += text[index] == '\n' || text[index] == '\r' ? text[index] : ' ';
    }
    source.assign(output.data(), output.size());
    return true;
}

/**
 * @brief 整体移动宏调用和全部原始片段的来源索引，仍保留各自真实列号。
 * @param offset 保存原始文件映射的追加区起点。
 */
void SourceFragments::rebase(std::size_t offset)
{
    for (auto &call : invocations_)
    {
        call.line += offset;
        for (auto &type : call.types) type.line += offset;
        for (auto &group : call.groups)
            for (auto &argument : group.arguments) argument.line += offset;
    }
}

/**
 * @brief 验证内部编号确实来自当前会话，不根据源码中伪造的编号越界访问。
 * @param token 编号词素。
 * @return 校验成功的记录编号，无效返回 size_t 最大值。
 */
std::size_t SourceFragments::resolveMarker(std::string_view token) const
{
    const auto marker = token.find("@!");
    const auto invalid = static_cast<std::size_t>(-1);
    if (marker == std::string_view::npos || marker < 2 || token[0] != '$' || marker + 2 == token.size()) return invalid;
    std::size_t value = 0;
    for (auto index = marker + 2; index < token.size(); ++index)
    {
        const char ch = token[index];
        if (ch < '0' || ch > '9' || value > (invalid - static_cast<std::size_t>(ch - '0')) / 10) return invalid;
        value = value * 10 + static_cast<std::size_t>(ch - '0');
    }
    return value < invocations_.size() && token.substr(1, marker - 1) == invocations_[value].name ? value : invalid;
}
}  // namespace chali_macro
