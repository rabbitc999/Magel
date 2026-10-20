/**
 * @brief 登记类型的字段派生请求，禁止触碰自动析构协议。
 * @param traits 请求的接口名称列表。
 * @param declaration 结构体或枚举声明。
 * @retval 已附着请求的声明；不支持或重复的接口返回诊断节点。
 */
int M_Derived(int traits, int declaration)
{
    Memory::Set<Memory::String> seen;
    for (const auto name : g_nodes[traits].kids)
    {
        const auto trait = g_names[name];
        if (trait == "Drop" || trait == "drop" || trait == "Closeable")
            return M_New(MNode::ParseBad, "#derive 禁止生成 Drop 或 Closeable：值语义已经自动递归清理字段");
        if (trait != "Equal" && trait != "Hash" && trait != "Clone")
            return M_New(MNode::ParseBad, "#derive 只支持 Equal、Hash、Clone，不能生成 " + trait);
        if (!seen.insert(trait).second) return M_New(MNode::ParseBad, "#derive 重复请求接口：" + trait);
        g_nodes[declaration].derives.push_back(trait);
    }
    return declaration;
}
