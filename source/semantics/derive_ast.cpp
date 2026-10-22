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

/**
 * @brief 记录一条编译器指令（`#No<接口>Replace` / `#NoRight<接口>`）。
 * @param interface 指令词法文本在名字池里的下标。
 * @param declaration 目标结构体声明。
 * @return 带指令元数据的声明节点编号。
 * @note 词法正则已保证写法合法；接口名是否存在、是否真的参与自动推导、有没有重复写，
 *       都由登记阶段（`RegisterOperatorSwitches`）核对并报源码位置。
 */
int M_OperatorSwitch(int interface, int declaration)
{
    const Memory::String text = g_names[static_cast<std::size_t>(interface)];
    const std::size_t replaceSuffix = 7u;  /* "Replace" */
    if (text.size() > 3u + replaceSuffix && text.compare(text.size() - replaceSuffix, replaceSuffix, "Replace") == 0)
    {
        /* `#No` + 接口名 + `Replace` */
        g_nodes[static_cast<std::size_t>(declaration)].noReplace.push_back(
            text.substr(3u, text.size() - 3u - replaceSuffix));
        return declaration;
    }
    const std::size_t rightPrefix = 8u;  /* "#NoRight" */
    if (text.size() > rightPrefix && text.compare(0u, rightPrefix, "#NoRight") == 0)
    {
        /* `#NoRight` + 左接口名：只关右向（右操作数当接收者）那一条推导。 */
        g_nodes[static_cast<std::size_t>(declaration)].noRight.push_back(text.substr(rightPrefix));
    }
    return declaration;
}

/**
 * @brief 把一串编译器指令依次挂到同一个结构体声明上。
 * @param switches `NO_REPLACE` / `NO_RIGHT` 词法值组成的列表。
 * @param declaration 目标结构体声明。
 * @return 带全部指令元数据的声明节点编号。
 */
int M_OperatorSwitchList(int switches, int declaration)
{
    for (const int name : g_nodes[static_cast<std::size_t>(switches)].kids)
        declaration = M_OperatorSwitch(name, declaration);
    return declaration;
}
