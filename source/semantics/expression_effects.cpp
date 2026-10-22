/* 完整表达式的对象副作用检查：类型绑定完成后记录冲突，不生成运行时代码。 */
/**
 * @brief 对象路径，动态下标保守视为可能重叠。
 */
struct MEffectPlace
{
    Memory::String root, label;
    Memory::Vector<Memory::String> parts;
};
/**
 * @brief 一次读取或写入；自增减同时带有更新标记。
 */
struct MEffectAccess
{
    MEffectPlace place;
    int node = -1;
    bool update = false, write = false;
};
Memory::Set<Memory::String> g_effectConflicts;
/**
 * @brief 引用别名的完整目标路径，保留字段与常量下标身份。
 */
Memory::Map<Memory::String, MEffectPlace> g_effectAliases;
/**
 * @brief 解析左值的存储身份，直接引用别名归并到来源。
 * @param id 左值节点。
 * @return 对象路径。
 */
MEffectPlace EffectPlace(int id)
{
    if (id < 0) return {};
    const auto node = N(id);
    if (node.kind == MNode::Var)
    {
        const auto variable = FindNodeVar(node);
        if (variable)
        {
            const auto alias = g_effectAliases.find(variable->emitted);
            if (alias != g_effectAliases.end() && !alias->second.root.empty()) return alias->second;
        }
        if (variable) return { variable->borrowOwner.empty() ? variable->emitted : variable->borrowOwner, node.s, {} };
        if (!g_owner.empty() && (node.s == "self" || FieldType(g_owner, node.s)))
            return { "self", node.s, node.s == "self" ? Memory::Vector<Memory::String>{} : Memory::Vector<Memory::String>{ "." + node.s } };
        return { ModuleSymbol(node.module, node.s), node.s, {} };
    }
    if ((node.kind == MNode::Member || node.kind == MNode::Arrow || node.kind == MNode::Index || (node.kind == MNode::Un && (node.s == "*" || node.s == "&"))) && !node.kids.empty())
    {
        auto place = EffectPlace(node.kids[0]);
        if (node.kind == MNode::Member || node.kind == MNode::Arrow)
        {
            place.parts.push_back("." + node.s);
            place.label += "." + node.s;
        }
        else if (node.kind == MNode::Index)
        {
            const auto index = N(node.kids[1]);
            const auto key = index.kind == MNode::Int ? Memory::toString(index.i) : Memory::String("*");
            place.parts.push_back("[" + key + "]");
            place.label += "[" + key + "]";
        }
        return place;
    }
    return {};
}
/**
 * @brief 判断两条对象路径是否可能指向重叠存储。
 * @param a 路径。
 * @param b 路径。
 * @return 可能重叠时为 true。
 */
bool OverlappingEffects(const MEffectPlace &a, const MEffectPlace &b)
{
    if (a.root.empty() || a.root != b.root) return false;
    for (std::size_t i = 0; i < std::min(a.parts.size(), b.parts.size()); ++i)
        if (a.parts[i] != b.parts[i] && a.parts[i] != "[*]" && b.parts[i] != "[*]") return false;
    return true;
}
/**
 * @brief 收集完整表达式中的更新、赋值与读取，按源码节点去重共享目标。
 * @param id 表达式。
 * @return 无；追加诊断。
 */
void RecordExpressionEffects(int id)
{
    if (id < 0 || (!N(id).checkedExpression && !(N(id).kind == MNode::Destructure && N(id).i))) return;
    Memory::Vector<MEffectAccess> accesses;
    Memory::Set<int> visited;
    std::function<void(int)> visit, address;
    address = [&](int target) {
        const auto node = N(target);
        if (node.kind == MNode::Index)
        {
            address(node.kids[0]);
            visit(node.kids[1]);
        }
        else if ((node.kind == MNode::Member || node.kind == MNode::Arrow || node.kind == MNode::Un) && !node.kids.empty())
            address(node.kids[0]);
    };
    visit = [&](int current) {
        if (current < 0 || !visited.insert(current).second) return;
        const auto node = N(current);
        if (node.kind == MNode::Block || node.kind == MNode::FuncDef) return;
        if (node.kind == MNode::Update)
        {
            accesses.push_back({ EffectPlace(node.kids[0]), current, true, true });
            address(node.kids[0]);
            return;
        }
        if (node.kind == MNode::Assign)
        {
            const auto &value = N(node.kids[1]);
            const bool compoundRight = value.kind == MNode::Assign && value.type2 == 1;
            const bool compoundChain = node.type2 == 1 && value.kind == MNode::Bin && value.kids.size() == 2 && N(value.kids[1]).kind == MNode::Assign;
            if (compoundRight || compoundChain)
            {
                ReductionSourceScope source(current);
                Fail("复合赋值不能参与链式赋值");
                return;
            }
            accesses.push_back({ EffectPlace(node.kids[0]), current, false, true });
            address(node.kids[0]);
            visit(node.kids[1]);
            return;
        }
        if (node.kind == MNode::Destructure && node.i)
        {
            std::function<void(int)> targets = [&](int target) {
                const auto pattern = N(target);
                if (pattern.kind == MNode::List || pattern.kind == MNode::Tuple)
                    for (int child : pattern.kids) targets(child);
                else
                {
                    accesses.push_back({ EffectPlace(target), target, false, true });
                    address(target);
                }
            };
            targets(node.kids[0]);
            visit(node.kids[1]);
            return;
        }
        if (node.kind == MNode::Var || node.kind == MNode::Member || node.kind == MNode::Arrow || node.kind == MNode::Index || (node.kind == MNode::Un && node.s == "*"))
        {
            accesses.push_back({ EffectPlace(current), current, false, false });
            address(current);
            return;
        }
        for (int child : node.kids) visit(child);
    };
    visit(id);
    for (std::size_t a = 0; a < accesses.size(); ++a)
        for (std::size_t b = a + 1; b < accesses.size(); ++b)
        {
            const auto &left = accesses[a], &right = accesses[b];
            if (!(left.update || right.update) || !OverlappingEffects(left.place, right.place)) continue;
            const int first = std::min(left.node, right.node), second = std::max(left.node, right.node);
            const auto key = Memory::toString(first) + ":" + Memory::toString(second);
            if (g_effectConflicts.insert(key).second)
                AddDiagnostic("[CHL1301] 同一表达式中，自增减目标与其他读写冲突：" + left.place.label,
                              N(left.update ? left.node : right.node).sourceLine);
        }
}
