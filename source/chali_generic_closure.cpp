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
Memory::Map<Memory::String, GenericClosure> genericClosures;
Memory::Map<Memory::String, Memory::String> genericClosureBodies;
Memory::Set<Memory::String> genericClosureNames;
/**
 * @brief 跳过源码掩码中的空白，字符串和注释由调用方预先遮蔽。
 * @param mask 与源码等长的代码掩码。
 * @param at 读写当前扫描位置。
 */
void closureSkipSpace(const Memory::String &mask, std::size_t &at)
{
    while (at < mask.size() && std::isspace(static_cast<unsigned char>(mask[at]))) ++at;
}

/**
 * @brief 将带类型参数的闭包提升为模板实现，原位置保留唯一的环境构造入口。
 * @param result 待归一化源码及原始行位置。
 * @param error 接收无效声明和重复特化的说明。
 * @return 完成所有声明替换时返回 true。
 * @note 函数签名、函数体和块语法仍由生成解析器检查。
 */
bool normalizeGenericClosures(preprocess::Result &result, Memory::String &error)
{
    genericClosures.clear();
    genericClosureBodies.clear();
    genericClosureNames.clear();
    const auto mask = codeMask(result.text);
    if (!std::regex_search(mask, std::regex("\\bfn[ \\t\\r\\n]*<"))) return true;
    struct Edit
    {
        std::size_t begin, end;
        Memory::String value;
    };
    Memory::Vector<Edit> edits;
    Memory::Vector<Memory::Map<Memory::String, Memory::String>> scopes(1);
    Memory::Vector<bool> structureScopes(1, false);
    Memory::Vector<Memory::String> templateScopes(1);
    Memory::String module, generated;
    Memory::Vector<preprocess::LineOrigin> generatedOrigins;
    std::size_t serial = 0;
    for (std::size_t at = 0; at < mask.size(); ++at)
    {
        if (mask[at] == '{')
        {
            scopes.emplace_back();
            const auto begin = mask.find_last_of(";{}", at ? at - 1 : 0);
            const auto prefix = trim(mask.substr(begin == Memory::String::npos ? 0 : begin + 1, at - (begin == Memory::String::npos ? 0 : begin + 1)));
            structureScopes.push_back(std::regex_search(prefix, std::regex("^(struct|enum)\\b")));
            auto parameters = templateScopes.back();
            for (std::size_t angle = 0; angle < prefix.size(); ++angle)
            {
                if (prefix[angle] != '<') continue;
                std::size_t end = 0;
                if (!balanced(prefix, angle, '<', '>', end)) break;
                auto after = end + 1;
                closureSkipSpace(prefix, after);
                if (structureScopes.back() || (after < prefix.size() && prefix[after] == '('))
                {
                    if (!parameters.empty()) parameters += ",";
                    parameters += prefix.substr(angle + 1, end - angle - 1);
                    break;
                }
                angle = end;
            }
            templateScopes.push_back(parameters);
        }
        if (mask[at] == '}' && scopes.size() > 1)
        {
            scopes.pop_back();
            structureScopes.pop_back();
            templateScopes.pop_back();
        }
        if (mask.compare(at, 6, "module") == 0 && (at == 0 || !ident(mask[at - 1])) && (at + 6 == mask.size() || !ident(mask[at + 6])))
        {
            const auto end = mask.find(';', at + 6);
            if (end != Memory::String::npos)
            {
                module = trim(mask.substr(at + 6, end - at - 6));
                if (module == "__root") module.clear();
            }
        }
        if (mask.compare(at, 2, "fn") != 0 || (at && ident(mask[at - 1])) || (at + 2 < mask.size() && ident(mask[at + 2]))) continue;
        auto open = at + 2;
        closureSkipSpace(mask, open);
        const bool primary = open < mask.size() && mask[open] == '<';
        std::size_t genericEnd = open;
        if (primary && !balanced(mask, open, '<', '>', genericEnd))
        {
            error = "泛型闭包参数缺少 >";
            return false;
        }
        auto equal = at;
        while (equal && std::isspace(static_cast<unsigned char>(mask[equal - 1]))) --equal;
        if (!equal || mask[equal - 1] != '=') continue;
        --equal;
        auto nameEnd = equal;
        while (nameEnd && std::isspace(static_cast<unsigned char>(mask[nameEnd - 1]))) --nameEnd;
        Memory::Vector<Memory::String> patterns;
        auto nameAt = nameEnd;
        if (!primary)
        {
            if (!nameEnd || mask[nameEnd - 1] != '>') continue;
            int depth = 1;
            auto angle = nameEnd - 1;
            while (angle && depth)
            {
                --angle;
                if (mask[angle] == '>') ++depth;
                if (mask[angle] == '<') --depth;
            }
            if (depth) continue;
            patterns = split(mask.substr(angle + 1, nameEnd - angle - 2));
            nameEnd = angle;
            while (nameEnd && std::isspace(static_cast<unsigned char>(mask[nameEnd - 1]))) --nameEnd;
        }
        nameAt = nameEnd;
        while (nameAt && ident(mask[nameAt - 1])) --nameAt;
        const auto name = mask.substr(nameAt, nameEnd - nameAt);
        if (name.empty()) continue;
        Memory::String seed;
        if (primary)
        {
            auto prefixEnd = nameAt;
            while (prefixEnd && std::isspace(static_cast<unsigned char>(mask[prefixEnd - 1]))) --prefixEnd;
            auto prefix = prefixEnd;
            while (prefix && ident(mask[prefix - 1])) --prefix;
            if (mask.substr(prefix, prefixEnd - prefix) != "let")
            {
                error = "泛型闭包使用 let 名称 = fn<T>(...) 声明";
                return false;
            }
            seed = "__chali_lambda_generic_" + Memory::toString(++serial);
            GenericClosure definition;
            definition.seed = seed;
            definition.sourceSeed = seed;
            definition.primary = seed + "_body";
            definition.module = module;
            definition.member = structureScopes.back();
            definition.outerParameters = templateScopes.back();
            for (const auto &parameter : split(definition.outerParameters))
                definition.outerNames.push_back(parameter.substr(0, parameter.find_first_of(":= \t\r\n")));
            definition.parameters = "<" + definition.outerParameters + (definition.outerParameters.empty() ? "" : ",")
                                    + mask.substr(open + 1, genericEnd - open - 1) + ">";
            for (auto parameter : split(mask.substr(open + 1, genericEnd - open - 1)))
            {
                const auto stop = parameter.find_first_of(":= \t\r\n");
                const auto parameterName = parameter.substr(0, stop);
                if (parameterName.empty() || !(std::isalpha(static_cast<unsigned char>(parameterName[0])) || parameterName[0] == '_'))
                {
                    error = "泛型闭包需要具名类型参数";
                    return false;
                }
                definition.names.push_back(parameterName);
            }
            if (definition.names.empty())
            {
                error = "泛型闭包的类型参数表不能为空";
                return false;
            }
            genericClosures.emplace(seed, std::move(definition));
            scopes.back()[name] = seed;
            genericClosureNames.insert(name);
            open = genericEnd + 1;
        }
        else
        {
            for (auto scope = scopes.rbegin(); scope != scopes.rend(); ++scope)
            {
                const auto found = scope->find(name);
                if (found != scope->end())
                {
                    seed = found->second;
                    break;
                }
            }
            if (seed.empty()) continue;
        }
        auto &definition = genericClosures.at(seed);
        if (!primary && patterns.size() != definition.names.size())
        {
            error = "闭包特化的类型参数数量不匹配：" + name;
            return false;
        }
        closureSkipSpace(mask, open);
        const auto capturesBegin = open;
        while (open < mask.size() && mask[open] != '(' && mask[open] != '{' && mask[open] != ';') ++open;
        std::size_t close = 0, bodyClose = 0;
        if (open == mask.size() || mask[open] != '(' || !balanced(mask, open, '(', ')', close))
        {
            error = "泛型闭包需要完整的参数表";
            return false;
        }
        auto body = close + 1;
        closureSkipSpace(mask, body);
        if (body == mask.size() || mask[body] != '{' || !balanced(mask, body, '{', '}', bodyClose))
        {
            error = "泛型闭包需要完整的函数体";
            return false;
        }
        const auto captureText = trim(result.text.substr(capturesBegin, open - capturesBegin));
        if (!primary && !captureText.empty())
        {
            error = "闭包特化沿用主定义的捕获方式，不能重新声明捕获";
            return false;
        }
        if (primary)
        {
            std::size_t captureAt = capturesBegin;
            bool defaultSeen = false;
            while (captureAt < open)
            {
                closureSkipSpace(mask, captureAt);
                if (captureAt == open) break;
                const auto begin = captureAt;
                while (captureAt < open && ident(mask[captureAt])) ++captureAt;
                const auto mode = mask.substr(begin, captureAt - begin);
                const int value = mode == "const" ? 0 : mode == "mut" ? 1 :
                                                    mode == "copy"    ? 2 :
                                                    mode == "move"    ? 3 :
                                                                        -1;
                if (value < 0 || defaultSeen)
                {
                    error = "无效或重复的泛型闭包捕获方式：" + mode;
                    return false;
                }
                closureSkipSpace(mask, captureAt);
                if (captureAt < open && mask[captureAt] == '[')
                {
                    std::size_t end = 0;
                    if (!balanced(mask, captureAt, '[', ']', end)) return false;
                    for (const auto &capture : split(mask.substr(captureAt + 1, end - captureAt - 1)))
                        if (capture.empty() || !definition.captures.emplace(capture, value).second)
                        {
                            error = "泛型闭包捕获名称无效或重复：" + capture;
                            return false;
                        }
                    captureAt = end + 1;
                }
                else
                {
                    definition.captureMode = value;
                    defaultSeen = true;
                }
            }
        }
        const auto function = primary ? definition.primary : seed + "_specialization_" + Memory::toString(definition.branches.size());
        const auto matcher = function + "_match";
        if (!primary)
        {
            for (const auto &branch : definition.branches)
                if (branch.patterns == patterns)
                {
                    error = "重复的泛型闭包特化：" + name;
                    return false;
                }
            definition.branches.push_back({ function, matcher, patterns });
        }
        genericClosureBodies[function] = seed;
        auto bodyText = trim(result.text.substr(body + 1, bodyClose - body - 1));
        const auto bodyMask = codeMask(bodyText);
        const bool expressionBody = !trim(bodyMask).empty() && bodyMask.find(';') == Memory::String::npos;
        const bool returnsValue = expressionBody || std::regex_search(bodyMask, std::regex("\\breturn[ \\t\\r\\n]+[^; \\t\\r\\n]"));
        if (expressionBody) bodyText = "return " + bodyText + "\n;";
        const auto resultType = returnsValue ? "let " : "void ";
        const auto generatedBegin = generated.size();
        generated += "\nmodule " + (module.empty() ? Memory::String("__root") : module) + ";\n" + resultType + function + definition.parameters
                     + result.text.substr(open, close - open + 1) + " {\n" + bodyText + "\n}\n";
        if (!primary)
        {
            generated += "void " + matcher + definition.parameters + "(";
            for (std::size_t index = 0; index < patterns.size(); ++index)
            {
                if (index) generated += ",";
                generated += patterns[index] + " __pattern" + Memory::toString(index);
            }
            generated += ") {}\n";
        }
        if (primary)
        {
            auto environment = seed;
            if (!definition.outerNames.empty())
            {
                environment += "<";
                for (std::size_t index = 0; index < definition.outerNames.size(); ++index)
                {
                    if (index) environment += ",";
                    environment += definition.outerNames[index];
                }
                environment += ">";
            }
            edits.push_back({ at, bodyClose + 1, environment });
            generated += "void " + seed + (definition.outerParameters.empty() ? Memory::String() : "<" + definition.outerParameters + ">") + "() {}\n";
        }
        else
        {
            auto end = bodyClose + 1;
            closureSkipSpace(mask, end);
            if (end == mask.size() || mask[end] != ';')
            {
                error = "闭包特化声明缺少 ;";
                return false;
            }
            edits.push_back({ nameAt, end + 1, "" });
        }
        const auto line = static_cast<std::size_t>(std::count(mask.begin(), mask.begin() + at, '\n'));
        const auto origin = line < result.lineMap.size() ? result.lineMap[line] : preprocess::LineOrigin{};
        const auto count = static_cast<std::size_t>(std::count(generated.begin() + generatedBegin, generated.end(), '\n'));
        generatedOrigins.insert(generatedOrigins.end(), count, origin);
        at = bodyClose;
    }
    for (auto edit = edits.rbegin(); edit != edits.rend(); ++edit)
    {
        edit->value.append(static_cast<std::size_t>(std::count(result.text.begin() + edit->begin, result.text.begin() + edit->end, '\n')), '\n');
        result.text.replace(edit->begin, edit->end - edit->begin, edit->value);
    }
    result.text += generated;
    result.lineMap.insert(result.lineMap.end(), generatedOrigins.begin(), generatedOrigins.end());
    return true;
}

/**
 * @brief 为尚未被普通模板替换的显式闭包调用标记类型参数边界。
 * @param source 已完成普通泛型单态化的源码。
 * @note 只处理紧随调用参数表的角括号，不改变普通比较表达式。
 */
void markClosureTypeArguments(Memory::String &source)
{
    const auto mask = codeMask(source);
    Memory::Set<std::size_t> edits;
    for (std::size_t at = 0; at < mask.size(); ++at)
    {
        if (mask[at] != '<' || !at || mask[at - 1] == '@') continue;
        auto name = at;
        while (name && std::isspace(static_cast<unsigned char>(mask[name - 1]))) --name;
        if (!name || !ident(mask[name - 1])) continue;
        const auto nameEnd = name;
        while (name && ident(mask[name - 1])) --name;
        if (name == nameEnd || !(std::isalpha(static_cast<unsigned char>(mask[name])) || mask[name] == '_')) continue;
        const auto spelling = mask.substr(name, nameEnd - name);
        if (spelling == "cast" || spelling == "fn" || (name && mask[name - 1] == '$')) continue;
        auto previous = name;
        while (previous && std::isspace(static_cast<unsigned char>(mask[previous - 1]))) --previous;
        auto keyword = previous;
        while (keyword && ident(mask[keyword - 1])) --keyword;
        if (keyword && mask[keyword - 1] == '$' && (mask.substr(keyword, previous - keyword) == "macro" || mask.substr(keyword, previous - keyword) == "replace")) continue;
        std::size_t end = 0;
        if (!balanced(mask, at, '<', '>', end)) continue;
        auto next = end + 1;
        closureSkipSpace(mask, next);
        if (next == mask.size() || mask[next] != '(') continue;
        const auto content = mask.substr(at + 1, end - at - 1);
        if (content.find_first_of(";{}=+!|/") != Memory::String::npos) continue;
        edits.insert(at);
        edits.insert(end);
        at = end;
    }
    for (auto at = edits.rbegin(); at != edits.rend(); ++at) source.insert(*at, "@!");
}

}  // namespace chali_lambda
