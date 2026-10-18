/* ---------- 聚合值的递归所有权（结构体 / 枚举载荷里的字符串与对象槽） ----------
 *
 * 聚合值（结构体 / 枚举的合成存储体）按**值语义**保存它槽位里的字符串与对象句柄：
 * 存进槽里的那一刻复制一份归这个聚合独占，聚合销毁（出块 / return / 被重新赋值 /
 * 被 delete）时按叶子逐个递归软释放。这样"把新拼的串塞进 Result 载荷再返回"不再漏，
 * 也不会出现"聚合还指着已释放的串"。
 *
 * 只有字符串与对象句柄（`bint` / 容器 / `obj` / 接口引用）进这套规则；运行时数组
 * 与宿主资源句柄仍是**借用**（谁 `new` 谁 `delete`），裸引用也一样。
 */

/**
 * @brief 聚合里的一个槽是不是"由聚合独占"的堆资源。
 * @param type 类型描述。
 * @retval 满足检查条件或操作完成时为 true，检查失败或无法完成时为 false。
 */
bool LeafOwnedByAggregate(const std::shared_ptr<MType> &type)
{
    return type && (type->kind == MType::String || IsObjectLike(type));
}

/**
 * @brief 这个结构体 / 枚举存储体有没有需要递归回收的叶子。
 * @param structName 结构体类型名称。
 * @retval 满足检查条件或操作完成时为 true，检查失败或无法完成时为 false。
 */
bool StructHasOwnedLeaves(const Memory::String &structName)
{
    const MStructDef *definition = FindStruct(structName);
    if (definition == nullptr) return false;
    for (const auto &leaf : definition->leaves)
        if (LeafOwnedByAggregate(leaf.second)) return true;
    return false;
}

/**
 * @brief 递归释放函数名（`__magel_agg_rel_<类型名>`）；用到才生成。
 * @param owner 所属对象或类型信息。
 * @retval 生成或读取的结果文本。
 */
Memory::String AggregateReleaseName(const Memory::String &owner)
{
    return "__magel_agg_rel_" + owner;
}

/**
 * @brief 递归深拷贝函数名（`__magel_agg_cpy_<类型名>`）；用到才生成。
 * @param owner 所属对象或类型信息。
 * @retval 生成或读取的结果文本。
 */
Memory::String AggregateCopyName(const Memory::String &owner)
{
    return "__magel_agg_cpy_" + owner;
}

/**
 * 需要生成 `__magel_agg_rel_*` / `__magel_agg_cpy_*` 的类型（收尾时统一发）。
 */
Memory::Set<Memory::String> g_needsAggregateRelease;
Memory::Set<Memory::String> g_needsAggregateCopy;
Memory::Set<Memory::String> g_needsCaptureDestructor;  ///< 拥有型闭包捕获需要的用户析构助手。

/* ── 运行时数组的元素释放 ──────────────────────────────────────────────
 *
 * `T[] a = new[n];` 的元素和字段 / 容器元素一样是**值**：字符串、对象、带字符串 / 对象
 * 叶子的结构体都归数组所有。出块 / `delete` 时按元素逐个释放，再还数组块本身。
 */

/**
 * @brief 查询聚合是否包含用户析构或拥有叶子。
 * @param structName 聚合类型。
 * @return 需要清理时为 true。
 */
bool StructNeedsCleanup(const Memory::String &structName);

/**
 * @brief 这个元素类型要不要逐个释放。
 * @param element 数组或容器的元素类型。
 * @retval 满足检查条件或操作完成时为 true，检查失败或无法完成时为 false。
 */
bool ArrayElementNeedsRelease(const std::shared_ptr<MType> &element)
{
    if (!element) return false;
    if (element->kind == MType::String || IsObjectLike(element)) return true;
    if (element->kind == MType::Struct) return StructNeedsCleanup(element->name);
    return false;
}

/**
 * @brief 元素释放助手名（`__magel_arr_rel_<类型名>`）；类型名本来就只含标识符字符。
 * @param element 数组或容器的元素类型。
 * @retval 生成或读取的结果文本。
 */
Memory::String ArrayReleaseName(const std::shared_ptr<MType> &element)
{
    return "__magel_arr_rel_" + (element ? element->name : Memory::String("unknown"));
}

/**
 * 需要生成 `__magel_arr_rel_*` 的元素类型（收尾时统一发）。
 */
Memory::Map<Memory::String, std::shared_ptr<MType>> g_needsArrayRelease;

/**
 * @brief 生成"释放运行时数组元素"的语句：`__magel_arr_rel_<T>(arr, 数组长度)`。
 * @param name 数组变量名。
 * @param element 元素类型。
 * @param fixedCount 定长数组的编译期元素个数；< 0 表示运行时长度的数组（从对象头读）。
 */
simple_ast::NodePtr MakeArrayElementRelease(const Memory::String &name,
                                            const std::shared_ptr<MType> &element,
                                            int fixedCount = -1)
{
    if (!ArrayElementNeedsRelease(element)) return nullptr;
    g_needsArrayRelease[ArrayReleaseName(element)] = element;
    /* 运行时数组的长度存在对象里（句柄前 8 字节，和 `len(a)` 同一把尺）；
     * 定长数组的长度直接是编译期常量。 */
    const auto storage = g_referenceStorage.find(name);
    const auto value = storage == g_referenceStorage.end() ? simple_ast::Node::var(name) : simple_ast::Node::loadInt(simple_ast::Node::var(storage->second));
    const simple_ast::NodePtr count =
        fixedCount >= 0 ? simple_ast::Node::i(fixedCount) : simple_ast::Node::loadInt(simple_ast::Node::binary("-", value, simple_ast::Node::i(8)));
    const auto release = simple_ast::Node::exprStmt(simple_ast::Node::call(
        ArrayReleaseName(element), { "__magel_arr", "__magel_count" },
        { value, count }));
    return fixedCount >= 0 ? release : simple_ast::Node::ifThenElse(simple_ast::Node::binary("!=", value, simple_ast::Node::i(0)), release, nullptr);
}

/**
 * @brief 标记结构体类型需要执行资源清理。
 * @param structName 结构体类型名称。
 */
void MarkStructNeedsCleanup(const Memory::String &structName)
{
    if (StructHasOwnedLeaves(structName)) g_needsAggregateRelease.insert(structName);
}

/**
 * @brief 结构体（含枚举）需不需要在出块 / 停机时做**任何**收尾（用户析构或递归回收）。
 * @param structName 结构体类型名称。
 * @retval 满足检查条件或操作完成时为 true，检查失败或无法完成时为 false。
 */
bool StructNeedsCleanup(const Memory::String &structName)
{
    return StructNeedsDestruction(structName) || StructHasOwnedLeaves(structName);
}

/**
 * @brief 标记结构体已定义析构处理。
 * @param emitted 已生成清理代码的聚合对象集合。
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
 * @brief 显式调用过 `drop()` / `close()` / `delete` 之后，把对应的一次性析构从自动名单里摘掉。
 *
 * `drop` / `close` 为 true 表示"这一路已经显式跑过，别再自动跑"。两个都摘掉后整条记录删掉。
 * 这是"drop 不能重复调用、close 不能重复关"的编译期保证；只能认**光秃秃的变量**。
 */
void UnmarkDestructor(const Memory::String &emitted, bool drop, bool close)
{
    /* `drop()` 是终结操作：显式跑过一次之后，聚合槽的递归回收也一并取消。 */
    const bool aggregate = drop;
    for (MOwnedScope &scope : g_ownedScopes)
    {
        for (MOwnedScope::Destructor &item : scope.destructors)
        {
            if (item.name != emitted) continue;
            if (drop) item.drop = false;
            if (close) item.close = false;
            if (aggregate) item.aggregate = false;
        }
        scope.destructors.erase(
            std::remove_if(scope.destructors.begin(), scope.destructors.end(),
                           [](const MOwnedScope::Destructor &item) {
                               return !item.drop && !item.close && !item.aggregate;
                           }),
            scope.destructors.end());
    }
    /* 显式析构过了（`delete value;` / `x.drop()` / `x.close()`）：停机清理别再跑一次。 */
    for (MCleanupItem &item : g_cleanupItems)
    {
        if (item.name != emitted) continue;
        if (drop) item.drop = false;
        if (close) item.close = false;
        if (aggregate) item.aggregate = false;
    }
    /* 全局项的快照同样要改：它会在函数体全部降级完后才展开成停机清理语句。 */
    for (MCleanupItem &item : g_globalCleanupItems)
    {
        if (item.name != emitted) continue;
        if (drop) item.drop = false;
        if (close) item.close = false;
        if (aggregate) item.aggregate = false;
    }
}

/**
 * @brief `delete x;` 之后把聚合递归回收也摘掉（`x` 可能没有 Drop / Closeable）。
 * @param emitted 已生成清理代码的聚合对象集合。
 */
void UnmarkAggregate(const Memory::String &emitted)
{
    for (MOwnedScope &scope : g_ownedScopes)
    {
        for (MOwnedScope::Destructor &item : scope.destructors)
            if (item.name == emitted) item.aggregate = false;
        scope.destructors.erase(
            std::remove_if(scope.destructors.begin(), scope.destructors.end(),
                           [](const MOwnedScope::Destructor &item) {
                               return !item.drop && !item.close && !item.aggregate;
                           }),
            scope.destructors.end());
    }
    for (MCleanupItem &item : g_cleanupItems)
        if (item.name == emitted) item.aggregate = false;
    for (MCleanupItem &item : g_globalCleanupItems)
        if (item.name == emitted) item.aggregate = false;
}

/**
 * @brief 这个变量现在是不是还挂着一条自动收尾（`drop` / `close` / 聚合递归回收任一路）。
 * @param emitted 已生成清理代码的聚合对象集合。
 * @retval 满足检查条件或操作完成时为 true，检查失败或无法完成时为 false。
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
 * @brief 包装聚合生命周期操作，跳过资源已被 std::lang::move 转移的存储。
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
    return Node::ifThenElse(Node::call("__vm_move_live", { "address", "bytes", "type" },
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
    return simple_ast::Node::exprStmt(simple_ast::Node::call("__vm_move_begin", { "address", "bytes" },
                                                             { value, simple_ast::Node::i(std::max(8, bytes)) }));
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
    out->push_back(Node::exprStmt(Node::call("__vm_move_end", { "address", "bytes", "type" },
                                             { value, Node::i(std::max(8, found->second.slotCount * 8)), Node::i(TypeIdOf(NamedType(owner))) })));
    for (const auto &field : found->second.fields)
        if (field.type && field.type->kind == MType::Struct)
            AppendMovedAggregate(field.type->name, Node::offset(value, Node::i(field.slot), 8), out);
}

/**
 * @brief 生成 `structName` 的 `methodName(self=value)` 零参调用。
 *
 * 先找 MAGEL 方法（`void drop() { … }`），没有再找胶水 `impl X : I = 符号` 的宿主实现
 * （按 `<符号>.<方法名>` 或单方法接口的裸符号查）。都没有返回 nullptr。
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
            symbol, { "__magel_self" }, { value }));
    }
    return nullptr;
}

/**
 * @brief 生成 `structName` 的 `drop(self=value)` 调用；没有 drop 方法返回 nullptr。
 * @param structName 结构体类型名称。
 * @param value 待转换、保存或参与计算的值。
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
    const auto mark = Node::exprStmt(Node::call("__vm_move_end", { "address", "bytes", "type" },
                                                { value, Node::i(bytes), Node::i(TypeIdOf(NamedType(structName))) }));
    return GuardLiveAggregate(structName, value, Node::block({ call, mark }));
}

/**
 * @brief 生成 `structName` 的 `close(self=value)` 调用；没有 close 方法返回 nullptr。
 * @param structName 结构体类型名称。
 * @param value 待转换、保存或参与计算的值。
 * @retval 生成的低层语法节点。
 */
simple_ast::NodePtr MakeCloseCallAt(const Memory::String &structName,
                                    const simple_ast::NodePtr &value)
{
    const auto call = MakeInterfaceMethodCallAt(structName, "close", value);
    return call ? GuardLiveAggregate(structName, value, call) : nullptr;
}

/**
 * @brief 生成所有者当前值的释放操作，定义见下文。
 * @param emitted 所有者变量的降级名称。
 * @param runtimeArray 是否采用运行时数组块首布局。
 * @return 释放当前值并清槽的语句。
 */
simple_ast::NodePtr MakeDrop(const Memory::String &emitted, bool runtimeArray);

/**
 * @brief 程序**正常停机**（`main` 返回）时全局值的收尾语句。
 *
 * 对实现了 `Closeable` 的全局跑一次 `close()` 关闭宿主句柄；被取引用的资源变量
 * 按当前存储值释放，聚合递归释放其拥有的叶子，避免正常退出后留下泄漏诊断。`std::lang::panic` 之类的异常中止不跑（与 `Drop` 的既有语义一致）。
 *
 * 必须在函数体全部降级完**之后**才调用：函数体里对全局写的 `g.close()` / `delete g;`
 * 会通过 `UnmarkDestructor` 改 `g_globalCleanupItems`，早展开会双重关闭。
 */
simple_ast::NodePtr MakeAggregateRelease(const Memory::String &owner, const simple_ast::NodePtr &value);
/**
 * @brief 为全局变量生成资源清理语句。
 * @retval 生成的低层语法节点。
 */
Memory::Vector<simple_ast::NodePtr> EmitGlobalCleanupStatements()
{
    Memory::Vector<simple_ast::NodePtr> statements;
    for (auto it = g_globalCleanupItems.rbegin(); it != g_globalCleanupItems.rend(); ++it)
    {
        /* 只有最外那层（全局那层）登记的值才敢收尾；里层块的变量可能没定义过。 */
        if (!it->outermost) continue;
        if (it->close)
            if (const simple_ast::NodePtr call = MakeInterfaceMethodCallAt(
                    it->owner, "close", simple_ast::Node::var(it->name)))
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
 * @brief 递归生成一个值（`value`，类型 `structName`）的析构。
 *
 * 顺序：自己的 `close()` → 自己的 `drop()` → 逆序递归字段。
 *
 * `close()` 必须在 `drop()` 前：`drop()` 是**终结**操作，跑完之后系统不能再自动回收
 * （否则"drop 调用后还有 close"就违背了这条）。与 C++ 的析构一致：结构体的析构 =
 * 用户 `drop()`（只清自己的直接资源）+ 编译器自动递归析构字段，所以结构体的 `drop()`
 * **不要**再手写 `self.字段.drop()`，否则会双重析构。
 *
 * @param drop /
 * @param close 这一层要不要跑对应的一步；递归到字段时按字段自己的接口重新算，
 *        因为"用户显式调用过 close"只对本层的那个变量成立。
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
            int count = 1;
            for (const auto dim : it->type->dims) count *= dim;
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
 * @brief 递归释放聚合槽里归它所有的字符串 / 对象；不是聚合返回 nullptr。
 *
 * 实际代码由收尾时统一生成的 `__magel_agg_rel_<类型>` 负责（按叶子逐个软释放），
 * 所以这里只发一条调用，并把该类型登记进"用到才生成"清单。
 * 顺序上放在用户 `close()` / `drop()` **之后**：用户析构可能自己把某个字段还了，
 * 软释放重入是空操作；反过来先释放会让用户析构读到已还的槽。
 */
simple_ast::NodePtr MakeAggregateRelease(const Memory::String &ownerName,
                                         const simple_ast::NodePtr &value)
{
    if (ownerName.empty() || !StructHasOwnedLeaves(ownerName)) return nullptr;
    MarkStructNeedsCleanup(ownerName);
    return simple_ast::Node::exprStmt(simple_ast::Node::call(
        AggregateReleaseName(ownerName), { "__magel_agg" }, { value }));
}

/**
 * @brief 把一个聚合值收尾（用户 close / drop + 递归释放它槽里的字符串 / 对象）。
 * @param type 类型描述。
 * @param value 待转换、保存或参与计算的值。
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
 * @brief 把一个变量从"自动释放名单"里摘掉（所有还开着的块里都摘）。
 *
 * 用在**显式 `delete`** 上：程序自己还回去了，块出口就别再还一次。
 * （`x = <新造的值>` 之后会被 `MarkOwned` 重新登记，所以"删了再要一块"也没问题。）
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
 * @param emitted 已生成清理代码的聚合对象集合。
 * @retval 满足检查条件或操作完成时为 true，检查失败或无法完成时为 false。
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
 * @brief 显式销毁值及其资源叶子，重复调用无效果，保留存储的自动释放。
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
    const auto address = AllocateName("__magel_explicit_drop");
    auto stmts = MakeAggregateCleanup(NamedType(ownerName), Node::var(address));
    if (!emitted.empty())
    {
        UnmarkDestructor(emitted, true, true);
        UnmarkAggregate(emitted);
    }
    return Node::block({ Node::declare(address, simple_ast::Type::i64()), Node::assign(address, receiver),
                         GuardLiveAggregate(ownerName, Node::var(address), Node::block(stmts)) });
}

/**
 * @brief 软释放所有者持有的值，并把变量槽清零，防止循环重入误释放复用地址。
 * @param emitted 变量名（四元式里的名字）。
 * @param runtimeArray 是不是运行时数组 —— 它的句柄是"元素起始"，块首在它前面 8 字节，
 *        所以还的是 `句柄 - 8`（和显式 `delete` 一个规矩）。
 * @return 语句节点。
 */
simple_ast::NodePtr MakeDrop(const Memory::String &emitted, bool runtimeArray = false)
{
    const auto storage = g_referenceStorage.find(emitted);
    const bool indirect = storage != g_referenceStorage.end();
    simple_ast::NodePtr value = indirect ? simple_ast::Node::loadInt(simple_ast::Node::var(storage->second)) : simple_ast::Node::var(emitted);
    const auto handle = value;
    if (runtimeArray)
    {
        value = simple_ast::Node::binary("-", value, simple_ast::Node::i(8));
    }
    const auto release = runtimeArray ? simple_ast::Node::ifThenElse(
                                            simple_ast::Node::binary("!=", handle, simple_ast::Node::i(0)), simple_ast::Node::release(value), nullptr) :
                                        simple_ast::Node::release(value);
    /* 软释放只检查当前地址是否活跃，无法区分堆地址复用后的新所有者。
     * 出块后保留旧地址，下一轮临时赋值前的 MakeDrop 就可能释放别人的新字符串。
     * 数组先释放元素、再释放块、最后清句柄；旧值在赋值路径中已由 DeferNewValue 保存。 */
    return simple_ast::Node::block({
        release,
        indirect ? simple_ast::Node::storeAt(simple_ast::Node::var(storage->second),
                                             simple_ast::Node::i(0), simple_ast::Type::i64()) :
                   simple_ast::Node::assign(emitted, simple_ast::Node::i(0)),
    });
}

/**
 * @brief 数组收尾的完整一步：先按元素释放，再软释放数组块本身。
 *
 * 数组元素是**值**：字符串 / 对象 / 带叶子的结构体都归数组所有。运行时数组的长度从
 * 对象头读，定长数组用登记下来的编译期个数。不是数组（没登记过）时就是一句普通的
 * `MakeDrop`。
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
 * @brief 同上，但类型是**当场知道**的（临时数组值没进 `g_arrayElementTypes`）。
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
        int count = 1;
        for (const int dim : type->dims) count *= dim;
        g_arrayFixedCounts[name] = count;
    }
    AppendOwnedDropByName(name, runtimeArray, out);
}

/**
 * @brief 把变量按"逆序"软释放，追加进 `out`。
 *
 * 顺序是**后声明的先释放**（后建的依赖前建的：`a` 可能被 `b` 用过），块与块之间
 * 也是"里层先、外层后"。
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
 * @brief `break` / `continue`：释放比 `depth` 更深的那些块（`depth` = 循环那层深度）。
 * @param keep 不释放的那个变量（`return` 用；空串 = 都释放）。
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

/**
 * @brief 这个类型是不是"块作用域所有"的那一类。
 *
 * 只有**运行时数组**与**字符串**：前者是"长度跟着对象走的表"，后者是堆上的字节串。
 * 可写引用（`T mut&`）**不算** —— 它归程序自己 `delete`（见 README 的三档所有权）。
 */
/**
 * 类型的人类可读写法（`int64` / `std::lang::List<string>` / …）；实现在下面。
 */
Memory::String Describe(const std::shared_ptr<MType> &type);
/**
 * 容器的元素类型说明（实现在下面）。
 */
bool CheckContainerElement(const std::shared_ptr<MType> &type, const Memory::String &where);

/**
 * @brief 是不是容器类型（`vec` / `set` / `map`）。
 * @param type 类型描述。
 * @retval 满足检查条件或操作完成时为 true，检查失败或无法完成时为 false。
 */
bool IsContainer(const std::shared_ptr<MType> &type)
{
    return type && type->kind == MType::Container;
}

/**
 * @brief 是不是"对象句柄"类（`bint` / 容器 / `obj` / 接口引用）：值不落在数据区，
 *        释放走对象表。
 *
 * 归到这一类的类型自动获得三件事：**值语义**（赋值 / 初始化时 `__vm_object_copy`
 * 复制一份）、**块作用域所有**（出块自动释放）、`delete` 能用。
 *
 * 接口引用也算 —— 它是"地址 + 类型号"的**胖引用**，存在对象表里；于是
 * `Shape mut& q = b.get();` 这样的写法天然就是值语义、也归声明它的块所有。
 */
bool IsObjectLike(const std::shared_ptr<MType> &type)
{
    return type && (type->kind == MType::BigInt || type->kind == MType::BigFloat || type->kind == MType::Container || type->kind == MType::Func || type->kind == MType::Obj || IsInterfacePointer(type));
}

/**
 * @brief 宿主资源句柄：在对象表里活着，但不做值语义复制。
 * @param type 类型描述。
 * @retval 满足检查条件或操作完成时为 true，检查失败或无法完成时为 false。
 */
bool IsResourceLike(const std::shared_ptr<MType> &type)
{
    return type && (type->kind == MType::File || type->kind == MType::Proc);
}

/**
 * 容器方法的返回值种类（决定表达式类型）。
 */
enum class MContainerResult
{
    Process,  ///< 过程：返回值不用（int 占位）
    Self,     ///< `copy()` 返回相同容器类型
    Element,  ///< 元素（`vec.at` / `set.at` / `vec.pop`）
    Value,    ///< `map.get` / `map.valAt` 的值
    Key,      ///< `map.keyAt` 的键
    Bool,     ///< 逻辑算法结果
};

/**
 * @brief 容器"方法 → 原生函数"表。
 *
 * `shape` 一个字符一个形参：
 *   * `h` 接收者句柄；`i` 第 1 个实参（整数：下标）；
 *   * `v` 第 1 个实参（按元素类型：值 / 键）；`n` = 它的 isText 标志；
 *     `o` = 它是不是对象句柄；
 *   * `w` 第 2 个实参（按元素类型）；`m` = 它的 isText 标志；
 *     `p` = 它是不是对象句柄；
 *   * `k` 第 1 个实参（按 map 的**键**类型）；`t` = 它的 isText 标志。
 *
 * `indexTest` = "返回值是下标 / -1"，整个表达式要写成 `结果 != -1`（`vec.contains`）。
 */
struct MContainerMethod
{
    const char *name;
    const char *native;
    int arity;
    const char *shape;
    MContainerResult result = MContainerResult::Process;
    bool indexTest = false;
};

/**
 * `std::lang::List<T>` 的方法。
 */
const MContainerMethod kVecMethods[] = {
    { "copy", "__vm_object_copy", 0, "h", MContainerResult::Self, false },
    { "push", "__vm_vec_push", 1, "hvno", MContainerResult::Process, false },
    { "insert", "__vm_vec_insert", 2, "hiwmp", MContainerResult::Process, false },
    { "at", "__vm_vec_at", 1, "hino", MContainerResult::Element, false },
    { "pop", "__vm_vec_pop", 0, "hno", MContainerResult::Element, false },
    { "remove", "__vm_vec_remove", 1, "hi", MContainerResult::Process, false },
    { "indexOf", "__vm_vec_find", 1, "hvno", MContainerResult::Process, false },
    { "contains", "__vm_vec_find", 1, "hvno", MContainerResult::Process, true },
    { "has", "__vm_vec_find", 1, "hvno", MContainerResult::Process, true },
    { "clear", "__vm_vec_clear", 0, "h", MContainerResult::Process, false },
    { "len", "__vm_vec_len", 0, "h", MContainerResult::Process, false },
};

/**
 * `std::lang::Set<T>` 的方法。
 */
const MContainerMethod kSetMethods[] = {
    { "copy", "__vm_object_copy", 0, "h", MContainerResult::Self, false },
    { "add", "__vm_set_add", 1, "hvno", MContainerResult::Process, false },
    { "has", "__vm_set_has", 1, "hvno", MContainerResult::Process, false },
    { "contains", "__vm_set_has", 1, "hvno", MContainerResult::Process, false },
    { "remove", "__vm_set_remove", 1, "hvno", MContainerResult::Process, false },
    { "at", "__vm_set_at", 1, "hino", MContainerResult::Element, false },
    { "clear", "__vm_set_clear", 0, "h", MContainerResult::Process, false },
    { "len", "__vm_set_len", 0, "h", MContainerResult::Process, false },
};

/**
 * `std::lang::Map<K,V>` 的方法。
 */
const MContainerMethod kMapMethods[] = {
    { "copy", "__vm_object_copy", 0, "h", MContainerResult::Self, false },
    { "put", "__vm_map_put", 2, "hktowmp", MContainerResult::Process, false },
    { "has", "__vm_map_has", 1, "hkto", MContainerResult::Process, false },
    { "contains", "__vm_map_has", 1, "hkto", MContainerResult::Process, false },
    { "remove", "__vm_map_remove", 1, "hkto", MContainerResult::Process, false },
    { "keyAt", "__vm_map_key_at", 1, "hito", MContainerResult::Key, false },
    { "valAt", "__vm_map_val_at", 1, "himp", MContainerResult::Value, false },
    { "clear", "__vm_map_clear", 0, "h", MContainerResult::Process, false },
    { "len", "__vm_map_len", 0, "h", MContainerResult::Process, false },
};

/**
 * @brief 找一个容器方法。
 * @param container 容器类型。
 * @param name 方法名。
 * @return 方法定义；没有返回 nullptr。
 */
const MContainerMethod *FindContainerMethod(const std::shared_ptr<MType> &container,
                                            const Memory::String &name)
{
    if (!IsContainer(container)) return nullptr;
    const MContainerMethod *table = kVecMethods;
    std::size_t count = sizeof(kVecMethods) / sizeof(kVecMethods[0]);
    if (container->name == "set")
    {
        table = kSetMethods;
        count = sizeof(kSetMethods) / sizeof(kSetMethods[0]);
    }
    else if (container->name == "map")
    {
        table = kMapMethods;
        count = sizeof(kMapMethods) / sizeof(kMapMethods[0]);
    }
    for (std::size_t index = 0; index < count; ++index)
    {
        if (name == table[index].name) return &table[index];
    }
    return nullptr;
}

/**
 * @brief 容器方法的结果类型（`ExprType` 与降级共用）。
 * @param container 容器类型或目标表达式。
 * @param method 压缩方法编号或目标方法名称。
 * @retval 对应的类型描述对象。
 */
std::shared_ptr<MType> ContainerMethodType(const std::shared_ptr<MType> &container,
                                           const MContainerMethod &method)
{
    switch (method.result)
    {
    case MContainerResult::Self: return container;
    case MContainerResult::Element: return container->elem;
    case MContainerResult::Value: return container->elem2 ? container->elem2 : container->elem;
    case MContainerResult::Key: return container->elem;
    case MContainerResult::Bool: return T(M_TypeBool());
    default: return T(M_TypeInt());
    }
}

/**
 * @brief 容器的元素 / 键值类型能不能用。
 *
 * 容器的元素在 C++ 侧按 `ObjectValue` 存：标量 / 引用 / 结构体地址是一槽值，
 * 字符串存文本副本，`bint` / 容器 / `obj` / 接口引用存对象副本。
 */
bool CheckContainerElement(const std::shared_ptr<MType> &type, const Memory::String &where)
{
    if (!type) return Fail("容器 " + where + " 的类型写不出来");
    if (type->kind == MType::Void)
    {
        return Fail("容器的 " + where + " 不能是 void");
    }
    if (IsResourceLike(type))
    {
        return Fail("容器的 " + where + " 不能是 " + Describe(type) + "：宿主资源句柄不能复制进容器");
    }
    return true;
}

/**
 * @brief 生成容器元素存储标志，结构体携带展平字段布局。
 *
 * A struct flag carries the flattened field layout to the VM.
 * @param type 类型描述。
 * @param allowResources 是否允许容器存放需要清理的资源。
 * @retval 生成的低层语法节点。
 */
simple_ast::NodePtr ContainerStorageFlag(const std::shared_ptr<MType> &type, bool allowResources = false)
{
    if (type && type->kind == MType::Func) return simple_ast::Node::i(-1);
    if (!type || type->kind != MType::Struct)
        return simple_ast::Node::i(IsObjectLike(type) || (allowResources && IsResourceLike(type)) ? 1 : 0);
    EnsureTupleLayout(type);
    const MStructDef *definition = FindStruct(type->name);
    if (definition == nullptr || !definition->flattened)
        return Fail("容器结构体没有可用布局：" + type->name), Bad();
    Memory::String layout = "@";
    for (const auto &leaf : definition->leaves)
    {
        const std::shared_ptr<MType> &field = leaf.second;
        if (IsResourceLike(field) && !allowResources)
            return Fail("容器结构体 " + type->name + " 的字段 " + leaf.first + " 是不可复制的资源句柄"), Bad();
        if (field->kind == MType::Array)
            return Fail("容器结构体 " + type->name + " 的数组字段 " + leaf.first + " 需要显式转换为值结构体或引用"), Bad();
        layout += field->kind == MType::String ? 's' : field->kind == MType::Func                                     ? 'c' :
                                                   (IsObjectLike(field) || (allowResources && IsResourceLike(field))) ? 'o' :
                                                   field->kind == MType::Double                                       ? 'f' :
                                                                                                                        'i';
    }
    return simple_ast::Node::str(layout);
}

/**
 * @brief 判断节点类型是否需要所有权管理。
 * @param type 类型描述。
 * @retval 节点类型需要所有权管理时为 true，否则为 false。
 */
bool IsOwnedKind(const std::shared_ptr<MType> &type)
{
    if (!type) return false;
    /* 运行时数组、堆上的字符串、以及 `bint` / vec / set / map（对象表里的句柄）：
     * 都归声明它的块所有，出块自动释放（见"生命周期"）。 */
    return IsRuntimeArray(type) || type->kind == MType::String || IsObjectLike(type) || IsResourceLike(type);
}

/**
 * @brief 查找最近的值出口或循环出口，带值退出跳过没有结果槽的循环。
 * @param hasValue 是否查找接收结果值的出口；为 false 时查找最近循环。
 * @param level 零表示默认出口，正数指定循环层数，负一选择最外层循环。
 * @retval 找到时返回上下文指针，否则返回空指针。
 */
MBlockContext *FindBreakContext(bool hasValue = true, long long level = 0)
{
    MBlockContext *outermost = nullptr;
    for (auto it = g_blockStack.rbegin(); it != g_blockStack.rend(); ++it)
    {
        if (level != 0)
        {
            if (it->continueLabel.empty()) continue;
            outermost = &*it;
            if (level > 0 && --level == 0) return outermost;
        }
        else if (!it->breakLabel.empty() && (hasValue ? !it->breakValue.empty() : !it->valueOnly))
            return &*it;
    }
    return level < 0 ? outermost : nullptr;
}

/**
 * @brief 最近的 `continue` 目标（只用循环登记；`match` 那层的 continueLabel 是空的）。
 * @param scopeDepth 输出：那层循环进来时的 `g_ownedScopes` 深度。
 */
Memory::String FindContinueTarget(std::size_t *scopeDepth)
{
    for (auto it = g_blockStack.rbegin(); it != g_blockStack.rend(); ++it)
    {
        if (it->continueLabel.empty()) continue;
        if (scopeDepth != nullptr) *scopeDepth = it->scopeDepth;
        return it->continueLabel;
    }
    return Memory::String();
}

/* "用到才生成"的开关在下面统一声明，这里先用一下（同一个匿名命名空间）。 */
extern bool g_usesStructCopy;

bool CheckMapKey(const std::shared_ptr<MType> &type, const Memory::String &collection);

/**
 * @brief 声明用到的类型名得**真的声明过**。
 *
 * 光靠文法分不出"类型名"和"变量名"，所以拼错的类型名（`Foo x;`）以前会悄悄当成
 * 一个没声明的结构体句柄 —— 现在这里报出来。
 * @param type 类型。
 * @param where 出错位置（变量名 / 形参名，拼进报错文案）。
 * @return 没问题返回 true。
 */
bool CheckDeclaredType(const std::shared_ptr<MType> &type, const Memory::String &where)
{
    if (!type) return true;
    if (type->kind == MType::Ptr && IsInterfaceType(type->elem)) return true;
    if (type->kind == MType::Ptr || type->kind == MType::Array)
        return CheckDeclaredType(type->elem, where);
    if (IsContainer(type))
    {
        if (!CheckDeclaredType(type->elem, where)) return false;
        if (type->name == "map")
            return CheckMapKey(type->elem, "Map") && CheckDeclaredType(type->elem2, where);
        if (type->name == "set" && type->elem->kind == MType::Struct && !IsTupleType(type->elem))
            return CheckMapKey(type->elem, "Set");
        return true;
    }
    /* 接口**不能当值类型**：它只描述"有哪些函数"，没有字段也没有大小，
     * 所以 `Shape s;` 装不出东西来。要指向实现了它的对象就写接口引用。 */
    if (IsInterfaceType(type))
    {
        return Fail("接口不能当值类型（它只描述函数、没有字段也没有大小）：" + type->name + "（" + where + "）；要指向实现了它的对象就写 `" + type->name + " const&` 或 `" + type->name + " mut&`");
    }
    if (type->kind == MType::Struct && g_structs.count(type->name) == 0u)
    {
        return Fail(MAGEL_TEXT("没有声明的类型：") + type->name + "（" + where + "）");
    }
    return true;
}

/**
 * @brief 这个类型是浮点吗（`float32` / `float64`）。
 * @param type 类型描述。
 * @retval 满足检查条件或操作完成时为 true，检查失败或无法完成时为 false。
 */
bool IsFloatType(const std::shared_ptr<MType> &type)
{
    return type && type->kind == MType::Double;
}

/**
 * 求值一个表达式（实现在下面）—— 隐式转换的助手要用。
 */
simple_ast::NodePtr LowerExpr(int id);
simple_ast::TypePtr LowerType(const std::shared_ptr<MType> &type, const Memory::String &label);

/**
 * 是不是 `某个obj.get()`（实现在下面）：接口引用的唯一来源。
 */
bool IsObjGetCall(int id);

/**
 * @brief 判断 Any.getValue 是否返回盒中借用的聚合地址。
 * @param id 表达式节点；仅由聚合所有权判定调用。
 * @return getValue 返回的聚合需要复制；getOptional 构造的新枚举仍可移动。
 */
bool IsBorrowedObjectValueCall(int id)
{
    return IsObjGetCall(id) && N(N(id).kids[0]).s == "getValue";
}

/**
 * @brief 判断目标上下文是否产生新的转换值，避免将源变量的所有权移交给结果。
 * @param valueId 源表达式。
 * @param target 目标类型。
 * @return 非引用目标存在直接隐式转换且类型不同则为 true。
 */
bool IsImplicitConversionValue(int valueId, const std::shared_ptr<MType> &target)
{
    if (valueId < 0 || !target || target->kind == MType::Ptr) return false;
    const auto source = ExprType(valueId);
    return !SameType(source, target) && HasImplicitConversion(source, target);
}

/**
 * @brief 十进制整数字面量的取值（含 `-300` 这种「一元负号 + 字面量」的常量形式）。
 *
 * 只认**十进制**。十六进制 / 二进制字面量（`0xFF`、`b1010`）在 Magel 里当
 * **位模式**看待（`0xFF` 就是"低 8 位全 1"），所以这里不认它们，交给原来那条
 * "整数字面量放行"的老路，免得 `int8 x = 0xFF;` 这种写法被范围检查拦下。
 *
 * @param id 表达式节点。
 * @param value 输出：折出来的十进制值。
 * @return 是十进制整数字面量（或其常量取负）则返回 true。
 */
bool DecimalIntegerLiteral(int id, long long *value)
{
    if (id < 0) return false;
    const MNode &node = N(id);
    if (node.kind == MNode::Int)
    {
        if (node.s.empty()) return false; /* 没有词法原文：判不出进制，放行 */
        const char first = node.s[0];
        if (node.s.size() >= 2u && first == '0' && (node.s[1] == 'x' || node.s[1] == 'X')) return false;
        if (first == 'b' || first == 'B') return false;
        *value = node.i;
        return true;
    }
    if (node.kind == MNode::Un && node.s == "-" && !node.kids.empty())
    {
        long long inner = 0;
        if (!DecimalIntegerLiteral(node.kids[0], &inner)) return false;
        *value = -inner;
        return true;
    }
    return false;
}

/**
 * @brief 目标整型能否装下这个十进制值（`int8` 是 `-128..127`，`uint8` 是 `0..255`）。
 */
bool IntegerInRange(const std::shared_ptr<MType> &type, long long value)
{
    if (!type || type->kind != MType::Int) return false;
    const int bits = type->bits >= 64 ? 64 : (type->bits < 1 ? 1 : type->bits);
    if (type->isUnsigned)
    {
        if (value < 0) return false;
        if (bits >= 64) return true;
        return static_cast<unsigned long long>(value) <= ((1ull << bits) - 1ull);
    }
    if (bits >= 64) return true;
    const long long limit = 1ll << (bits - 1);
    return value >= -limit && value <= limit - 1;
}

/**
 * @brief 整型的十进制取值范围文字（报错文案用）：`-128..127` / `0..255`。
 */
Memory::String IntegerRangeText(const std::shared_ptr<MType> &type)
{
    if (!type || type->kind != MType::Int) return Memory::String();
    const int bits = type->bits >= 64 ? 64 : (type->bits < 1 ? 1 : type->bits);
    if (type->isUnsigned)
        return bits >= 64 ? Memory::String("0..18446744073709551615") : Memory::String("0..") + Memory::toString(static_cast<long long>((1ull << bits) - 1ull));
    if (bits >= 64) return "-9223372036854775808..9223372036854775807";
    const long long limit = 1ll << (bits - 1);
    return Memory::toString(-limit) + ".." + Memory::toString(limit - 1);
}

/**
 * @brief 检查具体实现引用能否绑定为目标接口视图。
 * @param target 目标接口引用类型。
 * @param source 具体实现的引用类型。
 * @retval 接口实现及引用可变性均符合时返回 true。
 */
bool CanBindInterfaceReference(const std::shared_ptr<MType> &target, const std::shared_ptr<MType> &source);

/**
 * @brief 将具体值装入拥有独立清理记录的动态值或接口视图。
 * @param valueId 源表达式节点编号。
 * @param target 目标 Any 或接口引用类型。
 * @retval 持有装箱结果的低层表达式。
 */
simple_ast::NodePtr LowerBoxedValue(int valueId, const std::shared_ptr<MType> &target);

/**
 * @brief 隐式转换：**整数字面量落进浮点槽位**时，直接写成浮点字面量。
 *
 * `float64 d = 2;` / `d = 2;` / `f(2)`（形参是 float64）/ `return 2;`（返回 float64）——
 * 语义上都是 `2.0`，四元式文本里也该写 `2.0`：这样"有没有小数点"和槽位类型一致，
 * 数据区正文、化简日志、反汇编看起来都不会自相矛盾。
 * 只对**字面量**做转换：变量 / 表达式仍走运行期按目标类型归一化那条路。
 *
 * @param valueId 待求值的表达式。
 * @param target 目标类型。
 * @return 表达式节点。
 */
simple_ast::NodePtr LowerValueForType(int valueId, const std::shared_ptr<MType> &target)
{
    /* 接口引用绑定时同时保存具体实现类型号与原引用地址；沿用既有胖引用分派。
     * 已有接口引用及 Any 提取保持原有路径。 */
    if (target && IsInterfacePointer(target) && !IsObjGetCall(valueId))
    {
        const std::shared_ptr<MType> source = (target && target->kind == MType::Ptr ? RawExprType(valueId) : ExprType(valueId));
        if (!IsInterfacePointer(source))
        {
            if (CanBindInterfaceReference(target, source)) return LowerBoxedValue(valueId, target);
            Fail("接口引用 " + Describe(target) + " 需要实现该接口且可变性兼容的具体类型引用，实际是 " + Describe(source));
            return Bad();
        }
    }
    /* `obj.get()` 的类型**只能从上下文来**：这里就是那个"上下文" ——
     * 声明初始化 / 赋值 / 实参 / return 全都经过这个函数，一处设置就全顾上了
     * （`(int)o.get()` 那种写法由 LowerCast 设置）。 */
    InferExpression(valueId, target);
    const bool objGet = IsObjGetCall(valueId);
    const std::shared_ptr<MType> actual = objGet ? target : (target && target->kind == MType::Ptr ? RawExprType(valueId) : ExprType(valueId));
    if (target && actual && !objGet && ((target->kind == MType::Bool && (actual->kind == MType::Int || actual->kind == MType::Double)) || ((target->kind == MType::Int || target->kind == MType::Double) && actual->kind == MType::Bool)))
    {
        Fail("bool 与数值之间不能隐式转换：" + Describe(actual) + " -> " + Describe(target) + "（请显式强转）");
        return Bad();
    }
    if (target && actual && target->kind == MType::Obj && actual->kind != MType::Obj && !objGet && !HasImplicitConversion(actual, target))
        return LowerBoxedValue(valueId, target);
    const MNode &value = N(valueId);
    /* 字面量落进整型槽位：这是「字面量适配」（`uint8 b = 255;`），不是隐式转换。
     * 字符字面量照旧放行；十进制整数字面量（含 `-300` 这种常量取负）要求值落在目标
     * 类型的范围内 —— 装不下就明确报错，绝不像 C 那样静默回绕；十六进制 / 二进制
     * 字面量按「位模式」放行。 */
    bool integerLiteral = false;
    bool literalOutOfRange = false;
    long long literalValue = 0;
    if (target && target->kind == MType::Int)
    {
        if (value.kind == MNode::Char)
            integerLiteral = true;
        else if (value.kind == MNode::Int && value.unsignedLiteral)
        {
            const auto magnitude = static_cast<std::uint64_t>(value.i);
            integerLiteral = target->isUnsigned && (target->bits >= 64 || magnitude <= ((std::uint64_t(1) << target->bits) - 1));
        }
        else if (DecimalIntegerLiteral(valueId, &literalValue))
        {
            if (IntegerInRange(target, literalValue))
                integerLiteral = true;
            else
                literalOutOfRange = true;
        }
        else if (value.kind == MNode::Int)
        {
            integerLiteral = true;
        }
    }
    const bool contextualNew = value.kind == MNode::NewPtr;
    const bool nullHandle = value.kind == MNode::Null && target && (target->kind == MType::Ptr || target->kind == MType::Func || target->kind == MType::String || target->kind == MType::File || target->kind == MType::Proc);
    const bool overloadedFunction = target && target->kind == MType::Func && value.kind == MNode::Var && FindFunctionOverloads(value.module, value.s) != nullptr;
    if (target && actual && !objGet && !contextualNew && !nullHandle && !overloadedFunction && !Acceptable(target, actual, true) && !integerLiteral)
    {
        if (literalOutOfRange)
            Fail("整数字面量 " + Memory::toString(literalValue) + " 超出 " + Describe(target) + " 的取值范围（" + IntegerRangeText(target) + "）");
        else
            Fail("不能隐式转换：" + Describe(actual) + " -> " + Describe(target) + "（请显式强转）");
        return Bad();
    }
    const std::shared_ptr<MType> saved = g_expectedObjGet;
    if (target) g_expectedObjGet = target;
    if (IsFloatType(target))
    {
        if (value.kind == MNode::Int)
        {
            g_expectedObjGet = saved;
            return simple_ast::Node::f(actual && actual->isUnsigned ? static_cast<double>(static_cast<std::uint64_t>(value.i)) : static_cast<double>(value.i));
        }
    }
    if (target && actual && !SameType(target, actual) && HasImplicitConversion(actual, target))
    {
        const simple_ast::NodePtr converted = LowerDeclaredConversion(valueId, target, false);
        g_expectedObjGet = saved;
        return converted;
    }
    // 函数值在形参、返回值、容器和字段中也必须按当前目标签名选择并校验。
    const auto savedFunction = g_expectedFuncType;
    if (target && target->kind == MType::Func) g_expectedFuncType = target;
    const simple_ast::NodePtr node = target && target->kind == MType::Ptr ? LowerRawExpr(valueId) : LowerExpr(valueId);
    g_expectedFuncType = savedFunction;
    g_expectedObjGet = saved;
    if (target && target->kind == MType::Double && actual && actual->kind == MType::Int)
        return simple_ast::Node::cast(
            node, simple_ast::Type::make(simple_ast::Type::Kind::Float, target->bits));
    if (target && (target->kind == MType::Int || target->kind == MType::Double) && actual && !SameType(target, actual))
        return simple_ast::Node::cast(node, LowerType(target, "implicit cast"));
    return node;
}

/**
 * @brief 两个类型是不是"同一个结构体"（名字一样）。
 * @param left 左操作数。
 * @param right 右操作数。
 * @retval 满足检查条件或操作完成时为 true，检查失败或无法完成时为 false。
 */
bool SameStruct(const std::shared_ptr<MType> &left, const std::shared_ptr<MType> &right)
{
    return left && right && left->kind == MType::Struct && right->kind == MType::Struct && left->name == right->name;
}

/**
 * @brief 字符串字面量当"**变量的值**"时，先复制成一份堆串。
 *
 * 字符串的值一律是**堆地址**（8 字节长度 + UTF-8 字节数据，由虚拟机运行时管理）：
 * 数据区里只留只读的字面量字节当复制源（`__vm_str_lit` 把它们拷到堆上）。于是：
 *
 * - 变量手里那块是**自己能写**的（`s[i] = 'x'` 不会改到别人共享的字面量）；
 * - 所有权也统一了：**变量 = 那块堆串的主人**，离开语句块时自动释放（见 MarkOwned）。
 *
 * 只读用法**不复制**（比较、拼接的操作数、`std::lang::print("{}", "abc")` 这种实参）：那些地方
 * 不写这块内存，也没人该为它负责 —— 复制了反倒没人释放。
 *
 * @param valueId 右边的表达式。
 * @param target 目标类型（不是 string 就原样返回）。
 * @return 表达式节点。
 */
bool IsFreshValue(int exprId); /* 定义在本文件后面，这里先用 */
/**
 * @brief 将最终值的内部所有权交给接收方并清空源槽。
 */
simple_ast::NodePtr TakeExpressionTempValue(const simple_ast::NodePtr &value,
                                            const std::shared_ptr<MType> &type);

/**
 * @brief 复制运行时数组的元素及长度头，返回独立拥有的数组句柄。
 * @param source 只求值一次的源数组句柄表达式。
 * @param type 保留元素布局的运行时数组类型。
 * @return 逐元素深拷贝后由调用方接管的数组表达式。
 */
simple_ast::NodePtr CopyRuntimeArrayValue(const simple_ast::NodePtr &source, const std::shared_ptr<MType> &type);
extern bool g_usesNewArray;

/**
 * @brief 将字符串表达式保存为具有独立生存期的值。
 * @param valueId 表示输入值的节点或类型编号。
 * @param target 待访问或修改的目标。
 * @retval 生成的低层语法节点。
 */
simple_ast::NodePtr MaterializeStringValue(int valueId, const std::shared_ptr<MType> &target)
{
    const simple_ast::NodePtr value = LowerValueForType(valueId, target);
    if (IsRuntimeArray(target) && valueId >= 0 && !IsFreshValue(valueId)) return CopyRuntimeArrayValue(value, target);
    if (!target || target->kind != MType::String) return value;
    if (valueId < 0) return value;
    if (N(valueId).kind == MNode::Str)
        return simple_ast::Node::call("__vm_str_lit", { "__magel_addr" }, { value });
    if (IsFreshValue(valueId) || IsImplicitConversionValue(valueId, target)) return TakeExpressionTempValue(value, target);
    /*
     * 借来的值（变量 / 别名 / 字段 / 下标 / 解引用）**复制成自己的堆串**：
     * 接收方（变量 / 字段 / 返回值）随后会登记成所有者，直接交句柄的话两边指着同一块，
     * 先释放的一边把另一边弄成悬垂别名（`string a = b;` 之后 b 一改，a 就废）。
     */
    return simple_ast::Node::call("__vm_str_sub", { "__magel_s", "__magel_a", "__magel_b" },
                                  { value, simple_ast::Node::i(0), simple_ast::Node::i(-1) });
}

/**
 * @brief 返回语句里的字符串：**不是本函数那个所有者变量**就复制一份。
 *
 * 调用方按"调用即新值"接管返回值（接收变量会登记成所有者）。只有两种情况能直接交句柄：
 *
 *   - 当场造出来的（字面量 / 切片 / 拼接 / 内层调用）本来就是新的；
 *   - 返回的正是本函数里那个所有者变量（`keep` = `ReturnedOwnedNames`），所有权转移。
 *
 * 其余都是借来的——形参、别名变量、字段、下标：直接交句柄的话，两边指着同一块堆串，
 * 接收方释放时把源位置弄悬垂。
 */
simple_ast::NodePtr MaterializeReturnStringValue(int valueId, const std::shared_ptr<MType> &target,
                                                 const Memory::Set<Memory::String> &keep)
{
    const simple_ast::NodePtr value = LowerValueForType(valueId, target);
    if (IsImplicitConversionValue(valueId, target)) return TakeExpressionTempValue(value, target);
    if (IsRuntimeArray(target))
    {
        if (IsFreshValue(valueId)) return TakeExpressionTempValue(value, target);
        const auto &source = N(valueId);
        const auto *variable = source.kind == MNode::Var ? FindIn(source.module, source.s) : nullptr;
        if (variable == nullptr || keep.count(variable->emitted) == 0u) return CopyRuntimeArrayValue(value, target);
        return value;
    }
    if (target && IsObjectLike(target) && valueId >= 0 && !IsFreshValue(valueId))
    {
        int sourceId = valueId;
        while (N(sourceId).kind == MNode::Cast) sourceId = N(sourceId).kids[0];
        const MNode &source = N(sourceId);
        const MVar *variable = source.kind == MNode::Var ? FindIn(source.module, source.s) : nullptr;
        if (variable == nullptr || keep.count(variable->emitted) == 0u)
            return simple_ast::Node::call(ObjectCopyFunction(target), { "__magel_v" }, { value });
    }
    if (target && IsObjectLike(target) && IsFreshValue(valueId))
        return TakeExpressionTempValue(value, target);
    if (!target || target->kind != MType::String) return value;
    if (valueId < 0) return value;
    int sourceId = valueId;
    while (N(sourceId).kind == MNode::Cast) sourceId = N(sourceId).kids[0];
    const MNode &expr = N(sourceId);
    if (expr.kind == MNode::Str)
        return simple_ast::Node::call("__vm_str_lit", { "__magel_addr" }, { value });
    if (expr.kind == MNode::Var)
    {
        const MVar *found = FindIn(expr.module, expr.s);
        if (found != nullptr && keep.count(found->emitted) != 0u) return value; /* 所有权转移 */
    }
    if (IsFreshValue(valueId) || IsImplicitConversionValue(valueId, target)) return TakeExpressionTempValue(value, target);
    return simple_ast::Node::call("__vm_str_sub", { "__magel_s", "__magel_a", "__magel_b" },
                                  { value, simple_ast::Node::i(0), simple_ast::Node::i(-1) });
}

/**
 * @brief 按布局原样复制聚合存储槽，供已确定所有权的内部搬运使用。
 *
 * 此函数不复制槽中拥有资源；普通值赋值由 DeepCopyStructValue 等入口
 * 递归复制资源，所有权移交由调用方清理源拥有叶子。嵌套值字段按布局展开，
 * 引用字段保持借用身份，不能把原样复制句柄误称为普通深拷贝。
 *
 * @param dst 目标聚合存储地址。
 * @param src 源聚合存储地址。
 * @param type 决定总槽数的聚合类型。
 * @return 槽复制语句节点。
 */
simple_ast::NodePtr CopyStructValue(const simple_ast::NodePtr &dst, const simple_ast::NodePtr &src,
                                    const std::shared_ptr<MType> &type)
{
    g_usesStructCopy = true;
    const int slots = StructStride(type) / 8;
    return simple_ast::Node::block({ simple_ast::Node::exprStmt(simple_ast::Node::call(
                                         "__magel_copy_struct", { "__magel_dst", "__magel_src", "__magel_slots" },
                                         { dst, src, simple_ast::Node::i(slots) })),
                                     BeginAggregateLifetime(dst, slots * 8) });
}

/**
 * @brief 这个结构体（含枚举存储体）的槽里有没有要按值占有的字符串 / 对象。
 * @param type 类型描述。
 * @retval 满足检查条件或操作完成时为 true，检查失败或无法完成时为 false。
 */
bool AggregateNeedsDeepCopy(const std::shared_ptr<MType> &type)
{
    return type && type->kind == MType::Struct && StructHasOwnedLeaves(type->name);
}

/**
 * @brief 结构体**深拷贝**：字符串复制成新的堆串、对象复制成新的对象表句柄，标量按位搬。
 *
 * 没有要占有的叶子时退回 `__magel_copy_struct`。要真正"搬走一份"（`return` 交出去、
 * 函数返回值的接收）时用 `CopyStructValue`（按位搬，等于 move），别用这条。
 */
simple_ast::NodePtr DeepCopyStructValue(const simple_ast::NodePtr &dst,
                                        const simple_ast::NodePtr &src,
                                        const std::shared_ptr<MType> &type)
{
    if (!AggregateNeedsDeepCopy(type)) return CopyStructValue(dst, src, type);
    g_needsAggregateCopy.insert(type->name);
    return simple_ast::Node::block({ simple_ast::Node::exprStmt(simple_ast::Node::call(
                                         AggregateCopyName(type->name), { "__magel_agg_dst", "__magel_agg_src" }, { dst, src })),
                                     BeginAggregateLifetime(dst, StructStride(type)) });
}

/**
 * @brief 将数组复制到新分配的带长度存储，逐项复制拥有资源。
 * @param source 源数组表达式，整个复制过程仅求值一次。
 * @param type 包含元素布局的运行时数组类型。
 * @return 保存复制循环和独立结果句柄的序列表达式。
 * @note 沿用内存读取检查、数组分配溢出检查与聚合深复制，不复制宿主机器地址。
 */
simple_ast::NodePtr CopyRuntimeArrayValue(const simple_ast::NodePtr &source, const std::shared_ptr<MType> &type)
{
    using simple_ast::Node;
    const auto input = AllocateName("__magel_array_copy_source");
    const auto length = AllocateName("__magel_array_copy_length");
    const auto result = AllocateName("__magel_array_copy_result");
    const auto index = AllocateName("__magel_array_copy_index");
    const auto loop = NewBlockLabel("array_copy");
    const auto done = NewBlockLabel("array_copy_done");
    const auto element = type->elem;
    const int stride = ElementBytes(element);
    const auto from = Node::offset(Node::var(input), Node::var(index), stride);
    const auto into = Node::offset(Node::var(result), Node::var(index), stride);
    g_usesNewArray = true;
    Memory::Vector<simple_ast::NodePtr> out{
        Node::declare(input, simple_ast::Type::i64()), Node::assign(input, source),
        Node::declare(length, simple_ast::Type::i64()),
        Node::assign(length, Node::loadInt(Node::binary("-", Node::var(input), Node::i(8)))),
        Node::declare(result, LowerType(type, result)),
        Node::assign(result, Node::call("__magel_new_array", { "__magel_len", "__magel_stride" }, { Node::var(length), Node::i(stride) })),
        Node::declare(index, simple_ast::Type::i64()), Node::assign(index, Node::i(0)),
        Node::label(loop), Node::ifThenElse(Node::binary(">=", Node::var(index), Node::var(length)), Node::jump(done), nullptr)
    };
    if (element->kind == MType::Struct)
        out.push_back(DeepCopyStructValue(into, from, element));
    else
    {
        auto value = element->kind == MType::Char ? Node::call("__vm_char_load", { "address" }, { from }) : Node::loadFrom(from, LowerType(element, input));
        if (element->kind == MType::String)
            value = Node::call("__vm_str_sub", { "source", "start", "end" }, { value, Node::i(0), Node::i(-1) });
        else if (IsObjectLike(element))
            value = Node::call(ObjectCopyFunction(element), { "value" }, { value });
        out.push_back(element->kind == MType::Char ? Node::exprStmt(Node::call("__vm_char_store", { "address", "value" }, { into, value })) : Node::storeAt(into, value, LowerType(element, result)));
    }
    out.push_back(Node::assign(index, Node::binary("+", Node::var(index), Node::i(1))));
    out.push_back(Node::jump(loop));
    out.push_back(Node::label(done));
    return Node::sequence(std::move(out), Node::var(result));
}

/**
 * @brief 这个（结构体类型的）表达式是不是"当场造出来的聚合值"（函数 / 工厂的返回值）。
 *
 * 当场造出来的值所有权归调用点，接收方按 **move** 接住（按位搬）；借来的（变量 /
 * 字段 / 下标 / `?` 解出的载荷）则要 **深拷贝**，否则两边共有一个槽里的串 / 对象。
 */
bool IsFreshAggregate(int exprId)
{
    if (exprId < 0) return false;
    if (IsBorrowedObjectValueCall(exprId)) return false;
    const MNode &node = N(exprId);
    // 引用返回值仍借用原聚合；按值返回它时必须复制，不能接管源字段。
    if (RawExprType(exprId)->kind == MType::Ptr || (node.kind == MNode::Un && node.s == "*")) return false;
    if (node.kind == MNode::Try) return OwnedPropagation(exprId);
    if (node.kind == MNode::Call || node.kind == MNode::Tuple || node.kind == MNode::Loop || node.kind == MNode::Range) return true;
    /* 运算符重载（`a + b` / `-a`）就是方法调用：返回值是调用点当场造出来的新值。 */
    if (node.kind == MNode::Bin || node.kind == MNode::Un) return true;
    if (node.kind == MNode::EnumCtor) return node.type2 == 1; /* 静态工厂 */
    if (node.kind == MNode::Cast)
        return !node.kids.empty() && (HasDeclaredConversion(ExprType(node.kids[0]), T(node.type), true) || IsFreshAggregate(node.kids[0]));
    return false;
}

bool IsOwnedAggregateTemporary(int exprId); /* 定义在 lowering.cpp，这里先用 */
Memory::String AllocateName(const Memory::String &source);

/**
 * @brief 将拥有的聚合复制到独立存储后清空源返回槽，保证连续调用互不释放资源。
 * @param dst 目标地址。
 * @param src 拥有的源地址表达式，仅求值一次。
 * @param type 聚合类型。
 * @return 执行移动和清槽的语句块。
 */
simple_ast::NodePtr MoveStructValue(const simple_ast::NodePtr &dst, const simple_ast::NodePtr &src,
                                    const std::shared_ptr<MType> &type)
{
    using simple_ast::Node;
    const auto source = AllocateName("__magel_move_source");
    Memory::Vector<simple_ast::NodePtr> out{ Node::declare(source, simple_ast::Type::i64()),
                                             Node::assign(source, src), CopyStructValue(dst, Node::var(source), type) };
    const auto *definition = FindStruct(type->name);
    Memory::Vector<simple_ast::NodePtr> clear;
    if (definition)
        for (std::size_t i = 0; i < definition->leaves.size(); ++i)
        {
            const auto leaf = definition->leaves[i].second;
            if (!LeafOwnedByAggregate(leaf)) continue;
            clear.push_back(Node::storeAt(Node::offset(Node::var(source), Node::i(i), 8),
                                          IsFloatType(leaf) ? Node::f(0.0) : Node::i(0), LowerType(leaf, source)));
        }
    // 临时值搬运也结束源生命周期，不能在清槽后再执行自定义析构。
    AppendMovedAggregate(type->name, Node::var(source), &clear);
    /* 递归调用可复用同一静态聚合槽，自移动必须保留叶子。 */
    if (!clear.empty()) out.push_back(Node::ifThenElse(
        Node::binary("!=", dst, Node::var(source)), Node::block(std::move(clear)), nullptr));
    return Node::block(std::move(out));
}

/**
 * @brief 结构体按**值语义**存进一个位置：当场造的按 move，借来的**深拷贝**。
 *
 * 结构体和字符串一样是值类型（不可能是引用）：`Point a = b;` / `a = b;` / `arr[i] = b;` /
 * 绑定载荷，都得让接收方拿到**自己那份**字符串与对象。只有当场构造的（函数 / 工厂返回值、
 * 元组字面量、`new`）才能直接按位搬 —— 那份所有权本来就归调用点。
 */
simple_ast::NodePtr StoreStructValue(const simple_ast::NodePtr &dst, int srcExprId,
                                     const simple_ast::NodePtr &src,
                                     const std::shared_ptr<MType> &type)
{
    if (IsOwnedAggregateTemporary(srcExprId) || IsImplicitConversionValue(srcExprId, type)) return MoveStructValue(dst, src, type);
    return DeepCopyStructValue(dst, src, type);
}

/**
 * 表达式的静态类型（实现在下面）。
 */
std::shared_ptr<MType> ExprType(int id);
/**
 * @brief 查询用户运算符的实际返回类型，供所有权判断复用。
 * @param exprId 运算表达式节点。
 * @return 匹配的返回类型，内建运算返回空。
 */
std::shared_ptr<MType> UserOperatorResultType(int exprId);
struct MIndexDecl;
const MIndexDecl *FindIndex(const std::shared_ptr<MType> &owner,
                            const std::shared_ptr<MType> &key);

/**
 * @brief 这个表达式是不是"**当场造出来的新对象**"（堆上的新串 / 新的运行时数组）。
 *
 * 用来判断"声明即所有"：`string s = 拼接 / 切片 / format / 字面量;` 里 `s` 就是那块
 * 堆串的主人（离开块时自动释放）；借来的变量 / 字段等由 MaterializeStringValue
 * 复制成自己的值，同样登记为所有者。该判据只区分新值接管与借用值复制。
 *
 * `Call` 也算"新的"：函数返回时被返回的那个变量不参与释放（见 ReturnedVariableName），
 * 所以拿返回值初始化的变量正好接住这份所有权。认不出来的写法（成员 / 下标 / 强转）
 * 一律当"借来的"，宁可不释放也不误释放。
 */
bool IsFreshValue(int exprId)
{
    if (exprId < 0) return false;
    const MNode &expr = N(exprId);
    switch (expr.kind)
    {
    case MNode::Range: return true;
    case MNode::Var:
        return FindIn(expr.module, expr.s) == nullptr && FindFunctionOverloads(expr.module, expr.s) != nullptr;
    case MNode::Str:               /* 字面量：会先复制成堆串（MaterializeStringValue） */
    case MNode::Null:              /* 空引用：释放它是空操作 */
    case MNode::Slice:             /* 切片：新串 */
    case MNode::CollectionLiteral: /* Set / Map 字面量产生新容器。 */
    case MNode::NewPtr:            /* `new` / `new [n]`：新对象 */
    case MNode::Loop:              /* break 在退出作用域前保存独立值，调用点接管。 */
    case MNode::Call:              /* 函数返回：所有权归调用方 */
    {
        /*
         * `std::lang::BigInteger(x)` / `std::lang::BigDecimal(x)`（限定名写法）虽然 AST 上是"调用"，语义却与
         * `bint(x)` 一致：**从已有大数转发时不复制**。不特判的话会按"调用一律是当场造出来的
         * 新对象"处理，变量之间共享句柄，一旦其中一个被重新赋值就把旧句柄还了，另一个悬空
         * （`example/samples/object_copy.mgl` 踩过：打印时说"大整数已经释放过了"）。
         */
        if (expr.kids.size() < 2u) return true; /* 有的调用节点只带被调者，别越界 */
        const MNode &calleeNode = N(expr.kids[0]);
        if (calleeNode.kind == MNode::EnumCtor && calleeNode.s == "std::lang" && (calleeNode.s2 == "BigInteger" || calleeNode.s2 == "BigDecimal"))
        {
            const Memory::Vector<int> &callArgs = N(expr.kids[1]).kids;
            if (callArgs.size() == 1u)
            {
                /*
                 * 实参是"已有值"（变量 / 字段 / 下标）且**类型相同**才判为**转发旧句柄**：
                 * `std::lang::BigInteger(other)`（other 已经是 BigInteger）不复制；而
                 * `std::lang::BigInteger(k)`（k 是 int64）是**新建**，必须按新对象处理，
                 * 否则那个新对象没有所有者，循环里每次都漏一个。
                 *
                 * 变量走 `Find` 查**声明类型**，不碰 `ExprType` —— 类型推导会反过来问
                 * `IsFreshValue`，互相递归直接爆栈（踩过：整个编译器段错误）。字段 / 下标
                 * 仍按形态当转发（保守，宁可不复制也不误复制）。
                 */
                const MNode &argument = N(callArgs[0]);
                const MType::Kind wanted =
                    (calleeNode.s2 == "BigInteger") ? MType::BigInt : MType::BigFloat;
                if (argument.kind == MNode::Var)
                {
                    const MVar *found = FindIn(argument.module, argument.s);
                    if (found != nullptr && found->type)
                        return found->type->kind != wanted;
                }
                if (argument.kind == MNode::Member || argument.kind == MNode::Arrow || argument.kind == MNode::Index)
                    return false;
            }
        }
        /* 引用返回值只借用源对象；自动解引用不能把它变成调用点所有者。 */
        // 接口胖引用本身是拥有型对象表值；其副本拥有句柄，但仍借用被引用对象。
        return expr.kind != MNode::Call || RawExprType(exprId)->kind != MType::Ptr || IsInterfacePointer(RawExprType(exprId));
    }
    case MNode::Cast:
        return !expr.kids.empty() && T(expr.type)->kind != MType::Ptr && HasDeclaredConversion(ExprType(expr.kids[0]), T(expr.type), true);
    case MNode::EnumCtor:
        /* 静态方法与枚举构造共用节点，引用返回值不转移源对象所有权。 */
        return expr.type2 == 1 && (RawExprType(exprId)->kind != MType::Ptr || IsInterfacePointer(RawExprType(exprId))) && !IsEnumType(ExprType(exprId));
    case MNode::BigIntCtor:
        /* bint(整数/文本/浮点) 会建新对象；bint(已有 bint) 只是转发旧句柄。 */
        return !expr.kids.empty() && ExprType(expr.kids[0])->kind != MType::BigInt;
    case MNode::BigFloatCtor:
        return !expr.kids.empty() && ExprType(expr.kids[0])->kind != MType::BigFloat;
    case MNode::Index:
    {
        /* 容器的 `v[i]` / `m[k]`：字符串和运行期对象元素取出来都是**新造的副本**，
         * 所以也算"当场造出来的"。对象若不标成 fresh，声明初始化会再复制一次，
         * 第一个副本没有所有者并在这里泄漏。 */
        if (expr.kids.empty()) return false;
        const std::shared_ptr<MType> base = ExprType(expr.kids[0]);
        // 用户 Index<R,T> 与普通函数一样：值返回归调用方，普通引用返回继续借用。
        if (expr.kids.size() > 1u && FindIndex(base, ExprType(expr.kids[1])) != nullptr)
            return RawExprType(exprId)->kind != MType::Ptr || IsInterfacePointer(RawExprType(exprId));
        if (!IsContainer(base)) return false;
        const std::shared_ptr<MType> result = ExprType(exprId);
        return result && (result->kind == MType::String || IsObjectLike(result));
    }
    case MNode::Try:
    {
        /* `?` 取出的字符串 / 对象载荷是**复制给调用方的一份**（见 LowerTry），
         * 所有权跟着表达式走，所以算"当场造出来的"。结构体载荷仍是源聚合里的别名。 */
        const std::shared_ptr<MType> result = ExprType(exprId);
        if (RawExprType(exprId)->kind == MType::Ptr && !IsInterfacePointer(RawExprType(exprId))) return false;
        return result && (result->kind == MType::String || IsObjectLike(result) || (result->kind == MType::Struct && OwnedPropagation(exprId)));
    }
    case MNode::Un:
    {
        if (const auto result = UserOperatorResultType(exprId))
            return result->kind != MType::Ptr || IsInterfacePointer(result);
        /* 大整数 / 大浮点的一元取负（`-a`）是**新对象**（`__vm_bint_neg`），
         * 和二元算术一样要有主；不当"当场造出来的"就会漏在对象表里。 */
        if (expr.s != "-") return false;
        const std::shared_ptr<MType> type = ExprType(exprId);
        return type && (type->kind == MType::BigInt || type->kind == MType::BigFloat);
    }
    case MNode::Bin:
    {
        if (const auto result = UserOperatorResultType(exprId))
            return result->kind != MType::Ptr || IsInterfacePointer(result);
        /* 重载运算符按值返回的聚合归调用点；引用返回仍借用源对象。
         * 字符串的 `+` 是新串；大数算术每次产生新对象。 */
        const std::shared_ptr<MType> type = ExprType(exprId);
        if (!type) return false;
        if (type->kind == MType::Struct) return IsFreshAggregate(exprId);
        if (type->kind == MType::String) return expr.s == "+";
        if (type->kind == MType::BigInt)
        {
            return expr.s == "+" || expr.s == "-" || expr.s == "*" || expr.s == "/" || expr.s == "%";
        }
        if (type->kind == MType::BigFloat)
        {
            return expr.s == "+" || expr.s == "-" || expr.s == "*" || expr.s == "/";
        }
        return false;
    }
    default:
        return false;
    }
}

/**
 * @brief 对象类类型（`bint` / 容器）的**值语义**：不是"当场造出来的"就复制一份。
 *
 * 对象类类型的值是对象表里的句柄。如果赋值 / 初始化只抄句柄，`a = b;` 之后两个变量
 * 就会指向同一个对象 —— 谁被重新赋值（旧值要还回去）另一个就悬空了。所以这里按
 * **值语义**办：右边不是"当场造出来的新对象"（运算结果 / 方法返回值 / `bint(…)`）时，
 * 套一层 `__vm_object_copy` 复制一份。
 *
 * @param valueId 右边的表达式。
 * @param target 目标类型。
 * @return 表达式节点。
 */
simple_ast::NodePtr MaterializeObjectValue(int valueId, const std::shared_ptr<MType> &target)
{
    if (!IsObjectLike(target)) return LowerValueForType(valueId, target);
    if (IsFreshValue(valueId) || IsImplicitConversionValue(valueId, target))
        return TakeExpressionTempValue(LowerValueForType(valueId, target), target);
    return simple_ast::Node::call(ObjectCopyFunction(target), { "__magel_v" },
                                  { LowerValueForType(valueId, target) });
}
