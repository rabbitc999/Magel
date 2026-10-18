/* #derive 在语义入口读取字段元数据，把纯文本模板展开后交给同一生成解析器。 */

/**
 * @brief 运行派生方法的纯文本模板，不向宏引擎暴露类型或 AST。
 * @param definition 模板定义与调用文本。
 * @param output 接收展开后的普通 Magel 方法源码。
 * @retval 展开成功返回 true，失败保留编译诊断。
 */
bool ExpandDeriveTemplate(const Memory::String &definition, Memory::String &output)
{
    preprocess::Result result;
    result.text = definition;
    Memory::String error;
    if (!magel_macro::run(result, error)) return Fail("#derive 模板展开失败：" + error);
    output += result.text;
    return true;
}

/**
 * @brief 把每字段或每变体的源码片段作为独立宏尾参传入。
 * @param macro 模板名。
 * @param owner 被派生的类型名。
 * @param pieces 每字段或每变体的原始代码片段。
 * @retval 完整的文本宏调用。
 */
Memory::String DeriveInvocation(const Memory::String &macro, const Memory::String &owner,
                                const Memory::Vector<Memory::String> &pieces)
{
    Memory::String result = "$" + macro + "(" + owner;
    for (const auto &piece : pieces) result += ", " + piece;
    return result + ")\n";
}

/**
 * @brief 生成字段的哈希表达式，嵌套结构体只调用其已声明的 Hash 方法。
 * @param type 字段类型。
 * @param expression 字段读取源码。
 * @retval 普通表达式源码；不生成反射或运行时字段遍历。
 */
Memory::String DerivedHashExpression(const std::shared_ptr<MType> &type, const Memory::String &expression)
{
    if (type->kind == MType::Struct && (type->name == "std::lang::String" || (type->name == "String" && !magel_generic::coreAliasShadows.count("String"))))
        return "__magel_derive_hash_string(" + expression + ")";
    if (type->kind == MType::Struct) return expression + ".hash(" + expression + ")";
    if (type->kind == MType::String) return "__magel_derive_hash_string(" + expression + ")";
    if (type->kind == MType::Double) return "__magel_derive_hash_float(" + expression + ")";
    return "cast<uint64>(" + expression + ")";
}

/**
 * @brief 生成字段的显式克隆表达式，内建值沿用语言的复制语义。
 * @param type 字段类型。
 * @param expression 字段读取源码。
 * @retval 字段克隆表达式。
 */
Memory::String DerivedCloneExpression(const std::shared_ptr<MType> &type, const Memory::String &expression)
{
    if (type->kind == MType::Struct && (type->name == "std::lang::String" || (type->name == "String" && !magel_generic::coreAliasShadows.count("String")))) return expression;
    return type->kind == MType::Struct ? expression + ".clone()" : expression;
}

/**
 * @brief 构造枚举变体模式及其按序的局部绑定名。
 * @param owner 枚举类型名。
 * @param variant 变体声明。
 * @param prefix 绑定名的内部前缀。
 * @param bindings 输出各载荷绑定名。
 * @retval 枚举匹配模式。
 */
Memory::String DerivedVariantPattern(const Memory::String &owner, const MNode &variant,
                                     const Memory::String &prefix, Memory::Vector<Memory::String> &bindings)
{
    Memory::String result = owner + "::" + variant.s;
    if (variant.kids.empty()) return result;
    result += '(';
    for (std::size_t i = 0; i < N(variant.kids[0]).kids.size(); ++i)
    {
        if (i) result += ',';
        bindings.push_back(prefix + Memory::toString(i));
        result += bindings.back();
    }
    return result + ')';
}

/**
 * @brief 通过已有 $for/$concat 模板生成结构体或枚举的字段方法。
 * @param declaration 类型声明的语法节点。
 * @param trait Equal、Hash 或 Clone。
 * @param output 接收方法源码。
 * @retval 模板展开成功返回 true。
 */
bool DerivedMethodSource(const MNode &declaration, const Memory::String &trait, Memory::String &output)
{
    Memory::Vector<Memory::String> pieces;
    const auto owner = declaration.s;
    if (declaration.kind == MNode::StructDecl)
    {
        for (const auto fieldId : N(declaration.kids[0]).kids)
        {
            const auto field = N(fieldId);
            if (field.kind != MNode::Field) continue;
            if (field.i >= 0) return Fail("#derive 尚无数组字段的接口契约：" + owner + "." + field.s);
            if (trait == "Equal")
                pieces.push_back(field.s);
            else if (trait == "Hash")
                pieces.push_back("__magel_hash = (__magel_hash ^ " + DerivedHashExpression(T(field.type), "value." + field.s) + ") * 1099511628211u;");
            else
                pieces.push_back("__magel_clone." + field.s + " = " + DerivedCloneExpression(T(field.type), "self." + field.s) + ";");
        }
        if (trait == "Equal")
            return ExpandDeriveTemplate(
                "$macro __magel_derive_equal(Owner, ...Fields)\n"
                "bool $concat(left, Equal)(Owner other) { return true\n"
                "$for field in Fields\n && (self.field == other.field)\n$end_for\n; }\n$end_macro\n"
                    + DeriveInvocation("__magel_derive_equal", owner, pieces),
                output);
        if (trait == "Hash")
            return ExpandDeriveTemplate(
                "$macro __magel_derive_hash(Owner, ...Fields)\n"
                "uint64 hash(Owner value) { uint64 __magel_hash = 14695981039346656037u;\n"
                "$for field in Fields\nfield\n$end_for\nreturn __magel_hash; }\n$end_macro\n"
                    + DeriveInvocation("__magel_derive_hash", owner, pieces),
                output);
        return ExpandDeriveTemplate(
            "$macro __magel_derive_clone(Owner, ...Fields)\n"
            "Owner clone() { Owner __magel_clone;\n"
            "$for field in Fields\nfield\n$end_for\nreturn __magel_clone; }\n$end_macro\n"
                + DeriveInvocation("__magel_derive_clone", owner, pieces),
            output);
    }
    std::size_t tag = 0;
    for (const auto variantId : N(declaration.kids[0]).kids)
    {
        const auto variant = N(variantId);
        Memory::Vector<Memory::String> left, right;
        const auto pattern = DerivedVariantPattern(owner, variant, "magelDerivedLeft", left);
        const auto other = DerivedVariantPattern(owner, variant, "magelDerivedRight", right);
        Memory::String branch = pattern + " => { ";
        if (trait == "Equal")
        {
            branch += "match (other) { " + other + " => { return true";
            for (std::size_t i = 0; i < left.size(); ++i) branch += " && (" + left[i] + " == " + right[i] + ")";
            branch += "; } _ => { return false; } }";
        }
        else if (trait == "Hash")
        {
            branch += "uint64 __magel_hash = " + Memory::toString(++tag) + "u; ";
            for (std::size_t i = 0; i < left.size(); ++i)
            {
                const auto type = T(N(N(variant.kids[0]).kids[i]).type);
                branch += "__magel_hash = (__magel_hash ^ " + DerivedHashExpression(type, left[i]) + ") * 1099511628211u; ";
            }
            branch += "return __magel_hash;";
        }
        else
        {
            branch += "return " + owner + "::" + variant.s;
            if (!left.empty())
            {
                branch += '(';
                for (std::size_t i = 0; i < left.size(); ++i)
                {
                    if (i) branch += ',';
                    branch += DerivedCloneExpression(T(N(N(variant.kids[0]).kids[i]).type), left[i]);
                }
                branch += ')';
            }
            branch += ';';
        }
        pieces.push_back(branch + " }");
    }
    const auto signature = trait == "Equal" ? "bool leftEqual(Owner other)" : trait == "Hash" ? "uint64 hash(Owner value)" :
                                                                                                "Owner clone()";
    return ExpandDeriveTemplate(
        "$macro __magel_derive_enum(Owner, ...Variants)\n" + Memory::String(signature) + " { match (" + (trait == "Hash" ? Memory::String("value") : Memory::String("self")) + ") {\n"
                                                                                                                                                                               "$for variant in Variants\nvariant\n$end_for\n} }\n$end_macro\n"
            + DeriveInvocation("__magel_derive_enum", owner, pieces),
        output);
}

/**
 * @brief 为派生字段哈希提供普通数值与 UTF-8 字节算法，不使用动态反射。
 * @retval 可由生成解析器处理的普通函数源码。
 */
Memory::String DerivedHashHelpers()
{
    return R"MGL(
uint64 __magel_derive_hash_string(String value) {
    uint64 result = 14695981039346656037u;
    for (int64 index = 0; index < std::lang::len(value); index = index + 1) {
        result = (result ^ cast<uint64>(value[index])) * 1099511628211u;
    }
    return result;
}
uint64 __magel_derive_hash_float(float64 value) {
    if (value != value || value == 0.0) { return 0u; }
    if (value - value != 0.0) {
        if (value > 0.0) { return 9218868437227405312u; }
        return 18442240474082181120u;
    }
    float64 magnitude = value;
    uint64 sign = 0u;
    if (magnitude < 0.0) { magnitude = -magnitude; sign = 9223372036854775808u; }
    return cast<uint64>(magnitude % 18446744073709551616.0) ^ sign;
}
)MGL";
}

/**
 * @brief 在语义分析前将派生请求展开为普通方法及具体接口声明。
 * @retval 所有模板、具体接口与生成代码均有效时返回 true。
 * @note 只有派生语义层调用正常生成解析器；纯文本宏引擎不调用 Parser。
 */
bool ExpandDerivedDeclarations()
{
    Memory::Set<Memory::String> hashModules;
    for (const auto id : g_top)
        if (N(id).kind == MNode::FuncDef && N(id).s == "__magel_derive_hash_string") hashModules.insert(N(id).module);
    for (std::size_t index = 0; index < g_top.size(); ++index)
    {
        const auto id = g_top[index];
        const auto declaration = N(id);
        if (declaration.kind == MNode::ParseBad) return Fail(declaration.s);
        if (declaration.derives.empty() || declaration.derivesExpanded) continue;
        ReductionSourceScope location(id);
        Memory::String interfaces, methods, error;
        Memory::Vector<Memory::String> names;
        for (const auto &trait : declaration.derives)
        {
            const auto type = declaration.module.empty() ? declaration.s : declaration.module + "::" + declaration.s;
            const Memory::Vector<Memory::String> arguments = trait == "Equal" ? Memory::Vector<Memory::String>{ "bool", type } : Memory::Vector<Memory::String>{ type };
            Memory::String request = trait + "<";
            for (std::size_t i = 0; i < arguments.size(); ++i)
            {
                if (i) request += ',';
                request += arguments[i];
            }
            request += '>';
            if (!magel_generic::rewrite(request, magel_generic::session.generics, magel_generic::session.implemented,
                                        magel_generic::session.made, magel_generic::session.pending, error)
                || !magel_generic::instantiate(trait, arguments, interfaces, error)) return Fail("#derive 接口实例化失败：" + error);
            names.push_back(request);
            if (!DerivedMethodSource(declaration, trait, methods)) return false;
        }
        const auto module = declaration.module.empty() ? Memory::String("__root") : declaration.module;
        Memory::String source = interfaces + "\nmodule " + module + ";\n";
        if (std::find(declaration.derives.begin(), declaration.derives.end(), "Hash") != declaration.derives.end() && hashModules.insert(declaration.module).second)
            source += DerivedHashHelpers();
        const auto wrapper = "__magel_derive_members_" + Memory::toString(id);
        source += "\nstruct " + wrapper + " {\n" + methods + "\n}\n";
        const auto savedTop = g_top;
        const auto savedModule = g_parseModule;
        const auto first = g_nodes.size();
        magel::Engine parser;
        int value = 0;
        g_parseModule.clear();
        const auto status = parser.runText(source, &value);
        const auto added = g_top;
        g_top = savedTop;
        g_parseModule = savedModule;
        for (auto i = first; i < g_nodes.size(); ++i)
        {
            g_nodes[i].sourceLine = declaration.sourceLine;
            g_nodes[i].sourceColumn = declaration.sourceColumn;
            g_nodes[i].generatedDeclaration = true;
        }
        if (status != magel::Status::Ok) return Fail("#derive 生成的方法无法解析：" + declaration.s);
        auto &target = g_nodes[id];
        if (target.kind == MNode::EnumDecl && target.kids.size() == 1)
        {
            target.kids.push_back(M_List());
            target.i = -1;
        }
        const auto members = target.kids[target.kind == MNode::EnumDecl ? 1 : 0];
        for (const auto generated : added)
        {
            if (N(generated).kind == MNode::StructDecl && N(generated).s == wrapper)
            {
                for (const auto member : N(N(generated).kids[0]).kids)
                {
                    for (const auto existing : N(members).kids)
                        if (N(existing).kind == MNode::MethodDef && N(existing).s == N(member).s)
                            return Fail("#derive 不覆盖已有方法：" + declaration.s + "." + N(member).s);
                    M_ListAdd(members, member);
                }
            }
            else
                g_top.push_back(generated);
        }
        if (target.i < 0) target.i = M_List();
        for (const auto &name : names)
        {
            bool present = false;
            for (const auto old : N(static_cast<int>(target.i)).kids)
                if (g_names[old] == name) present = true;
            if (!present) M_ListAdd(static_cast<int>(target.i), M_Name(name.c_str()));
        }
        target.derivesExpanded = true;
    }
    return true;
}

/**
 * @brief 检查每个派生字段是否具有对应的语义接口，未使用的类型也必须检查。
 * @retval 所有字段均满足派生协议返回 true；缺失接口保留字段级诊断。
 */
bool ValidateDerivedFields()
{
    for (const auto id : g_top)
    {
        const auto declaration = N(id);
        if (declaration.derives.empty()) continue;
        ReductionSourceScope location(id);
        Memory::Vector<std::pair<Memory::String, std::shared_ptr<MType>>> fields;
        if (declaration.kind == MNode::EnumDecl)
        {
            const auto found = g_enums.find(declaration.s);
            if (found == g_enums.end()) continue;
            for (const auto &variant : found->second.variants)
                for (const auto &field : variant.payloads) fields.push_back({ variant.name + "." + field.name, field.type });
        }
        else if (const auto definition = FindStruct(declaration.s))
            for (const auto &field : definition->fields) fields.push_back({ field.name, field.type });
        for (const auto &trait : declaration.derives)
            for (const auto &field : fields)
            {
                const auto type = field.second;
                if (type->kind == MType::Int || type->kind == MType::Double || type->kind == MType::Bool || type->kind == MType::Char || type->kind == MType::String) continue;
                bool implemented = false;
                if (type->kind == MType::Struct)
                {
                    if (trait == "Equal")
                        for (const auto &op : g_operators)
                            if (op.op == "==" && SameType(op.left, type) && SameType(op.right, type) && op.result->kind == MType::Bool) implemented = true;
                    if (trait == "Hash")
                        for (const auto &hash : g_hashes)
                            if (SameType(hash.owner, type)) implemented = true;
                    if (trait == "Clone")
                        if (const auto definition = FindStruct(type->name))
                            for (const auto &interface : definition->interfaces)
                                if (!InheritedInterfaceWithPrefix(interface, "Clone__").empty()) implemented = true;
                }
                if (!implemented) return Fail("#derive(" + trait + ") 的字段 " + declaration.s + "." + field.first + " 未实现 " + trait + "：" + Describe(type));
            }
    }
    return true;
}
