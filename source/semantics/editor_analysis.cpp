/**
 * @brief 将已确定的静态类型显示为源码函数类型，避免暴露闭包环境或卫生绑定键。
 * @param type 完整静态类型。
 * @param depth 当前递归深度。
 * @return 用户可读的类型；未确定的泛型闭包返回空串，由编辑器保留符号签名。
 */
Memory::String EditorTypeText(const std::shared_ptr<MType> &type, std::size_t depth = 0)
{
    if (!type || depth >= 128) return {};
    if (type->kind == MType::Func)
    {
        if (!type->name.empty())
        {
            const auto generic = g_astGenericClosures.find(type->name);
            if (generic == g_astGenericClosures.end()) return {};
            const auto syntax = N(generic->second.syntax);
            int body = syntax.kids[2];
            if (N(body).kind == MNode::Block)
            {
                const auto statements = N(static_cast<int>(N(body).i)).kids;
                if (statements.size() != 1) return {};
                body = statements[0];
            }
            if (N(body).kind == MNode::Return && N(body).kids.size() == 1) body = N(body).kids[0];
            auto result = syntax.type >= 0 ? T(syntax.type) : std::shared_ptr<MType>{};
            if (!result && N(body).kind == MNode::Var)
                for (const int parameter : N(syntax.kids[1]).kids)
                    if (N(parameter).s == N(body).s && (!N(body).bindingIdentity || N(parameter).bindingIdentity == N(body).bindingIdentity)) result = T(N(parameter).type);
            if (!result && N(body).kind == MNode::Cast && N(body).type >= 0) result = T(N(body).type);
            if (!result) return {};
            auto text = EditorTypeText(result, depth + 1) + " fn<";
            bool first = true;
            for (const int parameter : N(syntax.kids[0]).kids)
            {
                if (!first) text += ", ";
                first = false;
                text += N(parameter).s;
            }
            text += ">(";
            first = true;
            for (const int parameter : N(syntax.kids[1]).kids)
            {
                if (!first) text += ", ";
                first = false;
                text += EditorTypeText(T(N(parameter).type), depth + 1);
            }
            return text + ")";
        }
        auto resultType = EditorTypeText(type->elem, depth + 1);
        if (type->elem && type->elem->kind == MType::Func) resultType = "(" + resultType + ")";
        auto text = resultType + " fn(";
        for (std::size_t index = 0; index < type->paramTypes.size(); ++index)
        {
            if (index) text += ", ";
            if (type->variadic && index + 1 == type->paramTypes.size()) text += "...";
            text += EditorTypeText(type->paramTypes[index], depth + 1);
        }
        return text + ")";
    }
    if (type->kind == MType::Ptr) return EditorTypeText(type->elem, depth + 1) + (type->readOnly ? " const&" : " mut&");
    if (type->kind == MType::Array)
    {
        auto text = EditorTypeText(type->elem, depth + 1);
        if (type->dims.empty()) return text + "[]";
        for (const auto dimension : type->dims) text += "[" + Memory::toString(dimension) + "]";
        return text;
    }
    if (type->kind == MType::Struct)
    {
        const auto instance = chali_generic::session.made.find(BareTypeName(type->name));
        if (instance != chali_generic::session.made.end())
        {
            auto text = instance->second.generic->name + "<";
            for (std::size_t index = 0; index < instance->second.args.size(); ++index)
            {
                if (index) text += ", ";
                text += instance->second.args[index];
            }
            return text + ">";
        }
    }
    return Describe(type);
}

/**
 * @brief 收集 auto 原始声明的最终静态类型，供编辑器显示宏与高阶闭包的真实结果。
 * @param id 原始变量声明节点。
 * @param type 完整推导得到的类型。
 */
void RecordEditorTypeHint(int id, const std::shared_ptr<MType> &type)
{
    if (!chali_editor::compilerAnalysis.enabled()) return;
    const auto &node = N(id);
    if (node.syntaxContext || node.s.empty() || node.s.rfind("__chali_", 0) == 0) return;
    const auto text = EditorTypeText(type);
    if (text.empty()) return;
    const auto file = node.sourceLine && node.sourceLine <= g_sourceFiles.size() ? g_sourceFiles[node.sourceLine - 1] : g_sourceFallback;
    const auto line = node.sourceLine && node.sourceLine <= g_sourceLines.size() && g_sourceLines[node.sourceLine - 1] ? g_sourceLines[node.sourceLine - 1] : node.sourceLine;
    chali_editor::compilerAnalysis.addType({ std::string(file.data(), file.size()), std::string(node.s.data(), node.s.size()), std::string(text.data(), text.size()), line, node.sourceColumn });
}

/**
 * @brief 将宏生成的节点显示为有界的结构树，不重新打印或重新解析可执行源码。
 * @param id 节点编号。
 * @param depth 当前深度。
 * @param budget 剩余节点预算。
 * @return 合法 JSON 结构树。
 */
std::string EditorMacroTree(int id, std::size_t depth, std::size_t &budget)
{
    if (id < 0) return "null";
    if (!budget || depth >= 128) return "{\"kind\":\"truncated\"}";
    --budget;
    const auto &node = N(id);
    const char *kind = "node";
    switch (node.kind)
    {
    case MNode::MacroTemplate: kind = "template"; break;
    case MNode::MacroCall: kind = "macro_call"; break;
    case MNode::MacroArgument: kind = "source_fragment"; break;
    case MNode::List: kind = "sequence"; break;
    case MNode::Var: kind = "identifier"; break;
    case MNode::Int: kind = "integer"; break;
    case MNode::Str: kind = "string"; break;
    case MNode::Bin: kind = "binary"; break;
    case MNode::Un: kind = "unary"; break;
    case MNode::Call: kind = "call"; break;
    case MNode::Declare: kind = "declaration"; break;
    case MNode::ExprStmt: kind = "expression_statement"; break;
    case MNode::Block: kind = "block"; break;
    case MNode::Loop: kind = "value_block"; break;
    case MNode::Return: kind = "return"; break;
    case MNode::Lambda: kind = "closure"; break;
    case MNode::FuncDef: kind = "function"; break;
    case MNode::Params: kind = "parameter"; break;
    case MNode::Cast: kind = "cast"; break;
    case MNode::Member:
    case MNode::Arrow: kind = "member"; break;
    case MNode::EnumCtor:
    case MNode::TypeQualifiedRef: kind = "qualified_member"; break;
    default: break;
    }
    const auto name = (node.kind == MNode::Lambda || node.kind == MNode::FuncDef) && node.s.rfind("__chali_lambda_", 0) == 0 ? Memory::String("fn") : node.s;
    auto result = std::string("{\"kind\":") + chali_editor::jsonString(kind) + ",\"name\":" + chali_editor::jsonString(std::string_view(name.data(), name.size()));
    result += ",\"origin\":" + chali_editor::jsonString(node.syntaxContext ? "definition" : "argument");
    if (!node.s2.empty()) result += ",\"member\":" + chali_editor::jsonString(std::string_view(node.s2.data(), node.s2.size()));
    if (node.kind == MNode::Int) result += ",\"value\":" + std::to_string(node.i);
    if (node.type >= 0)
    {
        const auto type = EditorTypeText(T(node.type));
        result += ",\"type\":" + chali_editor::jsonString(std::string_view(type.data(), type.size()));
    }
    result += ",\"children\":[";
    bool first = true;
    if (node.kind != MNode::MacroArgument)
    {
        auto children = node.kids;
        if (node.kind == MNode::Block) children.push_back(static_cast<int>(node.i));
        for (const int child : children)
        {
            if (!first) result += ',';
            first = false;
            result += EditorMacroTree(child, depth + 1, budget);
        }
    }
    return result + "]}";
}

/**
 * @brief 记录实际执行过的卫生模板实例化及其调用来源。
 * @param call 尚未被替换的调用节点。
 * @param root 实例化的模板根节点。
 */
void RecordEditorMacro(int call, int root)
{
    if (!chali_editor::compilerAnalysis.enabled()) return;
    const auto &node = N(call);
    const auto sourceLine = node.expansionLine ? node.expansionLine : node.sourceLine;
    const auto column = node.expansionLine ? node.expansionColumn : node.sourceColumn;
    const auto file = sourceLine && sourceLine <= g_sourceFiles.size() ? g_sourceFiles[sourceLine - 1] : g_sourceFallback;
    const auto line = sourceLine && sourceLine <= g_sourceLines.size() && g_sourceLines[sourceLine - 1] ? g_sourceLines[sourceLine - 1] : sourceLine;
    std::size_t budget = 10000;
    chali_editor::compilerAnalysis.addMacro({ std::string(file.data(), file.size()), std::string(node.s.data(), node.s.size()), EditorMacroTree(root, 0, budget), line, column });
}
