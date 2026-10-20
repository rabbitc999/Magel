/* ---------- 作用域 ---------- */

/**
 * @brief 进入新的词法作用域。
 */
void PushScope() { g_scope.push_back(Memory::Map<Memory::String, MVar>()); }
/**
 * @brief 退出当前词法作用域并恢复外层绑定。
 */
void PopScope()
{
    if (!g_scope.empty()) g_scope.pop_back();
}

/**
 * @brief 名字解析：从最内层作用域往外找（全局那一层按裸名）。
 * @param name 名称文本。
 * @retval 对应对象的指针。
 */
MVar *Find(const Memory::String &name)
{
    for (auto it = g_scope.rbegin(); it != g_scope.rend(); ++it)
    {
        const auto found = it->find(name);
        if (found != it->end()) return &found->second;
    }
    return nullptr;
}

/**
 * @brief 模块感知的名字解析：局部作用域按裸名，全局先看 `模块::名`，再兜底裸名。
 *
 * 全局变量按**模块限定名**登记（见 `RegisterTop`），所以模块里的 `x` 会先命中
 * 本模块的 `mod::x`；根模块与 prelude（`lang`）仍可按裸名兜底。两个模块各写一个
 * 同名全局时，各自的代码解析到各自的槽，不会再互相覆盖。
 * @param module 引用所在的定义模块。
 * @param name 源码名称。
 * @param globalOnly 是否跳过调用者局部作用域。
 * @return 找到的变量，不存在时返回空指针。
 */
MVar *FindIn(const Memory::String &module, const Memory::String &name, bool globalOnly)
{
    /* 局部作用域：0 号是全局作用域，跳过。 */
    for (std::size_t index = globalOnly ? 0u : g_scope.size(); index > 1u;)
    {
        --index;
        const auto found = g_scope[index].find(name);
        if (found != g_scope[index].end()) return &found->second;
    }
    if (g_scope.empty()) return nullptr;
    if (!module.empty())
    {
        const auto qualified = g_scope.front().find(module + "::" + name);
        if (qualified != g_scope.front().end()) return &qualified->second;
    }
    const auto bare = g_scope.front().find(name);
    if (bare != g_scope.front().end()) return &bare->second;
    Memory::Set<Memory::String> owners{ "std::lang" };
    const auto imported = g_moduleImports.find(module);
    if (imported != g_moduleImports.end()) owners.insert(imported->second.begin(), imported->second.end());
    MVar *result = nullptr;
    Memory::String chosen;
    for (const auto &owner : owners)
    {
        const auto candidate = g_scope.front().find(ModuleSymbol(owner, name));
        if (candidate == g_scope.front().end()) continue;
        if (result)
        {
            Fail("名称 " + name + " 有歧义：" + chosen + " 与 " + candidate->first + "；请使用作用域限定名");
            return nullptr;
        }
        result = &candidate->second;
        chosen = candidate->first;
    }
    return result;
}

/**
 * @brief 从独立的语法上下文和声明身份生成内部作用域键，不改变用户名字。
 * @param node 声明或引用节点。
 * @return 普通节点返回源名字，宏局部返回不能由源码标识符构造的绑定键。
 */
Memory::String NodeBindingKey(const MNode &node)
{
    if (!node.syntaxContext || !node.bindingIdentity) return node.s;
    return Memory::String("\x1fmacro:") + Memory::toString(node.bindingContext ? node.bindingContext : node.syntaxContext) + ":" + Memory::toString(node.bindingIdentity);
}

/**
 * @brief 判断节点是否为宏定义环境中的未限定辅助名称。
 * @param node 待解析的语法节点。
 * @return 应跳过调用者局部及接收者字段时返回真。
 */
bool NodeUsesDefinitionLookup(const MNode &node)
{
    return node.syntaxContext != 0 && node.bindingIdentity == 0;
}

/**
 * @brief 区分调用者片段、宏局部绑定与定义处辅助名称，统一供类型和资源检查使用。
 * @param node 带原始名字及卫生来源的节点。
 * @return 对应变量，不存在返回空指针。
 */
MVar *FindNodeVar(const MNode &node)
{
    if (node.bindingIdentity)
        for (auto scope = g_scope.rbegin(); scope != g_scope.rend(); ++scope)
            for (auto &entry : *scope)
                if (entry.second.bindingIdentity == node.bindingIdentity) return &entry.second;
    if (!node.syntaxContext) return FindIn(node.module, node.s);
    if (node.bindingIdentity) return Find(NodeBindingKey(node));
    return FindIn(node.module, node.s, true);
}

/**
 * @brief 按捕获时记录的声明身份查找环境来源，避免同名宏局部之间相互捕获。
 * @param module 闭包所在模块。
 * @param capture 环境字段及其原声明身份。
 * @return 可见的来源变量，找不到时返回空指针。
 */
MVar *FindCaptureVar(const Memory::String &module, const MParam &capture)
{
    MNode reference;
    reference.s = capture.source;
    reference.module = module;
    reference.bindingIdentity = capture.bindingIdentity;
    return FindNodeVar(reference);
}

/**
 * @brief 为与形参同名的捕获保留独立槽，源码名称仍优先指向形参。
 * @param capture 带原声明身份的捕获字段。
 * @param parameters 当前函数的实际形参列表。
 * @return 可放入当前作用域且不会覆盖形参的内部绑定键。
 */
Memory::String CaptureBindingKey(const MParam &capture, const Memory::Vector<MParam> &parameters)
{
    const auto key = capture.bindingKey.empty() ? capture.source : capture.bindingKey;
    for (const auto &parameter : parameters)
    {
        const auto parameterKey = parameter.bindingKey.empty() ? parameter.source : parameter.bindingKey;
        if (key == parameterKey && capture.bindingIdentity != parameter.bindingIdentity)
            return Memory::String("\x1f"
                                  "capture:")
                   + Memory::toString(capture.bindingIdentity) + ":" + capture.source;
    }
    return key;
}

/**
 * @brief 只认模块限定全局（`模块::名`），不兜底裸名。
 * @param module 当前模块的名称。
 * @param name 名称文本。
 * @retval 对应对象的指针。
 */
MVar *FindQualifiedGlobal(const Memory::String &module, const Memory::String &name)
{
    if (module.empty() || g_scope.empty()) return nullptr;
    const auto found = g_scope.front().find(module + "::" + name);
    return found == g_scope.front().end() ? nullptr : &found->second;
}

/**
 * @brief 核心库裸名只在没有局部变量和用户函数时生效。
 * @param module 调用所在模块。
 * @param name 未限定名称。
 * @return 可以回退核心库时为 true。
 */
bool CanUseImplicitLang(const Memory::String &module, const Memory::String &name)
{
    if (FindIn(module, name) != nullptr) return false;
    if (g_structs.count(name) || g_hostTypes.count(name)) return false;
    if (!module.empty() && module != "std::lang" && g_functionIndex.count(ModuleSymbol(module, name))) return false;
    if (g_functionIndex.count(name)) return false;
    const auto imported = g_moduleImports.find(module);
    if (imported != g_moduleImports.end())
        for (const auto &owner : imported->second)
            if (owner != "std::lang" && g_functionIndex.count(ModuleSymbol(owner, name))) return false;
    return true;
}

/**
 * @brief 模块感知的函数名解析：先找 `模块::名`，再兜底根模块裸名。
 *
 * 函数重载表按**模块限定名**登记（见 RegisterTop），所以模块里的裸函数名必须走
 * 这条路径 —— 否则取函数指针 / `sizeof(函数名)` / 重载判定都会落空。
 */
const Memory::Vector<Memory::String> *FindFunctionOverloads(const Memory::String &module,
                                                            const Memory::String &name)
{
    if (!module.empty())
    {
        const auto qualified = g_functionIndex.find(module + "::" + name);
        if (qualified != g_functionIndex.end()) return &qualified->second;
    }
    const auto bare = g_functionIndex.find(name);
    if (bare != g_functionIndex.end()) return &bare->second;
    Memory::Set<Memory::String> owners{ "std::lang" };
    const auto imported = g_moduleImports.find(module);
    if (imported != g_moduleImports.end()) owners.insert(imported->second.begin(), imported->second.end());
    const Memory::Vector<Memory::String> *result = nullptr;
    Memory::String chosen;
    for (const auto &owner : owners)
    {
        const auto candidate = g_functionIndex.find(ModuleSymbol(owner, name));
        if (candidate == g_functionIndex.end()) continue;
        if (result)
        {
            Fail("名称 " + name + " 有歧义：" + chosen + " 与 " + ModuleSymbol(owner, name) + "；请使用作用域限定名");
            return nullptr;
        }
        result = &candidate->second;
        chosen = candidate->first;
    }
    return result;
}

/**
 * @brief 模块感知的 auto 推导函数表查找（键的登记口径和 `g_functionIndex` 一致）。
 * @param module 当前模块的名称。
 * @param name 名称文本。
 * @retval 对应对象的指针。
 */
const MFuncSig *FindInferenceFunction(const Memory::String &module, const Memory::String &name)
{
    if (!module.empty())
    {
        const auto qualified = g_inferenceFunctions.find(module + "::" + name);
        if (qualified != g_inferenceFunctions.end()) return &qualified->second;
    }
    const auto bare = g_inferenceFunctions.find(name);
    if (bare != g_inferenceFunctions.end()) return &bare->second;
    const auto imported = g_moduleImports.find(module);
    const MFuncSig *result = nullptr;
    Memory::Set<Memory::String> owners{ "std::lang" };
    if (imported != g_moduleImports.end()) owners.insert(imported->second.begin(), imported->second.end());
    for (const auto &owner : owners)
    {
        const auto found = g_inferenceFunctions.find(ModuleSymbol(owner, name));
        if (found == g_inferenceFunctions.end()) continue;
        if (result) return nullptr;  // 最终重载解析报告同名导入歧义。
        result = &found->second;
    }
    return result;
}

/**
 * @brief 在当前作用域登记一个变量。
 *
 * 形参按名字绑定在**每个调用帧**里（stackvm 的 `Op::Call`），所以形参名字照抄源程序；
 * 函数体里的局部变量在 stackvm 里落在全局槽，所以重名时要加后缀，免得互相覆盖。
 *
 * @param name 源程序里的名字。
 * @param emitted 指定的降级名（形参 / 运算符重载的 `other`）；空表示按规则自己取。
 * @param isParam 是不是形参。
 * @return 降级后的名字。
 */
/**
 * @brief 分配一个**全程序唯一**的降级名（重名就加 `$序号`）。
 *
 * stackvm 的 `decl` 类型表按名字唯一，所以形参、局部变量、全局变量都得走这里；
 * 重名时改名而不是报错，是为了让"两个函数各写一个 `i`"这种正常写法继续可用。
 */
Memory::String AllocateName(const Memory::String &source)
{
    Memory::String name = source;
    if (g_usedNames.count(name) != 0u)
    {
        name = source + "$" + Memory::toString(++g_localSerial);
    }
    g_usedNames.insert(name);
    return name;
}

/**
 * @brief 在当前作用域登记变量及其类型信息。
 * @param name 名称文本。
 * @param type 类型描述。
 * @param emitted 已生成清理代码的聚合对象集合。
 * @param isConst 是否声明为只读变量。
 * @param bindingKey 内部卫生作用域键；空时使用源名字。
 * @param bindingIdentity 原声明稳定身份，零表示普通名字查找。
 * @retval 生成或读取的结果文本。
 */
Memory::String DeclareVar(const Memory::String &name, const std::shared_ptr<MType> &type,
                          const Memory::String &emitted = Memory::String(), bool isConst = false, const Memory::String &bindingKey = Memory::String(), std::uint32_t bindingIdentity = 0)
{
    MVar var;
    var.type = type;
    var.emitted = emitted.empty() ? AllocateName(name) : emitted;
    var.isConst = isConst;
    var.bindingIdentity = bindingIdentity;
    g_usedNames.insert(var.emitted);
    /* 记下"这个变量归哪一层块"（所有权跟着声明处，见 g_ownedHome） */
    if (!g_ownedScopes.empty()) g_ownedHome[var.emitted] = g_ownedScopes.size() - 1u;
    /* **同一个块里**不许重名（那多半是写错了）；外层已经有同名变量则允许 ——
     * 内层这份把外层**遮蔽**掉，名字解析从最内层往外找（见 Find）。 */
    const auto &key = bindingKey.empty() ? name : bindingKey;
    if (g_scope.back().count(key) != 0u)
    {
        Fail("同一个块里不能重复声明：" + name + "（要遮蔽外层变量就在内层块里声明）");
        return var.emitted;
    }
    g_scope.back()[key] = var;
    return var.emitted;
}
