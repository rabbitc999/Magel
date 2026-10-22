#include <algorithm>
#include <cctype>
#include <map>
#include <regex>
#include <set>
#include <string>
#include <vector>
#include "chali_lambda.hpp"

namespace chali_lambda
{

Memory::Map<Memory::String, Memory::Map<Memory::String, int>> captureSpecifications;

/**
 * @brief 移除文本两端的空白并保留内部文本。
 * @param value 原始文本。
 * @return 去除首尾空白的文本；全为空白时返回空串。
 */
Memory::String trim(const Memory::String &value)
{
    const std::size_t first = value.find_first_not_of(" \t\r\n");
    if (first == Memory::String::npos) return {};
    const std::size_t last = value.find_last_not_of(" \t\r\n");
    return value.substr(first, last - first + 1u);
}

/**
 * @brief 判断字符是否属于闭包源码中的标识符。
 * @param value 待检查的字符。
 * @return 字母、数字或下划线返回 true。
 */
bool ident(char value)
{
    return std::isalnum(static_cast<unsigned char>(value)) || value == '_';
}

/**
 * @brief 遮蔽注释和字面量内容，同时保持偏移和换行不变。
 * @param text 待分析的原始源码。
 * @return 与源码等长且只暴露代码结构的掩码。
 */
Memory::String codeMask(const Memory::String &text)
{
    Memory::String mask = text;
    char quote = 0;
    bool lineComment = false, blockComment = false;
    for (std::size_t at = 0; at < text.size(); ++at)
    {
        const char value = text[at];
        if (lineComment)
        {
            if (value == '\n')
                lineComment = false;
            else
                mask[at] = ' ';
            continue;
        }
        if (blockComment)
        {
            mask[at] = value == '\n' ? '\n' : ' ';
            if (value == '*' && at + 1u < text.size() && text[at + 1u] == '/')
            {
                mask[++at] = ' ';
                blockComment = false;
            }
            continue;
        }
        if (quote != 0)
        {
            mask[at] = ' ';
            if (value == '\\' && at + 1u < text.size())
                mask[++at] = ' ';
            else if (value == quote)
                quote = 0;
            continue;
        }
        if (value == '/' && at + 1u < text.size() && text[at + 1u] == '/')
        {
            mask[at] = ' ';
            mask[at + 1u] = ' ';
            ++at;
            lineComment = true;
            continue;
        }
        if (value == '/' && at + 1u < text.size() && text[at + 1u] == '*')
        {
            mask[at] = ' ';
            mask[at + 1u] = ' ';
            ++at;
            blockComment = true;
            continue;
        }
        if (value == '"' || value == '\'')
        {
            mask[at] = ' ';
            quote = value;
        }
    }
    return mask;
}

/**
 * @brief 跳过注释和字面量，查找成对分隔符的结束位置。
 * @param text 完整源码。
 * @param open 起始分隔符的位置。
 * @param left 左分隔符。
 * @param right 右分隔符。
 * @param close 成功时接收匹配的右分隔符位置。
 * @return 嵌套分隔符完整闭合时返回 true。
 */
bool balanced(const Memory::String &text, std::size_t open, char left, char right,
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
            if (value == '*' && at + 1u < text.size() && text[at + 1u] == '/')
            {
                blockComment = false;
                ++at;
            }
            continue;
        }
        if (quote != 0)
        {
            if (value == '\\' && at + 1u < text.size())
            {
                ++at;
                continue;
            }
            if (value == quote) quote = 0;
            continue;
        }
        if (value == '/' && at + 1u < text.size() && text[at + 1u] == '/')
        {
            lineComment = true;
            ++at;
            continue;
        }
        if (value == '/' && at + 1u < text.size() && text[at + 1u] == '*')
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
        else if (value == right && --depth == 0)
        {
            close = at;
            return true;
        }
    }
    return false;
}

/**
 * @brief 仅在最外层逗号处分割闭包调用实参。
 * @param text 可能包含嵌套调用的参数文本。
 * @return 各个实参的文本列表。
 */
Memory::Vector<Memory::String> split(const Memory::String &text)
{
    Memory::Vector<Memory::String> result;
    std::size_t begin = 0;
    int round = 0, square = 0, angle = 0;
    for (std::size_t at = 0; at <= text.size(); ++at)
    {
        const char value = at < text.size() ? text[at] : ',';
        if (value == '(')
            ++round;
        else if (value == ')')
            --round;
        if (value == '[')
            ++square;
        else if (value == ']')
            --square;
        if (value == '<')
            ++angle;
        else if (value == '>' && (at == 0 || text[at - 1u] != '-'))
            --angle;
        if (value == ',' && round == 0 && square == 0 && angle == 0)
        {
            result.push_back(trim(text.substr(begin, at - begin)));
            begin = at + 1u;
        }
    }
    if (result.size() == 1u && result[0].empty()) result.clear();
    return result;
}

/**
 * @brief 查找具有完整标识符边界的单词。
 * @param text 待查询的源码文本。
 * @param word 需要匹配的完整单词。
 * @return 至少存在一个边界完整的匹配时返回 true。
 */
bool hasWord(const Memory::String &text, const Memory::String &word)
{
    std::size_t at = text.find(word);
    while (at != Memory::String::npos)
    {
        const bool left = at == 0u || !ident(text[at - 1u]);
        const bool right = at + word.size() == text.size() || !ident(text[at + word.size()]);
        if (left && right) return true;
        at = text.find(word, at + word.size());
    }
    return false;
}

/**
 * @brief 从已知变量或基础字面量推断闭包调用实参类型。
 * @param argument 调用实参的原始文本。
 * @param variables 当前可见变量名及类型拼写。
 * @return 已知的类型拼写；文本阶段无法确定时返回空串。
 */
Memory::String inferArgument(const Memory::String &argument,
                             const Memory::Map<Memory::String, Memory::String> &variables)
{
    const Memory::String value = trim(argument);
    const auto found = variables.find(value);
    if (found != variables.end()) return found->second;
    if (value == "true" || value == "false") return "bool";
    if (!value.empty() && value.front() == '"') return "std::lang::String";
    if (std::regex_match(value, std::regex("[+-]?[0-9]+"))) return "int64";
    if (std::regex_match(value, std::regex("[+-]?[0-9]+\\.[0-9]+"))) return "float64";
    return {};
}

/**
 * @brief 保留模块与行映射并提升待推导 lambda。
 * @param result 源码及原行映射。
 * @param error 失败说明。
 * @return 成功返回 true。
 */
bool run(preprocess::Result &result, Memory::String &error)
{
    captureSpecifications.clear();
    if (!normalizeGenericClosures(result, error)) return false;
    const Memory::String mask = codeMask(result.text);
    Memory::Map<Memory::String, Memory::String> variables;
    Memory::Vector<std::pair<std::size_t, Memory::String>> modules;
    const std::regex moduleDeclaration("\\bmodule[ \\t\\r\\n]+([A-Za-z_][A-Za-z0-9_:]*)[ \\t\\r\\n]*;");
    for (std::regex_iterator<Memory::String::const_iterator> it(mask.begin(), mask.end(), moduleDeclaration), end;
         it != end; ++it)
        modules.push_back({ static_cast<std::size_t>((*it).position()), (*it)[1].str() });
    const std::regex declaration(
        "\\b(?:const[ \\t]+)?([A-Za-z_][A-Za-z0-9_]*(?:::[A-Za-z_][A-Za-z0-9_]*)*"
        "(?:<[^;{}()]+>)?(?:[ \\t]+(?:mut|const)[ \\t]*&)?)"
        "[ \\t]+([A-Za-z_][A-Za-z0-9_]*)\\b");
    for (std::regex_iterator<Memory::String::const_iterator> it(mask.begin(), mask.end(), declaration), end;
         it != end; ++it)
    {
        const Memory::String type = trim((*it)[1].str());
        if (type == "return" || type == "struct" || type == "interface" || type == "enum" || type == "module" || type == "import" || type == "fn") continue;
        if (type != "let") variables[(*it)[2].str()] = type;
    }

    Memory::String output;
    Memory::String generated;
    Memory::Vector<preprocess::LineOrigin> origins;
    std::size_t cursor = 0, serial = 0;
    while (cursor < result.text.size())
    {
        const std::size_t at = mask.find("fn", cursor);
        const std::size_t keywordLength = 2u;
        if (at == Memory::String::npos)
        {
            output += result.text.substr(cursor);
            break;
        }
        if ((at > 0 && ident(result.text[at - 1u])) || (at + keywordLength < result.text.size() && ident(result.text[at + keywordLength])))
        {
            output += result.text.substr(cursor, at + keywordLength - cursor);
            cursor = at + keywordLength;
            continue;
        }

        std::size_t open = at + keywordLength;
        while (open < result.text.size() && std::isspace(static_cast<unsigned char>(result.text[open]))) ++open;
        Memory::String captureMode = "const";
        Memory::Map<Memory::String, int> captureList;
        bool listed = false;
        for (;;)
        {
            Memory::String mode;
            for (const auto &candidate : { "mut", "const", "copy", "move" })
            {
                const std::size_t length = Memory::String(candidate).size();
                if (mask.compare(open, length, candidate) == 0 && (open + length == mask.size() || !ident(mask[open + length])))
                {
                    mode = candidate;
                    open += length;
                    while (open < mask.size() && std::isspace(static_cast<unsigned char>(mask[open]))) ++open;
                    break;
                }
            }
            if (mode.empty()) break;
            if (open >= mask.size() || mask[open] != '[')
            {
                captureMode = mode;
                break;
            }
            listed = true;
            std::size_t end = 0;
            if (!balanced(mask, open, '[', ']', end))
            {
                error = "lambda 捕获列表缺少 ]";
                return false;
            }
            for (const auto &name : split(mask.substr(open + 1, end - open - 1)))
            {
                if (!std::regex_match(name, std::regex("[A-Za-z_][A-Za-z0-9_]*")))
                {
                    error = "lambda 捕获列表只接受变量名：" + name;
                    return false;
                }
                if (!captureList.emplace(name, mode == "mut" ? 1 : mode == "copy" ? 2 :
                                                               mode == "move"     ? 3 :
                                                                                    0)
                         .second)
                {
                    error = "lambda 捕获列表重复声明变量：" + name;
                    return false;
                }
            }
            open = end + 1;
            while (open < mask.size() && std::isspace(static_cast<unsigned char>(mask[open]))) ++open;
        }
        for (const auto &mode : { "mut", "const", "copy", "move" })
        {
            const std::size_t length = Memory::String(mode).size();
            if (mask.compare(open, length, mode) == 0 && (open + length == mask.size() || !ident(mask[open + length])))
            {
                error = "lambda 默认捕获方式只能声明一次：" + captureMode + " 与 " + mode;
                return false;
            }
        }
        if (open >= result.text.size() || result.text[open] != '(')
        {
            output += result.text.substr(cursor, at + keywordLength - cursor);
            cursor = at + keywordLength;
            continue;
        }
        std::size_t close = 0;
        if (!balanced(result.text, open, '(', ')', close))
        {
            error = "lambda 形参表缺少 )";
            return false;
        }
        std::size_t bodyOpen = close + 1u;
        while (bodyOpen < result.text.size() && std::isspace(static_cast<unsigned char>(result.text[bodyOpen]))) ++bodyOpen;
        Memory::String resultType = "let";
        if (result.text.compare(bodyOpen, 2u, "->") == 0)
        {
            error = "0.12.0 已移除 lambda 箭头返回类型，请迁移为 int64 fn(int64 x) { ... }；推导返回类型时省略箭头";
            return false;
        }
        if (keywordLength == 2u && (bodyOpen >= mask.size() || mask[bodyOpen] != '{'))
        {
            output += result.text.substr(cursor, at + keywordLength - cursor);
            cursor = at + keywordLength;
            continue;
        }
        if (bodyOpen >= result.text.size() || result.text[bodyOpen] != '{')
        {
            error = "lambda 需要表达式体 { ... }";
            return false;
        }
        std::size_t bodyClose = 0;
        if (!balanced(result.text, bodyOpen, '{', '}', bodyClose))
        {
            error = "lambda 表达式体缺少 }";
            return false;
        }

        Memory::Vector<Memory::String> params = split(result.text.substr(open + 1u, close - open - 1u));
        Memory::Vector<Memory::String> names, types;
        bool needsContext = false;
        for (const Memory::String &param : params)
        {
            const std::size_t space = param.find_last_of(" \t");
            if (space == Memory::String::npos)
            {
                names.push_back(param);
                types.push_back({});
                needsContext = true;
            }
            else
            {
                names.push_back(trim(param.substr(space + 1u)));
                types.push_back(trim(param.substr(0u, space)));
            }
        }

        Memory::String assigned;
        const std::size_t statement = result.text.find_last_of(";{}", at == 0 ? 0 : at - 1u);
        const Memory::String prefix = result.text.substr(statement == Memory::String::npos ? 0u : statement + 1u,
                                                         at - (statement == Memory::String::npos ? 0u : statement + 1u));
        std::match_results<Memory::String::const_iterator> assignment;
        if (std::regex_search(prefix, assignment,
                              std::regex("let[ \\t]+([A-Za-z_][A-Za-z0-9_]*)[ \\t]*=[ \\t]*$")))
            assigned = assignment[1].str();
        Memory::String genericParameters;
        if (needsContext && assigned.empty())
        {
            for (std::size_t index = 0; index < types.size(); ++index)
            {
                if (!types[index].empty()) continue;
                types[index] = "McLambdaParameter" + Memory::toString(index);
                if (!genericParameters.empty()) genericParameters += ",";
                genericParameters += types[index];
            }
            genericParameters = "<" + genericParameters + ">";
        }
        else if (needsContext)
        {
            const std::regex call("\\b" + assigned + "[ \\t]*\\(");
            std::match_results<Memory::String::const_iterator> match;
            const Memory::String tail = result.text.substr(bodyClose + 1u);
            if (!std::regex_search(tail, match, call))
            {
                error = "无法推导 lambda " + assigned + " 的形参类型：至少调用一次";
                return false;
            }
            const std::size_t callOpen = bodyClose + 1u + static_cast<std::size_t>(match.position()) + static_cast<std::size_t>(match.length()) - 1u;
            std::size_t callClose = 0;
            if (!balanced(result.text, callOpen, '(', ')', callClose))
            {
                error = "lambda 调用缺少 )：" + assigned;
                return false;
            }
            const Memory::Vector<Memory::String> args = split(result.text.substr(callOpen + 1u, callClose - callOpen - 1u));
            if (args.size() != params.size())
            {
                error = "lambda " + assigned + " 的实参数量不匹配";
                return false;
            }
            for (std::size_t index = 0; index < types.size(); ++index)
            {
                if (!types[index].empty()) continue;
                types[index] = inferArgument(args[index], variables);
                if (types[index].empty())
                {
                    error = "无法从实参 `" + args[index] + "` 推导 lambda 形参 " + names[index] + " 的类型";
                    return false;
                }
            }
        }

        // 显式签名直接交给权威文法解析；只有省略类型的参数需要模板提升。
        if (genericParameters.empty() && !needsContext)
        {
            output += result.text.substr(cursor, bodyClose + 1u - cursor);
            cursor = bodyClose + 1u;
            continue;
        }
        const Memory::String function = "__chali_lambda_" + captureMode + "_" + Memory::toString(++serial);
        if (listed) captureSpecifications[function] = captureList;
        const std::size_t generatedBegin = generated.size();
        Memory::String parameterText;
        for (std::size_t index = 0; index < names.size(); ++index)
        {
            if (index != 0u) parameterText += ", ";
            parameterText += types[index] + " " + names[index];
        }
        const Memory::String body = trim(result.text.substr(bodyOpen + 1u, bodyClose - bodyOpen - 1u));
        const bool voidResult = resultType == "void";
        if (body.empty() && !voidResult)
        {
            error = "lambda 函数体不能为空";
            return false;
        }
        const bool statementBody = codeMask(body).find(';') != Memory::String::npos;
        if (statementBody && !voidResult && !hasWord(codeMask(body), "return"))
        {
            error = "lambda 多语句体必须使用 return 返回结果";
            return false;
        }

        const Memory::String loweredBody = body.empty() || statementBody ? body :
                                                                           (voidResult ? body + ";" : "return " + body + ";");
        Memory::String module = "__root";
        for (const auto &entry : modules)
        {
            if (entry.first > at) break;
            module = entry.second;
        }
        generated += "\nmodule " + module + ";\n" + resultType + " " + function + genericParameters + "(" + parameterText + ") { " + loweredBody + " }\n";
        const std::size_t line = 1u + static_cast<std::size_t>(std::count(result.text.begin(), result.text.begin() + static_cast<std::ptrdiff_t>(at), '\n'));
        preprocess::LineOrigin origin;
        if (line <= result.lineMap.size()) origin = result.lineMap[line - 1u];
        const std::size_t generatedLines = static_cast<std::size_t>(
            std::count(generated.begin() + static_cast<std::ptrdiff_t>(generatedBegin),
                       generated.end(), '\n'));
        for (std::size_t index = 0; index < generatedLines; ++index) origins.push_back(origin);
        output += result.text.substr(cursor, at - cursor) + function;
        // 提升多行函数体时保留原行数，使后续诊断仍映射到原源码。
        output.append(static_cast<std::size_t>(std::count(result.text.begin() + at,
                                                          result.text.begin() + bodyClose + 1u, '\n')),
                      '\n');
        cursor = bodyClose + 1u;
    }
    result.text = output + generated;
    result.lineMap.insert(result.lineMap.end(), origins.begin(), origins.end());
    return true;
}

}  // namespace chali_lambda
