/**
 * @brief 使用真实宏头与字符分组器生成按组工作的签名提示。
 * @param text 未保存源码。
 * @param cursor 光标字节偏移。
 * @param records 可见宏声明。
 * @return 包含分组计数的签名提示；非宏调用返回 null。
 */
J lspMacroSignature(const std::string &text, size_t cursor, const std::vector<Record> &records)
{
    auto tokens = lex(text);
    for (auto token = tokens.rbegin(); token != tokens.rend(); ++token)
    {
        if (token->a >= cursor || !starts(token->word, "$") || token->word.size() < 2) continue;
        for (const auto &record : records)
        {
            if (record.kind != "macro" || record.name != token->word.substr(1) || !starts(record.signature, "$macro")) continue;
            magel_macro::Macro macro;
            Memory::String name, problem;
            if (!magel_macro::header(record.signature.c_str(), name, macro, problem)) continue;
            auto groups = macro.groups;
            if (groups.empty() && macro.functionLike)
            {
                magel_macro::ParameterGroup group;
                group.params = macro.params;
                if (macro.variadic) group.rest = macro.variadicParam;
                groups.push_back(std::move(group));
            }
            if (groups.empty()) continue;
            const Memory::String source(text.c_str());
            size_t at = token->b, active = 0, parameterOffset = 0;
            bool hit = false;
            std::string description = record.documentation;
            A parameters, details;
            const auto header = lex(record.signature);
            const size_t macroName = header.size() > 1 && header[1].word == "expr" ? 2 : 1;
            const auto parameter = [&](const Memory::String &name) {
                for (size_t i = macroName + 1; i < header.size(); ++i)
                    if (header[i].word == name.c_str())
                        return object({ { "label", A{ get(position(record.signature, header[i].a), "character"), get(position(record.signature, header[i].b), "character") } } });
                return object({ { "label", std::string(name.c_str()) } });
            };
            for (const auto &group : groups)
            {
                for (const auto &p : group.params) parameters.push_back(parameter(p));
                if (!group.rest.empty()) parameters.push_back(parameter(group.rest));
            }
            for (size_t g = 0; g < groups.size(); ++g)
            {
                const auto &group = groups[g];
                magel_macro::spaces(source, at);
                if (at >= source.size()) break;
                if (source[at] != group.opening)
                {
                    if (magel_macro::groupClose(source[at]) && cursor > at)
                    {
                        Memory::Vector<Memory::String> wrong;
                        size_t wrongEnd = at;
                        if (magel_macro::groupArguments(source, at, wrong, wrongEnd, true) && cursor <= wrongEnd)
                        {
                            description = "宏Token分组不匹配：第 " + std::to_string(g + 1) + " 组期待 " + std::string(1, group.opening) + std::string(1, magel_macro::groupClose(group.opening)) + "，实际遇到 " + std::string(1, source[at]);
                            active = parameterOffset;
                            hit = true;
                        }
                    }
                    break;
                }
                Memory::Vector<Memory::String> parts;
                size_t end = at;
                bool closed = false;
                if (!magel_macro::groupArguments(source, at, parts, end, true, &closed)) break;
                std::string groupName;
                for (const auto &p : group.params)
                {
                    if (!groupName.empty()) groupName += ", ";
                    groupName += p.c_str();
                }
                if (!group.rest.empty())
                {
                    if (!groupName.empty()) groupName += ", ";
                    groupName += group.rest.c_str();
                }
                const bool empty = parts.size() == 1 && magel_macro::trim(parts.front()).empty();
                const auto count = empty ? 0 : parts.size();
                details.push_back(object({ { "index", int(g) }, { "name", groupName }, { "count", int(count) }, { "opening", std::string(1, group.opening) } }));
                description += "\n\n" + groupName + "：" + std::to_string(count) + " 项";
                if (at < cursor && (cursor < end || (!closed && cursor == end)))
                {
                    Memory::Vector<Memory::String> prefix;
                    size_t ignored = at;
                    if (!magel_macro::groupArguments(text.substr(0, cursor).c_str(), at, prefix, ignored, true)) break;
                    auto argument = prefix.empty() ? size_t(0) : prefix.size() - 1;
                    const auto fixed = group.params.size();
                    active = parameterOffset + std::min(argument, group.rest.empty() ? (fixed ? fixed - 1 : 0) : fixed);
                    description = "当前分组：" + groupName + "\n\n" + description;
                    hit = true;
                }
                parameterOffset += group.params.size() + (group.rest.empty() ? 0 : 1);
                at = end;
                if (!closed) break;
            }
            if (!hit) continue;
            return object({ { "signatures", A{ object({ { "label", record.signature }, { "documentation", object({ { "kind", "markdown" }, { "value", description } }) }, { "parameters", parameters }, { "activeParameter", int(active) } }) } },
                            { "activeSignature", 0 },
                            { "activeParameter", int(active) },
                            { "magelGroups", details } });
        }
    }
    return J();
}
