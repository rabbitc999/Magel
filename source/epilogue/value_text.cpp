/**
 * @brief 为默认文本表示构造不依赖反射的静态布局。
 * @param type 待格式化的类型。
 * @param visiting 当前递归路径，避免递归容器类型无限展开。
 * @param callbacks 接收只读自定义方法及其临时字段布局。
 * @param custom 是否允许当前节点使用用户覆盖。
 * @param depth 当前布局嵌套深度。
 * @retval 保持字段顺序、容器元素类型和数组边界的布局。
 */
stackvm::value_text::Shape ValueTextShape(const std::shared_ptr<MType> &type,
                                          Memory::Set<Memory::String> &visiting, Memory::Vector<simple_ast::NodePtr> &callbacks, bool custom = true, std::size_t depth = 0)
{
    using stackvm::value_text::Shape;
    Shape out;
    if (!type) return out;
    out.name = Describe(type);
    // 四元式的浮点存储统一使用双精度槽，float32 在写入前完成数值舍入。
    out.width = type->kind == MType::Char || type->kind == MType::Bool ? 1 :
                type->kind == MType::Int                               ? std::max(1, type->bits / 8) :
                                                                         8;
    if (depth > 32) return out;
    switch (type->kind)
    {
    case MType::Int: out.kind = type->isUnsigned ? 'u' : 'i'; break;
    case MType::Double: out.kind = 'f'; break;
    case MType::Bool: out.kind = 'b'; break;
    case MType::Char: out.kind = 'c'; break;
    case MType::String: out.kind = 's'; break;
    case MType::BigInt: out.kind = 'I'; break;
    case MType::BigFloat: out.kind = 'D'; break;
    case MType::Obj: out.kind = 'O'; break;
    case MType::Ptr: out.kind = 'R'; break;
    case MType::Func: out.kind = 'F'; break;
    case MType::Host: out.kind = 'H'; break;
    case MType::File:
    case MType::Proc: out.kind = 'H'; break;
    case MType::Container:
        out.kind = type->name == "vec" ? 'L' : type->name == "set" ? 'E' :
                                                                     'M';
        if (visiting.insert(TypeKey(type)).second)
        {
            out.children.push_back(ValueTextShape(type->elem, visiting, callbacks, true, depth + 1));
            if (type->elem2) out.children.push_back(ValueTextShape(type->elem2, visiting, callbacks, true, depth + 1));
            visiting.erase(TypeKey(type));
        }
        break;
    case MType::Array:
        out.kind = IsRuntimeArray(type) ? 'a' : 'A';
        out.count = 1;
        for (const auto dimension : type->dims) out.count *= static_cast<std::size_t>(dimension);
        out.width = type->elem->kind == MType::Struct ? StructStride(type->elem) : type->elem->kind == MType::Char ? 1 :
                                                                                                                     8;
        out.children.push_back(ValueTextShape(type->elem, visiting, callbacks, true, depth + 1));
        break;
    case MType::Struct:
    {
        EnsureTupleLayout(type);
        const auto definition = g_structs.find(type->name);
        if (definition == g_structs.end() || !visiting.insert(type->name).second) break;
        out.kind = IsTupleType(type) ? 'T' : 'S';
        out.width = static_cast<std::size_t>(StructStride(type));
        if (custom)
            for (const auto &method : definition->second.methods)
                if (!method.superseded && !method.defaultValueText && !method.isStatic && method.source == "toString" && method.params.empty() && method.result->kind == MType::String)
                {
                    Memory::Set<Memory::String> checking;
                    if (MethodMutatesReceiver(type->name, method, &checking))
                        out.formatter = std::numeric_limits<std::size_t>::max();
                    else
                    {
                        out.formatter = callbacks.size() / 2 + 1;
                        callbacks.push_back(FunctionPointerValue(type->name + "." + method.name));
                        const bool hasArray = std::any_of(definition->second.leaves.begin(), definition->second.leaves.end(), [](const auto &leaf) { return leaf.second->kind == MType::Array; });
                        callbacks.push_back(hasArray ? simple_ast::Node::i(0) : ContainerStorageFlag(type, true));
                    }
                    break;
                }
        const auto enumeration = g_enums.find(type->name);
        if (enumeration != g_enums.end())
        {
            out.kind = 'N';
            for (const auto &variant : enumeration->second.variants)
            {
                Shape child;
                child.kind = 'V';
                child.name = variant.name;
                child.count = static_cast<std::size_t>(variant.tag);
                for (const auto &payload : variant.payloads)
                {
                    auto field = ValueTextShape(payload.type, visiting, callbacks, true, depth + 1);
                    if (const auto declared = FindField(type->name, payload.storageName)) field.offset = static_cast<std::size_t>(declared->slot) * 8;
                    child.children.push_back(std::move(field));
                }
                out.children.push_back(std::move(child));
            }
        }
        else
            for (const auto &declared : definition->second.fields)
            {
                auto field = ValueTextShape(declared.type, visiting, callbacks, true, depth + 1);
                field.offset = static_cast<std::size_t>(declared.slot) * 8;
                out.children.push_back(std::move(field));
            }
        visiting.erase(type->name);
        break;
    }
    default: break;
    }
    return out;
}

/**
 * @brief 编码具体类型的默认字符串表示布局。
 * @param type 需要编码的类型。
 * @param customRoot 是否保留根值的用户覆盖。
 * @retval 供 VM 与 AOT 共用原语读取的布局。
 */
Memory::String ValueTextLayout(const std::shared_ptr<MType> &type, bool customRoot)
{
    Memory::Set<Memory::String> visiting;
    Memory::Vector<simple_ast::NodePtr> callbacks;
    return stackvm::value_text::encode(ValueTextShape(type, visiting, callbacks, customRoot));
}

/**
 * @brief 获取静态布局对应的自定义方法回调，顺序与布局索引一致。
 * @param type 需要描述的类型。
 * @param customRoot 是否保留根值的用户覆盖。
 * @retval 方法值与聚合存储布局组成的成对实参。
 */
Memory::Vector<simple_ast::NodePtr> ValueTextCallbacks(const std::shared_ptr<MType> &type, bool customRoot)
{
    Memory::Set<Memory::String> visiting;
    Memory::Vector<simple_ast::NodePtr> callbacks;
    ValueTextShape(type, visiting, callbacks, customRoot);
    return callbacks;
}

/**
 * @brief 生成默认文本转换，临时接收者由完整表达式持有。
 * @param valueId 只求值一次的源表达式。
 * @retval 新分配且由调用者接管的字符串。
 */
simple_ast::NodePtr LowerDefaultValueText(int valueId)
{
    const auto type = ExprType(valueId);
    if (!type || type->kind == MType::Void) return Fail("void 不是可转换为字符串的值"), Bad();
    auto value = type->kind == MType::Struct                            ? OwnedAggregateTempFor(valueId, type) :
                 type->kind == MType::String && NeedsOwnedTemp(valueId) ? OwnedStringTemp(valueId) :
                 IsObjectLike(type)                                     ? OwnedObjectTemp(valueId, type) :
                                                                          LowerExpr(valueId);
    if (g_freshSink && (IsRuntimeArray(type) || IsResourceLike(type)) && IsFreshValue(valueId))
    {
        const auto temporary = AllocateName("__magel_text_source");
        g_freshSink->push_back(simple_ast::Node::declare(temporary, simple_ast::Type::i64()));
        g_freshSink->push_back(MakeDrop(temporary, IsRuntimeArray(type)));
        g_freshSink->push_back(simple_ast::Node::assign(temporary, value));
        if (IsRuntimeArray(type))
            MarkOwnedArray(temporary, type);
        else
            MarkOwned(temporary);
        NoteExpressionTemp(temporary);
        value = simple_ast::Node::var(temporary);
    }
    Memory::Vector<Memory::String> names{ "value", "shape" };
    Memory::Vector<simple_ast::NodePtr> values{ value, simple_ast::Node::str(ValueTextLayout(type, false)) };
    for (const auto &callback : ValueTextCallbacks(type, false))
    {
        names.push_back("callback" + Memory::toString(names.size()));
        values.push_back(callback);
    }
    return simple_ast::Node::call("__vm_value_to_string", names, values);
}

/**
 * @brief 统一无参 toString 和格式化的字符串转换入口。
 * @param valueId 输入表达式。
 * @retval 自定义方法结果或默认值文本，均为调用点拥有的字符串。
 */
simple_ast::NodePtr LowerValueText(int valueId)
{
    const auto type = ExprType(valueId);
    if (type && type->kind == MType::Struct)
    {
        const auto definition = g_structs.find(type->name);
        if (definition != g_structs.end())
            for (const auto &method : definition->second.methods)
                if (!method.superseded && !method.isStatic && method.source == "toString" && method.params.empty() && method.result->kind == MType::String)
                {
                    const auto label = type->name + "." + method.name;
                    if (!CheckMethodReceiver(valueId, label)) return Bad();
                    return simple_ast::Node::call(label, { "self" }, { OwnedAggregateTempFor(valueId, type) });
                }
    }
    if (type)
        if (const auto converted = LowerDeclaredConversion(valueId, T(M_TypeString()), false)) return converted;
    return LowerDefaultValueText(valueId);
}
