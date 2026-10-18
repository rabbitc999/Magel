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
 */
MVar *FindIn(const Memory::String &module, const Memory::String &name)
{
    /* 局部作用域：0 号是全局作用域，跳过。 */
    for (std::size_t index = g_scope.size(); index-- > 1u;)
    {
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
 * @retval 生成或读取的结果文本。
 */
Memory::String DeclareVar(const Memory::String &name, const std::shared_ptr<MType> &type,
                          const Memory::String &emitted = Memory::String(), bool isConst = false)
{
    MVar var;
    var.type = type;
    var.emitted = emitted.empty() ? AllocateName(name) : emitted;
    var.isConst = isConst;
    g_usedNames.insert(var.emitted);
    /* 记下"这个变量归哪一层块"（所有权跟着声明处，见 g_ownedHome） */
    if (!g_ownedScopes.empty()) g_ownedHome[var.emitted] = g_ownedScopes.size() - 1u;
    /* **同一个块里**不许重名（那多半是写错了）；外层已经有同名变量则允许 ——
     * 内层这份把外层**遮蔽**掉，名字解析从最内层往外找（见 Find）。 */
    if (g_scope.back().count(name) != 0u)
    {
        Fail("同一个块里不能重复声明：" + name + "（要遮蔽外层变量就在内层块里声明）");
        return var.emitted;
    }
    g_scope.back()[name] = var;
    return var.emitted;
}
