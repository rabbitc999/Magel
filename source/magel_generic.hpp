#include "allocator/magel_allocator.hpp"
#ifndef MAGEL_GENERIC_HPP
#define MAGEL_GENERIC_HPP

#include "preprocess.hpp"
#include "magel_generic_name.hpp"
#include "magel_macro.hpp"
#include "magel_lambda.hpp"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace magel_generic
{

/**
 * @brief 泛型形参：类型约束或编译期整数，以及可选默认值。
 */
struct Parameter
{
    Memory::String name;
    Memory::String constraint;
    Memory::String fallback;
    bool hasDefault = false;
    bool constant = false;        ///< 是否为编译期整数参数。
    Memory::String constantType;  ///< 整数参数的声明类型。
};

/**
 * @brief 保留完整声明和来源位置的单态化模板。
 */
struct Generic
{
    Memory::String name;
    Memory::String module;
    Memory::Vector<Parameter> params;
    Memory::String declaration;
    std::size_t nameOffset = 0;
    preprocess::LineOrigin origin;
    bool intrinsic = false;
    Memory::String requiredQualifier;
    Memory::String derivation;  ///< 保留到具体实例的 #derive 指令，不交给文本宏做语义检查。
};
/**
 * @brief 可用于整数泛型实参的模块常量原文；求值时仍拒绝运行时初始化式。
 */
inline Memory::Map<Memory::String, Memory::String> constantArguments;

inline bool identStart(char value)
{
    return std::isalpha(static_cast<unsigned char>(value)) || value == '_';
}

inline bool identPart(char value)
{
    return std::isalnum(static_cast<unsigned char>(value)) || value == '_';
}

inline Memory::String trim(const Memory::String &value)
{
    const std::size_t first = value.find_first_not_of(" \t\r\n");
    if (first == Memory::String::npos) return {};
    const std::size_t last = value.find_last_not_of(" \t\r\n");
    return value.substr(first, last - first + 1);
}

inline void skipSpace(const Memory::String &text, std::size_t &at)
{
    while (at < text.size() && std::isspace(static_cast<unsigned char>(text[at]))) ++at;
}
/**
 * @brief 收集模块层整数常量，保留限定名与无歧义的裸名。
 * @param source 预处理后的完整源码。
 */
inline void collectConstantArguments(const Memory::String &source)
{
    constantArguments.clear();
    const auto mask = magel_lambda::codeMask(source);
    Memory::Map<Memory::String, Memory::Map<Memory::String, Memory::String>> declarations;
    Memory::String module;
    int braces = 0;
    for (std::size_t at = 0; at < mask.size();)
    {
        if (mask[at] == '{')
        {
            ++braces;
            ++at;
            continue;
        }
        if (mask[at] == '}')
        {
            --braces;
            ++at;
            continue;
        }
        if (!identStart(mask[at]))
        {
            ++at;
            continue;
        }
        const auto begin = at;
        while (at < mask.size() && identPart(mask[at])) ++at;
        const auto word = mask.substr(begin, at - begin);
        if (braces) continue;
        if (word == "module")
        {
            skipSpace(mask, at);
            const auto start = at;
            while (at < mask.size() && (identPart(mask[at]) || mask[at] == ':')) ++at;
            module = mask.substr(start, at - start);
            if (module == "__root") module.clear();
            continue;
        }
        if (word != "const") continue;
        skipSpace(mask, at);
        const auto typeStart = at;
        while (at < mask.size() && identPart(mask[at])) ++at;
        const auto type = mask.substr(typeStart, at - typeStart);
        if (type != "int8" && type != "int16" && type != "int32" && type != "int64" && type != "uint8" && type != "uint16" && type != "uint32" && type != "uint64") continue;
        skipSpace(mask, at);
        const auto nameStart = at;
        while (at < mask.size() && identPart(mask[at])) ++at;
        const auto name = mask.substr(nameStart, at - nameStart);
        skipSpace(mask, at);
        if (at >= mask.size() || mask[at] != '=') continue;
        const auto end = mask.find(';', ++at);
        if (end == Memory::String::npos) break;
        declarations[name][module] = trim(source.substr(at, end - at));
        at = end + 1;
    }
    for (const auto &entry : declarations)
    {
        for (const auto &definition : entry.second)
            constantArguments[definition.first.empty() ? entry.first : definition.first + "::" + entry.first] = definition.second;
        if (entry.second.size() == 1) constantArguments[entry.first] = entry.second.begin()->second;
        const auto root = entry.second.find("");
        if (root != entry.second.end()) constantArguments[entry.first] = root->second;
    }
}

inline bool wordAt(const Memory::String &text, std::size_t at, const Memory::String &word)
{
    return text.compare(at, word.size(), word) == 0 && (at == 0 || !identPart(text[at - 1])) && (at + word.size() == text.size() || !identPart(text[at + word.size()]));
}

inline bool balanced(const Memory::String &text, std::size_t open, char left, char right,
                     std::size_t &close)
{
    int depth = 0;
    char quote = 0;
    bool lineComment = false, blockComment = false;
    for (std::size_t at = open; at < text.size(); ++at)
    {
        const char value = text[at];
        if (lineComment)
        {
            if (value == '\n') lineComment = false;
            continue;
        }
        if (blockComment)
        {
            if (value == '*' && at + 1 < text.size() && text[at + 1] == '/')
            {
                blockComment = false;
                ++at;
            }
            continue;
        }
        if (quote != 0)
        {
            if (value == '\\' && at + 1 < text.size())
            {
                ++at;
                continue;
            }
            if (value == quote) quote = 0;
            continue;
        }
        if (value == '/' && at + 1 < text.size() && text[at + 1] == '/')
        {
            lineComment = true;
            ++at;
            continue;
        }
        if (value == '/' && at + 1 < text.size() && text[at + 1] == '*')
        {
            blockComment = true;
            ++at;
            continue;
        }
        if (value == '"' || value == '\'')
        {
            quote = value;
            continue;
        }
        if (value == left)
            ++depth;
        else if (value == right && !(right == '>' && at > 0 && text[at - 1] == '-') && --depth == 0)
        {
            close = at;
            return true;
        }
    }
    return false;
}

inline Memory::Vector<Memory::String> splitTop(const Memory::String &text)
{
    Memory::Vector<Memory::String> result;
    std::size_t begin = 0;
    int angle = 0, round = 0, square = 0;
    for (std::size_t at = 0; at <= text.size(); ++at)
    {
        const char value = at < text.size() ? text[at] : ',';
        if (value == '<')
            ++angle;
        else if (value == '>' && (at == 0 || text[at - 1] != '-'))
            --angle;
        else if (value == '(')
            ++round;
        else if (value == ')')
            --round;
        else if (value == '[')
            ++square;
        else if (value == ']')
            --square;
        else if (value == ',' && angle == 0 && round == 0 && square == 0)
        {
            result.push_back(trim(text.substr(begin, at - begin)));
            begin = at + 1;
        }
    }
    if (result.size() == 1u && result[0].empty()) result.clear();
    return result;
}

inline bool parseParameters(const Memory::String &text, Memory::Vector<Parameter> &result,
                            Memory::String &error)
{
    for (const Memory::String &item : splitTop(text))
    {
        Parameter parameter;
        Memory::String head = item;
        const std::size_t equal = head.find('=');
        if (equal != Memory::String::npos)
        {
            parameter.hasDefault = true;
            parameter.fallback = trim(head.substr(equal + 1));
            head = trim(head.substr(0, equal));
            if (parameter.fallback.empty())
            {
                error = "泛型参数的默认类型为空";
                return false;
            }
        }
        const std::size_t colon = head.find(':');
        if (colon != Memory::String::npos)
        {
            parameter.constraint = trim(head.substr(colon + 1));
            if (parameter.constraint == "int64")
            {
                parameter.constant = true;
                parameter.constantType = parameter.constraint;
                parameter.constraint.clear();
            }
            head = trim(head.substr(0, colon));
        }
        parameter.name = trim(head);
        if (parameter.name.empty() || !identStart(parameter.name[0]))
        {
            error = "泛型参数名不合法：" + item;
            return false;
        }
        for (char value : parameter.name)
            if (!identPart(value))
            {
                error = "泛型参数名不合法：" + parameter.name;
                return false;
            }
        for (const Parameter &old : result)
            if (old.name == parameter.name)
            {
                error = "重复的泛型参数 " + parameter.name;
                return false;
            }
        result.push_back(parameter);
    }
    if (result.empty())
    {
        error = "泛型声明的 <> 至少需要一个类型参数";
        return false;
    }
    for (std::size_t i = 0; i < result.size(); ++i)
    {
        if (!result[i].hasDefault) continue;
        for (std::size_t at = 0; at < result[i].fallback.size(); ++at)
        {
            for (std::size_t j = i; j < result.size(); ++j)
            {
                if (wordAt(result[i].fallback, at, result[j].name))
                {
                    error = "泛型默认类型只能引用前面的类型参数：" + result[i].name + " 引用了 " + result[j].name;
                    return false;
                }
            }
        }
    }
    return true;
}

inline std::size_t lineOf(const Memory::String &text, std::size_t offset)
{
    std::size_t line = 0;
    for (std::size_t at = 0; at < offset; ++at)
        if (text[at] == '\n') ++line;
    return line;
}

/**
 * @brief 暂存泛型类型前的派生指令，在产生具体实例时恢复原文。
 * @param source 待单态化源码；移走的指令以空白占位保持行号。
 * @param directives 输出模板名到派生指令文本的映射。
 * @param error 失败时接收非法派生请求的诊断。
 * @return 指令扫描成功返回 true，不执行字段或接口语义检查。
 */
inline bool collectGenericDerives(Memory::String &source, Memory::Map<Memory::String, Memory::String> &directives, Memory::String &error)
{
    auto mask = magel_lambda::codeMask(source);
    int depth = 0;
    for (std::size_t at = 0; at < mask.size(); ++at)
    {
        if (mask[at] == '{') ++depth;
        if (mask[at] == '}') --depth;
        if (depth || !wordAt(mask, at, "#derive")) continue;
        std::size_t open = at + 7;
        skipSpace(mask, open);
        Memory::Vector<Memory::String> traits;
        std::size_t end = open;
        if (open >= mask.size() || mask[open] != '(' || !magel_macro::groupArguments(mask, open, traits, end)) continue;
        auto type = end;
        skipSpace(mask, type);
        const bool structure = wordAt(mask, type, "struct");
        if (!structure && !wordAt(mask, type, "enum")) continue;
        type += structure ? 6 : 4;
        skipSpace(mask, type);
        const auto begin = type;
        while (type < mask.size() && identPart(mask[type])) ++type;
        const auto name = mask.substr(begin, type - begin);
        skipSpace(mask, type);
        if (type >= mask.size() || mask[type] != '<') continue;
        Memory::Set<Memory::String> seenTraits;
        if (traits.empty()) return error = "#derive 至少需要一个接口", false;
        for (const auto &item : traits)
        {
            const auto trait = trim(item);
            if (!seenTraits.insert(trait).second) return error = "#derive 重复请求接口：" + trait, false;
            if (trait == "Drop" || trait == "drop" || trait == "Closeable")
                return error = "#derive 禁止生成 Drop 或 Closeable：值语义已经自动递归清理字段", false;
            if (trait != "Equal" && trait != "Hash" && trait != "Clone")
                return error = "#derive 只支持 Equal、Hash、Clone，不能生成 " + trait, false;
        }
        directives[name] = source.substr(at, end - at) + "\n";
        for (auto i = at; i < end; ++i)
            if (source[i] != '\n') source[i] = mask[i] = ' ';
        at = end - 1;
    }
    return true;
}

/**
 * @brief 归一化顶层声明中名称后置的泛型参数，正文仍由生成解析器处理。
 * @param source 原地规范化的源码，保留行数以映射诊断。
 */
inline void normalizeGenericSyntax(Memory::String &source)
{
    struct Edit
    {
        std::size_t begin, end;
        Memory::String text;
    };
    Memory::Vector<Edit> edits;
    std::size_t declaration = 0;
    int braces = 0, rounds = 0;
    bool annotated = false;
    for (std::size_t at = 0; at < source.size(); ++at)
    {
        if (source.compare(at, 2, "//") == 0)
        {
            auto end = source.find('\n', at);
            if (end == Memory::String::npos) break;
            if (trim(source.substr(declaration, at - declaration)).empty()) declaration = end + 1;
            at = end;
            continue;
        }
        if (source.compare(at, 2, "/*") == 0)
        {
            auto end = source.find("*/", at + 2);
            if (end == Memory::String::npos) break;
            if (trim(source.substr(declaration, at - declaration)).empty()) declaration = end + 2;
            at = end + 1;
            continue;
        }
        if (source[at] == '\"' || source[at] == '\'')
        {
            const char quote = source[at++];
            while (at < source.size())
            {
                if (source[at] == '\\')
                {
                    at += 2;
                    continue;
                }
                if (source[at] == quote) break;
                ++at;
            }
            continue;
        }
        // 此处只跳过注解前缀以找泛型声明边界；语法与参数检查仍由 .gram 完成。
        if (braces == 0 && rounds == 0 && source[at] == '@')
        {
            std::size_t end = at + 1;
            const bool definition = wordAt(source, end, "annotation");
            if (definition)
            {
                end += 10;
                skipSpace(source, end);
            }
            while (end < source.size() && (identPart(source[end]) || source[end] == ':')) ++end;
            std::size_t args = end;
            skipSpace(source, args);
            if (args < source.size() && source[args] == '(')
            {
                std::size_t close = 0;
                if (balanced(source, args, '(', ')', close)) end = close + 1;
            }
            declaration = end;
            annotated = !definition;
            at = end - 1;
            continue;
        }
        if (source[at] == '{') ++braces;
        if (source[at] == '}' && --braces == 0)
        {
            declaration = at + 1;
            annotated = false;
        }
        if (braces != 0) continue;
        if (source[at] == '(') ++rounds;
        if (source[at] == ')') --rounds;
        if (source[at] == ';')
        {
            declaration = at + 1;
            rounds = 0;
            annotated = false;
        }
        // 首版不收集带前置注解的泛型模板，保留原文交给文法拒绝，避免丢失或错绑注解。
        if (source[at] != '<' || rounds != 0 || annotated) continue;
        const std::size_t open = at;
        std::size_t close = 0;
        if (!balanced(source, open, '<', '>', close)) continue;
        std::size_t nameEnd = open;
        while (nameEnd > declaration && std::isspace(static_cast<unsigned char>(source[nameEnd - 1]))) --nameEnd;
        std::size_t name = nameEnd;
        while (name > declaration && identPart(source[name - 1])) --name;
        const auto head = trim(source.substr(declaration, name - declaration));
        std::size_t after = close + 1u;
        skipSpace(source, after);
        const bool nominal = head == "struct" || head == "enum" || head == "interface" || head == "type";
        const bool function = !head.empty() && head.find('=') == Memory::String::npos && after < source.size() && source[after] == '(';
        if (!nominal && !function)
        {
            at = close;
            continue;
        }
        // 内部标记不是语言关键字；用户的 generic 标识符仍是普通名字。
        std::size_t start = declaration;
        skipSpace(source, start);
        edits.push_back(Edit{ start, close + 1u,
                              "__magel_template" + source.substr(open, close - open + 1u) + " " + source.substr(start, nameEnd - start) });
        at = close;
    }
    for (auto edit = edits.rbegin(); edit != edits.rend(); ++edit)
        source.replace(edit->begin, edit->end - edit->begin, edit->text);
}

/**
 * @brief 标记标准容器类型实参的标点，保留普通标识符与比较运算符的词法角色。
 * @param source 已完成单态化的源码；仅插入内部标点，不改变换行。
 * @return 无。
 */
inline void markContainerTypeArguments(Memory::String &source)
{
    Memory::Set<std::size_t> angles;
    for (std::size_t at = 0; at < source.size();)
    {
        if (source.compare(at, 2, "//") == 0)
        {
            auto end = source.find('\n', at);
            if (end == Memory::String::npos) break;
            at = end;
            continue;
        }
        if (source.compare(at, 2, "/*") == 0)
        {
            auto end = source.find("*/", at + 2);
            if (end == Memory::String::npos) break;
            at = end + 2;
            continue;
        }
        if (source[at] == '"' || source[at] == '\'')
        {
            const char quote = source[at++];
            while (at < source.size())
            {
                if (source[at] == '\\')
                {
                    at += 2;
                    continue;
                }
                if (source[at++] == quote) break;
            }
            continue;
        }
        if (!identStart(source[at]))
        {
            ++at;
            continue;
        }
        auto start = at++;
        while (at < source.size() && identPart(source[at])) ++at;
        const auto name = source.substr(start, at - start);
        if (name != "List" && name != "Set" && name != "Map") continue;
        auto open = at;
        skipSpace(source, open);
        if (open >= source.size() || source[open] != '<') continue;
        std::size_t close = 0;
        if (!balanced(source, open, '<', '>', close)) continue;
        int rounds = 0, squares = 0;
        bool type = true;
        Memory::Vector<std::size_t> candidate{ open };
        for (auto i = open + 1; i < close; ++i)
        {
            if (source.compare(i, 2, "/*") == 0)
            {
                auto end = source.find("*/", i + 2);
                if (end == Memory::String::npos || end > close)
                {
                    type = false;
                    break;
                }
                i = end + 1;
                continue;
            }
            if (source.compare(i, 2, "//") == 0)
            {
                auto end = source.find('\n', i + 2);
                if (end == Memory::String::npos || end > close)
                {
                    type = false;
                    break;
                }
                i = end;
                continue;
            }
            const char ch = source[i];
            if (ch == '<' || (ch == '>' && source[i - 1] != '-')) candidate.push_back(i);
            if (ch == '(')
                ++rounds;
            else if (ch == ')' && --rounds < 0)
            {
                type = false;
                break;
            }
            if (ch == '[')
                ++squares;
            else if (ch == ']' && --squares < 0)
            {
                type = false;
                break;
            }
            if (ch == ';' || ch == '{' || ch == '}' || ch == '=' || ch == '+' || ch == '/' || ch == '"' || ch == '\'')
            {
                type = false;
                break;
            }
        }
        if (!type || rounds || squares) continue;
        candidate.push_back(close);
        for (auto i : candidate) angles.insert(i);
    }
    for (auto at = angles.rbegin(); at != angles.rend(); ++at) source.insert(*at, 1u, '@');
}

inline bool collect(Memory::String &source, const Memory::Vector<preprocess::LineOrigin> &lines,
                    Memory::Map<Memory::String, Generic> &generics, Memory::String &error)
{
    Memory::String currentModule;
    for (std::size_t at = 0; at < source.size();)
    {
        if (source.compare(at, 2, "//") == 0)
        {
            const std::size_t end = source.find('\n', at);
            at = end == Memory::String::npos ? source.size() : end + 1;
            continue;
        }
        if (source.compare(at, 2, "/*") == 0)
        {
            const std::size_t end = source.find("*/", at + 2);
            at = end == Memory::String::npos ? source.size() : end + 2;
            continue;
        }
        if (source[at] == '"' || source[at] == '\'')
        {
            const char quote = source[at++];
            while (at < source.size())
            {
                if (source[at] == '\\' && at + 1 < source.size())
                {
                    at += 2;
                    continue;
                }
                if (source[at++] == quote) break;
            }
            continue;
        }
        if (wordAt(source, at, "module"))
        {
            std::size_t nameAt = at + 6u;
            skipSpace(source, nameAt);
            const std::size_t end = source.find(';', nameAt);
            if (end == Memory::String::npos)
            {
                error = "module 声明缺少 ;";
                return false;
            }
            currentModule = trim(source.substr(nameAt, end - nameAt));
            if (currentModule == "__root") currentModule.clear();
            at = end + 1u;
            continue;
        }
        if (!wordAt(source, at, "__magel_template"))
        {
            ++at;
            continue;
        }
        const std::size_t start = at;
        at += sizeof("__magel_template") - 1u;
        skipSpace(source, at);
        if (at >= source.size() || source[at] != '<')
        {
            error = "泛型声明名称后需要 <...>";
            return false;
        }
        std::size_t paramsEnd = 0;
        if (!balanced(source, at, '<', '>', paramsEnd))
        {
            error = "泛型参数缺少 >";
            return false;
        }
        Generic item;
        item.module = currentModule;
        if (!parseParameters(source.substr(at + 1, paramsEnd - at - 1), item.params, error)) return false;
        std::size_t decl = paramsEnd + 1;
        skipSpace(source, decl);
        std::size_t nameAt = Memory::String::npos;
        const bool structDecl = wordAt(source, decl, "struct");
        const bool enumDecl = wordAt(source, decl, "enum");
        const bool interfaceDecl = wordAt(source, decl, "interface");
        const bool hostTypeDecl = wordAt(source, decl, "type");
        if (structDecl || enumDecl || interfaceDecl)
        {
            nameAt = decl + (structDecl ? 6u : interfaceDecl ? 9u :
                                                               4u);
            skipSpace(source, nameAt);
        }
        else if (hostTypeDecl)
        {
            nameAt = decl + 4u;
            skipSpace(source, nameAt);
        }
        else
        {
            std::size_t open = Memory::String::npos;
            int angleDepth = 0;
            std::size_t signatureBegin = decl;
            // 元组返回类型的括号不是函数参数表。
            if (source[signatureBegin] == '(')
            {
                std::size_t tupleEnd = 0;
                if (balanced(source, signatureBegin, '(', ')', tupleEnd)) signatureBegin = tupleEnd + 1u;
            }
            for (std::size_t cursor = signatureBegin; cursor < source.size(); ++cursor)
            {
                if (source[cursor] == '<')
                    ++angleDepth;
                else if (source[cursor] == '>' && (cursor == 0 || source[cursor - 1] != '-'))
                    --angleDepth;
                else if (source[cursor] == '(' && angleDepth == 0)
                {
                    std::size_t word = cursor;
                    while (word > signatureBegin && std::isspace(static_cast<unsigned char>(source[word - 1]))) --word;
                    const auto end = word;
                    while (word > signatureBegin && identPart(source[word - 1])) --word;
                    if (source.substr(word, end - word) == "fn" || word == end)
                    {
                        std::size_t close = 0;
                        if (balanced(source, cursor, '(', ')', close))
                        {
                            cursor = close;
                            continue;
                        }
                    }
                    open = cursor;
                    break;
                }
                else if (source[cursor] == ';' || source[cursor] == '{')
                    break;
            }
            if (open == Memory::String::npos)
            {
                error = "泛型函数缺少参数表";
                return false;
            }
            nameAt = open;
            while (nameAt > decl && std::isspace(static_cast<unsigned char>(source[nameAt - 1]))) --nameAt;
            while (nameAt > decl && identPart(source[nameAt - 1])) --nameAt;
        }
        if (nameAt >= source.size() || !identStart(source[nameAt]))
        {
            error = "找不到泛型声明名称";
            return false;
        }
        std::size_t nameEnd = nameAt + 1;
        while (nameEnd < source.size() && identPart(source[nameEnd])) ++nameEnd;
        item.name = source.substr(nameAt, nameEnd - nameAt);
        std::size_t end = 0;
        const std::size_t semicolon = source.find(';', nameEnd);
        const std::size_t body = source.find('{', nameEnd);
        const bool hostFunctionDecl = !structDecl && !enumDecl && !interfaceDecl && !hostTypeDecl && semicolon != Memory::String::npos && (body == Memory::String::npos || semicolon < body);
        if (hostTypeDecl || hostFunctionDecl)
        {
            end = semicolon;
            if (end == Memory::String::npos)
            {
                error = "泛型宿主声明 " + item.name + " 缺少 ;";
                return false;
            }
            ++end;
        }
        else
        {
            if (body == Memory::String::npos)
            {
                error = "泛型声明 " + item.name + " 缺少函数体";
                return false;
            }
            std::size_t bodyEnd = 0;
            if (!balanced(source, body, '{', '}', bodyEnd))
            {
                error = "泛型声明 " + item.name + " 缺少 }";
                return false;
            }
            end = bodyEnd + 1;
            skipSpace(source, end);
            if ((structDecl || enumDecl || interfaceDecl) && end < source.size() && source[end] == ';') ++end;
        }
        item.declaration = source.substr(decl, end - decl);
        item.nameOffset = nameAt - decl;
        const std::size_t line = lineOf(source, start);
        if (line < lines.size()) item.origin = lines[line];
        if (generics.count(item.name))
        {
            error = "重复的泛型声明 " + item.name;
            return false;
        }
        generics.emplace(item.name, item);
        for (std::size_t erase = start; erase < end; ++erase)
            if (source[erase] != '\n' && source[erase] != '\r') source[erase] = ' ';
        at = end;
    }
    return true;
}

/**
 * @brief 生成具体泛型接口名称，供派生请求与普通接口约束使用相同身份。
 * @param name 模板名称。
 * @param args 实际类型参数。
 * @return 带稳定类型指纹的实例名。
 */
inline Memory::String mangle(const Memory::String &name, const Memory::Vector<Memory::String> &args);

/**
 * @brief 收集显式及请求派生的接口身份，排除字符串和注释中的伪声明。
 * @param source 已规范化的声明与普通源码。
 * @param implemented 输出具体类型到接口身份集合的映射。
 */
inline void collectInterfaces(const Memory::String &source,
                              Memory::Map<Memory::String, Memory::Set<Memory::String>> &implemented)
{
    Memory::String text = source;
    for (std::size_t i = 0; i < text.size(); ++i)
    {
        if (text[i] == '"' || text[i] == '\'')
        {
            char q = text[i];
            text[i] = ' ';
            while (++i < text.size())
            {
                char c = text[i];
                text[i] = ' ';
                if (c == '\\' && i + 1 < text.size())
                    text[++i] = ' ';
                else if (c == q)
                    break;
            }
        }
        else if (i + 1 < text.size() && text[i] == '/' && (text[i + 1] == '/' || text[i + 1] == '*'))
        {
            bool line = text[i + 1] == '/';
            text[i++] = ' ';
            text[i] = ' ';
            while (++i < text.size())
            {
                char c = text[i];
                if (line && c == '\n') break;
                if (!line && c == '*' && i + 1 < text.size() && text[i + 1] == '/')
                {
                    text[i++] = ' ';
                    text[i] = ' ';
                    break;
                }
                if (c != '\n') text[i] = ' ';
            }
        }
    }
    Memory::String module;
    Memory::Vector<Memory::String> derived;
    for (std::size_t at = 0; at < text.size(); ++at)
    {
        if (wordAt(text, at, "#derive"))
        {
            auto open = at + 7;
            skipSpace(text, open);
            auto end = open;
            derived.clear();
            if (open < text.size() && text[open] == '(' && magel_macro::groupArguments(text, open, derived, end)) at = end - 1;
            continue;
        }
        if (wordAt(text, at, "module"))
        {
            at += 6u;
            skipSpace(text, at);
            const auto begin = at;
            while (at < text.size() && (identPart(text[at]) || text[at] == ':')) ++at;
            module = text.substr(begin, at - begin);
            if (module == "__root") module.clear();
        }
        bool structure = wordAt(text, at, "struct"), isInterface = wordAt(text, at, "interface"), enumeration = wordAt(text, at, "enum");
        if (!structure && !isInterface && !enumeration) continue;
        at += structure ? 6 : isInterface ? 9 :
                                            4;
        skipSpace(text, at);
        std::size_t begin = at;
        while (at < text.size() && identPart(text[at])) ++at;
        auto name = text.substr(begin, at - begin);
        if (isInterface && name == "ToString")
            implemented["#string-protocol-declarations"].insert(module.empty() ? name : module + "::" + name);
        for (const auto &item : derived)
        {
            const auto trait = trim(item);
            if (trait != "Equal" && trait != "Hash" && trait != "Clone") continue;
            const auto owner = module.empty() ? name : module + "::" + name;
            const auto interface = trait == "Equal" ? mangle(trait, { "bool", owner }) : mangle(trait, { owner });
            implemented[name].insert(interface);
            implemented[owner].insert(interface);
        }
        derived.clear();
        skipSpace(text, at);
        if (at >= text.size() || text[at] != ':') continue;
        ++at;
        while (at < text.size() && text[at] != '{')
        {
            skipSpace(text, at);
            begin = at;
            while (at < text.size() && (identPart(text[at]) || text[at] == ':')) ++at;
            if (at > begin)
            {
                const auto base = text.substr(begin, at - begin);
                implemented[name].insert(base);
                if (!module.empty())
                {
                    implemented[module + "::" + name].insert(base);
                    if (base.find("::") == Memory::String::npos)
                    {
                        implemented[name].insert(module + "::" + base);
                        implemented[module + "::" + name].insert(module + "::" + base);
                    }
                }
            }
            int nesting = 0;
            while (at < text.size() && text[at] != '{' && !(text[at] == ',' && nesting == 0))
            {
                if (text[at] == '<')
                    ++nesting;
                else if (text[at] == '>')
                    --nesting;
                ++at;
            }
            if (at < text.size() && text[at] == ',') ++at;
        }
    }
    bool changed = true;
    while (changed)
    {
        changed = false;
        for (auto &entry : implemented)
        {
            auto bases = entry.second;
            for (const auto &base : bases)
            {
                auto inherited = implemented.find(base);
                if (inherited != implemented.end())
                    for (const auto &ancestor : inherited->second) changed |= entry.second.insert(ancestor).second;
            }
        }
    }
}

/**
 * @brief 生成长度受控的实例名，完整类型拼写参与散列以区分标点不同的类型。
 * @param name 模板名称。
 * @param args 已归一化的实际类型参数。
 * @return 保留可读前缀和稳定类型指纹的实例名称。
 */
inline Memory::String mangle(const Memory::String &name, const Memory::Vector<Memory::String> &args)
{
    Memory::String result = name;
    for (const Memory::String &arg : args)
        result += "__" + magel_generic_name::argumentSymbol(arg);
    return result;
}

/**
 * @brief 用户声明覆盖的核心公开别名，供泛型实参归一使用。
 */
inline Memory::Set<Memory::String> coreAliasShadows;
/**
 * @brief 源码具名类型的限定身份，供唯一导入名称与限定名称归一使用。
 */
inline Memory::Map<Memory::String, Memory::Set<Memory::String>> nominalTypeNames;
/**
 * @brief 泛型具名类型的模块，用于归一生成实例的限定名称。
 */
inline Memory::Map<Memory::String, Memory::String> genericTypeModules;

/**
 * @brief 收集用户具名类型，用于保留与核心别名同名的泛型实参。
 * @param source 完整预处理源码。
 */
inline void collectCoreAliasShadows(const Memory::String &source)
{
    coreAliasShadows.clear();
    nominalTypeNames.clear();
    genericTypeModules.clear();
    Memory::String module;
    for (std::size_t at = 0; at < source.size();)
    {
        if (source.compare(at, 2, "//") == 0)
        {
            at = source.find('\n', at);
            if (at == Memory::String::npos) break;
            continue;
        }
        if (source.compare(at, 2, "/*") == 0)
        {
            const auto end = source.find("*/", at + 2);
            if (end == Memory::String::npos) break;
            at = end + 2;
            continue;
        }
        if (source[at] == '\"' || source[at] == '\'')
        {
            const auto quote = source[at++];
            while (at < source.size())
            {
                if (source[at] == '\\')
                    at += 2;
                else if (source[at++] == quote)
                    break;
            }
            continue;
        }
        if (!identStart(source[at]))
        {
            ++at;
            continue;
        }
        const auto begin = at++;
        while (at < source.size() && identPart(source[at])) ++at;
        const auto word = source.substr(begin, at - begin);
        if (word == "module")
        {
            skipSpace(source, at);
            const auto name = at;
            while (at < source.size() && (identPart(source[at]) || source[at] == ':')) ++at;
            module = source.substr(name, at - name);
            if (module == "__root") module.clear();
            continue;
        }
        if (word != "struct" && word != "enum" && word != "type" && word != "interface") continue;
        skipSpace(source, at);
        const auto name = at;
        while (at < source.size() && identPart(source[at])) ++at;
        const auto bare = source.substr(name, at - name);
        coreAliasShadows.insert(bare);
        nominalTypeNames[bare].insert(module.empty() ? bare : module + "::" + bare);
        std::size_t open = at;
        skipSpace(source, open);
        if (open < source.size() && source[open] == '<') genericTypeModules[bare] = module;
    }
}

inline Memory::String canonicalType(Memory::String value)
{
    value = trim(value);
    /* 引用限定符与 & 之间的空白不参与泛型类型身份。 */
    for (std::size_t pos = value.find('&'); pos != Memory::String::npos; pos = value.find('&', pos + 1u))
        while (pos > 0u && std::isspace(static_cast<unsigned char>(value[pos - 1u])))
            value.erase(--pos, 1u);
    /* 内部类型文本也使用可由 0.12.0 文法重新解析的返回类型前置形式。 */
    {
        int paren = 0, angle = 0;
        for (std::size_t at = 0; at < value.size(); ++at)
        {
            if (value[at] == '(')
                ++paren;
            else if (value[at] == ')')
                --paren;
            else if (value[at] == '<')
                ++angle;
            else if (value[at] == '>' && (at == 0 || value[at - 1] != '-'))
                --angle;
            else if (paren == 0 && angle == 0 && at > 0 && wordAt(value, at, "fn"))
            {
                std::size_t open = at + 2u, close = 0;
                skipSpace(value, open);
                if (open < value.size() && value[open] == '(' && balanced(value, open, '(', ')', close) && trim(value.substr(close + 1u)).empty())
                {
                    Memory::String parameters;
                    const auto parts = splitTop(value.substr(open + 1u, close - open - 1u));
                    for (std::size_t index = 0; index < parts.size(); ++index)
                    {
                        if (index) parameters += ",";
                        const auto &part = parts[index];
                        const bool variadic = part.size() >= 3u && part.compare(part.size() - 3u, 3u, "...") == 0;
                        parameters += canonicalType(variadic ? part.substr(0, part.size() - 3u) : part);
                        if (variadic) parameters += "...";
                    }
                    return canonicalType(value.substr(0, at)) + " fn(" + parameters + ")";
                }
            }
        }
    }
    /* 复合类型先递归归一叶子，避免别名因引用或元组包装产生不同实例。 */
    for (const Memory::String suffix : { Memory::String(" const&"), Memory::String(" mut&") })
    {
        if (value.size() > suffix.size() && value.compare(value.size() - suffix.size(), suffix.size(), suffix) == 0)
            return canonicalType(value.substr(0, value.size() - suffix.size())) + suffix;
    }
    if (!value.empty() && value.back() == '&')
        return canonicalType(value.substr(0, value.size() - 1u)) + " const&";
    if (value.size() > 2u && value.front() == '(' && value.back() == ')')
    {
        const auto parts = splitTop(value.substr(1, value.size() - 2u));
        if (parts.size() == 1u && (wordAt(trim(parts[0]), 0, "fn") || parts[0].find(" fn(") != Memory::String::npos))
            return "(" + canonicalType(parts[0]) + ")";
        if (parts.size() >= 2u)
        {
            Memory::String tuple = "(";
            for (std::size_t index = 0; index < parts.size(); ++index)
            {
                if (index) tuple += ",";
                tuple += canonicalType(parts[index]);
            }
            return tuple + ")";
        }
    }
    const Memory::Map<Memory::String, Memory::String> aliases = {
        { "std::lang::String", "std::lang::String" },
        { "std::lang::BigInteger", "std::lang::BigInteger" },
        { "std::lang::BigDecimal", "std::lang::BigDecimal" },
        { "std::lang::File", "file" },
        { "std::lang::Process", "proc" },
        { "std::lang::Any", "std::lang::Any" },
    };
    const Memory::Map<Memory::String, Memory::String> implicitAliases = {
        { "String", "std::lang::String" }, { "BigInteger", "std::lang::BigInteger" }, { "BigDecimal", "std::lang::BigDecimal" }, { "File", "file" }, { "Process", "proc" }, { "Any", "std::lang::Any" }
    };
    const auto implicit = implicitAliases.find(value);
    if (implicit != implicitAliases.end() && !coreAliasShadows.count(value)) return implicit->second;
    for (const auto &container : { Memory::String("List"), Memory::String("Set"), Memory::String("Map") })
    {
        Memory::String prefix = container + "<";
        if (value.compare(0, prefix.size(), prefix) == 0) value = "std::lang::" + value;
        prefix = "std::lang::" + prefix;
        if (value.compare(0, prefix.size(), prefix) != 0 || value.back() != '>') continue;
        auto parts = splitTop(value.substr(prefix.size(), value.size() - prefix.size() - 1));
        value = prefix;
        for (std::size_t index = 0; index < parts.size(); ++index)
        {
            if (index) value += ",";
            value += canonicalType(parts[index]);
        }
        return value + ">";
    }
    const auto found = aliases.find(value);
    if (found != aliases.end()) return found->second;
    const auto nominal = nominalTypeNames.find(value);
    if (nominal != nominalTypeNames.end() && nominal->second.size() == 1u) return *nominal->second.begin();
    for (const auto &generic : genericTypeModules)
    {
        if (!generic.second.empty())
        {
            const auto prefix = generic.second + "::" + generic.first + "__";
            if (value.compare(0, prefix.size(), prefix) == 0) return value.substr(generic.second.size() + 2u);
        }
    }
    for (const auto &nominal : nominalTypeNames)
        if (nominal.second.count(value)) return value;
    /* `std::模块::类型` 一律按**裸类型名**参与改名与类型同一性：同一份
     * `Result<FileStream, IoError>` 不能因为写成了限定形式就变成另一个类型。 */
    if (value.compare(0u, 5u, "std::") == 0u && value.compare(0u, 11u, "std::lang::") != 0u)
    {
        const std::size_t last = value.rfind("::");
        if (last != Memory::String::npos && value.find('<', last) == Memory::String::npos)
            return value.substr(last + 2u);
    }
    if (value.compare(0u, 11u, "std::lang::") == 0u && value.compare(0u, 16u, "std::lang::List<") != 0u && value.compare(0u, 15u, "std::lang::Set<") != 0u && value.compare(0u, 15u, "std::lang::Map<") != 0u && value.find("::", 11u) == Memory::String::npos)
        return value.substr(11u);
    return value;
}

/**
 * @brief 单态化实例的实际参数、生成名称与已展开接口约束。
 */
struct Expansion
{
    const Generic *generic = nullptr;
    Memory::Vector<Memory::String> args;
    Memory::String name;
    Memory::Vector<Memory::String> constraints;
};

/**
 * @brief 一次编译保留的模板与特化记录，供语义分析按实参补充实例。
 */
struct Session
{
    Memory::Map<Memory::String, Generic> generics;
    Memory::Map<Memory::String, Memory::Set<Memory::String>> implemented;
    Memory::Map<Memory::String, Expansion> made;
    Memory::Vector<Memory::String> pending;
    std::size_t emitted = 0;
};
inline Session session;

/**
 * @brief 按标识符替换类型参数，保留字符串、字符和注释的原始内容。
 * @param source 待替换的声明或类型表达式。
 * @param values 类型参数与实际类型的映射。
 * @param nameOffset 可选的模板声明名称位置。
 * @param name 模板声明的实例名称。
 * @return 完成替换的文本。
 */
inline Memory::String substituteIdentifiers(const Memory::String &source,
                                            const Memory::Map<Memory::String, Memory::String> &values,
                                            std::size_t nameOffset = Memory::String::npos, const Memory::String &name = {})
{
    Memory::String result;
    for (std::size_t at = 0; at < source.size();)
    {
        const std::size_t begin = at;
        if (source.compare(at, 2, "//") == 0 || source.compare(at, 2, "/*") == 0)
        {
            const bool line = source[at + 1] == '/';
            const auto end = source.find(line ? "\n" : "*/", at + 2);
            at = end == Memory::String::npos ? source.size() : end + (line ? 1u : 2u);
        }
        else if (source[at] == '"' || source[at] == '\'')
        {
            const char quote = source[at++];
            while (at < source.size())
            {
                if (source[at] == '\\' && at + 1 < source.size())
                {
                    at += 2;
                    continue;
                }
                if (source[at++] == quote) break;
            }
        }
        else if (identStart(source[at]))
        {
            ++at;
            while (at < source.size() && identPart(source[at])) ++at;
            const auto word = source.substr(begin, at - begin);
            const auto found = values.find(word);
            if (found != values.end())
            {
                std::size_t dot = at;
                skipSpace(source, dot);
                if (dot < source.size() && source[dot] == '.')
                {
                    std::size_t member = dot + 1;
                    skipSpace(source, member);
                    const auto start = member;
                    if (member < source.size() && identStart(source[member]))
                    {
                        while (member < source.size() && identPart(source[member])) ++member;
                        result += found->second + "::" + source.substr(start, member - start);
                        at = member;
                        continue;
                    }
                }
            }
            result += begin == nameOffset ? name : found == values.end() ? word :
                                                                           found->second;
            continue;
        }
        else
            ++at;
        result += source.substr(begin, at - begin);
    }
    return result;
}

/**
 * @brief 以实际类型参数生成声明，并保留标准可选值的引用访问器语义。
 * @param expansion 模板及其完整类型实参。
 * @return 已替换参数和声明名称的源码。
 */
inline Memory::String substitute(const Expansion &expansion)
{
    Memory::Map<Memory::String, Memory::String> values;
    for (std::size_t i = 0; i < expansion.args.size(); ++i)
        values[expansion.generic->params[i].name] = expansion.args[i];
    Memory::String result = substituteIdentifiers(expansion.generic->declaration, values,
                                                  expansion.generic->nameOffset, expansion.name);
    // 引用访问器没有默认空值：成功直接返回载荷，失败路径停止执行。
    if (expansion.generic->name == "Option" || expansion.generic->name == "Result")
    {
        for (const Memory::String method : { Memory::String("unwrap"), Memory::String("unwrapOr"), Memory::String("expect"), Memory::String("error") })
        {
            const std::size_t parameter = method == "error" ? 1u : 0u;
            if (parameter >= expansion.args.size()) continue;
            const auto &reference = expansion.args[parameter];
            if (reference.find("mut&") == Memory::String::npos && reference.find("const&") == Memory::String::npos) continue;
            const auto begin = result.find(" " + method + "(");
            if (begin == Memory::String::npos) continue;
            const auto open = result.find('{', begin);
            std::size_t close = 0;
            if (!balanced(result, open, '{', '}', close)) continue;
            Memory::String body = result.substr(open, close - open + 1);
            const auto replaceAll = [&](const Memory::String &from, const Memory::String &to) {
                for (std::size_t at = 0; (at = body.find(from, at)) != Memory::String::npos; at += to.size()) body.replace(at, from.size(), to);
            };
            replaceAll(reference + " result;", "");
            replaceAll(reference + " result = fallback;", "");
            replaceAll("result = value;", "return value;");
            replaceAll("result = error;", "return error;");
            replaceAll("return result;", method == "unwrapOr" ? "return fallback;" : "std::lang::panic(\"不可从空的可选值获取引用\");");
            result.replace(open, close - open + 1, body);
        }
    }
    return expansion.generic->derivation + result;
}

/**
 * @brief 检查具体类型表达式的嵌套深度，避免归一化默认类型时耗尽调用栈。
 * @param text 类型表达式。
 * @param error 超限时接收诊断。
 * @return 嵌套深度未达到上限时返回 true。
 */
inline bool checkTypeNesting(const Memory::String &text, Memory::String &error)
{
    std::size_t nesting = 0;
    for (std::size_t i = 0; i < text.size(); ++i)
    {
        const char value = text[i];
        if (value == '<' || value == '(' || value == '[')
        {
            if (++nesting >= 128u)
            {
                error = "泛型类型嵌套超过 128 层，请检查递归类型展开";
                return false;
            }
        }
        else if ((value == '>' && (i == 0 || text[i - 1] != '-')) || value == ')' || value == ']')
        {
            if (nesting) --nesting;
        }
    }
    return true;
}

inline bool rewrite(Memory::String &text, const Memory::Map<Memory::String, Generic> &generics,
                    const Memory::Map<Memory::String, Memory::Set<Memory::String>> &implemented,
                    Memory::Map<Memory::String, Expansion> &made, Memory::Vector<Memory::String> &pending,
                    Memory::String &error, std::size_t depth = 0)
{
    if (depth >= 128u)
    {
        error = "泛型类型嵌套超过 128 层，请检查递归默认类型";
        return false;
    }
    Memory::String output;
    for (std::size_t at = 0; at < text.size();)
    {
        if (text.compare(at, 2, "//") == 0)
        {
            const std::size_t end = text.find('\n', at);
            const std::size_t stop = end == Memory::String::npos ? text.size() : end + 1;
            output += text.substr(at, stop - at);
            at = stop;
            continue;
        }
        if (text.compare(at, 2, "/*") == 0)
        {
            const std::size_t end = text.find("*/", at + 2);
            const std::size_t stop = end == Memory::String::npos ? text.size() : end + 2;
            output += text.substr(at, stop - at);
            at = stop;
            continue;
        }
        if (text[at] == '"' || text[at] == '\'')
        {
            const char quote = text[at];
            const std::size_t begin = at++;
            while (at < text.size())
            {
                if (text[at] == '\\' && at + 1 < text.size())
                {
                    at += 2;
                    continue;
                }
                if (text[at++] == quote) break;
            }
            output += text.substr(begin, at - begin);
            continue;
        }
        if (!identStart(text[at]))
        {
            output += text[at++];
            continue;
        }
        const std::size_t begin = at++;
        while (at < text.size() && identPart(text[at])) ++at;
        const Memory::String name = text.substr(begin, at - begin);
        const auto generic = generics.find(name);
        std::size_t open = at;
        while (open < text.size() && std::isspace(static_cast<unsigned char>(text[open]))) ++open;
        const bool qualifierMatches = generic == generics.end() || generic->second.requiredQualifier.empty() || (begin >= generic->second.requiredQualifier.size() && text.compare(begin - generic->second.requiredQualifier.size(), generic->second.requiredQualifier.size(), generic->second.requiredQualifier) == 0);
        if (generic == generics.end() || !qualifierMatches || open >= text.size() || text[open] != '<')
        {
            output += name;
            continue;
        }
        std::size_t close = 0;
        if (!balanced(text, open, '<', '>', close))
        {
            error = name + " 的类型实参缺少 >";
            return false;
        }
        Memory::Vector<Memory::String> args = splitTop(text.substr(open + 1, close - open - 1));
        for (Memory::String &arg : args)
        {
            if (!checkTypeNesting(arg, error)) return false;
            if (!rewrite(arg, generics, implemented, made, pending, error, depth + 1u)) return false;
            arg = canonicalType(arg);
        }
        if (args.size() > generic->second.params.size())
        {
            error = name + " 至多接受 " + Memory::toString(generic->second.params.size()) + " 个类型实参";
            return false;
        }
        Memory::Map<Memory::String, Memory::String> bindings;
        auto constants = constantArguments;
        const auto evaluateArgument = [&](std::size_t index, Memory::String &argument) {
            const auto &parameter = generic->second.params[index];
            if (!parameter.constant) return true;
            Memory::String problem;
            magel_macro::MacroCondition evaluator{ argument, constants, problem };
            const auto value = evaluator.run();
            if (!problem.empty() || value.string)
            {
                error = name + " 的 const 泛型实参 " + parameter.name + " 必须是 int64 编译期常量：" + (problem.empty() ? argument : problem);
                return false;
            }
            argument = Memory::toString(value.number);
            constants[parameter.name] = argument;
            return true;
        };
        for (std::size_t i = 0; i < args.size(); ++i)
        {
            if (!evaluateArgument(i, args[i])) return false;
            bindings[generic->second.params[i].name] = args[i];
        }
        while (args.size() < generic->second.params.size())
        {
            const auto &parameter = generic->second.params[args.size()];
            if (!parameter.hasDefault)
            {
                error = name + " 缺少类型实参 " + parameter.name + "，请显式指定类型；不会默认采用 Any";
                return false;
            }
            auto fallback = substituteIdentifiers(parameter.fallback, bindings);
            if (!checkTypeNesting(fallback, error)) return false;
            if (!rewrite(fallback, generics, implemented, made, pending, error, depth + 1u)) return false;
            fallback = canonicalType(fallback);
            if (!evaluateArgument(args.size(), fallback)) return false;
            bindings[parameter.name] = fallback;
            args.push_back(std::move(fallback));
        }
        Memory::Vector<Memory::String> constraints;
        for (const auto &parameter : generic->second.params)
        {
            auto constraint = substituteIdentifiers(parameter.constraint, bindings);
            if (!constraint.empty() && !rewrite(constraint, generics, implemented, made, pending, error, depth + 1u)) return false;
            constraints.push_back(std::move(constraint));
        }
        const Memory::String specialized = mangle(name, args);
        const auto existing = made.find(specialized);
        if (existing != made.end() && (existing->second.generic != &generic->second || existing->second.args != args))
        {
            error = "泛型实例名称冲突：" + name;
            return false;
        }
        if (!generic->second.intrinsic && existing == made.end())
        {
            if (pending.size() >= 1024u)
            {
                error = "泛型实例化超过 1024 个实例，请检查递归类型展开";
                return false;
            }
            made[specialized] = Expansion{ &generic->second, args, specialized, constraints };
            pending.push_back(specialized);
        }
        output += specialized;
        at = close + 1;
    }
    text.swap(output);
    return true;
}

/**
 * @brief 在全部嵌套实例展开后检查约束，包含泛型实例实现的接口及传递继承。
 * @param made 本次编译的实例记录。
 * @param implemented 普通声明与生成声明实现的接口。
 * @param error 失败时接收具体实例的诊断。
 * @return 所有具体类型实参满足约束时返回 true。
 */
inline bool validateConstraints(const Memory::Map<Memory::String, Expansion> &made,
                                const Memory::Map<Memory::String, Memory::Set<Memory::String>> &implemented,
                                Memory::String &error)
{
    for (const auto &item : made)
    {
        const auto &expansion = item.second;
        for (std::size_t i = 0; i < expansion.args.size(); ++i)
        {
            const auto &constraint = expansion.constraints[i];
            if (constraint.empty()) continue;
            // 标准字符串能力由每个值类型的默认 toString 提供，具体方法签名由语义阶段核验。
            const auto declarations = implemented.find("#string-protocol-declarations");
            bool stringCapability = constraint == "std::io::ToString" ||
                (constraint == "ToString" && declarations != implemented.end() && declarations->second.size() == 1 && declarations->second.count("std::io::ToString"));
            const auto protocol = made.find(constraint);
            if (protocol != made.end() && (protocol->second.generic->module == "std::lang" || protocol->second.generic->origin.file == "<内建 ImplicitConvert>") &&
                protocol->second.generic->name == "ImplicitConvert" && protocol->second.args.size() == 1 &&
                (protocol->second.args[0] == "String" || protocol->second.args[0] == "std::lang::String"))
                stringCapability = true;
            if (stringCapability && expansion.args[i] != "void") continue;
            const auto concrete = implemented.find(expansion.args[i]);
            if (concrete == implemented.end() || !concrete->second.count(constraint))
            {
                error = expansion.generic->name + " 的类型实参 " + expansion.args[i] + " 未实现接口 " + constraint;
                return false;
            }
        }
    }
    return true;
}

inline bool run(preprocess::Result &result, Memory::String &error)
{
    collectConstantArguments(result.text);
    session = Session{};
    collectCoreAliasShadows(result.text);
    Memory::Map<Memory::String, Memory::String> derivedGenerics;
    if (!collectGenericDerives(result.text, derivedGenerics, error)) return false;
    // 泛型方法的显式错误类型交给语义层补齐其余参数；跳过注释与字符串。
    for (std::size_t at = 0; at < result.text.size(); ++at)
    {
        if (result.text.compare(at, 2, "//") == 0)
        {
            auto end = result.text.find('\n', at);
            if (end == Memory::String::npos) break;
            at = end;
            continue;
        }
        if (result.text.compare(at, 2, "/*") == 0)
        {
            auto end = result.text.find("*/", at + 2);
            if (end == Memory::String::npos) break;
            at = end + 1;
            continue;
        }
        if (result.text[at] == '"' || result.text[at] == '\'')
        {
            auto quote = result.text[at++];
            while (at < result.text.size())
            {
                if (result.text[at] == '\\')
                {
                    at += 2;
                    continue;
                }
                if (result.text[at] == quote) break;
                ++at;
            }
            continue;
        }
        if (result.text[at] != '.') continue;
        std::size_t begin = at + 1;
        skipSpace(result.text, begin);
        for (const auto &pair : { std::make_pair("mapErr", "__magelMapErr"), std::make_pair("okOr", "__magelOkOr"), std::make_pair("okOrElse", "__magelOkOrElse") })
        {
            if (!wordAt(result.text, begin, pair.first)) continue;
            auto end = begin + Memory::String(pair.first).size();
            skipSpace(result.text, end);
            if (end < result.text.size() && result.text[end] == '<') result.text.replace(begin, Memory::String(pair.first).size(), pair.second);
        }
    }
    normalizeGenericSyntax(result.text);
    auto &generics = session.generics;
    const auto addBuiltin = [&](const Memory::String &name, Memory::Vector<Parameter> params,
                                const Memory::String &declaration, std::size_t nameOffset) {
        Generic builtin;
        builtin.name = name;
        builtin.params = std::move(params);
        builtin.declaration = declaration;
        builtin.nameOffset = nameOffset;
        builtin.origin.file = "<内建 " + name + ">";
        builtin.origin.line = 1u;
        generics.emplace(name, std::move(builtin));
    };
    const Parameter resultType{ "R", "", "std::lang::Any" };
    const Parameter operand{ "T", "", "std::lang::Any" };
    /* 名字约定：左边机器（接收者在左）**不带方向前缀**、用全名（`Add` / `Subtract` /
     * `Multiply` / `Divide` / `Modulo` / `Equal` / `NotEqual` / `Less` / `LessOrEqual` /
     * `Greater` / `GreaterOrEqual` / `And` / `Or` / `Xor` / `ShiftLeft` / `ShiftRight`）；
     * 右边机器保留 `Right` 前缀、同样用全名（`RightAdd` / `RightSubtract` / …）。
     * pair 的第二个是**方法名后缀**，仍沿用老缩写（`leftAdd` / `leftSub` / `leftMul` …），
     * `magel_epilogue.cpp` 的 `OperatorInterfaceSpecs()` 按方法名配方向，两边必须一致。 */
    const Memory::Vector<std::pair<Memory::String, Memory::String>> operators = {
        { "Add", "Add" },
        { "Subtract", "Sub" },
        { "Multiply", "Mul" },
        { "Divide", "Div" },
        { "Modulo", "Mod" },
        { "Equal", "Equal" },
        { "NotEqual", "NotEqual" },
        { "Less", "Less" },
        { "LessOrEqual", "LessEqual" },
        { "Greater", "Greater" },
        { "GreaterOrEqual", "GreaterEqual" },
        { "And", "BitAnd" },
        { "Or", "BitOr" },
        { "Xor", "BitXor" },
        { "ShiftLeft", "ShiftLeft" },
        { "ShiftRight", "ShiftRight" },
    };
    for (const auto &item : operators)
    {
        for (const Memory::String &side : { Memory::String("Left"), Memory::String("Right") })
        {
            const Memory::String name = (side == "Left") ? item.first : ("Right" + item.first);
            const Memory::String method = (side == "Left" ? "left" : "right") + item.second;
            addBuiltin(name, { resultType, operand },
                       "interface " + name + " { R " + method + "(T value); };", 10u);
        }
    }
    /* 一元运算符：接口方法只有一个参数，就是**操作数本身**（接收者）。
     * 与二元接口的区别是"没有另一个操作数"。 */
    const Memory::Vector<std::pair<Memory::String, Memory::String>> unaryOperators = {
        { "Negate", "negate" },
        { "BitNot", "bitNot" },
        { "Positive", "positive" },
    };
    for (const auto &item : unaryOperators)
    {
        addBuiltin(item.first, { resultType, operand },
                   "interface " + item.first + " { R " + item.second + "(T value); };", 10u);
    }
    addBuiltin("Hash", { Parameter{ "T", "", "std::lang::Any" } },
               "interface Hash { uint64 hash(T value); };", 10u);
    addBuiltin("Clone", { Parameter{ "T", "", "std::lang::Any" } },
               "interface Clone { T clone(); };", 10u);
    addBuiltin("ExplicitConvert", { Parameter{ "T", "", "std::lang::Any" } },
               "interface ExplicitConvert { T explicitConvert(); };", 10u);
    addBuiltin("ImplicitConvert", { Parameter{ "T", "", "std::lang::Any" } },
               "interface ImplicitConvert { T implicitConvert(); };", 10u);
    addBuiltin("Index", { Parameter{ "R", "", "std::lang::Any" }, Parameter{ "T", "", "std::lang::Any" } },
               "interface Index { R index(T key); };", 10u);
    addBuiltin("Iterator", { Parameter{ "R", "", "std::lang::Any" } },
               R"MAGEL(interface Iterator {
    Option<R> iterator(int64 index);
};)MAGEL",
               10u);
    const auto addPointerConversion = [&](const Memory::String &name, const Memory::String &declaration) {
        addBuiltin(name, { Parameter{ "T", "", "std::lang::Any" } }, declaration, declaration.find(name));
        generics[name].module = "std::lang";
    };
    addPointerConversion("toReference", R"MAGEL(T mut& toReference(Option<T mut&> value) {
    return value.unwrap();
})MAGEL");
    const Memory::String toOptional = R"MAGEL(Option<T mut&> toOptional(T mut& value) {
    return Option<T mut&>::Value(value);
})MAGEL";
    addPointerConversion("toOptional", toOptional);

    /* 标准算法是编译器 intrinsic：这里仅让泛型预处理器吃掉显式 `<T>`，不生成
     * Magel 包装函数。限定名约束避免误改用户自己的 `findValue<T>` 等泛型。
     *
     * 算法分两组：`xxxValue` 收 / 返回 `std::lang::List<T>`（保持原有的列表语义），
     * `xxxPointer` 收 `T mut&` / `T const&` + 个数、直接在调用者缓冲区上操作
     * （见下面 `module std::algorithms;` 里的那批泛型函数）。 */
    Memory::Map<Memory::String, Generic> algorithmIntrinsics;
    for (const Memory::String &name : {
             Memory::String("findValue"), Memory::String("sortValue"),
             Memory::String("binarySearchValue"),
             Memory::String("minValue"), Memory::String("maxValue"),
             Memory::String("clampValue"),
             Memory::String("allValue"), Memory::String("anyValue"),
             Memory::String("foldValue"),
             Memory::String("mapValue"), Memory::String("filterValue"),
             Memory::String("reduceValue"),
             Memory::String("sumValue"), Memory::String("countValue") })
    {
        Generic algorithm;
        algorithm.name = name;
        algorithm.params = { Parameter{ "T", "", "std::lang::Any" } };
        algorithm.intrinsic = true;
        algorithm.requiredQualifier = "std::algorithms::";
        algorithm.origin.file = "<内建 std::algorithms::" + name + ">";
        algorithm.origin.line = 1u;
        algorithmIntrinsics.emplace(name, std::move(algorithm));
    }

    const Memory::String rangeBuiltinRaw = R"MAGEL(
module std::lang;

struct __Range : Iterator<int64> {
    int64 start;
    int64 stop;
    int64 step;

    Option<int64> iterator(int64 index) {
        int64 value = self.start + index * self.step;
        if (self.step > 0 && value < self.stop) { return Option<int64>::Value(value); }
        if (self.step < 0 && value > self.stop) { return Option<int64>::Value(value); }
        return Option<int64>::None;
    }
};

__Range range(int64 start, int64 stop) {
    __Range result;
    result.start = start;
    result.stop = stop;
    result.step = 1;
    return result;
}

__Range range(int64 start, int64 stop, int64 step) {
    std::lang::assert(step != 0, "range 的步长不能为 0");
    __Range result;
    result.start = start;
    result.stop = stop;
    result.step = step;
    return result;
}

/* fileOpen / fileRead / … 与 processRun / system 已迁到 std/io.mgl 与 std/process.mgl：
 * 现在要 `import std::io;` / `import std::process;` 才可见。 */

/* `std::lang::Math` 已迁到 std/math.mgl：现在要 `import std::math;` 才可见。 */

/* 已迁到 std/：见下面的说明注释。 */


module std::algorithms;

/*
 * `choice` / `shuffle` 放这儿不是随手挑的，两条语言约束逼出来的：
 *   ① MAGEL **不支持泛型方法**（`T pick<T>(...)` 是语法错；`impl` 只用来实现宿主接口、
 *      不能给 struct 加方法），所以它们没法做成 `Random::choice` 这样的成员；
 *   ② `std::lang::Random` 这个名字已经被同名的 **struct** 占着（withSeed / fromEntropy /
 *      nextUInt64 / nextInt / nextFloat64），再开一个同名模块会让
 *      `std::lang::Random::withSeed(…)` 被当成"模块函数"而找不到。
 * 放 `std::algorithms` 也顺理成章：`shuffleValue` 本来就是 <algorithm> 里的东西。
 *
 * 两个函数都**不改 Random 的状态**，而是收一个随机数当种子 —— 调用点自己
 * `rng.nextUInt64()` 往前推，这样"取一个 / 洗一副"的顺序仍然完全可复现。
 */

/** 从表里随机取一个元素；空表直接停机（和 `at` 越界一个口径）。 */
T choiceValue<T>(std::lang::List<T> items, uint64 seed) {
    std::lang::assert(items.len() > 0, "std::algorithms::choiceValue 不能作用在空表上");
    return items.at((int64)(seed % (uint64)items.len()));
}

/**
 * Fisher-Yates 洗牌，返回**打乱后的副本**（原表不动）。
 * 从后往前逐个和前面 `[0, i]` 里的一个随机位置交换；同一个 `seed` 结果可复现。
 */
std::lang::List<T> shuffleValue<T>(std::lang::List<T> items, uint64 seed) {
    std::lang::List<T> result = items;
    uint64 state = seed;
    int64 index = result.len() - 1;
    while (index > 0) {
        state = state * 6364136223846793005 + 1442695040888963407;
        int64 other = (int64)(state % (uint64)(index + 1));
        /* 列表没有 `set` 方法（`set` 还是容器关键字），用下标赋值。 */
        T swap = result[index];
        result[index] = result[other];
        result[other] = swap;
        index = index - 1;
    }
    return result;
}

/**
 * Fisher-Yates 洗牌的**引用形式**：直接在 `data` 里 `count` 个元素上原地打乱。
 * 同一个 `seed` 结果可复现，和 `shuffleValue` 用同一条 LCG。
 */
void shuffleReference<T>(T mut& data, int64 count, uint64 seed) {
    uint64 state = seed;
    int64 index = count - 1;
    while (index > 0) {
        state = state * 6364136223846793005 + 1442695040888963407;
        int64 other = (int64)(state % (uint64)(index + 1));
        T swap = std::lang::index(data, index);
        std::lang::index(data, index) = std::lang::index(data, other);
        std::lang::index(data, other) = swap;
        index = index - 1;
    }
}

/** Pair corresponding elements; excess elements in the longer list are ignored. */
std::lang::List<(T, U)> zipValue<T, U>(std::lang::List<T> left, std::lang::List<U> right) {
    std::lang::List<(T, U)> result;
    int64 count = left.len();
    if (right.len() < count) { count = right.len(); }
    for (int64 index = 0; index < count; index = index + 1) {
        T leftValue = left[index];
        U rightValue = right[index];
        result.push((leftValue, rightValue));
    }
    return result;
}

/* ---- 引用形式的算法族 ----
 *
 * 上面这批（`sortValue` / `findValue` / `binarySearchValue` / `foldValue` …）都收 `std::lang::List<T>`，
 * `sortValue` 还要复制一份；这里给对照：统一收 `T mut& data, int64 count`，直接在调用者的
 * 缓冲区上读写，不造列表副本、不经过 VM 容器对象。`&raw[0]` / `raw`（运行时数组会
 * 自动退化）/ `std::memory::Span<T>.data` 都能直接喂进来。
 *
 * 约定和 `std::lang::List` 版一致：
 *   * 比较用 `>`（内建数值 / 字符串直接可用；结构体实现 `Greater<bool,T>` 定顺序）；
 *   * 返回下标的找不到时给 -1；返回可选引用的（`minReference` / `maxReference`）空区间给 `None`；
 *   * 调用点要写 `std::algorithms::` 前缀，并 `import std::algorithms;`。
 */

/** 原地排序，O(1) 额外空间：`data` 里 `count` 个元素直接排好。
 * 默认是插入排序（稳定）；构建编译器时定义 `SIMPLE_SORT` 改用希尔排序（把间距逐步
 * 缩到 1，同样就地、不额外分配，但**不稳定**）。VM 里的列表排序 `sortValue` 也吃同一个开关。 */
void sortReference<T>(T mut& data, int64 count) {
@@SORT_POINTER_IMPL@@
}

/** 原地反转 `[0, count)`。 */
void reverseReference<T>(T mut& data, int64 count) {
    int64 left = 0;
    int64 right = count - 1;
    while (left < right) {
        T swap = std::lang::index(data, left);
        std::lang::index(data, left) = std::lang::index(data, right);
        std::lang::index(data, right) = swap;
        left = left + 1;
        right = right - 1;
    }
}

/** 线性查找，返回下标；找不到返回 -1。 */
int64 findReference<T>(T const& data, int64 count, T value) {
    int64 index = 0;
    while (index < count) {
        if (std::lang::index(data, index) == value) { return index; }
        index = index + 1;
    }
    return -1;
}

/** 在按 `>` 升序排好的区间上二分，返回下标；找不到返回 -1。 */
int64 binarySearchReference<T>(T const& data, int64 count, T value) {
    int64 low = 0;
    int64 high = count - 1;
    while (low <= high) {
        int64 middle = low + (high - low) / 2;
        if (std::lang::index(data, middle) == value) { return middle; }
        if (std::lang::index(data, middle) > value) { high = middle - 1; } else { low = middle + 1; }
    }
    return -1;
}

/** 最小元素的地址；空区间返回 `None`。拿着它可以直接改元素。 */
Option<T mut&> minReference<T>(T mut& data, int64 count) {
    if (count <= 0) { return Option<T mut&>::None; }
    T mut& best = data;
    int64 index = 1;
    while (index < count) {
        if (*best > std::lang::index(data, index)) { best = &std::lang::index(data, index); }
        index = index + 1;
    }
    return Option<T mut&>::Value(best);
}

/** 最大元素的地址；空区间返回 `None`。 */
Option<T mut&> maxReference<T>(T mut& data, int64 count) {
    if (count <= 0) { return Option<T mut&>::None; }
    T mut& best = data;
    int64 index = 1;
    while (index < count) {
        if (std::lang::index(data, index) > *best) { best = &std::lang::index(data, index); }
        index = index + 1;
    }
    return Option<T mut&>::Value(best);
}

/** 等于 `value` 的元素个数。 */
int64 countReference<T>(T const& data, int64 count, T value) {
    int64 total = 0;
    int64 index = 0;
    while (index < count) {
        if (std::lang::index(data, index) == value) { total = total + 1; }
        index = index + 1;
    }
    return total;
}

/** 求和；空区间直接停机（泛型里没有通用的"零值"可造）。 */
T sumReference<T>(T mut& data, int64 count) {
    std::lang::assert(count > 0, "std::algorithms::sumReference 需要至少一个元素");
    T total = std::lang::index(data, 0);
    int64 index = 1;
    while (index < count) {
        total = total + std::lang::index(data, index);
        index = index + 1;
    }
    return total;
}

module std::lang;

/* 已迁到 std/：见下面的说明注释。 */


module std::lang;

/* 已迁到 std/：见下面的说明注释。 */


/* 已迁到 std/：见下面的说明注释。 */


module std::lang;

int64 __sharedCreate<T>(T value);
T __sharedGet<T>(int64 handle);
void __sharedSet<T>(int64 handle, T value);
T __sharedSwap<T>(int64 handle, T value);

interface Drop { void drop(); };

/* Closeable：把"释放宿主句柄"从"释放结构体自身资源"里分出来。
 *
 * 实现 Closeable 的值在生命周期结束时（出块 / return / `delete` / 停机清理）会自动调用
 * `close()`；用户显式调用过 `close()` 的变量不会再自动调用。Drop 与 Closeable 可以同时
 * 实现：析构顺序是先 `drop()` 再 `close()`。close() 应当对"未打开的句柄"（0）安全，
 * 因为它可能在默认初始化的值上跑。 */
interface Closeable { void close(); };

/*
/* `Text`（字符串工具族）与 `__str*` 宿主签名已迁到 std/text.mgl：
 * 现在要 `import std::text;` 才可见。 */

)MAGEL";
    /* `SIMPLE_SORT` 是**构建编译器时**的 C++ 宏（和功能裁剪宏一类，整个构建
     * 必须一致定义）：定义了就让排序改用希尔排序，否则用各自的默认实现。两套算法都是纯
     * 文本，这里挑一份塞进引用排序的宏体；VM 侧 `vecStableSort` 走同一个宏。 */
#ifdef SIMPLE_SORT
    const Memory::String sortReferenceImpl = R"MAGEL(    int64 gap = count / 2;
    while (gap > 0) {
        int64 index = gap;
        while (index < count) {
            T key = std::lang::index(data, index);
            int64 slot = index;
            while (slot >= gap && std::lang::index(data, slot - gap) > key) {
                std::lang::index(data, slot) = std::lang::index(data, slot - gap);
                slot = slot - gap;
            }
            std::lang::index(data, slot) = key;
            index = index + 1;
        }
        gap = gap / 2;
    }
)MAGEL";
#else
    const Memory::String sortReferenceImpl = R"MAGEL(    int64 index = 1;
    while (index < count) {
        T key = std::lang::index(data, index);
        int64 slot = index - 1;
        while (slot >= 0 && std::lang::index(data, slot) > key) {
            std::lang::index(data, slot + 1) = std::lang::index(data, slot);
            slot = slot - 1;
        }
        std::lang::index(data, slot + 1) = key;
        index = index + 1;
    }
)MAGEL";
#endif
    Memory::String rangeBuiltin = rangeBuiltinRaw;
    // 每种固定宽度整型使用内部状态迭代器；到达闭区间上界后不再递增。
    rangeBuiltin += "\nmodule std::lang;\n";
    for (const auto &type : { "int8", "uint8", "int16", "uint16", "int32", "uint32", "int64", "uint64" })
    {
        const Memory::String valueType = type;
        const Memory::String owner = "__Interval_" + valueType;
        rangeBuiltin += "struct " + owner + " : Iterator<" + valueType + "> { " + valueType + " current; " + valueType + " stop; bool inclusive; bool finished; " + "Option<" + valueType + "> iterator(int64 index) { " + "if(self.finished || (!self.inclusive && self.current==self.stop)){self.finished=true;return Option<" + valueType + ">::None;} " + valueType + " value=self.current; if(self.current==self.stop){self.finished=true;} " + "else{self.current=(" + valueType + ")(self.current+1);} return Option<" + valueType + ">::Value(value); } };\n" + owner + " __interval_" + valueType + "(" + valueType + " start," + valueType + " stop,bool inclusive){" + owner + " result;result.current=start;result.stop=stop;result.inclusive=inclusive;result.finished=false;return result;}\n";
    }

    const Memory::String sortReferenceMarker = "@@SORT_POINTER_IMPL@@";
    const std::size_t sortReferenceSlot = rangeBuiltin.find(sortReferenceMarker);
    if (sortReferenceSlot != Memory::String::npos)
        rangeBuiltin.replace(sortReferenceSlot, sortReferenceMarker.size(), sortReferenceImpl);
    result.text += rangeBuiltin;
    preprocess::LineOrigin rangeOrigin;
    rangeOrigin.file = "<内建 range>";
    rangeOrigin.line = 1u;
    const std::size_t rangeLines = static_cast<std::size_t>(
        std::count(rangeBuiltin.begin(), rangeBuiltin.end(), '\n'));
    result.lineMap.insert(result.lineMap.end(), rangeLines, rangeOrigin);
    /* 右向禁止标记接口 `NoRightXxx`：空接口、**没有类型实参**，所以永远不会被实例化
     * （`rewrite` 只在名字后面紧跟 `<` 时才展开）—— 直接当普通声明塞进前置文本。
     * 实现它表示「这个运算符的右操作数不许当接收者」；名字与 `RightXxx` 一一对应，
     * 两边同写由 `FindOperator` 判冲突。空接口没有方法，不参与方法登记。 */
    Memory::String noRightBuiltin("module __root;\n");
    for (const auto &item : operators)
    {
        noRightBuiltin += "interface NoRight" + item.first + " { };\n";
    }
    result.text += noRightBuiltin;
    const std::size_t noRightLines = static_cast<std::size_t>(
        std::count(noRightBuiltin.begin(), noRightBuiltin.end(), '\n'));
    result.lineMap.insert(result.lineMap.end(), noRightLines, rangeOrigin);

    Memory::Map<Memory::String, Memory::Set<Memory::String>> noImplementations;
    Memory::Map<Memory::String, Expansion> noExpansions;
    Memory::Vector<Memory::String> noPending;
    if (!rewrite(result.text, algorithmIntrinsics, noImplementations,
                 noExpansions, noPending, error)) return false;
    normalizeGenericSyntax(result.text);
    if (!collect(result.text, result.lineMap, generics, error)) return false;
    for (const auto &entry : derivedGenerics)
        if (generics.count(entry.first)) generics[entry.first].derivation = entry.second;
    auto &implemented = session.implemented;
    collectInterfaces(result.text, implemented);
    auto &made = session.made;
    auto &pending = session.pending;
    if (!rewrite(result.text, generics, implemented, made, pending, error)) return false;
    Memory::String generated;
    Memory::Vector<preprocess::LineOrigin> generatedOrigins;
    for (std::size_t index = 0; index < pending.size(); ++index)
    {
        const Expansion expansion = made[pending[index]];
        Memory::String declaration = substitute(expansion);
        if (!rewrite(declaration, generics, implemented, made, pending, error)) return false;
        const Memory::String module = expansion.generic->module.empty() ? Memory::String("__root") : expansion.generic->module;
        generated += "\nmodule " + module + ";\n" + declaration + "\n";
        const std::size_t lines = 3u + static_cast<std::size_t>(std::count(declaration.begin(), declaration.end(), '\n'));
        for (std::size_t line = 0; line < lines; ++line)
            generatedOrigins.push_back(expansion.generic->origin);
    }
    collectInterfaces(result.text + generated, implemented);
    if (!validateConstraints(made, implemented, error)) return false;
    result.text += generated;
    result.lineMap.insert(result.lineMap.end(), generatedOrigins.begin(), generatedOrigins.end());
    session.emitted = pending.size();
    markContainerTypeArguments(result.text);
    return true;
}

/**
 * @brief 将语义推导出的具体类型展开成普通声明；错误由调用方报告。
 * @param name 模板名称。
 * @param args 推导出的完整类型实参。
 * @param generated 接收本次新增声明。
 * @param error 失败时接收诊断。
 * @return 类型实参和约束有效且展开成功时返回 true。
 */
inline bool instantiate(const Memory::String &name, const Memory::Vector<Memory::String> &args,
                        Memory::String &generated, Memory::String &error)
{
    Memory::String request = name + "<";
    for (std::size_t index = 0; index < args.size(); ++index)
    {
        if (index) request += ",";
        request += args[index];
    }
    request += ">";
    if (!rewrite(request, session.generics, session.implemented, session.made, session.pending, error)) return false;
    while (session.emitted < session.pending.size())
    {
        const Expansion expansion = session.made.at(session.pending[session.emitted++]);
        auto declaration = substitute(expansion);
        if (!rewrite(declaration, session.generics, session.implemented, session.made, session.pending, error)) return false;
        generated += "\nmodule " + (expansion.generic->module.empty() ? Memory::String("__root") : expansion.generic->module)
                     + ";\n" + declaration + "\n";
    }
    collectInterfaces(generated, session.implemented);
    if (!validateConstraints(session.made, session.implemented, error)) return false;
    markContainerTypeArguments(generated);
    return true;
}

}  // namespace magel_generic

#endif
