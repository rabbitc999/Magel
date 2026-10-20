/**
 * @brief 在类型注册前展开已解析的表达式宏调用，保留实参结构和求值次数。
 * @param id 当前节点编号，负数表示省略。
 * @param registry 已登记当前编译宏定义的注册表。
 * @param depth 当前嵌套深度，用于限制递归宏。
 * @param budget 本次编译剩余的节点遍历预算。
 * @return 展开成功返回真，失败记录编译诊断。
 */
bool ExpandAstMacroNode(int id, magel_macro::Registry &registry, std::size_t depth, std::size_t &budget)
{
    if (id < 0) return true;
    if (depth >= 256 || budget == 0) return Fail("宏展开超过深度或规模预算"), false;
    --budget;
    auto &node = g_nodes[id];
    if (node.kind == MNode::MacroDefinition) return true;
    if (node.kind == MNode::MacroCall)
    {
        for (const int child : node.kids)
            if (!ExpandAstMacroNode(child, registry, depth + 1, budget)) return false;
        std::string error;
        const int expanded = registry.instantiate(id, error);
        if (expanded < 0) return AddDiagnostic(Memory::String(error.data(), error.size()), node.sourceLine, node.sourceColumn), false;
        const auto body = g_nodes[expanded];
        if (body.kind != MNode::MacroTemplate || body.kids.size() != 2 || !g_nodes[body.kids[0]].kids.empty() || body.kids[1] < 0)
            return AddDiagnostic("宏语句及编译期分支尚未接入语义展开", node.sourceLine, node.sourceColumn), false;
        const int expression = body.kids[1];
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
    const bool indexedChild = node.kind == MNode::Block || node.kind == MNode::Case || node.kind == MNode::NewPtr || ((node.kind == MNode::Declare || node.kind == MNode::Field) && !declarationGroup && node.i >= 0) || (node.kind == MNode::Index && node.s2 == "__magel_bare_index");
    return !indexedChild || ExpandAstMacroNode(static_cast<int>(node.i), registry, depth + 1, budget);
}

/**
 * @brief 注册当前程序的 AST 宏并在四元式生成前展开表达式模板。
 * @return 登记与展开全部成功时返回真。
 * @note 旧预处理路径仍待迁移；未实现的模板形式明确诊断，不作为已完成能力。
 */
bool ExpandAstMacros()
{
    magel_macro::HygieneResolver resolver;
    magel_macro::Registry registry(g_nodes, resolver);
    bool hasDefinitions = false;
    for (const int id : g_top)
        if (g_nodes[id].kind == MNode::MacroDefinition)
        {
            hasDefinitions = true;
            std::string error;
            if (!registry.add(id, 0, error))
                return AddDiagnostic(Memory::String(error.data(), error.size()), g_nodes[id].sourceLine, g_nodes[id].sourceColumn), false;
        }
    if (!hasDefinitions && std::none_of(g_nodes.begin(), g_nodes.end(), [](const MNode &node) { return node.kind == MNode::MacroCall; })) return true;
    std::size_t budget = 1000000;
    for (const int id : g_top)
        if (!ExpandAstMacroNode(id, registry, 0, budget)) return false;
    g_top.erase(std::remove_if(g_top.begin(), g_top.end(), [](int id) { return g_nodes[id].kind == MNode::MacroDefinition; }), g_top.end());
    return true;
}
