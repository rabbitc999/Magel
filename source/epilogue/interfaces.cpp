/* ---------- 接口：签名比较 / 实现核对 ---------- */

/**
 * @brief 接口要求的签名（名字 + 返回类型 + 形参类型）拼成一个串，用来判重。
 * @param name 名称文本。
 * @param result 接收处理结果的对象。
 * @param params 形参或类型参数列表。
 * @retval 生成或读取的结果文本。
 */
Memory::String InterfaceSignatureKey(const Memory::String &name, const std::shared_ptr<MType> &result,
                                     const Memory::Vector<std::shared_ptr<MType>> &params)
{
    Memory::String key = name + "(";
    for (std::size_t index = 0; index < params.size(); ++index)
    {
        if (index != 0u) key += ", ";
        key += Describe(params[index]);
    }
    return key + ") -> " + Describe(result);
}

/**
 * @brief 把接口 `name` 的方法表从基接口展开（`g_interfaces[name]` 就地变成"全集"）。
 *
 * 规则与结构体那一侧同口径：
 *   * 自己的方法在前，基接口里同名同签名的直接丢掉 —— 派生接口**覆盖**了它；
 *   * 同名**不同签名**是真冲突，报错（接口不接受重载）；
 *   * 两个**不同**的基接口都给了同名同签名的默认方法、派生接口又没覆盖 → 报错，
 *     和 `struct X : A, B` 撞默认方法时一个规矩；
 *   * `visiting` 检测继承成环（`interface A : B` 且 `B : A`）；成环返回 false。
 *
 * @param name 接口名。
 * @param visiting 当前递归栈。
 * @param done 已经展开过的接口（基接口被多个派生接口共享时只算一次）。
 */
bool FlattenInterface(const Memory::String &name,
                      Memory::Set<Memory::String> &visiting,
                      Memory::Set<Memory::String> &done)
{
    if (done.count(name) != 0u) return true;
    if (!visiting.insert(name).second)
        return Fail("接口继承成环：" + name + "（接口不能直接或间接继承自己）");
    const auto bases = g_interfaceBases.find(name);
    if (bases != g_interfaceBases.end())
    {
        Memory::Vector<MInterfaceMethod> &methods = g_interfaces[name];
        for (const Memory::String &base : bases->second)
        {
            if (g_interfaces.count(base) == 0u)
                return Fail("接口 " + name + " 继承的接口没有声明：" + base);
            if (!FlattenInterface(base, visiting, done)) return false;
            g_associatedRequirements[name].insert(g_associatedRequirements[base].begin(), g_associatedRequirements[base].end());
            for (const MInterfaceMethod &inherited : g_interfaces[base])
            {
                bool taken = false;
                for (MInterfaceMethod &existing : methods)
                {
                    if (existing.name != inherited.name) continue;
                    const Memory::String here =
                        InterfaceSignatureKey(inherited.name, inherited.result, inherited.params);
                    const Memory::String there =
                        InterfaceSignatureKey(existing.name, existing.result, existing.params);
                    if (here != there)
                        return Fail("接口 " + name + " 里 " + inherited.name + " 有两个不同签名的声明：" + there + " 与 " + here);
                    taken = true;
                    if (existing.owner == name) break; /* 自己的声明覆盖基接口的 */
                    if (existing.owner != inherited.owner && existing.bodyId >= 0 && inherited.bodyId >= 0)
                        return Fail("接口 " + name + " 的默认方法 " + here + " 同时来自 " + existing.owner + " 和 " + inherited.owner + "；请显式覆盖");
                    if (existing.bodyId < 0 && inherited.bodyId >= 0)
                        existing = inherited; /* 抽象要求让位给默认实现 */
                    break;
                }
                if (!taken) methods.push_back(inherited);
            }
        }
    }
    visiting.erase(name);
    done.insert(name);
    return true;
}

/**
 * @brief 结构体里的一个方法，签名是不是正好接得住接口的这个要求。
 * @param required 要求实现的接口方法或签名。
 * @param method 压缩方法编号或目标方法名称。
 * @retval 满足检查条件或操作完成时为 true，检查失败或无法完成时为 false。
 */
bool SameInterfaceSignature(const MInterfaceMethod &required, const MMethod &method)
{
    if (method.isStatic != required.isStatic || method.isConstructor) return false;
    if (method.source != required.name) return false;
    if (!SameType(method.result, required.result)) return false;
    if (method.params.size() != required.params.size()) return false;
    for (std::size_t index = 0; index < required.params.size(); ++index)
    {
        if (!SameType(method.params[index].type, required.params[index])) return false;
    }
    return true;
}

/**
 * @brief 将接口默认实现加入具体类型的方法集合。
 * @param owner 所属对象或类型信息。
 * @param definition 当前类型或接口的定义。
 * @retval 满足检查条件或操作完成时为 true，检查失败或无法完成时为 false。
 */
bool AddInterfaceDefaults(const Memory::String &owner, MStructDef &definition)
{
    Memory::Map<Memory::String, std::pair<Memory::String, const MInterfaceMethod *>> defaults;
    for (const Memory::String &ifaceName : definition.interfaces)
    {
        const auto found = g_interfaces.find(ifaceName);
        if (found == g_interfaces.end()) continue;
        for (const MInterfaceMethod &required : AssociatedMethods(owner, ifaceName))
        {
            bool overridden = false;
            for (const MMethod &method : definition.methods)
            {
                if (method.source != required.name || method.params.size() != required.params.size()) continue;
                bool sameParams = true;
                for (std::size_t index = 0; index < required.params.size(); ++index)
                    if (!SameType(method.params[index].type, required.params[index]))
                        sameParams = false;
                if (!sameParams) continue;
                if (!SameInterfaceSignature(required, method))
                {
                    if (required.bodyId >= 0)
                        return Fail(owner + "." + required.name + " 与接口 " + ifaceName + " 的方法签名不一致");
                    continue;
                }
                overridden = true;
                break;
            }
            if (overridden || required.bodyId < 0) continue;
            const Memory::String key = InterfaceSignatureKey(required.name, required.result,
                                                             required.params);
            const auto earlier = defaults.find(key);
            if (earlier != defaults.end())
            {
                /* 同一个声明经继承路径被带进来两次（`struct X : Base, Derived`）：不算冲突。 */
                if (earlier->second.second->owner == required.owner) continue;
                return Fail(owner + " 的接口默认方法 " + key + " 同时来自 " + earlier->second.second->owner + " 和 " + required.owner + "；请显式覆盖");
            }
            defaults.emplace(key, std::make_pair(required.owner, &required));
        }
    }
    for (const auto &entry : defaults)
    {
        const MInterfaceMethod &required = *entry.second.second;
        MMethod method;
        method.isStatic = required.isStatic;
        method.source = required.name;
        method.result = required.result;
        method.params = required.bodyParams;
        method.bodyId = required.bodyId;
        method.module = required.module;
        definition.methods.push_back(std::move(method));
    }
    return true;
}

/**
 * @brief 为具体聚合补充默认无参 toString，隐式字符串转换实现可作为转发目标。
 * @param owner 具体类型名，用于诊断。
 * @param definition 待补充方法的类型定义。
 * @param module 合成方法所属模块。
 * @retval 方法签名合法时返回 true。
 * @note 用户无参方法优先；仅补一层单向转发，不生成相互调用的两个方法。
 */
bool AddDefaultToString(const Memory::String &owner, MStructDef &definition, const Memory::String &module)
{
    bool present = false;
    const MMethod *conversion = nullptr;
    const MMethod *formatted = nullptr;
    for (const auto &method : definition.methods)
    {
        if (method.superseded || method.isConstructor) continue;
        if (method.source == "toString" && method.params.empty())
        {
            if (method.isStatic || !method.result || method.result->kind != MType::String)
                return Fail(owner + ".toString() 必须是返回 String 的实例方法");
            present = true;
        }
        if (!method.isStatic && method.source == "implicitConvert" && method.params.empty() && method.result && method.result->kind == MType::String) conversion = &method;
        if (!method.isStatic && method.source == "toString" && method.params.size() == 1 && method.params[0].type->kind == MType::String && method.result && method.result->kind == MType::String) formatted = &method;
    }
    if (!present)
    {
        const auto begin = g_nodes.size();
        const auto self = M_Var(M_Name("self"));
        int call = -1;
        if (conversion || formatted)
        {
            const auto method = conversion ? conversion->source : formatted->source;
            const auto arguments = conversion ? M_List() : M_List1(M_New(MNode::Str, ""));
            call = M_CallOn(M_Member(self, M_Name(method.c_str())), arguments);
        }
        else
            call = M_CallOn(M_Var(M_Name("__magel_default_to_string")), M_List1(self));
        MMethod method;
        method.source = "toString";
        method.defaultValueText = !conversion && !formatted;
        method.result = T(M_TypeString());
        method.bodyId = M_Block(M_List1(M_Return(call)));
        method.module = module;
        for (std::size_t index = begin; index < g_nodes.size(); ++index) g_nodes[index].module = module;
        definition.methods.push_back(std::move(method));
    }
    bool requiresStringConversion = false;
    for (const auto &interfaceName : definition.interfaces)
    {
        const auto found = g_interfaces.find(interfaceName);
        if (found == g_interfaces.end()) continue;
        for (const auto &required : found->second)
            if (required.name == "implicitConvert" && required.params.empty() && required.result->kind == MType::String)
                requiresStringConversion = true;
    }
    if (requiresStringConversion && !conversion)
    {
        const auto begin = g_nodes.size();
        const auto self = M_Var(M_Name("self"));
        const auto call = M_CallOn(M_Member(self, M_Name("toString")), M_List());
        MMethod method;
        method.source = "implicitConvert";
        method.result = T(M_TypeString());
        method.bodyId = M_Block(M_List1(M_Return(call)));
        method.module = module;
        for (std::size_t index = begin; index < g_nodes.size(); ++index) g_nodes[index].module = module;
        definition.methods.push_back(std::move(method));
    }
    // 默认方法构成实际的 ToString 实现；未导入 std::io 时不强制加载 I/O 模块。
    for (const auto &entry : g_interfaces)
        if ((entry.first == "std::io::ToString" || entry.first == "ToString") && entry.second.size() == 1 && entry.second[0].name == "toString" && entry.second[0].params.empty() && entry.second[0].result->kind == MType::String && std::find(definition.interfaces.begin(), definition.interfaces.end(), entry.first) == definition.interfaces.end())
            definition.interfaces.push_back(entry.first);
    return true;
}

/**
 * @brief 核对"继承的接口是不是都**完全实现**了"。
 *
 * 两个接口要的是同一个函数（同名同签名）时只算一条 —— 这就是"接口函数相同会去重"：
 * 结构体把那个函数实现一遍就够了，不必实现两次。
 *
 * @param owner 结构体名。
 * @param definition 结构体的定义（字段 / 方法）。
 * @return 都实现了返回 true；否则记下缺的那些并返回 false。
 */
bool VerifyInterfaces(const Memory::String &owner, const MStructDef &definition,
                      const Memory::String &ownerKind = "struct")
{
    Memory::Vector<Memory::String> checked; /* 已经核对过的签名（跨接口去重用） */
    for (const Memory::String &ifaceName : definition.interfaces)
    {
        const auto found = g_interfaces.find(ifaceName);
        if (found == g_interfaces.end()) continue; /* 上面（继承检查）已经报过了 */
        for (const auto &item : g_associatedRequirements[ifaceName])
            if (!AssociatedDefinition(owner, item)) return Fail(owner + " 未定义接口 " + ifaceName + " 要求的关联类型 " + item);
        Memory::Vector<Memory::String> missing;
        for (const MInterfaceMethod &required : AssociatedMethods(owner, ifaceName))
        {
            const Memory::String key =
                InterfaceSignatureKey(required.name, required.result, required.params);
            if (std::find(checked.begin(), checked.end(), key) != checked.end()) continue;
            checked.push_back(key);
            bool implemented = false;
            for (const MMethod &method : definition.methods)
            {
                if (SameInterfaceSignature(required, method))
                {
                    implemented = true;
                    break;
                }
            }
            if (!implemented) missing.push_back(key);
        }
        if (!missing.empty())
        {
            return Fail(ownerKind + " " + owner + " 实现了接口 " + ifaceName + "，但没有完全实现：" + JoinLabels(missing) + "（接口要求同名、同返回类型、同形参类型；一个都不能少）");
        }
    }
    return true;
}

/**
 * @brief 类型的可读名字（报错文案用）。
 * @param type 类型描述。
 * @retval 生成或读取的结果文本。
 */
Memory::String Describe(const std::shared_ptr<MType> &type)
{
    if (!type) return Memory::String("void");
    switch (type->kind)
    {
    case MType::Void: return "void";
    case MType::Bool: return "bool";
    case MType::Char: return "char";
    case MType::Int:
        if (type->bits == 64) return type->isUnsigned ? "uint64" : "int64";
        return Memory::String(type->isUnsigned ? "uint" : "int") + Memory::toString(type->bits);
    case MType::Double: return type->bits == 64 ? "float64" : "float32";
    case MType::String: return "std::lang::String";
    case MType::Host: return type->name;
    case MType::Struct:
    {
        if (!IsTupleType(type)) return type->name;
        Memory::Vector<std::shared_ptr<MType>> elements = type->paramTypes;
        const auto known = g_tupleElements.find(type->name);
        if (elements.empty() && known != g_tupleElements.end()) elements = known->second;
        Memory::String text = "(";
        for (std::size_t index = 0; index < elements.size(); ++index)
        {
            if (index != 0u) text += ", ";
            text += Describe(elements[index]);
        }
        return text + ")";
    }
    case MType::BigInt: return "std::lang::BigInteger";
    case MType::BigFloat: return "std::lang::BigDecimal";
    case MType::File: return "file";
    case MType::Proc: return "proc";
    case MType::Obj: return "std::lang::Any";
    case MType::Container:
        if (type->name == "map")
        {
            return "std::lang::Map<" + Describe(type->elem) + ", " + Describe(type->elem2) + ">";
        }
        return Memory::String(type->name == "vec" ? "std::lang::List<" : "std::lang::Set<") + Describe(type->elem) + ">";
    case MType::Ptr:
    {
        const Memory::String star = type->readOnly ? " const&" : " mut&";
        return Describe(type->elem) + star;
    }
    case MType::Func:
    {
        /* `int(int32, int32)` 这种写法：返回类型 + 形参类型表。 */
        Memory::String text = Describe(type->elem) + "(";
        for (std::size_t index = 0; index < type->paramTypes.size(); ++index)
        {
            if (index != 0u) text += ", ";
            if (type->variadic && index + 1u == type->paramTypes.size()) text += "...";
            text += Describe(type->paramTypes[index]);
        }
        return text + ")";
    }
    case MType::Array:
    {
        Memory::String text = Describe(type->elem);
        for (const int dim : type->dims) text += "[" + Memory::toString(dim) + "]";
        return text;
    }
    }
    return "void";
}

/**
 * @brief 两个前端类型是不是同一个（运算符重载的形参要求完全一致，不做隐式转换）。
 * @param left 左操作数。
 * @param right 右操作数。
 * @retval 满足检查条件或操作完成时为 true，检查失败或无法完成时为 false。
 */
bool SameType(const std::shared_ptr<MType> &left, const std::shared_ptr<MType> &right)
{
    if (!left || !right) return false;
    if (left->kind != right->kind) return false;
    switch (left->kind)
    {
    case MType::Bool: return true;
    case MType::Int:
        return left->isUnsigned == right->isUnsigned && left->bits == right->bits;
    case MType::Double:
        return left->bits == right->bits;
    case MType::File:
    case MType::Proc:
        return true; /* 宿主资源句柄只有一种静态形状 */
    case MType::Obj:
        return true; /* 装箱格子只有一种（里面装什么不看静态类型，看运行期的类型号） */
    /* 解析后的名义类型键保留不同模块同名类型的身份。 */
    case MType::Struct:
        if (g_structs.count(left->name) && g_structs.count(right->name)) return left->name == right->name;
        return BareTypeName(left->name) == BareTypeName(right->name);
    case MType::Host:
        if (g_hostTypes.count(left->name) && g_hostTypes.count(right->name)) return left->name == right->name;
        return BareTypeName(left->name) == BareTypeName(right->name);
    case MType::Ptr: return left->readOnly == right->readOnly && SameType(left->elem, right->elem);
    case MType::Func:
    {
        if (left->variadic != right->variadic) return false;
        if (left->paramTypes.size() != right->paramTypes.size()) return false;
        if (!SameType(left->elem, right->elem)) return false;
        for (std::size_t index = 0; index < left->paramTypes.size(); ++index)
        {
            if (!SameType(left->paramTypes[index], right->paramTypes[index])) return false;
        }
        return true;
    }
    case MType::Array:
        return (left->dims == right->dims) && SameType(left->elem, right->elem);
    default: return true;
    }
}

/**
 * @brief 类型助记符：重载后的函数 / 方法标签用它拼（`int64` / `float64` / `Money` / `int64_ptr`）。
 * @param type 类型描述。
 * @retval 生成或读取的结果文本。
 */
Memory::String TypeTag(const std::shared_ptr<MType> &type)
{
    if (!type) return "void";
    switch (type->kind)
    {
    case MType::Void: return "void";
    case MType::Bool: return "bool";
    case MType::Char: return "char";
    case MType::Int:
        if (type->bits == 64) return type->isUnsigned ? "uint64" : "int64";
        return Memory::String(type->isUnsigned ? "uint" : "int") + Memory::toString(type->bits);
    case MType::Double: return type->bits == 64 ? "float64" : "float32";
    case MType::String: return "string";
    case MType::Host: return "host_" + type->name;
    case MType::BigInt: return "bint";
    case MType::BigFloat: return "bfloat";
    case MType::Obj: return "Any";
    case MType::File: return "file";
    case MType::Proc: return "proc";
    case MType::Struct: return type->name;
    case MType::Container:
    {
        Memory::String tag = type->name + "_" + TypeTag(type->elem);
        if (type->elem2) tag += "_" + TypeTag(type->elem2);
        return tag;
    }
    case MType::Ptr: return TypeTag(type->elem) + (type->readOnly ? "_const_ptr" : "_ptr");
    case MType::Func:
    {
        Memory::String tag = TypeTag(type->elem) + "_fn";
        for (const std::shared_ptr<MType> &param : type->paramTypes) tag += "_" + TypeTag(param);
        return tag;
    }
    case MType::Array:
    {
        Memory::String tag = TypeTag(type->elem);
        for (const int dim : type->dims) tag += "_" + Memory::toString(dim);
        return tag + "_arr";
    }
    }
    return "void";
}

/**
 * @brief 重载之后的名字：**只在同名函数 / 方法多于一个时**才加 `$类型` 后缀。
 *
 * `int64 add(int64,int64)` 只有一个 → 标签还是 `add`（四元式好读）；
 * `max(int64,int64)` 与 `max(float64,float64)` 并存 → `max$int64$int64` / `max$float64$float64`。
 */
Memory::String MangledName(const Memory::String &source, const Memory::Vector<MParam> &params,
                           bool overloaded)
{
    if (!overloaded) return source;
    Memory::String label = source;
    for (const MParam &param : params)
        label += "$" + Memory::String(param.variadic ? "var_" : "") + TypeTag(param.type);
    return label;
}

/**
 * @brief 一组类型的可读列表（报错文案用）：`(int64, float64)`。
 * @param types 当前可见的类型定义集合。
 * @retval 生成或读取的结果文本。
 */
Memory::String DescribeTypes(const Memory::Vector<std::shared_ptr<MType>> &types)
{
    Memory::String text = "(";
    for (std::size_t index = 0; index < types.size(); ++index)
    {
        if (index != 0u) text += ", ";
        text += Describe(types[index]);
    }
    return text + ")";
}

/**
 * @brief 检查具体结构体引用是否可以直接形成接口视图。
 * @param target 目标接口引用，决定只读或可写权限。
 * @param source 源具体结构体引用。
 * @retval 源类型实现目标接口且不增加写权限时返回 true。
 * @note 这里只检查类型关系，借用来源与逃逸由现有生命周期检查处理。
 */
bool CanBindInterfaceReference(const std::shared_ptr<MType> &target, const std::shared_ptr<MType> &source)
{
    return IsInterfacePointer(target) && source && source->kind == MType::Ptr && !IsInterfacePointer(source) && source->elem && source->elem->kind == MType::Struct && (target->readOnly || !source->readOnly) && ImplementsInterface(source->elem->name, target->elem->name);
}

/**
 * @brief 判断具体类型是否具有 Any 支持的装箱表示。
 * @param type 待装箱的源类型。
 * @retval 类型可由现有装箱原语保存时返回 true。
 * @note 存储地址的寿命及引用逃逸仍由表达式降级阶段检查。
 */
bool AnyAcceptsType(const std::shared_ptr<MType> &type)
{
    if (!type) return false;
    switch (type->kind)
    {
    case MType::Int:
    case MType::Char:
    case MType::Bool:
    case MType::Double:
    case MType::String:
    case MType::Func:
    case MType::Struct:
    case MType::Array:
    case MType::Container:
    case MType::BigInt:
    case MType::BigFloat:
    case MType::Obj:
        return true;
    case MType::Ptr:
        return type->elem && type->elem->kind != MType::Void;
    default: return false;
    }
}

/**
 * @brief 形参能不能接住实参：完全一致最好，其次只允许 int64 → float64 的加宽。
 * @param param 目标形参的类型描述。
 * @param argument 待解析的参数文本。
 * @param widening 是否允许数值拓宽转换。
 * @retval 满足检查条件或操作完成时为 true，检查失败或无法完成时为 false。
 */
bool Acceptable(const std::shared_ptr<MType> &param, const std::shared_ptr<MType> &argument,
                bool widening)
{
    if (SameType(param, argument)) return true;
    if (param && argument && param->kind != MType::Ptr && argument->kind == MType::Ptr && !IsInterfacePointer(argument))
        return Acceptable(param, argument->elem, widening);
    /* 数组名退化成引用传参：`int m[8]` 可以喂给 `int p[]`（两者的值都是数据区基址） */
    if (param && argument && param->kind == MType::Ptr && argument->kind == MType::Array)
    {
        return SameType(param->elem, argument->elem);
    }
    if (!widening || !param || !argument) return false;
    /* 只允许隐式增加单层引用的只读限定；嵌套引用仍要求目标类型完全一致。 */
    if (param->kind == MType::Ptr && argument->kind == MType::Ptr && param->readOnly && !argument->readOnly && SameType(param->elem, argument->elem))
        return true;
    /* 数值加宽：整型按位宽升序，整型 → float32 → float64。
     * 整数之间一律按「目标值域装得下源值域」放行：
     *   同符号  —— 目标位宽不减即可（`int8 -> int64`、`uint8 -> uint32`）；
     *   无符号 -> 有符号 —— 目标必须**严格更宽**才装得下（`uint8 -> int16` 可以，
     *                        `uint8 -> int8` / `uint64 -> int64` 装不下）；
     *   有符号 -> 无符号 —— 一律要显式强转（负数塞不进无符号）。
     */
    if (param->kind == MType::Int && argument->kind == MType::Int)
    {
        if (param->isUnsigned == argument->isUnsigned) return param->bits >= argument->bits;
        return !param->isUnsigned && argument->isUnsigned && param->bits > argument->bits;
    }
    if (param->kind == MType::Double && argument->kind == MType::Double)
        return param->bits >= argument->bits;
    if (param->kind == MType::Double && argument->kind == MType::Int) return true;
    /* new 的待定元素类型由目标引用补全；不允许整数地址隐式转换。 */
    if (param->kind == MType::Ptr && argument->kind == MType::Ptr && argument->elem && argument->elem->kind == MType::Void)
    {
        return true;
    }
    if (CanBindInterfaceReference(param, argument)) return true;
    if (HasImplicitConversion(argument, param)) return true;
    return param->kind == MType::Obj && AnyAcceptsType(argument);
}

/**
 * @brief 为多值槽装箱建立在对象复制后仍有效的存储地址。
 *
 * A boxed multi-slot address must refer to storage that survives any copy of its box.
 * @param valueId 表示输入值的节点或类型编号。
 * @retval 满足检查条件或操作完成时为 true，检查失败或无法完成时为 false。
 */
bool StableBoxAddress(int valueId)
{
    if (valueId < 0) return false;
    const MNode &node = N(valueId);
    if (node.kind == MNode::Var)
    {
        const MVar *found = FindIn(node.module, node.s);
        if (found == nullptr || g_scope.empty()) return false;
        /* 得是**全局**（不是局部）才算稳定地址。 */
        const Memory::String key = node.module.empty() ? node.s : node.module + "::" + node.s;
        const auto global = g_scope.front().find(key);
        return global != g_scope.front().end() && found->emitted == global->second.emitted;
    }
    /* `模块::全局变量` 也是"数据区里稳定的一块"，可以直接取地址装箱。 */
    if (node.kind == MNode::EnumCtor && FindQualifiedGlobal(node.s, node.s2) != nullptr) return true;
    if ((node.kind == MNode::Member || node.kind == MNode::Index) && !node.kids.empty())
    {
        const std::shared_ptr<MType> base = ExprType(node.kids[0]);
        return !IsContainer(base) && !IsRuntimeArray(base) && StableBoxAddress(node.kids[0]);
    }
    return false;
}

/**
 * @brief 检查存储地址表达式的类型与可访问性。
 * @param valueId 表示输入值的节点或类型编号。
 * @param where 用于错误定位的源码节点。
 * @retval 满足检查条件或操作完成时为 true，检查失败或无法完成时为 false。
 */
bool CheckStoredAddress(int valueId, const Memory::String &where)
{
    const std::shared_ptr<MType> type = ExprType(valueId);
    if (!type || (type->kind != MType::Struct && type->kind != MType::Array)) return true;
    if (IsRuntimeArray(type))
        return Fail(where + " 不能保存运行时数组句柄：数组可能被释放；请显式存引用");
    if (StableBoxAddress(valueId)) return true;
    return Fail(where + " 不能保存局部或临时结构体/数组的地址：只允许全局对象；"
                        "需要手动管理生命周期时请显式存引用");
}

/**
 * @brief 校验容器操作参数与元素类型是否匹配。
 * @param expected 期望的类型或匹配规则。
 * @param valueId 表示输入值的节点或类型编号。
 * @param where 用于错误定位的源码节点。
 * @param stored 待存入容器的值类型。
 * @retval 满足检查条件或操作完成时为 true，检查失败或无法完成时为 false。
 */
bool CheckContainerArgument(const std::shared_ptr<MType> &expected, int valueId,
                            const Memory::String &where, bool stored)
{
    if (stored && expected && expected->kind != MType::Struct && expected->kind != MType::Ptr && !HasImplicitConversion(ExprType(valueId), expected) && !CheckStoredAddress(valueId, where)) return false;
    if (IsObjGetCall(valueId)) return true; /* get() uses the expected type at lowering. */
    const std::shared_ptr<MType> actual = expected && expected->kind == MType::Ptr ? RawExprType(valueId) : ExprType(valueId);
    if (expected && actual && (TypeKey(expected) == TypeKey(actual) || ((expected->kind != MType::Container && expected->kind != MType::Array) && Acceptable(expected, actual, true))))
        return true;
    return Fail(where + " 需要 " + Describe(expected) + "，实际是 " + Describe(actual));
}

/**
 * @brief 校验容器索引类型与访问要求。
 * @param valueId 表示输入值的节点或类型编号。
 * @param where 用于错误定位的源码节点。
 * @retval 满足检查条件或操作完成时为 true，检查失败或无法完成时为 false。
 */
bool CheckContainerIndex(int valueId, const Memory::String &where)
{
    const std::shared_ptr<MType> type = ExprType(valueId);
    if (type && type->kind == MType::Int) return true;
    return Fail(where + " 需要整数下标，实际是 " + Describe(type));
}

/**
 * @brief 算术结果的类型：浮点优先（float64 > float32），否则取更宽的整型
 *        （同宽时取无符号，与 C 的通常算术转换一致）。
 */
std::shared_ptr<MType> NumericResult(const std::shared_ptr<MType> &left,
                                     const std::shared_ptr<MType> &right)
{
    if (left->kind == MType::Double || right->kind == MType::Double)
    {
        const int leftBits = left->kind == MType::Double ? left->bits : 0;
        const int rightBits = right->kind == MType::Double ? right->bits : 0;
        return T(M_TypeFloatBits(std::max(leftBits, rightBits)));
    }
    if (left->kind == MType::Int && right->kind == MType::Int)
    {
        return T(M_TypeIntBits(std::max(left->bits, right->bits),
                               left->isUnsigned || right->isUnsigned));
    }
    return T(M_TypeInt());
}

/**
 * @brief "没有匹配的运算符重载"的统一报错（类型查询与降级两处共用同一句话）。
 * @param op 运算符文本或类别。
 * @param left 左操作数。
 * @param right 右操作数。
 * @retval 满足检查条件或操作完成时为 true，检查失败或无法完成时为 false。
 */
bool FailNoOverload(const Memory::String &op, const std::shared_ptr<MType> &left,
                    const std::shared_ptr<MType> &right)
{
    /* `&&` / `||` 是内建短路，压根不在运算符接口表里。以前文案会 fallback 到 `Add`，
     * 指向一个跟它毫无关系的接口 —— 这里单独说清为什么不能重载。 */
    if (op == "&&" || op == "||")
        return Fail("`" + op + "` 是短路运算符，不参与运算符重载：" + Describe(left) + " " + op + " " + Describe(right) + "（短路要先决定左边，与运算符重载「先求两个操作数再调方法」的模型对不上；"
                                                                                                                          "要自定义请写成普通方法）");
    const MOperatorInterfaceSpec *spec = BinaryOperatorSpec(op);
    if (spec != nullptr)
    {
        /* 右操作数那侧表过态时，报错要说清是被谁挡的 —— 否则用户只看到「没有匹配」，
         * 会以为是漏写了左接口。 */
        const MOperatorRightDirection direction = RightDirectionOf(*spec, right);
        if (direction == MOperatorRightDirection::Forbidden)
            return Fail("右向被显式禁止：" + Describe(right) + " 实现了 No" + spec->rightInterface + "，" + Describe(left) + " " + op + " " + Describe(right) + " 不再推导；要去掉该标记接口，或改为实现 " + spec->rightInterface + "<R,T>");
        if (direction == MOperatorRightDirection::ExplicitRight)
            return Fail("右向归 " + Describe(right) + " 自己实现：它声明了 " + spec->rightInterface + "，" + Describe(left) + " " + op + " " + Describe(right) + " 对不上它的形参，不再借用 " + spec->leftInterface + " 兜底（改成能接住左操作数的形参，或写成显式强转）");
    }
    /* 文案里的接口名跟着运算符走：`-` 该说 Subtract / RightSubtract，不能一律说成 Add。 */
    Memory::String leftInterface("Add"), rightInterface("RightAdd");
    if (spec != nullptr)
    {
        leftInterface = spec->leftInterface;
        rightInterface = spec->rightInterface;
    }
    return Fail("没有匹配的运算符接口实现：" + Describe(left) + " " + op + " " + Describe(right) + "（由左操作数实现 " + leftInterface + "<R,T> 这类左接口，或由右操作数实现 " + rightInterface + "<R,T> 这类右接口）；不会偷偷按数值算");
}

/**
 * @brief 找运算符重载：两个实参类型都要和某个签名的两个形参对得上。
 *
 * 先要求"完全一致"，再放宽到 int64 → float32 → float64（就是 Acceptable 的两档）。
 * 同一档里有两个签名都能接住时**不猜**，直接报错让用户先强转。
 *
 * @param label 输出：选中的四元式标签。
 * @param expected 调用点的目标类型；同一对操作数有多条声明时靠它挑。
 * @return 找到返回 true（没找到返回 false，由调用方决定要不要报错）。
 */
/**
 * @brief 运算符匹配的附加说明：命中的声明要按什么姿势调用。
 */
struct MOperatorMatch
{
    bool swap = false;    ///< 两个操作数要交换位置（交换律取到、或对偶推导的要求）。
    bool negate = false;  ///< 结果要取反（`a >= b` 借 `!(a < b)` 这类推导）。
};

/**
 * @brief 查找类型支持的运算符实现。
 * @param op 运算符文本或类别。
 * @param left 左操作数。
 * @param right 右操作数。
 * @param result 接收处理结果的对象。
 * @param match 接收匹配到的运算符候选。
 * @param expected 期望的类型或匹配规则。
 * @param requireUnique 是否要求运算符候选唯一。
 * @retval 满足检查条件或操作完成时为 true，检查失败或无法完成时为 false。
 */
bool FindOperator(const Memory::String &op, const std::shared_ptr<MType> &left,
                  const std::shared_ptr<MType> &right, const MOperatorDecl **result,
                  MOperatorMatch *match = nullptr, const std::shared_ptr<MType> *expected = nullptr,
                  bool requireUnique = true)
{
    if (match != nullptr) *match = MOperatorMatch{};
    enum class Outcome
    {
        Found,
        NotFound,
        Ambiguous
    };
    /* 按声明里的运算符文本找一次：`swap` 表示拿 (right, left) 去对声明的两个形参。 */
    const auto search = [&](const Memory::String &declOp, bool swap, bool negate) -> Outcome {
        for (int tier = 0; tier < 2; ++tier)
        {
            const bool widening = (tier == 1);
            Memory::Vector<const MOperatorDecl *> matched;
            for (const MOperatorDecl &candidate : g_operators)
            {
                if (candidate.op != declOp || candidate.right == nullptr) continue;
                if (!Acceptable(candidate.left, swap ? right : left, widening)) continue;
                if (!Acceptable(candidate.right, swap ? left : right, widening)) continue;
                matched.push_back(&candidate);
            }
            if (matched.size() == 1u)
            {
                *result = matched.front();
                if (match != nullptr)
                {
                    match->swap = swap;
                    match->negate = negate;
                }
                return Outcome::Found;
            }
            if (matched.size() > 1u)
            {
                /* 同一对操作数命中多条声明时，先看目标类型能不能挑出一个 —— 运算符接口
                 * 允许「同一对操作数、不同返回类型」（`Add<int64,int64>` 与
                 * `Add<Vec,int64>` 共存），这时只有上下文能定。 */
                if (expected != nullptr && *expected != nullptr)
                {
                    Memory::Vector<const MOperatorDecl *> narrowed;
                    for (const MOperatorDecl *candidate : matched)
                        if (SameType(candidate->result, *expected)) narrowed.push_back(candidate);
                    if (narrowed.size() == 1u)
                    {
                        *result = narrowed.front();
                        if (match != nullptr)
                        {
                            match->swap = swap;
                            match->negate = negate;
                        }
                        return Outcome::Found;
                    }
                }
                if (!requireUnique)
                {
                    /* 类型查询阶段先于「目标类型下传」到达（`RawExprType` 顶部就会推一次），
                     * 在那里报二义会把上下文其实定得下来的情形一起毙掉。所以宽松档只取
                     * 第一条让查询继续；真需要唯一声明的降级阶段用严格档（默认）再查一次，
                     * 那时 `expected` 已经是推断期钉住的类型，定不下来才是真二义。 */
                    *result = matched.front();
                    if (match != nullptr)
                    {
                        match->swap = swap;
                        match->negate = negate;
                    }
                    return Outcome::Found;
                }
                /* 文案报**返回类型**而不是内部修饰名：同一对操作数匹配多条时，真正要用户
                 * 决定的就是「要哪个结果类型」。 */
                Memory::Vector<Memory::String> outcomes;
                for (const MOperatorDecl *candidate : matched)
                    outcomes.push_back(Describe(candidate->result));
                Memory::String hint("（先强转再算；若它们只是返回类型不同，给表达式一个目标类型就能定）");
                if (expected != nullptr && *expected != nullptr)
                    hint = "，且没有一条的返回类型是 " + Describe(*expected) + "（补显式强转，或给表达式一个目标类型）";
                Fail("运算符 " + op + " 有多条声明匹配同一对操作数 " + Describe(left) + " / " + Describe(right) + "，返回类型分别是 " + JoinLabels(outcomes) + hint);
                return Outcome::Ambiguous;
            }
        }
        return Outcome::NotFound;
    };
    /* ① 直接匹配：声明里的运算符文本一样，操作数按原顺序对形参。 */
    const Outcome direct = search(op, false, false);
    if (direct == Outcome::Found) return true;
    if (direct == Outcome::Ambiguous) return false;
    /* 右操作数那侧自己表了态就不再兜底：`RightXxx` 表示右向归它，`NoRightXxx` 表示禁止。
     * 判定的是**用户写出来的这个运算符** —— `5 > a` 看 `NoRightGreater` / `RightGreater`，
     * 不管这条推导最终借的是哪个运算符的声明。 */
    const MOperatorInterfaceSpec *binary = BinaryOperatorSpec(op);
    const bool rightDirectionOpen = binary == nullptr || RightDirectionOf(*binary, right) == MOperatorRightDirection::Open;
    /* ② 交换律：`a + b` 里只有 b 那侧的类型实现了 `Add` 时，等价于 `b + a`
     * （`+ * == != & | ^` 才走这步，减法 / 除法 / 比较 / 移位不交换）。 */
    if (OperatorIsSymmetric(op) && rightDirectionOpen)
    {
        const Outcome swapped = search(op, true, false);
        if (swapped == Outcome::Found) return true;
        if (swapped == Outcome::Ambiguous) return false;
    }
    /* ③ 对偶推导：`a > b` 借 `<`、`a >= b` 借 `!(a < b)`、`a != b` 借 `!(a == b)` 等，
     * 表按优先级排（见 OperatorDerivations）。换边的条目同样受右向开关约束，只取反的
     * （`swap == false`）不换接收者，不受影响。 */
    for (const MOperatorDerivation &derivation : OperatorDerivations())
    {
        if (op != derivation.op) continue;
        if (derivation.swap && !rightDirectionOpen) continue;
        const Outcome derived = search(derivation.via, derivation.swap, derivation.negate);
        if (derived == Outcome::Found) return true;
        if (derived == Outcome::Ambiguous) return false;
    }
    return false;
}

/**
 * @brief 找**一元**运算符重载：`-x` / `~x` / `+x`。
 *
 * 只看"右边为空"的那些条目（二元条目的 right 非空），其余规则与 FindOperator 一致：
 * 先要求类型完全一致，再放宽一档；同一档里两个都能接住就报错不猜。
 *
 * @param op 运算符文本。
 * @param operand 操作数类型（也就是接收者）。
 * @return 找到返回声明；没找到返回 nullptr（调用方继续走内建降级）。
 */
const MOperatorDecl *FindUnaryOperator(const Memory::String &op,
                                       const std::shared_ptr<MType> &operand)
{
    for (int tier = 0; tier < 2; ++tier)
    {
        const MOperatorDecl *matched = nullptr;
        std::size_t count = 0u;
        for (const MOperatorDecl &candidate : g_operators)
        {
            if (candidate.right != nullptr) continue; /* 只认一元 */
            if (candidate.op != op) continue;
            if (!Acceptable(candidate.left, operand, tier == 1)) continue;
            matched = &candidate;
            ++count;
        }
        if (count == 1u) return matched;
        if (count > 1u)
        {
            Fail("一元运算符 " + op + " 有多个接口实现同样匹配（先强转再算）");
            return nullptr;
        }
    }
    return nullptr;
}

/**
 * @brief 查询用户运算符的实际返回类型，避免把借用结果当作拥有值。
 * @param exprId 运算表达式节点。
 * @return 匹配的返回类型，内建运算返回空。
 */
std::shared_ptr<MType> UserOperatorResultType(int exprId)
{
    const MNode &expr = N(exprId);
    if (expr.kind == MNode::Un && !expr.kids.empty() && (expr.s == "-" || expr.s == "+" || expr.s == "~"))
    {
        const auto operand = ExprType(expr.kids[0]);
        if (operand && (operand->kind == MType::Struct || IsEnumType(operand)))
            if (const auto *method = FindUnaryOperator(expr.s, operand)) return method->result;
    }
    if (expr.kind == MNode::Bin && expr.kids.size() >= 2u)
    {
        const auto left = ExprType(expr.kids[0]);
        const auto right = ExprType(expr.kids[1]);
        // 与类型推导及降级保持一致：字符串和大数优先使用内建运算。
        if (left->kind == MType::String || right->kind == MType::String || left->kind == MType::BigInt || right->kind == MType::BigInt || left->kind == MType::BigFloat || right->kind == MType::BigFloat) return nullptr;
        const MOperatorDecl *method = nullptr;
        /* 与推断、降级同一口径：带上这个节点钉住的类型，免得挑到另一条返回类型不同的声明
         * （挑错会让所有权判断按错的类型走）。 */
        std::shared_ptr<MType> expected;
        if (expr.type >= 0) expected = T(expr.type);
        if (FindOperator(expr.s, left, right, &method, nullptr, &expected, false)) return method->result;
    }
    return nullptr;
}
