/**
 * @brief 为调用者模式的声明叶子分配稳定身份，保持嵌套遮蔽的词法顺序。
 * @param id 模式根。
 * @param bindings 当前层可见声明。
 * @param depth 当前模式深度。
 * @return 模式结构未超过深度预算时返回真。
 */
bool BindMacroCallerPattern(int id, Memory::Map<Memory::String, std::uint32_t> &bindings, std::size_t depth)
{
    if (id < 0) return true;
    if (depth >= 256) return Fail("宏调用者模式超过语法深度预算");
    auto &node = g_nodes[id];
    if (node.kind == MNode::Var && node.s != "_")
    {
        node.bindingIdentity = 0x80000000u | (static_cast<std::uint32_t>(id) + 1u);
        bindings[node.s] = node.bindingIdentity;
    }
    else if (node.kind == MNode::List || node.kind == MNode::Tuple)
        for (const int child : node.kids)
            if (!BindMacroCallerPattern(child, bindings, depth + 1)) return false;
    return true;
}

/**
 * @brief 在展开前记录调用者的词法声明身份，防止导出名字改变既有实参绑定。
 * @param id 当前源码节点。
 * @param bindings 当前作用域内可见名字到声明身份的映射。
 * @param depth 当前递归深度。
 * @param argumentSyntax 是否位于需要保留原绑定的调用实参内部。
 * @return 绑定成功时返回真。
 */
bool BindMacroCallerSyntax(int id, Memory::Map<Memory::String, std::uint32_t> &bindings, std::size_t depth, bool argumentSyntax = false)
{
    if (id < 0) return true;
    if (depth >= 1024) return Fail("宏调用者绑定超过语法深度预算"), false;
    auto &node = g_nodes[id];
    if (node.kind == MNode::MacroDefinition) return true;
    if (node.kind == MNode::Var)
    {
        const auto found = bindings.find(node.s);
        if (argumentSyntax && found != bindings.end()) node.bindingIdentity = found->second;
        return true;
    }
    if (node.kind == MNode::MacroArgument)
    {
        for (const int child : node.kids)
        {
            auto local = bindings;
            if (!BindMacroCallerSyntax(child, local, depth + 1, true)) return false;
        }
        return true;
    }
    if (node.kind == MNode::Block)
    {
        auto local = bindings;
        return BindMacroCallerSyntax(static_cast<int>(node.i), local, depth + 1, argumentSyntax);
    }
    if (node.kind == MNode::Destructure && !node.i)
    {
        if (!BindMacroCallerSyntax(node.kids[1], bindings, depth + 1, argumentSyntax)) return false;
        return BindMacroCallerPattern(node.kids[0], bindings, depth + 1);
    }
    if (node.kind == MNode::Case)
    {
        auto branch = bindings;
        if (node.i >= 0)
        {
            const auto pattern = N(static_cast<int>(node.i));
            if (pattern.kind == MNode::EnumCtor || pattern.kind == MNode::TypeQualifiedRef)
            {
                for (const int payload : pattern.kids)
                    if (!BindMacroCallerPattern(payload, branch, depth + 1)) return false;
            }
            else if (pattern.kind == MNode::Call && pattern.kids.size() == 2)
            {
                if (!BindMacroCallerPattern(pattern.kids[1], branch, depth + 1)) return false;
            }
            else if (!BindMacroCallerSyntax(static_cast<int>(node.i), branch, depth + 1, argumentSyntax))
                return false;
        }
        if (!BindMacroCallerSyntax(node.guard, branch, depth + 1, argumentSyntax)) return false;
        for (const int child : node.kids)
            if (!BindMacroCallerSyntax(child, branch, depth + 1, argumentSyntax)) return false;
        return true;
    }
    const bool localScope = node.kind == MNode::FuncDef || node.kind == MNode::MethodDef || node.kind == MNode::For || node.kind == MNode::Foreach || node.kind == MNode::GenericLambda;
    auto local = localScope ? bindings : Memory::Map<Memory::String, std::uint32_t>{};
    auto &scope = localScope ? local : bindings;
    const bool declarationGroup = node.kind == MNode::Declare && node.kids.size() == 1 && node.kids[0] >= 0 && g_nodes[node.kids[0]].kind == MNode::List;
    if (node.kind == MNode::Foreach)
    {
        if (!BindMacroCallerSyntax(node.kids[0], scope, depth + 1, argumentSyntax)) return false;
        node.bindingIdentity = 0x80000000u | (static_cast<std::uint32_t>(id) + 1u);
        scope[node.s] = node.bindingIdentity;
        return BindMacroCallerSyntax(node.kids[1], scope, depth + 1, argumentSyntax);
    }
    for (const int child : node.kids)
        if (!BindMacroCallerSyntax(child, scope, depth + 1, argumentSyntax)) return false;
    for (const int annotation : node.annotations)
        if (!BindMacroCallerSyntax(annotation, scope, depth + 1, argumentSyntax)) return false;
    if (!BindMacroCallerSyntax(node.guard, scope, depth + 1, argumentSyntax)) return false;
    const bool indexed = node.kind == MNode::Case || node.kind == MNode::NewPtr || ((node.kind == MNode::Declare || node.kind == MNode::Field) && !declarationGroup && node.i >= 0) || (node.kind == MNode::Index && node.s2 == "__chali_bare_index");
    if (indexed && !BindMacroCallerSyntax(static_cast<int>(node.i), scope, depth + 1, argumentSyntax)) return false;
    if ((node.kind == MNode::Declare && !declarationGroup) || node.kind == MNode::Params)
    {
        node.bindingIdentity = 0x80000000u | (static_cast<std::uint32_t>(id) + 1u);
        scope[node.s] = node.bindingIdentity;
    }
    return true;
}

/**
 * @brief 用同一生成解析器完整检查一种实参语法，不将语法失败当最终语义诊断。
 * @param source 保留原文及起点的片段。
 * @param category 五类片段的编号。
 * @param module 调用或定义所在模块。
 * @return 语法成立时返回 AST 根编号，否则返回负一并回滚本次失败解析。
 */
int ParseMacroFragment(const chali_macro::SourceFragment &source, int category, const Memory::String &module)
{
    static const char *entries[] = { "@!macroparse_expr\n", "@!macroparse_ident\n", "@!macroparse_type\n", "@!macroparse_block\n", "@!macroparse_stmt\n" };
    const auto nodes = g_nodes.size();
    const auto types = g_types.size();
    const auto names = g_names.size();
    const auto diagnostics = g_parseDiagnostics.size();
    const auto captureSpecifications = chali_lambda::captureSpecifications;
    const auto previousModule = g_parseModule;
    const auto previousLine = g_macroFragmentFirstLine;
    const auto previousColumn = g_macroFragmentFirstColumn;
    const auto previousReductionLine = g_reductionLine;
    const auto previousReductionColumn = g_reductionColumn;
    const auto previousGlobalType = g_globalType;
    g_parseModule = module;
    Memory::String text(source.text.data(), source.text.size());
    std::string error;
    int result = -1;
    bool success = chali_macro::sourceFragments.capture(text, error, source.line, source.column, true);
    if (success)
    {
        preprocess::Result normalized;
        normalized.text = text;
        Memory::String problem;
        success = chali_interpolation::run(normalized, problem);
        text = normalized.text;
        chali_lambda::markClosureTypeArguments(text);
        chali_generic::markContainerTypeArguments(text, category == 2);
        chali_macro::markHeaderBoundaries(text);
        text = Memory::String(entries[category]) + text;
        g_macroFragmentFirstLine = source.line;
        g_macroFragmentFirstColumn = source.column;
        chali::Engine parser;
        success = success && parser.runText(text, &result) == chali::Status::Ok && g_parseDiagnostics.size() == diagnostics;
    }
    chali_macro::sourceFragments.finishParsing();
    g_parseModule = previousModule;
    g_macroFragmentFirstLine = previousLine;
    g_macroFragmentFirstColumn = previousColumn;
    g_reductionLine = previousReductionLine;
    g_reductionColumn = previousReductionColumn;
    g_globalType = previousGlobalType;
    g_parseDiagnostics.resize(diagnostics);
    if (!success)
    {
        g_nodes.resize(nodes);
        g_types.resize(types);
        g_names.resize(names);
        g_rawMacroNames.erase(g_rawMacroNames.lower_bound(static_cast<int>(names)), g_rawMacroNames.end());
        g_stringSources.erase(g_stringSources.lower_bound(static_cast<int>(names)), g_stringSources.end());
        chali_lambda::captureSpecifications = captureSpecifications;
        return -1;
    }
    return result;
}

/**
 * @brief 普通泛型实例带入原始片段的静态类型和值环境，原文仍保持定义时的字节。
 */
struct MacroFragmentEnvironment
{
    Memory::Map<Memory::String, std::shared_ptr<MType>> types;
    Memory::Map<Memory::String, int> values;
};

/**
 * @brief 将外层普通泛型的具体参数应用到片段 AST，不改写正则或字符串化所用原文。
 * @param root 新解析的候选根。
 * @param environment 外层单态化环境。
 * @return 类型和值替换成功时返回真。
 */
bool ApplyMacroFragmentEnvironment(int root, const MacroFragmentEnvironment &environment)
{
    if (root < 0 || (environment.types.empty() && environment.values.empty())) return true;
    Memory::Function<std::shared_ptr<MType>(const std::shared_ptr<MType> &, std::size_t)> bind;
    bind = [&](const std::shared_ptr<MType> &type, std::size_t depth) -> std::shared_ptr<MType> {
        if (!type || depth >= 256) return type;
        const auto replacement = environment.types.find(type->name);
        if (type->kind == MType::Struct && replacement != environment.types.end()) return replacement->second;
        auto result = Memory::makeShared<MType>(*type);
        if (type->kind == MType::Struct)
        {
            const auto split = type->name.find("::");
            if (split != Memory::String::npos && environment.types.count(type->name.substr(0, split)))
            {
                result = Memory::makeShared<MType>(*environment.types.at(type->name.substr(0, split)));
                result->genericMember = type->name.substr(split + 2);
            }
        }
        result->elem = bind(result->elem, depth + 1);
        result->elem2 = bind(result->elem2, depth + 1);
        for (auto &parameter : result->paramTypes) parameter = bind(parameter, depth + 1);
        for (auto &argument : result->genericArguments) argument = bind(argument, depth + 1);
        return result;
    };
    Memory::Set<int> seen;
    Memory::Function<bool(int, std::size_t)> visit;
    visit = [&](int id, std::size_t depth) {
        if (id < 0 || !seen.insert(id).second) return true;
        if (depth >= 256) return Fail("宏片段的外层泛型绑定超过语法深度预算");
        auto &node = g_nodes[id];
        if (node.kind == MNode::Var && environment.values.count(node.s))
        {
            const auto line = node.sourceLine, column = node.sourceColumn;
            chali_macro::AstSubstitution copy(g_nodes, 1000000, &g_types);
            std::string error;
            const int value = copy.instantiate(environment.values.at(node.s), {}, 0, error);
            if (value < 0) return Fail(Memory::String(error.data(), error.size()));
            node = N(value);
            node.sourceLine = line;
            node.sourceColumn = column;
        }
        if (node.type >= 0) node.type = M_AddType(bind(T(node.type), 0));
        if (node.kind == MNode::EnumCtor)
        {
            const auto split = node.s.find("::");
            const auto owner = split == Memory::String::npos ? node.s : node.s.substr(0, split);
            const auto type = environment.types.find(owner);
            if (type != environment.types.end())
            {
                if (split != Memory::String::npos) node.s2 = node.s.substr(split + 2) + "::" + node.s2;
                node.kind = MNode::TypeQualifiedRef;
                node.type = M_AddType(type->second);
                node.s.clear();
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
 * @brief 为原始调用构建五类实参候选，保持结构不成立与语义非法的区别。
 * @param id 当前可达 AST 节点。
 * @param seen 已检查的节点集合，防止重复处理共享子树。
 * @param depth 当前遍历深度。
 * @param outer 外层普通泛型实例的参数环境。
 * @return 所有原始调用均完成结构化时返回真。
 */
bool MaterializeMacroFragments(int id, Memory::Set<int> &seen, std::size_t depth, const MacroFragmentEnvironment *outer = nullptr)
{
    if (id < 0 || !seen.insert(id).second) return true;
    if (depth >= 256 || seen.size() > 1000000) return Fail("宏原始片段超过深度或规模预算"), false;
    auto &node = g_nodes[id];
    MacroFragmentEnvironment environment = outer ? *outer : MacroFragmentEnvironment{};
    if (node.kind == MNode::FuncDef || node.kind == MNode::StructDecl || node.kind == MNode::EnumDecl)
    {
        auto instance = chali_generic::session.made.find(node.s);
        if (instance == chali_generic::session.made.end()) instance = chali_generic::session.made.find(BareTypeName(node.s));
        if (instance != chali_generic::session.made.end() && instance->second.generic)
            for (std::size_t index = 0; index < instance->second.args.size() && index < instance->second.generic->params.size(); ++index)
            {
                const auto &parameter = instance->second.generic->params[index];
                const auto &argument = instance->second.args[index];
                const chali_macro::SourceFragment text{ std::string(argument.data(), argument.size()), 1, 1 };
                const int parsed = ParseMacroFragment(text, parameter.constant ? 0 : 2, node.module);
                if (parsed < 0) continue;
                if (parameter.constant)
                    environment.values[parameter.name] = parsed;
                else
                    environment.types[parameter.name] = T(N(parsed).type);
            }
    }
    if (node.kind == MNode::MacroCall && node.i > 0 && node.kids.empty())
    {
        g_macroSyntaxFiltering = true;
        const auto call = chali_macro::sourceFragments.at(static_cast<std::size_t>(node.i - 1));
        node.sourceLine = call.line;
        node.sourceColumn = call.column;
        const auto module = node.module;
        const int groups = M_New(MNode::MacroArguments);
        for (const auto &sourceGroup : call.groups)
        {
            const int group = M_New(MNode::MacroArgumentGroup);
            g_nodes[group].i = sourceGroup.opening;
            for (const auto &source : sourceGroup.arguments)
            {
                const int argument = M_New(MNode::MacroArgument, Memory::String(source.text.data(), source.text.size()));
                g_nodes[argument].module = module;
                g_nodes[argument].sourceLine = source.line;
                g_nodes[argument].sourceColumn = source.column;
                for (int category = 0; category < 5; ++category)
                {
                    const int candidate = ParseMacroFragment(source, category, module);
                    if (!ApplyMacroFragmentEnvironment(candidate, environment)) return false;
                    g_nodes[argument].kids.push_back(candidate);
                }
                g_nodes[group].kids.push_back(argument);
            }
            g_nodes[groups].kids.push_back(group);
        }
        node.kids = { call.groups.empty() ? -1 : groups };
        if (call.explicitTypes)
        {
            const int types = M_List();
            for (const auto &source : call.types)
            {
                const int type = ParseMacroFragment(source, 2, module);
                if (type < 0) return AddDiagnostic("宏显式泛型实参必须是完整类型", source.line, source.column), false;
                if (!ApplyMacroFragmentEnvironment(type, environment)) return false;
                M_ListAdd(types, type);
            }
            node.kids.push_back(types);
        }
        node.i = 0;
    }
    const auto original = node;
    for (const int child : original.kids)
        if (!MaterializeMacroFragments(child, seen, depth + 1, &environment)) return false;
    for (const int annotation : original.annotations)
        if (!MaterializeMacroFragments(annotation, seen, depth + 1, &environment)) return false;
    if (!MaterializeMacroFragments(original.guard, seen, depth + 1, &environment)) return false;
    const bool group = original.kind == MNode::Declare && original.kids.size() == 1 && original.kids[0] >= 0 && g_nodes[original.kids[0]].kind == MNode::List;
    const bool indexed = original.kind == MNode::Block || original.kind == MNode::Case || original.kind == MNode::NewPtr || ((original.kind == MNode::Declare || original.kind == MNode::Field) && !group && original.i >= 0) || (original.kind == MNode::Index && original.s2 == "__chali_bare_index");
    return !indexed || MaterializeMacroFragments(static_cast<int>(original.i), seen, depth + 1, &environment);
}

/**
 * @brief 仅将最终展开树中的节点和递归类型描述交给名称及关联类型检查。
 */
void RefreshMacroActiveSyntax()
{
    g_macroActiveNodes.clear();
    g_macroActiveTypes.clear();
    if (!g_macroSyntaxFiltering) return;
    Memory::Function<void(const std::shared_ptr<MType> &)> type;
    type = [&](const std::shared_ptr<MType> &value) {
        if (!value || !g_macroActiveTypes.insert(value.get()).second) return;
        type(value->elem);
        type(value->elem2);
        for (const auto &parameter : value->paramTypes) type(parameter);
        for (const auto &argument : value->genericArguments) type(argument);
    };
    Memory::Function<void(int)> visit;
    visit = [&](int id) {
        if (id < 0) return;
        const auto &node = N(id);
        if (node.kind == MNode::MacroDefinition || !g_macroActiveNodes.insert(&node).second) return;
        if (node.kind == MNode::MacroCall) return;
        if (node.type >= 0) type(T(node.type));
        if (node.kind == MNode::MacroArgument)
        {
            if (!node.kids.empty()) visit(node.kids[0]);
            return;
        }
        for (const int child : node.kids) visit(child);
        for (const int annotation : node.annotations) visit(annotation);
        visit(node.guard);
        const bool group = node.kind == MNode::Declare && node.kids.size() == 1 && node.kids[0] >= 0 && N(node.kids[0]).kind == MNode::List;
        const bool indexed = node.kind == MNode::Block || node.kind == MNode::Case || node.kind == MNode::NewPtr || ((node.kind == MNode::Declare || node.kind == MNode::Field) && !group && node.i >= 0) || (node.kind == MNode::Index && node.s2 == "__chali_bare_index");
        if (indexed) visit(static_cast<int>(node.i));
    };
    for (const int id : g_top) visit(id);
}

/**
 * @brief 只提升最终展开树中可达的闭包函数，隔离未调用模板及未选中候选。
 * @return 提升成功返回真，过深语法树记录错误后返回假。
 */
bool LiftReachableAstLambdas()
{
    Memory::Set<int> seen;
    Memory::Vector<int> functions;
    Memory::Function<bool(int, std::size_t)> visit;
    visit = [&](int id, std::size_t depth) {
        if (id < 0 || !seen.insert(id).second) return true;
        if (depth >= 1024) return Fail("闭包展开的语法树过深");
        auto &node = g_nodes[id];
        if (node.kind == MNode::MacroDefinition || node.kind == MNode::MacroCall || node.kind == MNode::MacroArgument) return true;
        if (node.kind == MNode::GenericLambda)
        {
            const int function = LiftAstGenericClosure(id);
            if (function < 0) return false;
            functions.push_back(function);
            return true;
        }
        if (node.kind == MNode::Lambda)
        {
            const int function = node.kids[0];
            const auto original = node.s;
            node.s = original + "_macro_" + Memory::toString(node.syntaxContext) + "_" + Memory::toString(id);
            g_nodes[function].s = node.s;
            if (node.kids.size() > 1) g_nodes[function].guard = node.kids[1];
            const auto captures = chali_lambda::captureSpecifications.find(original);
            if (captures != chali_lambda::captureSpecifications.end()) chali_lambda::captureSpecifications[node.s] = captures->second;
            functions.push_back(function);
            node.kind = MNode::Var;
            node.kids.clear();
            node.bindingIdentity = 0;
            return visit(function, depth + 1);
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
    const auto roots = g_top;
    for (const int id : roots)
        if (!visit(id, 0)) return false;
    g_top.insert(g_top.end(), functions.begin(), functions.end());
    return true;
}

/**
 * @brief 发现延迟宏生成的闭包并使编译入口以完整声明集合重做语义检查。
 * @return 没有新增声明且提升成功时返回真。
 */
bool PrepareExpandedMacroLambdas()
{
    const auto count = g_top.size();
    if (!LiftReachableAstLambdas()) return false;
    if (g_top.size() != count) throw MMacroSemanticReplay{};
    return true;
}

/**
 * @brief 临时加入唯一被选择的实参入口，提升其闭包后撤销入口但保留函数声明。
 * @param root 正在推导静态类型的实参表达式。
 * @return 无需新增函数声明时返回真；有新声明则请求完整语义重放。
 */
bool PrepareMacroArgumentLambdas(int root)
{
    const auto count = g_top.size();
    g_top.push_back(root);
    const bool success = LiftReachableAstLambdas();
    g_top.erase(g_top.begin() + static_cast<std::ptrdiff_t>(count));
    if (!success) return false;
    if (g_top.size() != count) throw MMacroSemanticReplay{};
    return true;
}
