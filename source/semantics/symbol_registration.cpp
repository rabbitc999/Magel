/* ---------- 第一遍：登记 ---------- */

/**
 * @brief 检查用户声明名称是否占用编译器保留名称。
 * @param name 名称文本。
 * @retval 满足检查条件或操作完成时为 true，检查失败或无法完成时为 false。
 */
bool CheckUserName(const Memory::String &name)
{
    if (name.rfind("__chali_", 0) == 0u && name.rfind("__chali_lambda_", 0) != 0u)
        return Fail("`__chali_` 前缀是编译器的运行时保留名：" + name);
    if (name.rfind("__std_", 0) == 0u
        && !(g_reductionLine > 0 && g_reductionLine <= g_sourceFiles.size()
             && g_sourceFiles[g_reductionLine - 1].rfind("<内建 std::", 0) == 0u))
        return Fail("`__std_` 前缀是标准库内部胶水的保留名：" + name);
    if (name == "self") return Fail("`self` 是方法接收者的保留名");
    return true;
}

/**
 * @brief 登记结构体声明上的运算符开关（`#No<接口>Replace` / `#NoRight<接口>`）。
 *
 * 两类开关都只关闭**自动推导**，直接写出与实现匹配的运算符不受影响：
 * - `#NoAddReplace`：换边（交换律）与对偶 / 取反（比较与相等）两处全部关掉；
 * - `#NoRightAdd`：只关「右操作数当接收者」那一条（交换律换边、右向推导）。
 *
 * 因此只有当前确实有对应推导可取消的接口才允许出现在指令里 —— 写
 * `#NoSubtractReplace` 会被拦下，因为 `-` 本来就没有自动推导可取消，
 * 右向要显式实现 `RightSubtract`；`#NoRightSubtract` 同理。
 *
 * @param top 结构体声明节点。
 * @param definition 正在登记的结构体定义。
 * @retval 全部指令合法时返回 true；接口名不认识、一元接口、无可取消的推导
 *         或同一接口重复登记时返回 false 并记下诊断。
 */
bool RegisterOperatorSwitches(const MNode &top, MStructDef &definition)
{
    const auto findSpec = [](const Memory::String &name) -> const MOperatorInterfaceSpec * {
        for (const MOperatorInterfaceSpec &candidate : OperatorInterfaceSpecs())
            if (name == Memory::String(candidate.leftInterface)) return &candidate;
        return nullptr;
    };
    for (const Memory::String &name : top.noReplace)
    {
        const MOperatorInterfaceSpec *spec = findSpec(name);
        if (spec == nullptr)
            return Fail("不认识的运算符接口：" + name + "（#No<接口>Replace 里的接口名写作 Add、Multiply、Less、LessOrEqual 等）");
        if (spec->unary)
            return Fail("一元运算符没有自动推导：#No" + name + "Replace（`-x` / `~x` / `+x` 只能直接实现 " + spec->leftInterface + "）");
        bool derivable = spec->symmetric;
        for (const MOperatorDerivation &derivation : OperatorDerivations())
            if (!derivable && name == Memory::String(derivation.via)) derivable = true;
        if (!derivable)
            return Fail("#No" + name + "Replace 没有可取消的自动推导：`" + spec->op + "` 不满足交换律，也没有对偶写法（需要右向请显式实现 " + spec->rightInterface + "）");
        if (!definition.noReplace.insert(name).second)
            return Fail("重复声明运算符推导开关：#No" + name + "Replace");
    }
    for (const Memory::String &name : top.noRight)
    {
        const MOperatorInterfaceSpec *spec = findSpec(name);
        if (spec == nullptr)
            return Fail("不认识的运算符接口：" + name + "（#NoRight<接口> 里的接口名写作 Add、Less、LessOrEqual 等）");
        if (spec->unary || spec->rightInterface == nullptr || spec->rightInterface[0] == '\0')
            return Fail("#NoRight" + name + " 没有可关闭的右向推导：`" + spec->op + "` 是" +
                        (spec->unary ? "一元运算符" : "内建运算符") + "，不存在右操作数当接收者的写法");
        if (!definition.noRight.insert(name).second)
            return Fail("重复声明运算符开关：#NoRight" + name);
    }
    return true;
}

/**
 * @brief 从函数体返回语句推导自动返回类型。
 * @param statementId 语句在节点池中的编号。
 * @param result 接收处理结果的对象。
 * @retval 满足检查条件或操作完成时为 true，检查失败或无法完成时为 false。
 */
bool InferAutoFunctionResult(int statementId, std::shared_ptr<MType> *result)
{
    const MNode &statement = N(statementId);
    if (statement.kind == MNode::Return)
    {
        if (statement.kids.empty()) return Fail("let 返回函数的 return 必须带值");
        const std::shared_ptr<MType> current = ExprType(statement.kids[0]);
        if (*result && !SameType(*result, current))
            return Fail("let 返回函数的多个 return 类型不一致：" + Describe(*result) + " 与 " + Describe(current));
        *result = current;
        return true;
    }
    if (statement.kind == MNode::Declare)
    {
        for (const int declId : N(statement.kids[0]).kids)
        {
            const MNode &decl = N(declId);
            if (decl.kids.empty() || decl.kids[0] < 0)
            {
                if (decl.type < 0) return Fail("let 变量必须给初值：" + decl.s);
                MVar binding{ T(decl.type), decl.s, decl.type2 != 0 };
                binding.bindingIdentity = decl.bindingIdentity;
                g_scope.back()[NodeBindingKey(decl)] = binding;
                continue;
            }
            const std::shared_ptr<MType> type = decl.type < 0 ? ExprType(decl.kids[0]) : T(decl.type);
            MVar binding{ type, decl.s, decl.type2 != 0 };
            binding.bindingIdentity = decl.bindingIdentity;
            g_scope.back()[NodeBindingKey(decl)] = binding;
        }
        return true;
    }
    if (statement.kind == MNode::Destructure)
    {
        if (statement.i) return true;
        auto type = ExprType(statement.kids[1]);
        std::function<bool(int, const std::shared_ptr<MType> &)> bind;
        bind = [&](int id, const std::shared_ptr<MType> &t) {const auto &p=N(id);
            if(p.kind==MNode::List){if(!IsTupleType(t)||p.kids.size()!=t->paramTypes.size())return Fail("元组解构的元素数量或嵌套形状不匹配");for(std::size_t i=0;i<p.kids.size();++i)if(!bind(p.kids[i],t->paramTypes[i]))return false;return true;}
            if(p.s!="_"){MVar binding{t,p.s,false};binding.bindingIdentity=p.bindingIdentity;g_scope.back()[NodeBindingKey(p)]=binding;}return true; };
        return bind(statement.kids[0], type);
    }
    if (statement.kind == MNode::Block)
    {
        PushScope();
        for (const int child : N(static_cast<int>(statement.i)).kids)
            if (!InferAutoFunctionResult(child, result))
            {
                PopScope();
                return false;
            }
        PopScope();
    }
    return true;
}

/**
 * @brief 取得 C ABI 使用的结构体名称。
 * @param name 名称文本。
 * @retval 生成或读取的结果文本。
 */
Memory::String CAbiStructName(const Memory::String &name)
{
    Memory::String result = "McAbi_";
    const char *hex = "0123456789abcdef";
    for (unsigned char ch : name)
    {
        result += hex[ch >> 4];
        result += hex[ch & 15];
    }
    return result;
}

/**
 * @brief 取得 C ABI 类型对应的底层表示。
 * @param type 类型描述。
 * @retval 对应的类型描述对象。
 */
std::shared_ptr<MType> CAbiUnderlying(std::shared_ptr<MType> type)
{
    while (type && type->kind == MType::Host) type = type->elem;
    return type;
}

/**
 * @brief ABI 可空指针只接受标准 Option 包装的单层引用。
 * @param type CHALI 类型。
 * @return 引用载荷类型，其他类型返回空。
 */
std::shared_ptr<MType> CAbiOptionalReference(const std::shared_ptr<MType> &type)
{
    const MEnumVariantDef *value = nullptr;
    if (!OptionVariants(CAbiUnderlying(type), &value, nullptr)) return nullptr;
    const auto reference = CAbiUnderlying(value->payloads[0].type);
    return reference && reference->kind == MType::Ptr ? reference : nullptr;
}

/**
 * @brief 将语言类型转换为 C ABI 类型表示。
 * @param type 类型描述。
 * @retval 生成或读取的结果文本。
 */
Memory::String CAbiType(const std::shared_ptr<MType> &type)
{
    if (!type) return {};
    if (IsStandardOpcode(type, "String")) return "const char *";
    if (const auto reference = CAbiOptionalReference(type)) return CAbiType(reference);
    switch (type->kind)
    {
    case MType::Void: return "void";
    case MType::Bool: return "bool";
    case MType::Char: return "std::uint8_t";
    case MType::Double: return type->bits == 32 ? "float" : "double";

    case MType::Int:
        if (type->bits == 8) return type->isUnsigned ? "std::uint8_t" : "std::int8_t";
        if (type->bits == 16) return type->isUnsigned ? "std::uint16_t" : "std::int16_t";
        if (type->bits == 32) return type->isUnsigned ? "std::uint32_t" : "std::int32_t";
        if (type->bits == 64) return type->isUnsigned ? "std::uint64_t" : "std::int64_t";
        return {};
    case MType::Func:
    {
        const auto scalar = [](const std::shared_ptr<MType> &part, bool result) {
            return part && (part->kind == MType::Int || part->kind == MType::Double || part->kind == MType::Bool || part->kind == MType::Char || (result && part->kind == MType::Void));
        };
        if (type->variadic || !scalar(type->elem, true)) return {};
        for (const auto &parameter : type->paramTypes)
            if (!scalar(parameter, false)) return {};
        return CAbiStructName("callback_" + TypeKey(type));
    }
    case MType::Host: return CAbiType(type->elem);
    case MType::Array:
    case MType::Ptr:
    {
        if (type->kind == MType::Array && !type->dims.empty()) return {};
        const auto target = CAbiUnderlying(type->elem);
        if (!target || CAbiOptionalReference(target) || target->kind == MType::Ptr || target->kind == MType::Func || target->kind == MType::Void || IsStandardOpcode(target, "String") || target->kind == MType::Array) return {};
        const auto element = CAbiType(type->elem);
        return element.empty() ? Memory::String() : element + (type->readOnly ? " const *" : " *");
    }
    case MType::Struct:
    {
        static Memory::Set<Memory::String> visiting;
        const auto found = g_structs.find(type->name);
        if (found == g_structs.end() || found->second.fields.empty() || !visiting.insert(type->name).second) return {};
        bool valid = true;
        for (const auto &field : found->second.fields)
            if (!CAbiUnderlying(field.type) || CAbiOptionalReference(field.type) || CAbiUnderlying(field.type)->kind == MType::Ptr || CAbiUnderlying(field.type)->kind == MType::Func || IsStandardOpcode(CAbiUnderlying(field.type), "String") || CAbiUnderlying(field.type)->kind == MType::Array || CAbiType(field.type).empty() || CAbiType(field.type) == "void") valid = false;
        visiting.erase(type->name);
        return valid ? CAbiStructName(type->name) : Memory::String();
    }
    default: return {};
    }
}

/**
 * @brief 在完整声明与导入已知后解析类型裸名，并隔离不同模块的同名类型。
 * @param registerOpcodes 是否同时登记 opcode 声明；延迟宏只归一化新增类型时为假。
 * @return 无歧义时返回 true；失败保留普通名称解析错误。
 */
bool ResolveImportedTypeNames(bool registerOpcodes = true)
{
    Memory::Map<Memory::String, Memory::Map<Memory::String, Memory::String>> declarations;
    Memory::Set<Memory::String> opcodeNames;
    for (const int id : g_top)
    {
        const auto &node = N(id);
        ReductionSourceScope source(id);
        if (node.kind != MNode::StructDecl && node.kind != MNode::EnumDecl && node.kind != MNode::InterfaceDecl && node.kind != MNode::HostTypeDecl && node.kind != MNode::OpcodeDecl) continue;
        const auto bare = BareTypeName(node.s);
        declarations[bare][node.module] = node.s;
        if (node.kind == MNode::OpcodeDecl) opcodeNames.insert(ModuleSymbol(node.module, bare));
    }
    for (auto &entry : declarations)
        for (auto &owner : entry.second)
            owner.second = entry.second.size() > 1 || opcodeNames.count(ModuleSymbol(owner.first, entry.first)) ? ModuleSymbol(owner.first, entry.first) : entry.first;
    const auto resolve = [&](const Memory::String &name, const Memory::String &module,
                             Memory::String *result) -> bool {
        const auto split = name.rfind("::");
        const auto bare = split == Memory::String::npos ? name : name.substr(split + 2);
        const auto known = declarations.find(bare);
        if (known == declarations.end())
        {
            *result = name;
            return true;
        }
        const auto &owners = known->second;
        if (split != Memory::String::npos)
        {
            const auto qualifier = name.substr(0, split);
            const auto exact = owners.find(qualifier == "__root" ? Memory::String() : qualifier);
            *result = exact == owners.end() ? name : exact->second;
            return true;
        }
        auto local = owners.find(module);
        if (local == owners.end()) local = owners.find("");
        if (local != owners.end())
        {
            *result = local->second;
            return true;
        }
        Memory::Set<Memory::String> imports{ "std::lang" };
        const auto imported = g_moduleImports.find(module);
        if (imported != g_moduleImports.end()) imports.insert(imported->second.begin(), imported->second.end());
        Memory::String chosen;
        for (const auto &owner : imports)
        {
            const auto found = owners.find(owner);
            if (found == owners.end()) continue;
            if (!chosen.empty() && chosen != found->second)
                return Fail("类型 " + name + " 有歧义；请使用作用域限定名");
            chosen = found->second;
        }
        *result = chosen.empty() ? name : chosen;
        return true;
    };
    for (const int id : g_top)
    {
        auto &node = g_nodes[id];
        ReductionSourceScope source(id);
        const auto found = declarations.find(BareTypeName(node.s));
        if (found == declarations.end()) continue;
        if (node.kind == MNode::StructDecl || node.kind == MNode::EnumDecl || node.kind == MNode::InterfaceDecl || node.kind == MNode::HostTypeDecl || node.kind == MNode::OpcodeDecl)
            node.s = found->second.at(node.module);
    }
    for (const int id : g_top)
    {
        const auto &node = N(id);
        if (!registerOpcodes || node.kind != MNode::OpcodeDecl) continue;
        ReductionSourceScope source(id);
        if (!CheckUserName(node.s)) return false;
        MOpcodeDefinition definition;
        definition.nonCopyable = node.nonCopyable;
        definition.module = node.module;
        Memory::Set<long long> operations;
        for (const int methodId : N(node.kids[0]).kids)
        {
            const auto &method = N(methodId);
            if (method.kind != MNode::OpcodeMethod)
            {
                if (method.kind == MNode::Field && method.type2 != 1) definition.aggregate = true;
                continue;
            }
            if (!operations.insert(method.i).second) return Fail("opcode 生命周期方法重复声明：" + node.s);
            Memory::String symbol = method.s;
            if (symbol.find("::") == Memory::String::npos && symbol.rfind("__std_", 0) != 0u)
                symbol = ModuleSymbol(node.module, symbol);
            if (method.i == 0)
                definition.create = symbol;
            else if (method.i == 1)
                definition.destroy = symbol;
            else
                definition.copy = symbol;
        }
        if (definition.create.empty() || definition.destroy.empty())
            return Fail("opcode 必须声明 new 和 drop 胶水方法：" + node.s);
        if (definition.nonCopyable && !definition.copy.empty())
            return Fail("#NonCopyable opcode 不能声明 copy 方法：" + node.s);
        if (!definition.nonCopyable && definition.copy.empty())
            return Fail("opcode 必须声明 copy 方法，或使用 #NonCopyable：" + node.s);
        if (!g_opcodeTypes.emplace(node.s, definition).second)
            return Fail("opcode 类型重复声明：" + node.s);
        if (definition.aggregate)
        {
            auto raw = definition;
            raw.aggregate = false;
            raw.internal = true;
            raw.handleType.clear();
            g_opcodeTypes[node.s + "$opcode"] = raw;
            g_opcodeTypes[node.s].handleType = node.s + "$opcode";
        }
    }
    Memory::Vector<std::shared_ptr<MType>> parsedTypes;
    Memory::Set<const MType *> parsedTypeNodes;
    Memory::Function<void(const std::shared_ptr<MType> &)> collectType;
    collectType = [&](const std::shared_ptr<MType> &type) {
        if (!type || !parsedTypeNodes.insert(type.get()).second) return;
        parsedTypes.push_back(type);
        collectType(type->elem);
        collectType(type->elem2);
        for (const auto &parameter : type->paramTypes) collectType(parameter);
        for (const auto &argument : type->genericArguments) collectType(argument);
    };
    for (const auto &type : g_types) collectType(type);
    for (const auto &type : parsedTypes)
    {
        if (!type || (g_macroSyntaxFiltering && !g_macroActiveTypes.count(type.get())) || !type->unresolvedName) continue;
        const auto original = type->name;
        if (!resolve(original, type->lookupModule, &type->name)) return false;
        if (type->name == original && original.find("::") == Memory::String::npos && declarations.count(original) == 0)
        {
            const int core = BuiltinTypeByName(original);
            if (core >= 0) *type = *T(core);
        }
        if (const auto opcode = g_opcodeTypes.find(type->name); opcode != g_opcodeTypes.end())
        {
            type->kind = opcode->second.aggregate ? MType::Struct : MType::Opcode;
        }
        if (FindGenericClosureDefinition(type->name)) *type = *GenericClosureType(type->name);
        type->unresolvedName = false;
    }
    for (auto &node : g_nodes)
    {
        if ((!g_macroSyntaxFiltering || g_macroActiveNodes.count(&node)) && node.kind == MNode::EnumCtor && declarations.count(BareTypeName(node.s)))
        {
            if (!resolve(node.s, node.module, &node.s)) return false;
        }
    }
    Memory::Set<const MType *> visited;
    Memory::Function<void(const std::shared_ptr<MType> &)> tuples;
    tuples = [&](const std::shared_ptr<MType> &type) {
        if (!type || !visited.insert(type.get()).second) return;
        tuples(type->elem);
        tuples(type->elem2);
        for (const auto &part : type->paramTypes) tuples(part);
        if (IsTupleType(type)) type->name = TupleInternalName(type->paramTypes);
    };
    for (const auto &type : g_types) tuples(type);
    return true;
}

/**
 * @brief 登记顶层类型、函数与变量声明。
 * @retval 满足检查条件或操作完成时为 true，检查失败或无法完成时为 false。
 */
bool RegisterTop()
{
    RefreshMacroActiveSyntax();
    for (const int topId : g_top)
    {
        const auto &top = N(topId);
        ReductionSourceScope source(topId);
        if (top.kind == MNode::ImportDecl) g_moduleImports[top.module].insert(top.s);
    }
    g_hasDefer = std::any_of(g_nodes.begin(), g_nodes.end(), [](const MNode &node) { return node.kind == MNode::Defer; });
    if (!ResolveImportedTypeNames() || !ValidateAnnotations()) return false;
    for (const auto &type : g_types)
        if ((!g_macroSyntaxFiltering || g_macroActiveTypes.count(type.get())) && !ResolveAstGenericType(type)) return false;
    g_associatedDefinitions.clear();
    g_associatedRequirements.clear();
    g_associatedMethods.clear();
    for (const auto topId : g_top)
    {
        const auto top = N(topId);
        if (top.kind != MNode::StructDecl && top.kind != MNode::EnumDecl && top.kind != MNode::InterfaceDecl) continue;
        for (const auto memberId : N(top.kids[0]).kids)
        {
            const auto member = N(memberId);
            if (member.kind == MNode::AssociatedDecl)
            {
                if (!g_associatedRequirements[top.s].insert(member.s).second)
                    return Fail("重复关联类型声明：" + top.s + "::" + member.s);
            }
            else if (member.kind == MNode::AssociatedDef)
            {
                if (!g_associatedDefinitions[top.s].emplace(member.s, T(member.type)).second)
                    return Fail("重复关联类型定义：" + top.s + "::" + member.s);
            }
        }
    }
    // 模块别名的处理维持原顺序；具体关联投影在类型登记前展开。
    const auto associatedTypeCount = g_types.size();
    for (std::size_t index = 0; index < associatedTypeCount; ++index)
    {
        const auto type = g_types[index];
        if (type && (!g_macroSyntaxFiltering || g_macroActiveTypes.count(type.get()))) *type = *ResolveAssociated(type);
        if (!g_error.empty()) return false;
    }
    /* 显式 `(T1, T2, ...)` 类型在解析期已经带着元素表；先合成匿名 struct，后面的
     * 字段、函数签名和全局声明便都能像普通值结构一样核验。 */
    for (const std::shared_ptr<MType> &type : g_types)
        if (!g_macroSyntaxFiltering || g_macroActiveTypes.count(type.get())) EnsureTupleDefinition(type);

    /* 数学常数由编译器直接给（不依赖宿主胶水）：`std::math::PI` / `std::math::E` 是
     * `std::math` 模块里的静态只读字段，键就是"模块::名字"。
     *
     * **只在这个模块真的被 import 时登记**：整份程序（含所有已加载模块）的 import
     * 都在 `g_top` 里，直接扫一遍就行；不登记的话，"没导入却能用"就破坏了
     * "移出的符号必须显式 import"这条口径。 */
    bool mathImported = false;
    for (const int topId : g_top)
    {
        const MNode top = N(topId);
        ReductionSourceScope source(topId);
        g_reductionLine = top.sourceLine;
        if (top.kind == MNode::ImportDecl && top.s == "std::math")
        {
            mathImported = true;
            break;
        }
    }
    if (mathImported)
    {
        g_staticFields["std::math::PI"] =
            MStaticField{ T(M_TypeDouble()), AllocateName("std::math::PI"), true, true,
                          3.141592653589793238462643383279502884 };
        g_staticFields["std::math::E"] =
            MStaticField{ T(M_TypeDouble()), AllocateName("std::math::E"), true, true,
                          2.718281828459045235360287471352662498 };
        for (const auto &name : { Memory::String("PI"), Memory::String("E") })
        {
            const auto &field = g_staticFields.at("std::math::" + name);
            g_scope.front()["std::math::" + name] = MVar{ field.type, field.emitted, true };
        }
    }

    /* ---- -0.5 宿主 ABI 类型：先登记并展开，后面的全局/函数签名才能直接使用 ---- */
    for (const int topId : g_top)
    {
        const MNode top = N(topId);
        ReductionSourceScope source(topId);
        if (top.kind != MNode::HostTypeDecl) continue;
        if (!CheckUserName(top.s)) return false;
        if (g_hostTypes.count(top.s) != 0u) return Fail("宿主类型重复声明：" + top.s);
        for (const int otherId : g_top)
        {
            const MNode &other = N(otherId);
            if (otherId == topId || other.s != top.s) continue;
            if (other.kind == MNode::StructDecl || other.kind == MNode::EnumDecl || other.kind == MNode::InterfaceDecl)
                return Fail("宿主类型与语言类型重名：" + top.s);
        }
        g_hostTypes[top.s] = T(top.type);
    }
    Memory::Function<bool(const std::shared_ptr<MType> &, Memory::Set<Memory::String> &)> resolveHostAbi;
    resolveHostAbi = [&](const std::shared_ptr<MType> &type,
                         Memory::Set<Memory::String> &resolving) -> bool {
        if (!type) return true;
        if (type->kind == MType::Struct)
        {
            const auto alias = g_hostTypes.find(type->name);
            if (alias != g_hostTypes.end())
            {
                const Memory::String aliasName = type->name;
                if (!resolving.insert(aliasName).second)
                    return Fail("宿主类型别名形成循环：" + aliasName);
                if (!resolveHostAbi(alias->second, resolving)) return false;
                const MType expanded = *alias->second;
                resolving.erase(aliasName);
                *type = expanded;
            }
        }
        if (!resolveHostAbi(type->elem, resolving)) return false;
        if (!resolveHostAbi(type->elem2, resolving)) return false;
        for (const std::shared_ptr<MType> &param : type->paramTypes)
            if (!resolveHostAbi(param, resolving)) return false;
        return true;
    };
    for (const auto &item : g_hostTypes)
    {
        Memory::Set<Memory::String> resolving;
        if (!resolveHostAbi(item.second, resolving)) return false;
    }
    for (const auto &item : g_hostTypes)
    {
        const MType::Kind kind = item.second->kind;
        if (kind != MType::Bool && kind != MType::Int && kind != MType::Double && !IsStandardOpcode(item.second, "String") && kind != MType::Ptr)
            return Fail("宿主类型 " + item.first + " 的 ABI 必须是 bool、整数、浮点、std::lang::String 或引用；复杂对象请用 uint64 句柄");
    }
    /* 泛型参数和字段中的宿主类型同样保留名义身份，并递归绑定 ABI。 */
    Memory::Set<const MType *> boundHostTypes;
    Memory::Function<void(const std::shared_ptr<MType> &)> bindHostType;
    bindHostType = [&](const std::shared_ptr<MType> &type) {
        if (!type || !boundHostTypes.insert(type.get()).second) return;
        if (type->kind == MType::Struct)
        {
            const auto alias = g_hostTypes.find(type->name);
            if (alias != g_hostTypes.end())
            {
                type->kind = MType::Host;
                type->elem = alias->second;
            }
        }
        bindHostType(type->elem);
        bindHostType(type->elem2);
        for (const auto &parameter : type->paramTypes) bindHostType(parameter);
        for (const auto &argument : type->genericArguments) bindHostType(argument);
    };
    for (const auto &type : g_types) bindHostType(type);

    /* ---- 0. 全局变量先占名字：降级名要求"全程序唯一"，形参与局部变量都排在它后面 ---- */
    for (const int topId : g_top)
    {
        const MNode top = N(topId);
        ReductionSourceScope source(topId);
        if (top.kind != MNode::Declare) continue;
        for (const int declId : N(top.kids[0]).kids)
        {
            const MNode &decl = N(declId);
            if (decl.kind != MNode::Declare) continue;
            if (!CheckUserName(decl.s)) return false;
            /* 全局按**模块限定名**占名字：模块里的 `x` 与 `mod::x` 都指同一个槽。
             * 根模块没有前缀，保持原样；裸名兜底只在还空着时登记，避免两个模块的同名
             * 全局互相覆盖（模块内解析一律走 `FindIn` 的限定名优先）。 */
            const Memory::String qualified = ModuleSymbol(decl.module, decl.s);
            g_globalNames[declId] = AllocateName(qualified);
            if (decl.type >= 0)
            {
                MVar global{ T(decl.type), g_globalNames[declId], decl.type2 != 0 };
                if (global.isConst && !decl.kids.empty()) global.constantExpression = decl.kids[0];
                g_scope[0][qualified] = global;
                if (decl.module.empty())
                    g_scope[0][decl.s] = global;
            }
        }
    }

    /* ---- 0.25 enum：登记变体，并合成供 simple_ast 布局使用的值结构 ---- */
    for (const int topId : g_top)
    {
        const MNode top = N(topId);
        ReductionSourceScope source(topId);
        if (top.kind != MNode::EnumDecl) continue;
        if (!CheckUserName(top.s)) return false;
        if (g_enums.count(top.s) != 0u || g_structs.count(top.s) != 0u)
            return Fail("enum 重名：" + top.s);
        for (const int otherId : g_top)
        {
            const MNode &other = N(otherId);
            if (otherId != topId && other.kind == MNode::StructDecl && other.s == top.s)
                return Fail("enum 和 struct 重名：" + top.s);
            if (other.kind == MNode::InterfaceDecl && other.s == top.s)
                return Fail("enum 和接口重名：" + top.s);
        }
        const Memory::Vector<int> variantIds = N(top.kids[0]).kids;
        if (variantIds.empty()) return Fail("enum 至少要有一个变体：" + top.s);
        MEnumDef definition;
        MStructDef storage;
        storage.fields.push_back(MField{ "__tag", T(M_TypeIntBits(32, 0)) });
        Memory::Set<Memory::String> variantNames;
        int tag = 0;
        for (const int variantId : variantIds)
        {
            const MNode &variantNode = N(variantId);
            if (!CheckUserName(variantNode.s)) return false;
            if (!variantNames.insert(variantNode.s).second)
                return Fail("enum " + top.s + " 的变体重名：" + variantNode.s);
            MEnumVariantDef variant;
            variant.name = variantNode.s;
            variant.tag = tag++;
            Memory::Set<Memory::String> payloadNames;
            if (!variantNode.kids.empty())
            {
                for (const int paramId : N(variantNode.kids[0]).kids)
                {
                    const MNode &param = N(paramId);
                    if (!param.kids.empty()) return Fail("enum 载荷不能使用默认参数");
                    if (param.i != 0)
                        return Fail("enum 载荷不能使用变长参数：" + top.s + "::" + variant.name);
                    if (!CheckUserName(param.s)) return false;
                    if (!payloadNames.insert(param.s).second)
                        return Fail("enum " + top.s + "::" + variant.name + " 的载荷字段重名：" + param.s);
                    const std::shared_ptr<MType> payloadType = T(param.type);
                    if (!payloadType || payloadType->kind == MType::Void)
                        return Fail("enum 载荷不能是 void：" + top.s + "::" + variant.name + "." + param.s);
                    const Memory::String storageName = "__v" + Memory::toString(variant.tag) + "_" + param.s;
                    variant.payloads.push_back(MEnumPayload{ param.s, storageName, payloadType });
                    storage.fields.push_back(MField{ storageName, payloadType });
                }
            }
            definition.variants.push_back(variant);
        }
        g_enums[top.s] = definition;
        g_structs[top.s] = storage;
    }

    /* ---- 0.5 接口：只包含方法，同名同签名的要求去重 ----
     *
     * 接口登记得比结构体早：结构体的继承列表要拿它核对"是不是都实现了"。
     * 接口名和结构体名共用一个名字空间（都写成"一个名字"当类型用），所以两边都要防重。 */
    for (const int topId : g_top)
    {
        const MNode top = N(topId);
        ReductionSourceScope source(topId);
        if (top.kind != MNode::InterfaceDecl) continue;
        if (!CheckUserName(top.s)) return false;
        if (g_interfaces.count(top.s) != 0u) return Fail("接口重名：" + top.s);
        for (const int otherId : g_top)
        {
            if (N(otherId).kind == MNode::StructDecl && N(otherId).s == top.s)
                return Fail("接口和结构体重名：" + top.s);
            if (N(otherId).kind == MNode::EnumDecl && N(otherId).s == top.s)
                return Fail("接口和 enum 重名：" + top.s);
        }
        Memory::Vector<MInterfaceMethod> required;
        for (const int memberId : N(top.kids[0]).kids)
        {
            const MNode &member = N(memberId);
            if (member.kind == MNode::AssociatedDecl) continue;
            if (member.kind == MNode::ParseBad) return Fail(member.s);
            if (!CheckUserName(member.s)) return false;
            MInterfaceMethod method;
            method.isStatic = member.type2 == 1;
            method.name = member.s;
            method.result = T(member.type);
            method.bodyId = member.kids.size() > 1u ? member.kids[1] : -1;
            method.module = top.module;
            method.owner = top.s;
            for (const int paramId : N(member.kids[0]).kids)
            {
                if (!N(paramId).kids.empty()) return Fail("接口方法暂不支持默认参数");
                if (N(paramId).i != 0)
                    return Fail("接口方法暂不支持变长参数：" + top.s + "." + member.s);
                const MNode &param = N(paramId);
                if (!CheckUserName(param.s)) return false;
                method.params.push_back(T(param.type));
                method.bodyParams.push_back(MParam{ param.s, Memory::String(), T(param.type), false });
                method.bodyParams.back().bindingKey = NodeBindingKey(param);
                method.bodyParams.back().bindingIdentity = param.bindingIdentity;
            }
            /* **去重**：同名同签名只算一条要求；同名**不同签名**是真冲突，直接报。 */
            bool duplicate = false;
            for (MInterfaceMethod &declared : required)
            {
                if (declared.name != method.name) continue;
                const Memory::String here =
                    InterfaceSignatureKey(method.name, method.result, method.params);
                const Memory::String there =
                    InterfaceSignatureKey(declared.name, declared.result, declared.params);
                if (here == there)
                {
                    if (declared.bodyId >= 0 && method.bodyId >= 0)
                        return Fail("接口 " + top.s + " 重复定义默认方法：" + here);
                    if (declared.bodyId < 0 && method.bodyId >= 0)
                        declared = method;
                    duplicate = true;
                    break;
                }
                return Fail("接口 " + top.s + " 里 " + method.name + " 有两个不同签名的声明：" + there + " 与 " + here);
            }
            if (!duplicate) required.push_back(method);
        }
        g_interfaces[top.s] = required;
        /* `interface B : A, C`：先记下基接口名，继承展开等所有接口都登记完再做（允许基接口
         * 声明在后面）。名字都按裸名，和 `struct X : A` 一个口径。 */
        Memory::Vector<Memory::String> bases;
        if (top.i >= 0)
        {
            for (const int nameId : N(static_cast<int>(top.i)).kids)
            {
                const Memory::String &base = g_names[static_cast<std::size_t>(nameId)];
                if (std::find(bases.begin(), bases.end(), base) == bases.end())
                    bases.push_back(base);
            }
        }
        if (!bases.empty()) g_interfaceBases[top.s] = bases;
        /* `Closeable` 的号在 `RegisterBuiltinTypeIds` 里固定在 10；这里不再排队，
         * 免得它的接口 / 引用号插在用户接口前面把 `__typeTag` 全推后。 */
        if (top.s != "Closeable") g_interfaceOrder.push_back(top.s);
    }

    /* ---- 0.6 接口继承：把基接口的方法并进派生接口（`g_interfaces` 存展开后的表）。 ---- */
    {
        Memory::Set<Memory::String> visiting;
        Memory::Set<Memory::String> done;
        for (const int topId : g_top)
        {
            const MNode top = N(topId);
            ReductionSourceScope source(topId);
            if (top.kind != MNode::InterfaceDecl) continue;
            if (!FlattenInterface(top.s, visiting, done)) return false;
        }
    }

    /* ---- 0.75 enum 方法与接口：变体仍负责构造，方法复用 struct 的 self/接口机制。 ---- */
    for (const int topId : g_top)
    {
        const MNode top = N(topId);
        ReductionSourceScope source(topId);
        if (top.kind != MNode::EnumDecl || top.kids.size() < 2u) continue;
        MStructDef &definition = g_structs[top.s];
        if (top.i >= 0)
        {
            for (const int nameId : N(static_cast<int>(top.i)).kids)
            {
                const Memory::String &iface = g_names[static_cast<std::size_t>(nameId)];
                if (g_interfaces.count(iface) == 0u)
                    return Fail("enum " + top.s + " 实现的接口没有声明：" + iface);
                if (std::find(definition.interfaces.begin(), definition.interfaces.end(), iface) == definition.interfaces.end()) definition.interfaces.push_back(iface);
            }
        }
        for (const int memberId : N(top.kids[1]).kids)
        {
            const MNode &member = N(memberId);
            if (member.kind != MNode::MethodDef)
                return Fail("enum " + top.s + " 内只能在变体后声明方法");
            if (member.type2 == 2)
                return Fail("enum " + top.s + " 不声明构造函数；请使用变体构造器");
            if (!CheckUserName(member.s)) return false;
            MMethod method;
            method.source = member.s;
            method.result = T(member.type);
            method.isStatic = member.type2 == 1;
            method.isNamedConstructor = method.isStatic && member.s == "create";
            method.isConstructor = method.isNamedConstructor;
            if (method.isNamedConstructor && !SameType(method.result, NamedType(top.s)))
                return Fail("static create 的返回类型必须是所属类型：" + top.s);
            method.bodyId = member.kids.size() > 1u ? member.kids[1] : -1;
            method.module = top.module;
            for (const int paramId : N(member.kids[0]).kids)
            {
                method.params.push_back(MParam{ N(paramId).s, Memory::String(), T(N(paramId).type),
                                                N(paramId).i != 0, N(paramId).kids.empty() ? -1 : N(paramId).kids[0] });
                method.params.back().bindingKey = NodeBindingKey(N(paramId));
                method.params.back().bindingIdentity = N(paramId).bindingIdentity;
            }
            if (!ValidateVariadicParameters(method.params,
                                            "enum 方法 " + top.s + "." + member.s))
                return false;
            definition.methods.push_back(method);
            g_nodes[static_cast<std::size_t>(memberId)].i =
                static_cast<long long>(definition.methods.size()) - 1;
        }
        if (!AddInterfaceDefaults(top.s, definition) || !AddDefaultToString(top.s, definition, top.module) || !VerifyInterfaces(top.s, definition, "enum")) return false;
    }

    /* ---- 1. struct：字段 + 普通方法（运算符重载是顶层特殊函数，第 2 步再挂上来） ---- */
    for (const int topId : g_top)
    {
        const MNode top = N(topId);
        ReductionSourceScope source(topId);
        if (top.kind == MNode::StructDecl || top.kind == MNode::OpcodeDecl)
        {
            if (!CheckUserName(top.s)) return false;
            if (g_enums.count(top.s) != 0u) return Fail("struct 和 enum 重名：" + top.s);
            if (g_structs.count(top.s) != 0u) return Fail("struct 重名：" + top.s);
            if (g_interfaces.count(top.s) != 0u) return Fail("struct 和接口重名：" + top.s);
            MStructDef definition;
            definition.abiPacked = top.abiPacked;
            const bool opcodeOwner = top.kind == MNode::OpcodeDecl;
            if (opcodeOwner && g_opcodeTypes.at(top.s).aggregate)
                definition.fields.push_back(MField{ "opcode", NamedType(g_opcodeTypes.at(top.s).handleType) });
            else if (opcodeOwner)
                definition.fields.push_back(MField{ "opcode", T(M_TypeInt()) });
            /* 继承的接口：`struct X : A, B`（同一个接口写两遍只算一遍）。 */
            if (!opcodeOwner && top.i >= 0)
            {
                for (const int nameId : N(static_cast<int>(top.i)).kids)
                {
                    const Memory::String &iface = g_names[static_cast<std::size_t>(nameId)];
                    if (g_interfaces.count(iface) == 0u)
                        return Fail("struct " + top.s + " 继承的接口没有声明：" + iface);
                    Memory::Vector<Memory::String> &list = definition.interfaces;
                    if (std::find(list.begin(), list.end(), iface) == list.end())
                        list.push_back(iface);
                }
            }
            if (!RegisterOperatorSwitches(top, definition)) return false;
            for (const int memberId : N(top.kids[0]).kids)
            {
                const MNode &member = N(memberId);
                if (member.kind == MNode::Field)
                {
                    if (opcodeOwner && member.s == "opcode") return Fail("opcode 是只读内部句柄的保留字段名");
                    if (!CheckUserName(member.s)) return false;
                    const Memory::String qualified = top.s + "::" + member.s;
                    if (member.type2 == 1)
                    {
                        if (g_staticFields.count(qualified) != 0u)
                            return Fail("struct " + top.s + " 的 static 字段重名：" + member.s);
                        g_staticFields[qualified] = MStaticField{ T(member.type), AllocateName(qualified) };
                        continue;
                    }
                    for (const MField &declared : definition.fields)
                    {
                        if (declared.name == member.s)
                            return Fail("struct " + top.s + " 的字段重名：" + member.s);
                    }
                    /* 字段类型不再限制：数据区每个槽位 8 字节，整型 / 浮点 / 句柄
                     * （struct / array / string / ptr）都放得下，`load` / `store` 按
                     * 元素的类型后缀区分整数与 IEEE-754 双精度。 */
                    std::shared_ptr<MType> field;
                    if (member.type < 0)
                    {
                        if (member.kids.size() != 1 || N(member.kids[0]).kind != MNode::Var || !FindGenericClosureDefinition(N(member.kids[0]).s))
                            return Fail("let 字段需要泛型闭包初始化式：" + member.s);
                        field = GenericClosureType(N(member.kids[0]).s);
                    }
                    else
                        field = T(member.type);
                    if (member.i >= 0)
                    {
                        Memory::Vector<int> dimensions;
                        if (!ReadDims(static_cast<int>(member.i), &dimensions)) return false;
                        field = ArrayType(field, dimensions);
                    }
                    definition.fields.push_back(MField{ member.s, field });
                    if (!member.kids.empty()) definition.fields.back().initializer = member.kids[0];
                    continue;
                }
                if (member.kind == MNode::MethodDef)
                {
                    if (opcodeOwner && (member.type2 == 2 || member.s == "drop" || member.s == "copy"))
                        return Fail("opcode 生命周期使用 new/drop/copy 绑定，附加方法不能替代生命周期入口");
                    if (!CheckUserName(member.s)) return false;
                    MMethod method;
                    method.isStatic = member.type2 == 1;
                    method.isConstructor = member.type2 == 2;
                    method.isNamedConstructor = !opcodeOwner && method.isStatic && member.s == "create";
                    method.isConstructor = method.isConstructor || method.isNamedConstructor;
                    method.bodyId = member.kids.size() > 1u ? member.kids[1] : -1;
                    method.module = top.module;
                    if (method.isConstructor && !method.isNamedConstructor && member.s != top.s)
                        return Fail("构造函数名必须与 struct 相同：" + top.s);
                    method.source = method.isConstructor && !method.isNamedConstructor ? "__ctor" : member.s;
                    method.result = T(member.type);
                    if (method.isNamedConstructor && !SameType(method.result, NamedType(top.s)))
                        return Fail("static create 的返回类型必须是所属类型：" + top.s);
                    for (const int paramId : N(member.kids[0]).kids)
                    {
                        method.params.push_back(
                            MParam{ N(paramId).s, Memory::String(), T(N(paramId).type),
                                    N(paramId).i != 0, N(paramId).kids.empty() ? -1 : N(paramId).kids[0] });
                        method.params.back().bindingKey = NodeBindingKey(N(paramId));
                        method.params.back().bindingIdentity = N(paramId).bindingIdentity;
                    }
                    if (!ValidateVariadicParameters(method.params,
                                                    "方法 " + top.s + "." + member.s))
                        return false;
                    definition.methods.push_back(method);
                    /* 记录方法在定义表里的下标：降级时按同一个下标取标签与签名。 */
                    g_nodes[static_cast<std::size_t>(memberId)].i =
                        static_cast<long long>(definition.methods.size()) - 1;
                }
            }
            /* 继承的接口**必须完全实现**：同名、同返回类型、同形参类型，一个都不能少。
             * 两个接口要的是同一个函数时只算一条（去重），实现一遍就够。 */
            if (!opcodeOwner && (!AddInterfaceDefaults(top.s, definition) || !AddDefaultToString(top.s, definition, top.module) || !VerifyInterfaces(top.s, definition))) return false;
            g_structs[top.s] = definition;
            continue;
        }
    }

    /* ---- 2.4 实现 Closeable 的结构体自动获得 #NonCopyable ----
     * 这类值持有外部句柄（文件、连接、宿主资源），复制一份就是两份所有权，`close()` 会跑两遍。
     * 接口继承（`interface Channel : Closeable`）同样算数，所以等全部接口与结构体登记完再统一判定。 */
    for (auto &item : g_structs)
        if (!item.second.nonCopyable && StructHasInterface(item.first, "Closeable"))
            item.second.nonCopyable = true;

    /* 旧 operator+ 自由函数已由左右方向的运算符接口取代。 */
    for (const int topId : g_top)
    {
        const MNode top = N(topId);
        ReductionSourceScope source(topId);
        if (top.kind != MNode::OperatorDef) continue;
        return Fail("`" + top.s + top.s2 + "(...)` 自由函数式运算符重载已禁用；"
                                           "请实现 Add<R,T> 这类左接口或 RightAdd<R,T> 这类右接口");
    }

    /* ---- 3. 方法标签：同名方法多于一个 → 都加 `$类型` 后缀；同名的同签名 → 报错 ---- */
    for (auto &item : g_structs)
    {
        MStructDef &definition = item.second;
        for (std::size_t index = 0; index < definition.methods.size(); ++index)
        {
            MMethod &method = definition.methods[index];
            if (method.isNamedConstructor)
                for (const auto &param : method.params)
                    if (param.variadic) return Fail("static new 构造入口暂不支持变长参数");
            /* 形参名字也全程序唯一（stackvm 的 decl 类型表按名字走）。 */
            for (std::size_t slot = 0; slot < method.params.size(); ++slot)
            {
                MParam &param = method.params[slot];
                if (param.source == "self") return Fail("`self` 是方法接收者的保留名");
                if (param.source.rfind("__chali_", 0) == 0u)
                    return Fail("`__chali_` 前缀是编译器的运行时保留名：" + param.source);
                for (std::size_t before = 0; before < slot; ++before)
                {
                    if (method.params[before].source == param.source)
                        return Fail("形参重名：" + param.source);
                }
                param.emitted = AllocateName(param.source);
            }
            std::size_t sameName = 0;
            for (const MMethod &other : definition.methods)
            {
                if (other.source == method.source) ++sameName;
            }
            method.name = MangledName(method.source, method.params, sameName > 1u);
            /* 运算符接口方法的 VIP：同一对操作数类型配不同返回类型在数学上合法
             * （`Add<int64,int64>` 与 `Add<Vec,int64>` 共存），而普通方法重载光靠返回
             * 类型分不开。这里把返回类型也写进修饰名，让两条都活下来 ——
             * `SameInterfaceSignature` 本来就比返回类型，挑哪条由调用点的期望类型定
             * （见 `FindOperator` 的 expected 参数）。 */
            if (IsOperatorInterfaceMethod(method.source) || method.source == "implicitConvert" || method.source == "explicitConvert")
            {
                bool resultDiffers = false;
                for (const MMethod &other : definition.methods)
                {
                    if (other.source != method.source || SameType(other.result, method.result))
                        continue;
                    if (other.params.size() != method.params.size()) continue;
                    bool sameParams = true;
                    for (std::size_t slot = 0; slot < method.params.size(); ++slot)
                        if (!SameType(other.params[slot].type, method.params[slot].type))
                            sameParams = false;
                    if (sameParams)
                    {
                        resultDiffers = true;
                        break;
                    }
                }
                if (resultDiffers) method.name += "$ret$" + TypeTag(method.result);
            }
            for (std::size_t before = 0; before < index; ++before)
            {
                if (definition.methods[before].name != method.name) continue;
                /* **重复定义：后面的覆盖前面的**（同一 struct 里重名同签名的方法）。 */
                definition.methods[before].superseded = true;
                Note("struct " + item.first + " 的方法 " + method.source + " 重复定义，后面的覆盖前面的");
                break;
            }
            MFuncSig signature;
            signature.result = method.result;
            signature.params = method.params;
            g_funcs[item.first + "." + method.name] = signature;
            if (method.isNamedConstructor)
            {
                signature.result = SimpleType(MType::Void);
                for (auto &param : signature.params) param.emitted = AllocateName(param.source);
                g_funcs[item.first + "." + method.name + "$body"] = signature;
            }
        }
    }

    /* ---- 3.5 运算符 / 转换接口：从已核验的方法和宿主胶水实现生成调用表 ---- */
    const auto addOperator = [&](const std::shared_ptr<MType> &owner,
                                 const Memory::String &interfaceName,
                                 const MInterfaceMethod &required,
                                 const Memory::String &label,
                                 const Memory::Vector<Memory::String> &callNames) -> bool {
        bool receiverLeft = true;
        const MOperatorInterfaceSpec *spec =
            OperatorInterfaceSpec(interfaceName, required.name, &receiverLeft);
        if (spec == nullptr) return true;
        if (required.params.size() != 1u)
            return Fail("内建运算符接口 " + interfaceName + " 必须正好有一个参数");
        MOperatorDecl declaration;
        declaration.op = spec->op;
        declaration.result = required.result;
        declaration.label = label;
        declaration.receiverLeft = receiverLeft;
        if (spec->unary)
        {
            /* 一元：接口方法那个唯一的参数**就是接收者**，没有第二个操作数；
             * CHALI 实现同时绑定隐式 self 与声明的操作数形参；宿主实现沿用单实参入口。 */
            declaration.left = owner;
            declaration.right = nullptr;
            declaration.callNames = g_funcs.count(label) != 0u ? callNames : Memory::Vector<Memory::String>{ callNames.empty() ? Memory::String("self") : callNames.front() };
        }
        else
        {
            declaration.left = receiverLeft ? owner : required.params[0];
            declaration.right = receiverLeft ? required.params[0] : owner;
            declaration.callNames = callNames;
        }
        g_operators.push_back(std::move(declaration));
        return true;
    };
    const auto addConversion = [&](const std::shared_ptr<MType> &owner,
                                   const Memory::String &interfaceName,
                                   const MInterfaceMethod &required,
                                   const Memory::String &label,
                                   const Memory::Vector<Memory::String> &callNames) -> bool {
        /* 按**方法名**认方向：`implicitConvert` / `explicitConvert`。接口可以同时继承
         * 两种转换（甚至不同目标类型），所以不能只看接口名挑一个。 */
        const bool implicit =
            required.name == "implicitConvert" && !InheritedInterfaceWithPrefix(interfaceName, "ImplicitConvert__").empty();
        const bool explicitOnly =
            required.name == "explicitConvert" && !InheritedInterfaceWithPrefix(interfaceName, "ExplicitConvert__").empty();
        if (!implicit && !explicitOnly) return true;
        if (!required.params.empty())
            return Fail("转换接口 " + interfaceName + " 的方法不能带参数");
        for (const MConversionDecl &old : g_conversions)
        {
            if (SameType(old.source, owner) && SameType(old.target, required.result) && old.implicit == implicit)
            {
                // 继承闭包重复发现同一个方法时只登记一次。
                if (old.label == label && old.callNames == callNames) return true;
                return Fail("转换接口重复：" + Describe(owner) + " -> " + Describe(required.result));
            }
        }
        g_conversions.push_back(
            MConversionDecl{ owner, required.result, label, callNames, implicit });
        return true;
    };
    const auto addIndex = [&](const std::shared_ptr<MType> &owner,
                              const Memory::String &interfaceName,
                              const MInterfaceMethod &required,
                              const Memory::String &label,
                              const Memory::Vector<Memory::String> &callNames) -> bool {
        if (InheritedInterfaceWithPrefix(interfaceName, "Index__").empty()) return true;
        if (required.name != "index") return true; /* 派生接口里别的方法 */
        if (required.params.size() != 1u)
            return Fail("Index<R,T> 必须声明 `R index(T key)`");
        g_indexes.push_back(MIndexDecl{ owner, required.params[0], required.result,
                                        label, callNames });
        return true;
    };
    const auto addHash = [&](const std::shared_ptr<MType> &owner,
                             const Memory::String &interfaceName,
                             const MInterfaceMethod &required,
                             const Memory::String &label,
                             const Memory::Vector<Memory::String> &callNames) -> bool {
        if (required.name != "hash" || InheritedInterfaceWithPrefix(interfaceName, "Hash__").empty()) return true;
        if (required.params.size() != 1u || !SameType(required.params[0], owner) || required.result->kind != MType::Int || required.result->bits != 64 || !required.result->isUnsigned)
            return Fail("Hash<T> 必须声明 `uint64 hash(T value)`，T 必须是实现类型");
        for (const auto &old : g_hashes)
            if (SameType(old.owner, owner)) return true;
        g_hashes.push_back(MHashDecl{ owner, label, callNames });
        return true;
    };
    const auto addIterator = [&](const std::shared_ptr<MType> &owner,
                                 const Memory::String &interfaceName,
                                 const MInterfaceMethod &required,
                                 const Memory::String &label,
                                 const Memory::Vector<Memory::String> &callNames) -> bool {
        if (InheritedInterfaceWithPrefix(interfaceName, "Iterator__").empty()) return true;
        if (required.name != "iterator") return true;
        if (required.params.size() != 1u || !SameType(required.params[0], T(M_TypeInt())))
            return Fail("Iterator<R> 必须声明 `Option<R> iterator(int64 index)`");
        const MEnumVariantDef *value = nullptr;
        if (!OptionVariants(required.result, &value, nullptr))
            return Fail("Iterator<R> 的 iterator 必须返回 Option<R>");
        g_iterators.push_back(MIteratorDecl{ owner, value->payloads[0].type, required.result,
                                             label, callNames });
        return true;
    };
    for (const auto &item : g_structs)
    {
        const std::shared_ptr<MType> owner = NamedType(item.first);
        for (const Memory::String &interfaceName : item.second.interfaces)
        {
            const auto interfaceIt = g_interfaces.find(interfaceName);
            if (interfaceIt == g_interfaces.end()) continue;
            for (const MInterfaceMethod &required : interfaceIt->second)
            {
                const MMethod *implementation = nullptr;
                for (const MMethod &method : item.second.methods)
                {
                    if (!method.superseded && SameInterfaceSignature(required, method))
                    {
                        implementation = &method;
                        break;
                    }
                }
                if (implementation == nullptr) continue; /* VerifyInterfaces 已给出主诊断。 */
                const Memory::String label = item.first + "." + implementation->name;
                Memory::Vector<Memory::String> callNames{ "self" };
                for (const MParam &param : implementation->params)
                    callNames.push_back(param.emitted);
                if (!addOperator(owner, interfaceName, required, label, callNames)) return false;
                if (!addConversion(owner, interfaceName, required, label, callNames)) return false;
                if (!addIndex(owner, interfaceName, required, label, callNames)) return false;
                if (!addIterator(owner, interfaceName, required, label, callNames)) return false;
                if (!addHash(owner, interfaceName, required, label, callNames)) return false;
            }
        }
    }
    for (const auto &entry : g_structs)
    {
        const auto owner = NamedType(entry.first);
        if (FindConversion(owner, T(M_TypeString()), false)) continue;
        for (const auto &method : entry.second.methods)
            if (!method.superseded && !method.isStatic && method.source == "toString" && method.params.empty() && IsStandardOpcode(method.result, "String"))
            {
                g_conversions.push_back(MConversionDecl{ owner, method.result, entry.first + "." + method.name, { "self" }, true });
                break;
            }
    }
    for (const int topId : g_top)
    {
        const MNode top = N(topId);
        ReductionSourceScope source(topId);
        if (top.kind != MNode::HostImplDecl) continue;
        const std::shared_ptr<MType> owner = T(top.type);
        const auto interfaceIt = g_interfaces.find(top.s2);
        if (interfaceIt == g_interfaces.end())
            return Fail("宿主 impl 的接口没有声明：" + top.s2);
        if (interfaceIt->second.empty())
            return Fail("宿主 impl 的接口没有方法：" + top.s2);
        for (const MHostInterfaceImpl &old : g_hostInterfaceImpls)
            if (SameType(old.owner, owner) && old.interfaceName == top.s2)
                return Fail("宿主接口实现重复：" + Describe(owner) + " : " + top.s2);
        const Memory::String function = g_names[static_cast<std::size_t>(top.i)];
        /* 多方法接口没法用一个符号盖住全部方法：约定方法 `m` 查 `<符号>.m`。
         * 单方法接口沿用原符号名，兼容既有 `impl Owner : Interface = symbol;` 写法
         * （协议接口 Add / ExplicitConvert 等本来就只有一条方法）。
         * 绑定入口见 `chali_glue.hpp` 的 `Registry::bindTypedMethod`。 */
        const bool qualified = interfaceIt->second.size() > 1u;
        g_hostInterfaceImpls.push_back(MHostInterfaceImpl{ owner, top.s2, function, qualified });
        for (const MInterfaceMethod &required : interfaceIt->second)
        {
            const Memory::String label =
                qualified ? function + "." + required.name : function;
            Memory::Vector<Memory::String> callNames{ "__chali_self" };
            for (std::size_t index = 0; index < required.params.size(); ++index)
                callNames.push_back("__chali_value" + Memory::toString(index));
            if (!addOperator(owner, top.s2, required, label, callNames)) return false;
            if (!addConversion(owner, top.s2, required, label, callNames)) return false;
            if (!addIndex(owner, top.s2, required, label, callNames)) return false;
            if (!addIterator(owner, top.s2, required, label, callNames)) return false;
            if (!addHash(owner, top.s2, required, label, callNames)) return false;
        }
    }

    for (const auto &implementation : g_hostInterfaceImpls)
    {
        if (FindConversion(implementation.owner, T(M_TypeString()), false)) continue;
        const MInterfaceMethod *required = nullptr;
        Memory::String label;
        if (FindHostInterfaceImplementation(implementation.owner, "toString", {}, &required, &label) && required && required->result && IsStandardOpcode(required->result, "String"))
            g_conversions.push_back(MConversionDecl{ implementation.owner, required->result, label, { "__chali_self" }, true });
    }

    /* ---- 4. 普通函数：先把同名的一起收齐，再按"是不是重载"分配标签 ---- */
    for (const int topId : g_top)
    {
        const MNode top = N(topId);
        ReductionSourceScope source(topId);
        if (top.kind != MNode::FuncDef && top.kind != MNode::ExternFuncDecl) continue;
        if (!top.generatedDeclaration && !CheckUserName(top.s)) return false;
        // 标准库小驼峰内建和其他 prelude 函数一样，允许用户函数遮蔽。
        MFunctionDecl declaration;
        declaration.source = top.s;
        declaration.module = top.module;
        declaration.captureSyntax = top.guard;
        for (const int paramId : N(top.kids[0]).kids)
        {
            declaration.signature.params.push_back(
                MParam{ N(paramId).s, Memory::String(), T(N(paramId).type),
                        N(paramId).i != 0, N(paramId).kids.empty() ? -1 : N(paramId).kids[0] });
            declaration.signature.params.back().bindingKey = NodeBindingKey(N(paramId));
            declaration.signature.params.back().bindingIdentity = N(paramId).bindingIdentity;
        }
        if (!ValidateVariadicParameters(declaration.signature.params,
                                        "函数 " + declaration.source))
            return false;
        declaration.external = top.kind == MNode::ExternFuncDecl;
        declaration.signature.result = top.type >= 0 ? T(top.type) : nullptr;
        if (!top.abiLibrary.empty())
        {
            declaration.abiLibrary = DecodeAbiLibraryPath(top.abiLibrary);
            if (!declaration.abiLibrary.empty())
            {
                namespace fs = std::filesystem;
                const auto library = Memory::u8path(declaration.abiLibrary);
                const bool driveAbsolute = declaration.abiLibrary.size() >= 3 && std::isalpha(static_cast<unsigned char>(declaration.abiLibrary[0])) && declaration.abiLibrary[1] == ':' && declaration.abiLibrary[2] == '/';
                if (!library.is_absolute() && !driveAbsolute && declaration.abiLibrary.compare(0, 2, "//") != 0)
                {
                    const auto base = top.abiSourceFile.empty() || top.abiSourceFile.front() == '<' ? fs::current_path() : fs::absolute(Memory::u8path(top.abiSourceFile)).parent_path();
                    declaration.abiLibrary = (base / library).lexically_normal().generic_u8string();
                }
            }
            declaration.abiRelease = UnescapeText(top.abiRelease);
            declaration.abiSymbol = top.s2.empty() ? chali_glue::conventionSymbol(ModuleSymbol(top.module, top.s)) : UnescapeText(top.s2);
            if (declaration.abiLibrary.find(static_cast<char>(0)) != Memory::String::npos || declaration.abiSymbol.find(static_cast<char>(0)) != Memory::String::npos || declaration.abiRelease.find(static_cast<char>(0)) != Memory::String::npos)
                return Fail("abi 库路径和符号不能包含 NUL 字节");
            if (declaration.abiLibrary.empty() || declaration.abiSymbol.empty())
                return Fail("abi 库路径与 C 符号名不能为空");
            if (!declaration.abiRelease.empty())
            {
                const auto result = CAbiUnderlying(declaration.signature.result);
                if (!IsStandardOpcode(result, "String") && result->kind != MType::Ptr && result->kind != MType::Array && !CAbiOptionalReference(declaration.signature.result))
                    return Fail("abi 释放函数只能用于字符串、引用或数组返回值");
            }
            if (CAbiUnderlying(declaration.signature.result)->kind == MType::Func || CAbiType(declaration.signature.result).empty())
                return Fail("abi 函数 " + declaration.source + " 的返回类型不是受支持的 C ABI 类型");
            if (CAbiUnderlying(declaration.signature.result)->kind == MType::Array)
            {
                for (std::size_t i = 0; i < declaration.signature.params.size(); ++i)
                    if (declaration.signature.params[i].source == top.abiLength && CAbiUnderlying(declaration.signature.params[i].type)->kind == MType::Int)
                        declaration.abiResultCountIndex = static_cast<int>(i);
                if (declaration.abiResultCountIndex < 0)
                    return Fail("abi 数组返回值的 [长度] 必须引用整数形参");
            }
            for (const MParam &param : declaration.signature.params)
                if (param.variadic || CAbiType(param.type).empty() || CAbiType(param.type) == "void")
                    return Fail("abi 函数 " + declaration.source + " 的参数必须是固定的 C ABI 标量、纯值结构体或单对象引用");
        }
        declaration.moduleLocal = top.moduleLocal;
        declaration.bodyId = declaration.external ? -1 : static_cast<int>(top.kids[1]);
        if (declaration.external)
        {
            if (declaration.source == "main") return Fail("main 不能只有宿主签名");
            if (!CheckDeclaredType(declaration.signature.result, declaration.source)) return false;
            for (const MParam &param : declaration.signature.params)
                if (!CheckDeclaredType(param.type, declaration.source + "." + param.source)) return false;
        }
        g_functionDecls.push_back(declaration);
        /* 记录声明在表里的下标：降级时按同一个下标取标签。 */
        g_nodes[static_cast<std::size_t>(topId)].i =
            static_cast<long long>(g_functionDecls.size()) - 1;
    }
    {
        /**
         * @brief 为 auto 返回推导临时提供完整重载目录，结束后恢复正式登记前的状态。
         */
        struct InferenceCatalogScope
        {
            decltype(g_funcs) functions = g_funcs;
            decltype(g_functionIndex) index = g_functionIndex;
            /**
             * @brief 恢复临时推导目录，正式登记随后分配最终形参名称。
             */
            ~InferenceCatalogScope()
            {
                g_funcs = std::move(functions);
                g_functionIndex = std::move(index);
            }
        } inferenceCatalog;
        Memory::Map<Memory::String, std::size_t> overloadCounts;
        for (const auto &declaration : g_functionDecls)
            ++overloadCounts[ModuleSymbol(declaration.module, declaration.source)];
        const auto registerInferenceSignature = [&](const MFunctionDecl &declaration) {
            const auto key = ModuleSymbol(declaration.module, declaration.source);
            const bool argumentsMain = declaration.source == "main" && !declaration.signature.params.empty();
            const auto label = declaration.source == "main" ? (argumentsMain ? Memory::String("__chali_main_arguments") : Memory::String("main")) : MangledName(key, declaration.signature.params, overloadCounts[key] > 1u);
            g_funcs[label] = declaration.signature;
            auto &candidates = g_functionIndex[key];
            if (std::find(candidates.begin(), candidates.end(), label) == candidates.end()) candidates.push_back(label);
        };
        for (const auto &declaration : g_functionDecls)
            if (declaration.signature.result) registerInferenceSignature(declaration);
        g_inferenceFunctions.clear();
        for (const MFunctionDecl &declaration : g_functionDecls)
            if (declaration.signature.result)
                g_inferenceFunctions[ModuleSymbol(declaration.module, declaration.source)] = declaration.signature;
        for (std::size_t reverse = g_functionDecls.size(); reverse > 0u; --reverse)
        {
            MFunctionDecl &declaration = g_functionDecls[reverse - 1u];
            if (declaration.signature.result) continue;
            if (declaration.source.rfind("__chali_lambda_", 0) == 0)
            {
                g_lambdaAutoReturns.insert(ModuleSymbol(declaration.module, declaration.source));
                declaration.signature.result = SimpleType(MType::Int);
                continue;
            }
            const MNode &body = N(declaration.bodyId);
            const Memory::Vector<int> &statements = N(static_cast<int>(body.i)).kids;
            PushScope();
            const Memory::String savedModule = g_currentModule;
            g_currentModule = declaration.module;
            for (const MParam &param : declaration.signature.params)
                g_scope.back()[param.source] = MVar{ param.type, param.source, true };
            std::shared_ptr<MType> inferred;
            for (const int statement : statements)
            {
                if (!InferAutoFunctionResult(statement, &inferred))
                {
                    g_currentModule = savedModule;
                    PopScope();
                    return false;
                }
            }
            g_currentModule = savedModule;
            PopScope();
            if (!inferred) return Fail("let 返回函数必须至少有一个带值 return：" + declaration.source);
            declaration.signature.result = inferred;
            g_inferenceFunctions[ModuleSymbol(declaration.module, declaration.source)] = declaration.signature;
            registerInferenceSignature(declaration);
        }
    }
    for (std::size_t index = 0; index < g_functionDecls.size(); ++index)
    {
        MFunctionDecl &declaration = g_functionDecls[index];
        std::size_t sameName = 0;
        for (const MFunctionDecl &other : g_functionDecls)
        {
            if (other.source == declaration.source && other.module == declaration.module) ++sameName;
        }
        if (declaration.external && sameName > 1u)
            return Fail("宿主函数不能重载或与 CHALI 函数同名（胶水层按名称绑定）：" + declaration.source);
        if (declaration.source == "main" && (!declaration.signature.result || (declaration.signature.result->kind != MType::Void && (declaration.signature.result->kind != MType::Int || declaration.signature.result->isUnsigned || declaration.signature.result->bits != 64))))
            return Fail("main 返回类型必须为 int64 或 void");
        const bool argumentsMain = declaration.source == "main" && !declaration.signature.params.empty();
        declaration.label = declaration.source == "main" ? (argumentsMain ? Memory::String("__chali_main_arguments") : Memory::String("main")) : MangledName(ModuleSymbol(declaration.module, declaration.source), declaration.signature.params, sameName > 1u);
        /* 形参名字全程序唯一（同方法里的那段同理：stackvm 的类型表按名字走）。 */
        for (std::size_t slot = 0; slot < declaration.signature.params.size(); ++slot)
        {
            MParam &param = declaration.signature.params[slot];
            if (param.source == "self") return Fail("`self` 是保留名：" + declaration.source);
            if (param.source.rfind("__chali_", 0) == 0u)
                return Fail("`__chali_` 前缀是编译器的运行时保留名：" + param.source);
            for (std::size_t before = 0; before < slot; ++before)
            {
                if (declaration.signature.params[before].source == param.source)
                    return Fail("函数 " + declaration.source + " 的形参重名：" + param.source);
            }
            param.emitted = AllocateName(param.source);
        }
        bool superseded = false;
        for (std::size_t before = 0; before < index; ++before)
        {
            if (g_functionDecls[before].label != declaration.label) continue;
            /* **重复定义：后面的覆盖前面的** —— 前面那条不再降级（函数体丢掉），
             * 标签与签名都沿用（两者本来就完全一样）。 */
            g_functionDecls[before].superseded = true;
            superseded = true;
            Note("函数 " + declaration.source + " 重复定义，后面的覆盖前面的");
            break;
        }
        if (argumentsMain)
        {
            const std::shared_ptr<MType> type = declaration.signature.params.size() == 1u ? declaration.signature.params[0].type : nullptr;
            if (!type || !IsContainer(type) || type->name != "vec" || !type->elem || !IsStandardOpcode(type->elem, "String") || declaration.signature.params[0].variadic)
            {
                return Fail("main 只允许无参数，或单个 std::lang::List<std::lang::String> 参数");
            }
        }
        g_funcs[declaration.label] = declaration.signature;
        if (!superseded)
            g_functionIndex[ModuleSymbol(declaration.module, declaration.source)].push_back(declaration.label);
        if (!superseded && declaration.source == "main" && !declaration.module.empty())
            g_functionIndex["main"].push_back(declaration.label);
    }
    /* 「有没有入口」是**构建 / 运行**时才成立的要求：多模块工程里单独检查一个模块文件，
     * 入口 main 在别的文件里，这里没有是完全正常的。所以 `--check`（只检查，不执行）
     * 不报这条，留给真正编译时报——编辑器里也不会因此一直挂红。 */
    if (g_functionIndex.find("main") == g_functionIndex.end() && !g_checkOnly)
        return Fail("chali 程序必须有一个 main 函数");
    /* 名字都登记完了，可以给类型发号了：`__typeTag` 与 `obj` 的类型号从此固定下来。 */
    for (const auto &entry : g_structs)
    {
        const auto reads = PropagationInterfaces(entry.first, "Try");
        const auto moves = PropagationInterfaces(entry.first, "TryMove");
        if (reads.size() > 1u || moves.size() > 1u) return Fail("同一个类型只能实现唯一的 Try/TryMove 成功与失败类型：" + entry.first);
        if (!reads.empty())
            for (const auto &requirement : g_interfaces[*reads.begin()])
                for (const auto &method : entry.second.methods)
                    if (!method.superseded && SameInterfaceSignature(requirement, method))
                    {
                        Memory::Set<Memory::String> visiting;
                        if (MethodMutatesReceiver(entry.first, method, &visiting)) return Fail("Try.branch 必须只读，移动或修改接收者请使用 TryMove.intoBranch：" + entry.first);
                    }
        if (!reads.empty() && !moves.empty())
        {
            const auto &read = g_interfaces[*reads.begin()];
            const auto &move = g_interfaces[*moves.begin()];
            if (read.size() != 1u || move.size() != 1u || !SameType(read[0].result, move[0].result))
                return Fail("Try 与 TryMove 必须具有相同的成功类型和传播载荷：" + entry.first);
        }
        for (const auto &protocol : PropagationInterfaces(entry.first, "FromResidual"))
            for (const auto &method : g_interfaces[protocol])
                if (!method.isStatic || method.params.size() != 1u || !SameType(method.result, NamedType(entry.first)))
                    return Fail("FromResidual<F,R> 的 F 必须是实现类型自身，且 fromResidual 必须是单参数静态方法：" + entry.first);
    }
    RegisterNamedTypeIds();
    const std::size_t parsedNodes = g_nodes.size();
    for (std::size_t id = 0; id < parsedNodes; ++id)
        ResolveBufferIndexShadowing(static_cast<int>(id));
    return true;
}
