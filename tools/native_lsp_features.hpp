/**
 * @brief 编辑期绑定的稳定身份及可重构的原始源码范围。
 */
struct SymbolIdentity
{
    std::string source, name, kind;
    size_t offset = 0, end = 0;
    std::set<std::pair<std::string, size_t>> candidates;
    /**
     * @brief 判断身份是否唯一且属于用户源码。
     * @return 身份唯一时为 true。
     */
    bool valid() const { return !source.empty() && candidates.size() == 1; }
    /**
     * @brief 比较两个引用是否指向同一原始声明。
     * @param other 另一个声明身份。
     * @return 两者均唯一且来源和偏移相同时为 true。
     */
    bool same(const SymbolIdentity &other) const { return valid() && other.valid() && source == other.source && offset == other.offset; }
};

/**
 * @brief 判断位置是否处于宏模板正文，避免把纯文本当成语言绑定。
 * @param text 原始源码。
 * @param at 查询偏移。
 * @return 位于宏头或正文时为 true。
 */
bool inMacroTemplate(const std::string &text, size_t at)
{
    if (activeUnit && activeUnit->text.get() == &text)
    {
        const auto &regions = activeUnit->macroRegions;
        auto region = std::upper_bound(regions.begin(), regions.end(), std::make_pair(at, std::string::npos));
        return region != regions.begin() && at < std::prev(region)->second;
    }
    bool macro = false;
    for (const auto &token : lex(text))
    {
        if (token.a > at) break;
        if (token.word == "$macro") macro = true;
        if (token.word == "$end_macro") macro = false;
    }
    return macro;
}

/**
 * @brief 解析原始引用到唯一声明；重载仅在实参类型足以区分时选择。
 * @param u 当前文档 URI。
 * @param text 当前不可变源码。
 * @param records 该文档可见声明。
 * @param at 查询字节偏移。
 * @param indexed 可选的同名绑定索引，用于工作区引用扫描复用。
 * @return 唯一身份或带候选集的歧义结果。
 */
SymbolIdentity resolveSymbol(const std::string &u, const std::string &text, const std::vector<Record> &records, size_t at, const std::vector<Binding> *indexed = nullptr)
{
    SymbolIdentity result;
    const auto localTokens = activeUnit && activeUnit->text.get() == &text ? std::vector<Token>() : lex(text);
    const auto &tokens = activeUnit && activeUnit->text.get() == &text ? activeUnit->tokens : localTokens;
    auto token = std::upper_bound(tokens.begin(), tokens.end(), at, [](size_t value, const Token &part) { return value < part.a; });
    if (token == tokens.begin()) return result;
    --token;
    if (at >= token->b || (!identifier(token->word) && !(starts(token->word, "$") && identifier(token->word.substr(1)))) || keywords.count(token->word)) return result;
    result.name = starts(token->word, "$") ? token->word.substr(1) : token->word;
    const auto localBindings = indexed ? std::vector<Binding>() : bindings(text, types);
    const auto &vars = indexed ? *indexed : localBindings;
    const auto scope = scopeAt(text, at);
    const auto name = expression(text, token->b);
    const Binding *binding = nullptr;
    if (name.find('.') == name.npos && name.find("::") == name.npos)
        for (const auto &b : vars)
            if (b.name == token->word && (b.offset == token->a || visible(b, scope, at)) && (!binding || b.scope.size() > binding->scope.size() || (b.scope.size() == binding->scope.size() && b.offset > binding->offset))) binding = &b;
    if (binding && (binding->kind != "function" || binding->scope.size() > 1) && !inMacroTemplate(text, at))
    {
        result.source = u;
        result.kind = binding->kind;
        result.offset = binding->offset;
        result.end = binding->end;
        result.candidates.insert({ u, result.offset });
        return result;
    }
    auto entries = matching(records, name, text, at, types);
    std::vector<Record> declaration;
    for (const auto &record : records)
        if (record.source == u && record.offset == token->a) declaration.push_back(record);
    if (!declaration.empty()) entries = declaration;
    if (inMacroTemplate(text, at) && declaration.empty()) return result;
    auto next = size_t(token - tokens.begin()) + 1;
    if (next < tokens.size() && tokens[next].word == "<") next = groupEnd(tokens, next, "<", ">");
    if (entries.size() > 1 && next < tokens.size() && tokens[next].word == "(")
    {
        const auto stop = groupEnd(tokens, next, "(", ")");
        if (stop <= tokens.size() && stop > next && tokens[stop - 1].word == ")")
        {
            Memory::Vector<Memory::String> args;
            size_t ignored = 0;
            if (magel_macro::groupArguments(text.substr(tokens[next].a, tokens[stop - 1].b - tokens[next].a).c_str(), 0, args, ignored))
            {
                ReceiverTypes resolver(records, text, types);
                entries = resolver.selectedCalls(entries, args, at);
            }
        }
    }
    for (const auto &record : entries)
        if (!record.source.empty())
        {
            result.candidates.insert({ record.source, record.offset });
            result.source = record.source;
            result.offset = record.offset;
            result.end = record.offset + record.name.size();
            result.kind = record.kind;
        }
    return result;
}

/**
 * @brief 查找真实声明的原始引用并构造带版本保护的工作区重命名。
 * @param method references、prepareRename 或 rename 方法名。
 * @param params 原始请求参数。
 * @param u 当前文档 URI。
 * @param text 当前源码。
 * @param records 当前可见声明。
 * @param cursor 查询偏移。
 * @return 引用位置、可重命名范围或 WorkspaceEdit。
 */
J refactor(const std::string &method, const J &params, const std::string &u, const std::string &text, const std::vector<Record> &records, size_t cursor)
{
    const auto target = resolveSymbol(u, text, records, cursor);
    const bool renaming = method != "textDocument/references";
    if (!target.valid())
    {
        if (renaming) throw std::runtime_error("无法安全重命名：符号未知、重载有歧义或没有可编辑的原始声明");
        return A{};
    }
    if (target.name == "main" || starts(target.name, "__magel"))
    {
        if (renaming) throw std::runtime_error("入口和编译器保留符号不能重命名");
    }
    if (method == "textDocument/prepareRename")
    {
        for (const auto &token : lex(text))
            if (token.a <= cursor && cursor < token.b) return object({ { "range", range(text, token.a + (starts(token.word, "$") ? 1 : 0), token.b) }, { "placeholder", target.name } });
        return J();
    }
    const auto newName = str(params, "newName");
    if (renaming && (!identifier(newName) || keywords.count(newName) || starts(newName, "__magel"))) throw std::runtime_error("新名称必须是非保留的标识符");
    if (renaming)
    {
        const auto declarationText = source(target.source);
        const auto declarationRecords = collect(target.source, declarationText);
        (void)declarationRecords;
        const auto declared = bindings(declarationText, types);
        const Binding *original = nullptr;
        for (const auto &binding : declared)
            if (binding.offset == target.offset) original = &binding;
        if (original)
            for (const auto &binding : declared)
                if (binding.name == newName && binding.offset != original->offset && binding.scope == original->scope) throw std::runtime_error("重命名会与同一作用域稍后或已有的声明冲突：" + newName);
    }
    auto files = workspace();
    if (!files.count(target.source)) files[target.source] = source(target.source);
    A references, changes;
    std::set<std::string> dependencies;
    for (const auto &file : files)
    {
        if (requestCancelled && requestCancelled->load()) throw std::runtime_error("请求已取消");
        dependencies.insert(file.first);
        const auto unit = cache->unit(file.first, file.second);
        LspIndexScope index(*unit);
        const auto &body = *unit->text;
        auto visibleRecords = collect(file.first, body);
        const auto vars = bindings(body, types);
        std::vector<Binding> targetBindings;
        for (const auto &binding : vars)
            if (binding.name == target.name) targetBindings.push_back(binding);
        A edits;
        size_t visitedTokens = 0;
        for (const auto &token : unit->tokens)
        {
            if ((++visitedTokens & 127u) == 0 && requestCancelled && requestCancelled->load()) throw std::runtime_error("请求已取消");
            if (token.word != target.name && token.word != "$" + target.name) continue;
            if (renaming && inMacroTemplate(body, token.a) && !(file.first == target.source && token.a == target.offset)) throw std::runtime_error("宏模板含同名文本，无法确认展开后的绑定；已取消重命名");
            auto symbol = resolveSymbol(file.first, body, visibleRecords, token.a, &targetBindings);
            if (!symbol.same(target))
            {
                if (renaming && symbol.candidates.count({ target.source, target.offset })) throw std::runtime_error("存在不能唯一解析的引用，已取消整个重命名");
                continue;
            }
            const auto a = token.a + (starts(token.word, "$") ? 1 : 0);
            const auto location = object({ { "uri", file.first }, { "range", range(body, a, token.b) } });
            const bool declaration = file.first == target.source && token.a == target.offset;
            if (!declaration || get(get(params, "context"), "includeDeclaration").asBool().value_or(false)) references.push_back(location);
            if (!renaming) continue;
            const auto scope = scopeAt(body, token.a);
            for (const auto &binding : vars)
                if (binding.name == newName && visible(binding, scope, token.a)) throw std::runtime_error("重命名会与可见绑定冲突：" + newName);
            auto name = expression(body, token.b);
            const auto suffix = name.size() >= token.word.size() ? name.substr(0, name.size() - token.word.size()) : "";
            for (const auto &other : matching(visibleRecords, suffix + newName, body, token.a, types))
                if (!(other.source == target.source && other.offset == target.offset)) throw std::runtime_error("重命名会与已有声明冲突：" + newName);
            edits.push_back(object({ { "range", range(body, a, token.b) }, { "newText", newName } }));
        }
        if (!edits.empty())
        {
            auto document = documents.find(file.first);
            changes.push_back(object({ { "textDocument", object({ { "uri", file.first }, { "version", document == documents.end() ? J() : document->second->second } }) }, { "edits", edits } }));
        }
    }
    usedDocuments.insert(dependencies.begin(), dependencies.end());
    return renaming ? object({ { "documentChanges", changes } }) : J(references);
}

/**
 * @brief 生成 auto 变量和上下文匿名函数参数的类型内联提示。
 * @param text 当前源码。
 * @param records 当前可见声明。
 * @param requested 编辑器请求的可见范围。
 * @return 只包含唯一可推断类型的提示。
 */
J inlayHints(const std::string &text, const std::vector<Record> &records, const J &requested)
{
    A hints;
    ReceiverTypes resolver(records, text, types);
    const auto begin = offsetAt(text, get(requested, "start")), end = offsetAt(text, get(requested, "end"));
    for (const auto &binding : bindings(text, types))
    {
        if (binding.type != "auto" || binding.offset < begin || binding.end > end || inMacroTemplate(text, binding.offset)) continue;
        const auto type = resolver.bindingType(binding, binding.end);
        if (type.empty() || type == "auto") continue;
        hints.push_back(object({ { "position", position(text, binding.end) }, { "label", ": " + type }, { "kind", 1 }, { "paddingLeft", false }, { "tooltip", markup(lspTypeDescription(type) + "\n\n提示不修改源代码。") } }));
    }
    return hints;
}

/**
 * @brief 为编译器确认的所有权错误关联原始绑定及相关显式捕获位置。
 * @param text 当前源码。
 * @param u 当前 URI。
 * @param diagnostic 编译器产生的诊断。
 * @return 带关联源码位置的诊断，不改变原错误码或级别。
 */
J ownershipInformation(const std::string &text, const std::string &u, J diagnostic)
{
    const auto message = str(diagnostic, "message");
    const bool moved = str(diagnostic, "code") == "MGL1205";
    if (!moved && message.find("借用") == message.npos && message.find("生命周期") == message.npos) return diagnostic;
    const auto at = offsetAt(text, get(get(diagnostic, "range"), "start"));
    auto records = collect(u, text);
    const auto vars = bindings(text, types);
    A related;
    std::set<std::string> mentioned;
    for (const auto &token : lex(message))
        if (identifier(token.word)) mentioned.insert(token.word);
    for (const auto &binding : vars)
    {
        if (binding.offset >= at || !mentioned.count(binding.name)) continue;
        const auto scope = scopeAt(text, at);
        if (!visible(binding, scope, at)) continue;
        const auto symbol = resolveSymbol(u, text, records, at);
        if (moved && (!symbol.valid() || symbol.offset != binding.offset)) continue;
        related.push_back(object({ { "location", object({ { "uri", u }, { "range", range(text, binding.offset, binding.end) } }) }, { "message", "相关变量在此声明：" + binding.name } }));
        auto tokens = lex(text);
        const SymbolIdentity bindingIdentity{ u, binding.name, binding.kind, binding.offset, binding.end, { { u, binding.offset } } };
        for (size_t i = 1; i + 1 < tokens.size(); ++i)
        {
            if (tokens[i].a >= at) break;
            if (tokens[i].word == "defer" && tokens[i + 1].word == "{")
            {
                const auto stop = groupEnd(tokens, i + 1, "{", "}");
                for (size_t j = i + 2; j < stop && j < tokens.size(); ++j)
                    if (tokens[j].word == binding.name && resolveSymbol(u, text, records, tokens[j].a).same(bindingIdentity))
                    {
                        related.push_back(object({ { "location", object({ { "uri", u }, { "range", range(text, tokens[i].a, tokens[i].b) } }) }, { "message", "此处登记的 defer 清理仍读取该变量；退出时清理必须能访问有效来源。" } }));
                        break;
                    }
            }
            if (tokens[i].word == "move" && tokens[i + 1].word == "(" && i + 2 < tokens.size() && tokens[i + 2].word == binding.name && resolveSymbol(u, text, records, tokens[i + 2].a).same(bindingIdentity))
                related.push_back(object({ { "location", object({ { "uri", u }, { "range", range(text, tokens[i].a, tokens[i + 2].b) } }) }, { "message", "此处显式调用 move 使用该变量；请结合主诊断检查所有权转移。" } }));
            if (tokens[i - 1].word != "fn" || (tokens[i].word != "move" && tokens[i].word != "mut" && tokens[i].word != "const" && tokens[i].word != "copy")) continue;
            size_t body = i + 1;
            if (tokens[body].word != "(") continue;
            body = groupEnd(tokens, body, "(", ")");
            if (body >= tokens.size() || tokens[body].word != "{") continue;
            const auto end = groupEnd(tokens, body, "{", "}");
            for (size_t j = body + 1; j < end && j < tokens.size(); ++j)
                if (tokens[j].word == binding.name && resolveSymbol(u, text, records, tokens[j].a).same(bindingIdentity))
                {
                    related.push_back(object({ { "location", object({ { "uri", u }, { "range", range(text, tokens[i - 1].a, tokens[i].b) } }) }, { "message", "此闭包通过 fn " + tokens[i].word + " 使用该变量；请结合主诊断检查捕获与后续访问。" } }));
                    break;
                }
        }
    }
    if (!related.empty()) (*diagnostic.asObject())["relatedInformation"] = related;
    return diagnostic;
}
