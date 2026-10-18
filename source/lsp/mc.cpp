#include "mc.hpp"
#include "../magel_macro.hpp"

#include <algorithm>
#include <cctype>
#include <regex>
#include <sstream>

namespace lsp
{
namespace
{

/**
 * @brief 判断字符是否可作为标识符首字符。
 * @param c 原生调用上下文。
 * @retval 字符可作为标识符首字符时为 true，否则为 false。
 */
bool nameStart(char c) { return std::isalpha(static_cast<unsigned char>(c)) || c == '_'; }
/**
 * @brief 判断字符是否可作为标识符后续字符。
 * @param c 原生调用上下文。
 * @retval 字符可作为标识符后续字符时为 true，否则为 false。
 */
bool namePart(char c) { return std::isalnum(static_cast<unsigned char>(c)) || c == '_'; }

/**
 * @brief 去除文本首尾空白。
 * @param text 待处理的文本。
 * @retval 生成或读取的结果文本。
 */
std::string trim(const std::string &text)
{
    const std::size_t start = text.find_first_not_of(" \t\r");
    if (start == std::string::npos) return {};
    const std::size_t end = text.find_last_not_of(" \t\r");
    return text.substr(start, end - start + 1);
}

/**
 * @brief 将源码位置转换为语言服务器范围对象。
 * @param line 源码行号。
 * @param column 源码列号。
 * @param length 数据长度、码长表或结束位置。
 * @retval 保存将源码位置转换为语言服务器范围对象结果的对象。
 */
Range range(std::size_t line, std::size_t column, std::size_t length)
{
    return { { line, column }, { line, column + length } };
}

/**
 * @brief 屏蔽注释与字符串内容以便扫描语法结构。
 * @param line 源码行号。
 * @param blockComment 接收扫描结束时是否处于块注释中的标志。
 * @retval 生成或读取的结果文本。
 */
std::string mask(const std::string &line, bool &blockComment)
{
    std::string result = line;
    for (std::size_t i = 0; i < line.size();)
    {
        if (blockComment)
        {
            if (line.compare(i, 2, "*/") == 0)
            {
                result[i] = result[i + 1] = ' ';
                blockComment = false;
                i += 2;
            }
            else
                result[i++] = ' ';
        }
        else if (line.compare(i, 2, "/*") == 0)
        {
            result[i] = result[i + 1] = ' ';
            blockComment = true;
            i += 2;
        }
        else if (line.compare(i, 2, "//") == 0)
        {
            std::fill(result.begin() + static_cast<std::ptrdiff_t>(i), result.end(), ' ');
            break;
        }
        else if (line[i] == '"' || line[i] == '\'')
        {
            const char quote = line[i];
            result[i++] = ' ';
            while (i < line.size())
            {
                if (line[i] == '\\' && i + 1 < line.size())
                {
                    result[i++] = ' ';
                    result[i++] = ' ';
                }
                else
                {
                    const bool end = line[i] == quote;
                    result[i++] = ' ';
                    if (end) break;
                }
            }
        }
        else
            ++i;
    }
    return result;
}

/**
 * @brief 从声明文本提取参数信息。
 * @param text 待处理的文本。
 * @param params 形参或类型参数列表。
 * @param variadicParam 接收可变形参名称。
 * @retval 满足检查条件或操作完成时为 true，检查失败或无法完成时为 false。
 */
bool parseParams(const std::string &text, std::vector<std::string> &params,
                 std::string &variadicParam)
{
    if (trim(text).empty()) return true;
    std::size_t start = 0;
    while (start < text.size())
    {
        const std::size_t comma = text.find(',', start);
        const bool last = comma == std::string::npos;
        std::string param = trim(text.substr(start, comma - start));
        const bool variadic = param.compare(0, 3, "...") == 0;
        if (variadic)
        {
            if (!last) return false;
            param = trim(param.substr(3));
        }
        if (!param.empty() && param.front() == '(' && param.back() == ')')
            param = trim(param.substr(1, param.size() - 2));
        if (param.empty() || !nameStart(param[0]) || !std::all_of(param.begin() + 1, param.end(), namePart) || std::find(params.begin(), params.end(), param) != params.end()) return false;
        if (variadic)
        {
            variadicParam = param;
            return true;
        }
        params.push_back(param);
        if (last) return true;
        start = comma + 1;
    }
    return false;
}

/**
 * @brief 统计调用表达式中的实参数量。
 * @param line 源码行号。
 * @param open 左括号或起始分组的位置。
 * @retval 扫描到的实参数量。
 */
std::size_t argumentCount(const std::string &line, std::size_t open)
{
    std::size_t count = 0;
    std::size_t begin = open + 1;
    int depth = 0;
    char quote = 0;
    for (std::size_t i = begin; i < line.size(); ++i)
    {
        const char ch = line[i];
        if (quote)
        {
            if (ch == '\\' && i + 1 < line.size())
            {
                ++i;
                continue;
            }
            if (ch == quote) quote = 0;
        }
        else if (ch == '"' || ch == '\'')
            quote = ch;
        else if (ch == '(' || ch == '[' || ch == '{')
            ++depth;
        else if (ch == ')' && depth == 0)
        {
            return count + (trim(line.substr(begin, i - begin)).empty() ? 0 : 1);
        }
        else if (ch == ')' || ch == ']' || ch == '}')
            --depth;
        else if (ch == ',' && depth == 0)
        {
            ++count;
            begin = i + 1;
        }
    }
    return static_cast<std::size_t>(-1);
}

/**
 * @brief 这一行（从 first 起）是不是某个完整词面的 `$指令`：`$match_if` 不会被当成更短的 `$指令`。
 * @param code 错误码、字符值或待分析的源码文本。
 * @param first 首个节点或待检查的文本起点。
 * @param word 预处理指令的名称。
 * @retval 满足检查条件或操作完成时为 true，检查失败或无法完成时为 false。
 */
bool directiveAt(const std::string &code, std::size_t first, const std::string &word)
{
    return code.compare(first, word.size(), word) == 0 && (first + word.size() == code.size() || !namePart(code[first + word.size()]));
}

}  // namespace

/**
 * @brief 更新文档文本并重建宏与符号分析结果。
 * @param text 待处理的文本。
 */
void McDocument::setText(std::string text)
{
    text_ = std::move(text);
    lines_.clear();
    macros_.clear();
    occurrences_.clear();
    diagnostics_.clear();
    semanticTokens_.clear();
    std::size_t start = 0;
    for (;;)
    {
        const std::size_t end = text_.find('\n', start);
        lines_.push_back(text_.substr(start, end - start));
        if (end == std::string::npos) break;
        start = end + 1;
    }
    std::vector<std::string> masked;
    bool blockComment = false;
    for (const auto &line : lines_) masked.push_back(mask(line, blockComment));

    /* expr 宏和普通宏共用参数列表，允许一层形参括号标注。 */
    const std::regex header(R"(^\s*\$macro\s+(?:expr\s+)?([A-Za-z_][A-Za-z_0-9]*)\s*\(((?:[^()]|\([^()]*\))*)\)\s*$)");
    /* `$replace` 是单行宏：头这一行剩下的内容就是宏体，没有 `$end_macro`。 */
    const std::regex replaceHeader(
        R"(^\s*\$replace\s+(?:expr\s+)?([A-Za-z_][A-Za-z_0-9]*)\s*\(((?:[^()]|\([^()]*\))*)\)\s*(\S.*)$)");
    /* 省掉括号的对象宏写法 `$replace NAME 宏体`；宏体首字符不是 `(`，免得和形参表抢。 */
    const std::regex replacePlainHeader(
        R"(^\s*\$replace\s+(?:expr\s+)?([A-Za-z_][A-Za-z_0-9]*)[ \t]+([^(\s].*)$)");
    std::size_t active = static_cast<std::size_t>(-1);
    /* `$macro` 头那一行里的名字是**声明**，不是调用；扫描调用点时整行跳过。 */
    std::vector<char> declarationLine(masked.size(), 0);
    /* 登记一条宏声明（`$macro` 与 `$replace` 共用）：语义高亮 + 重复定义检查。
     * 口径和编译器一致：同名 + 同参数个数 + 同变长性才算重复。 */
    const auto recordMacro = [&](std::size_t line, const std::smatch &match,
                                 std::vector<std::string> params, std::string variadicParam,
                                 std::string groupedSignature = {}, std::string groupedShape = {}) {
        const std::string name = match[1].str();
        const std::size_t column = static_cast<std::size_t>(match.position(1));
        const Range declared = range(line, column, name.size());
        semanticTokens_.push_back({ declared, "macro" });
        Macro entry;
        entry.name = name;
        entry.params = std::move(params);
        entry.variadic = !variadicParam.empty();
        entry.variadicParam = std::move(variadicParam);
        entry.groupedSignature = std::move(groupedSignature);
        entry.groupedShape = std::move(groupedShape);
        /* 括号里空着就是对象宏：裸写宏名取用，`NAME(...)` 反而是错的。 */
        entry.objectMacro = entry.groupedSignature.empty() && !entry.variadic && entry.params.empty();
        entry.range = declared;
        const bool duplicated = std::any_of(
            macros_.begin(), macros_.end(), [&](const Macro &item) {
                return !item.deleted && item.name == name && item.params.size() == entry.params.size() && item.variadic == entry.variadic && item.groupedShape == entry.groupedShape;
            });
        if (duplicated)
            diagnostics_.push_back({ declared, Severity::Error, "magel-macro-duplicate",
                                     "重复定义宏 " + name + "（同参数个数）" });
        macros_.push_back(std::move(entry));
        declarationLine[line] = 1;
    };
    for (std::size_t line = 0; line < masked.size(); ++line)
    {
        const std::string &code = masked[line];
        const std::size_t first = code.find_first_not_of(" \t\r");
        if (first == std::string::npos || code[first] != '$') continue;
        if (code.compare(first, 6, "$macro") == 0 && (first + 6 == code.size() || !namePart(code[first + 6])))
        {
            semanticTokens_.push_back({ range(line, first, 6), "keyword" });
            if (active != static_cast<std::size_t>(-1))
            {
                diagnostics_.push_back({ range(line, first, 6), Severity::Error,
                                         "magel-nested-macro", "不支持嵌套 $macro" });
                continue;
            }
            std::smatch match;
            magel_macro::Macro parsed;
            Memory::String parsedName, problem;
            if (magel_macro::header(Memory::String(code.c_str()), parsedName, parsed, problem) && !parsed.groups.empty())
            {
                const std::regex groupedHeader(R"(^\s*\$macro\s+(?:expr\s+)?([A-Za-z_][A-Za-z_0-9]*))");
                std::regex_search(code, match, groupedHeader);
                std::vector<std::string> params;
                std::string shape;
                for (const auto &group : parsed.groups)
                {
                    shape += group.opening;
                    shape += std::to_string(group.params.size());
                    shape += group.rest.empty() ? '-' : '+';
                    for (const auto &param : group.params) params.emplace_back(param.c_str());
                    if (!group.rest.empty()) params.emplace_back(group.rest.c_str());
                }
                const auto signature = trim(code.substr(static_cast<std::size_t>(match.position(1))));
                recordMacro(line, match, std::move(params), {}, signature, shape);
                active = macros_.size() - 1;
                continue;
            }
            if (!std::regex_match(code, match, header))
            {
                diagnostics_.push_back({ range(line, first, 6), Severity::Error,
                                         "magel-macro-header",
                                         "格式应为 $macro NAME()（对象宏）或 $macro NAME(x, y)" });
                continue;
            }
            std::vector<std::string> params;
            std::string variadicParam;
            if (!parseParams(match[2].str(), params, variadicParam))
            {
                diagnostics_.push_back({ range(line, first, 6), Severity::Error,
                                         "magel-macro-params",
                                         "宏参数必须是不重复的标识符（形参可写 `(a)` 自动加括号；"
                                         "变长尾参写 `...rest` 放最后）" });
                continue;
            }
            recordMacro(line, match, std::move(params), std::move(variadicParam));
            active = macros_.size() - 1;
        }
        else if (code.compare(first, 8, "$replace") == 0 && (first + 8 == code.size() || !namePart(code[first + 8])))
        {
            semanticTokens_.push_back({ range(line, first, 8), "keyword" });
            if (active != static_cast<std::size_t>(-1))
            {
                diagnostics_.push_back({ range(line, first, 8), Severity::Error,
                                         "magel-nested-macro", "不支持嵌套 $macro / $replace" });
                continue;
            }
            std::smatch match;
            std::vector<std::string> params;
            std::string variadicParam;
            if (std::regex_match(code, match, replaceHeader))
            {
                if (!parseParams(match[2].str(), params, variadicParam))
                {
                    diagnostics_.push_back({ range(line, first, 8), Severity::Error,
                                             "magel-macro-params",
                                             "宏参数必须是不重复的标识符（形参可写 `(a)` 自动加括号；"
                                             "变长尾参写 `...rest` 放最后）" });
                    continue;
                }
            }
            else if (!std::regex_match(code, match, replacePlainHeader))
            {
                diagnostics_.push_back({ range(line, first, 8), Severity::Error,
                                         "magel-replace-header",
                                         "格式应为 $replace NAME(x, y) 宏体 或 $replace NAME 宏体"
                                         "（单行宏，宏体写在同一行）" });
                continue;
            }
            recordMacro(line, match, std::move(params), std::move(variadicParam));
        }
        else if (code.compare(first, 10, "$end_macro") == 0 && (first + 10 == code.size() || !namePart(code[first + 10])))
        {
            semanticTokens_.push_back({ range(line, first, 10), "keyword" });
            if (active == static_cast<std::size_t>(-1))
                diagnostics_.push_back({ range(line, first, 10), Severity::Error,
                                         "magel-macro-end", "多余的 $end_macro" });
            else
                active = static_cast<std::size_t>(-1);
        }
        else if (code.compare(first, 8, "$end_for") == 0 && (first + 8 == code.size() || !namePart(code[first + 8])))
        {
            semanticTokens_.push_back({ range(line, first, 8), "keyword" });
            if (active == static_cast<std::size_t>(-1))
                diagnostics_.push_back({ range(line, first, 8), Severity::Error,
                                         "magel-end-for", "多余的 $end_for（它只在 `$for` 后面配对）" });
        }
        else if (code.compare(first, 13, "$delete_macro") == 0 && (first + 13 == code.size() || !namePart(code[first + 13])))
        {
            /* 撤销整组同名宏（和编译器一样：只在宏体外生效）。撤销之后可以重新定义，
             * 所以不再算重复定义 —— 真实样例 `macros.mgl` 就是"删掉 REVISION 再定义"。 */
            semanticTokens_.push_back({ range(line, first, 13), "keyword" });
            if (active != static_cast<std::size_t>(-1)) continue;
            std::size_t i = first + 13;
            while (i < code.size() && (code[i] == ' ' || code[i] == '\t')) ++i;
            if (i == code.size() || !nameStart(code[i]))
            {
                diagnostics_.push_back({ range(line, first, 13), Severity::Error, "magel-delete-macro",
                                         "`$delete_macro` 后面要写宏名" });
                continue;
            }
            const std::size_t begin = i++;
            while (i < code.size() && namePart(code[i])) ++i;
            const std::string erased = code.substr(begin, i - begin);
            for (Macro &item : macros_)
                if (item.name == erased) item.deleted = true;
        }
        else if (code.compare(first, 4, "$end") == 0 && (first + 4 == code.size() || !namePart(code[first + 4])))
        {
            /* 旧拼写：`$end` 已拆成 `$end_macro` / `$end_for`。 */
            semanticTokens_.push_back({ range(line, first, 4), "keyword" });
            diagnostics_.push_back({ range(line, first, 4), Severity::Error, "magel-macro-end-legacy",
                                     "`$end` 已拆开：宏体用 `$end_macro`，`$for` 用 `$end_for`" });
            active = static_cast<std::size_t>(-1);
        }
    }
    if (active != static_cast<std::size_t>(-1))
        diagnostics_.push_back({ macros_[active].range, Severity::Error,
                                 "magel-macro-unclosed", "缺少 $end_macro" });

    for (std::size_t line = 0; line < masked.size(); ++line)
    {
        const std::string &code = masked[line];
        if (declarationLine[line] != 0) continue;
        for (std::size_t i = 0; i < code.size();)
        {
            if (!nameStart(code[i]))
            {
                ++i;
                continue;
            }
            const std::size_t begin = i++;
            while (i < code.size() && namePart(code[i])) ++i;
            const std::string name = code.substr(begin, i - begin);
            if (begin > 0 && code[begin - 1] == '$') continue; /* `$xxx` 是指令，不是宏调用 */
            std::vector<std::size_t> candidates;
            for (std::size_t index = 0; index < macros_.size(); ++index)
                if (!macros_[index].deleted && macros_[index].name == name && macros_[index].groupedSignature.empty())
                    candidates.push_back(index);
            if (candidates.empty()) continue;
            std::size_t open = i;
            while (open < code.size() && (code[open] == ' ' || code[open] == '\t')) ++open;
            const bool called = open < code.size() && code[open] == '(';
            const Range used = range(line, begin, name.size());

            /* 挑形态：裸用只认对象宏；带括号先按实参数目找固定形态，再退到变长形态 ——
             * 和编译器 `expand()` 里的顺序一致，同名重载因此不会被误判成参数个数不对。 */
            std::size_t picked = static_cast<std::size_t>(-1);
            if (!called)
            {
                for (const std::size_t index : candidates)
                    if (macros_[index].objectMacro)
                    {
                        picked = index;
                        break;
                    }
                if (picked == static_cast<std::size_t>(-1)) continue; /* 函数式宏没写 `(`：原样留着 */
            }
            else
            {
                const std::size_t count = argumentCount(lines_[line], open);
                for (const std::size_t index : candidates)
                    if (!macros_[index].variadic && macros_[index].params.size() == count)
                    {
                        picked = index;
                        break;
                    }
                if (picked == static_cast<std::size_t>(-1))
                    for (const std::size_t index : candidates)
                        if (macros_[index].variadic && count >= macros_[index].params.size())
                        {
                            picked = index;
                            break;
                        }
                if (picked == static_cast<std::size_t>(-1))
                {
                    std::string shapes;
                    for (const std::size_t index : candidates)
                    {
                        if (!shapes.empty()) shapes += " / ";
                        shapes += name + "(";
                        for (std::size_t n = 0; n < macros_[index].params.size(); ++n)
                        {
                            if (n != 0u) shapes += ", ";
                            shapes += macros_[index].params[n];
                        }
                        if (macros_[index].variadic)
                            shapes += macros_[index].params.empty() ? "..." : ", ...";
                        shapes += ")";
                    }
                    semanticTokens_.push_back({ used, "macro" });
                    diagnostics_.push_back(
                        { used, Severity::Error, "magel-macro-arity",
                          count == static_cast<std::size_t>(-1) ? ("宏 " + name + " 的调用缺少 )（候选：" + shapes + "）") : ("宏 " + name + " 没有匹配的形态：给了 " + std::to_string(count) + " 个实参（候选：" + shapes + "）") });
                    continue;
                }
            }
            occurrences_.push_back({ picked, used });
            semanticTokens_.push_back({ used, "macro" });
            /* 对象宏裸用；写成 `NAME(…)` 就是把对象宏当函数调。 */
            if (called && macros_[picked].objectMacro)
                diagnostics_.push_back({ used, Severity::Error, "magel-macro-object-call",
                                         "对象宏 " + name + " 不带括号：写成 " + name });
        }
    }
    std::sort(semanticTokens_.begin(), semanticTokens_.end(),
              [](const SemanticToken &a, const SemanticToken &b) { return a.range.start < b.range.start; });
}

/**
 * @brief 取得当前文档的符号列表。
 * @retval 按处理顺序保存的结果元素集合。
 */
std::vector<SymbolInfo> McDocument::symbols() const
{
    std::vector<SymbolInfo> result;
    for (const auto &macro : macros_)
    {
        if (macro.deleted) continue; /* `$delete_macro` 撤掉的宏不进大纲 */
        result.push_back({ macro.name, "directive", "$macro", macro.range });
    }
    return result;
}

/**
 * @brief 查询指定位置的悬停说明。
 * @param position 目标位置或源码位置。
 * @retval 保存查询指定位置的悬停说明结果的对象。
 */
HoverResult McDocument::hover(Position position) const
{
    for (const auto &macro : macros_)
        if (!macro.deleted && contains(macro.range, position))
            return { true, macro.range, "`$macro " + (macro.groupedSignature.empty() ? macro.name + "(...)" : macro.groupedSignature) + "`" };
    for (const auto &occurrence : occurrences_)
        if (contains(occurrence.range, position))
        {
            const auto &macro = macros_[occurrence.macro];
            std::string signature = macro.name + "(";
            for (std::size_t i = 0; i < macro.params.size(); ++i)
            {
                if (i) signature += ", ";
                signature += macro.params[i];
            }
            if (macro.variadic)
            {
                if (!macro.params.empty()) signature += ", ";
                signature += "..." + macro.variadicParam;
            }
            return { true, occurrence.range, "宏 `" + signature + ")`" };
        }
    return {};
}

/**
 * @brief 查询指定位置的补全候选项。
 * @retval 按处理顺序保存的结果元素集合。
 */
std::vector<CompletionItem> McDocument::completion(Position) const
{
    std::vector<CompletionItem> result = {
        { "$macro", "keyword", "开始参数宏", "$macro NAME()\n$end_macro" },
        { "$end_macro", "keyword", "结束参数宏", "$end_macro" },
        { "$for", "keyword", "按变长尾参循环展开", "$for value in rest\n$end_for" },
        { "$end_for", "keyword", "结束 $for 循环", "$end_for" },
        { "$line", "keyword", "当前源码行号（可在 $macro 之外用）", "$line" },
        { "$file", "keyword", "当前源码文件名（可在 $macro 之外用）", "$file" },
        { "$delete_macro", "keyword", "撤销宏定义", "$delete_macro NAME" },
    };
    for (const auto &macro : macros_)
    {
        if (macro.deleted) continue; /* `$delete_macro` 撤掉的宏不再提示 */
        if (!macro.groupedSignature.empty())
        {
            result.push_back({ macro.name, "function", "MAGEL 多括号文本宏", "$" + macro.groupedSignature });
            continue;
        }
        if (macro.objectMacro)
        {
            result.push_back({ macro.name, "constant", "MAGEL 对象宏（裸写，不带括号）", macro.name });
            continue;
        }
        std::string call = macro.name + "(";
        for (std::size_t i = 0; i < macro.params.size(); ++i)
        {
            if (i) call += ", ";
            call += macro.params[i];
        }
        if (macro.variadic)
        {
            if (!macro.params.empty()) call += ", ";
            call += macro.variadicParam;
        }
        result.push_back({ macro.name, "function", "MAGEL 参数宏", call + ")" });
    }
    return result;
}

/**
 * @brief 查询指定位置引用的定义位置。
 * @param position 目标位置或源码位置。
 * @retval 按处理顺序保存的结果元素集合。
 */
std::vector<Range> McDocument::definition(Position position) const
{
    for (const auto &occurrence : occurrences_)
        if (contains(occurrence.range, position)) return { macros_[occurrence.macro].range };
    return {};
}

/**
 * @brief 生成当前文档的检查报告。
 * @param path 目标文件或目录的路径。
 * @retval 生成或读取的结果文本。
 */
std::string McDocument::checkReport(const std::string &path) const
{
    std::ostringstream out;
    for (const auto &diagnostic : diagnostics_)
    {
        const char *level = diagnostic.severity == Severity::Error ? "error" : diagnostic.severity == Severity::Warning ? "warning" :
                                                                                                                          "info";
        out << path << ":" << (diagnostic.range.start.line + 1) << ":"
            << (diagnostic.range.start.column + 1) << ": "
            << level << ": "
            << diagnostic.message << " [" << diagnostic.code << "]\n";
    }
    return out.str();
}

}  // namespace lsp
