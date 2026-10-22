/**
 * @file reflection_calls.cpp
 * @brief 生成精确类型的普通 CHALI 调用桥接，不保存或转换函数地址。
 */
/**
 * @brief 封闭值调用生成器，资源转移与接口分派复用现有语言语义。
 */
struct MReflectionCalls
{
    MReflectionQueries &queries;
    MReflectionBuilder &metadata;
    /**
     * @brief 返回封闭值变体名，空串表示不支持。
     * @param type 具体类型。
     * @return 变体名。
     */
    static Memory::String tag(const std::shared_ptr<MType> &type)
    {
        if (!type) return "Empty";
        if (IsStandardOpcode(type, "String")) return "Text";
        switch (type->kind)
        {
        case MType::Void: return "Empty";
        case MType::Bool: return "Bool";
        case MType::Char: return "Char";

        case MType::Int: return Memory::String(type->isUnsigned ? "U" : "I") + Memory::toString(type->bits);
        case MType::Double: return "F" + Memory::toString(type->bits);
        default: return {};
        }
    }
    /**
     * @brief 一条已编译调用候选。
     */
    struct Candidate
    {
        Memory::String name, expression;
        std::shared_ptr<MType> result;
        Memory::Vector<MParam> params;
        bool allowed = true;
        Memory::String fixedArgs;
        Memory::String receiverOperand;
    };
    /**
     * @brief 确认身份在显式暴露表中。
     * @param owner 内部类型名。
     * @return 是否暴露。
     */
    bool exposed(const Memory::String &owner)
    {
        auto id = metadata.typeId(NamedType(owner));
        for (const auto &v : metadata.table.structs)
            if (v.typeId == id) return true;
        for (const auto &v : metadata.table.enums)
            if (v.typeId == id) return true;
        for (const auto &v : metadata.table.interfaces)
            if (v.typeId == id) return true;
        return false;
    }
    /**
     * @brief 生成保持静态类型与拥有关系的字段读取或赋值桥接。
     * @param decl 已单态化的反射入口。
     * @param owner 已解析的接收类型名。
     * @param write 是否生成可写字段访问。
     */
    void generateField(const MFunctionDecl &decl, const Memory::String &owner, bool write)
    {
        const auto result = InferenceTypeText(decl.signature.result);
        Memory::String body = result + " " + decl.source + "(";
        for (std::size_t index = 0; index < decl.signature.params.size(); ++index)
        {
            if (index) body += ",";
            body += InferenceTypeText(decl.signature.params[index].type) + " " + decl.signature.params[index].source;
        }
        body += "){";
        const auto error = [&](const char *name) {
            return "return " + result + "::Err(std::reflect::ReflectError::" + name + ");";
        };
        if (!exposed(owner))
            body += error("NotExposed");
        else
        {
            const auto *definition = FindStruct(owner);
            std::shared_ptr<MType> expected;
            if (write)
                expected = decl.signature.params.back().type;
            else
            {
                const auto enumeration = g_enums.find(decl.signature.result->name);
                if (enumeration != g_enums.end())
                    for (const auto &variant : enumeration->second.variants)
                        if (variant.name == "Ok" && !variant.payloads.empty()) expected = variant.payloads.front().type;
            }
            if (definition && g_enums.count(owner) == 0)
                for (const auto &field : definition->fields)
                {
                    body += "if(name==" + queries.quote(field.name) + "){";
                    if (!expected || !SameType(expected, field.type))
                        body += error("TypeMismatch");
                    else if (field.type->kind == MType::Ptr || field.type->kind == MType::Func
                             || field.type->kind == MType::Array || HasNonCopyableValue(field.type))
                        body += error("UnsupportedSignature");
                    else if (write)
                        body += "obj." + field.name + "=value;return " + result + "::Ok(true);";
                    else
                        body += "return " + result + "::Ok(obj." + field.name + ");";
                    body += "}";
                }
            body += error("NotFound");
        }
        queries.functions += body + "}\n";
        queries.replaced.insert(decl.source);
    }
    /**
     * @brief 生成一个已使用的具体入口。
     * @param decl 占位声明。
     * @param api 原始 API 名。
     * @param owner 具体接收类型。
     */
    void generate(const MFunctionDecl &decl, const Memory::String &api, const Memory::String &owner)
    {
        const bool free = api == "call", constructor = api == "construct";
        const bool isInterface = api == "callInterface" || api == "callInterfaceConst";
        const bool isConst = api.find("Const") != Memory::String::npos;
        const bool isStatic = api == "callStatic" || api == "callEnumStatic";
        const bool isEnum = api.find("Enum") != Memory::String::npos;
        const auto result = InferenceTypeText(decl.signature.result);
        auto err = [&](const char *error) { return "return " + result + "::Err(std::reflect::ReflectError::" + error + ");"; };
        Memory::String body = result + " " + decl.source + "(";
        for (std::size_t i = 0; i < decl.signature.params.size(); ++i)
        {
            if (i) body += ",";
            body += InferenceTypeText(decl.signature.params[i].type) + " " + decl.signature.params[i].source;
        }
        body += "){";
        Memory::Vector<Candidate> candidates;
        if (free)
        {
            for (int id : g_top)
            {
                const auto &node = N(id);
                if (node.kind != MNode::FuncDef || !metadata.exposed(node)) continue;
                const auto &fn = g_functionDecls.at(static_cast<std::size_t>(node.i));
                if (fn.superseded) continue;
                auto name = ModuleSymbol(fn.module, fn.source);
                // 在声明模块中绑定目标，避免 reflect 同名 API 抢占根模块函数。
                Memory::String target = name;
                bool supported = !tag(fn.signature.result).empty();
                for (const auto &param : fn.signature.params)
                    if (tag(param.type).empty() || param.variadic) supported = false;
                if (supported)
                {
                    auto helper = "__reflect_invoke" + Memory::toString(queries.serial++);
                    auto source = "module " + (fn.module.empty() ? Memory::String("__root") : fn.module) + ";\n" + InferenceTypeText(fn.signature.result) + " " + helper + "(";
                    Memory::String arguments;
                    for (std::size_t i = 0; i < fn.signature.params.size(); ++i)
                    {
                        if (i)
                        {
                            source += ",";
                            arguments += ",";
                        }
                        auto argument = "argument" + Memory::toString(i);
                        source += InferenceTypeText(fn.signature.params[i].type) + " " + argument;
                        arguments += argument;
                    }
                    source += "){";
                    if (tag(fn.signature.result) != "Empty") source += "return ";
                    source += fn.source + "(" + arguments + ");";
                    if (tag(fn.signature.result) == "Empty") source += "return;";
                    queries.helpers += source + "}\nmodule std::reflect;\n";
                    target = ModuleSymbol(fn.module, helper);
                }
                candidates.push_back({ name, target, fn.signature.result, fn.signature.params, true });
            }
        }
        else if (!exposed(owner))
            body += err("NotExposed");
        else if (isInterface)
        {
            if (!g_interfaces.count(owner))
            {
                Fail("callInterface 只接受已单态化的接口类型");
                return;
            }
            // 一个接口调用永远通过普通接口分派；这里仅检查实际实现的权限。
            body += "int64 actual=__reflect_actualType<" + owner + ">(obj);";
            Memory::String known = "false";
            for (const auto &entry : g_structs)
            {
                if (!exposed(entry.first) || !StructHasInterface(entry.first, owner)) continue;
                auto condition = "(actual==__typeTag(" + entry.first + " mut&) || actual==__typeTag(" + entry.first + " const&))";
                known += " || " + condition;
                if (isConst)
                    for (const auto &method : entry.second.methods)
                    {
                        if (method.superseded || method.isStatic || method.isConstructor) continue;
                        Memory::Set<Memory::String> visiting;
                        if (MethodMutatesReceiver(entry.first, method, &visiting))
                            body += "if(" + condition + " && name==" + queries.quote(method.source) + "){" + err("ReceiverMismatch") + "}";
                    }
            }
            body += "if(!(" + known + ")){" + err("NotExposed") + "}";
            for (const auto &method : g_interfaces.at(owner))
            {
                candidates.push_back({ method.name, "obj." + method.name, method.result, method.bodyParams, true });
                bool left = true;
                const auto spec = OperatorInterfaceSpec(owner, method.name, &left);
                if (spec && spec->unary)
                {
                    auto &candidate = candidates.back();
                    candidate.params.clear();
                    candidate.expression = "__reflect_unary<" + owner + "," + InferenceTypeText(method.result) + ">";
                    candidate.fixedArgs = "obj," + queries.quote(method.name);
                    if (method.params.size() == 1 && method.params[0]->kind == MType::Struct)
                        candidate.receiverOperand = method.params[0]->name;
                    else
                        candidate.receiverOperand = "!unsupported";
                }
            }
        }
        else
        {
            if (!g_structs.count(owner) || (((g_enums.count(owner) != 0) != isEnum) && !constructor))
            {
                Fail(CHALI_TEXT("反射调用 API 与具体结构体或枚举类型不匹配"));
                return;
            }
            for (const auto &method : g_structs.at(owner).methods)
            {
                if (method.superseded || method.isConstructor != constructor || (!constructor && method.isStatic != isStatic)) continue;
                Memory::Set<Memory::String> visiting;
                bool allowed = !isConst || !MethodMutatesReceiver(owner, method, &visiting);
                candidates.push_back({ constructor ? "new" : method.source,
                                       constructor ? "new " + owner : isStatic ? owner + "::" + method.source :
                                                                                 "obj." + method.source,
                                       method.result, method.params, allowed });
                if (!constructor && !isStatic)
                    for (const auto &entry : g_interfaces)
                    {
                        if (!StructHasInterface(owner, entry.first)) continue;
                        bool left = true;
                        const auto spec = OperatorInterfaceSpec(entry.first, method.source, &left);
                        if (spec && spec->unary)
                        {
                            if (method.params.size() != 1 || !SameType(method.params[0].type, NamedType(owner)))
                                candidates.back().receiverOperand = "!unsupported";
                            candidates.back().params.clear();
                            candidates.back().fixedArgs = "obj";
                            break;
                        }
                    }
            }
            if (constructor && candidates.empty() && !g_enums.count(owner))
                candidates.push_back({ "new", {}, NamedType(owner), {}, true });
        }
        body += "bool found=false;bool countMatches=false;bool unsupported=false;int64 selected=-1;";
        unsigned index = 0;
        for (const auto &candidate : candidates)
        {
            body += constructor ? "{" : "if(name==" + queries.quote(candidate.name) + "){";
            body += "found=true;if(args.len()==" + Memory::toString(candidate.params.size()) + "){countMatches=true;";
            bool supported = constructor || !tag(candidate.result).empty();
            for (const auto &param : candidate.params)
                if (tag(param.type).empty() || tag(param.type) == "Empty" || param.variadic) supported = false;
            if (!free && !isStatic && (candidate.name == "drop" || candidate.name == "close")) supported = false;
            if (candidate.receiverOperand == "!unsupported") supported = false;
            if (!supported)
                body += "unsupported=true;";
            else
            {
                Memory::String condition = "true";
                for (std::size_t i = 0; i < candidate.params.size(); ++i)
                    condition += " && args[" + Memory::toString(i) + "].typeId()==" + queries.number(metadata.typeId(candidate.params[i].type));
                body += "if(" + condition + "){if(selected>=0){" + err("Ambiguous") + "}selected=" + Memory::toString(index) + ";}";
            }
            body += "}}";
            ++index;
        }
        body += "if(!found){" + err("NotFound") + "}if(!countMatches){" + err("ArgumentCount") + "}if(selected<0){if(unsupported){" + err("UnsupportedSignature") + "}" + err("TypeMismatch") + "}";
        index = 0;
        for (const auto &candidate : candidates)
        {
            bool supported = constructor || !tag(candidate.result).empty();
            for (const auto &param : candidate.params)
                if (tag(param.type).empty() || tag(param.type) == "Empty" || param.variadic) supported = false;
            if (!free && !isStatic && (candidate.name == "drop" || candidate.name == "close")) supported = false;
            if (candidate.receiverOperand == "!unsupported") supported = false;
            if (supported)
            {
                body += "if(selected==" + Memory::toString(index) + "){";
                if (!candidate.allowed)
                    body += err("ReceiverMismatch");
                else
                {
                    if (!candidate.receiverOperand.empty())
                        body += "if(actual!=__typeTag(" + candidate.receiverOperand + " mut&) && actual!=__typeTag(" + candidate.receiverOperand + " const&)){" + err("UnsupportedSignature") + "}";
                    auto call = candidate.expression + "(" + candidate.fixedArgs;
                    for (std::size_t i = 0; i < candidate.params.size(); ++i)
                    {
                        if (i) call += ",";
                        call += "args[" + Memory::toString(i) + "].as" + tag(candidate.params[i].type) + "().unwrap()";
                    }
                    call += ")";
                    if (constructor)
                        body += owner + " instance" + (candidate.expression.empty() ? "" : "=" + call) + ";return " + result + "::Ok(instance);";
                    else if (tag(candidate.result) == "Empty")
                        body += call + ";return " + result + "::Ok(std::reflect::ReflectValue::Empty);";
                    else
                        body += "return " + result + "::Ok(std::reflect::ReflectValue::" + tag(candidate.result) + "(" + call + "));";
                }
                body += "}";
            }
            ++index;
        }
        body += err("NotFound") + "}\n";
        queries.functions += body;
        queries.replaced.insert(decl.source);
    }
    /**
     * @brief 生成本程序已实例化的所有调用入口。
     */
    void build()
    {
        const Memory::Set<Memory::String> apis = { "call", "callMethod", "callMethodConst", "callStatic", "callEnumMethod", "callEnumMethodConst", "callEnumStatic", "construct", "callInterface", "callInterfaceConst", "getField", "setField" };
        for (const auto &decl : g_functionDecls)
        {
            if (decl.module != "std::reflect" || !decl.external || decl.superseded) continue;
            if (decl.source == "call")
            {
                generate(decl, "call", {});
                continue;
            }
            for (const auto &entry : chali_generic::session.made)
            {
                const auto &expansion = entry.second;
                if (!expansion.generic || expansion.generic->module != "std::reflect" || expansion.name != decl.source || !apis.count(expansion.generic->name)) continue;
                Memory::String owner;
                if (!decl.signature.params.empty() && decl.signature.params[0].type->kind == MType::Ptr)
                    owner = decl.signature.params[0].type->elem->name;
                else
                {
                    for (const auto &type : g_structs)
                        if (type.first == expansion.args[0] || metadata.named(type.first) == expansion.args[0]) owner = type.first;
                    for (const auto &type : g_interfaces)
                        if (type.first == expansion.args[0] || metadata.named(type.first) == expansion.args[0]) owner = type.first;
                }
                if (owner.empty())
                {
                    Fail("反射调用要求已单态化的具体类型");
                    return;
                }
                if (expansion.generic->name == "getField" || expansion.generic->name == "setField")
                    generateField(decl, owner, expansion.generic->name == "setField");
                else
                    generate(decl, expansion.generic->name, owner);
                break;
            }
        }
    }
};
