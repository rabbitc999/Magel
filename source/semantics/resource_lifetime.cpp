/**
 * @brief 判断类型是否实现资源协议或包含需要回收的拥有叶子。
 * @param type 类型描述。
 * @return 类型具有 Drop、Closeable 或拥有叶子时返回 true，否则返回 false。
 */
bool IsDestructibleValue(const std::shared_ptr<MType> &type)
{
    const Memory::String owner = StructNameOf(type);
    return !owner.empty() && (StructHasInterface(owner, "Drop") || StructHasInterface(owner, "Closeable") || StructHasOwnedLeaves(owner));
}

/* 资源构造与收尾：close/drop、聚合析构、拥有槽清理及作用域出口。 */

/**
 * @brief 一个命名聚合的构造入口、自动关闭操作与编译器关闭标志。
 */
struct MResourceCloseState
{
    Memory::String flag;
    Memory::Vector<simple_ast::NodePtr> begins, calls;
};
Memory::Map<Memory::String, MResourceCloseState> g_resourceCloseStates;
Memory::Map<Memory::String, Memory::String> g_resourceCloseAliases;
/**
 * @brief 取得不会与用户变量冲突的编译器内部名称。
 * @param base 名称前缀。
 * @return 新的内部名称。
 */
Memory::String AllocateName(const Memory::String &base);
/**
 * @brief 用局部关闭标志守卫关闭操作，保持调用在实际执行的分支上发生一次。
 * @param flag 编译器关闭标志。
 * @param call 原关闭操作。
 * @return 关闭后更新标志的条件语句。
 */
simple_ast::NodePtr GuardResourceClose(const Memory::String &flag, const simple_ast::NodePtr &call)
{
    using simple_ast::Node;
    return Node::ifThenElse(Node::binary("==", Node::var(flag), Node::i(0)),
                            Node::block({ call, Node::assign(flag, Node::i(1)) }), nullptr);
}
/**
 * @brief 为构造过的命名聚合建立关闭标志，并补齐先前生成的构造与自动关闭代码。
 * @param emitted 聚合存储名。
 * @return 标志名；无本地构造入口的借用参数返回空名称。
 */
Memory::String EnsureResourceCloseFlag(const Memory::String &emitted)
{
    auto &state = g_resourceCloseStates[emitted];
    if (!state.flag.empty() || state.begins.empty()) return state.flag;
    using simple_ast::Node;
    state.flag = AllocateName("__chali_closed");
    for (const auto &begin : state.begins)
    {
        begin->kids.push_back(Node::declare(state.flag, simple_ast::Type::i64()));
        begin->kids.push_back(Node::assign(state.flag, Node::i(0)));
    }
    for (const auto &call : state.calls)
    {
        const auto original = std::make_shared<Node>(*call);
        *call = *GuardResourceClose(state.flag, original);
    }
    return state.flag;
}
/**
 * @brief 记录显式关闭的执行路径，保留其他分支的自动关闭。
 * @param emitted 接收者存储名。
 * @param call 用户显式关闭调用。
 * @return 更新关闭标志的表达式；借用参数沿用原调用。
 */
simple_ast::NodePtr FinishExplicitClose(const Memory::String &emitted, const simple_ast::NodePtr &call)
{
    const auto flag = EnsureResourceCloseFlag(emitted);
    if (flag.empty()) return call;
    using simple_ast::Node;
    return Node::sequence({ Node::exprStmt(call), Node::assign(flag, Node::i(1)) }, Node::i(0));
}

/**
 * @brief 标记结构体已定义析构处理。
 * @param emitted 待登记或查询的内部存储名。
 * @param type 类型描述。
 */
void MarkDestructor(const Memory::String &emitted, const std::shared_ptr<MType> &type)
{
    if (g_ownedScopes.empty() || !type || type->kind != MType::Struct || !StructNeedsCleanup(type->name)) return;
    const bool drop = StructHasInterface(type->name, "Drop");
    const bool close = StructHasInterface(type->name, "Closeable");
    const bool aggregate = StructHasOwnedLeaves(type->name);
    if (aggregate) MarkStructNeedsCleanup(type->name);
    g_ownedScopes.back().destructors.push_back(
        MOwnedScope::Destructor{ emitted, type->name, drop, close, aggregate });
    NoteCleanup(emitted, g_ownedScopes.size() - 1u, false, drop, close, type->name, aggregate);
}

/**
 * @brief 这个变量现在是不是还挂着一条自动收尾（`drop` / `close` / 聚合递归回收任一路）。
 * @param emitted 待登记或查询的内部存储名。
 * @return 任一活动作用域仍登记该存储的析构、关闭或聚合清理时返回 true，否则返回 false。
 */
bool HasDestructorNow(const Memory::String &emitted)
{
    for (const MOwnedScope &scope : g_ownedScopes)
        for (const MOwnedScope::Destructor &item : scope.destructors)
            if (item.name == emitted && (item.drop || item.close || item.aggregate)) return true;
    return false;
}

bool g_usesLanguageMove = false;  ///< 当前编译单元是否需要引用可见的移动状态。

/**
 * @brief 包装聚合生命周期操作，跳过资源已被 move 转移的存储。
 * @param owner 聚合类型名。
 * @param value 存储地址，调用者保证没有求值副作用。
 * @param action 待执行操作。
 * @return 受运行时存活状态保护的语句。
 */
simple_ast::NodePtr GuardLiveAggregate(const Memory::String &owner,
                                       const simple_ast::NodePtr &value, const simple_ast::NodePtr &action)
{
    if (!g_usesLanguageMove) return action;
    using simple_ast::Node;
    const auto found = g_structs.find(owner);
    const int bytes = found == g_structs.end() ? 8 : std::max(8, found->second.slotCount * 8);
    return Node::ifThenElse(Node::call("__std_move_live", { "address", "bytes", "type" },
                                       { value, Node::i(bytes), Node::i(TypeIdOf(NamedType(owner))) }),
                            action, nullptr);
}

/**
 * @brief 开始聚合存储的新生命周期。
 * @param value 地址。
 * @param bytes 存储字节数。
 * @return 初始化标记语句。
 */
simple_ast::NodePtr BeginAggregateLifetime(const simple_ast::NodePtr &value, int bytes)
{
    if (!g_usesLanguageMove) return simple_ast::Node::block({});
    using simple_ast::Node;
    const auto begin = Node::block({ Node::exprStmt(Node::call("__std_move_begin", { "address", "bytes" },
                                                               { value, Node::i(std::max(8, bytes)) })) });
    if (value && value->kind == Node::Kind::Var)
    {
        auto &state = g_resourceCloseStates[value->text];
        state.begins.push_back(begin);
        if (!state.flag.empty())
        {
            begin->kids.push_back(Node::declare(state.flag, simple_ast::Type::i64()));
            begin->kids.push_back(Node::assign(state.flag, Node::i(0)));
        }
    }
    return begin;
}

/**
 * @brief 标记已销毁的聚合及其值字段，类型号区分同址同大小的外层与首字段。
 * @param owner 聚合类型名。
 * @param value 存储地址。
 * @param out 接收标记语句。
 */
void AppendMovedAggregate(const Memory::String &owner, const simple_ast::NodePtr &value,
                          Memory::Vector<simple_ast::NodePtr> *out)
{
    using simple_ast::Node;
    const auto found = g_structs.find(owner);
    if (found == g_structs.end()) return;
    out->push_back(Node::exprStmt(Node::call("__std_move_end", { "address", "bytes", "type" },
                                             { value, Node::i(std::max(8, found->second.slotCount * 8)), Node::i(TypeIdOf(NamedType(owner))) })));
    for (const auto &field : found->second.fields)
        if (field.type && field.type->kind == MType::Struct)
            AppendMovedAggregate(field.type->name, Node::offset(value, Node::i(field.slot), 8), out);
}

/**
 * @brief 为聚合生成无显式实参的语言或宿主接口方法调用。
 * @param structName 聚合类型的完整名称。
 * @param methodName 需要查找的接口方法名。
 * @param value 作为 self 传入的聚合地址。
 * @return 找到实现时返回调用语句，没有实现时返回空节点。
 * @note 优先使用语言方法；宿主实现沿用接口登记的原生符号。
 */
simple_ast::NodePtr MakeInterfaceMethodCallAt(const Memory::String &structName,
                                              const Memory::String &methodName,
                                              const simple_ast::NodePtr &value)
{
    const auto definition = g_structs.find(structName);
    if (definition != g_structs.end())
    {
        for (const MMethod &method : definition->second.methods)
            if (!method.isStatic && !method.isConstructor && method.source == methodName && method.params.empty())
                return simple_ast::Node::exprStmt(simple_ast::Node::call(
                    structName + "." + method.name, { "self" }, { value }));
    }
    for (const MHostInterfaceImpl &implementation : g_hostInterfaceImpls)
    {
        if (!implementation.owner || implementation.owner->kind != MType::Struct || implementation.owner->name != structName) continue;
        const auto iface = g_interfaces.find(implementation.interfaceName);
        if (iface == g_interfaces.end()) continue;
        bool declares = false;
        for (const MInterfaceMethod &required : iface->second)
            if (required.name == methodName && required.params.empty())
            {
                declares = true;
                break;
            }
        if (!declares) continue;
        const Memory::String symbol = implementation.methodQualified ? implementation.function + "." + methodName : implementation.function;
        return simple_ast::Node::exprStmt(simple_ast::Node::call(
            symbol, { "__chali_self" }, { value }));
    }
    return nullptr;
}

/**
 * @brief 生成 `structName` 的 `drop(self=value)` 调用；没有 drop 方法返回 nullptr。
 * @param structName 结构体类型名称。
 * @param value 作为 self 传入、且求值无副作用的聚合存储地址。
 * @retval 生成的低层语法节点。
 */
simple_ast::NodePtr MakeDropCallAt(const Memory::String &structName,
                                   const simple_ast::NodePtr &value)
{
    const auto call = MakeInterfaceMethodCallAt(structName, "drop", value);
    if (!call) return nullptr;
    using simple_ast::Node;
    const auto found = g_structs.find(structName);
    const int bytes = found == g_structs.end() ? 8 : std::max(8, found->second.slotCount * 8);
    const auto mark = Node::exprStmt(Node::call("__std_move_end", { "address", "bytes", "type" },
                                                { value, Node::i(bytes), Node::i(TypeIdOf(NamedType(structName))) }));
    return GuardLiveAggregate(structName, value, Node::block({ call, mark }));
}

/**
 * @brief 生成 `structName` 的 `close(self=value)` 调用；没有 close 方法返回 nullptr。
 * @param structName 结构体类型名称。
 * @param value 作为 self 传入、且求值无副作用的聚合存储地址。
 * @retval 生成的低层语法节点。
 */
simple_ast::NodePtr MakeCloseCallAt(const Memory::String &structName,
                                    const simple_ast::NodePtr &value)
{
    const auto call = MakeInterfaceMethodCallAt(structName, "close", value);
    if (!call) return nullptr;
    const auto guarded = GuardLiveAggregate(structName, value, call);
    if (!value || value->kind != simple_ast::Node::Kind::Var) return guarded;
    const auto alias = g_resourceCloseAliases.find(value->text);
    const auto name = alias == g_resourceCloseAliases.end() ? value->text : alias->second;
    auto &state = g_resourceCloseStates[name];
    const auto result = state.flag.empty() ? guarded : GuardResourceClose(state.flag, guarded);
    if (state.flag.empty()) state.calls.push_back(result);
    return result;
}

/**
 * @brief 生成所有者当前值的释放操作，定义见下文。
 * @param emitted 所有者变量的降级名称。
 * @param runtimeArray 是否采用运行时数组块首布局。
 * @return 释放当前值并清槽的语句。
 */
simple_ast::NodePtr MakeDrop(const Memory::String &emitted, bool runtimeArray);

/**
 * @brief 生成聚合拥有叶子的递归回收调用，定义见下文。
 * @param owner 聚合类型的完整名称。
 * @param value 待回收叶子所属聚合的存储地址。
 * @return 递归回收调用；聚合没有拥有叶子时返回空节点。
 */
simple_ast::NodePtr MakeAggregateRelease(const Memory::String &owner, const simple_ast::NodePtr &value);
/**
 * @brief 为主函数正常返回生成全局资源的收尾语句。
 * @return 按登记逆序生成的全局关闭、聚合回收及拥有槽释放语句。
 * @note 应在全部函数降级后生成，以使用完整的全局所有权和关闭路径登记；异常中止不执行此收尾。
 */
Memory::Vector<simple_ast::NodePtr> EmitGlobalCleanupStatements()
{
    Memory::Vector<simple_ast::NodePtr> statements;
    for (auto it = g_globalCleanupItems.rbegin(); it != g_globalCleanupItems.rend(); ++it)
    {
        /* 只有最外那层（全局那层）登记的值才敢收尾；里层块的变量可能没定义过。 */
        if (!it->outermost) continue;
        if (it->close)
            if (const simple_ast::NodePtr call = MakeCloseCallAt(
                    it->owner, simple_ast::Node::var(it->name)))
                statements.push_back(call);
        if (it->aggregate)
        {
            if (const auto release = MakeAggregateRelease(it->owner, simple_ast::Node::var(it->name)))
                statements.push_back(release);
        }
        else if (it->owner.empty())
            statements.push_back(MakeDrop(it->name, it->runtimeArray));
    }
    return statements;
}

/**
 * @brief 按关闭、析构、逆序字段的顺序生成聚合终结代码。
 * @param out 接收终结语句的列表。
 * @param value 待终结聚合的存储地址。
 * @param structName 用于查询接口及字段布局的聚合类型名。
 * @param drop 是否调用当前聚合自己的 Drop 实现。
 * @param close 是否调用当前聚合自己的 Closeable 实现。
 * @note 字段按各自接口递归终结；用户析构不应再次手动析构同一字段。
 */
void AppendDestruction(Memory::Vector<simple_ast::NodePtr> *out, const simple_ast::NodePtr &value,
                       const Memory::String &structName, bool drop, bool close)
{
    const auto definition = g_structs.find(structName);
    if (definition == g_structs.end()) return;
    if (close)
        if (const simple_ast::NodePtr selfClose = MakeCloseCallAt(structName, value))
            out->push_back(selfClose);
    if (drop)
        if (const simple_ast::NodePtr selfDrop = MakeDropCallAt(structName, value))
            out->push_back(selfDrop);
    // 枚举只有当前标签的载荷已构造；不能对 Err 中未激活的 Ok 资源调用 Drop。
    const auto enumeration = g_enums.find(structName);
    if (enumeration != g_enums.end())
    {
        for (const auto &variant : enumeration->second.variants)
        {
            Memory::Vector<simple_ast::NodePtr> payload;
            for (auto it = variant.payloads.rbegin(); it != variant.payloads.rend(); ++it)
            {
                if (!it->type || it->type->kind != MType::Struct) continue;
                for (const auto &field : definition->second.fields)
                    if (field.name == it->storageName)
                    {
                        AppendDestruction(&payload, simple_ast::Node::offset(value, simple_ast::Node::i(field.slot), 8),
                                          it->type->name, StructHasInterface(it->type->name, "Drop"), StructHasInterface(it->type->name, "Closeable"));
                        break;
                    }
            }
            if (!payload.empty()) out->push_back(simple_ast::Node::ifThenElse(
                simple_ast::Node::binary("==", simple_ast::Node::loadInt(value), simple_ast::Node::i(variant.tag)),
                simple_ast::Node::block(payload), nullptr));
        }
        return;
    }
    for (auto it = definition->second.fields.rbegin(); it != definition->second.fields.rend(); ++it)
    {
        if (it->type && it->type->kind == MType::Array && !it->type->dims.empty() && it->type->elem->kind == MType::Struct)
        {
            const int count = static_cast<int>(FixedArrayElementCount(it->type->dims));
            const auto element = it->type->elem;
            for (int index = count; index-- > 0;) AppendDestruction(out,
                                                                    simple_ast::Node::offset(value, simple_ast::Node::i(it->slot + index * (StructStride(element) / 8)), 8),
                                                                    element->name, StructHasInterface(element->name, "Drop"), StructHasInterface(element->name, "Closeable"));
            continue;
        }
        if (!it->type || it->type->kind != MType::Struct) continue;
        /* 取字段**地址**（`基址 + 槽号×8`，见 FieldAddress），不是 `member` 取出的值：
         * `drop` 的 `self` 要的是结构体地址，`member` 会 load 成标量值（拿值当地址就错了）。 */
        AppendDestruction(out, simple_ast::Node::offset(value, simple_ast::Node::i(it->slot), 8),
                          it->type->name, StructHasInterface(it->type->name, "Drop"),
                          StructHasInterface(it->type->name, "Closeable"));
    }
}

/**
 * @brief 登记并生成聚合拥有叶子的递归回收调用。
 * @param ownerName 聚合类型的完整名称。
 * @param value 保存字符串、对象等拥有叶子的聚合地址。
 * @return 拥有叶子存在时返回辅助函数调用，否则返回空节点。
 * @note 调用安排在用户 close/drop 之后；拥有槽由编译器清零，后续回收跳过空槽。
 */
simple_ast::NodePtr MakeAggregateRelease(const Memory::String &ownerName,
                                         const simple_ast::NodePtr &value)
{
    if (ownerName.empty() || !StructHasOwnedLeaves(ownerName)) return nullptr;
    MarkStructNeedsCleanup(ownerName);
    return simple_ast::Node::exprStmt(simple_ast::Node::call(
        AggregateReleaseName(ownerName), { "__chali_agg" }, { value }));
}

/**
 * @brief 把一个聚合值收尾（用户 close / drop + 递归释放它槽里的字符串 / 对象）。
 * @param type 类型描述。
 * @param value 作为 self 传入、且求值无副作用的聚合存储地址。
 * @retval 生成的低层语法节点。
 */
Memory::Vector<simple_ast::NodePtr> MakeAggregateCleanup(
    const std::shared_ptr<MType> &type, const simple_ast::NodePtr &value)
{
    Memory::Vector<simple_ast::NodePtr> stmts;
    const Memory::String owner = StructNameOf(type);
    if (owner.empty()) return stmts;
    AppendDestruction(&stmts, value, owner, StructHasInterface(owner, "Drop"),
                      StructHasInterface(owner, "Closeable"));
    if (const simple_ast::NodePtr release = MakeAggregateRelease(owner, value))
        stmts.push_back(release);
    AppendMovedAggregate(owner, value, &stmts);
    return stmts;
}

/**
 * @brief 一个变量的收尾语句块：自身 `close()` → `drop()` → 递归回收聚合槽。
 * @param item 待追加或处理的节点编号。
 * @retval 生成的低层语法节点。
 */
simple_ast::NodePtr MakeDestructorCall(const MOwnedScope::Destructor &item)
{
    Memory::Vector<simple_ast::NodePtr> stmts;
    AppendDestruction(&stmts, simple_ast::Node::var(item.name), item.owner, item.drop, item.close);
    if (item.aggregate)
        if (const simple_ast::NodePtr release =
                MakeAggregateRelease(item.owner, simple_ast::Node::var(item.name)))
            stmts.push_back(release);
    if (stmts.empty()) return simple_ast::Node::block({});
    const auto cleanup = GuardLiveAggregate(item.owner, simple_ast::Node::var(item.name), simple_ast::Node::block(stmts));
    stmts = { cleanup };
    if (item.temporary)
    {
        // 独立存储随后由所有者清理释放并清槽，不能在释放块之前丢失地址。
        if (!g_parameterValueStorage.count(item.name))
            stmts.push_back(simple_ast::Node::assign(item.name, simple_ast::Node::i(0)));
        return simple_ast::Node::ifThenElse(
            simple_ast::Node::binary("!=", simple_ast::Node::var(item.name), simple_ast::Node::i(0)),
            simple_ast::Node::block(stmts), nullptr);
    }
    if (stmts.empty()) return simple_ast::Node::block({});
    if (stmts.size() == 1u) return stmts.front();
    return simple_ast::Node::block(stmts);
}

/**
 * @brief 取消已转交所有权的内部临时变量的自动回收登记。
 * @param emitted 已由新拥有者接管的临时变量存储名。
 * @note 仅调整内部所有权登记；分支上的显式 drop() 通过清槽和路径守卫保留其他分支清理。
 */
void UnmarkOwned(const Memory::String &emitted)
{
    const auto hit = [&emitted](const MOwnedScope::Item &item) { return item.name == emitted; };
    for (MOwnedScope &scope : g_ownedScopes)
    {
        scope.names.erase(std::remove_if(scope.names.begin(), scope.names.end(), hit),
                          scope.names.end());
    }
}

/**
 * @brief 这个变量现在是不是"某个还开着的块"的所有者。
 * @param emitted 待登记或查询的内部存储名。
 * @return 任一活动作用域仍拥有该存储时返回 true，否则返回 false。
 */
bool IsOwnedNow(const Memory::String &emitted)
{
    for (const MOwnedScope &scope : g_ownedScopes)
    {
        for (const MOwnedScope::Item &item : scope.names)
        {
            if (item.name == emitted) return true;
        }
    }
    return false;
}

/**
 * @brief 分配不会与用户变量冲突的临时名称。
 * @param base 名称前缀。
 * @return 唯一名称。
 */
Memory::String AllocateName(const Memory::String &base);
/**
 * @brief 检查并显式销毁值及其资源叶子，自动清理通过生命周期标记跳过已销毁值。
 * @param ownerName 所属类型。
 * @param emitted 命名变量的存储名。
 * @param receiver 接收者地址，只求值一次。
 * @param dropCall 原调用节点。
 * @return 带生命周期守卫的清理语句块。
 */
simple_ast::NodePtr FinishExplicitDrop(const Memory::String &ownerName,
                                       const Memory::String &emitted,
                                       const simple_ast::NodePtr &receiver,
                                       const simple_ast::NodePtr &dropCall)
{
    using simple_ast::Node;
    (void)dropCall;
    const auto address = AllocateName("__chali_explicit_drop");
    if (!emitted.empty())
    {
        g_resourceCloseAliases[address] = emitted;
        if (StructHasInterface(ownerName, "Closeable")) EnsureResourceCloseFlag(emitted);
    }
    auto stmts = MakeAggregateCleanup(NamedType(ownerName), Node::var(address));
    (void)emitted;
    const auto type = NamedType(ownerName);
    const int bytes = std::max(8, StructStride(type));
    return Node::block({ Node::declare(address, simple_ast::Type::i64()), Node::assign(address, receiver),
                         Node::exprStmt(Node::call("__std_move_require", { "address", "bytes", "type" },
                                                   { Node::var(address), Node::i(bytes), Node::i(TypeIdOf(type)) })),
                         Node::block(stmts) });
}

/**
 * @brief 按拥有槽状态释放有效资源并清零，空槽由编译器守卫跳过。
 * @param emitted 变量名（四元式里的名字）。
 * @param runtimeArray 是不是运行时数组 —— 它的句柄是"元素起始"，块首在它前面 8 字节，
 *        所以还的是 `句柄 - 8`（和显式 `drop` 一个规矩）。
 * @return 语句节点。
 */
simple_ast::NodePtr MakeDrop(const Memory::String &emitted, bool runtimeArray = false)
{
    const auto storage = g_referenceStorage.find(emitted);
    const bool indirect = storage != g_referenceStorage.end();
    simple_ast::NodePtr value = indirect ? simple_ast::Node::loadInt(simple_ast::Node::var(storage->second)) : simple_ast::Node::var(emitted);
    const auto handle = value;
    if (runtimeArray || g_arrayElementTypes.count(emitted) != 0u)
    {
        value = simple_ast::Node::binary("-", value, simple_ast::Node::i(8));
    }
    const auto ownedType = g_opcodeValueTypes.find(emitted);
    const bool referenceAllocation = std::any_of(g_referenceStorage.begin(), g_referenceStorage.end(), [&](const auto &entry) { return entry.second == emitted; });
    const auto *opcode = referenceAllocation || ownedType == g_opcodeValueTypes.end() ? nullptr : FindOpcodeDefinition(ownedType->second);
    const auto opcodeRelease = opcode ? simple_ast::Node::exprStmt(simple_ast::Node::call(opcode->destroy, { "__chali_v" }, { value })) : simple_ast::Node::release(value);
    const auto release = simple_ast::Node::ifThenElse(
        simple_ast::Node::binary("!=", handle, simple_ast::Node::i(0)), opcodeRelease, nullptr);
    /* 释放资源后清空拥有槽；引用单元本身保留到其所属作用域退出。
     * 新值已由 DeferNewValue 保存，因此自赋值拼接不会提前销毁源资源。 */
    return simple_ast::Node::block({
        release,
        indirect ? simple_ast::Node::storeAt(simple_ast::Node::var(storage->second),
                                             simple_ast::Node::i(0), simple_ast::Type::i64()) :
                   simple_ast::Node::assign(emitted, simple_ast::Node::i(0)),
    });
}

/**
 * @brief 先释放数组拥有元素，再回收命名拥有槽的当前资源。
 * @param name 待清理的内部存储名。
 * @param runtimeArray 是否按运行时数组块首布局回收。
 * @param out 接收元素清理与资源释放语句的列表。
 */
void AppendOwnedDropByName(const Memory::String &name, bool runtimeArray,
                           Memory::Vector<simple_ast::NodePtr> *out)
{
    const auto element = g_arrayElementTypes.find(name);
    if (element != g_arrayElementTypes.end() && ArrayElementNeedsRelease(element->second))
    {
        const auto fixed = g_arrayFixedCounts.find(name);
        const int fixedCount = fixed == g_arrayFixedCounts.end() ? -1 : fixed->second;
        if (const simple_ast::NodePtr release =
                MakeArrayElementRelease(name, element->second, fixedCount))
            out->push_back(release);
    }
    out->push_back(MakeDrop(name, runtimeArray));
}

/**
 * @brief 根据已知类型为临时拥有值生成资源及数组元素回收。
 * @param name 保存临时拥有值的内部存储名。
 * @param type 决定数组布局和元素清理方式的语言类型。
 * @param out 接收生成的回收语句。
 * @note 数组类型补齐元素登记；其他类型复用命名拥有槽的回收入口。
 */
void AppendOwnedDrop(const Memory::String &name, const std::shared_ptr<MType> &type,
                     Memory::Vector<simple_ast::NodePtr> *out)
{
    if (!type || type->kind != MType::Array)
    {
        out->push_back(MakeDrop(name, false));
        return;
    }
    const bool runtimeArray = IsRuntimeArray(type);
    g_arrayElementTypes[name] = type->elem;
    if (!runtimeArray)
    {
        g_arrayFixedCounts[name] = static_cast<int>(FixedArrayElementCount(type->dims));
    }
    AppendOwnedDropByName(name, runtimeArray, out);
}

/**
 * @brief 按构造的逆序清理当前列表中的拥有资源。
 * @param names 按登记顺序排列的拥有槽列表。
 * @param keep 返回或转交给调用方而须保留的存储名；空串表示不保留。
 * @param out 接收生成的资源回收语句。
 */
void AppendDrops(const Memory::Vector<MOwnedScope::Item> &names, const Memory::String &keep,
                 Memory::Vector<simple_ast::NodePtr> *out)
{
    for (auto it = names.rbegin(); it != names.rend(); ++it)
    {
        if (!keep.empty() && it->name == keep) continue; /* 被 return 出去的：不释放 */
        /* 数组先释放拥有元素，再归还独立存储；全局数据区块的软释放为空操作。 */
        AppendOwnedDropByName(it->name, it->runtimeArray, out);
    }
}

/**
 * @brief 出块：释放这一层（调用方负责把 `g_ownedScopes` 弹掉）。
 * @param scope 待查询或清理的词法作用域。
 * @param keep 清理时需要保留的对象集合。
 * @param out 接收处理结果的输出对象。
 */
void AppendScopeDrops(const MOwnedScope &scope, const Memory::String &keep,
                      Memory::Vector<simple_ast::NodePtr> *out)
{
    out->insert(out->end(), scope.finalizers.rbegin(), scope.finalizers.rend());
    for (auto it = scope.destructors.rbegin(); it != scope.destructors.rend(); ++it)
        if (keep.empty() || it->name != keep) out->push_back(MakeDestructorCall(*it));
    AppendDrops(scope.names, keep, out);
}

/**
 * @brief 为跨越循环或块的退出生成内层作用域清理。
 * @param depth 退出后保留的作用域深度。
 * @param keep 本次退出转交给调用方而不释放的存储名。
 * @param out 接收从内向外排列的作用域清理语句。
 */
void AppendDropsAbove(std::size_t depth, const Memory::String &keep,
                      Memory::Vector<simple_ast::NodePtr> *out)
{
    for (std::size_t index = g_ownedScopes.size(); index > depth; --index)
    {
        AppendScopeDrops(g_ownedScopes[index - 1u], keep, out);
    }
}

/**
 * @brief `return`：释放本函数里所有块（`keep` 里那个变量除外 —— 它的所有权给调用方）。
 * @param keep 清理时需要保留的对象集合。
 * @param out 接收处理结果的输出对象。
 */
void AppendDropsAll(const Memory::String &keep, Memory::Vector<simple_ast::NodePtr> *out)
{
    AppendDropsAbove(0u, keep, out);
}

/**
 * @brief return 可同时把多个枚举载荷交给调用方；这些变量在当前返回路径都不能释放。
 * @param keep 清理时需要保留的对象集合。
 * @param out 接收处理结果的输出对象。
 */
void AppendDropsAllExcept(const Memory::Set<Memory::String> &keep,
                          Memory::Vector<simple_ast::NodePtr> *out)
{
    for (std::size_t index = g_ownedScopes.size(); index > 0u; --index)
    {
        const MOwnedScope &scope = g_ownedScopes[index - 1u];
        out->insert(out->end(), scope.finalizers.rbegin(), scope.finalizers.rend());
        for (auto it = scope.destructors.rbegin(); it != scope.destructors.rend(); ++it)
            if (keep.find(it->name) == keep.end()) out->push_back(MakeDestructorCall(*it));
        for (auto it = scope.names.rbegin(); it != scope.names.rend(); ++it)
            if (keep.find(it->name) == keep.end())
                AppendOwnedDropByName(it->name, it->runtimeArray, out);
    }
}
