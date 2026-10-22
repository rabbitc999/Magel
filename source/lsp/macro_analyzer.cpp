#include "macro_analyzer.hpp"
#include "../macro/signature.hpp"

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
        entry.objectMacro = entry.groupedShape.empty();
        entry.range = declared;
        const bool duplicated = std::any_of(
            macros_.begin(), macros_.end(), [&](const Macro &item) {
                return item.name == name && item.params.size() == entry.params.size() && item.variadic == entry.variadic && item.groupedShape == entry.groupedShape;
            });
        if (duplicated)
            diagnostics_.push_back({ declared, Severity::Error, "chali-macro-duplicate",
                                     "重复定义宏 " + name + "（同参数个数）" });
        macros_.push_back(std::move(entry));
        declarationLine[line] = 1;
    };
    for (std::size_t line = 0; line < masked.size(); ++line)
    {
        const std::string &code = masked[line];
        const std::size_t first = code.find_first_not_of(" \t\r");
        if (first == std::string::npos || code[first] != '$') continue;
        const bool macroHeader = directiveAt(code, first, "$macro");
        const bool replacementHeader = directiveAt(code, first, "$replace");
        if (macroHeader || replacementHeader)
        {
            const std::size_t length = replacementHeader ? 8 : 6;
            semanticTokens_.push_back({ range(line, first, length), "keyword" });
            if (active != static_cast<std::size_t>(-1))
            {
                diagnostics_.push_back({ range(line, first, length), Severity::Error, "chali-nested-macro", "不支持嵌套宏定义" });
                continue;
            }
            chali_macro::Signature parsed;
            Memory::String error;
            const auto &input = replacementHeader ? lines_[line] : code;
            if (!chali_macro::readSignature(Memory::String(input.c_str()), parsed, error))
            {
                diagnostics_.push_back({ range(line, first, length), Severity::Error, "chali-macro-header", error.c_str() });
                continue;
            }
            std::smatch match;
            const std::regex declaration(R"(^\s*\$(?:macro|replace)\s+([A-Za-z_][A-Za-z_0-9]*))");
            std::regex_search(code, match, declaration);
            std::vector<std::string> params;
            std::string signature = parsed.name.c_str(), shape;
            if (!parsed.generics.empty())
            {
                signature += '<';
                for (size_t item = 0; item < parsed.generics.size(); ++item)
                {
                    if (item) signature += ", ";
                    signature += chali_syntax::trim(parsed.generics[item]).c_str();
                }
                signature += '>';
            }
            for (const auto &group : parsed.groups)
            {
                shape += group.opening;
                shape += std::to_string(group.params.size());
                shape += group.rest.empty() ? '-' : '+';
                signature += group.opening;
                bool comma = false;
                for (const auto &parameter : group.params)
                {
                    if (comma) signature += ", ";
                    comma = true;
                    signature += parameter.c_str();
                    params.emplace_back(parameter.c_str());
                }
                if (!group.rest.empty())
                {
                    if (comma) signature += ", ";
                    signature += "..." + std::string(group.rest.c_str());
                    params.emplace_back(group.rest.c_str());
                }
                signature += chali_syntax::groupClose(group.opening);
            }
            recordMacro(line, match, std::move(params), {}, signature, shape);
            if (!replacementHeader) active = macros_.size() - 1;
        }
        else if (code.compare(first, 10, "$end_macro") == 0 && (first + 10 == code.size() || !namePart(code[first + 10])))
        {
            semanticTokens_.push_back({ range(line, first, 10), "keyword" });
            if (active == static_cast<std::size_t>(-1))
                diagnostics_.push_back({ range(line, first, 10), Severity::Error,
                                         "chali-macro-end", "多余的 $end_macro" });
            else
                active = static_cast<std::size_t>(-1);
        }
        else if (code.compare(first, 8, "$end_for") == 0 && (first + 8 == code.size() || !namePart(code[first + 8])))
        {
            semanticTokens_.push_back({ range(line, first, 8), "keyword" });
            if (active == static_cast<std::size_t>(-1))
                diagnostics_.push_back({ range(line, first, 8), Severity::Error,
                                         "chali-end-for", "多余的 $end_for（它只在 `$for` 后面配对）" });
        }
    }
    if (active != static_cast<std::size_t>(-1))
        diagnostics_.push_back({ macros_[active].range, Severity::Error,
                                 "chali-macro-unclosed", "缺少 $end_macro" });

    std::string fullMask;
    for (const auto &line : masked) fullMask += line + "\n";
    const Memory::String raw(text_.c_str());
    std::size_t lineStart = 0;
    for (std::size_t line = 0; line < masked.size(); ++line)
    {
        const auto &code = masked[line];
        if (!declarationLine[line])
            for (std::size_t at = 0; at < code.size(); ++at)
            {
                if (code[at] != '$' || at + 1 == code.size() || !nameStart(code[at + 1])) continue;
                const auto begin = at++;
                while (at < code.size() && namePart(code[at])) ++at;
                const auto name = code.substr(begin + 1, at - begin - 1);
                std::vector<std::size_t> candidates;
                for (std::size_t index = 0; index < macros_.size(); ++index)
                    if (macros_[index].name == name) candidates.push_back(index);
                if (candidates.empty())
                {
                    --at;
                    continue;
                }
                auto next = lineStart + at;
                const auto skipSpace = [&] { while (next < fullMask.size() && std::isspace(static_cast<unsigned char>(fullMask[next]))) ++next; };
                skipSpace();
                Memory::Vector<Memory::String> arguments;
                if (next < fullMask.size() && fullMask[next] == '<')
                {
                    std::size_t end = next;
                    if (chali_syntax::groupArguments(raw, next, arguments, end, true)) next = end;
                    skipSpace();
                }
                std::vector<std::pair<char, std::size_t>> groups;
                bool partial = false;
                while (next < raw.size() && (fullMask[next] == '(' || fullMask[next] == '[' || fullMask[next] == '{'))
                {
                    arguments.clear();
                    std::size_t end = next;
                    bool closed = false;
                    if (!chali_syntax::groupArguments(raw, next, arguments, end, true, &closed)) break;
                    groups.push_back({ fullMask[next], arguments.size() });
                    next = end;
                    if (!closed)
                    {
                        partial = true;
                        break;
                    }
                    skipSpace();
                }
                std::size_t picked = static_cast<std::size_t>(-1);
                for (const auto candidate : candidates)
                {
                    const auto &shape = macros_[candidate].groupedShape;
                    std::size_t position = 0;
                    bool matches = true;
                    for (std::size_t item = 0; item < groups.size(); ++item)
                    {
                        if (position == shape.size() || shape[position++] != groups[item].first)
                        {
                            matches = false;
                            break;
                        }
                        std::size_t fixed = 0;
                        while (position < shape.size() && std::isdigit(static_cast<unsigned char>(shape[position]))) fixed = fixed * 10 + static_cast<std::size_t>(shape[position++] - '0');
                        if (position == shape.size())
                        {
                            matches = false;
                            break;
                        }
                        const bool variadic = shape[position++] == '+';
                        if (!(partial && item + 1 == groups.size()) && (variadic ? groups[item].second < fixed : groups[item].second != fixed)) matches = false;
                    }
                    if (!partial && position != shape.size()) matches = false;
                    if (matches && (picked == static_cast<std::size_t>(-1) || shape.find('+') == shape.npos)) picked = candidate;
                }
                const auto used = range(line, begin, at - begin);
                semanticTokens_.push_back({ used, "macro" });
                if (picked != static_cast<std::size_t>(-1))
                    occurrences_.push_back({ picked, used });
                else if (!partial)
                    diagnostics_.push_back({ used, Severity::Error, "chali-macro-arity", "宏调用的参数组或实参数量不匹配：" + name });
                --at;
            }
        lineStart += lines_[line].size() + 1;
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
        if (contains(macro.range, position))
            return { true, macro.range, "`$macro " + (macro.groupedSignature.empty() ? macro.name : macro.groupedSignature) + "`" };
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
        { "$match", "keyword", "完整语法片段匹配", "$match value is expr\n$end_match" },
        { "$regex", "keyword", "原文正则完整匹配", "$regex value is \"[0-9]+\"\n$end_regex" },
        { "$module", "keyword", "调用处模块名称", "$module" },
    };
    for (const auto &macro : macros_)
    {
        auto call = macro.groupedSignature.empty() ? macro.name : macro.groupedSignature;
        const auto generic = call.find('<');
        if (generic != call.npos)
        {
            const auto end = call.find('>', generic);
            if (end != call.npos) call.erase(generic, end - generic + 1);
        }
        result.push_back({ "$" + macro.name, "function", "卫生 AST 宏", "$" + call });
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
