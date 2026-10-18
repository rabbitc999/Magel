#include "magel_i18n.hpp"
#include "allocator/magel_allocator.hpp"
#ifndef MAGEL_MACRO_HPP
#define MAGEL_MACRO_HPP

#include "preprocess.hpp"
#include "magel_interpolation.hpp"

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cstdlib>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace magel_macro
{

/**
 * @brief 定义中一组独立的文本参数，保留括号种类与局部变长尾参。
 */
struct ParameterGroup
{
    char opening = '(';
    Memory::Vector<Memory::String> params;
    Memory::String rest;
};

/**
 * @brief 保存宏的原始文本、调用分组和既有表达式保护规则。
 */
struct Macro
{
    Memory::Vector<ParameterGroup> groups;
    Memory::Vector<Memory::String> params;
    /**
     * 使用 `(参数名)` 标注或处于 expr 宏中的形参：每次出现在宏体里都替换成 `(实参)`，给表达式宏当护栏。
     */
    Memory::Set<Memory::String> parenthesized;
    bool functionLike = false;
    bool variadic = false;
    Memory::String variadicParam;
    /**
     * 变长尾参使用 `...(rest)` 标注或处于 expr 宏中：替换时给**每个**尾参元素各包一层括号
     * （`rest` → `(a), (b), (c)`），`$for v in rest` 的循环变量也跟着变成 `(a)`。
     */
    bool parenthesizeVariadic = false;
    /**
     * `$macro expr NAME(…)`：整段展开及所有实参加括号。
     */
    bool parenthesizeResult = false;
    Memory::String body;
    Memory::String debugName;
};

/**
 * @brief 编码内部宏来源标记；解析前移除。
 * @param origin 原始位置。
 * @param column 原文中从零计数的列偏移。
 * @return 标记文本。
 */
inline Memory::String debugMarker(const preprocess::LineOrigin &origin, std::size_t column = 0)
{
    const char *digits = "0123456789abcdef";
    Memory::String hex;
    for (unsigned char c : origin.file)
    {
        hex += digits[c >> 4];
        hex += digits[c & 15];
    }
    return "/*__magel_source:" + Memory::toString(origin.line) + ":" + hex + ":" + Memory::toString(column) + "__*/";
}

/**
 * @brief 清除拼接边界上的内部标记，保持 token 相邻。
 * @param text 展开文本。
 * @param all 是否清除所有标记。
 * @return 语义不变的文本。
 */
inline Memory::String cleanPasteMarkers(const Memory::String &text, bool all = false)
{
    Memory::String result;
    for (std::size_t i = 0; i < text.size();)
    {
        if (text[i] == '"' || text[i] == '\'')
        {
            const char quote = text[i];
            result += text[i++];
            while (i < text.size())
            {
                const char c = text[i++];
                result += c;
                if (c == '\\' && i < text.size())
                    result += text[i++];
                else if (c == quote)
                    break;
            }
            continue;
        }
        if (text.compare(i, sizeof("/*__magel_source:") - 1u, "/*__magel_source:") == 0)
        {
            const auto end = text.find("__*/", i + sizeof("/*__magel_source:") - 1u);
            if (end != Memory::String::npos)
            {
                std::size_t before = result.size();
                while (before && std::isspace(static_cast<unsigned char>(result[before - 1]))) --before;
                std::size_t after = end + 4;
                while (after < text.size() && std::isspace(static_cast<unsigned char>(text[after]))) ++after;
                /* `##` 已从语言删除，标记不再需要为拼接让路：只有 all 才清。 */
                if (!all) result += text.substr(i, end + 4 - i);
                i = end + 4;
                continue;
            }
        }
        result += text[i++];
    }
    return result;
}

inline bool identStart(char ch)
{
    return std::isalpha(static_cast<unsigned char>(ch)) || ch == '_';
}

inline bool identPart(char ch)
{
    return std::isalnum(static_cast<unsigned char>(ch)) || ch == '_';
}

inline void spaces(const Memory::String &s, std::size_t &i)
{
    while (i < s.size() && std::isspace(static_cast<unsigned char>(s[i]))) ++i;
}

inline Memory::String trim(const Memory::String &text)
{
    std::size_t begin = 0;
    std::size_t end = text.size();
    while (begin < end && std::isspace(static_cast<unsigned char>(text[begin]))) ++begin;
    while (end > begin && std::isspace(static_cast<unsigned char>(text[end - 1]))) --end;
    return text.substr(begin, end - begin);
}

inline Memory::String stringify(const Memory::String &text)
{
    const Memory::String value = trim(text);
    Memory::String out = "\"";
    for (char ch : value)
    {
        switch (ch)
        {
        case '\\': out += "\\\\"; break;
        case '"': out += "\\\""; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        case '\t': out += "\\t"; break;
        default: out += ch; break;
        }
    }
    out += '"';
    return out;
}

/**
 * @brief 判断文本是不是**单个**标识符（调用方先去掉首尾空白）。
 *
 * `$concat` 的每个实参都要满足它：`$` 后缀的拼接只拼标识符，不拼任意文本，
 * 所以实参类型在宏展开阶段就能查出来。
 *
 * @param text 待检查文本（应已去掉首尾空白）。
 * @return 是单个标识符返回 true。
 */
inline bool identifierText(const Memory::String &text)
{
    if (text.empty() || !identStart(text[0])) return false;
    for (std::size_t i = 1; i < text.size(); ++i)
        if (!identPart(text[i])) return false;
    return true;
}

inline Memory::String origin(const preprocess::Result &result, std::size_t line)
{
    if (line < result.lineMap.size())
    {
        const auto &at = result.lineMap[line];
        return at.file + " 第 " + Memory::toString(at.line) + " 行";
    }
    return "第 " + Memory::toString(line + 1) + " 行";
}

/**
 * @brief 解析 `$macro` / `$replace` 头的三种写法，并填好形态。
 *
 * | 写法 | 形态 | 调用 |
 * | --- | --- | --- |
 * | `$macro NAME()` | 对象宏 | 裸写 `NAME`（**不带**括号） |
 * | `$macro NAME(a, b)` | 定参宏 | `NAME(x, y)` |
 * | `$macro NAME(x, ...rest)` | 变长宏 | `NAME(x, y, z)` |
 * | `$macro NAME(x, ...(rest))` | 变长宏（元素加括号） | `NAME(x, y, z)` → `x, (y), (z)` |
 *
 * `$replace NAME(x, y) 宏体` 是**单行宏**：宏体就是头这同一行剩下的内容，没有 `$end_macro`，
 * 无形参的对象宏可省括号（`$replace NAME 宏体`）；行内规则（`expr` / 参数括号 / `$stringify` /
 * `$concat` / `$line`…）都支持，但装不下 `$for` 这类块。
 *
 * 表达式括号规则：`$macro expr ADD(a, b)` 为结果和全部实参加括号；
 * expr 宏允许 `(a)` 冗余标注，与 `a` 完全等价。普通宏使用 `(a)` 显式标注，
 * `...(rest)` 为每个尾参元素分别加括号，`$for` 循环变量沿用该标注。
 * 定义端标注不改变调用形式：`$ADD(x + 1, y * 2)`。
 *
 * 拼接与字符串化推荐写结构化形式：`$concat(a, b)` 只拼标识符，结果是标识符，不重新参与
 * 宏展开；`$stringify(expr)` 捕获实参原文，结果是字符串字面量。两者都用**原始**实参、
 * 不加括号。
 *
 * `$macro` 的括号不能省：括号里空着表示对象宏，有形参表示函数式宏。
 * `$replace` 的对象宏可以省括号。
 *
 * @param line 整行源码。
 * @param name 输出：宏名。
 * @param macro 输出：形态（含形参、变长尾参）。
 * @param problem 输出：失败原因（成功时为空串）。
 * @param inlineBody 非空时走单行宏（`$replace`）：头这一行剩下的内容当宏体收下；
 *        为空指针时走多行宏（`$macro`）：头这一行后面不能有多余内容。
 * @return 成功返回 true。
 */

/**
 * @brief 完成形参表解析，或收取单行替换宏的宏体。
 * @param line 声明源码。
 * @param i 参数表后的位置。
 * @param problem 输出声明尾部的错误。
 * @param inlineBody 非空时接收剩余文本，否则禁止多余内容。
 * @retval 声明尾部有效返回 true。
 */
inline bool headerTail(const Memory::String &line, std::size_t i, Memory::String &problem,
                       Memory::String *inlineBody)
{
    problem.clear();
    if (i == line.size())
    {
        if (inlineBody != nullptr) inlineBody->clear();
        return true;
    }
    if (inlineBody == nullptr)
    {
        problem = "`$macro` 头这一行只能写宏名和形参表：宏体写在下一行，用 `$end_macro` 收尾";
        return false;
    }
    *inlineBody = trim(line.substr(i));
    return true;
}
/**
 * @brief 保持单圆括号宏的对象形态、表达式保护和三点尾参解析。
 * @param line 声明源码。
 * @param name 输出宏名。
 * @param macro 输出原有参数形态。
 * @param problem 输出声明错误。
 * @param inlineBody 非空时接收单行宏体。
 * @param keyword 当前声明关键字。
 * @retval 解析成功返回 true。
 */
inline bool legacyHeader(const Memory::String &line, Memory::String &name, Macro &macro,
                         Memory::String &problem, Memory::String *inlineBody = nullptr,
                         const Memory::String &keyword = Memory::String("$macro"))
{
    problem.clear();
    const std::size_t at = line.find(keyword);
    if (at == Memory::String::npos)
    {
        problem = "找不到 `" + keyword + "`";
        return false;
    }
    std::size_t i = at + keyword.size();
    spaces(line, i);
    /* expr 同时为展开结果和全部形参提供括号保护，显式形参括号仍允许。 */
    if (line.compare(i, 4, "expr") == 0 && i + 4 < line.size() && std::isspace(static_cast<unsigned char>(line[i + 4])))
    {
        macro.parenthesizeResult = true;
        i += 4;
        spaces(line, i);
    }
    if (i == line.size() || !identStart(line[i]))
    {
        problem = "`" + keyword + "` 后面要写宏名";
        return false;
    }
    const std::size_t begin = i++;
    while (i < line.size() && identPart(line[i])) ++i;
    name = line.substr(begin, i - begin);
    spaces(line, i);
    if (i == line.size() || line[i] != '(')
    {
        /* `$replace NAME 宏体`：单行宏可以省括号，直接就是对象宏。 */
        if (inlineBody != nullptr)
        {
            if (i == line.size())
            {
                problem = "`$replace` 是单行宏，宏名后面要写宏体；多行宏请用 `$macro … $end_macro`";
                return false;
            }
            *inlineBody = trim(line.substr(i));
            problem.clear();
            return true;
        }
        problem = "宏声明缺少参数括号：`" + keyword + " " + name + "()`"
                                                                   "（括号里空着就是对象宏，调用处写 `"
                  + name + "`，不带括号）";
        return false;
    }
    ++i;
    spaces(line, i);
    if (i < line.size() && line[i] == ')')
    {
        ++i; /* `()` = 没有形参 = 对象宏 */
        spaces(line, i);
        return headerTail(line, i, problem, inlineBody);
    }
    macro.functionLike = true;
    problem = "形参表写法不对：`" + keyword + " NAME(a, b)` 或 `" + keyword + " NAME(x, ...rest)`";
    for (;;)
    {
        const bool variadic = line.compare(i, 3, "...") == 0;
        if (variadic)
        {
            i += 3;
            spaces(line, i);
        }
        const bool annotated = i < line.size() && line[i] == '(';
        if (annotated)
        {
            ++i;
            spaces(line, i);
        }
        if (i == line.size() || !identStart(line[i])) return false;
        const std::size_t p = i++;
        while (i < line.size() && identPart(line[i])) ++i;
        const Memory::String param = line.substr(p, i - p);
        for (const auto &existing : macro.params)
            if (existing == param) return false;
        spaces(line, i);
        if (annotated)
        {
            if (i == line.size() || line[i++] != ')') return false;
            spaces(line, i);
        }
        if (variadic)
        {
            macro.variadicParam = param;
            macro.variadic = true;
            macro.parenthesizeVariadic = annotated || macro.parenthesizeResult;
            if (i == line.size() || line[i++] != ')') return false;
            break;
        }
        macro.params.push_back(param);
        if (annotated || macro.parenthesizeResult) macro.parenthesized.insert(param);
        spaces(line, i);
        if (i < line.size() && line[i] == ')')
        {
            ++i;
            break;
        }
        if (i == line.size() || line[i++] != ',') return false;
        spaces(line, i);
    }
    spaces(line, i);
    return headerTail(line, i, problem, inlineBody);
}

/**
 * @brief 返回文本分组左括号对应的右括号。
 * @param ch 待检查字符。
 * @retval 右括号；非左括号返回零。
 */
inline char groupClose(char ch)
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
inline bool groupArguments(const Memory::String &text, std::size_t open,
                           Memory::Vector<Memory::String> &parts, std::size_t &end,
                           bool allowIncomplete = false, bool *closed = nullptr)
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

/**
 * @brief 在原有宏声明解析上扩展有序括号组与每组独立的双点尾参。
 * @param line 声明行。
 * @param name 输出宏名。
 * @param macro 输出宏参数定义。
 * @param problem 输出分组或声明错误。
 * @param inlineBody 单行替换宏的输出宏体。
 * @param keyword 当前声明关键字。
 * @retval 声明有效返回 true。
 */
inline bool header(const Memory::String &line, Memory::String &name, Macro &macro,
                   Memory::String &problem, Memory::String *inlineBody = nullptr,
                   const Memory::String &keyword = Memory::String("$macro"))
{
    std::size_t i = line.find(keyword) + keyword.size();
    spaces(line, i);
    if (line.compare(i, 5, "expr ") == 0)
    {
        i += 5;
        spaces(line, i);
    }
    if (i >= line.size() || !identStart(line[i]))
        return legacyHeader(line, name, macro, problem, inlineBody, keyword);
    const std::size_t nameBegin = i++;
    while (i < line.size() && identPart(line[i])) ++i;
    name = line.substr(nameBegin, i - nameBegin);
    spaces(line, i);
    const std::size_t first = i;
    Memory::Vector<ParameterGroup> groups;
    Memory::Set<Memory::String> names;
    while (i < line.size() && groupClose(line[i]))
    {
        ParameterGroup group;
        group.opening = line[i];
        Memory::Vector<Memory::String> declarations;
        std::size_t end = i;
        if (!groupArguments(line, i, declarations, end))
        {
            problem = "宏Token分组不匹配";
            return false;
        }
        for (std::size_t n = 0; n < declarations.size(); ++n)
        {
            auto param = trim(declarations[n]);
            const bool rest = param.compare(0, 2, "..") == 0;
            if (rest) param = trim(param.substr(param.compare(0, 3, "...") == 0 ? 3 : 2));
            if (!identifierText(param) || !names.insert(param).second || (rest && n + 1 != declarations.size()))
            {
                // 单圆括号原有的表达式保护标注仍交给原解析逻辑。
                return legacyHeader(line, name, macro, problem, inlineBody, keyword);
            }
            if (rest)
                group.rest = param;
            else
                group.params.push_back(param);
        }
        groups.push_back(std::move(group));
        i = end;
        spaces(line, i);
    }
    const bool structured = groups.size() > 1 || (!groups.empty() && groups[0].opening != '(')
                            || (line.find("..", first) != Memory::String::npos && line.find("...", first) == Memory::String::npos);
    if (!structured) return legacyHeader(line, name, macro, problem, inlineBody, keyword);
    macro.groups = std::move(groups);
    macro.functionLike = true;
    for (const auto &group : macro.groups)
    {
        for (const auto &param : group.params) macro.params.push_back(param);
        if (!group.rest.empty()) macro.params.push_back(group.rest);
    }
    return headerTail(line, i, problem, inlineBody);
}

inline bool directive(const Memory::String &line, const Memory::String &word)
{
    std::size_t i = 0;
    spaces(line, i);
    return line.compare(i, word.size(), word) == 0 && (i + word.size() == line.size() || !identPart(line[i + word.size()]));
}

/**
 * @brief 比较两份宏定义的文本调用形态，忽略形参名称。
 * @param left 已有宏。
 * @param right 新宏。
 * @retval 括号顺序、每组元数及尾参形态相同返回 true。
 */
inline bool sameShape(const Macro &left, const Macro &right)
{
    if (left.groups.size() != right.groups.size()) return false;
    if (left.groups.empty()) return left.params.size() == right.params.size() && left.variadic == right.variadic;
    for (std::size_t n = 0; n < left.groups.size(); ++n)
    {
        const auto &a = left.groups[n];
        const auto &b = right.groups[n];
        if (a.opening != b.opening || a.params.size() != b.params.size() || a.rest.empty() != b.rest.empty()) return false;
    }
    return true;
}

/**
 * @brief 找出当前行开始的结构化宏调用所占源码范围。
 * @param text 完整源码。
 * @param start 当前行起点。
 * @param stop 当前行终点，不含换行。
 * @param macros 当前可见宏。
 * @retval 返回应一起送入文本展开的末尾位置。
 * @note 只扫描字符、注释和引号，不调用词法器或解析器。
 */
inline std::size_t groupedLineEnd(const Memory::String &text, std::size_t start, std::size_t stop,
                                  const Memory::Map<Memory::String, Memory::Vector<Macro>> &macros)
{
    for (std::size_t i = start; i < stop; ++i)
    {
        if (text.compare(i, 2, "//") == 0)
        {
            auto next = text.find('\n', i + 2);
            if (next == Memory::String::npos || next >= stop) break;
            i = next;
            continue;
        }
        if (text.compare(i, 2, "/*") == 0)
        {
            auto next = text.find("*/", i + 2);
            if (next == Memory::String::npos) break;
            i = next + 1;
            continue;
        }
        if (text[i] == '\"' || text[i] == '\'')
        {
            const char quote = text[i];
            while (++i < stop)
            {
                if (text[i] == '\\' && i + 1 < stop)
                    ++i;
                else if (text[i] == quote)
                    break;
            }
            continue;
        }
        if (text[i] != '$') continue;
        std::size_t end = i + 1;
        while (end < text.size() && identPart(text[end])) ++end;
        auto found = macros.find(text.substr(i + 1, end - i - 1));
        if (found == macros.end()) continue;
        for (const auto &macro : found->second)
        {
            if (macro.groups.empty()) continue;
            std::size_t cursor = end;
            bool matched = true;
            for (const auto &group : macro.groups)
            {
                spaces(text, cursor);
                Memory::Vector<Memory::String> ignored;
                if (cursor >= text.size() || text[cursor] != group.opening || !groupArguments(text, cursor, ignored, cursor))
                {
                    matched = false;
                    break;
                }
            }
            if (matched && cursor > stop)
            {
                stop = text.find('\n', cursor);
                if (stop == Memory::String::npos) stop = text.size();
            }
        }
    }
    return stop;
}

/**
 * @brief 宏名不能占用的内部 DSL 关键字（`$macro` / `$replace` / `$for` / `$concat` …）。
 *
 * 这些词在 `$` 指令里有固定含义，拿来当宏名会让 `$for(…)` 这类调用点无法区分，所以
 * 定义期直接拒绝；`$line` / `$file` / `$module` 与 `$concat` / `$stringify` 也一并占住。
 */
inline bool macroDslKeyword(const Memory::String &name)
{
    static const char *const words[] = {
        "macro",
        "replace",
        "delete_macro",
        "end_macro",
        "for",
        "end_for",
        "line",
        "file",
        "module",
        "concat",
        "stringify",
        "if",
        "elif",
        "else",
        "end_if",
        "let",
        "argc",
        "count",
        "typecheck",
    };
    for (const char *word : words)
        if (name == word) return true;
    return false;
}

/**
 * 找单行宏体里出现的块指令（`$for` 及它的 `$end_for`，外加 `$end_macro`）。
 * `$replace` 的宏体只有一行，装不下这些块；找到就报错让用户改回 `$macro`。
 * 字符串和注释里的字面 `$for` 不算。
 */
inline bool findBlockDirective(const Memory::String &text, Memory::String &found)
{
    static const char *const words[] = { "$for", "$end_for", "$end_macro", "$if", "$elif", "$else", "$end_if", "$let" };
    bool blockComment = false;
    for (std::size_t i = 0; i < text.size();)
    {
        if (blockComment)
        {
            if (text.compare(i, 2, "*/") == 0)
            {
                blockComment = false;
                i += 2;
                continue;
            }
            ++i;
            continue;
        }
        if (text.compare(i, 2, "/*") == 0)
        {
            blockComment = true;
            i += 2;
            continue;
        }
        if (text.compare(i, 2, "//") == 0) break;
        if (text[i] == '"' || text[i] == '\'')
        {
            const char quote = text[i++];
            while (i < text.size())
            {
                if (text[i] == '\\' && i + 1 < text.size())
                {
                    i += 2;
                    continue;
                }
                if (text[i++] == quote) break;
            }
            continue;
        }
        if (text[i] != '$' || (i > 0 && identPart(text[i - 1])))
        {
            ++i;
            continue;
        }
        for (const char *word : words)
        {
            std::size_t length = 0;
            while (word[length] != '\0') ++length;
            if (text.compare(i, length, word) == 0 && (i + length == text.size() || !identPart(text[i + length])))
            {
                found = word;
                return true;
            }
        }
        ++i;
    }
    return false;
}

/**
 * @brief 使用同一字符分组器读取原有圆括号调用与内建文本操作。
 * @param s 调用文本。
 * @param open 左括号位置。
 * @param args 输出当前层的原始参数。
 * @param end 输出右括号后的偏移。
 * @retval 分组成功返回 true。
 */
inline bool arguments(const Memory::String &s, std::size_t open,
                      Memory::Vector<Memory::String> &args, std::size_t &end)
{
    return groupArguments(s, open, args, end);
}

/**
 * @brief 展开 `$concat(…)` / `$stringify(…)`。
 *
 * 两者和 `$line` / `$file` / `$module` 一样带 `$` 前缀，普通代码与宏体内都能用；
 * 旧的 `##` 拼接与 `#形参` 字符串化已在 0.16.0 删除，只留这两个结构化形式。
 *
 * - `$concat(A, B)`：两个实参各解析成一段文本（宏体内是形参名就取调用点的实参文本，
 *   否则就是写下的字面标识符），二者都必须整体是一个标识符；结果是拼出来的标识符，
 *   **不重新参与宏展开**。
 * - `$stringify(X)`：X 在宏体内是形参名时取调用点的**原始**实参文本，否则取写下的原文；
 *   结果是一个字符串字面量（内部来源标记不会进字符串）。
 *
 * 检查放在展开阶段：写错的实参在宏被调用时立刻报错，不需要宏体再往下走。
 *
 * @param text 正在扫描的文本（普通代码行或宏体）。
 * @param at `$` 的位置。
 * @param keyword `"$concat"` 或 `"$stringify"`。
 * @param values 宏形参 → 展开后的实参文本；宏体外为空指针。
 * @param rawValues 宏形参 → 调用点的原始实参文本；宏体外为空指针。
 * @param output 输出：结果追加到这里。
 * @param end 输出：`)` 之后的位置。
 * @param problem 输出：失败原因（成功时为空串）。
 * @return 成功返回 true。
 */
inline Memory::String substituteMacroLocals(const Memory::String &text,
                                            const Memory::Map<Memory::String, Memory::String> &values);
inline bool expandIntrinsic(const Memory::String &text, std::size_t at,
                            const Memory::String &keyword,
                            const Memory::Map<Memory::String, Memory::String> *values,
                            const Memory::Map<Memory::String, Memory::String> *rawValues,
                            Memory::String &output, std::size_t &end,
                            Memory::String &problem)
{
    problem.clear();
    /* 宏体内写形参名就取调用点的实参文本，字面标识符原样保留。 */
    const auto piece = [&](const Memory::String &argument, bool wantRaw) {
        const Memory::String name = trim(argument);
        const Memory::Map<Memory::String, Memory::String> *table = wantRaw ? rawValues : values;
        if (table == nullptr) return name;
        const auto found = table->find(name);
        if (found == table->end()) return name;
        return wantRaw ? found->second : trim(cleanPasteMarkers(found->second, true));
    };
    std::size_t open = at + keyword.size();
    while (open < text.size() && std::isspace(static_cast<unsigned char>(text[open]))) ++open;
    if (open == text.size() || text[open] != '(')
    {
        problem = keyword + " 后面要写实参：`" + keyword + "(…)`";
        return false;
    }
    Memory::Vector<Memory::String> args;
    if (!arguments(text, open, args, end))
    {
        problem = keyword + " 的实参表缺少 `)`";
        return false;
    }
    if (keyword == "$stringify")
    {
        if (args.size() != 1u)
        {
            problem = "$stringify 只接受一个表达式，给了 " + Memory::toString(args.size()) + " 个实参";
            return false;
        }
        /* 内部来源标记只在编译器里用；跨行写实参时可能落在捕获文本中间，不能进字符串。 */
        output += stringify(cleanPasteMarkers(magel_interpolation::rawText(piece(args[0], true)), true));
        return true;
    }
    if (keyword == "$count")
    {
        if (args.size() != 1)
        {
            output += Memory::toString(args.size());
            return true;
        }
        Memory::Vector<Memory::String> items;
        std::size_t ignored;
        arguments("(" + piece(args[0], false) + ")", 0, items, ignored);
        output += Memory::toString(items.size());
        return true;
    }
    if (keyword == "$typecheck")
    {
        if (args.size() != 2)
        {
            problem = "$typecheck 需要类型和表达式两个实参";
            return false;
        }
        auto expression = piece(args[1], false);
        auto type = piece(args[0], false);
        if (values)
        {
            expression = substituteMacroLocals(expression, *values);
            type = substituteMacroLocals(type, *values);
        }
        output += "__magel_typecheck(" + type + ",(" + expression + "))";
        return true;
    }
    if (args.size() != 2u)
    {
        problem = "$concat 只接受两个标识符，给了 " + Memory::toString(args.size()) + " 个实参";
        return false;
    }
    Memory::String joined;
    for (const Memory::String &argument : args)
    {
        const Memory::String item = piece(argument, false);
        if (!identifierText(item))
        {
            problem = "$concat 的实参必须是标识符，得到 `" + item + "`";
            return false;
        }
        joined += item;
    }
    output += joined;
    return true;
}

/**
 * @brief 把宏体里的 `$for NAME in <尾参名>` … `$end_for` 按尾参逐个重复放开。
 *
 * 只能遍历**变长尾参**（`...rest`）：一次一个实参，用**展开后**的实参文本（和 `rest` 一致），
 * 循环变量按标识符整体替换，每次迭代自成一行。体里还能再嵌 `$for` —— 从最内层开始逐层展开。
 *
 * @param body 宏体（原地替换）。
 * @param variadicParam 变长尾参名；宏不是变长宏时为空串（此时任何 `$for` 都报错）。
 * @param tail 展开后的尾部实参，按顺序。
 * @param parenthesizeElement 尾参使用括号标注或处于 expr 宏中时，循环变量每次替换成 `(实参)`。
 * @param error 出错信息。
 * @return 成功返回 true。
 */
inline bool expandFors(Memory::String &body, const Memory::String &variadicParam,
                       const Memory::Vector<Memory::String> &tail,
                       bool parenthesizeElement, Memory::String &error)
{
    for (int round = 0; round < 64; ++round)
    {
        const std::size_t at = body.find("$for");
        if (at == Memory::String::npos)
        {
            if (body.find("$end_for") != Memory::String::npos)
            {
                error = "`$end_for` 没有配对的 `$for`";
                return false;
            }
            return true;
        }
        std::size_t i = at + 4;
        spaces(body, i);
        if (i == body.size() || !identStart(body[i]))
        {
            error = "`$for` 后面要写循环变量名";
            return false;
        }
        const std::size_t nameBegin = i++;
        while (i < body.size() && identPart(body[i])) ++i;
        const Memory::String loopVar = body.substr(nameBegin, i - nameBegin);
        spaces(body, i);
        if (body.compare(i, 2, "in") != 0 || (i + 2 < body.size() && identPart(body[i + 2])))
        {
            error = "`$for` 的写法是 `$for 变量 in 尾参名`";
            return false;
        }
        i += 2;
        spaces(body, i);
        if (i == body.size() || !identStart(body[i]))
        {
            error = "`$for` 后面要写尾参名";
            return false;
        }
        const std::size_t paramBegin = i++;
        while (i < body.size() && identPart(body[i])) ++i;
        const Memory::String loopParam = body.substr(paramBegin, i - paramBegin);
        while (i < body.size() && body[i] != '\n') ++i; /* 行尾（可以跟注释）忽略 */
        if (i < body.size()) ++i;
        if (loopParam != variadicParam)
        {
            error = "`$for " + loopVar + " in " + loopParam + "` 只能遍历变长尾参" + (variadicParam.empty() ? "（这个宏没有 `...尾参`）" : "（这里是 `..." + variadicParam + "`）");
            return false;
        }
        /* 找配对的 `$end_for`：把里面嵌套的 `$for` 一起数进去。 */
        std::size_t scan = i;
        int depth = 0;
        std::size_t loopEnd = Memory::String::npos;
        std::size_t afterEnd = Memory::String::npos;
        while (scan < body.size())
        {
            const std::size_t lineEnd = body.find('\n', scan);
            const std::size_t stop = lineEnd == Memory::String::npos ? body.size() : lineEnd;
            const Memory::String line = body.substr(scan, stop - scan);
            if (directive(line, "$for"))
                ++depth;
            else if (directive(line, "$end_for"))
            {
                if (depth == 0)
                {
                    loopEnd = scan;
                    afterEnd = lineEnd == Memory::String::npos ? stop : lineEnd + 1;
                    break;
                }
                --depth;
            }
            if (lineEnd == Memory::String::npos) break;
            scan = lineEnd + 1;
        }
        if (loopEnd == Memory::String::npos)
        {
            error = "`$for` 缺少配对的 `$end_for`";
            return false;
        }
        const Memory::String loopBody = body.substr(i, loopEnd - i);
        Memory::String expanded;
        for (const Memory::String &argument : tail)
        {
            Memory::String piece;
            for (std::size_t b = 0; b < loopBody.size();)
            {
                if (!identStart(loopBody[b]))
                {
                    piece += loopBody[b++];
                    continue;
                }
                const std::size_t begin = b++;
                while (b < loopBody.size() && identPart(loopBody[b])) ++b;
                const Memory::String word = loopBody.substr(begin, b - begin);
                if (word != loopVar)
                {
                    piece += word;
                    continue;
                }
                piece += parenthesizeElement ? ("(" + argument + ")") : argument;
            }
            expanded += piece;
            if (!piece.empty() && piece.back() != '\n') expanded += '\n';
        }
        body.replace(at, afterEnd - at, expanded);
    }
    error = "`$for` 嵌套层数过多（超过 64 层）";
    return false;
}

#include "magel_macro_controls.hpp"

/**
 * @brief 一次展开发生的位置，供 `$line` / `$file` 取值。
 *
 * 宏体里的 `$line` 取的是**调用点**那一行（和 C 的 `__LINE__` 一个意思），不是宏体
 * 自己所在的行；所以这个上下文只描述"当前正在展开的输出行"，递归进宏体时原样传下去。
 */
struct Site
{
    /**
     * 输出行（0 基）。
     */
    std::size_t line = 0;

    /**
     * 行号映射的来源（可为空指针，此时按"行号与主文件一致"处理）。
     */
    const preprocess::Result *result = nullptr;

    /**
     * 源文件 → 模块名；为空指针时 `$module` 给空串。
     */
    const Memory::Map<Memory::String, Memory::String> *moduleByFile = nullptr;
};

/**
 * @brief `$line` 的取值：映射回源文件后的行号（1 基）。
 */
inline std::size_t siteLine(const Site &site)
{
    if (site.result != nullptr && site.line < site.result->lineMap.size() && site.result->lineMap[site.line].line != 0)
        return site.result->lineMap[site.line].line;
    return site.line + 1;
}

/**
 * @brief `$file` 的取值：映射回源文件后的文件名；没有映射时为空串。
 */
inline Memory::String siteFile(const Site &site)
{
    if (site.result != nullptr && site.line < site.result->lineMap.size())
        return site.result->lineMap[site.line].file;
    return Memory::String();
}

/**
 * @brief `$module` 的取值：当前源文件所属的模块名；查不到时为空串。
 */
inline Memory::String siteModule(const Site &site)
{
    if (site.moduleByFile == nullptr) return Memory::String();
    const auto found = site.moduleByFile->find(siteFile(site));
    return found == site.moduleByFile->end() ? Memory::String() : found->second;
}

/**
 * @brief 把宏名净化成生成名里能安全当标识符用的一段：非 `[A-Za-z0-9_]` 一律换成 `_`。
 */
inline Memory::String sanitizeMacroName(const Memory::String &name)
{
    Memory::String out;
    for (char ch : name)
    {
        const bool ok = std::isalnum(static_cast<unsigned char>(ch)) || ch == '_';
        out += ok ? ch : '_';
    }
    return out;
}

/**
 * @brief 识别美元符号后的十进制整数编号，不把编号转换成宿主整数。
 * @param text 待扫描的原始文本。
 * @param start 美元符号的位置。
 * @return 合法编号后一位置；不是整数占位符时返回 start。
 */
inline std::size_t placeholderEnd(const Memory::String &text, std::size_t start)
{
    if (start >= text.size() || text[start] != '$') return start;
    std::size_t next = start + 1;
    if (next < text.size() && (text[next] == '+' || text[next] == '-')) ++next;
    const auto digits = next;
    while (next < text.size() && text[next] >= '0' && text[next] <= '9') ++next;
    return next == digits ? start : next;
}

/**
 * @brief 为任意长度的整数编号产生当前展开独有的合法标识符。
 * @param macroName 宏名称。
 * @param expandId 当前宏调用的唯一展开编号。
 * @param slot 十进制整数文本，允许符号和前导零。
 * @return 相同整数在一次展开内对应相同名字，不同展开互相隔离。
 * @note 不使用数值转换，因此编号不受 int64 或 uint64 上限限制。
 */
inline Memory::String gensymName(const Memory::String &macroName, std::size_t expandId, const Memory::String &slot)
{
    const bool negative = !slot.empty() && slot[0] == '-';
    std::size_t first = !slot.empty() && (slot[0] == '+' || slot[0] == '-') ? 1u : 0u;
    while (first + 1u < slot.size() && slot[first] == '0') ++first;
    const auto digits = slot.substr(first);
    const auto normalized = negative && digits != "0" ? "negative_" + digits : digits;
    return "__magel_macro_" + sanitizeMacroName(macroName) + "_" + Memory::toString(expandId) + "_" + normalized;
}

/**
 * @brief 为原始调用片段记录文件、行及列，供展开后还原诊断位置。
 * @param source 当前调用所在文本。
 * @param begin 片段起点。
 * @param length 片段字节长度。
 * @param site 当前输入文本的来源。
 * @retval 带内部来源标记的原始片段。
 * @note 引号和块注释内部不插入标记，避免改变字面文本。
 */
inline Memory::String mappedPiece(const Memory::String &source, std::size_t begin, std::size_t length, const Site &site)
{
    if (!site.result) return source.substr(begin, length);
    // 嵌套展开的片段已经带有逐行来源；直接保留，避免把内部标记字节算进原始列号。
    const auto piece = source.substr(begin, length);
    if (piece.find("/*__magel_source:") != Memory::String::npos) return piece;
    std::size_t row = site.line;
    std::size_t column = 0;
    preprocess::LineOrigin currentOrigin = row < site.result->lineMap.size() ? site.result->lineMap[row] : preprocess::LineOrigin{ siteFile(site), row + 1 };
    bool explicitOrigin = false;
    for (std::size_t i = 0; i < begin; ++i)
    {
        if (source.compare(i, sizeof("/*__magel_source:") - 1, "/*__magel_source:") == 0)
        {
            const auto end = source.find("__*/", i);
            const auto colon = source.find(':', i + sizeof("/*__magel_source:") - 1);
            const auto columnAt = source.find(':', colon + 1);
            if (end < begin && colon < end && columnAt < end)
            {
                currentOrigin.line = static_cast<std::size_t>(std::strtoull(source.c_str() + i + sizeof("/*__magel_source:") - 1, nullptr, 10));
                currentOrigin.file.clear();
                for (auto n = colon + 1; n + 1 < columnAt; n += 2)
                {
                    const auto byte = source.substr(n, 2);
                    currentOrigin.file += static_cast<char>(std::strtoul(byte.c_str(), nullptr, 16));
                }
                column = static_cast<std::size_t>(std::strtoull(source.c_str() + columnAt + 1, nullptr, 10));
                explicitOrigin = true;
                i = end + 3;
                continue;
            }
        }
        if (source[i] == '\n')
        {
            ++row;
            column = 0;
            if (explicitOrigin)
                ++currentOrigin.line;
            else
                currentOrigin = row < site.result->lineMap.size() ? site.result->lineMap[row] : preprocess::LineOrigin{ siteFile(site), row + 1 };
        }
        else
            ++column;
    }
    const auto marker = [&]() {
        return debugMarker(currentOrigin, column);
    };
    Memory::String out = marker();
    char quote = 0;
    bool block = false, lineComment = false;
    bool multilineToken = false;
    for (std::size_t i = begin; i < begin + length; ++i)
    {
        const char ch = source[i];
        out += ch;
        bool restore = false;
        if (quote)
        {
            if (ch == '\\' && i + 1 < begin + length)
            {
                out += source[++i];
                ++column;
            }
            else if (ch == quote)
            {
                quote = 0;
                restore = multilineToken;
                multilineToken = false;
            }
        }
        else if (block)
        {
            if (ch == '*' && i + 1 < begin + length && source[i + 1] == '/')
            {
                out += source[++i];
                ++column;
                block = false;
                restore = multilineToken;
                multilineToken = false;
            }
        }
        else if (!lineComment)
        {
            if (ch == '\"' || ch == '\'')
                quote = ch;
            else if (source.compare(i, 2, "/*") == 0)
                block = true;
            else if (source.compare(i, 2, "//") == 0)
                lineComment = true;
        }
        if (ch == '\n')
        {
            ++row;
            column = 0;
            if (explicitOrigin)
                ++currentOrigin.line;
            else
                currentOrigin = row < site.result->lineMap.size() ? site.result->lineMap[row] : preprocess::LineOrigin{ siteFile(site), row + 1 };
            lineComment = false;
            if (quote || block) multilineToken = true;
            if (!quote && !block && i + 1 < begin + length) out += marker();
        }
        else
        {
            ++column;
            if (restore && i + 1 < begin + length) out += marker();
        }
    }
    return out;
}

inline bool expand(const Memory::String &source,
                   const Memory::Map<Memory::String, Memory::Vector<Macro>> &macros,
                   Memory::String &output, Memory::String &error,
                   Memory::Vector<Memory::String> &stack, bool &blockComment,
                   const Site &site, std::size_t &nextExpandId)
{
    for (std::size_t i = 0; i < source.size();)
    {
        if (blockComment)
        {
            const std::size_t end = source.find("*/", i);
            if (end == Memory::String::npos)
            {
                output += source.substr(i);
                return true;
            }
            output += source.substr(i, end + 2 - i);
            i = end + 2;
            blockComment = false;
            continue;
        }
        if (source.compare(i, 2, "//") == 0)
        {
            const std::size_t end = source.find('\n', i);
            if (end == Memory::String::npos)
            {
                output += source.substr(i);
                return true;
            }
            output += source.substr(i, end - i + 1);
            i = end + 1;
            continue;
        }
        if (source.compare(i, 2, "/*") == 0)
        {
            output += "/*";
            i += 2;
            blockComment = true;
            continue;
        }
        if (source[i] == '"' || source[i] == '\'')
        {
            const char quote = source[i];
            const std::size_t begin = i++;
            while (i < source.size())
            {
                if (source[i] == '\\' && i + 1 < source.size())
                {
                    i += 2;
                    continue;
                }
                if (source[i++] == quote) break;
            }
            output += source.substr(begin, i - begin);
            continue;
        }
        if (source[i] == '$')
        {
            /* `$line` / `$file` / `$module` 是预处理器变量，宏体内外都能用；宏体内取的是
             * **调用点**那一行的信息（和 C 的 `__LINE__` / `__FILE__` 一个意思）。 */
            const std::size_t shortAt = i + 5;
            const std::size_t moduleAt = i + 7;
            if (source.compare(i, 7, "$module") == 0 && (moduleAt >= source.size() || !identPart(source[moduleAt])))
            {
                output += stringify(siteModule(site));
                i = moduleAt;
                continue;
            }
            const bool boundary = shortAt >= source.size() || !identPart(source[shortAt]);
            if (boundary && source.compare(i, 5, "$line") == 0)
            {
                output += Memory::toString(siteLine(site));
                i = shortAt;
                continue;
            }
            if (boundary && source.compare(i, 5, "$file") == 0)
            {
                output += stringify(siteFile(site));
                i = shortAt;
                continue;
            }
            /* `$concat(…)` / `$stringify(…)`：和 `$line` 一样带 `$` 前缀；普通代码里没有形参
             * 上下文，实参照字面量解析。 */
            {
                Memory::String keyword;
                if (source.compare(i, 7, "$concat") == 0 && (i + 7 >= source.size() || !identPart(source[i + 7])))
                {
                    keyword = "$concat";
                }
                else if (source.compare(i, 10, "$stringify") == 0 && (i + 10 >= source.size() || !identPart(source[i + 10])))
                {
                    keyword = "$stringify";
                }
                for (const auto extra : { "$count", "$typecheck" })
                {
                    const Memory::String candidate = extra;
                    if (source.compare(i, candidate.size(), candidate) == 0 && (i + candidate.size() == source.size() || !identPart(source[i + candidate.size()]))) keyword = candidate;
                }
                if (!keyword.empty())
                {
                    std::size_t intrinsicEnd = 0;
                    if (!expandIntrinsic(source, i, keyword, nullptr, nullptr, output,
                                         intrinsicEnd, error))
                        return false;
                    i = intrinsicEnd;
                    continue;
                }
            }
            /* 整数编号 `$n` 是宏体专属的占位符；普通代码里出现一律编译期报错。
             * 字符串与注释在上面就已经整段抄过去了，所以这里的 `$n` 一定在代码里。 */
            if (const auto end = placeholderEnd(source, i); end > i)
            {
                const auto digits = source.substr(i + 1, end - i - 1);
                error = "占位符 `$" + digits + "` 只能出现在 `$macro` / `$replace` 的宏体内";
                return false;
            }
        }
        std::size_t begin = i;
        bool fromDollar = false; /* `$NAME(…)` 才是宏调用；裸名永远是普通标识符。 */
        if (source[i] == '$')
        {
            std::size_t cursor = i + 1;
            if (cursor < source.size() && source[cursor] == '!')
            {
                error = "旧宏调用标注 `$!` 已移除，请使用 `$NAME(...)` 并在定义中声明 expr";
                return false;
            }
            if (cursor < source.size() && identStart(source[cursor]))
            {
                std::size_t scan = cursor + 1;
                while (scan < source.size() && identPart(source[scan])) ++scan;
                const Memory::String candidate = source.substr(cursor, scan - cursor);
                /* `.name` / `::name` 是成员或限定名，`$NAME` 也不当宏调用。 */
                const bool glued = i > 0 && (source[i - 1] == '.' || source[i - 1] == ':' || identPart(source[i - 1]));
                if (macros.find(candidate) != macros.end() && !glued)
                {
                    begin = cursor;
                    fromDollar = true;
                }
                else
                {
                    output += source[i++];
                    continue;
                }
            }
            else
            {
                output += source[i++];
                continue;
            }
        }
        else if (!identStart(source[i]))
        {
            output += source[i++];
            continue;
        }
        i = begin;
        begin = i++;
        while (i < source.size() && identPart(source[i])) ++i;
        const Memory::String name = source.substr(begin, i - begin);
        auto found = macros.find(name);
        if (found == macros.end() || !fromDollar)
        {
            output += name;
            continue;
        }
        const Memory::Vector<Macro> &candidates = found->second;
        Memory::Vector<Memory::String> args;
        std::size_t end = i;
        std::size_t open = i;
        while (open < source.size() && (source[open] == ' ' || source[open] == '\t')) ++open;
        const Macro *picked = nullptr;
        std::size_t groupedArgc = 0;
        bool groupedCandidate = false;
        Memory::String groupProblem;
        std::size_t furthestGroup = 0;
        for (const auto &item : candidates)
        {
            if (item.groups.empty()) continue;
            groupedCandidate = true;
            std::size_t cursor = open;
            Memory::Vector<Memory::String> bound;
            std::size_t supplied = 0;
            bool matched = true;
            std::size_t groupIndex = 0;
            for (const auto &group : item.groups)
            {
                spaces(source, cursor);
                const std::size_t groupOpen = cursor;
                const std::size_t groupBegin = cursor + 1;
                Memory::Vector<Memory::String> pieces;
                if (cursor >= source.size() || source[cursor] != group.opening || !groupArguments(source, cursor, pieces, cursor) || pieces.size() < group.params.size() || (group.rest.empty() && pieces.size() != group.params.size()))
                {
                    if (groupProblem.empty() || groupIndex >= furthestGroup)
                    {
                        furthestGroup = groupIndex;
                        groupProblem = "；第 " + Memory::toString(groupIndex + 1) + " 组期待 " + Memory::String(1, group.opening) + Memory::String(1, groupClose(group.opening));
                        groupProblem += groupOpen < source.size() && source[groupOpen] != group.opening ? "，实际遇到 " + Memory::String(1, source[groupOpen]) : "，请检查该组的闭合与文本实参分组";
                    }
                    matched = false;
                    break;
                }
                std::size_t pieceBegin = groupBegin;
                supplied += pieces.size();
                for (auto &piece : pieces)
                {
                    const auto length = piece.size();
                    piece = mappedPiece(source, pieceBegin, length, site);
                    pieceBegin += length + 1;
                }
                for (std::size_t n = 0; n < group.params.size(); ++n) bound.push_back(pieces[n]);
                if (!group.rest.empty())
                {
                    Memory::String tail;
                    for (std::size_t n = group.params.size(); n < pieces.size(); ++n)
                    {
                        if (n != group.params.size()) tail += ',';
                        tail += pieces[n];
                    }
                    bound.push_back(std::move(tail));
                }
                ++groupIndex;
            }
            if (matched)
            {
                const bool fixed = std::all_of(item.groups.begin(), item.groups.end(), [](const ParameterGroup &group) { return group.rest.empty(); });
                if (!picked || fixed)
                {
                    picked = &item;
                    args = std::move(bound);
                    end = cursor;
                    groupedArgc = supplied;
                }
                if (fixed) break;
            }
        }
        if (!picked && groupedCandidate && std::all_of(candidates.begin(), candidates.end(), [](const Macro &item) { return !item.groups.empty(); }))
        {
            error = "宏Token分组不匹配：$" + name + groupProblem;
            return false;
        }
        const bool called = open < source.size() && source[open] == '(';
        if (!picked && called && std::any_of(candidates.begin(), candidates.end(), [](const Macro &item) { return item.functionLike && item.groups.empty(); }))
        {
            if (!arguments(source, open, args, end))
            {
                error = "宏 " + name + " 的调用缺少 )";
                return false;
            }
        }
        /* 按元数挑形态：个数正好对上的固定形态优先，其次变长形态，最后才是对象宏。 */
        if (!picked && !called)
        {
            for (const Macro &item : candidates)
                if (!item.functionLike)
                {
                    picked = &item;
                    break;
                }
            if (picked == nullptr)
            {
                /* `$NAME` 只能是对象宏；带形参的宏必须写 `$NAME(…)`。 */
                Memory::String shapes;
                for (const Macro &item : candidates)
                {
                    if (!shapes.empty()) shapes += " / ";
                    shapes += "$" + name + "(";
                    for (std::size_t n = 0; n < item.params.size(); ++n)
                    {
                        if (n != 0u) shapes += ", ";
                        shapes += item.params[n];
                    }
                    if (item.variadic) shapes += item.params.empty() ? "..." : ", ...";
                    shapes += ")";
                }
                error = "宏 " + name + " 需要实参：写作 `$" + name + "(…)`"
                                                                     "（候选："
                        + shapes + "）";
                return false;
            }
        }
        else if (!picked)
        {
            for (const Macro &item : candidates)
                if (item.groups.empty() && item.functionLike && !item.variadic && item.params.size() == args.size())
                {
                    picked = &item;
                    break;
                }
            if (picked == nullptr)
                for (const Macro &item : candidates)
                    if (item.groups.empty() && item.functionLike && item.variadic && args.size() >= item.params.size())
                    {
                        picked = &item;
                        break;
                    }
        }
        if (picked == nullptr)
        {
            Memory::String shapes;
            for (const Macro &item : candidates)
            {
                if (!shapes.empty()) shapes += " / ";
                shapes += "$" + name + "(";
                for (std::size_t n = 0; n < item.params.size(); ++n)
                {
                    if (n != 0u) shapes += ", ";
                    shapes += item.params[n];
                }
                if (item.variadic) shapes += item.params.empty() ? "..." : ", ...";
                shapes += ")";
            }
            error = "宏 $" + name + " 没有匹配的形态：给了 " + Memory::toString(args.size()) + " 个实参（候选：" + shapes + "）";
            return false;
        }
        const Macro &macro = *picked;
        const auto actualArgc = macro.groups.empty() ? args.size() : groupedArgc;
        const std::size_t fixedCount = macro.params.size();
        if (stack.size() >= 64)
        {
            error = "宏递归展开超过 64 层：" + name;
            return false;
        }
        if (nextExpandId >= 100000)
        {
            error = "宏展开次数超过 100000 次";
            return false;
        }
        stack.push_back(name);
        Memory::Map<Memory::String, Memory::String> values;
        Memory::Map<Memory::String, Memory::String> rawValues;
        Memory::Vector<Memory::String> expandedArgs;
        expandedArgs.reserve(args.size());
        for (std::size_t n = 0; n < args.size(); ++n)
        {
            bool comment = false;
            Memory::String value;
            if (!expand(args[n], macros, value, error, stack, comment, site, nextExpandId))
                return false;
            expandedArgs.push_back(std::move(value));
        }
        for (std::size_t n = 0; n < fixedCount; ++n)
        {
            values.emplace(macro.params[n], expandedArgs[n]);
            rawValues.emplace(macro.params[n], magel_interpolation::rawText(args[n]));
        }
        Memory::String wrappedTail; /* 尾参使用括号标注或处于 expr 宏中时：每个元素各包一层括号 */
        if (macro.variadic)
        {
            Memory::String tail;
            Memory::String rawTail;
            for (std::size_t n = fixedCount; n < expandedArgs.size(); ++n)
            {
                if (!tail.empty()) tail += ", ";
                if (!rawTail.empty()) rawTail += ", ";
                tail += expandedArgs[n];
                rawTail += magel_interpolation::rawText(args[n]);
                if (!wrappedTail.empty()) wrappedTail += ", ";
                wrappedTail += "(" + expandedArgs[n] + ")";
            }
            values.emplace(macro.variadicParam, std::move(tail));
            rawValues.emplace(macro.variadicParam, std::move(rawTail));
        }
        /* 先把宏体里的 `$for … $end_for` 按尾参放开，再走常规的参数替换。 */
        Memory::String bodyText = macro.body;
        const Memory::Vector<Memory::String> tailArgs(
            expandedArgs.begin() + static_cast<std::ptrdiff_t>(fixedCount), expandedArgs.end());
        Memory::Map<Memory::String, Memory::Vector<Memory::String>> groupTails;
        std::size_t argumentIndex = 0;
        for (const auto &group : macro.groups)
        {
            argumentIndex += group.params.size();
            if (!group.rest.empty())
            {
                std::size_t ignored = 0;
                groupArguments("(" + expandedArgs[argumentIndex++] + ")", 0, groupTails[group.rest], ignored);
            }
        }
        if (!expandMacroControls(bodyText, values, rawValues, actualArgc, error,
                                 macro.variadic ? macro.variadicParam : Memory::String(), tailArgs, macro.parenthesizeVariadic, 0,
                                 macro.groups.empty() ? nullptr : &groupTails)) return false;
        /* 每次展开分配一个新 ID：整数编号 `$n` 拿它拼出本次展开专属的临时名。
         * 放在 `$for` 之后 —— 同一次展开里的每轮 `$for` 共用这一个 ID。 */
        const std::size_t expandId = nextExpandId++;
        Memory::String substituted;
        bool bodyComment = false;
        for (std::size_t b = 0; b < bodyText.size();)
        {
            const Memory::String &body = bodyText;
            if (body.compare(b, 2, "/*") == 0)
            {
                bodyComment = true;
                substituted += "/*";
                b += 2;
                continue;
            }
            if (bodyComment)
            {
                if (body.compare(b, 2, "*/") == 0)
                {
                    bodyComment = false;
                    substituted += "*/";
                    b += 2;
                }
                else
                    substituted += body[b++];
                continue;
            }
            if (body.compare(b, 2, "//") == 0)
            {
                const std::size_t endComment = body.find('\n', b);
                if (endComment == Memory::String::npos)
                {
                    substituted += body.substr(b);
                    b = body.size();
                }
                else
                {
                    substituted += body.substr(b, endComment - b + 1);
                    b = endComment + 1;
                }
                continue;
            }
            if (body[b] == '"' || body[b] == '\'')
            {
                const char quote = body[b];
                const std::size_t start = b++;
                while (b < body.size())
                {
                    if (body[b] == '\\' && b + 1 < body.size())
                    {
                        b += 2;
                        continue;
                    }
                    if (body[b++] == quote) break;
                }
                substituted += body.substr(start, b - start);
                continue;
            }
            /* `$concat(…)` / `$stringify(…)`：宏体里能拿到
             * 形参各自的实参文本（`$concat` 取展开后的标识符，`$stringify` 取原始文本）。 */
            if (body[b] == '$')
            {
                Memory::String keyword;
                if (body.compare(b, 7, "$concat") == 0 && (b + 7 >= body.size() || !identPart(body[b + 7])))
                {
                    keyword = "$concat";
                }
                else if (body.compare(b, 10, "$stringify") == 0 && (b + 10 >= body.size() || !identPart(body[b + 10])))
                {
                    keyword = "$stringify";
                }
                for (const auto extra : { "$count", "$typecheck" })
                {
                    const Memory::String candidate = extra;
                    if (body.compare(b, candidate.size(), candidate) == 0 && (b + candidate.size() == body.size() || !identPart(body[b + candidate.size()]))) keyword = candidate;
                }
                if (!keyword.empty())
                {
                    std::size_t intrinsicEnd = 0;
                    Memory::String problem;
                    if (!expandIntrinsic(body, b, keyword, &values, &rawValues, substituted,
                                         intrinsicEnd, problem))
                    {
                        error = "宏 " + name + " 的 " + problem;
                        return false;
                    }
                    b = intrinsicEnd;
                    continue;
                }
            }
            if (body.compare(b, 5, "$argc") == 0 && (b + 5 == body.size() || !identPart(body[b + 5])))
            {
                substituted += Memory::toString(actualArgc);
                b += 5;
                continue;
            }
            if (const auto end = placeholderEnd(body, b); end > b)
            {
                substituted += gensymName(name, expandId, body.substr(b + 1, end - b - 1));
                b = end;
                continue;
            }
            if (body[b] == '$')
            {
                /* 走到这里 `$` 后面只可能是标识符（宏调用 / `$line` / `$concat`…）；
                 * 别的写法是笔误。 */
                const char follow = b + 1 < body.size() ? body[b + 1] : '\0';
                if (!identStart(follow))
                {
                    error = "宏 " + name + " 的宏体里 `$` 后面要跟 整数编号 `$n`、宏调用或 "
                                           "`$line` / `$file` / `$module` / `$concat` / `$stringify`";
                    return false;
                }
            }
            if (!identStart(body[b]))
            {
                /* MAGEL 源码没有反引号字符，宏体里出现它一定是想给形参加括号：
                 * 标记只写在 `$macro` 头的形参名前，宏体里引用形参写裸名字。 */
                if (body[b] == '`')
                {
                    error = "宏 " + name + " 的宏体里有反引号：反引号标注已移除，请在定义头使用 `(参数名)`，"
                                           "宏体里引用形参写裸名字（自动加括号由头里的 `(参数名)` 或 expr 负责）";
                    return false;
                }
                substituted += body[b++];
                continue;
            }
            const std::size_t start = b++;
            while (b < body.size() && identPart(body[b])) ++b;
            const Memory::String word = body.substr(start, b - start);
            auto value = values.find(word);
            const bool isParam = value != values.end();
            Memory::String replacement = isParam ? value->second : word;
            if (isParam)
            {
                if (macro.variadic && word == macro.variadicParam && macro.parenthesizeVariadic)
                    replacement = wrappedTail; /* 每个尾参元素已各包一层括号 */
                else if (macro.parenthesized.count(word) != 0u)
                    replacement = "(" + replacement + ")";
            }
            substituted += replacement;
            if (isParam && !macro.groups.empty()) substituted += mappedPiece(body, b, 0, site);
        }
        /* expr 宏统一保护整个展开结果。 */
        if (macro.parenthesizeResult) substituted = "(" + substituted + ")";
        bool nestedComment = false;
        if (!expand(substituted, macros, output, error, stack, nestedComment, site, nextExpandId))
            return false;
        stack.pop_back();
        if (site.result && site.line < site.result->lineMap.size())
        {
            const auto suffix = mappedPiece(source, end, 0, site);
            output += macro.groups.empty() ? debugMarker(site.result->lineMap[site.line]) : suffix;
        }
        i = end;
    }
    return true;
}

inline bool run(preprocess::Result &result, Memory::String &error,
                const Memory::Map<Memory::String, Memory::String> *moduleByFile = nullptr)
{
    if (result.lineMap.empty())
    {
        const auto rows = 1 + static_cast<std::size_t>(std::count(result.text.begin(), result.text.end(), '\n'));
        for (std::size_t row = 0; row < rows; ++row) result.lineMap.push_back({ {}, row + 1 });
    }
    /* 同名宏可以按"实参个数"重载：一个名字挂一组形态（`$macro F(a)` 与 `$macro F(a, b)` 并存）。 */
    Memory::Map<Memory::String, Memory::Vector<Macro>> macros;
    Memory::String output;
    Memory::Vector<preprocess::LineOrigin> lineMap;
    Memory::Vector<Memory::String> stack;
    std::size_t nextExpandId = 0; /* 整数编号 `$n` 生成名用的全局递增展开 ID。 */
    bool comment = false;
    bool defining = false;
    /* 先扫一遍用户源码：`__magel_macro_` 前缀保留给 整数编号 `$n` 生成的临时名。
     * 生成名要到展开期才出现，所以现在扫到的这一批一定是用户自己写的。 */
    {
        bool scanComment = false;
        std::size_t scanLine = 0;
        for (std::size_t start = 0; start < result.text.size(); ++scanLine)
        {
            const std::size_t stop = result.text.find('\n', start);
            const std::size_t lineStop = stop == Memory::String::npos ? result.text.size() : stop;
            const Memory::String line = result.text.substr(start, lineStop - start);
            for (std::size_t i = 0; i < line.size();)
            {
                if (scanComment)
                {
                    if (line.compare(i, 2, "*/") == 0)
                    {
                        scanComment = false;
                        i += 2;
                        continue;
                    }
                    ++i;
                    continue;
                }
                if (line.compare(i, 2, "/*") == 0)
                {
                    scanComment = true;
                    i += 2;
                    continue;
                }
                if (line.compare(i, 2, "//") == 0) break;
                if (line[i] == '"' || line[i] == '\'')
                {
                    const char quote = line[i++];
                    while (i < line.size())
                    {
                        if (line[i] == '\\' && i + 1 < line.size())
                        {
                            i += 2;
                            continue;
                        }
                        if (line[i++] == quote) break;
                    }
                    continue;
                }
                if (identStart(line[i]))
                {
                    const std::size_t begin = i++;
                    while (i < line.size() && identPart(line[i])) ++i;
                    if (line.compare(begin, sizeof("__magel_macro_") - 1u, "__magel_macro_") == 0)
                    {
                        error = origin(result, scanLine) + "：`__magel_macro_` 前缀保留给宏 整数编号 `$n` 生成的临时名，用户代码不能使用";
                        return false;
                    }
                    continue;
                }
                ++i;
            }
            if (stop == Memory::String::npos) break;
            start = stop + 1;
        }
    }
    /* 当前宏体里打开的块：只有 `$for`。
     * 只用来做配对检查 —— 定义期就查完，所以"从来没被调用过的宏写错了"也能立刻报出来。 */
    struct Block
    {
        Memory::String kind; /* for */
        Memory::String name; /* `$for` 的循环变量名 */
    };
    Memory::Vector<Block> blocks;
    Memory::String name;
    Macro current;
    std::size_t definitionLine = 0;
    std::size_t bodyLines = 0;
    bool bodyBlockComment = false;
    char bodyQuote = 0;
    std::size_t lineNumber = 0;
    for (std::size_t start = 0; start < result.text.size();)
    {
        std::size_t stop = result.text.find('\n', start);
        if (!defining && !comment && !directive(result.text.substr(start, (stop == Memory::String::npos ? result.text.size() : stop) - start), "$macro") && !directive(result.text.substr(start, (stop == Memory::String::npos ? result.text.size() : stop) - start), "$replace"))
        {
            stop = groupedLineEnd(result.text, start, stop == Memory::String::npos ? result.text.size() : stop, macros);
            if (stop == result.text.size()) stop = Memory::String::npos;
        }
        const bool newline = stop != Memory::String::npos;
        const Memory::String line = result.text.substr(start, (newline ? stop : result.text.size()) - start);
        Memory::String replacement;
        if (defining)
        {
            if (!bodyBlockComment && !bodyQuote && directive(line, "$end_macro"))
            {
                if (!blocks.empty())
                {
                    error = origin(result, lineNumber) + "：`$" + blocks.back().kind + "` 缺少配对的 `$end_" + blocks.back().kind + "`";
                    return false;
                }
                std::size_t i = line.find("$end_macro") + 10;
                spaces(line, i);
                if (i != line.size())
                {
                    error = origin(result, lineNumber) + "：$end_macro 后有多余内容";
                    return false;
                }
                current.body = cleanPasteMarkers(current.body);
                macros[name].push_back(current);
                defining = false;
            }
            else
            {
                const bool control = !bodyBlockComment && !bodyQuote;
                if (control && (directive(line, "$macro") || directive(line, "$replace")))
                {
                    error = origin(result, lineNumber) + "：不支持嵌套 $macro / $replace";
                    return false;
                }
                /* 定义时就把 `$for` 的配对查完（交叉配对当场就能报出来）。 */
                if (control && directive(line, "$for"))
                {
                    std::size_t i = line.find("$for") + 4;
                    spaces(line, i);
                    if (i == line.size() || !identStart(line[i]))
                    {
                        error = origin(result, lineNumber) + "：`$for` 后面要写循环变量名";
                        return false;
                    }
                    const std::size_t begin = i++;
                    while (i < line.size() && identPart(line[i])) ++i;
                    blocks.push_back(Block{ "for", line.substr(begin, i - begin) });
                }
                else if (control && directive(line, "$if"))
                {
                    blocks.push_back(Block{ "if", {} });
                }
                else if (control && (directive(line, "$elif") || directive(line, "$else")))
                {
                    if (blocks.empty() || blocks.back().kind != "if")
                    {
                        error = origin(result, lineNumber) + "：条件分支没有配对的 $if";
                        return false;
                    }
                }
                else if (control && directive(line, "$end_if"))
                {
                    if (blocks.empty() || blocks.back().kind != "if")
                    {
                        error = origin(result, lineNumber) + "：$end_if 没有配对的 $if";
                        return false;
                    }
                    blocks.pop_back();
                }
                else if (control && directive(line, "$end_for"))
                {
                    if (blocks.empty() || blocks.back().kind != "for")
                    {
                        error = origin(result, lineNumber) + "：`$end_for` 没有配对的 `$for`";
                        return false;
                    }
                    blocks.pop_back();
                }
                if (bodyLines++) current.body += '\n';
                auto at = lineNumber < result.lineMap.size() ? result.lineMap[lineNumber] : preprocess::LineOrigin{};
                if (current.groups.empty()) at.file += " [宏 " + current.debugName + "]";
                bool controlDirective = false;
                for (const auto *keyword : { "$for", "$end_for", "$if", "$elif", "$else", "$end_if", "$let" })
                    if (directive(line, keyword)) controlDirective = true;
                if (!bodyBlockComment && !bodyQuote && !trim(line).empty() && !controlDirective) current.body += debugMarker(at);
                // 跳过字符串与单行注释，避免在多行注释内部插入来源标记。
                for (std::size_t n = 0; n < line.size();)
                {
                    if (bodyBlockComment)
                    {
                        auto end = line.find("*/", n);
                        if (end == Memory::String::npos) break;
                        bodyBlockComment = false;
                        n = end + 2;
                    }
                    else if (bodyQuote)
                    {
                        const char c = line[n++];
                        if (c == '\\' && n < line.size())
                            ++n;
                        else if (c == bodyQuote)
                            bodyQuote = 0;
                    }
                    else if (line.compare(n, 2, "//") == 0)
                        break;
                    else if (line.compare(n, 2, "/*") == 0)
                    {
                        bodyBlockComment = true;
                        n += 2;
                    }
                    else if (line[n] == '"' || line[n] == '\'')
                        bodyQuote = line[n++];
                    else
                        ++n;
                }
                current.body += line;
            }
        }
        else if (!comment && directive(line, "$macro"))
        {
            current = Macro{};
            bodyLines = 0;
            bodyBlockComment = false;
            bodyQuote = 0;
            Memory::String problem;
            if (!header(line, name, current, problem))
            {
                error = origin(result, lineNumber) + "：" + problem;
                return false;
            }
            if (macroDslKeyword(name))
            {
                error = origin(result, lineNumber) + "：宏名 " + name + " 是内部 DSL 关键字（`$macro` / `$replace` / `$for` / "
                                                                        "`$line` …），不能占用";
                return false;
            }
            for (const Macro &existing : macros[name])
            {
                if (sameShape(existing, current))
                {
                    error = origin(result, lineNumber) + "：重复定义宏 " + name + "（同参数个数）";
                    return false;
                }
            }
            current.debugName = name;
            defining = true;
            definitionLine = lineNumber;
        }
        else if (!comment && directive(line, "$replace"))
        {
            /* 单行宏：头这一行剩下的内容就是宏体，没有 `$end_macro`。只装得下参数替换、
             * `expr` / 参数括号、`$concat` / `$stringify` 与 `$line` / `$file` / `$module` 这类行内规则。 */
            current = Macro{};
            bodyLines = 0;
            bodyBlockComment = false;
            bodyQuote = 0;
            Memory::String problem;
            Memory::String inlineBody;
            if (!header(line, name, current, problem, &inlineBody, "$replace"))
            {
                error = origin(result, lineNumber) + "：" + problem;
                return false;
            }
            if (macroDslKeyword(name))
            {
                error = origin(result, lineNumber) + "：宏名 " + name + " 是内部 DSL 关键字（`$macro` / `$replace` / `$for` / "
                                                                        "`$line` …），不能占用";
                return false;
            }
            if (inlineBody.empty())
            {
                error = origin(result, lineNumber) + "：`$replace` 是单行宏，形参表后面要写宏体；多行宏请用 `$macro … $end_macro`";
                return false;
            }
            Memory::String block;
            if (findBlockDirective(inlineBody, block))
            {
                error = origin(result, lineNumber) + "：`$replace` 是单行宏，宏体里不能写 `" + block + "`；要 `$for` 请改用 `$macro … $end_macro`";
                return false;
            }
            current.debugName = name;
            auto at = lineNumber < result.lineMap.size() ? result.lineMap[lineNumber] : preprocess::LineOrigin{};
            if (current.groups.empty()) at.file += " [宏 " + name + "]";
            current.body = debugMarker(at) + inlineBody;
            for (const Macro &existing : macros[name])
            {
                if (sameShape(existing, current))
                {
                    error = origin(result, lineNumber) + "：重复定义宏 " + name + "（同参数个数）";
                    return false;
                }
            }
            current.body = cleanPasteMarkers(current.body);
            macros[name].push_back(current);
        }
        else if (!comment && directive(line, "$delete_macro"))
        {
            std::size_t i = line.find("$delete_macro") + 13;
            spaces(line, i);
            if (i == line.size() || !identStart(line[i]))
            {
                error = origin(result, lineNumber) + "：$delete_macro 后面要写宏名";
                return false;
            }
            const std::size_t begin = i++;
            while (i < line.size() && identPart(line[i])) ++i;
            const Memory::String erased = line.substr(begin, i - begin);
            spaces(line, i);
            if (i != line.size())
            {
                error = origin(result, lineNumber) + "：$delete_macro 后有多余内容";
                return false;
            }
            macros.erase(erased);
        }
        else if (!comment && directive(line, "$end_macro"))
        {
            error = origin(result, lineNumber) + "：多余的 $end_macro";
            return false;
        }
        else if (!comment && directive(line, "$end_for"))
        {
            error = origin(result, lineNumber) + "：多余的 $end_for（它只在 `$macro` 体内的 `$for` 后面配对）";
            return false;
        }
        else if (!expand(line, macros, replacement, error, stack, comment,
                         Site{ lineNumber, &result, moduleByFile }, nextExpandId))
        {
            error = origin(result, lineNumber) + "：" + error;
            return false;
        }
        for (char ch : replacement)
            if (ch == '\n')
            {
                lineMap.push_back(lineNumber < result.lineMap.size() ? result.lineMap[lineNumber] : preprocess::LineOrigin{});
            }
        lineMap.push_back(lineNumber < result.lineMap.size() ? result.lineMap[lineNumber] : preprocess::LineOrigin{});
        output += replacement;
        if (newline) output += '\n';
        lineNumber += 1 + static_cast<std::size_t>(std::count(line.begin(), line.end(), '\n'));
        start = newline ? stop + 1 : result.text.size();
    }
    if (defining)
    {
        error = origin(result, definitionLine) + "：缺少 $end_macro";
        return false;
    }
    // 内部标记不进入用户源码；按标记拆行，使同一调用行的宏体有独立来源。
    Memory::String mapped;
    Memory::Vector<preprocess::LineOrigin> origins;
    std::size_t row = 0;
    preprocess::LineOrigin currentOrigin = lineMap.empty() ? preprocess::LineOrigin{} : lineMap[0];
    auto finishLine = [&]() { origins.push_back(currentOrigin); mapped+='\n'; };
    for (std::size_t i = 0; i < output.size();)
    {
        if (output.compare(i, sizeof("/*__magel_source:") - 1u, "/*__magel_source:") == 0)
        {
            const auto end = output.find("__*/", i + sizeof("/*__magel_source:") - 1u);
            const auto colon = output.find(':', i + sizeof("/*__magel_source:") - 1u);
            if (end != Memory::String::npos && colon < end)
            {
                if (!mapped.empty() && mapped.back() != '\n') finishLine();
                currentOrigin.line = static_cast<std::size_t>(std::strtoull(output.substr(i + sizeof("/*__magel_source:") - 1u, colon - i - (sizeof("/*__magel_source:") - 1u)).c_str(), nullptr, 10));
                currentOrigin.file.clear();
                auto digit = [](char c) { return c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 :
                                                                                                         -1; };
                auto columnAt = output.find(':', colon + 1);
                if (columnAt == Memory::String::npos || columnAt > end) columnAt = end;
                for (auto n = colon + 1; n + 1 < columnAt; n += 2)
                {
                    const int hi = digit(output[n]), lo = digit(output[n + 1]);
                    if (hi < 0 || lo < 0)
                    {
                        error = "非法宏来源标记";
                        return false;
                    }
                    currentOrigin.file += static_cast<char>((hi << 4) | lo);
                }
                if (columnAt < end)
                {
                    const auto column = static_cast<std::size_t>(std::strtoull(output.substr(columnAt + 1, end - columnAt - 1).c_str(), nullptr, 10));
                    mapped.append(column, ' ');
                }
                i = end + 4;
                continue;
            }
        }
        if (output.compare(i, 2, "//") == 0 || output.compare(i, 2, "/*") == 0)
        {
            const bool block = output[i + 1] == '*';
            auto end = block ? output.find("*/", i + 2) : output.find('\n', i + 2);
            end = end == Memory::String::npos ? output.size() : (block ? end + 2 : end);
            while (i < end)
            {
                const char c = output[i++];
                if (c == '\n')
                {
                    finishLine();
                    ++row;
                    currentOrigin = row < lineMap.size() ? lineMap[row] : preprocess::LineOrigin{};
                }
                else
                    mapped += c;
            }
            continue;
        }
        if (output[i] == '"' || output[i] == '\'')
        {
            const char quote = output[i];
            mapped += output[i++];
            while (i < output.size())
            {
                const char c = output[i++];
                if (c == '\n')
                {
                    finishLine();
                    ++row;
                    currentOrigin = row < lineMap.size() ? lineMap[row] : preprocess::LineOrigin{};
                }
                else
                    mapped += c;
                if (c == '\\' && i < output.size())
                {
                    const char next = output[i++];
                    if (next == '\n')
                    {
                        finishLine();
                        ++row;
                        currentOrigin = row < lineMap.size() ? lineMap[row] : preprocess::LineOrigin{};
                    }
                    else
                        mapped += next;
                }
                else if (c == quote)
                    break;
            }
            continue;
        }
        const char c = output[i++];
        if (c == '\n')
        {
            finishLine();
            ++row;
            currentOrigin = row < lineMap.size() ? lineMap[row] : preprocess::LineOrigin{};
        }
        else
            mapped += c;
    }
    if (!mapped.empty() && mapped.back() != '\n') origins.push_back(currentOrigin);
    result.text = std::move(mapped);
    result.lineMap = std::move(origins);
    return true;
}

}  // namespace magel_macro

#endif
