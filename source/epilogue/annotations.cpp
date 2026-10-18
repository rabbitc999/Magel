/**
 * @file annotations.cpp
 * @brief 编译期注解符号绑定及精确类型校验；不生成运行时元数据。
 */
/**
 * @brief 本轮按声明顺序可见的注解定义；值为 AST 节点下标。
 */
Memory::Map<Memory::String, int> g_annotationDeclarations;

/**
 * @brief 在注解起始 token 报错。
 * @param node 注解位置。
 * @param message 消息。
 * @return false。
 */
bool AnnotationFail(const MNode &node, const Memory::String &message)
{
    g_reductionLine = node.sourceLine;
    if (g_error.empty()) g_errorColumn = node.sourceColumn;
    return Fail(message);
}
/**
 * @brief 归一声明所属命名空间。
 * @param node 声明。
 * @return 限定名称。
 */
Memory::String AnnotationName(const MNode &node)
{
    return node.s.find("::") != Memory::String::npos || node.module.empty() ? node.s : node.module + "::" + node.s;
}
/**
 * @brief 注解基础类型白名单，拒绝宿主别名、聚合、引用及 void。
 * @param type 类型。
 * @return 是否允许。
 */
bool AnnotationType(const std::shared_ptr<MType> &type)
{
    if (!type) return false;
    return type->kind == MType::Bool || type->kind == MType::Char || type->kind == MType::Int || type->kind == MType::Double || type->kind == MType::String;
}
/**
 * @brief 检查字面量常量并归一到精确声明类型；显式 cast 不允许隐式换型。
 * @param expression 表达式节点。
 * @param typeId 目标类型。
 * @param value 常量输出。
 * @param origin 用于诊断的注解起始位置。
 * @return 校验成功时返回 true。
 */
bool AnnotationConstant(int expression, int typeId, MAnnotationValue &value, const MNode &origin)
{
    const auto type = T(typeId);
    const auto &node = N(expression);
    value = MAnnotationValue{};
    value.type = typeId;
    if (node.kind == MNode::Cast)
    {
        if (!SameType(T(node.type), type))
            return AnnotationFail(origin, MAGEL_TEXT("注解参数类型不匹配：禁止隐式转换"));
        return AnnotationConstant(node.kids[0], typeId, value, origin);
    }
    if (node.kind == MNode::Bool && type->kind == MType::Bool)
    {
        value.integer = node.i;
        return true;
    }
    if (node.kind == MNode::Str && type->kind == MType::String)
    {
        g_reductionLine = origin.sourceLine;
        g_errorColumn = origin.sourceColumn;
        value.text = UnescapeText(node.s);
        if (g_error.empty()) g_errorColumn = 0;
        return g_error.empty();
    }
    if (node.kind == MNode::Char && type->kind == MType::Char)
    {
        g_reductionLine = origin.sourceLine;
        g_errorColumn = origin.sourceColumn;
        const auto text = UnescapeText(node.s.substr(1, node.s.size() - 2));
        if (!g_error.empty()) return false;
        g_errorColumn = 0;
        if (text.size() != 1) return AnnotationFail(origin, "注解 char 常量必须为一个字节");
        value.integer = static_cast<unsigned char>(text[0]);
        return true;
    }
    bool negative = false;
    const MNode *literal = &node;
    if (node.kind == MNode::Un && (node.s == "-" || node.s == "+"))
    {
        negative = node.s == "-";
        literal = &N(node.kids[0]);
    }
    if (literal->kind == MNode::Int && type->kind == MType::Int)
    {
        std::uint64_t magnitude = 0;
        if (literal->s.empty())
            magnitude = static_cast<std::uint64_t>(literal->i);
        else
        {
            auto raw = literal->s;
            if (literal->unsignedLiteral)
            {
                if (!type->isUnsigned) return AnnotationFail(origin, "u 后缀整数字面量只能用于无符号注解参数");
                raw.pop_back();
            }
            unsigned base = 10;
            std::size_t at = 0;
            if (raw.size() > 2 && raw[0] == '0' && (raw[1] == 'x' || raw[1] == 'X'))
            {
                base = 16;
                at = 2;
            }
            else if (raw[0] == 'b' || raw[0] == 'B')
            {
                base = 2;
                at = 1;
            }
            for (; at < raw.size(); ++at)
            {
                const unsigned char ch = raw[at];
                if (ch == '_') continue;
                const unsigned digit = ch >= '0' && ch <= '9' ? ch - '0' :
                                       ch >= 'a' && ch <= 'f' ? ch - 'a' + 10 :
                                       ch >= 'A' && ch <= 'F' ? ch - 'A' + 10 :
                                                                base;
                if (digit >= base || magnitude > (UINT64_MAX - digit) / base)
                    return AnnotationFail(origin, "注解整数常量超出声明类型范围");
                magnitude = magnitude * base + digit;
            }
        }
        const unsigned bits = static_cast<unsigned>(type->bits);
        const std::uint64_t limit = type->isUnsigned ? (bits == 64 ? UINT64_MAX : (std::uint64_t(1) << bits) - 1) : (std::uint64_t(1) << (bits - 1)) - (negative ? 0 : 1);
        if (magnitude > limit || (type->isUnsigned && negative && magnitude != 0))
            return AnnotationFail(origin, "注解整数常量超出声明类型范围");
        const std::uint64_t raw = negative ? std::uint64_t(0) - magnitude : magnitude;
        static_assert(sizeof(value.integer) == sizeof(raw), "注解整数槽为 64 位");
        std::memcpy(&value.integer, &raw, sizeof(raw));
        return true;
    }
    if (literal->kind == MNode::Float && type->kind == MType::Double)
    {
        value.floating = negative ? -literal->d : literal->d;
        if (!std::isfinite(value.floating) || (type->bits == 32 && std::abs(value.floating) > std::numeric_limits<float>::max()))
            return AnnotationFail(origin, "注解浮点常量超出声明类型范围");
        if (type->bits == 32) value.floating = static_cast<float>(value.floating);
        return true;
    }
    if (literal->kind == MNode::Int || literal->kind == MNode::Float || node.kind == MNode::Bool || node.kind == MNode::Str || node.kind == MNode::Char)
        return AnnotationFail(origin, MAGEL_TEXT("注解参数类型不匹配：禁止隐式转换"));
    return AnnotationFail(origin, "注解参数及默认值必须是编译期常量字面量，首版不支持表达式或变量");
}
/**
 * @brief 查找当前可见注解；局部模块、根模块优先，然后检查直接导入的唯一名称。
 * @param use 使用节点。
 * @param imports 模块导入关系。
 * @return 定义下标；失败为 -1。
 */
int ResolveAnnotation(const MNode &use, const Memory::Map<Memory::String, Memory::Set<Memory::String>> &imports)
{
    const auto exact = g_annotationDeclarations.find(AnnotationName(use));
    if (exact != g_annotationDeclarations.end()) return exact->second;
    if (use.s.find("::") == Memory::String::npos)
    {
        const auto root = g_annotationDeclarations.find(use.s);
        if (root != g_annotationDeclarations.end()) return root->second;
        int chosen = -1;
        auto found = imports.find(use.module);
        if (found != imports.end())
            for (const auto &owner : found->second)
            {
                auto candidate = g_annotationDeclarations.find(owner + "::" + use.s);
                if (candidate == g_annotationDeclarations.end()) continue;
                if (chosen >= 0 && chosen != candidate->second)
                {
                    AnnotationFail(use, "注解名称有歧义，请使用限定名称：" + use.s);
                    return -1;
                }
                chosen = candidate->second;
            }
        if (chosen >= 0) return chosen;
    }
    // 普通用户声明优先，保留已有同名用户注解的行为；限定内建名可明确选择。
    if (use.s == "Reflect" || use.s == "std::reflect::Reflect") return -2;
    AnnotationFail(use, "注解必须先声明后使用：" + use.s);
    return -1;
}
/**
 * @brief 验证并绑定单次注解使用，默认值补齐到声明顺序。
 * @param useId 使用节点。
 * @param imports 导入关系。
 * @return 成功时返回 true。
 */
bool BindAnnotation(int useId, const Memory::Map<Memory::String, Memory::Set<Memory::String>> &imports)
{
    auto &use = g_nodes[useId];
    use.annotationValues.clear();
    const int declaration = ResolveAnnotation(use, imports);
    if (declaration == -2)
    {
        if (!N(use.kids[0]).kids.empty()) return AnnotationFail(use, "内建 @Reflect 不接受参数");
        use.type2 = -2;
        return true;
    }
    if (declaration < 0) return false;
    use.type2 = declaration;
    const auto &params = N(N(declaration).kids[0]).kids;
    const auto &args = N(use.kids[0]).kids;
    Memory::Vector<int> supplied(params.size(), -1);
    std::size_t position = 0;
    bool named = false;
    for (int argument : args)
    {
        const auto &arg = N(argument);
        std::size_t index = position;
        if (!arg.s.empty())
        {
            named = true;
            index = 0;
            while (index < params.size() && N(params[index]).s != arg.s) ++index;
            if (index == params.size()) return AnnotationFail(use, "注解没有这个命名参数：" + arg.s);
        }
        else
        {
            if (named) return AnnotationFail(use, "注解位置参数不能出现在命名参数之后");
            ++position;
        }
        if (index >= params.size()) return AnnotationFail(use, MAGEL_TEXT("注解参数个数过多：") + use.s);
        if (supplied[index] >= 0) return AnnotationFail(use, "注解参数重复赋值：" + N(params[index]).s);
        supplied[index] = arg.kids[0];
    }
    for (std::size_t index = 0; index < params.size(); ++index)
    {
        const auto &param = N(params[index]);
        MAnnotationValue value;
        if (supplied[index] < 0)
        {
            if (param.annotationValues.empty()) return AnnotationFail(use, "注解缺少必填参数：" + param.s);
            value = param.annotationValues[0];
        }
        else if (!AnnotationConstant(supplied[index], param.type, value, use))
            return false;
        use.annotationValues.push_back(std::move(value));
    }
    return true;
}
/**
 * @brief 按顶层顺序登记注解并校验附着声明；不会进入函数正文注册任何注解。
 * @return 所有注解有效时返回 true；关闭开关时不登记任何注解符号。
 */
bool ValidateAnnotations()
{
    g_annotationDeclarations.clear();
    if (!g_reflectionEnabled)
    {
        // 节点池可跨编译保留，仅检查当前顶层树，不能把历史编译的注解当作本次输入。
        Memory::Function<bool(int)> check;
        check = [&](int id) {
            const auto &node = N(id);
            if (node.kind == MNode::AnnotationDecl || !node.annotations.empty())
                return AnnotationFail(node.kind == MNode::AnnotationDecl ? node : N(node.annotations[0]),
                                      "注解与反射功能未开启，请使用支持该能力的工具链及 --reflection");
            if (node.kind == MNode::StructDecl || node.kind == MNode::InterfaceDecl || node.kind == MNode::EnumDecl || node.kind == MNode::List)
                for (int child : node.kids)
                    if (!check(child)) return false;
            return true;
        };
        for (int id : g_top)
        {
            if (N(id).module == "std::reflect" || (N(id).kind == MNode::ModuleDecl && N(id).s == "std::reflect"))
                return AnnotationFail(N(id), "注解与反射功能未开启，请使用 --reflection");
            if (!check(id)) return false;
        }
        return true;
    }
    Memory::Set<Memory::String> types;
    Memory::Map<Memory::String, Memory::Set<Memory::String>> imports;
    for (int id : g_top)
    {
        const auto &node = N(id);
        if (node.kind == MNode::StructDecl || node.kind == MNode::EnumDecl || node.kind == MNode::InterfaceDecl || node.kind == MNode::HostTypeDecl) types.insert(AnnotationName(node));
        if (node.kind == MNode::ImportDecl) imports[node.module].insert(node.s);
    }
    const auto bindTarget = [&](const MNode &node) -> bool {
        const bool supported = node.kind == MNode::StructDecl || node.kind == MNode::EnumDecl || node.kind == MNode::InterfaceDecl || node.kind == MNode::FuncDef || node.kind == MNode::ExternFuncDecl || node.kind == MNode::Field || node.kind == MNode::MethodDef || node.kind == MNode::ModuleDecl;
        for (int use : node.annotations)
        {
            if (!supported) return AnnotationFail(N(use), "此声明不支持注解");
            if (!BindAnnotation(use, imports)) return false;
            if (N(use).type2 == -2 && node.kind != MNode::StructDecl && node.kind != MNode::EnumDecl && node.kind != MNode::InterfaceDecl && node.kind != MNode::FuncDef)
                return AnnotationFail(N(use), "内建 @Reflect 只暴露具体类型或有实现的自由函数；成员随类型暴露");
        }
        return true;
    };
    for (int id : g_top)
    {
        auto &node = g_nodes[id];
        if (node.kind == MNode::AnnotationDecl)
        {
            const auto name = AnnotationName(node);
            if (g_annotationDeclarations.count(name)) return AnnotationFail(node, "注解不可重新声明：" + name);
            // 类型冲突检查覆盖后续声明，避免登记顺序让类型和注解共享身份。
            if (types.count(name) || BuiltinTypeByName(name) >= 0 || name == "List" || name == "Set" || name == "Map")
                return AnnotationFail(node, "注解名称与类型冲突：" + name);
            Memory::Set<Memory::String> names;
            for (int paramId : N(node.kids[0]).kids)
            {
                auto &param = g_nodes[paramId];
                param.annotationValues.clear();
                if (!names.insert(param.s).second) return AnnotationFail(node, "注解参数重名：" + param.s);
                if (!AnnotationType(T(param.type))) return AnnotationFail(node, "注解参数只支持基础类型，禁止引用、聚合、接口、函数指针及 void：" + param.s);
                if (!param.kids.empty())
                {
                    MAnnotationValue value;
                    if (!AnnotationConstant(param.kids[0], param.type, value, node)) return false;
                    param.annotationValues.push_back(std::move(value));
                }
            }
            g_annotationDeclarations.emplace(name, id);
            continue;
        }
        if (!bindTarget(node)) return false;
        if (node.kind == MNode::StructDecl || node.kind == MNode::InterfaceDecl)
        {
            for (int member : N(node.kids[0]).kids)
                if (!bindTarget(N(member))) return false;
        }
        else if (node.kind == MNode::EnumDecl && node.kids.size() > 1)
        {
            for (int member : N(node.kids[1]).kids)
                if (!bindTarget(N(member))) return false;
        }
    }
    return true;
}
