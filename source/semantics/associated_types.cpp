/* ---------- 关联类型：声明占位、具体绑定与接口签名实例化 ---------- */
/**
 * @brief 具体类型的关联类型定义；键保持声明的限定身份。
 */
Memory::Map<Memory::String, Memory::Map<Memory::String, std::shared_ptr<MType>>> g_associatedDefinitions;
/**
 * @brief 接口要求的关联类型，包括继承要求。
 */
Memory::Map<Memory::String, Memory::Set<Memory::String>> g_associatedRequirements;
/**
 * @brief 按实现者实例化接口签名，避免不同 Item 类型共用同一占位签名。
 */
Memory::Map<Memory::String, Memory::Vector<MInterfaceMethod>> g_associatedMethods;

/**
 * @brief 查找关联类型的具体定义。
 * @param owner 实现类型。
 * @param item 关联名称。
 * @return 具体定义或空。
 */
std::shared_ptr<MType> AssociatedDefinition(const Memory::String &owner, const Memory::String &item)
{
    auto found = g_associatedDefinitions.find(owner);
    if (found == g_associatedDefinitions.end()) found = g_associatedDefinitions.find(BareTypeName(owner));
    if (found == g_associatedDefinitions.end()) return nullptr;
    const auto definition = found->second.find(item);
    return definition == found->second.end() ? nullptr : definition->second;
}
/**
 * @brief 递归替换投影和接口占位，保留引用、容器及函数类型结构。
 * @param type 原类型。
 * @param implementation 接口实现者；空串表示具体投影。
 * @param depth 防止循环关联定义。
 * @return 具体类型。
 */
std::shared_ptr<MType> ResolveAssociated(const std::shared_ptr<MType> &type,
                                         const Memory::String &implementation = {}, unsigned depth = 0)
{
    if (!type) return nullptr;
    if (depth > 64)
    {
        Fail("关联类型定义循环或嵌套超过 64 层");
        return type;
    }
    if (type->kind == MType::Struct)
    {
        const auto split = type->name.rfind("::");
        if (split != Memory::String::npos)
        {
            const auto owner = type->name.substr(0, split), item = type->name.substr(split + 2);
            auto required = g_associatedRequirements.find(owner);
            if (required == g_associatedRequirements.end()) required = g_associatedRequirements.find(BareTypeName(owner));
            const bool placeholder = required != g_associatedRequirements.end() && required->second.count(item);
            const auto actual = AssociatedDefinition(!implementation.empty() && placeholder ? implementation : owner, item);
            if (actual) return ResolveAssociated(actual, implementation, depth + 1);
        }
    }
    auto result = Memory::makeShared<MType>(*type);
    result->elem = ResolveAssociated(type->elem, implementation, depth + 1);
    result->elem2 = ResolveAssociated(type->elem2, implementation, depth + 1);
    for (auto &param : result->paramTypes) param = ResolveAssociated(param, implementation, depth + 1);
    return result;
}
/**
 * @brief 为接口默认方法复制带具体类型的语法树。
 * @param id 原节点。
 * @param owner 实现者。
 * @return 新节点。
 */
int CopyAssociatedBody(int id, const Memory::String &owner)
{
    if (id < 0) return id;
    const auto original = N(id);
    const int copy = M_New(original.kind);
    g_nodes[copy] = original;
    if (original.type >= 0) g_nodes[copy].type = M_AddType(ResolveAssociated(T(original.type), owner));
    if (original.kind == MNode::Block && original.i >= 0) g_nodes[copy].i = CopyAssociatedBody(static_cast<int>(original.i), owner);
    for (std::size_t i = 0; i < original.kids.size(); ++i) g_nodes[copy].kids[i] = CopyAssociatedBody(original.kids[i], owner);
    return copy;
}
/**
 * @brief 返回按实现类型实例化后的接口签名。
 * @param owner 实现者。
 * @param iface 接口。
 * @return 签名表。
 */
const Memory::Vector<MInterfaceMethod> &AssociatedMethods(const Memory::String &owner, const Memory::String &iface)
{
    if (g_associatedRequirements[iface].empty()) return g_interfaces[iface];
    const auto key = owner + "\n" + iface;
    auto found = g_associatedMethods.find(key);
    if (found != g_associatedMethods.end()) return found->second;
    auto methods = g_interfaces[iface];
    for (auto &method : methods)
    {
        method.result = ResolveAssociated(method.result, owner);
        for (auto &param : method.params) param = ResolveAssociated(param, owner);
        for (auto &param : method.bodyParams) param.type = ResolveAssociated(param.type, owner);
        method.bodyId = CopyAssociatedBody(method.bodyId, owner);
    }
    return g_associatedMethods.emplace(key, std::move(methods)).first->second;
}
