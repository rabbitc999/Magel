/**
 * @brief 收集泛型闭包的自由变量语法，排除其形参和局部声明。
 * @param root 泛型闭包节点。
 * @return 保留卫生身份及来源的自由引用节点。
 */
Memory::Vector<int> AstGenericClosureReferences(int root)
{
    Memory::Set<std::uint32_t> declarations;
    Memory::Set<Memory::String> sourceDeclarations;
    Memory::Vector<int> references;
    Memory::Function<void(int, bool, std::size_t)> visit;
    visit = [&](int id, bool collect, std::size_t depth) {
        if (id < 0 || depth >= 256) return;
        const auto &node = N(id);
        if (!collect && (node.kind == MNode::Params || node.kind == MNode::Declare || node.kind == MNode::Foreach))
        {
            if (node.bindingIdentity)
                declarations.insert(node.bindingIdentity);
            else if (!node.s.empty())
                sourceDeclarations.insert(node.s);
        }
        if (!collect)
            VisitAstLocalPatternBindings(node, [&](int binding) {
                if (N(binding).bindingIdentity)
                    declarations.insert(N(binding).bindingIdentity);
                else
                    sourceDeclarations.insert(N(binding).s);
            });
        if (collect && node.kind == MNode::Var && !declarations.count(node.bindingIdentity) && (node.bindingIdentity || !sourceDeclarations.count(node.s))) references.push_back(id);
        for (const int child : node.kids) visit(child, collect, depth + 1);
        visit(node.guard, collect, depth + 1);
        if (node.kind == MNode::Block || node.kind == MNode::Case) visit(static_cast<int>(node.i), collect, depth + 1);
    };
    visit(root, false, 0);
    visit(root, true, 0);
    return references;
}

/**
 * @brief 提升可达泛型闭包，环境入口与模板体保持独立，模板体不转回源码。
 * @param id 待提升的语法节点。
 * @return 环境入口函数编号，失败返回负一。
 */
int LiftAstGenericClosure(int id)
{
    ReductionSourceScope source(id);
    const auto original = N(id);
    if (original.guard >= 0)
    {
        Memory::Set<std::pair<std::uint32_t, Memory::String>> captured;
        for (const int item : N(original.guard).kids)
        {
            const auto &entry = N(item);
            if (entry.kids.size() != 1 || N(entry.kids[0]).kind != MNode::Var) continue;
            const auto &reference = N(entry.kids[0]);
            if (!captured.insert({ reference.bindingIdentity, reference.s }).second)
                return Fail("泛型闭包捕获列表重复声明变量：" + reference.s), -1;
            for (const int parameter : N(original.kids[1]).kids)
                if (N(parameter).s == reference.s && N(parameter).bindingIdentity == reference.bindingIdentity)
                    return Fail("泛型闭包捕获变量与形参重名：" + reference.s), -1;
        }
    }
    const auto seed = Memory::String("__chali_lambda_ast_generic_") + Memory::toString(id);
    const int syntax = static_cast<int>(g_nodes.size());
    g_nodes.push_back(original);
    g_astGenericClosures[seed] = { syntax, AstGenericClosureReferences(syntax) };
    chali_lambda::GenericClosure definition;
    definition.seed = definition.sourceSeed = seed;
    definition.primary = seed + "_body";
    definition.module = original.module;
    definition.captureMode = static_cast<int>(original.i);
    for (const int generic : N(original.kids[0]).kids) definition.names.push_back(N(generic).s);
    chali_lambda::genericClosures[seed] = definition;
    const int body = M_Block(M_List1(M_Return(M_Int(0))));
    const int function = M_New(MNode::FuncDef, seed);
    g_nodes[function].type = M_TypeInt();
    g_nodes[function].kids = { M_List(), body };
    g_nodes[function].module = original.module;
    g_nodes[function].sourceLine = original.sourceLine;
    g_nodes[function].sourceColumn = original.sourceColumn;
    g_nodes[function].macroExpansion = original.macroExpansion;
    g_nodes[function].guard = original.guard;
    auto &node = g_nodes[id];
    node.kind = MNode::Var;
    node.s = seed;
    node.type = -1;
    node.type2 = 0;
    node.kids.clear();
    node.guard = -1;
    node.syntaxContext = node.bindingIdentity = 0;
    return function;
}

/**
 * @brief 将 AST 闭包类型参数转为既有统一器识别的模式类型。
 * @param type 参数类型树。
 * @param names 声明顺序的泛型参数名。
 * @param depth 当前类型递归深度。
 * @return 独立的模式类型树。
 */
std::shared_ptr<MType> AstGenericClosurePattern(const std::shared_ptr<MType> &type, const Memory::Vector<Memory::String> &names, std::size_t depth = 0)
{
    if (!type || depth >= 256) return type;
    for (std::size_t index = 0; index < names.size(); ++index)
        if (type->kind == MType::Struct && type->name == names[index]) return NamedType("McInferenceParameter" + Memory::toString(index));
    auto result = Memory::makeShared<MType>(*type);
    result->elem = AstGenericClosurePattern(type->elem, names, depth + 1);
    result->elem2 = AstGenericClosurePattern(type->elem2, names, depth + 1);
    for (auto &parameter : result->paramTypes) parameter = AstGenericClosurePattern(parameter, names, depth + 1);
    for (auto &argument : result->genericArguments) argument = AstGenericClosurePattern(argument, names, depth + 1);
    return result;
}

/**
 * @brief 从具体实参推导并克隆泛型闭包 AST；新增实现通过语义重放登记。
 * @param id 完整调用表达式。
 * @param seed 原闭包环境入口名称。
 * @return 属于该闭包时返回真，失败保留原语义诊断。
 */
bool ResolveAstGenericClosureCall(int id, const Memory::String &seed)
{
    const auto call = N(id);
    if (call.type >= 0 && !call.s.empty()) return true;
    const auto definition = g_astGenericClosures.at(seed);
    const auto syntax = N(definition.syntax);
    const auto &closure = chali_lambda::genericClosures.at(seed);
    const auto params = N(syntax.kids[1]).kids;
    const auto actuals = N(call.kids[1]).kids;
    if (params.size() != actuals.size()) return Fail("泛型闭包的实参数量不匹配"), true;
    const auto selection = N(call.kids[0]);
    const bool explicitTypes = selection.kind == MNode::GenericClosureSelect;
    Memory::Map<Memory::String, std::shared_ptr<MType>> bindings;
    if (explicitTypes)
    {
        const auto supplied = N(selection.kids[1]).kids;
        if (supplied.size() != closure.names.size()) return Fail("泛型闭包的显式类型实参数量不匹配"), true;
        for (std::size_t index = 0; index < supplied.size(); ++index)
            bindings["McInferenceParameter" + Memory::toString(index)] = T(N(supplied[index]).type);
    }
    for (std::size_t index = 0; index < params.size(); ++index)
    {
        const auto pattern = AstGenericClosurePattern(T(N(params[index]).type), closure.names);
        const auto bound = BoundCallbackType(pattern, bindings);
        if (ConcreteCallbackParameter(bound)) InferExpression(actuals[index], bound);
        const auto actual = pattern->kind == MType::Ptr ? RawExprType(actuals[index]) : ExprType(actuals[index]);
        if (!UnifyGenericType(pattern, actual, bindings)) return Fail("泛型闭包的类型推导冲突"), true;
    }
    MacroFragmentEnvironment environment;
    Memory::Vector<Memory::String> arguments;
    const auto generics = N(syntax.kids[0]).kids;
    for (std::size_t index = 0; index < closure.names.size(); ++index)
    {
        const auto key = "McInferenceParameter" + Memory::toString(index);
        if (!bindings.count(key) && N(generics[index]).type >= 0) bindings[key] = T(N(generics[index]).type);
        if (!bindings.count(key)) return Fail("无法推导泛型闭包的全部类型参数"), true;
        environment.types[closure.names[index]] = bindings.at(key);
        arguments.push_back(GenericArgumentText(bindings.at(key)));
        if (N(generics[index]).type2 >= 0)
        {
            const auto constraint = T(N(generics[index]).type2);
            if (!ImplementsInterface(bindings.at(key)->name, constraint->name)) return Fail("泛型闭包类型实参不满足接口约束：" + closure.names[index]), true;
        }
    }
    const auto name = chali_generic::mangle(seed + "_body", arguments);
    const bool registered = std::any_of(g_functionDecls.begin(), g_functionDecls.end(), [&](const MFunctionDecl &function) {
        return function.module == closure.module && function.source == name;
    });
    if (!registered)
    {
        chali_macro::AstSubstitution copy(g_nodes, 1000000, &g_types);
        std::string error;
        const int body = copy.instantiate(syntax.kids[2], {}, 0, error);
        const int parameters = copy.instantiate(syntax.kids[1], {}, 0, error);
        if (body < 0 || parameters < 0) return Fail(Memory::String(error.data(), error.size())), true;
        const int lambda = M_Lambda(syntax.type, parameters, body, static_cast<int>(syntax.i));
        if (N(lambda).kind != MNode::Lambda) return Fail("AST 泛型闭包实现无法建立"), true;
        const int function = N(lambda).kids[0];
        g_nodes[function].s = name;
        g_nodes[function].module = closure.module;
        g_nodes[function].sourceLine = syntax.sourceLine;
        g_nodes[function].sourceColumn = syntax.sourceColumn;
        g_nodes[function].macroExpansion = syntax.macroExpansion;
        if (!ApplyMacroFragmentEnvironment(function, environment)) return true;
        g_astGenericImplementations[name] = seed;
        g_top.push_back(function);
        if (!LiftReachableAstLambdas()) return true;
        throw MMacroSemanticReplay{};
    }
    if (!CheckLambdaCaptures(name, closure.module)) return true;
    const auto resolved = FindInferenceFunction(closure.module, name);
    g_nodes[id].s = name;
    g_nodes[id].s2 = seed;
    g_nodes[id].type = M_AddType(resolved->result);
    return true;
}
