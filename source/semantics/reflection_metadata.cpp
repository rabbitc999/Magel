/**
 * @file reflection_metadata.cpp
 * @brief 从已校验的具体声明和布局生成反射元数据。
 */
namespace mr = stackvm::reflection;
/**
 * @brief 单次编译的元数据生成器，不改动 __typeTag 或接口分派的既有编号。
 */
struct MReflectionBuilder
{
    mr::Table table;
    Memory::Map<Memory::String, Memory::String> names;
    Memory::Map<std::uint64_t, Memory::String> identities;
    Memory::Map<Memory::String, const MNode *> declarations;
    bool usesBuiltin = false;
    /**
     * @brief 判断类型签名是否包含不向反射公开的胶水对象。
     * @param type 待检查的具体类型或引用。
     * @return 直接或经数组、函数、容器包含 opcode 时返回 true。
     */
    bool hidden(const std::shared_ptr<MType> &type) const
    {
        if (!type) return false;
        if (const auto *opcode = FindOpcodeDeclaration(type); opcode && opcode->internal) return true;
        if (hidden(type->elem) || hidden(type->elem2)) return true;
        for (const auto &param : type->paramTypes)
            if (hidden(param)) return true;
        return false;
    }
    /**
     * @brief 登记规范身份并检查哈希碰撞。
     * @param key 规范身份。
     * @return 稳定编号。
     */
    std::uint64_t identity(const Memory::String &key)
    {
        const auto id = mr::stableId(key);
        const auto old = identities.find(id);
        if (old != identities.end())
        {
            if (old->second != key) Fail("反射身份发生哈希碰撞：" + key);
        }
        else
        {
            identities.emplace(id, key);
            table.identities.push_back({ id, key });
        }
        return id;
    }
    /**
     * @brief 还原具体泛型名称，不公开编译器的特化编号。
     * @param name 内部名称。
     * @return 完整名称。
     */
    Memory::String named(const Memory::String &name) const
    {
        const auto found = names.find(name);
        if (found != names.end()) return found->second;
        return name;
    }
    /**
     * @brief 递归规范化类型，保留位宽、符号、引用权限及容器元素身份。
     * @param type 类型。
     * @return 规范键。
     */
    Memory::String key(const std::shared_ptr<MType> &type)
    {
        if (!type) return "type:void";
        if (IsContainer(type)) return "type:" + type->name + "<" + key(type->elem) + (type->elem2 ? "," + key(type->elem2) : Memory::String()) + ">";
        if (type->kind == MType::Struct || type->kind == MType::Host || type->kind == MType::Opcode)
        {
            const auto prefix = g_interfaces.count(type->name) ? "interface:" :
                                g_enums.count(type->name)      ? "enum:" :
                                g_structs.count(type->name)    ? "struct:" :
                                                                 "host:";
            return prefix + named(type->name);
        }
        if (type->kind == MType::Ptr) return "type:" + key(type->elem) + (type->readOnly ? " const&" : " mut&");
        if (type->kind == MType::Array)
        {
            auto value = "type:" + key(type->elem);
            if (type->dims.empty()) value += "[]";
            for (int size : type->dims) value += "[" + Memory::toString(size) + "]";
            return value;
        }
        if (type->kind == MType::Func)
        {
            Memory::String value = "type:fptr(";
            for (const auto &param : type->paramTypes) value += key(param) + ";";
            return value + (type->variadic ? "..." : "") + ")->" + key(type->elem);
        }
        return "type:" + TypeKey(type);
    }
    /**
     * @brief 登记类型及其组成类型身份。
     * @param type 已解析类型。
     * @return 稳定编号。
     */
    std::uint64_t typeId(const std::shared_ptr<MType> &type)
    {
        if (hidden(type)) return 0;
        if (type)
        {
            if (type->unresolvedName)
            {
                Fail("反射只支持已解析的具体类型：" + type->name);
                return 0;
            }
            if (type->elem) typeId(type->elem);
            if (type->elem2) typeId(type->elem2);
            for (const auto &param : type->paramTypes) typeId(param);
        }
        return identity(key(type));
    }
    /**
     * @brief 将编译期常量转为不含运行时句柄的记录。
     * @param value 已校验常量。
     * @return 常量记录。
     */
    mr::Constant constant(const MAnnotationValue &value)
    {
        mr::Constant result;
        result.typeId = typeId(T(value.type));
        result.text = value.text;
        if (T(value.type)->kind == MType::Double && T(value.type)->bits == 32)
        {
            const float number = static_cast<float>(value.floating);
            std::uint32_t bits;
            std::memcpy(&bits, &number, sizeof(bits));
            result.bits = bits;
        }
        else if (T(value.type)->kind == MType::Double)
            std::memcpy(&result.bits, &value.floating, sizeof(result.bits));
        else
            std::memcpy(&result.bits, &value.integer, sizeof(result.bits));
        return result;
    }
    /**
     * @brief 生成声明注解实例，内建暴露标记使用独立的定义身份。
     * @param node 声明。
     * @return 注解实例。
     */
    Memory::Vector<mr::Annotation> annotations(const MNode &node)
    {
        Memory::Vector<mr::Annotation> values;
        for (int useId : node.annotations)
        {
            const auto &use = N(useId);
            mr::Annotation annotation;
            if (use.type2 == -2)
            {
                usesBuiltin = true;
                annotation.annotationId = identity("builtin-annotation:std::reflect::Reflect");
            }
            else
                annotation.annotationId = identity("annotation:" + AnnotationName(N(use.type2)));
            for (const auto &value : use.annotationValues) annotation.args.push_back(constant(value));
            values.push_back(std::move(annotation));
        }
        return values;
    }
    /**
     * @brief 判断声明是否显式选择反射。
     * @param node 声明。
     * @return 选择时为真。
     */
    bool exposed(const MNode &node) const
    {
        for (int use : node.annotations)
            if (N(use).type2 == -2) return true;
        return false;
    }
    /**
     * @brief 查找源码方法节点，默认方法回到其原始接口声明。
     * @param owner 类型。
     * @param method 方法。
     * @return 源节点或空。
     */
    const MNode *methodNode(const Memory::String &owner, const MMethod &method) const
    {
        auto find = [&](const Memory::String &name) -> const MNode * {
            const auto decl = declarations.find(name);
            if (decl == declarations.end()) return nullptr;
            const auto &top = *decl->second;
            const std::size_t memberIndex = top.kind == MNode::EnumDecl ? 1u : 0u;
            if (top.kids.size() <= memberIndex) return nullptr;
            const auto list = top.kids[memberIndex];
            for (int memberId : N(list).kids)
            {
                const auto &member = N(memberId);
                if (member.kind != MNode::MethodDef) continue;
                const bool sameName = method.isConstructor && !method.isNamedConstructor ? (member.s == owner || member.s == BareTypeName(owner)) : member.s == method.source;
                if (!sameName) continue;
                if ((member.kids.size() > 1 && member.kids[1] == method.bodyId) || (method.bodyId < 0 && top.kind == MNode::InterfaceDecl)) return &member;
            }
            return nullptr;
        };
        if (const auto node = find(owner)) return node;
        for (const auto &entry : g_interfaces)
            if (const auto node = find(entry.first)) return node;
        return nullptr;
    }
    /**
     * @brief 生成具体签名；接收者不计入显式参数。
     * @param name 名称。
     * @param domain 身份域。
     * @param result 返回类型。
     * @param params 参数。
     * @param node 可选源码节点。
     * @return 函数记录。
     */
    mr::FuncInfo function(const Memory::String &name, const Memory::String &domain,
                          const std::shared_ptr<MType> &result, const Memory::Vector<MParam> &params, const MNode *node)
    {
        mr::FuncInfo info;
        info.name = name;
        info.returnTypeId = typeId(result);
        auto signature = domain + "(";
        for (const auto &param : params)
        {
            info.params.push_back({ param.source, typeId(param.type) });
            signature += key(param.type) + (param.variadic ? "...;" : ";");
        }
        info.typeId = identity(signature + ")->" + key(result));
        if (node) info.annotations = annotations(*node);
        return info;
    }
    /**
     * @brief 一元运算符的唯一操作数由接收者隐式提供。
     * @param owner 类型或接口。
     * @param name 方法名。
     * @param params 源签名参数。
     * @return 对调用者可见的参数。
     */
    Memory::Vector<MParam> callParams(const Memory::String &owner, const Memory::String &name, const Memory::Vector<MParam> &params)
    {
        for (const auto &entry : g_interfaces)
        {
            if (entry.first != owner && !StructHasInterface(owner, entry.first)) continue;
            bool left = true;
            const auto spec = OperatorInterfaceSpec(entry.first, name, &left);
            if (spec && spec->unary) return {};
        }
        return params;
    }
    /**
     * @brief 生成结构体和枚举所有活动方法，包含继承来的默认实现。
     * @param owner 内部类型名。
     * @return 方法表。
     */
    Memory::Vector<mr::MethodInfo> methods(const Memory::String &owner)
    {
        Memory::Vector<mr::MethodInfo> result;
        for (const auto &method : g_structs.at(owner).methods)
        {
            if (method.superseded) continue;
            bool privateSignature = hidden(method.result);
            for (const auto &param : method.params) privateSignature = privateSignature || hidden(param.type);
            if (privateSignature) continue;
            mr::MethodInfo info;
            auto returnType = method.result;
            if (method.isConstructor)
            {
                returnType = Memory::makeShared<MType>();
                returnType->kind = MType::Struct;
                returnType->name = owner;
            }
            info.function = function(method.isConstructor ? "new" : method.source,
                                     "method:" + named(owner) + "::" + (method.isConstructor ? "new" : method.source) + (method.isStatic ? ":static" : ":instance"), returnType, callParams(owner, method.source, method.params), methodNode(owner, method));
            info.isStatic = method.isStatic;
            info.isConstructor = method.isConstructor;
            Memory::Set<Memory::String> visiting;
            info.isConst = !method.isStatic && !method.isConstructor && !MethodMutatesReceiver(owner, method, &visiting);
            result.push_back(std::move(info));
        }
        bool constructor = false;
        for (const auto &method : result)
            if (method.isConstructor) constructor = true;
        if (!constructor && !g_enums.count(owner))
        {
            mr::MethodInfo info;
            info.isConstructor = true;
            info.function = function("new", "method:" + named(owner) + "::new:instance", NamedType(owner), {}, nullptr);
            result.push_back(std::move(info));
        }
        return result;
    }
    /**
     * @brief 使用既有所有权分类，不把借用数组和普通引用误判为拥有资源。
     * @param type 字段类型。
     * @return 拥有标志。
     */
    bool owned(const std::shared_ptr<MType> &type) const
    {
        return type && (type->kind == MType::Struct ? (StructHasOwnedLeaves(type->name) || StructNeedsDestruction(type->name)) : LeafOwnedByAggregate(type));
    }
    /**
     * @brief 生成静态成员及原始注解。
     * @param top 类型声明。
     * @return 静态成员表。
     */
    Memory::Vector<mr::StaticMemberInfo> statics(const MNode &top)
    {
        Memory::Vector<mr::StaticMemberInfo> result;
        const std::size_t memberIndex = top.kind == MNode::EnumDecl ? 1u : 0u;
        if (top.kids.size() <= memberIndex) return result;
        const auto list = top.kids[memberIndex];
        for (int id : N(list).kids)
        {
            const auto &member = N(id);
            if (member.kind == MNode::Field && member.type2 == 1 && !hidden(T(member.type)))
                result.push_back({ member.s, typeId(T(member.type)), owned(T(member.type)), annotations(member) });
        }
        return result;
    }
    /**
     * @brief 生成包括继承关系的接口身份。
     * @param owner 类型。
     * @return 接口编号表。
     */
    Memory::Vector<std::uint64_t> interfaces(const Memory::String &owner)
    {
        Memory::Vector<std::uint64_t> result;
        for (const auto &entry : g_interfaces)
            if (StructHasInterface(owner, entry.first))
                result.push_back(identity("interface:" + named(entry.first)));
        return result;
    }
    /**
     * @brief 暴露已编译接口契约，始终保留实际类型分派。
     * @param owner 具体接口名。
     */
    void interfaceInfo(const Memory::String &owner)
    {
        for (const auto &info : table.interfaces)
            if (info.typeId == identity("interface:" + named(owner))) return;
        mr::InterfaceInfo info;
        info.name = named(owner);
        info.typeId = identity("interface:" + info.name);
        for (const auto &method : g_interfaces.at(owner))
        {
            bool privateSignature = hidden(method.result);
            for (const auto &param : method.bodyParams) privateSignature = privateSignature || hidden(param.type);
            if (privateSignature) continue;
            MMethod source;
            source.source = method.name;
            source.bodyId = method.bodyId;
            mr::MethodInfo value;
            value.function = function(method.name, "interface-method:" + info.name + "::" + method.name, method.result, callParams(owner, method.name, method.bodyParams), methodNode(method.owner, source));
            value.hasDefaultBody = method.bodyId >= 0;
            info.methods.push_back(std::move(value));
        }
        for (const auto &entry : declarations)
            if (entry.second->kind != MNode::InterfaceDecl && StructHasInterface(entry.first, owner))
                info.implementedBy.push_back(identity((entry.second->kind == MNode::EnumDecl ? "enum:" : "struct:") + named(entry.first)));
        if (declarations.count(owner)) info.annotations = annotations(*declarations.at(owner));
        table.interfaces.push_back(std::move(info));
    }
    /**
     * @brief 从完整具体声明和实际布局构建表。
     * @return 校验成功时为真。
     */
    bool build()
    {
        for (int id : g_top)
        {
            const auto &top = N(id);
            if (top.kind != MNode::StructDecl && top.kind != MNode::EnumDecl && top.kind != MNode::InterfaceDecl && top.kind != MNode::OpcodeDecl) continue;
            names[top.s] = AnnotationName(top);
            declarations[top.s] = &top;
        }
        for (const auto &item : chali_generic::session.made)
        {
            const auto &expansion = item.second;
            if (!expansion.generic) continue;
            auto name = ModuleSymbol(expansion.generic->module, expansion.generic->name) + "<";
            for (std::size_t i = 0; i < expansion.args.size(); ++i)
            {
                if (i) name += ",";
                name += named(expansion.args[i]);
            }
            name += ">";
            names[expansion.name] = name;
            const auto qualified = ModuleSymbol(expansion.generic->module, expansion.name);
            if (names.count(qualified)) names[qualified] = name;
        }
        // 注解声明无论是否用于暴露声明都保留，保证可独立查询默认值。
        for (int id : g_top)
        {
            const auto &top = N(id);
            if (top.kind != MNode::AnnotationDecl) continue;
            mr::AnnotationInfo info;
            info.name = AnnotationName(top);
            info.typeId = identity("annotation:" + info.name);
            for (int paramId : N(top.kids[0]).kids)
            {
                const auto &param = N(paramId);
                mr::AnnotationParameter value;
                value.name = param.s;
                value.typeId = typeId(T(param.type));
                if (!param.annotationValues.empty()) value.defaultValue = constant(param.annotationValues[0]);
                info.params.push_back(std::move(value));
            }
            table.annotations.push_back(std::move(info));
        }
        for (int id : g_top)
        {
            const auto &top = N(id);
            if (top.kind == MNode::ModuleDecl)
            {
                const auto values = annotations(top);
                table.moduleAnnotations.insert(table.moduleAnnotations.end(), values.begin(), values.end());
            }
            if (!exposed(top)) continue;
            if (top.kind == MNode::FuncDef)
            {
                const auto &decl = g_functionDecls.at(static_cast<std::size_t>(top.i));
                if (decl.superseded) continue;
                bool privateSignature = hidden(decl.signature.result);
                for (const auto &param : decl.signature.params) privateSignature = privateSignature || hidden(param.type);
                if (privateSignature) continue;
                const auto name = ModuleSymbol(decl.module, decl.source);
                table.functions.push_back(function(name, "function:" + name, decl.signature.result, decl.signature.params, &top));
            }
            else if (top.kind == MNode::StructDecl || top.kind == MNode::OpcodeDecl)
            {
                mr::StructInfo info;
                info.name = named(top.s);
                info.typeId = identity("struct:" + info.name);
                for (const auto &field : g_structs.at(top.s).fields)
                {
                    if (field.name == "opcode" && top.kind == MNode::OpcodeDecl) continue;
                    if (hidden(field.type)) continue;
                    mr::FieldInfo value{ field.name, static_cast<std::uint32_t>(field.slot * 8), typeId(field.type), owned(field.type), {} };
                    for (int memberId : N(top.kids[0]).kids)
                        if (N(memberId).kind == MNode::Field && N(memberId).s == field.name && N(memberId).type2 != 1) value.annotations = annotations(N(memberId));
                    info.fields.push_back(std::move(value));
                }
                info.methods = methods(top.s);
                info.staticMembers = statics(top);
                info.implementedInterfaces = interfaces(top.s);
                info.annotations = annotations(top);
                table.structs.push_back(std::move(info));
            }
            else if (top.kind == MNode::EnumDecl)
            {
                mr::EnumInfo info;
                info.name = named(top.s);
                info.typeId = identity("enum:" + info.name);
                for (const auto &variant : g_enums.at(top.s).variants)
                {
                    mr::VariantInfo value{ variant.name, static_cast<std::uint32_t>(variant.tag), {} };
                    for (const auto &payload : variant.payloads) value.payloads.push_back({ payload.name, typeId(payload.type) });
                    info.variants.push_back(std::move(value));
                }
                info.methods = methods(top.s);
                info.staticMembers = statics(top);
                info.implementedInterfaces = interfaces(top.s);
                info.annotations = annotations(top);
                table.enums.push_back(std::move(info));
            }
            else if (top.kind == MNode::InterfaceDecl)
            {
                interfaceInfo(top.s);
            }
        }
        // 显式暴露类型同时暴露其封闭的已单态化接口契约，包含运算符接口。
        Memory::Set<std::uint64_t> contracts;
        for (const auto &info : table.structs)
            for (auto id : info.implementedInterfaces) contracts.insert(id);
        for (const auto &info : table.enums)
            for (auto id : info.implementedInterfaces) contracts.insert(id);
        for (const auto &entry : g_interfaces)
            if (contracts.count(identity("interface:" + named(entry.first)))) interfaceInfo(entry.first);
        if (usesBuiltin) table.annotations.push_back({ "std::reflect::Reflect", identity("builtin-annotation:std::reflect::Reflect"), {} });
        Memory::String error;
        Memory::Vector<std::uint8_t> bytes;
        if (!g_error.empty()) return false;
        if (!mr::encode(table, bytes, &error)) return Fail(error);
        return true;
    }
};
