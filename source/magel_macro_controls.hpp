/* 此文件在 magel_macro 命名空间中包含，复用宏词法与实参读取入口。 */
/**
 * @brief 宏条件的编译期值，支持整数、布尔和字符串比较。
 */
struct MacroConstant
{
    long long number = 0;  ///< 整数槽。
    Memory::String text;   ///< 字符串槽。
    bool string = false;   ///< 是否为字符串。
    /**
     * @brief 条件真值。
     * @return 是否为真。
     */
    bool truth() const { return string ? !text.empty() : number != 0; }
};
/**
 * @brief 宏条件常量表达式解析器；不接触语言运行时表达式。
 */
struct MacroCondition
{
    const Memory::String &source;                               ///< 条件文本。
    const Memory::Map<Memory::String, Memory::String> &values;  ///< 本次调用的文本绑定。
    Memory::String &error;                                      ///< 编译期诊断。
    std::size_t at = 0;                                         ///< 当前字符。
    unsigned depth = 0;                                         ///< 绑定求值深度。
    /**
     * @brief 读取一个常量原子。
     * @param enabled 是否实际求值。
     * @return 常量。
     */
    MacroConstant atom(bool enabled)
    {
        spaces(source, at);
        if (at == source.size())
        {
            error = "宏条件缺少常量表达式";
            return {};
        }
        if (source[at] == '(')
        {
            ++at;
            auto value = expression(1, enabled);
            spaces(source, at);
            if (at == source.size() || source[at++] != ')') error = "宏条件缺少 `)`";
            return value;
        }
        if (source[at] == '!' || source[at] == '-' || source[at] == '+' || source[at] == '~')
        {
            if (source[at] == '-' && at + 1 < source.size() && std::isdigit(static_cast<unsigned char>(source[at + 1])))
            {
                char *end = nullptr;
                errno = 0;
                const bool hex = source.compare(at + 1, 2, "0x") == 0 || source.compare(at + 1, 2, "0X") == 0;
                const auto number = std::strtoll(source.c_str() + at, &end, hex ? 16 : 10);
                if (errno == ERANGE && enabled) error = "编译期整数超出 int64 范围";
                at = static_cast<std::size_t>(end - source.c_str());
                return { number, {}, false };
            }
            const auto op = source[at++];
            auto value = atom(enabled);
            if (!enabled) return {};
            if (op == '!') return { value.truth() ? 0 : 1, {}, false };
            if (value.string)
            {
                error = "宏条件的一元算术需要整数";
                return {};
            }
            if (op == '-') value.number = static_cast<long long>(0ULL - static_cast<unsigned long long>(value.number));
            if (op == '~') value.number = ~value.number;
            return value;
        }
        if (source[at] == '"' || source[at] == '\'')
        {
            const auto quote = source[at++];
            Memory::String text;
            while (at < source.size() && source[at] != quote)
            {
                auto ch = source[at++];
                if (ch == '\\' && at < source.size())
                {
                    ch = source[at++];
                    if (ch == 'n')
                        ch = '\n';
                    else if (ch == 't')
                        ch = '\t';
                }
                text += ch;
            }
            if (at == source.size())
                error = "宏条件字符串缺少引号";
            else
                ++at;
            return { 0, text, true };
        }
        if (std::isdigit(static_cast<unsigned char>(source[at])))
        {
            char *end = nullptr;
            errno = 0;
            const bool hex = source.compare(at, 2, "0x") == 0 || source.compare(at, 2, "0X") == 0;
            const auto value = std::strtoll(source.c_str() + at, &end, hex ? 16 : 10);
            if (errno == ERANGE && enabled) error = "编译期整数超出 int64 范围";
            if (end == source.c_str() + at)
            {
                error = "宏条件整数不合法";
                return {};
            }
            at = static_cast<std::size_t>(end - source.c_str());
            return { value, {}, false };
        }
        const auto start = at;
        if (source[at] == '$') ++at;
        while (at < source.size() && identPart(source[at])) ++at;
        while (source.compare(at, 2, "::") == 0)
        {
            at += 2;
            while (at < source.size() && identPart(source[at])) ++at;
        }
        if (at == start)
        {
            error = "宏条件只能使用常量、形参和局部绑定";
            return {};
        }
        const auto name = source.substr(start, at - start);
        if (name == "true" || name == "false") return { name == "true" ? 1 : 0, {}, false };
        if (name == "$stringify")
        {
            spaces(source, at);
            Memory::Vector<Memory::String> args;
            std::size_t end = at;
            if (at >= source.size() || source[at] != '(' || !arguments(source, at, args, end) || args.size() != 1)
            {
                error = "$stringify 需要一个文本参数";
                return {};
            }
            at = end;
            const auto found = values.find(trim(args[0]));
            return { 0, trim(cleanPasteMarkers(found == values.end() ? args[0] : found->second, true)), true };
        }
        if (name == "$count")
        {
            spaces(source, at);
            Memory::Vector<Memory::String> args;
            std::size_t end = at;
            if (at >= source.size() || source[at] != '(' || !arguments(source, at, args, end) || args.size() != 1)
            {
                error = "$count 需要一个尾参名或文本绑定";
                return {};
            }
            at = end;
            const auto found = values.find(trim(args[0]));
            const auto text = found == values.end() ? args[0] : found->second;
            Memory::Vector<Memory::String> items;
            std::size_t ignored;
            arguments("(" + text + ")", 0, items, ignored);
            return { static_cast<long long>(items.size()), {}, false };
        }
        if (!enabled) return {};
        const auto found = values.find(name);
        if (found == values.end())
        {
            error = "宏条件不是编译期常量：" + name;
            return {};
        }
        if (depth >= 64)
        {
            error = "宏局部绑定求值超过 64 层";
            return {};
        }
        const auto text = cleanPasteMarkers(found->second, true);
        MacroCondition nested{ text, values, error, 0, depth + 1 };
        return nested.run();
    }
    /**
     * @brief 读取运算符优先级。
     * @param op 输出运算符。
     * @return 优先级，零表示结束。
     */
    int operation(Memory::String &op)
    {
        spaces(source, at);
        for (const auto item : { "||", "&&", "==", "!=", "<=", ">=", "<<", ">>", "|", "^", "&", "<", ">", "+", "-", "*", "/", "%" })
        {
            const Memory::String token = item;
            if (source.compare(at, token.size(), token)) continue;
            op = token;
            if (op == "||") return 1;
            if (op == "&&") return 2;
            if (op == "|") return 3;
            if (op == "^") return 4;
            if (op == "&") return 5;
            if (op == "==" || op == "!=") return 6;
            if (op == "<" || op == ">" || op == "<=" || op == ">=") return 7;
            if (op == "<<" || op == ">>") return 8;
            if (op == "+" || op == "-") return 9;
            return 10;
        }
        return 0;
    }
    /**
     * @brief 按优先级求值，逻辑运算保持短路。
     * @param minimum 最低优先级。
     * @param enabled 是否求值。
     * @return 结果。
     */
    MacroConstant expression(int minimum, bool enabled)
    {
        auto left = atom(enabled);
        while (error.empty())
        {
            Memory::String op;
            const int priority = operation(op);
            if (priority < minimum) break;
            at += op.size();
            const bool rightEnabled = enabled && !(op == "&&" && !left.truth()) && !(op == "||" && left.truth());
            const auto right = expression(priority + 1, rightEnabled);
            if (!enabled) continue;
            const auto a = left.number, b = right.number;
            if (op == "&&")
                left = { left.truth() && right.truth(), {}, false };
            else if (op == "||")
                left = { left.truth() || right.truth(), {}, false };
            else if (left.string || right.string)
            {
                if (left.string != right.string || (op != "==" && op != "!="))
                {
                    error = "宏条件字符串只支持同类型的 == / !=";
                    break;
                }
                left = { op == "==" ? left.text == right.text : left.text != right.text, {}, false };
            }
            else if (op == "==")
                left.number = a == b;
            else if (op == "!=")
                left.number = a != b;
            else if (op == "<")
                left.number = a < b;
            else if (op == ">")
                left.number = a > b;
            else if (op == "<=")
                left.number = a <= b;
            else if (op == ">=")
                left.number = a >= b;
            else if (op == "+")
                left.number = static_cast<long long>(static_cast<unsigned long long>(a) + static_cast<unsigned long long>(b));
            else if (op == "-")
                left.number = static_cast<long long>(static_cast<unsigned long long>(a) - static_cast<unsigned long long>(b));
            else if (op == "*")
                left.number = static_cast<long long>(static_cast<unsigned long long>(a) * static_cast<unsigned long long>(b));
            else if (op == "/" || op == "%")
            {
                if (!b)
                {
                    error = "宏条件除数不能为零";
                    break;
                }
                if (a == (-9223372036854775807LL - 1) && b == -1)
                    left.number = op == "/" ? a : 0;
                else
                    left.number = op == "/" ? a / b : a % b;
            }
            else if (op == "&")
                left.number = a & b;
            else if (op == "|")
                left.number = a | b;
            else if (op == "^")
                left.number = a ^ b;
            else
            {
                if (b < 0 || b >= 64)
                {
                    error = "宏条件移位次数必须在 0..63";
                    break;
                }
                left.number = op == "<<" ? static_cast<long long>(static_cast<unsigned long long>(a) << b) : a >> b;
            }
        }
        return left;
    }
    /**
     * @brief 求值整个条件并检查多余文本。
     * @return 编译期值。
     */
    MacroConstant run()
    {
        auto value = expression(1, true);
        spaces(source, at);
        if (at != source.size() && error.empty()) error = "宏条件中有多余内容：" + source.substr(at);
        return value;
    }
};

/**
 * @brief 替换局部文本绑定，字符串与注释保持原样。
 * @param text 源文。
 * @param values 局部绑定。
 * @return 替换结果。
 */
inline Memory::String substituteMacroLocals(const Memory::String &text,
                                            const Memory::Map<Memory::String, Memory::String> &values)
{
    Memory::String out;
    for (std::size_t i = 0; i < text.size();)
    {
        const auto begin = i;
        if (text[i] == '"' || text[i] == '\'')
        {
            const char quote = text[i++];
            while (i < text.size())
            {
                if (text[i] == '\\' && i + 1 < text.size())
                    i += 2;
                else if (text[i++] == quote)
                    break;
            }
        }
        else if (text.compare(i, 2, "//") == 0 || text.compare(i, 2, "/*") == 0)
        {
            const bool line = text[i + 1] == '/';
            const auto end = text.find(line ? "\n" : "*/", i + 2);
            i = end == Memory::String::npos ? text.size() : end + (line ? 1u : 2u);
        }
        else if (identStart(text[i]))
        {
            while (i < text.size() && identPart(text[i])) ++i;
            const auto word = text.substr(begin, i - begin);
            const auto found = values.find(word);
            out += found == values.end() ? word : found->second;
            continue;
        }
        else
            ++i;
        out += text.substr(begin, i - begin);
    }
    return out;
}
/**
 * @brief 选择宏内条件分支并创建调用局部绑定。
 * @param body 宏体。
 * @param values 展开后的形参。
 * @param rawValues 原始形参。
 * @param argc 实参数量。
 * @param error 诊断。
 * @param variadicParam 尾参名称。
 * @param tail 尾参元素。
 * @param protect 是否保护各元素。
 * @param depth 嵌套深度。
 * @param groupTails 多括号宏中各组的独立尾参；为空时使用原有单尾参。
 * @return 是否成功。
 */
inline bool expandMacroControls(Memory::String &body,
                                Memory::Map<Memory::String, Memory::String> &values,
                                Memory::Map<Memory::String, Memory::String> &rawValues, std::size_t argc, Memory::String &error,
                                const Memory::String &variadicParam, const Memory::Vector<Memory::String> &tail, bool protect, unsigned depth = 0,
                                const Memory::Map<Memory::String, Memory::Vector<Memory::String>> *groupTails = nullptr)
{
    if (depth > 64)
    {
        error = "宏控制块嵌套超过 64 层";
        return false;
    }
    struct Branch
    { bool parent, active, chosen, alternate = false; };
    Memory::Vector<Branch> branches;
    Memory::Map<Memory::String, Memory::String> locals;
    values["$argc"] = Memory::toString(argc);
    Memory::String output;
    bool comment = false;
    char quote = 0;
    for (std::size_t start = 0; start < body.size();)
    {
        const auto stop = body.find('\n', start);
        const auto line = body.substr(start, (stop == Memory::String::npos ? body.size() : stop) - start);
        const bool active = branches.empty() || branches.back().active;
        const bool control = !comment && !quote;
        if (control && directive(line, "$for"))
        {
            std::size_t closeBegin = stop == Memory::String::npos ? body.size() : stop + 1, closeEnd = body.size();
            int nested = 0;
            bool foundEnd = false;
            for (std::size_t cursor = closeBegin; cursor < body.size();)
            {
                const auto end = body.find('\n', cursor);
                const auto text = body.substr(cursor, (end == Memory::String::npos ? body.size() : end) - cursor);
                if (directive(text, "$for"))
                    ++nested;
                else if (directive(text, "$end_for"))
                {
                    if (!nested)
                    {
                        closeBegin = cursor;
                        closeEnd = end == Memory::String::npos ? body.size() : end + 1;
                        foundEnd = true;
                        break;
                    }
                    --nested;
                }
                cursor = end == Memory::String::npos ? body.size() : end + 1;
            }
            if (!foundEnd)
            {
                error = "$for 缺少配对的 $end_for";
                return false;
            }
            if (active)
            {
                std::size_t at = line.find("$for") + 4;
                spaces(line, at);
                const auto begin = at;
                while (at < line.size() && identPart(line[at])) ++at;
                const auto variable = line.substr(begin, at - begin);
                spaces(line, at);
                if (!identifierText(variable) || line.compare(at, 2, "in") || (at + 2 < line.size() && identPart(line[at + 2])))
                {
                    error = "$for 的写法是 $for 变量 in 尾参名";
                    return false;
                }
                at += 2;
                const auto list = trim(line.substr(at));
                const auto *elements = &tail;
                if (groupTails != nullptr)
                {
                    const auto found = groupTails->find(list);
                    elements = found == groupTails->end() ? nullptr : &found->second;
                }
                else if (variadicParam.empty() || list != variadicParam)
                    elements = nullptr;
                if (elements == nullptr)
                {
                    error = "$for 只能遍历变长尾参：" + list;
                    return false;
                }
                const auto inner = body.substr(stop + 1, closeBegin - stop - 1);
                for (const auto &element : *elements)
                {
                    Memory::Map<Memory::String, Memory::String> replacement{ { variable, protect ? "(" + element + ")" : element } };
                    auto expanded = substituteMacroLocals(inner, replacement);
                    auto innerValues = values, innerRaw = rawValues;
                    innerValues[variable] = element;
                    innerRaw[variable] = element;
                    if (!expandMacroControls(expanded, innerValues, innerRaw, argc, error, variadicParam, tail, protect, depth + 1, groupTails)) return false;
                    output += expanded;
                }
            }
            start = closeEnd;
            continue;
        }
        else if (control && directive(line, "$end_for"))
        {
            error = "$end_for 没有配对的 $for";
            return false;
        }
        else if (control && directive(line, "$if"))
        {
            bool selected = false;
            if (active)
            {
                const auto text = trim(line.substr(line.find("$if") + 3));
                MacroCondition evaluator{ text, values, error };
                selected = evaluator.run().truth();
            }
            branches.push_back({ active, active && selected, selected });
        }
        else if (control && (directive(line, "$elif") || directive(line, "$else")))
        {
            if (branches.empty() || branches.back().alternate)
            {
                error = "宏条件分支没有配对的 $if，或出现在 $else 之后";
                return false;
            }
            auto &branch = branches.back();
            bool selected = false;
            if (directive(line, "$else"))
            {
                branch.alternate = true;
                selected = !branch.chosen;
            }
            else if (branch.parent && !branch.chosen)
            {
                const auto text = trim(line.substr(line.find("$elif") + 5));
                MacroCondition evaluator{ text, values, error };
                selected = evaluator.run().truth();
            }
            branch.active = branch.parent && !branch.chosen && selected;
            branch.chosen = branch.chosen || selected;
        }
        else if (control && directive(line, "$end_if"))
        {
            if (branches.empty())
            {
                error = "$end_if 没有配对的 $if";
                return false;
            }
            branches.pop_back();
        }
        else if (control && directive(line, "$let"))
        {
            if (active)
            {
                std::size_t at = line.find("$let") + 4;
                spaces(line, at);
                const auto begin = at;
                while (at < line.size() && identPart(line[at])) ++at;
                const auto name = line.substr(begin, at - begin);
                spaces(line, at);
                if (!identifierText(name) || at >= line.size() || line[at++] != '=' || (values.count(name) && !locals.count(name)))
                {
                    error = "$let 需要新的局部名称与文本值：$let name = value";
                    return false;
                }
                const auto raw = substituteMacroLocals(trim(line.substr(at)), rawValues);
                const auto value = substituteMacroLocals(trim(line.substr(at)), values);
                if (value.empty())
                {
                    error = "$let 的文本值不能为空";
                    return false;
                }
                locals[name] = value;
                values[name] = value;
                rawValues[name] = raw;
            }
        }
        else
        {
            if (active) output += substituteMacroLocals(line, locals) + (stop == Memory::String::npos ? "" : "\n");
            for (std::size_t i = 0; i < line.size(); ++i)
            {
                if (comment)
                {
                    if (line.compare(i, 2, "*/") == 0)
                    {
                        comment = false;
                        ++i;
                    }
                }
                else if (quote)
                {
                    if (line[i] == '\\')
                        ++i;
                    else if (line[i] == quote)
                        quote = 0;
                }
                else if (line.compare(i, 2, "/*") == 0)
                {
                    comment = true;
                    ++i;
                }
                else if (line.compare(i, 2, "//") == 0)
                    break;
                else if (line[i] == '"' || line[i] == '\'')
                    quote = line[i];
            }
        }
        if (!error.empty()) return false;
        start = stop == Memory::String::npos ? body.size() : stop + 1;
    }
    if (!branches.empty())
    {
        error = "$if 缺少配对的 $end_if";
        return false;
    }
    body = std::move(output);
    return true;
}
