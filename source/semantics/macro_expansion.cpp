/**
 * @brief 在类型注册前展开已解析的表达式宏调用，保留实参结构和求值次数。
 * @param id 当前节点编号，负数表示省略。
 * @param registry 已登记当前编译宏定义的注册表。
 * @param depth 当前嵌套深度，用于限制递归宏。
 * @param budget 本次编译剩余的节点遍历预算。
 * @return 展开成功返回真，失败记录编译诊断。
 */
bool ExpandAstMacroNode(int id, chali_macro::Registry &registry, std::size_t depth, std::size_t &budget);

/**
 * @brief 为未完成实例化的调用错误保留定义位置及已有嵌套展开链。
 * @param call 发生错误的宏调用。
 * @param message 注册或替换失败的诊断。
 * @return 始终返回假，便于向调用方传播失败。
 */
bool ReportAstMacroFailure(int call, const Memory::String &message)
{
    const auto node = N(call);
    int declaration = -1, best = 0;
    for (std::size_t index = 0; index < g_nodes.size(); ++index)
    {
        const auto &candidate = g_nodes[index];
        if (candidate.kind != MNode::MacroDefinition || candidate.s != node.s) continue;
        int score = candidate.module == node.module ? 3 : candidate.module == "std::lang" ? 1 :
                                                                                            0;
        if (!score)
            for (const int root : g_top)
                if (N(root).kind == MNode::ImportDecl && N(root).module == node.module && N(root).s == candidate.module) score = 2;
        if (score > best)
        {
            best = score;
            declaration = static_cast<int>(index);
        }
    }
    const int previous = g_macroDiagnosticFrame;
    if (node.macroExpansion >= 0) g_macroDiagnosticFrame = node.macroExpansion;
    if (declaration >= 0)
    {
        const int frame = M_New(MNode::MacroExpansionFrame, node.s, declaration);
        g_nodes[frame].sourceLine = node.expansionLine ? node.expansionLine : node.sourceLine;
        g_nodes[frame].sourceColumn = node.expansionLine ? node.expansionColumn : node.sourceColumn;
        g_nodes[frame].kids = { node.macroExpansion };
        g_macroDiagnosticFrame = frame;
    }
    AddDiagnostic(message, node.expansionLine ? node.expansionLine : node.sourceLine,
                  node.expansionLine ? node.expansionColumn : node.sourceColumn);
    g_macroDiagnosticFrame = previous;
    return false;
}

/**
 * @brief 取得模板的表达式输出，显式书写的单个块沿用语言块表达式语义。
 * @param root 已实例化的模板根节点。
 * @return 表达式编号；模板不能用于表达式位置时返回负一。
 * @note 只转换用户原本写出的块，不给语句模板自动增加运行时作用域。
 */
int AstMacroExpression(int root)
{
    const auto &body = N(root);
    if (body.kind != MNode::MacroTemplate || body.kids.size() != 2) return -1;
    const auto &statements = N(body.kids[0]).kids;
    if (statements.empty()) return body.kids[1];
    if (body.kids[1] >= 0 || statements.size() != 1) return -1;
    const auto source = N(statements[0]);
    const int expression = source.kind == MNode::Block  ? M_BlockValue(statements[0]) :
                           source.kind == MNode::If     ? M_IfValue(statements[0]) :
                           source.kind == MNode::Switch ? M_MatchValue(statements[0]) :
                                                          -1;
    if (expression < 0) return -1;
    g_nodes[expression].syntaxContext = source.syntaxContext;
    g_nodes[expression].sourceLine = source.sourceLine;
    g_nodes[expression].sourceColumn = source.sourceColumn;
    g_nodes[expression].expansionLine = source.expansionLine;
    g_nodes[expression].expansionColumn = source.expansionColumn;
    g_nodes[expression].expansionModule = source.expansionModule;
    g_nodes[expression].macroExpansion = source.macroExpansion;
    return expression;
}

/**
 * @brief 将来源变量转换为普通字面量，字符串只在语言运行时解码一次。
 * @param id 宏调用节点。
 * @return 属于来源变量且替换成功时返回真。
 */
bool ExpandAstMacroSource(int id)
{
    auto &node = g_nodes[id];
    if (node.kind != MNode::MacroCall || (node.s != "line" && node.s != "file" && node.s != "module")) return false;
    if (node.kids.empty() || node.kids[0] >= 0 || node.kids.size() != 1) return false;
    const auto line = node.expansionLine ? node.expansionLine : node.sourceLine;
    if (node.s == "line")
    {
        node.kind = MNode::Int;
        node.i = line && line <= g_sourceLines.size() && g_sourceLines[line - 1] ? g_sourceLines[line - 1] : line;
        node.s.clear();
    }
    else
    {
        const auto value = node.s == "module"                                                       ? (node.expansionLine ? node.expansionModule : node.module) :
                           line && line <= g_sourceFiles.size() && !g_sourceFiles[line - 1].empty() ? g_sourceFiles[line - 1] :
                                                                                                      g_sourceFallback;
        Memory::String escaped;
        for (const char ch : value)
            switch (ch)
            {
            case '\\': escaped += "\\\\"; break;
            case '"': escaped += "\\\""; break;
            case '\n': escaped += "\\n"; break;
            case '\r': escaped += "\\r"; break;
            case '\t': escaped += "\\t"; break;
            case '\0': escaped += "\\0"; break;
            default: escaped += ch; break;
            }
        node.kind = MNode::Str;
        node.s = std::move(escaped);
    }
    node.kids.clear();
    return true;
}

/**
 * @brief 实现保留原文的字符串化与仅生成标识符的拼名操作。
 * @param id 内建宏调用。
 * @param error 接收实参或标识符错误。
 * @return 属于这两个内建宏时返回真，错误通过 error 表示。
 */
bool ExpandAstMacroText(int id, Memory::String &error)
{
    auto &node = g_nodes[id];
    if (node.kind != MNode::MacroCall || (node.s != "stringify" && node.s != "concat")) return false;
    if (node.kids.size() != 1 || node.kids[0] < 0 || g_nodes[node.kids[0]].kind != MNode::MacroArguments || g_nodes[node.kids[0]].kids.size() != 1)
    {
        error = "字符串化与拼名宏需要一组圆括号参数";
        return true;
    }
    const auto &group = g_nodes[g_nodes[node.kids[0]].kids[0]];
    if (group.i != '(' || (node.s == "stringify" && group.kids.size() != 1) || group.kids.empty())
    {
        error = "字符串化需要一个片段，拼名需要至少一个片段";
        return true;
    }
    Memory::String value;
    for (const int child : group.kids)
    {
        if (g_nodes[child].kind != MNode::MacroArgument && !(node.s == "stringify" && g_nodes[child].kind == MNode::MacroVariadic))
        {
            error = "宏文本操作需要保留原文的片段参数";
            return true;
        }
        auto text = g_nodes[child].s;
        if (node.s == "concat")
        {
            const auto begin = text.find_first_not_of(" \t\r\n");
            const auto end = text.find_last_not_of(" \t\r\n");
            text = begin == Memory::String::npos ? Memory::String{} : text.substr(begin, end - begin + 1);
        }
        value += text;
    }
    if (node.s == "stringify")
    {
        Memory::String escaped;
        for (const char ch : value)
            switch (ch)
            {
            case '\\': escaped += "\\\\"; break;
            case '"': escaped += "\\\""; break;
            case '\n': escaped += "\\n"; break;
            case '\r': escaped += "\\r"; break;
            case '\t': escaped += "\\t"; break;
            case '\0': escaped += "\\0"; break;
            default: escaped += ch; break;
            }
        node.kind = MNode::Str;
        node.s = std::move(escaped);
    }
    else
    {
        const auto first = [](unsigned char ch) { return std::isalpha(ch) || ch == '_'; };
        if (value.empty() || !first(static_cast<unsigned char>(value[0])) || !std::all_of(value.begin(), value.end(), [&](unsigned char ch) { return first(ch) || std::isdigit(ch); }))
        {
            error = "拼名宏只能生成单个标识符";
            return true;
        }
        node.kind = MNode::Var;
        node.s = std::move(value);
    }
    node.kids.clear();
    return true;
}

/**
 * @brief 将已展开的调用者声明加入后续宏可见环境，不穿透显式运行时块。
 * @param id 已生成的当前层语句。
 * @param exports 调用者名字到当前声明身份的映射。
 */
void RememberAstMacroExports(int id, Memory::Map<Memory::String, std::uint32_t> &exports)
{
    if (id < 0) return;
    const auto &node = N(id);
    if (node.kind == MNode::Destructure && !node.i)
        VisitAstLocalPatternBindings(node, [&](int binding) {
            const auto &pattern = N(binding);
            if (!pattern.syntaxContext && pattern.bindingIdentity && pattern.s != "_") exports[pattern.s] = pattern.bindingIdentity;
        });
    if (node.kind != MNode::Declare) return;
    if (node.kids.size() == 1 && node.kids[0] >= 0 && N(node.kids[0]).kind == MNode::List)
    {
        for (const int child : N(node.kids[0]).kids) RememberAstMacroExports(child, exports);
        return;
    }
    if (!node.syntaxContext && node.bindingIdentity) exports[node.s] = node.bindingIdentity;
}

/**
 * @brief 后续原始调用按前面已导出的声明绑定，实参内部自己的声明仍保留身份。
 * @param root 尚未展开的当前语句。
 * @param exports 当前层已有的调用者声明。
 */
void RebindAstMacroExports(int root, const Memory::Map<Memory::String, std::uint32_t> &exports)
{
    if (exports.empty()) return;
    Memory::Set<std::uint32_t> locals;
    Memory::Set<int> seen;
    Memory::Function<void(int, bool, bool, std::size_t)> visit;
    visit = [&](int id, bool argument, bool collect, std::size_t depth) {
        if (id < 0 || depth >= 256 || !seen.insert(id).second) return;
        auto &node = g_nodes[id];
        if (node.kind == MNode::MacroDefinition) return;
        if (collect && (node.kind == MNode::Params || node.kind == MNode::Declare || node.kind == MNode::Foreach) && node.bindingIdentity)
            locals.insert(node.bindingIdentity);
        if (collect)
            VisitAstLocalPatternBindings(node, [&](int binding) {
                if (N(binding).bindingIdentity) locals.insert(N(binding).bindingIdentity);
            });
        argument = argument || node.kind == MNode::MacroArgument;
        if (!collect && argument && node.kind == MNode::Var && !node.syntaxContext && !locals.count(node.bindingIdentity))
        {
            const auto binding = exports.find(node.s);
            if (binding != exports.end()) node.bindingIdentity = binding->second;
        }
        for (const int child : node.kids) visit(child, argument, collect, depth + 1);
        for (const int annotation : node.annotations) visit(annotation, argument, collect, depth + 1);
        visit(node.guard, argument, collect, depth + 1);
        const bool group = node.kind == MNode::Declare && node.kids.size() == 1 && node.kids[0] >= 0 && N(node.kids[0]).kind == MNode::List;
        const bool indexed = node.kind == MNode::Block || node.kind == MNode::Case || node.kind == MNode::NewPtr || ((node.kind == MNode::Declare || node.kind == MNode::Field) && !group && node.i >= 0) || (node.kind == MNode::Index && node.s2 == "__chali_bare_index");
        if (indexed) visit(static_cast<int>(node.i), argument, collect, depth + 1);
    };
    visit(root, false, true, 0);
    seen.clear();
    visit(root, false, false, 0);
}

/**
 * @brief 将语句位置的宏模板直接拼入原语句列表，不增加运行时作用域。
 * @param id 语句列表节点。
 * @param registry 本次编译的宏注册表。
 * @param depth 当前展开深度。
 * @param budget 剩余遍历预算。
 * @return 拼接及嵌套展开成功时返回真。
 */
bool ExpandAstMacroStatements(int id, chali_macro::Registry &registry, std::size_t depth, std::size_t &budget)
{
    if (depth >= 256 || budget == 0) return Fail("宏展开超过深度或规模预算"), false;
    --budget;
    const auto original = g_nodes[id].kids;
    Memory::Vector<int> statements;
    Memory::Map<Memory::String, std::uint32_t> exports;
    std::size_t remembered = 0;
    for (const int child : original)
    {
        while (remembered < statements.size()) RememberAstMacroExports(statements[remembered++], exports);
        RebindAstMacroExports(child, exports);
        if (g_nodes[child].kind == MNode::MacroTemplate)
        {
            const auto body = g_nodes[child];
            if (!ExpandAstMacroStatements(body.kids[0], registry, depth + 1, budget)) return false;
            const auto &items = g_nodes[body.kids[0]].kids;
            statements.insert(statements.end(), items.begin(), items.end());
            if (body.kids[1] >= 0)
            {
                if (!ExpandAstMacroNode(body.kids[1], registry, depth + 1, budget)) return false;
                statements.push_back(M_ExprStmt(body.kids[1]));
            }
            continue;
        }
        if (g_nodes[child].kind == MNode::ExprStmt && g_nodes[child].kids.size() == 1)
        {
            ExpandAstMacroSource(g_nodes[child].kids[0]);
            Memory::String error;
            if (ExpandAstMacroText(g_nodes[child].kids[0], error) && !error.empty())
                return ReportAstMacroFailure(g_nodes[child].kids[0], error);
        }
        const auto statement = g_nodes[child];
        const bool invocation = statement.kind == MNode::ExprStmt && statement.kids.size() == 1 && g_nodes[statement.kids[0]].kind == MNode::MacroCall;
        if (!invocation)
        {
            if (!ExpandAstMacroNode(child, registry, depth + 1, budget)) return false;
            statements.push_back(child);
            continue;
        }
        const int call = statement.kids[0];
        ReductionSourceScope source(call);
        std::string error;
        const int expanded = registry.instantiate(call, error);
        if (expanded == -2)
        {
            statements.push_back(child);
            continue;
        }
        if (expanded < 0) return ReportAstMacroFailure(call, Memory::String(error.data(), error.size()));
        RecordEditorMacro(call, expanded);
        const auto body = g_nodes[expanded];
        if (!ExpandAstMacroStatements(body.kids[0], registry, depth + 1, budget)) return false;
        const auto &items = g_nodes[body.kids[0]].kids;
        statements.insert(statements.end(), items.begin(), items.end());
        if (body.kids[1] >= 0)
        {
            if (!ExpandAstMacroNode(body.kids[1], registry, depth + 1, budget)) return false;
            statements.push_back(M_ExprStmt(body.kids[1]));
        }
    }
    g_nodes[id].kids = std::move(statements);
    return true;
}

/**
 * @brief 展开表达式及结构子树中的宏，保留调用处完整表达式边界。
 * @param id 待处理节点。
 * @param registry 当前宏注册表。
 * @param depth 当前深度。
 * @param budget 剩余节点预算。
 * @return 展开成功返回真。
 */
bool ExpandAstMacroNode(int id, chali_macro::Registry &registry, std::size_t depth, std::size_t &budget)
{
    if (id < 0) return true;
    if (depth >= 256 || budget == 0) return Fail("宏展开超过深度或规模预算"), false;
    --budget;
    auto &node = g_nodes[id];
    if (ExpandAstMacroSource(id)) return true;
    Memory::String textError;
    if (ExpandAstMacroText(id, textError))
        return textError.empty() || ReportAstMacroFailure(id, textError);
    if (node.kind == MNode::MacroDefinition) return true;
    if (node.kind == MNode::MacroFor || node.kind == MNode::MacroMatch || node.kind == MNode::MacroRegex)
        return AddDiagnostic("编译期分支与片段循环必须位于宏模板内部", node.sourceLine, node.sourceColumn), false;
    if (node.kind == MNode::List) return ExpandAstMacroStatements(id, registry, depth, budget);
    if (node.kind == MNode::MacroCall)
    {
        ReductionSourceScope source(id);
        std::string error;
        const int expanded = registry.instantiate(id, error);
        if (expanded == -2) return true;
        if (expanded < 0) return ReportAstMacroFailure(id, Memory::String(error.data(), error.size()));
        RecordEditorMacro(id, expanded);
        const int expression = AstMacroExpression(expanded);
        if (expression < 0)
            return AddDiagnostic("表达式位置的宏必须产生一个表达式；语句模板请在语句位置调用", node.sourceLine, node.sourceColumn), false;
        if (!ExpandAstMacroNode(expression, registry, depth + 1, budget)) return false;
        const bool checkedExpression = node.checkedExpression;
        node = g_nodes[expression];
        node.checkedExpression = node.checkedExpression || checkedExpression;
        return true;
    }
    for (const int child : node.kids)
        if (!ExpandAstMacroNode(child, registry, depth + 1, budget)) return false;
    for (const int annotation : node.annotations)
        if (!ExpandAstMacroNode(annotation, registry, depth + 1, budget)) return false;
    if (!ExpandAstMacroNode(node.guard, registry, depth + 1, budget)) return false;
    const bool declarationGroup = node.kind == MNode::Declare && node.kids.size() == 1 && node.kids[0] >= 0 && g_nodes[node.kids[0]].kind == MNode::List;
    const bool indexedChild = node.kind == MNode::Block || node.kind == MNode::Case || node.kind == MNode::NewPtr || ((node.kind == MNode::Declare || node.kind == MNode::Field) && !declarationGroup && node.i >= 0) || (node.kind == MNode::Index && node.s2 == "__chali_bare_index");
    return !indexedChild || ExpandAstMacroNode(static_cast<int>(node.i), registry, depth + 1, budget);
}

/**
 * @brief 注册当前程序的 AST 宏并在四元式生成前展开表达式模板。
 * @return 登记与展开全部成功时返回真。
 * @note 模板仅以 AST 替换，原始实参只用于语法候选、来源及文本工具。
 */
bool ExpandAstMacros()
{
    g_macroSyntaxFiltering = false;
    g_astGenericClosures.clear();
    g_astGenericImplementations.clear();
    Memory::Set<int> fragmentsSeen;
    for (const int id : g_top)
        if (!MaterializeMacroFragments(id, fragmentsSeen, 0)) return false;
    g_macroRegistry.reset();
    g_macroResolver = std::make_unique<chali_macro::HygieneResolver>();
    g_macroRegistry = std::make_unique<chali_macro::Registry>(g_nodes, *g_macroResolver, &g_types);
    auto &registry = *g_macroRegistry;
    for (const int id : g_top)
        if (g_nodes[id].kind == MNode::ImportDecl)
            registry.addImport(std::string(g_nodes[id].module.data(), g_nodes[id].module.size()), std::string(g_nodes[id].s.data(), g_nodes[id].s.size()));
    bool hasDefinitions = false;
    for (const int id : g_top)
        if (g_nodes[id].kind == MNode::MacroDefinition)
        {
            hasDefinitions = true;
            g_macroSyntaxFiltering = true;
            std::string error;
            if (!registry.add(id, 0, error))
                return AddDiagnostic(Memory::String(error.data(), error.size()), g_nodes[id].sourceLine, g_nodes[id].sourceColumn), false;
        }
    if (!hasDefinitions && std::none_of(g_nodes.begin(), g_nodes.end(), [](const MNode &node) {
            return node.kind == MNode::MacroCall || node.kind == MNode::MacroFor || node.kind == MNode::MacroMatch || node.kind == MNode::MacroRegex;
        })) return true;
    for (const int id : g_top)
    {
        Memory::Map<Memory::String, std::uint32_t> callerBindings;
        if (!BindMacroCallerSyntax(id, callerBindings, 0)) return false;
    }
    std::size_t budget = 1000000;
    for (const int id : g_top)
        if (!ExpandAstMacroNode(id, registry, 0, budget)) return false;
    g_top.erase(std::remove_if(g_top.begin(), g_top.end(), [](int id) { return g_nodes[id].kind == MNode::MacroDefinition; }), g_top.end());
    return true;
}

/**
 * @brief 按具体类型替换模板中的类型参数，克隆类型图以免修改定义和调用实参。
 * @param root 实例化模板根节点。
 * @param arguments 泛型名称到具体类型池编号的映射。
 * @param error 接收结构或深度错误。
 * @return 替换成功时返回真。
 */
bool ApplyAstMacroTypes(int root, const std::map<std::string, int> &arguments, std::string &error)
{
    std::function<std::shared_ptr<MType>(const std::shared_ptr<MType> &, std::size_t)> bind;
    bind = [&](const std::shared_ptr<MType> &type, std::size_t depth) -> std::shared_ptr<MType> {
        if (!type) return nullptr;
        if (depth >= 256)
        {
            error = "宏类型替换超过类型深度预算";
            return nullptr;
        }
        const auto key = std::string(type->name.data(), type->name.size());
        const auto replacement = arguments.find(key);
        if (type->kind == MType::Struct && replacement != arguments.end()) return T(replacement->second);
        if (type->kind == MType::Struct)
        {
            const auto split = key.find("::");
            if (split != std::string::npos && arguments.count(key.substr(0, split)))
            {
                auto qualified = Memory::makeShared<MType>(*T(arguments.at(key.substr(0, split))));
                qualified->genericMember.assign(key.data() + split + 2, key.size() - split - 2);
                return qualified;
            }
        }
        auto result = Memory::makeShared<MType>(*type);
        result->elem = bind(type->elem, depth + 1);
        result->elem2 = bind(type->elem2, depth + 1);
        for (auto &parameter : result->paramTypes) parameter = bind(parameter, depth + 1);
        for (auto &argument : result->genericArguments) argument = bind(argument, depth + 1);
        return result;
    };
    Memory::Set<int> seen;
    std::function<bool(int, std::size_t)> visit;
    visit = [&](int id, std::size_t depth) {
        if (id < 0 || !seen.insert(id).second) return true;
        if (depth >= 256)
        {
            error = "宏类型替换超过语法深度预算";
            return false;
        }
        auto &node = g_nodes[id];
        // 调用者片段保留自己的类型环境，不能被宏的同名类型参数改写。
        if (!node.syntaxContext && !node.bindingContext) return true;
        if (node.type >= 0) node.type = M_AddType(bind(T(node.type), 0));
        if (!error.empty()) return false;
        if (node.kind == MNode::Var || node.kind == MNode::EnumCtor)
            for (const auto &argument : arguments)
            {
                const Memory::String name(argument.first.data(), argument.first.size());
                if (node.s == name || node.s.rfind(name + "::", 0) == 0)
                {
                    node.s = Describe(T(argument.second)) + node.s.substr(name.size());
                    break;
                }
            }
        for (const int child : node.kids)
            if (!visit(child, depth + 1)) return false;
        for (const int annotation : node.annotations)
            if (!visit(annotation, depth + 1)) return false;
        if (!visit(node.guard, depth + 1)) return false;
        const bool group = node.kind == MNode::Declare && node.kids.size() == 1 && node.kids[0] >= 0 && N(node.kids[0]).kind == MNode::List;
        const bool indexed = node.kind == MNode::Block || node.kind == MNode::Case || node.kind == MNode::NewPtr || ((node.kind == MNode::Declare || node.kind == MNode::Field) && !group && node.i >= 0) || (node.kind == MNode::Index && node.s2 == "__chali_bare_index");
        return !indexed || visit(static_cast<int>(node.i), depth + 1);
    };
    return visit(root, 0);
}

/**
 * @brief 使用静态实参类型或外层期望类型展开泛型宏，不求值实参。
 * @param id 表达式位置的宏调用。
 * @param expected 外层期望结果类型。
 * @param templateRoot 非空时接收模板供语句位置展开。
 * @return 成功替换为普通表达式时返回真。
 */
bool ExpandDeferredAstMacro(int id, const std::shared_ptr<MType> &expected, int *templateRoot)
{
    if (N(id).kind != MNode::MacroCall || !g_macroRegistry) return false;
    if (g_macroSemanticDepth >= 256) return Fail("宏类型推导发生循环或超过展开深度预算"), false;
    ++g_macroSemanticDepth;
    struct RestoreDepth
    {
        /**
         * @brief 恢复递归推导深度，即使类型推导抛出实例化请求也不残留状态。
         */
        ~RestoreDepth() { --g_macroSemanticDepth; }
    } restoreDepth;
    const auto original = N(id);
    std::string error;
    const auto activate = [&](int root) {
        const auto size = g_top.size();
        struct RestoreRoots
        {
            std::size_t size;
            /**
             * @brief 即使类型实例化请求中断本次推导，也撤销临时的类型遍历入口。
             */
            ~RestoreRoots() { g_top.resize(size); }
        } restore{ size };
        g_top.push_back(root);
        RefreshMacroActiveSyntax();
        if (!ResolveImportedTypeNames(false)) return false;
        for (const auto &type : g_types)
            if (g_macroActiveTypes.count(type.get()) && !ResolveAstGenericType(type)) return false;
        return true;
    };
    const auto resolve = [&](int generic, int argument, int explicitType) -> int {
        std::shared_ptr<MType> type;
        if (explicitType >= 0)
        {
            if (!activate(explicitType)) return -1;
            type = T(N(explicitType).type);
            if (!ResolveAstGenericType(type)) return -1;
        }
        else if (argument >= 0)
        {
            const auto &fragment = N(argument);
            if (fragment.kind != MNode::MacroArgument || fragment.kids.size() != 5 || fragment.kids[0] < 0)
            {
                AddDiagnostic("宏泛型的实参推导来源必须是完整表达式", fragment.sourceLine, fragment.sourceColumn);
                return -1;
            }
            if (!PrepareMacroArgumentLambdas(fragment.kids[0]) || !activate(fragment.kids[0])) return -1;
            type = ExprType(fragment.kids[0]);
        }
        else
            type = expected;
        if (!type || type->kind == MType::Void)
        {
            AddDiagnostic("宏泛型缺少静态类型约束：" + N(generic).s, original.sourceLine, original.sourceColumn);
            return -1;
        }
        return M_AddType(type);
    };
    const int expanded = g_macroRegistry->instantiate(id, error, resolve, ApplyAstMacroTypes);
    if (expanded < 0) return ReportAstMacroFailure(id, Memory::String(error.data(), error.size()));
    RecordEditorMacro(id, expanded);
    if (!activate(expanded)) return false;
    if (templateRoot)
    {
        std::size_t budget = 1000000;
        if (!ExpandAstMacroStatements(N(expanded).kids[0], *g_macroRegistry, 0, budget)) return false;
        if (N(expanded).kids[1] >= 0 && !ExpandAstMacroNode(N(expanded).kids[1], *g_macroRegistry, 0, budget)) return false;
        if (!activate(expanded)) return false;
        *templateRoot = expanded;
        return true;
    }
    const int expression = AstMacroExpression(expanded);
    if (expression < 0)
        return AddDiagnostic("表达式位置的宏必须产生一个表达式；语句模板请在语句位置调用", original.sourceLine, original.sourceColumn), false;
    std::size_t budget = 1000000;
    if (!ExpandAstMacroNode(expression, *g_macroRegistry, 0, budget)) return false;
    g_nodes[id] = N(expression);
    g_nodes[id].checkedExpression = original.checkedExpression || g_nodes[id].checkedExpression;
    if (!PrepareExpandedMacroLambdas()) return false;
    InferExpression(id, expected);
    return true;
}

/**
 * @brief 将普通泛型实例中新解析出的调用接到同一卫生宏会话。
 * @return 所有新增根完成片段分类、调用绑定及非泛型展开时返回真。
 */
bool ExpandAdditionalAstMacros()
{
    if (!g_macroRegistry) return true;
    Memory::Set<int> seen;
    std::size_t budget = 1000000;
    for (const int id : g_top)
    {
        if (!MaterializeMacroFragments(id, seen, 0)) return false;
        Memory::Map<Memory::String, std::uint32_t> bindings;
        if (!BindMacroCallerSyntax(id, bindings, 0)) return false;
        if (!ExpandAstMacroNode(id, *g_macroRegistry, 0, budget)) return false;
    }
    return true;
}
