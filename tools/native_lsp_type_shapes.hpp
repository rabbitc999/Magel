/**
 * @brief 编辑期函数类型结构，保留嵌套返回类型及逐项参数类型。
 */
struct LspFunctionShape
{
    std::string result;
    std::vector<std::string> arguments;
    bool valid = false;
};

/**
 * @brief 取得完整块的作用域终点，未闭合块保留至缓冲区末尾之后。
 * @param tokens 当前快照的词法项。
 * @param open 左大括号索引。
 * @return 完整右括号后的偏移；尚未闭合时返回 npos。
 */
size_t lspBlockLimit(const std::vector<Token> &tokens, size_t open)
{
    const auto end = groupEnd(tokens, open, "{", "}");
    return end && end <= tokens.size() && tokens[end - 1].word == "}" ? tokens[end - 1].b : std::string::npos;
}

/**
 * @brief 去掉包围整个类型的分组括号。
 * @param text 类型文本。
 * @return 去除外层分组后的类型。
 */
std::string lspUngroup(std::string text)
{
    text = trim(text);
    for (;;)
    {
        auto tokens = lex(text);
        if (tokens.empty() || tokens.front().word != "(" || groupEnd(tokens, 0, "(", ")") != tokens.size()) return text;
        text = trim(text.substr(tokens.front().b, tokens.back().a - tokens.front().b));
    }
}

/**
 * @brief 按函数类型的外层 fn 拆分返回类型与参数，忽略函数体。
 * @param text 类型、带类型匿名函数或声明文本。
 * @return 可识别的函数类型结构；缺少外层 fn 时 valid 为 false。
 */
LspFunctionShape lspFunctionShape(const std::string &text)
{
    auto tokens = lex(text);
    int depth = 0;
    for (size_t i = 0; i < tokens.size(); ++i)
    {
        const auto &word = tokens[i].word;
        if (word == "fn" && depth == 0)
        {
            size_t open = i + 1;
            if (open < tokens.size() && (tokens[open].word == "copy" || tokens[open].word == "move" || tokens[open].word == "mut" || tokens[open].word == "const")) ++open;
            if (open >= tokens.size() || tokens[open].word != "(") return {};
            const auto end = groupEnd(tokens, open, "(", ")");
            if (!end || end > tokens.size() || tokens[end - 1].word != ")") return {};
            LspFunctionShape shape;
            shape.valid = true;
            shape.result = lspUngroup(text.substr(0, tokens[i].a));
            size_t begin = open + 1;
            int nesting = 0;
            for (size_t j = begin; j < end; ++j)
            {
                const auto &w = tokens[j].word;
                if (nesting == 0 && (w == "," || j + 1 == end))
                {
                    if (begin < j)
                    {
                        size_t stop = j;
                        // 只剥离声明末端的形参名；限定名和单独的类型名须保留。
                        if (stop > begin + 1 && identifier(tokens[stop - 1].word) && !keywords.count(tokens[stop - 1].word) && tokens[stop - 2].word != "::") --stop;
                        shape.arguments.push_back(cleanSignature(text.substr(tokens[begin].a, tokens[stop - 1].b - tokens[begin].a)));
                    }
                    begin = j + 1;
                    continue;
                }
                if (w == "(" || w == "[" || w == "<") ++nesting;
                if (w == ")" || w == "]" || w == ">") --nesting;
            }
            return shape;
        }
        if (word == "(" || word == "[" || word == "<") ++depth;
        if (word == ")" || word == "]" || word == ">") --depth;
        if (word == "{" || word == ";") break;
    }
    return {};
}

/**
 * @brief 把函数类型结构还原为适合悬停与内联提示的签名。
 * @param shape 已解析的函数类型。
 * @return 规范化函数类型；未知返回空串。
 */
std::string lspFunctionText(const LspFunctionShape &shape)
{
    if (!shape.valid || shape.result.empty()) return "";
    std::string text = shape.result.find("fn") == std::string::npos ? shape.result : "(" + shape.result + ")";
    text += " fn(";
    for (size_t i = 0; i < shape.arguments.size(); ++i) text += (i ? ", " : "") + shape.arguments[i];
    return text + ")";
}

/**
 * @brief 为完整推断类型生成函数层次或泛型实参说明。
 * @param type 唯一推断的完整类型。
 * @param depth 函数返回类型的嵌套层次。
 * @return Markdown 说明，完整类型不截断。
 */
std::string lspTypeDescription(const std::string &type, unsigned depth = 0)
{
    if (type.empty() || type == "auto") return "尚不能唯一确定类型；以完整编译检查结果为准。";
    std::string description = "完整类型：`" + type + "`";
    if (depth >= 8) return description;
    const auto function = lspFunctionShape(type);
    if (function.valid)
    {
        description += "\n\n返回类型：`" + function.result + "`";
        for (size_t i = 0; i < function.arguments.size(); ++i) description += "\n\n参数 " + std::to_string(i + 1) + "：`" + function.arguments[i] + "`";
        if (lspFunctionShape(function.result).valid) description += "\n\n返回的函数：\n\n" + lspTypeDescription(function.result, depth + 1);
        return description;
    }
    const auto open = type.find('<');
    if (open != type.npos)
    {
        Memory::Vector<Memory::String> arguments;
        size_t end = open;
        if (magel_macro::groupArguments(type.c_str(), open, arguments, end))
            for (size_t i = 0; i < arguments.size(); ++i) description += "\n\n泛型实参 " + std::to_string(i + 1) + "：`" + trim(arguments[i].c_str()) + "`";
    }
    return description;
}
