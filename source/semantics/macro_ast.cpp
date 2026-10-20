/**
 * @brief 建立宏参数组，不将空参数组归一化为无参数组。
 * @param names 形参列表节点。
 * @return 参数组节点。
 */
int M_MacroParameters(int names)
{
    const int id = M_New(MNode::MacroParameters);
    g_nodes[id].kids = g_nodes[names].kids;
    return id;
}

/**
 * @brief 将宏形参名称保存为占位声明节点。
 * @param name 名称池编号。
 * @return 参数节点。
 */
int M_MacroParameter(int name)
{
    return M_New(MNode::MacroParameter, g_names[name]);
}

/**
 * @brief 保存宏体语句及可选表达式尾项。
 * @param statements 模板语句列表。
 * @param expression 末尾表达式，负值表示不存在。
 * @return 模板节点。
 */
int M_MacroTemplate(int statements, int expression)
{
    const int id = M_New(MNode::MacroTemplate);
    g_nodes[id].kids = { statements, expression };
    return id;
}

/**
 * @brief 保存语法分析后的宏定义，展开留到语义分析。
 * @param name 宏名称池编号。
 * @param parameters 参数组节点，负值表示无参数组。
 * @param body 已解析宏体。
 * @param generics 宏泛型参数节点列表。
 * @return 定义节点。
 */
int M_MacroDefinition(int name, int parameters, int body, int generics)
{
    const int id = M_New(MNode::MacroDefinition, g_names[name]);
    g_nodes[id].kids = { parameters, body, generics };
    return id;
}

/**
 * @brief 保存宏调用实参结构，禁止提前拼接实参文本。
 * @param name 含美元前缀的名称池编号。
 * @param arguments 实参列表，负值表示未提供参数组。
 * @return 调用节点。
 */
int M_MacroCall(int name, int arguments)
{
    const int id = M_New(MNode::MacroCall, g_names[name].substr(1));
    g_nodes[id].kids = { arguments };
    return id;
}

/**
 * @brief 保存泛型推导来源，空来源表示外部期望类型。
 * @param name 泛型参数名。
 * @param source 推导来源参数名，负值表示未指定。
 * @return 泛型参数节点。
 */
int M_MacroGeneric(int name, int source)
{
    const int id = M_New(MNode::MacroGeneric, g_names[name]);
    if (source >= 0) g_nodes[id].s2 = g_names[source];
    return id;
}

/**
 * @brief 为宏调用记录显式类型参数。
 * @param name 带美元前缀的宏名。
 * @param types 显式类型列表。
 * @param arguments 实参列表，负值表示无参数组。
 * @return 调用节点。
 */
int M_MacroTypedCall(int name, int types, int arguments)
{
    const int id = M_MacroCall(name, arguments);
    g_nodes[id].kids.push_back(types);
    return id;
}

/**
 * @brief 创建语法匹配的一个候选分支。
 * @param category 类别名称，负值表示兜底。
 * @param body 分支模板。
 * @return 分支节点。
 */
int M_MacroBranch(int category, int body)
{
    const int id = M_New(MNode::MacroBranch, category < 0 ? Memory::String() : g_names[category]);
    g_nodes[id].kids = { body };
    return id;
}

/**
 * @brief 将候选分支按源代码顺序加入列表。
 * @param branch 当前候选。
 * @param rest 后续候选列表。
 * @return 合并后的列表。
 */
int M_MacroBranches(int branch, int rest)
{
    const int id = M_List1(branch);
    g_nodes[id].kids.insert(g_nodes[id].kids.end(), g_nodes[rest].kids.begin(), g_nodes[rest].kids.end());
    return id;
}

/**
 * @brief 创建编译期语法匹配节点。
 * @param parameter 宏形参名称。
 * @param branches 已按顺序排列的分支。
 * @return 匹配节点。
 */
int M_MacroMatch(int parameter, int branches)
{
    const int id = M_New(MNode::MacroMatch, g_names[parameter]);
    g_nodes[id].kids = { branches };
    return id;
}

/**
 * @brief 保存正则分支语法，不在解析阶段执行或展开分支。
 * @param parameter 待匹配参数名。
 * @param pattern 正则字符串的名称池编号。
 * @param body 匹配成功时的模板。
 * @param otherwise 不匹配时的可选模板。
 * @return 正则分支节点。
 */
int M_MacroRegex(int parameter, int pattern, int body, int otherwise)
{
    const int id = M_New(MNode::MacroRegex, g_names[parameter]);
    g_nodes[id].s2 = g_names[pattern];
    g_nodes[id].kids = { body, otherwise };
    return id;
}
