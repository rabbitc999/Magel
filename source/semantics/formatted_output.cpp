std::shared_ptr<MType> ExprType(int id);

/**
 * @brief 将标准错误流与文件打印的格式参数归一为普通字符串调用。
 * @param id 待推导或降级的表达式节点编号。
 * @note 仅识别标准库入口，保留用户同名函数和方法；格式化结果沿用普通临时值清理。
 */
void NormalizeFormattedOutputCall(int id)
{
    if (id < 0) return;
    const MNode call = g_nodes[id];
    if (call.kind != MNode::Call || call.kids.size() < 2u) return;
    const MNode callee = g_nodes[call.kids[0]];
    const Memory::String standard = StandardCallName(callee);
    const bool errorStream = standard == "eprint" || standard == "eprintln";
    bool fileOutput = false;
    std::size_t first = 0u;
    if (callee.kind == MNode::Member || callee.kind == MNode::Arrow)
    {
        if ((callee.s == "print" || callee.s == "println") && !callee.kids.empty())
        {
            const auto receiver = ExprType(callee.kids[0]);
            fileOutput = IsStandardOpcode(receiver, "File");
            if (!fileOutput && receiver && receiver->kind == MType::Struct && BareTypeName(receiver->name) == "FileStream")
                if (const auto *owner = FindStruct(receiver->name))
                    for (const auto &method : owner->methods)
                        if (method.module == "std::io" && method.source == "print") fileOutput = true;
        }
    }
    else
    {
        const Memory::String name = callee.kind == MNode::EnumCtor ? callee.s2 : callee.s;
        if (name == "filePrint" || name == "filePrintln")
        {
            const auto found = g_functionIndex.find("std::io::" + name);
            fileOutput = callee.kind == MNode::EnumCtor ? callee.s == "std::io" :
                                                          (callee.kind == MNode::Var && FindIn(callee.module, name) == nullptr && found != g_functionIndex.end() && FindFunctionOverloads(callee.module, name) == &found->second);
            first = 1u;
        }
    }
    if (!errorStream && !fileOutput) return;
    const auto arguments = g_nodes[call.kids[1]].kids;
    if (arguments.size() < first) return;
    int text = -1;
    if (arguments.size() == first)
        text = M_New(MNode::Str, "");
    else if (arguments.size() == first + 1u && g_nodes[arguments[first]].kind != MNode::Str && IsStandardOpcode(ExprType(arguments[first]), "String"))
    {
        if (!errorStream) return;
        text = arguments[first];
    }
    else
    {
        const int values = M_List();
        for (std::size_t i = first; i < arguments.size(); ++i) g_nodes[values].kids.push_back(arguments[i]);
        text = M_FString(values);
    }
    if (arguments.size() != first + 1u || text != arguments[first])
    {
        g_nodes[text].sourceLine = call.sourceLine;
        g_nodes[text].sourceColumn = call.sourceColumn;
        g_nodes[text].module = call.module;
    }
    const int values = M_List();
    for (std::size_t i = 0u; i < first; ++i) g_nodes[values].kids.push_back(arguments[i]);
    g_nodes[values].kids.push_back(text);
    g_nodes[id].kids[1] = values;
    if (errorStream)
    {
        const int target = M_EnumCtor(M_Name("std::lang"), M_Name(standard == "eprintln" ? "__std_eprintln" : "__std_eprint"), M_List());
        g_nodes[target].sourceLine = call.sourceLine;
        g_nodes[target].sourceColumn = call.sourceColumn;
        g_nodes[target].module = call.module;
        g_nodes[id].kids[0] = target;
    }
}
