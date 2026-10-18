/**
 * @brief 宏模板中一个有独立着色角色的原始文本片段。
 */
struct LspMacroToken
{
    size_t end;
    int kind, modifiers;
};

/**
 * @brief 根据真实宏头和文本指令标记模板形参、循环绑定、指令及整数槽。
 * @param text 原始未展开源码。
 * @param tokens 跳过注释和字符串的快照词法项。
 * @return 以字节起点索引的语义着色片段；不生成 AST 或检查文本参数类型。
 */
std::map<size_t, LspMacroToken> lspMacroTokens(const std::string &text, const std::vector<Token> &tokens)
{
    std::map<size_t, LspMacroToken> result;
    const std::set<std::string> directives{ "$macro", "$end_macro", "$for", "$end_for", "$if", "$elif", "$else", "$end_if", "$let", "$replace", "$delete_macro" };
    const std::set<std::string> values{ "$line", "$file", "$module", "$argc", "$count", "$typecheck" };
    std::set<std::string> parameters, locals;
    std::vector<std::string> loops;
    size_t headerEnd = 0, nameOffset = std::string::npos;
    bool inside = false;
    const Memory::String raw(text.c_str());
    for (size_t i = 0; i < tokens.size(); ++i)
    {
        const auto &token = tokens[i];
        const auto &word = token.word;
        const auto newline = directives.count(word) ? text.rfind('\n', token.a) : token.a;
        const bool leading = directives.count(word) && trim(text.substr(newline == text.npos ? 0 : newline + 1, token.a - (newline == text.npos ? 0 : newline + 1))).empty();
        if (word == "$macro" && leading)
        {
            inside = true;
            parameters.clear();
            locals.clear();
            loops.clear();
            headerEnd = text.find('\n', token.b);
            if (headerEnd == text.npos) headerEnd = text.size();
            const auto name = i + 1 < tokens.size() && tokens[i + 1].word == "expr" ? i + 2 : i + 1;
            nameOffset = name < tokens.size() && tokens[name].a < headerEnd ? tokens[name].a : std::string::npos;
            magel_macro::Macro macro;
            Memory::String macroName, problem;
            if (magel_macro::header(text.substr(token.a, headerEnd - token.a).c_str(), macroName, macro, problem))
            {
                for (const auto &parameter : macro.params) parameters.insert(parameter.c_str());
                if (!macro.variadicParam.empty()) parameters.insert(macro.variadicParam.c_str());
            }
        }
        if (directives.count(word))
            result[token.a] = { token.b, 7, 0 };
        else if (values.count(word))
            result[token.a] = { token.b, 4, 2 };
        else if (!inside && starts(word, "$") && identifier(word.substr(1)))
            result[token.a] = { token.b, 6, 0 };
        if (!inside) continue;
        if (word == "$end_macro" && leading)
        {
            inside = false;
            parameters.clear();
            locals.clear();
            loops.clear();
            continue;
        }
        if (token.a == nameOffset)
            result[token.a] = { token.b, 6, 1 };
        else if (token.a < nameOffset && word == "expr")
            result[token.a] = { token.b, 7, 0 };
        else if (locals.count(word) || std::find(loops.rbegin(), loops.rend(), word) != loops.rend())
            result[token.a] = { token.b, 4, i && (tokens[i - 1].word == "$for" || tokens[i - 1].word == "$let") ? 3 : 2 };
        else if (parameters.count(word))
            result[token.a] = { token.b, 3, token.a < headerEnd ? 3 : 2 };
        if (word == "$for" && leading && i + 2 < tokens.size() && identifier(tokens[i + 1].word) && tokens[i + 2].word == "in")
        {
            loops.push_back(tokens[i + 1].word);
            result[tokens[i + 1].a] = { tokens[i + 1].b, 4, 3 };
            result[tokens[i + 2].a] = { tokens[i + 2].b, 7, 0 };
        }
        else if (word == "$end_for" && leading && !loops.empty())
            loops.pop_back();
        else if (word == "$let" && leading && i + 1 < tokens.size() && identifier(tokens[i + 1].word))
        {
            locals.insert(tokens[i + 1].word);
            result[tokens[i + 1].a] = { tokens[i + 1].b, 4, 3 };
        }
        else if (starts(word, "$"))
        {
            const auto end = magel_macro::placeholderEnd(raw, token.a);
            if (end > token.a + 1)
                result[token.a] = { end, 4, 2 };
            else if (!directives.count(word) && !values.count(word))
                result[token.a] = { token.b, 6, 0 };
        }
    }
    return result;
}
