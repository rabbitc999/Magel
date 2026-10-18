/* 名称后置泛型：签名仍由生成解析器读取，推导只处理类型树。 */

/**
 * @brief 请求补充一个特化；编译入口重放语义分析，不输出中间产物。
 */
struct MInstantiationRequest
{
    Memory::String name;
    Memory::Vector<Memory::String> args;
};

/**
 * @brief 泛型签名的类型树及枚举载荷，符号类型参数不参与最终降级。
 */
struct MGenericSignature
{
    std::shared_ptr<MType> result;
    Memory::Vector<std::shared_ptr<MType>> params;
    Memory::Map<Memory::String, Memory::Vector<std::shared_ptr<MType>>> variants;
    bool variadic = false;
    std::size_t required = 0;
};
Memory::Map<Memory::String, MGenericSignature> g_genericSignatures;
Memory::Map<Memory::String, magel_generic::Expansion> g_genericPatterns;
Memory::Set<int> g_inferringExpressions;
Memory::Vector<std::shared_ptr<MType>> g_genericPatternTypes;

/**
 * @brief 将类型树还原为实例化器接受的类型拼写。
 */
Memory::String InferenceTypeText(const std::shared_ptr<MType> &type)
{
    if (!type) return "void";
    if (IsTupleType(type))
    {
        Memory::String text = "(";
        for (std::size_t i = 0; i < type->paramTypes.size(); ++i)
        {
            if (i) text += ",";
            text += InferenceTypeText(type->paramTypes[i]);
        }
        return text + ")";
    }
    if (type->kind == MType::Func)
    {
        auto result = InferenceTypeText(type->elem);
        if (type->elem && type->elem->kind == MType::Array) result = "(" + result + ")";
        Memory::String text = result + " fn(";
        for (std::size_t index = 0; index < type->paramTypes.size(); ++index)
        {
            if (index) text += ",";
            const bool variadic = type->variadic && index + 1u == type->paramTypes.size();
            text += InferenceTypeText(variadic ? type->paramTypes[index]->elem : type->paramTypes[index]);
            if (variadic) text += "...";
        }
        return text + ")";
    }
    if (type->kind == MType::Struct || type->kind == MType::Host) return type->name;
    if (type->kind == MType::Ptr)
    {
        auto text = InferenceTypeText(type->elem);
        if (type->elem && type->elem->kind == MType::Func) text = "(" + text + ")";
        return text + (type->readOnly ? " const&" : " mut&");
    }
    if (type->kind == MType::Array) return InferenceTypeText(type->elem) + "[]";
    if (type->kind == MType::Container)
    {
        auto text = Memory::String("std::lang::") + (type->name == "vec" ? "List" : type->name == "set" ? "Set" :
                                                                                                          "Map")
                    + "<" + InferenceTypeText(type->elem);
        if (type->elem2) text += "," + InferenceTypeText(type->elem2);
        return text + ">";
    }
    return TypeKey(type);
}

/**
 * @brief 具体类型直接使用可解析的匿名函数类型参与特化。
 */
Memory::String GenericArgumentText(const std::shared_ptr<MType> &type)
{
    return InferenceTypeText(type);
}

Memory::Map<Memory::String, Memory::String> g_inferenceCanonicalNames;
Memory::Map<Memory::String, std::shared_ptr<MType>> g_inferenceConcreteTypes, g_inferenceSymbolicTypes;
std::size_t g_inferenceConcreteCount = 0, g_inferenceSymbolicCount = 0;

/**
 * @brief 缓存本次编译中稳定的类型规范名，避免嵌套推导反复扫描模块类型表。
 * @param text 已解析类型的拼写。
 * @return 对应的规范类型名。
 */
const Memory::String &CanonicalInferenceName(const Memory::String &text)
{
    auto found = g_inferenceCanonicalNames.find(text);
    if (found == g_inferenceCanonicalNames.end())
        found = g_inferenceCanonicalNames.emplace(text, magel_generic::canonicalType(text)).first;
    return found->second;
}

/**
 * @brief 增量索引类型池；语义重放时具体类型索引单独清空。
 * @param pool 实际或符号类型池。
 * @param count 已索引的元素数。
 * @param index 规范类型名到类型树的映射。
 */
void IndexInferenceTypes(const Memory::Vector<std::shared_ptr<MType>> &pool, std::size_t &count,
                         Memory::Map<Memory::String, std::shared_ptr<MType>> &index)
{
    for (; count < pool.size(); ++count)
        index[CanonicalInferenceName(InferenceTypeText(pool[count]))] = pool[count];
}

/**
 * @brief 读取符号签名；函数正文不参与推导，枚举保留其全部载荷定义。
 */
bool PrepareGenericSignatures()
{
    g_inferenceCanonicalNames.clear();
    g_inferenceSymbolicTypes.clear();
    g_inferenceSymbolicCount = 0;
    g_genericSignatures.clear();
    g_genericPatterns.clear();
    g_genericPatternTypes.clear();
    const auto savedTop = g_top;
    const auto savedModule = g_parseModule;
    const auto nodeCount = g_nodes.size(), typeCount = g_types.size(), nameCount = g_names.size();
    for (const auto &entry : magel_generic::session.generics)
    {
        const auto &generic = entry.second;
        const auto head = magel_generic::trim(generic.declaration);
        const bool enumeration = magel_generic::wordAt(head, 0, "enum");
        if (!enumeration && (magel_generic::wordAt(head, 0, "struct") || magel_generic::wordAt(head, 0, "interface") || magel_generic::wordAt(head, 0, "type"))) continue;
        magel_generic::Generic probe = generic;
        if (!enumeration)
        {
            const auto body = probe.declaration.find('{');
            if (body != Memory::String::npos)
                probe.declaration.erase(body);
            else
            {
                const auto semi = probe.declaration.find(';');
                if (semi != Memory::String::npos) probe.declaration.erase(semi);
            }
            probe.declaration += " {}";
        }
        Memory::Vector<Memory::String> arguments;
        for (std::size_t i = 0; i < generic.params.size(); ++i)
            arguments.push_back("McInferenceParameter" + Memory::toString(i));
        magel_generic::Expansion expansion{ &probe, arguments, "McInferenceProbe" };
        auto source = magel_generic::substitute(expansion);
        Memory::Map<Memory::String, magel_generic::Expansion> patterns;
        Memory::Vector<Memory::String> pending;
        Memory::Map<Memory::String, Memory::Set<Memory::String>> unchecked;
        // 符号签名不执行约束检查，具体实例在展开全部嵌套声明后统一检查约束。
        auto templates = magel_generic::session.generics;
        for (auto &item : templates)
            for (auto &param : item.second.params)
            {
                param.constraint.clear();
                param.constant = false;
            }
        Memory::String error;
        if (!magel_generic::rewrite(source, templates, unchecked, patterns, pending, error))
            return Fail(error);
        for (const auto &item : patterns)
        {
            auto pattern = item.second;
            pattern.generic = &magel_generic::session.generics.at(pattern.generic->name);
            g_genericPatterns[item.first] = std::move(pattern);
        }
        magel::Engine parser;
        int value = 0;
        g_parseModule.clear();
        magel_generic::markContainerTypeArguments(source);
        if (parser.runText(source, &value) != magel::Status::Ok)
            return Fail("泛型声明的签名无法解析：" + generic.name);
        MGenericSignature signature;
        for (int top : g_top)
        {
            const auto &node = N(top);
            if (node.s != "McInferenceProbe") continue;
            if (node.kind == MNode::FuncDef)
            {
                signature.result = node.type >= 0 ? T(node.type) : nullptr;
                for (int param : N(node.kids[0]).kids)
                {
                    signature.params.push_back(T(N(param).type));
                    if (N(param).kids.empty() && N(param).i == 0) ++signature.required;
                    signature.variadic = N(param).i != 0;
                }
            }
            else if (node.kind == MNode::EnumDecl)
            {
                for (int variant : N(node.kids[0]).kids)
                {
                    auto &payload = signature.variants[N(variant).s];
                    if (!N(variant).kids.empty())
                        for (int param : N(N(variant).kids[0]).kids) payload.push_back(T(N(param).type));
                }
            }
        }
        g_genericSignatures[generic.name] = std::move(signature);
        g_genericPatternTypes.insert(g_genericPatternTypes.end(), g_types.begin() + typeCount, g_types.end());
        g_nodes.resize(nodeCount);
        g_types.resize(typeCount);
        g_names.resize(nameCount);
    }
    g_top = savedTop;
    g_parseModule = savedModule;
    return true;
}

/**
 * @brief 将模式类型中的类型参数与实参统一；同一参数不得推导出冲突类型。
 */
bool UnifyGenericType(const std::shared_ptr<MType> &pattern, const std::shared_ptr<MType> &actual,
                      Memory::Map<Memory::String, std::shared_ptr<MType>> &bindings)
{
    if (!pattern || !actual) return false;
    if (pattern->kind == MType::Struct && pattern->name.rfind("McInferenceParameter", 0) == 0)
    {
        const auto old = bindings.find(pattern->name);
        if (old != bindings.end()) return TypeKey(old->second) == TypeKey(actual);
        bindings[pattern->name] = actual;
        return true;
    }
    if (pattern->kind != actual->kind) return false;
    if (pattern->kind == MType::Ptr || pattern->kind == MType::Array || pattern->kind == MType::Container || pattern->kind == MType::Func)
    {
        if (pattern->kind == MType::Container && pattern->name != actual->name) return false;
        if (pattern->kind == MType::Ptr && !pattern->readOnly && actual->readOnly) return false;
        if (!UnifyGenericType(pattern->elem, actual->elem, bindings)) return false;
        if (pattern->elem2 && !UnifyGenericType(pattern->elem2, actual->elem2, bindings)) return false;
        if (pattern->paramTypes.size() != actual->paramTypes.size()) return false;
        for (std::size_t i = 0; i < pattern->paramTypes.size(); ++i)
            if (!UnifyGenericType(pattern->paramTypes[i], actual->paramTypes[i], bindings)) return false;
        return true;
    }
    if (IsTupleType(pattern) && IsTupleType(actual))
    {
        if (pattern->paramTypes.size() != actual->paramTypes.size()) return false;
        for (std::size_t i = 0; i < pattern->paramTypes.size(); ++i)
            if (!UnifyGenericType(pattern->paramTypes[i], actual->paramTypes[i], bindings)) return false;
        return true;
    }
    if (pattern->kind == MType::Struct)
    {
        const auto symbolic = g_genericPatterns.find(CanonicalInferenceName(pattern->name));
        const auto concrete = magel_generic::session.made.find(CanonicalInferenceName(actual->name));
        if (symbolic != g_genericPatterns.end() && concrete != magel_generic::session.made.end())
        {
            if (symbolic->second.generic->name != concrete->second.generic->name) return false;
            for (std::size_t i = 0; i < symbolic->second.args.size(); ++i)
            {
                // 实际类型参数由已解析类型池提供，不从 mangled 名字猜测。
                std::shared_ptr<MType> a, p;
                IndexInferenceTypes(g_types, g_inferenceConcreteCount, g_inferenceConcreteTypes);
                IndexInferenceTypes(g_genericPatternTypes, g_inferenceSymbolicCount, g_inferenceSymbolicTypes);
                const auto actualType = g_inferenceConcreteTypes.find(concrete->second.args[i]);
                const auto patternType = g_inferenceSymbolicTypes.find(symbolic->second.args[i]);
                if (actualType != g_inferenceConcreteTypes.end()) a = actualType->second;
                if (patternType != g_inferenceSymbolicTypes.end()) p = patternType->second;
                if (!p)
                {
                    p = Memory::makeShared<MType>();
                    p->kind = MType::Struct;
                    p->name = symbolic->second.args[i];
                }
                if (i < concrete->second.generic->params.size() && concrete->second.generic->params[i].constant)
                {
                    a = Memory::makeShared<MType>();
                    a->kind = MType::Struct;
                    a->name = concrete->second.args[i];
                }
                if (!a || !UnifyGenericType(p, a, bindings)) return false;
            }
            return true;
        }
    }
    return SameType(pattern, actual);
}

/**
 * @brief 从绑定生成完整实参表；没有信息的类型参数绝不隐式退化成 Any。
 */
bool CompleteGenericArguments(const magel_generic::Generic &generic,
                              const Memory::Map<Memory::String, std::shared_ptr<MType>> &bindings, Memory::Vector<Memory::String> &args)
{
    Memory::Map<Memory::String, Memory::String> resolved;
    for (std::size_t i = 0; i < generic.params.size(); ++i)
    {
        const auto found = bindings.find("McInferenceParameter" + Memory::toString(i));
        Memory::String argument;
        if (found == bindings.end())
        {
            if (!generic.params[i].hasDefault) return false;
            argument = magel_generic::substituteIdentifiers(generic.params[i].fallback, resolved);
            auto made = magel_generic::session.made;
            Memory::Vector<Memory::String> pending;
            Memory::String error;
            if (!magel_generic::checkTypeNesting(argument, error)) return Fail(error);
            if (!magel_generic::rewrite(argument, magel_generic::session.generics,
                                        magel_generic::session.implemented, made, pending, error)) return Fail(error);
            // 先请求默认类型中的内层实例，重放后再实例化外层函数或枚举。
            if (!pending.empty())
            {
                const auto &inner = made.at(pending.front());
                throw MInstantiationRequest{ inner.generic->name, inner.args };
            }
            argument = magel_generic::canonicalType(argument);
        }
        else
            argument = GenericArgumentText(found->second);
        resolved[generic.params[i].name] = argument;
        args.push_back(std::move(argument));
    }
    return true;
}

/**
 * @brief 优先保留局部变量、字段及普通函数的名称含义。
 */
bool HasOrdinaryName(const MNode &node)
{
    return FindIn(node.module, node.s) != nullptr || (!g_owner.empty() && (node.s == "self" || FieldType(g_owner, node.s))) || FindFunctionOverloads(node.module, node.s) != nullptr;
}

/**
 * @brief 用已绑定的外层类型参数构造回调上下文，返回类型可暂时保持未绑定。
 * @param pattern 泛型形参类型树。
 * @param bindings 已由其他实参确定的类型参数。
 * @return 替换后的类型树；不修改共享签名。
 */
std::shared_ptr<MType> BoundCallbackType(const std::shared_ptr<MType> &pattern,
                                         const Memory::Map<Memory::String, std::shared_ptr<MType>> &bindings)
{
    if (!pattern) return nullptr;
    const auto found = bindings.find(pattern->name);
    if (pattern->kind == MType::Struct && found != bindings.end()) return found->second;
    auto type = Memory::makeShared<MType>(*pattern);
    type->elem = BoundCallbackType(pattern->elem, bindings);
    type->elem2 = BoundCallbackType(pattern->elem2, bindings);
    type->paramTypes.clear();
    for (const auto &param : pattern->paramTypes)
        type->paramTypes.push_back(BoundCallbackType(param, bindings));
    return type;
}

/**
 * @brief 回调参数必须完全具体，不能把外层未绑定参数当作用户类型实例化。
 */
bool ConcreteCallbackParameter(const std::shared_ptr<MType> &type)
{
    if (!type) return false;
    if (type->kind == MType::Struct && type->name.rfind("McInferenceParameter", 0) == 0)
        return false;
    if (type->elem && !ConcreteCallbackParameter(type->elem)) return false;
    if (type->elem2 && !ConcreteCallbackParameter(type->elem2)) return false;
    for (const auto &param : type->paramTypes)
        if (!ConcreteCallbackParameter(param)) return false;
    return true;
}

/**
 * @brief 在真实符号作用域中检查 lambda 自由变量，覆盖解构和 match 等非类型声明绑定。
 * @param name 提升后的函数名。
 * @param module 原声明模块。
 * @return 捕获方式符合只读和所有权要求并已登记时返回 true。
 */
bool CheckLambdaCaptures(const Memory::String &name, const Memory::String &module)
{
    const auto key = ModuleSymbol(module, name);
    if (g_lambdaPrepared.count(key)) return true;
    for (auto &declaration : g_functionDecls)
    {
        if (declaration.source != name || declaration.module != module || declaration.bodyId < 0) continue;
        const int captureMode = name.find("_mut_") != Memory::String::npos  ? 1 :
                                name.find("_copy_") != Memory::String::npos ? 2 :
                                name.find("_move_") != Memory::String::npos ? 3 :
                                                                              0;
        Memory::Vector<MParam> captures;
        Memory::Set<Memory::String> captured;
        Memory::Set<Memory::String> locals;
        for (const auto &param : declaration.signature.params) locals.insert(param.source);
        const Memory::Map<Memory::String, int> *specifications = nullptr;
        std::size_t specificationLength = 0;
        for (const auto &entry : magel_lambda::captureSpecifications)
            if ((name == entry.first || name.rfind(entry.first + "__", 0) == 0) && entry.first.size() > specificationLength)
            {
                specifications = &entry.second;
                specificationLength = entry.first.size();
            }
        const auto addCapture = [&](const Memory::String &source, const MVar *variable, int mode) -> bool {
            if (!variable) return Fail("lambda 捕获列表引用了不存在的变量：" + source);
            if (name.find("_defer") != Memory::String::npos && (variable->isConst || (variable->type->kind == MType::Ptr && variable->type->readOnly))) mode = 0;
            if ((mode == 1 || mode == 3) && (variable->isConst || (variable->type->kind == MType::Ptr && variable->type->readOnly)))
                return Fail("lambda 的 mut/move 捕获不能借用或移动只读变量：" + source);
            if (captured.insert(source).second)
            {
                MParam capture{ source, {}, variable->type, false };
                capture.captureMode = mode;
                if (mode == 0 && capture.type->kind == MType::Ptr)
                    capture.type = PointerType(capture.type->elem, true);
                captures.push_back(capture);
            }
            return true;
        };
        if (specifications)
            for (const auto &entry : *specifications)
            {
                if (locals.count(entry.first)) return Fail("lambda 捕获变量与形参重名：" + entry.first);
                if (!addCapture(entry.first, FindIn(module, entry.first), entry.second)) return false;
            }
        std::function<void(int, Memory::Set<Memory::String> &)> bind;
        bind = [&](int id, Memory::Set<Memory::String> &names) {
            if (id < 0) return;
            const auto &node = N(id);
            if (node.kind == MNode::Var && node.s != "_")
                names.insert(node.s);
            else
                for (const int child : node.kids) bind(child, names);
        };
        std::function<bool(int, Memory::Set<Memory::String> &)> visit;
        visit = [&](int id, Memory::Set<Memory::String> &names) -> bool {
            if (id < 0) return true;
            const auto &node = N(id);
            if (node.kind == MNode::Var && !names.count(node.s))
            {
                const auto resolved = FindIn(node.module, node.s);
                for (std::size_t scope = g_scope.size(); scope-- > 1u;)
                {
                    const auto found = g_scope[scope].find(node.s);
                    if (found == g_scope[scope].end() || resolved != &found->second) continue;
                    const auto explicitMode = specifications ? specifications->find(node.s) : Memory::Map<Memory::String, int>::const_iterator{};
                    const int mode = specifications && explicitMode != specifications->end() ? explicitMode->second : captureMode;
                    if (!addCapture(node.s, &found->second, mode)) return false;
                    break;
                }
                if (node.s == "self" || (!resolved && !g_owner.empty() && FieldType(g_owner, node.s)))
                    return Fail("lambda 不能隐式捕获 self 或接收者字段：" + node.s);
                if (node.s.rfind("__magel_lambda_", 0) == 0)
                {
                    for (const auto &nested : g_functionDecls)
                    {
                        if (nested.source != node.s || nested.module != node.module || nested.bodyId < 0) continue;
                        auto inner = names;
                        for (const auto &param : nested.signature.params) inner.insert(param.source);
                        for (const auto &specification : magel_lambda::captureSpecifications)
                        {
                            if (nested.source != specification.first && nested.source.rfind(specification.first + "__", 0) != 0) continue;
                            for (const auto &entry : specification.second)
                            {
                                if (inner.count(entry.first)) continue;
                                const auto variable = FindIn(module, entry.first);
                                if (!variable) continue;
                                const auto explicitMode = specifications ? specifications->find(entry.first) : Memory::Map<Memory::String, int>::const_iterator{};
                                const int mode = specifications && explicitMode != specifications->end() ? explicitMode->second : captureMode;
                                if (!addCapture(entry.first, variable, mode)) return false;
                            }
                        }
                        if (!visit(nested.bodyId, inner)) return false;
                        break;
                    }
                }
            }
            if (node.kind == MNode::Block || node.kind == MNode::For || node.kind == MNode::Case)
            {
                auto nested = names;
                if (node.kind == MNode::Block) return visit(static_cast<int>(node.i), nested);
                if (node.kind == MNode::Case && node.i >= 0)
                {
                    const auto &pattern = N(static_cast<int>(node.i));
                    if (pattern.kind == MNode::EnumCtor)
                        for (const int child : pattern.kids) bind(child, nested);
                    else if (pattern.kind == MNode::Call && pattern.kids.size() == 2u)
                        bind(pattern.kids[1], nested);
                    else if (!visit(static_cast<int>(node.i), nested))
                        return false;
                    if (!visit(node.guard, nested)) return false;
                }
                for (const int child : node.kids)
                    if (!visit(child, nested)) return false;
                return true;
            }
            if (node.kind == MNode::Declare)
            {
                for (const int child : N(node.kids[0]).kids)
                {
                    const auto &decl = N(child);
                    for (const int init : decl.kids)
                        if (!visit(init, names)) return false;
                    names.insert(decl.s);
                }
                return true;
            }
            if (node.kind == MNode::Destructure)
            {
                if (!visit(node.kids[1], names)) return false;
                if (node.i) return visit(node.kids[0], names);
                bind(node.kids[0], names);
                return true;
            }
            if (node.kind == MNode::Foreach)
            {
                if (!visit(node.kids[0], names)) return false;
                auto nested = names;
                nested.insert(node.s);
                return visit(node.kids[1], nested);
            }
            for (const int child : node.kids)
                if (!visit(child, names)) return false;
            return true;
        };
        if (!visit(declaration.bodyId, locals)) return false;
        g_lambdaCaptures[key] = captures;
        g_lambdaPrepared.insert(key);
        if (g_lambdaAutoReturns.count(key))
        {
            const auto savedModule = g_currentModule;
            const auto savedOwner = g_owner;
            g_currentModule = declaration.module;
            g_owner.clear();
            PushScope();
            for (const auto &capture : captures) g_scope.back()[capture.source] = MVar{ capture.type, capture.source, capture.captureMode == 0 };
            for (const auto &param : declaration.signature.params) g_scope.back()[param.source] = MVar{ param.type, param.source, false };
            std::shared_ptr<MType> result;
            const bool ok = InferAutoFunctionResult(declaration.bodyId, &result);
            PopScope();
            g_currentModule = savedModule;
            g_owner = savedOwner;
            if (!ok || !result) return ok ? Fail("auto lambda 必须返回值") : false;
            declaration.signature.result = result;
            g_inferenceFunctions[key] = declaration.signature;
            if (!declaration.label.empty()) g_funcs[declaration.label] = declaration.signature;
        }
        return true;
    }
    return true;
}

/**
 * @brief 从函数指针的形参类型实例化省略类型的 lambda，返回类型由函数体推导。
 * @param id lambda 提升后的内部函数名节点。
 * @param expected 函数指针上下文，不要求其返回类型已绑定。
 * @return 是待实例化 lambda 时返回 true。
 */
bool InferLambdaCallback(int id, const std::shared_ptr<MType> &expected)
{
    const auto node = N(id);
    if (node.kind != MNode::Var || node.s.rfind("__magel_lambda_", 0) != 0) return false;
    const auto generic = magel_generic::session.generics.find(node.s);
    const auto signature = g_genericSignatures.find(node.s);
    if (generic == magel_generic::session.generics.end() || signature == g_genericSignatures.end())
        return false;
    if (!expected || expected->kind != MType::Func) return true;
    if (signature->second.params.size() != expected->paramTypes.size())
    {
        Fail("lambda 的形参数量与函数指针上下文不匹配");
        return true;
    }
    Memory::Map<Memory::String, std::shared_ptr<MType>> bindings;
    for (std::size_t i = 0; i < expected->paramTypes.size(); ++i)
    {
        if (!ConcreteCallbackParameter(expected->paramTypes[i]))
        {
            Fail("无法从未确定的函数指针参数类型推导 lambda，请显式写出形参类型");
            return true;
        }
        if (!UnifyGenericType(signature->second.params[i], expected->paramTypes[i], bindings))
        {
            Fail("lambda 的形参类型与函数指针上下文不匹配");
            return true;
        }
    }
    Memory::Vector<Memory::String> args;
    if (!CompleteGenericArguments(generic->second, bindings, args))
    {
        Fail("无法从函数指针上下文推导 lambda 的全部形参类型");
        return true;
    }
    const auto specialized = magel_generic::mangle(node.s, args);
    if (!magel_generic::session.made.count(specialized)) throw MInstantiationRequest{ node.s, args };
    g_nodes[id].s = specialized;
    CheckLambdaCaptures(specialized, node.module);
    return true;
}

/**
 * @brief 无载荷泛型变体没有可供推导的信息，须等待其他实参提供上下文。
 */
bool IsUnboundEmptyVariant(int id)
{
    const auto &node = N(id);
    if (node.kind != MNode::Var || HasOrdinaryName(node)) return false;
    for (const auto &entry : g_genericSignatures)
    {
        const auto found = entry.second.variants.find(node.s);
        if (found != entry.second.variants.end() && found->second.empty()) return true;
    }
    return false;
}

/**
 * @brief 回调和方法调用在查询载荷类型前，先向实参传递已知形参类型。
 */
void InferKnownCallArguments(const MNode &callee, const Memory::Vector<int> &arguments)
{
    Memory::Vector<Memory::Vector<std::shared_ptr<MType>>> candidates;
    if (callee.kind == MNode::Var)
    {
        const auto *variable = FindIn(callee.module, callee.s);
        if (variable && variable->type && variable->type->kind == MType::Func)
            candidates.push_back(variable->type->paramTypes);
    }
    else if (callee.kind == MNode::Member || callee.kind == MNode::Arrow)
    {
        const auto base = ExprType(callee.kids[0]);
        if (IsContainer(base))
        {
            const auto *method = FindContainerMethod(base, callee.s);
            if (method && arguments.size() == static_cast<std::size_t>(method->arity))
            {
                const Memory::String shape(method->shape);
                if (!arguments.empty() && (shape.find('v') != Memory::String::npos || shape.find('k') != Memory::String::npos))
                    InferExpression(arguments[0], base->elem);
                if (arguments.size() > 1u && shape.find('w') != Memory::String::npos)
                    InferExpression(arguments[1], base->elem2 ? base->elem2 : base->elem);
            }
            return;
        }
        const auto owner = StructNameOf(base);
        const auto found = g_structs.find(owner);
        if (found != g_structs.end())
            for (const auto &method : found->second.methods)
            {
                if (method.source != callee.s || method.superseded || !AcceptsArgumentCount(method.params, arguments.size())) continue;
                Memory::Vector<std::shared_ptr<MType>> params;
                for (const auto &param : method.params) params.push_back(param.type);
                candidates.push_back(std::move(params));
            }
    }
    if (callee.kind == MNode::Var)
    {
        const auto *overloads = FindFunctionOverloads(callee.module, callee.s);
        if (overloads)
            for (const auto &label : *overloads)
            {
                const auto found = g_funcs.find(label);
                if (found == g_funcs.end() || !AcceptsArgumentCount(found->second.params, arguments.size())) continue;
                Memory::Vector<std::shared_ptr<MType>> params;
                for (const auto &param : found->second.params) params.push_back(param.type);
                candidates.push_back(std::move(params));
            }
    }
    if (candidates.empty()) return;
    for (std::size_t i = 0; i < arguments.size(); ++i)
    {
        if (i >= candidates[0].size()) continue;
        const auto type = candidates[0][i];
        bool same = true;
        for (const auto &candidate : candidates)
            if (i >= candidate.size() || !SameType(type, candidate[i])) same = false;
        if (same) InferExpression(arguments[i], type);
    }
}

/**
 * @brief 将标准迭代器适配器点调用归一为 std::iter 的泛型函数。
 * @param id 调用节点。
 * @return 已重写返回 true；普通用户方法保持原状。
 */
bool RewriteIteratorCall(int id)
{
    const auto node = N(id);
    if (node.kind != MNode::Call || node.kids.size() != 2u) return false;
    const auto callee = N(node.kids[0]);
    if (callee.kind != MNode::Member || callee.kids.empty()) return false;
    const int receiver = callee.kids[0];
    const auto type = ExprType(receiver);
    Memory::String function;
    bool address = false;
    const auto made = magel_generic::session.made.find(type->name);
    if (made != magel_generic::session.made.end() && made->second.generic->module == "std::iter" && made->second.generic->name == "Iter")
    {
        static const Memory::Set<Memory::String> adapters{
            "map", "filter", "filterMap", "mapWhile", "takeWhile", "skipWhile", "findMap",
            "fold", "tryFold", "scan", "take", "skip", "zip", "cycle", "chain", "rev",
            "enumerate", "peekingTakeWhile", "stepBy", "fuse", "inspect", "sameItems", "collect"
        };
        if (adapters.count(callee.s)) function = callee.s;
    }
    else if (IsContainer(type) && type->name == "vec" && (callee.s == "iter" || callee.s == "iterMut" || callee.s == "intoIter"))
    {
        function = callee.s == "iter" ? "listIter" : callee.s == "iterMut" ? "listIterMut" :
                                                                             "listIntoIter";
        address = true;
    }
    else if (IsContainer(type) && (type->name == "set" || type->name == "map") && (callee.s == "iter" || callee.s == "intoIter"))
    {
        /* `iter` 是借用快照（来源容器保持不变），`intoIter` 是搬空容器。 */
        if (type->name == "set") function = callee.s == "iter" ? "setIter" : "setIntoIter";
        else function = callee.s == "iter" ? "mapIter" : "mapIntoIter";
        address = true;
    }
    else if (type->kind == MType::String && callee.s == "chars")
        function = "chars";
    else if (type->kind == MType::Struct && StructNameOf(type).find("__Range") != Memory::String::npos && callee.s == "intoIter")
        function = "fromRange";
    if (function.empty()) return false;
    const auto generic = magel_generic::session.generics.find(function);
    if ((generic == magel_generic::session.generics.end() || generic->second.module != "std::iter") && FindFunctionOverloads("std::iter", function) == nullptr)
    {
        Fail("迭代器方法需要先 import std::iter：" + callee.s);
        return false;
    }
    int source = receiver;
    if (address)
    {
        source = M_New(MNode::Un, "&");
        g_nodes[source].kids = { receiver };
    }
    const int args = M_New(MNode::List);
    g_nodes[args].kids = N(node.kids[1]).kids;
    g_nodes[args].kids.insert(g_nodes[args].kids.begin(), source);
    const int target = M_New(MNode::EnumCtor, "std::iter");
    g_nodes[target].s2 = function;
    g_nodes[target].type2 = 0;
    g_nodes[id].kids = { target, args };
    return true;
}

/**
 * @brief 将标准错误适配方法转换为同语义的泛型函数调用。
 * @param id 调用节点。
 * @return 是否已处理。
 */
bool RewriteErrorAdapter(int id)
{
    const auto node = N(id);
    if (node.kind != MNode::Call || node.kids.size() != 2u) return false;
    const auto callee = N(node.kids[0]);
    if (callee.kind != MNode::Member || callee.kids.empty()) return false;
    const auto type = ExprType(callee.kids[0]);
    const auto source = magel_generic::session.made.find(type->name);
    if (source == magel_generic::session.made.end() || source->second.generic->module != "std::lang") return false;
    Memory::String method = callee.s, explicitType;
    auto marker = magel_generic::session.made.find(method);
    if (marker != magel_generic::session.made.end() && marker->second.generic->module == "std::lang")
    {
        auto name = marker->second.generic->name;
        if (name == "__magelMapErr")
            method = "mapErr";
        else if (name == "__magelOkOr")
            method = "okOr";
        else if (name == "__magelOkOrElse")
            method = "okOrElse";
        else
            return false;
        explicitType = marker->second.args[0];
    }
    Memory::String function;
    if (method == "mapErr" && source->second.generic->name == "Result") function = "__mapErrImpl";
    if (method == "okOr" && source->second.generic->name == "Option") function = "__okOrImpl";
    if (method == "okOrElse" && source->second.generic->name == "Option") function = "__okOrElseImpl";
    if (function.empty()) return false;
    if (!explicitType.empty())
    {
        auto arguments = source->second.args;
        arguments.push_back(explicitType);
        const auto specialized = magel_generic::mangle(function, arguments);
        if (!magel_generic::session.made.count(specialized)) throw MInstantiationRequest{ function, arguments };
        function = specialized;
    }
    const int arguments = M_New(MNode::List);
    g_nodes[arguments].kids = N(node.kids[1]).kids;
    g_nodes[arguments].kids.insert(g_nodes[arguments].kids.begin(), callee.kids[0]);
    const int target = M_New(MNode::EnumCtor, "std::lang");
    g_nodes[target].s2 = function;
    g_nodes[id].kids = { target, arguments };
    return true;
}

/**
 * @brief 解析枚举简写与普通泛型函数调用，递归期间避免重复推导同一节点。
 */
void InferExpression(int id, const std::shared_ptr<MType> &expected)
{
    if (id < 0 || !g_inferringExpressions.insert(id).second) return;
    struct Guard
    {
        int id;
        /**
         * @brief 移除当前表达式的类型推导递归标记。
         */
        ~Guard() { g_inferringExpressions.erase(id); }
    } guard{ id };
    if (N(id).kind == MNode::Loop && !N(id).s.empty() && expected)
    {
        g_nodes[id].type = M_AddType(expected);
        return;
    }
    RewriteErrorAdapter(id);
    RewriteIteratorCall(id);
    const auto node = N(id);
    if (node.kind == MNode::Int && node.unsignedLiteral && expected && expected->kind == MType::Int)
    {
        PinIntegerLiteral(id, expected);
        return;
    }
    /* 目标类型下传：`int32 c = ff() + 1;` 里的字面量按 int32 定，而不是默认的 int64。
     * 浮点槽位走「整数字面量落浮点槽位」那条老路，这里只管整型上下文。 */
    const bool literalContext = expected && expected->kind == MType::Int && (node.kind == MNode::Bin ? LiteralAdaptingOperator(node.s) : (node.kind == MNode::Un && !node.kids.empty() && (node.s == "-" || node.s == "+" || node.s == "~")));
    if (literalContext) PinLiteralInExpression(id, expected);
    /* 运算符接口允许「同一对操作数、不同返回类型」（`Add<int64,int64>` 与
     * `Add<Vec,int64>` 共存），光看操作数挑不出来，只能靠上下文。把目标类型带进匹配，
     * 定下来就把结果类型钉在这个节点上（`RawExprType` 会优先用它）；上下文定不下来的
     * 留给降级阶段报「请指明目标类型」，不在这里悄悄挑一个。 */
    if (node.kind == MNode::Bin && expected && expected->kind != MType::Void)
    {
        const MOperatorDecl *chosen = nullptr;
        MOperatorMatch chosenMatch;
        if (FindOperator(node.s, ExprType(node.kids[0]), ExprType(node.kids[1]),
                         &chosen, &chosenMatch, &expected, false))
            g_nodes[id].type = M_AddType(chosen->result);
    }
    if (literalContext) return;
    // 装箱提取的目标类型也供生命周期检查使用，不能在降低前退回默认整数类型。
    if (expected && IsObjGetCall(id))
    {
        g_nodes[id].type = M_AddType(expected);
        return;
    }
    if (InferLambdaCallback(id, expected)) return;
    if (node.kind == MNode::Var && node.s.rfind("__magel_lambda_", 0) == 0 && !CheckLambdaCaptures(node.s, node.module)) return;
    if (node.kind == MNode::CollectionLiteral)
    {
        InferCollectionLiteral(id, expected);
        return;
    }
    if (node.kind == MNode::Tuple && IsTupleType(expected) && node.kids.size() == expected->paramTypes.size())
    {
        bool referenceElements = false;
        for (std::size_t i = 0; i < node.kids.size(); ++i)
        {
            InferExpression(node.kids[i], expected->paramTypes[i]);
            referenceElements = referenceElements || expected->paramTypes[i]->kind == MType::Ptr;
        }
        /* 值表达式默认解引用；只有明确的元组引用元素上下文才保留地址。 */
        if (referenceElements) g_nodes[id].type = M_AddType(expected);
        return;
    }
    Memory::String name;
    Memory::String enumOwner;
    Memory::Vector<int> arguments;
    int calleeId = -1;
    if (node.kind == MNode::Var)
    {
        if (HasOrdinaryName(node)) return;
        name = node.s;
    }
    else if (node.kind == MNode::Call && node.kids.size() == 2)
    {
        calleeId = node.kids[0];
        const auto &callee = N(calleeId);
        arguments = N(node.kids[1]).kids;
        InferKnownCallArguments(callee, arguments);
        if (callee.kind != MNode::Var && callee.kind != MNode::EnumCtor) return;
        name = callee.kind == MNode::Var ? callee.s : callee.s2;
        const auto generic = magel_generic::session.generics.find(name);
        const auto signature = g_genericSignatures.find(name);
        if (generic != magel_generic::session.generics.end() && signature != g_genericSignatures.end() && signature->second.variants.empty() && (callee.kind != MNode::Var || !HasOrdinaryName(callee)) && (callee.kind != MNode::EnumCtor || callee.s == generic->second.module))
        {
            auto &sig = signature->second;
            if ((!sig.variadic && arguments.size() > sig.params.size()) || arguments.size() < sig.required)
            {
                Fail("泛型函数 " + name + " 的实参数量不匹配");
                return;
            }
            Memory::Map<Memory::String, std::shared_ptr<MType>> bindings;
            Memory::Vector<std::size_t> order;
            for (std::size_t i = 0; i < arguments.size(); ++i)
                if (!InferLambdaCallback(arguments[i], nullptr)) order.push_back(i);
            for (std::size_t i = 0; i < arguments.size(); ++i)
                if (InferLambdaCallback(arguments[i], nullptr)) order.push_back(i);
            // 先从普通实参绑定类型，再处理回调；不要求回调在形参表中排在最后。
            for (const std::size_t i : order)
            {
                if (IsUnboundEmptyVariant(arguments[i])) continue;
                auto pattern = sig.params[std::min(i, sig.params.size() - 1)];
                if (sig.variadic && i + 1 >= sig.params.size()) pattern = pattern->elem;
                if (pattern->kind == MType::Func)
                    InferExpression(arguments[i], BoundCallbackType(pattern, bindings));
                const auto actual = pattern->kind == MType::Ptr ? RawExprType(arguments[i]) : ExprType(arguments[i]);
                if (!UnifyGenericType(pattern, actual, bindings))
                {
                    Fail("泛型函数 " + name + " 的类型实参推导冲突，请显式指定类型实参\nnote: 第 " + Memory::toString(i + 1) + " 个实参实际为 " + Describe(actual) + "，形参模式为 " + Describe(pattern));
                    return;
                }
            }
            Memory::Vector<Memory::String> args;
            if (expected && bindings.size() < generic->second.params.size() && sig.result)
                UnifyGenericType(sig.result, expected, bindings);
            if (!CompleteGenericArguments(generic->second, bindings, args))
            {
                Memory::String missing;
                for (const auto &p : generic->second.params)
                    if (!bindings.count(p.name))
                    {
                        if (!missing.empty()) missing += ", ";
                        missing += p.name;
                    }
                Fail("无法推导泛型函数 " + name + " 的全部类型实参，请显式指定\nnote: 未绑定类型参数：" + missing);
                return;
            }
            const auto specialized = magel_generic::mangle(name, args);
            if (!magel_generic::session.made.count(specialized)) throw MInstantiationRequest{ name, args };
            auto &target = g_nodes[static_cast<std::size_t>(calleeId)];
            if (target.kind == MNode::Var)
                target.s = specialized;
            else
                target.s2 = specialized;
            return;
        }
        if (callee.kind != MNode::Var || HasOrdinaryName(callee)) return;
    }
    else if (node.kind == MNode::EnumCtor)
    {
        if (const auto *owner = FindStruct(node.s))
        {
            for (const auto &method : owner->methods)
            {
                if (!method.isStatic || method.source != node.s2 || !AcceptsArgumentCount(method.params, node.kids.size())) continue;
                for (std::size_t i = 0; i < node.kids.size(); ++i)
                {
                    const auto &param = method.params[std::min(i, method.params.size() - 1u)];
                    InferExpression(node.kids[i], param.variadic ? param.type->elem : param.type);
                }
            }
        }
        const auto *variant = FindEnumVariant(node.s, node.s2);
        if (variant)
        {
            for (std::size_t i = 0; i < node.kids.size() && i < variant->payloads.size(); ++i)
                InferExpression(node.kids[i], variant->payloads[i].type);
            return;
        }
        const auto generic = g_genericSignatures.find(node.s);
        if (generic == g_genericSignatures.end() || generic->second.variants.empty()) return;
        enumOwner = node.s;
        name = node.s2;
        arguments = node.kids;
    }
    else
        return;

    Memory::Vector<Memory::String> candidates;
    if (expected && IsEnumType(expected) && FindEnumVariant(expected->name, name) && (enumOwner.empty() || expected->name.rfind(enumOwner + "__", 0) == 0))
        candidates.push_back(expected->name);
    else
    {
        for (const auto &entry : g_enums)
        {
            if (!enumOwner.empty()) continue;
            if (magel_generic::session.made.count(entry.first)) continue;
            const auto *variant = FindEnumVariant(entry.first, name);
            if (variant && variant->payloads.size() == arguments.size())
            {
                bool matches = true;
                for (std::size_t i = 0; i < arguments.size(); ++i)
                    if (!Acceptable(variant->payloads[i].type, RawExprType(arguments[i]), true)) matches = false;
                if (matches) candidates.push_back(entry.first);
            }
        }
        for (const auto &entry : g_genericSignatures)
        {
            if (!enumOwner.empty() && entry.first != enumOwner) continue;
            const auto found = entry.second.variants.find(name);
            if (found == entry.second.variants.end() || found->second.size() != arguments.size()) continue;
            Memory::Map<Memory::String, std::shared_ptr<MType>> bindings;
            bool match = true;
            for (std::size_t i = 0; i < arguments.size(); ++i)
                if (!UnifyGenericType(found->second[i], found->second[i]->kind == MType::Ptr ? RawExprType(arguments[i]) : ExprType(arguments[i]), bindings)) match = false;
            Memory::Vector<Memory::String> args;
            if (!match || !CompleteGenericArguments(magel_generic::session.generics.at(entry.first), bindings, args)) continue;
            const auto specialized = magel_generic::mangle(entry.first, args);
            candidates.push_back(specialized);
        }
    }
    if (candidates.size() > 1)
    {
        Fail("枚举变体 " + name + " 存在歧义，请指定枚举类型");
        return;
    }
    if (candidates.empty())
    {
        if (IsUnboundEmptyVariant(id)) Fail("无法推导无载荷枚举变体 " + name + " 的类型参数，请提供目标类型");
        return;
    }
    if (!g_enums.count(candidates[0]))
    {
        for (const auto &entry : g_genericSignatures)
        {
            const auto found = entry.second.variants.find(name);
            if (found == entry.second.variants.end() || found->second.size() != arguments.size()) continue;
            Memory::Map<Memory::String, std::shared_ptr<MType>> bindings;
            bool matches = true;
            for (std::size_t i = 0; i < arguments.size(); ++i)
                if (!UnifyGenericType(found->second[i], found->second[i]->kind == MType::Ptr ? RawExprType(arguments[i]) : ExprType(arguments[i]), bindings)) matches = false;
            Memory::Vector<Memory::String> args;
            if (matches && CompleteGenericArguments(magel_generic::session.generics.at(entry.first), bindings, args) && magel_generic::mangle(entry.first, args) == candidates[0]) throw MInstantiationRequest{ entry.first, args };
        }
    }
    auto &target = g_nodes[static_cast<std::size_t>(id)];
    target.kind = MNode::EnumCtor;
    target.s = candidates[0];
    target.s2 = name;
    target.kids = arguments;
    target.type2 = arguments.empty() ? 0 : 1;
    const auto *variant = FindEnumVariant(target.s, name);
    if (variant)
        for (std::size_t i = 0; i < arguments.size() && i < variant->payloads.size(); ++i)
            InferExpression(arguments[i], variant->payloads[i].type);
}
