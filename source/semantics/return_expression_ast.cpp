/**
 * @brief 查找返回控制表达式中越过当前函数返回边界的非法出口。
 * @param id 当前语法节点。
 * @param visited 已检查节点，避免共享子树重复遍历。
 * @retval 非法出口的诊断；合法时为空串。
 */
Memory::String M_ReturnControlProblem(int id, Memory::Set<int> &visited)
{
    if (id < 0 || !visited.insert(id).second) return {};
    const auto node = g_nodes[id];
    if (node.kind == MNode::Lambda || node.kind == MNode::FuncDef) return {};
    if (node.kind == MNode::Return) return "return 表达式内部禁止嵌套 return；请使用 yield 值;";
    if (node.kind == MNode::Break && node.i != 0) return "return 表达式内部禁止 break #level 跳转";
    if (node.kind == MNode::Try) return "return 表达式内部禁止使用会提前返回的 ?；请显式匹配并使用 yield 值;";
    if (node.kind == MNode::Block)
    {
        const auto problem = M_ReturnControlProblem(static_cast<int>(node.i), visited);
        if (!problem.empty()) return problem;
    }
    for (const auto child : node.kids)
    {
        const auto problem = M_ReturnControlProblem(child, visited);
        if (!problem.empty()) return problem;
    }
    return {};
}
/**
 * @brief 将返回控制表达式转换为拥有独立结果槽的局部声明与返回。
 * @param value 返回表达式。
 * @retval 脱糖后的块；非法控制出口返回携带诊断的节点。
 */
int M_ReturnValue(int value)
{
    if (value < 0 || g_nodes[value].kind != MNode::Loop) return M_Return(value);
    M_CheckUpdates(value);
    Memory::Set<int> visited;
    const auto problem = M_ReturnControlProblem(value, visited);
    if (!problem.empty())
    {
        PushParseDiagnostic(problem);
        return M_New(MNode::ParseBad, problem);
    }
    g_nodes[value].returnExpression = true;
    const auto hidden = "__magel_return_expr$" + Memory::toString(g_nodes.size());
    const int name = M_Name(hidden.c_str());
    const int declaration = M_Decl(name, -1, value);
    g_nodes[declaration].type = -1;
    g_nodes[declaration].type2 = 0;
    const int group = M_LocalDecl(-1, M_List1(declaration), 0);
    g_nodes[group].returnExpression = true;
    return M_Block(M_List2(group, M_Return(M_Var(name))));
}
