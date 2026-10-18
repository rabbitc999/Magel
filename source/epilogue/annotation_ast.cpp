/**
 * @file annotation_ast.cpp
 * @brief 注解语义动作；由权威文法调用，不驱动解析器。
 */
/**
 * @brief 创建注解声明并保留起始位置。
 * @param marker 起始 token。
 * @param name 名称。
 * @param params 参数表。
 * @return 声明节点。
 */
int M_AnnotationDecl(int marker, int name, int params)
{
    const int id = M_New(MNode::AnnotationDecl, g_names[name]);
    auto &node = g_nodes[id];
    node.kids = { params };
    node.sourceLine = g_nodes[marker].sourceLine;
    node.sourceColumn = g_nodes[marker].sourceColumn;
    return id;
}
/**
 * @brief 创建注解参数。
 * @param name 名称。
 * @param type 类型。
 * @param value 默认值或 -1。
 * @return 参数节点。
 */
int M_AnnotationParam(int name, int type, int value)
{
    const int id = M_New(MNode::AnnotationParam, g_names[name]);
    g_nodes[id].type = type;
    if (value >= 0) g_nodes[id].kids = { value };
    return id;
}
/**
 * @brief 创建注解使用。
 * @param marker 起始 token。
 * @param name 名称。
 * @param args 实参表。
 * @return 使用节点。
 */
int M_AnnotationUse(int marker, int name, int args)
{
    const int id = M_AnnotationDecl(marker, name, args);
    g_nodes[id].kind = MNode::AnnotationUse;
    return id;
}
/**
 * @brief 创建注解实参。
 * @param name 命名名称或 -1。
 * @param value 表达式。
 * @return 实参节点。
 */
int M_AnnotationArg(int name, int value)
{
    const int id = M_New(MNode::AnnotationArg, name < 0 ? Memory::String() : g_names[name]);
    g_nodes[id].kids = { value };
    return id;
}
/**
 * @brief 将注解挂载到声明。
 * @param annotations 注解表。
 * @param declaration 目标节点。
 * @return 目标节点。
 */
int M_Annotated(int annotations, int declaration)
{
    g_nodes[declaration].annotations = g_nodes[annotations].kids;
    return declaration;
}

/**
 * @brief 读取编译器语法节点。
 * @param id 节点下标。
 * @return 只读节点，越界抛出异常。
 */
const MNode &M_ParsedNode(int id) { return g_nodes.at(static_cast<std::size_t>(id)); }
