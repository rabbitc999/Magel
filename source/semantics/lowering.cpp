/* ---------- 语句 / 表达式的降级 ---------- */

/**
 * @brief 当前完整表达式中创建的内部所有者，独立于用户变量的块生命周期。
 */
struct MFullExpressionTemps;
MFullExpressionTemps *g_fullExpressionTemps = nullptr;

/**
 * @brief 收集表达式中间变量，在结果保存后逆序析构并清空槽位。
 * 嵌套语句、条件各自收集；return / break 仍可从块登记中找到尚未清理的中间值。
 */
struct MFullExpressionTemps
{
    MFullExpressionTemps *previous = g_fullExpressionTemps;
    Memory::Vector<Memory::String> names;
    bool extendToBlock = false;

    /**
     * @brief 开始一个完整表达式的临时量收集。
     */
    MFullExpressionTemps() { g_fullExpressionTemps = this; }
    /**
     * @brief 恢复外层表达式的收集上下文。
     */
    ~MFullExpressionTemps() { g_fullExpressionTemps = previous; }

    /**
     * @brief 摘除本表达式的块级登记并生成清理。
     * @return 逆序析构与清槽语句。
     */
    Memory::Vector<simple_ast::NodePtr> finish()
    {
        Memory::Vector<simple_ast::NodePtr> out;
        if (extendToBlock) return out;
        for (auto name = names.rbegin(); name != names.rend(); ++name)
        {
            for (auto scope = g_ownedScopes.rbegin(); scope != g_ownedScopes.rend(); ++scope)
            {
                for (auto it = scope->destructors.begin(); it != scope->destructors.end();)
                {
                    if (it->name != *name)
                    {
                        ++it;
                        continue;
                    }
                    out.push_back(MakeDestructorCall(*it));
                    it = scope->destructors.erase(it);
                }
                for (auto it = scope->names.begin(); it != scope->names.end();)
                {
                    if (it->name != *name)
                    {
                        ++it;
                        continue;
                    }
                    AppendOwnedDropByName(it->name, it->runtimeArray, &out);
                    it = scope->names.erase(it);
                }
            }
            g_cleanupItems.erase(std::remove_if(g_cleanupItems.begin(), g_cleanupItems.end(),
                                                [&](const MCleanupItem &item) { return item.name == *name; }),
                                 g_cleanupItems.end());
        }
        return out;
    }
};

/**
 * @brief 登记内部所有者为表达式临时量；普通变量和借用来源不调用此入口。
 * @param name 已经登记块级所有权的内部变量名。
 */
void NoteExpressionTemp(const Memory::String &name)
{
    if (!g_fullExpressionTemps) return;
    g_fullExpressionTemps->names.push_back(name);
    for (auto &scope : g_ownedScopes)
        for (auto &item : scope.destructors)
            if (item.name == name) item.temporary = true;
}

/**
 * @brief 将内部临时句柄移交给最终值或下一所有者，并清空旧槽避免重复回收。
 * @param value 已降级的值表达式。
 * @param type 值类型。
 * @return 只求值一次并转移内部所有权的表达式；借用值保持原样。
 */
simple_ast::NodePtr TakeExpressionTempValue(const simple_ast::NodePtr &value,
                                            const std::shared_ptr<MType> &type)
{
    if (value && value->kind == simple_ast::Node::Kind::Sequence && !value->kids.empty())
    {
        auto transferred = std::make_shared<simple_ast::Node>(*value);
        transferred->kids.back() = TakeExpressionTempValue(value->kids.back(), type);
        return transferred;
    }
    if (!g_fullExpressionTemps || !value || value->kind != simple_ast::Node::Kind::Var || !type || (!IsStandardOpcode(type, "String") && !IsObjectLike(type))) return value;
    const auto &names = g_fullExpressionTemps->names;
    if (std::find(names.begin(), names.end(), value->text) == names.end()) return value;
    const auto transferred = AllocateName("__chali_transferred_value");
    return simple_ast::Node::sequence({ simple_ast::Node::declare(transferred, LowerType(type, transferred)),
                                        simple_ast::Node::assign(transferred, value),
                                        simple_ast::Node::assign(value->text, simple_ast::Node::i(0)) },
                                      simple_ast::Node::var(transferred));
}

/**
 * @brief 为完整表达式提供前置语句入口，退出时恢复外层入口。
 */
struct MExpressionFreshScope
{
    Memory::Vector<simple_ast::NodePtr> *saved;
    /**
     * @brief 设置前置语句入口。
     * @param sink 本表达式的前置语句列表。
     */
    explicit MExpressionFreshScope(Memory::Vector<simple_ast::NodePtr> *sink) : saved(g_freshSink) { g_freshSink = sink; }
    /**
     * @brief 恢复外层前置语句入口，包含错误提前返回路径。
     */
    ~MExpressionFreshScope() { g_freshSink = saved; }
};

/**
 * @brief 保存拥有的聚合中间值；借用来源保持原地址。
 */
simple_ast::NodePtr OwnedAggregateTempFor(int exprId, const std::shared_ptr<MType> &type);

/**
 * @brief 交换律换边时把**左操作数**先钳到一个临时量里，保住源码的求值顺序。
 * @param value 左操作数已降级的值节点。
 * @param type 左操作数类型。
 * @param prologue 输出：求值前缀，调用方要把它挂在**接收者那个实参**前面。
 * @param held 输出：读临时量的节点。
 * @return 能安全落地返回 true；对象类类型或没有语句槽时返回 false。
 */
bool HoldOperandBeforeReceiver(const simple_ast::NodePtr &value,
                               const std::shared_ptr<MType> &type,
                               Memory::Vector<simple_ast::NodePtr> *prologue,
                               simple_ast::NodePtr *held);

/**
 * @brief 转换容器键或元素，并登记聚合临时值的调用后清理。
 * @param argId 实参表达式。
 * @param element 目标元素类型。
 * @param post 接收调用后的清理语句。
 * @return 可传给运行时容器操作的值。
 */
simple_ast::NodePtr ContainerStoreValue(int argId,
                                        const std::shared_ptr<MType> &element,
                                        Memory::Vector<simple_ast::NodePtr> *post);

/**
 * @brief 构造空容器，为自定义 Map 键或 Set 元素绑定 Equal 与可选 Hash。
 * @param type 容器类型。
 * @return 创建容器句柄的表达式。
 */
simple_ast::NodePtr NewContainerValue(const std::shared_ptr<MType> &type)
{
    using simple_ast::Node;
    if (!CheckDeclaredType(type, "容器构造")) return Bad();
    const bool customSet = type->name == "set" && type->elem->kind == MType::Struct && !IsTupleType(type->elem);
    if (type->name != "map" && !customSet)
    {
        const auto element = type->elem;
        const bool intrinsic = element->kind == MType::Int || element->kind == MType::Double || element->kind == MType::Bool || element->kind == MType::Char || IsStandardOpcode(element, "String");
        if (type->name == "set" && !intrinsic)
            return Node::call("__std_set_new_policy", { "equal", "hash" }, { Node::i(0), Node::i(0) });
        return Node::call(type->name == "set" ? "__std_set_new" : "__std_vec_new", {}, {});
    }
    const auto key = type->elem;
    if (key->kind == MType::Obj)
        return Node::call("__std_map_new_policy", { "equal", "hash" }, { Node::i(0), Node::i(0) });
    if (key->kind == MType::Int || key->kind == MType::Double || key->kind == MType::Bool || key->kind == MType::Char || IsStandardOpcode(key, "String"))
        return Node::call("__std_map_new", {}, {});
    const auto name = TypeKey(key);
    auto found = g_mapKeyPolicies.find(name);
    if (found == g_mapKeyPolicies.end())
    {
        MMapKeyPolicy policy;
        for (const auto &op : g_operators)
            if (op.op == "==" && SameType(op.left, key) && SameType(op.right, key) && op.result->kind == MType::Bool)
            {
                policy.equal = op;
                break;
            }
        policy.equalLabel = AllocateName("__chali_map_equal");
        for (const auto &hash : g_hashes)
            if (SameType(hash.owner, key))
            {
                policy.hash = hash;
                policy.hashLabel = AllocateName("__chali_map_hash");
                break;
            }
        found = g_mapKeyPolicies.emplace(name, std::move(policy)).first;
    }
    return Node::call(customSet ? "__std_set_new_policy" : "__std_map_new_policy", { "equal", "hash" },
                      { FunctionPointerValue(found->second.equalLabel), found->second.hashLabel.empty() ? Node::i(0) :
                                                                                                          FunctionPointerValue(found->second.hashLabel) });
}

/**
 * @brief 将语句列表逐项降级并合并结果。
 * @param listId 语句列表在节点池中的编号。
 * @retval 生成的低层语法节点。
 */
Memory::Vector<simple_ast::NodePtr> LowerStmtList(int listId)
{
    Memory::Vector<simple_ast::NodePtr> out;
    if (listId < 0) return out;
    std::size_t expansionDepth = 0;
    for (std::size_t index = 0; index < N(listId).kids.size();)
    {
        const int stmtId = N(listId).kids[index];
        const auto statement = N(stmtId);
        if (statement.kind == MNode::ExprStmt && statement.kids.size() == 1 && N(statement.kids[0]).kind == MNode::MacroCall)
        {
            if (++expansionDepth >= 256) return Fail("宏语句展开超过深度预算"), out;
            int root = -1;
            if (!ExpandDeferredAstMacro(statement.kids[0], nullptr, &root)) return out;
            const auto body = N(root);
            auto items = N(body.kids[0]).kids;
            if (body.kids[1] >= 0) items.push_back(M_ExprStmt(body.kids[1]));
            auto &statements = g_nodes[listId].kids;
            statements.erase(statements.begin() + static_cast<std::ptrdiff_t>(index));
            statements.insert(statements.begin() + static_cast<std::ptrdiff_t>(index), items.begin(), items.end());
            if (!PrepareExpandedMacroLambdas()) return out;
            continue;
        }
        expansionDepth = 0;
        const simple_ast::NodePtr node = LowerStmt(stmtId);
        if (node) out.push_back(node);
        ++index;
    }
    return out;
}

/**
 * @brief 固定数组维度采用无副作用的编译期常量求值。
 */
bool ReadDims(int dimsId, Memory::Vector<int> *out)
{
    for (const int exprId : N(dimsId).kids)
    {
        MConstant value;
        if (!EvaluateConstant(exprId, &value)) return false;
        if (value.kind != MConstant::Integer) return Fail("[CHL1300] 数组维度必须是整数常量表达式");
        if (value.integer <= 0 || value.integer > INT32_MAX) return Fail("数组维度必须是正整数，且不能超过 int32 上限");
        out->push_back(static_cast<int>(value.integer));
    }
    return true;
}

/**
 * @brief 一个左值 / 表达式的"名字"（诊断文案用：`row` / `row 第 2 维`）。
 * @param id 待查找或操作的对象编号。
 * @param suffix 名称后追加的内部后缀。
 * @retval 生成或读取的结果文本。
 */
Memory::String NameOfExpr(int id, const Memory::String &suffix = Memory::String())
{
    int cursor = id;
    while (cursor >= 0)
    {
        const MNode &node = N(cursor);
        if (node.kind == MNode::Var)
        {
            /* 运行时下标错误应显示源码变量名，不能泄露局部重命名的 $序号。 */
            return node.s + suffix;
        }
        if ((node.kind == MNode::Index || node.kind == MNode::Member || node.kind == MNode::Arrow || node.kind == MNode::Un) && !node.kids.empty())
        {
            cursor = node.kids[0];
            continue;
        }
        break;
    }
    return suffix.empty() ? Memory::String("表达式") : (Memory::String("表达式") + suffix);
}

/**
 * @brief 下标：多维按行主序折成一个线性下标，交给 simple_ast 的"基址 + 下标×8"。
 *
 * `m[i][j]` 在语法树里是 `index(index(m, i), j)`，这里一次折平：
 * 线性下标 = i × 4 + j（每一维的长度都写在声明里），**每一维先查越界**再折。
 * @param baseId 索引基址节点。
 * @param positionId 下标表达式节点。
 * @param bufferIndex 是否按引用缓冲区元素访问，跳过被引用字符串或容器的内建下标。
 * @return 索引读取节点或聚合元素地址；失败时登记诊断。
 */
simple_ast::NodePtr LowerIndexChain(int baseId, int positionId, bool bufferIndex = false)
{
    /* 接口引用是"地址 + 类型号"的胖引用（值在对象表里），**不能取下标**：
     * 它前面没有元素布局可谈。要访问对象内部就走具体类型（`b.get()` 取回来）。 */
    if (IsInterfacePointer(RawExprType(baseId)))
    {
        return Fail("接口引用不能取下标（它是「地址 + 类型号」的胖引用）："
                    "先用 `o.get()` 按具体类型取回对象，再访问它的元素"),
               Bad();
    }
    Memory::Vector<int> indices{ positionId };
    int cursor = baseId;
    /* `matrix[i][j]` 的同型数组链需要折平；`list[i][j]` 可能是先取出 String/List，
     * 再对结果取下标，必须保留为两次独立操作。 */
    const std::shared_ptr<MType> immediateBase = ExprType(baseId);
    const bool indexesPreviousValue = immediateBase && (IsStandardOpcode(immediateBase, "String") || IsContainer(immediateBase));
    while (!indexesPreviousValue && N(cursor).kind == MNode::Index)
    {
        indices.insert(indices.begin(), N(cursor).kids[1]);
        cursor = N(cursor).kids[0];
    }
    auto base = RawExprType(cursor);
    if (!bufferIndex && base->kind == MType::Ptr && base->elem && (IsStandardOpcode(base->elem, "String") || base->elem->kind == MType::Array || IsContainer(base->elem))) base = base->elem;
    if (IsContainer(base))
    {
        /* `v[i]` / `m[k]`：全是运行期对象，下标/键交给 C++ 侧查（越界 / 没这个键都有说法）。 */
        if (indices.size() != 1u)
        {
            Fail("容器只支持一维下标：" + Describe(base));
            return Bad();
        }
        const bool elementText = base->elem && IsStandardOpcode(base->elem, "String");
        if (base->name == "vec")
        {
            if (!CheckContainerIndex(indices[0], "vec 下标")) return Bad();
            return simple_ast::Node::call(
                "__std_vec_at", { "__chali_v", "__chali_i", "__chali_t", "__chali_o" },
                { OwnedObjectTemp(cursor, base), LowerExpr(indices[0]),
                  simple_ast::Node::i(elementText ? 1 : 0),
                  ContainerStorageFlag(base->elem) });
        }
        if (base->name == "map")
        {
            const bool keyText = base->elem && IsStandardOpcode(base->elem, "String");
            const bool valueText = base->elem2 && IsStandardOpcode(base->elem2, "String");
            const auto keyType = base->elem;
            if (!CheckContainerArgument(keyType, indices[0], "map 下标键", false)) return Bad();
            Memory::Vector<simple_ast::NodePtr> prefix, post;
            struct SinkGuard
            {
                Memory::Vector<simple_ast::NodePtr> *saved;
                /**
                 * @brief 保存并切换当前表达式清理语句的接收位置。
                 * @param sink 接收生成语句或资源清理操作的容器。
                 */
                explicit SinkGuard(Memory::Vector<simple_ast::NodePtr> *sink) : saved(g_freshSink) { g_freshSink = sink; }
                /**
                 * @brief 恢复进入表达式前的清理语句接收位置。
                 */
                ~SinkGuard() { g_freshSink = saved; }
            } sink(&prefix);
            const auto receiver = OwnedObjectTemp(cursor, base);
            const auto key = ContainerStoreValue(indices[0], keyType, &post);
            const auto call = simple_ast::Node::call("__std_map_get",
                                                     { "__chali_v", "__chali_k", "__chali_t", "__chali_o", "__chali_t", "__chali_o" },
                                                     { receiver, key, simple_ast::Node::i(keyText), ContainerStorageFlag(keyType),
                                                       simple_ast::Node::i(valueText), ContainerStorageFlag(base->elem2) });
            const auto result = AllocateName("__chali_map_get_result");
            prefix.push_back(simple_ast::Node::declare(result, LowerType(base->elem2, result)));
            prefix.push_back(simple_ast::Node::assign(result, call));
            prefix.insert(prefix.end(), post.begin(), post.end());
            return simple_ast::Node::sequence(std::move(prefix), simple_ast::Node::var(result));
        }
        Fail("set 没有下标：用 `s.has(x)` 查在不在、`s.at(i)` 按下标取（按升序）");
        return Bad();
    }
    for (const int position : indices)
        if (!CheckContainerIndex(position, "下标")) return Bad();
    if (IsStandardOpcode(base, "String"))
    {
        /* 字符串变量与引用统一按字节下标访问。
         * 字符串按**字节**下标：`s[i]` 得到第 i 个字节（0..255）。
         * 交给运行时的 `__std_str_at`：**一次读一个字节**（越界会报错，下标写到结尾的
         * `\0` 是允许的 —— `while (s[i] != 0)` 这种数长度写法要用它）。
         * 早先是"offset 算地址 + 读 8 字节 + & 255"，字符串搬到堆上之后那样读会
         * 越过块尾（堆块是紧的）。 */
        if (indices.size() != 1u)
        {
            Fail("字符串只支持一维下标（`s[i]`）");
            return Bad();
        }
        return simple_ast::Node::call(
            "__std_str_at", { "__chali_s", "__chali_i" },
            { OwnedObjectTemp(cursor, base), LowerExpr(indices[0]) });
    }
    if (IsRuntimeArray(base))
    {
        /* 运行时长度数组：长度存在对象里（句柄前 8 字节），下标逐维查。 */
        if (indices.size() != 1u)
        {
            Fail("运行时数组只支持一维下标（`a[i]`）");
            return Bad();
        }
        const Memory::String label = NameOfExpr(cursor);
        const simple_ast::NodePtr length = simple_ast::Node::loadInt(simple_ast::Node::binary(
            "-", LowerExpr(cursor), simple_ast::Node::i(8)));
        const simple_ast::NodePtr position =
            simple_ast::Node::checkIndex(LowerExpr(indices[0]), length, label);
        const int stride = StructStride(base->elem);
        if (stride > 0) return simple_ast::Node::offset(LowerExpr(cursor), position, stride);
        if (IsByteStorageType(base->elem))
            return simple_ast::Node::loadFrom(simple_ast::Node::offset(LowerExpr(cursor), position, 1), LowerType(base->elem, label));
        return simple_ast::Node::index(LowerExpr(cursor), position);
    }
    if (base->kind == MType::Array)
    {
        if (indices.size() != base->dims.size())
        {
            Fail("多维数组必须把每一维都写下（不支持把一行当句柄用）");
            return Bad();
        }
        /* 逐维查越界：`m[i][j]` 里 `m[i]` 超了就报 i 那一维，`[j]` 超了报 j 那一维。 */
        const Memory::String name = NameOfExpr(cursor);
        Memory::Vector<simple_ast::NodePtr> checked;
        for (std::size_t index = 0u; index < indices.size(); ++index)
        {
            const Memory::String label =
                (indices.size() == 1u) ? name : (name + " 第 " + Memory::toString(index + 1u) + " 维");
            checked.push_back(simple_ast::Node::checkIndex(
                LowerExpr(indices[index]),
                simple_ast::Node::i(base->dims[index]), label));
        }
        simple_ast::NodePtr linear = checked[0];
        for (std::size_t index = 1u; index < checked.size(); ++index)
        {
            linear = simple_ast::Node::binary("*", linear,
                                              simple_ast::Node::i(base->dims[index]));
            linear = simple_ast::Node::binary("+", linear, checked[index]);
        }
        /* 元素是结构体：`arr[i]` 是那个元素的**句柄**（地址），步长是"字段数 × 8"，
         * 用 offset（不补 load）；标量元素才用 index（base + i×8 再取值）。 */
        const int stride = StructStride(base->elem);
        if (stride > 0) return simple_ast::Node::offset(LowerRawExpr(cursor), linear, stride);
        if (IsByteStorageType(base->elem))
            return simple_ast::Node::loadFrom(simple_ast::Node::offset(LowerRawExpr(cursor), linear, 1), LowerType(base->elem, name));
        return simple_ast::Node::index(LowerRawExpr(cursor), linear);
    }
    if (base->kind == MType::Ptr)
    {
        if (indices.size() != 1u)
        {
            Fail("引用只支持一维下标（`p[i]`）");
            return Bad();
        }
        const int stride = StructStride(base->elem);
        const int bytes = ElementBytes(base->elem);
        const simple_ast::NodePtr position = simple_ast::Node::checkPointer(
            LowerExpr(indices[0]), LowerRawExpr(cursor), bytes, NameOfExpr(cursor));
        if (stride > 0) return simple_ast::Node::offset(LowerRawExpr(cursor), position, stride);
        if (IsByteStorageType(base->elem))
            return simple_ast::Node::loadFrom(simple_ast::Node::offset(LowerRawExpr(cursor), position, 1), LowerType(base->elem, "pointer index"));
        return simple_ast::Node::index(LowerRawExpr(cursor), position);
    }
    Fail("下标只能用在数组或引用上");
    return Bad();
}

/**
 * `.字段` / `->字段`：结构体字段是"基址 + 序号×8"，引用里的值本身就是那片结构体的地址。
 */
/**
 * @brief 取字段的**地址**（不取值）：`a.f` / `p->f` 都先算到这一步。
 *
 * 结构体字段是按**摊平后的槽号**定位的（见 FlattenStructFields）：`槽号 * 8` 就是
 * 它在对象里的偏移。用"算偏移"而不是 simple_ast 的 `member(基, 名字)`：
 * 后者是"拿基类型按名字查下标"，而嵌套结构体字段（`a.inner.x`）的基类型是**内层**
 * 结构体，查出来会是内层的下标（不是内层在外的偏移）—— 算出来就错位了。
 *
 * 引用接收者（`p->f`）先按目标 struct 类型转一次，simple_ast 才知道"这块是哪个结构体"。
 *
 * @param exprId `MNode::Member` / `MNode::Arrow`。
 * @param field 输出：字段定义（拿它的槽号与类型）。
 * @param owner 输出：所属 struct 名。
 * @return 地址表达式；出错返回坏节点。
 */
/**
 * @brief 按名字在 struct 里找字段定义（找不到返回 nullptr）。
 * @param owner 所属对象或类型信息。
 * @param field 待访问的字段名称或描述。
 * @retval 对应对象的指针。
 */
const MField *FindField(const Memory::String &owner, const Memory::String &field)
{
    const auto found = g_structs.find(owner);
    if (found == g_structs.end()) return nullptr;
    for (const MField &item : found->second.fields)
    {
        if (item.name == field) return &item;
    }
    return nullptr;
}

/**
 * @brief 字段的**地址**：`基址 + 槽号 × 8`（见 FlattenStructFields）。
 *
 * 为什么不用 simple_ast 的 `member(基, 名字)`（它是"按名字在基类型里查下标"）：
 * 嵌套字段 `a.inner.x` 里，外层那一步的"基类型"是**内层**结构体，查出来的下标是
 * 内层自己的下标，而不是内层在外的偏移 —— 算出来就错位了。自己按槽号算就不会。
 *
 * 引用基址（`p->f`）先按目标 struct 类型转一次，simple_ast 才知道"这块是哪个结构体"。
 */
simple_ast::NodePtr FieldAddress(const simple_ast::NodePtr &baseNode,
                                 const std::shared_ptr<MType> &baseType, const Memory::String &owner,
                                 const MField &declared)
{
    simple_ast::NodePtr node = baseNode;
    if (baseType && baseType->kind == MType::Ptr)
    {
        const auto ownerType = g_structType.find(owner);
        if (ownerType == g_structType.end())
        {
            Fail("没有登记 struct " + owner);
            return Bad();
        }
        node = simple_ast::Node::cast(node, ownerType->second);
    }
    return simple_ast::Node::offset(node, simple_ast::Node::i(declared.slot), 8);
}

/**
 * @brief `基.字段` / `基->字段` 取值（`LowerExpr` 传的是**基址 id + 字段名**）。
 *
 * 两种情况：
 *
 *   * 值字段（`struct Box { Point inner; }`）：它就长在 Box 里，所以"这个字段的值"
 *     是那块地方**自己的地址**（不再是从某个槽里 load 出一个句柄）—— 于是
 *     `a.inner.x` 变成"地址 + 0 再取 x"，`a.inner = p` 是往这块地方深拷贝。
 *   * **叶子字段**：按字段类型从那个槽读一个值（`(load:T, 地址, _, t)`）。
 */
simple_ast::NodePtr LowerMember(int baseId, const Memory::String &field)
{
    const std::shared_ptr<MType> base = ExprType(baseId);
    if (field == "opcode")
        if (const auto *declaration = FindOpcodeDeclaration(base))
        {
            if (g_currentModule != declaration->module) return Fail("opcode 内部句柄仅允许在声明模块中访问"), Bad();
            if (!declaration->aggregate) return OwnedObjectTemp(baseId, base);
            return simple_ast::Node::loadInt(LowerExpr(baseId));
        }
    if (IsEnumType(base))
    {
        Fail("enum 载荷不能直接按字段读取：" + base->name + "." + field + "；请在 match 的 `" + base->name + "::变体(绑定名) => { ... }` 中解构");
        return Bad();
    }
    /* 接口引用只能**调接口里声明过的方法**（那条路在 LowerCall 里，走运行期分派）；
     * 接口本身没有字段，所以 `.字段` 是错的。 */
    if (IsInterfacePointer(base))
    {
        Fail("接口引用没有字段可以取（接口只声明函数）：" + field + "；要调方法就直接写 `q." + field + "(…)`，要取对象内部的字段请先 "
                                                                                                       "`o.get()` 取回具体类型");
        return Bad();
    }
    const Memory::String owner = StructNameOf(base);
    if (owner.empty())
    {
        Fail("`.` / `->` 只能用在 struct 上");
        return Bad();
    }
    const MField *declared = FindField(owner, field);
    if (declared == nullptr)
    {
        if (IsTupleType(base))
        {
            const Memory::String shown = field.rfind("__", 0u) == 0u ? field.substr(2u) : field;
            Fail("元组 " + Describe(base) + " 没有第 " + shown + " 个元素");
            return Bad();
        }
        Fail("struct " + owner + " 没有字段 " + field);
        return Bad();
    }
    const simple_ast::NodePtr address = FieldAddress(OwnedAggregateTempFor(baseId, base), base, owner, *declared);
    if (!address) return Bad();
    if (declared->type->kind == MType::Struct || (declared->type->kind == MType::Array && !declared->type->dims.empty())) return address;
    return simple_ast::Node::loadFrom(address, LowerType(declared->type, owner + "." + field));
}

/**
 * @brief 将方法内未被局部变量遮蔽的字段名展开为 self 的成员节点。
 * @param id 原始变量节点。
 * @return 显式成员节点；不是隐式接收者字段时返回 -1。
 */
int ExplicitReceiverField(int id)
{
    if (id < 0 || N(id).kind != MNode::Var) return -1;
    const auto source = N(id);
    if (NodeUsesDefinitionLookup(source) || g_owner.empty() || FindNodeVar(source) || !FieldType(g_owner, source.s)) return -1;
    const int self = M_New(MNode::Var, "self");
    g_nodes[self].module = source.module;
    const int member = M_New(MNode::Member, source.s);
    g_nodes[member] = source;
    g_nodes[member].kind = MNode::Member;
    g_nodes[member].kids = { self };
    return member;
}

/**
 * @brief 为内部句柄字段转发取得原始所有者变量的引用。
 * @param id 被取引用的表达式编号。
 * @return 表达式对应的稳定存储地址。
 */
simple_ast::NodePtr LowerAddressOf(int id);

/**
 * @brief 成员**节点**的地址（`&a.f` 那条路用它：传的是 `MNode::Member` / `MNode::Arrow` 的 id）。
 * @param memberId 成员节点。
 * @param field 输出：字段定义（拿它的槽号与类型）。
 * @param owner 输出：所属 struct 名。
 * @return 地址表达式。
 */
simple_ast::NodePtr LowerFieldAddress(int memberId, const MField **field, Memory::String *owner)
{
    const MNode &node = N(memberId);
    /* 先把要用的东西抄下来：下面 `LowerExpr` 会往节点池里加东西，
     * 池子一扩容，`node` 这个引用就悬了。 */
    const int baseId = node.kids.empty() ? -1 : node.kids[0];
    const Memory::String fieldName = node.s;
    const std::shared_ptr<MType> base = ExprType(baseId);
    if (fieldName == "opcode")
        if (const auto *declaration = FindOpcodeDeclaration(base); declaration && !declaration->aggregate)
        {
            if (g_currentModule != declaration->module) return Fail("opcode 内部句柄仅允许在声明模块中访问"), Bad();
            *owner = base->name;
            *field = FindField(*owner, fieldName);
            return LowerAddressOf(baseId);
        }
    if (IsEnumType(base))
    {
        Fail("enum 载荷不能直接按字段写入：" + base->name + "." + fieldName + "；请构造一个新的枚举值再赋值");
        return Bad();
    }
    if (IsInterfacePointer(base))
    {
        Fail("接口引用没有字段可以取（接口只声明函数）：" + fieldName);
        return Bad();
    }
    *owner = StructNameOf(base);
    if (owner->empty())
    {
        Fail("`.` / `->` 只能用在 struct 上");
        return Bad();
    }
    *field = FindField(*owner, fieldName);
    if (*field == nullptr)
    {
        Fail("struct " + *owner + " 没有字段 " + fieldName);
        return Bad();
    }
    return FieldAddress(LowerExpr(baseId), base, *owner, **field);
}

/**
 * 一元运算：`!` / 一元 `-` / `*`（解引用等价于 `p[0]`）。
 */
/**
 * @brief `&x` 取得元素引用，沿用来源的可写性。
 *
 * 能取地址的是**具有稳定存储的对象**：数组 / 引用元素（`&a[i]`）、结构体字段（`&s.f`、
 * `&p.f`）、字符串的第 i 个字节（`&s[i]`，步长 1）。被取引用的标量、字符串、大数与容器变量
 * 通过隐藏单元素存储保持变量与引用的读写一致。
 */
simple_ast::NodePtr LowerAddressOf(int id)
{
    if (const int member = ExplicitReceiverField(id); member >= 0) return LowerAddressOf(member);
    const MNode &node = N(id);
    if (RawExprType(id)->kind == MType::Ptr) return LowerRawExpr(id);
    if (node.kind == MNode::Var && !NodeUsesDefinitionLookup(node) && node.s == "self" && !g_owner.empty()) return simple_ast::Node::var("self");
    const auto temporaryType = ExprType(id);
    if (g_freshSink && IsContainer(temporaryType) && IsFreshValue(id))
    {
        /* 临时容器入口也需要句柄槽；清理读取转移后的当前句柄。 */
        const auto owner = AllocateName("__chali_iter_source");
        const auto storage = AllocateName("__chali_iter_source_slot");
        const auto value = LowerExpr(id);
        g_freshSink->push_back(simple_ast::Node::declare(owner, LowerType(temporaryType, owner)));
        g_freshSink->push_back(simple_ast::Node::assign(owner, TakeExpressionTempValue(value, temporaryType)));
        g_freshSink->push_back(simple_ast::Node::declare(storage, simple_ast::Type::i64()));
        g_freshSink->push_back(simple_ast::Node::assign(storage, simple_ast::Node::alloc(simple_ast::Node::i(8))));
        g_freshSink->push_back(simple_ast::Node::storeAt(simple_ast::Node::var(storage),
                                                         simple_ast::Node::var(owner), LowerType(temporaryType, storage)));
        g_referenceStorage[owner] = storage;
        // 先登记槽，再登记当前拥有值：逆序清理时值仍可读取自己的槽。
        MarkOwned(storage);
        NoteExpressionTemp(storage);
        MarkOwned(owner);
        NoteExpressionTemp(owner);
        return simple_ast::Node::var(storage);
    }
    if (node.kind == MNode::EnumCtor)
        if (const MVar *global = FindQualifiedGlobal(node.s, node.s2); global != nullptr && global->elementReference)
            return simple_ast::Node::var(global->emitted);
    if (node.kind == MNode::Var)
    {
        /* `&结构体变量`：值以聚合存储地址表示，局部按调用分配，全局使用数据区，
         * 所以取地址得到已有存储 —— 于是 `o.set(&p)` 能把一个**现成的**
         * 结构体装进 obj（`&p` 的类型是只读的 `Point const&`）。
         *
         * 标量、字符串、大数与容器变量由隐藏单元素槽提供地址；结构体与元组
         * 直接使用自身的存储地址。运行时数组等其他句柄不在此处转换为变量引用。 */
        const MVar *found = FindNodeVar(node);
        if (found != nullptr && found->elementReference)
            return simple_ast::Node::var(found->emitted);
        if (found != nullptr && found->type && (found->type->kind == MType::Struct || (found->type->kind == MType::Array && !IsRuntimeArray(found->type))))
        {
            return LowerExpr(id);
        }
        Fail("`&` 取不了 " + node.s + " 的地址：该类型没有可取引用的变量存储。能取地址的是：标量、字符串、大数、"
                                      "容器、结构体与元组变量（`&p`）、数组 / 引用的元素"
                                      "（`&a[i]`）、结构体字段（`&s.f`）、字符串的字节（`&s[i]`）");
        return Bad();
    }
    if (node.kind == MNode::Index)
    {
        auto base = RawExprType(node.kids[0]);
        const bool valueReference = node.s != "__chali_buffer_index" && base->kind == MType::Ptr && base->elem && (IsStandardOpcode(base->elem, "String") || base->elem->kind == MType::Array);
        if (valueReference) base = base->elem;
        if (!CheckContainerIndex(node.kids[1], "下标")) return Bad();
        const auto target = valueReference ? LowerExpr(node.kids[0]) : LowerRawExpr(node.kids[0]);
        const simple_ast::NodePtr position = LowerExpr(node.kids[1]);
        const Memory::String label = NameOfExpr(node.kids[0]);
        if (IsStandardOpcode(base, "String")) /* `&s[i]`：字符串按字节 */
        {
            return simple_ast::Node::call("__std_str_ref", { "__chali_s", "__chali_i" }, { target, position });
        }
        if (IsRuntimeArray(base))
        {
            /* 运行时数组：长度在对象里（句柄前 8 字节），先查越界再算地址 */
            const simple_ast::NodePtr length = simple_ast::Node::loadInt(simple_ast::Node::binary(
                "-", target, simple_ast::Node::i(8)));
            const int stride = StructStride(base->elem);
            return simple_ast::Node::offset(
                target, simple_ast::Node::checkIndex(position, length, label),
                ElementBytes(base->elem));
        }
        if (base->kind == MType::Ptr)
        {
            const int stride = StructStride(base->elem);
            const int bytes = ElementBytes(base->elem);
            return simple_ast::Node::offset(
                target, simple_ast::Node::checkPointer(position, target, bytes, label), bytes);
        }
        if (base->kind == MType::Array)
        {
            /* 一次下标：把后面的维度先折进系数（与 `a[i]` 的线性化同一套算法） */
            int factor = 1;
            for (std::size_t index = 1u; index < base->dims.size(); ++index)
                factor *= base->dims[index];
            const simple_ast::NodePtr checked = simple_ast::Node::checkIndex(
                position, simple_ast::Node::i(base->dims[0]),
                (base->dims.size() == 1u) ? label : (label + " 第 1 维"));
            const simple_ast::NodePtr linear =
                (factor == 1) ? checked : simple_ast::Node::binary("*", checked, simple_ast::Node::i(factor));
            const int stride = StructStride(base->elem);
            return simple_ast::Node::offset(target, linear, ElementBytes(base->elem));
        }
        Fail("`&` 只能取数组 / 引用元素或结构体字段的地址");
        return Bad();
    }
    if (node.kind == MNode::Member || node.kind == MNode::Arrow)
    {
        /* 字段地址就是"基址 + 槽号 × 8"（见 LowerFieldAddress）—— 值字段（嵌套结构体）
         * 与叶子字段一样都是这块地方，区别只在于取不取那次 load。 */
        const MField *declared = nullptr;
        Memory::String owner;
        return LowerFieldAddress(id, &declared, &owner);
    }
    Fail("`&` 只能取数据区里对象的地址（数组元素 / 结构体字段 / 字符串字节；栈上的标量变量取不了）");
    return Bad();
}

/**
 * @brief 检查并降级一元运算表达式。
 * @param op 运算符文本或类别。
 * @param operandId 操作数在节点池中的编号。
 * @retval 生成的低层语法节点。
 */
simple_ast::NodePtr LowerUnary(const Memory::String &op, int operandId)
{
    if (op == "&") return LowerAddressOf(operandId);
    if (ExprType(operandId)->kind == MType::Host)
    {
        Fail("宿主类型不支持一元运算 " + op + "；请实现对应接口或先显式转换");
        return Bad();
    }
    if (op == "!" && ExprType(operandId)->kind != MType::Bool)
    {
        Fail("逻辑非 ! 只能用于 bool，现在给的是 " + Describe(ExprType(operandId)));
        return Bad();
    }
    /* `-a`（大整数）：取负也是对象表里的活儿，结果是**新的**大整数。 */
    if (op == "-")
    {
        const std::shared_ptr<MType> type = ExprType(operandId);
        if (type && IsStandardOpcode(type, "BigInteger"))
        {
            return simple_ast::Node::call("__std_bint_neg", { "__chali_v" },
                                          { LowerExpr(operandId) });
        }
        if (type && IsStandardOpcode(type, "BigDecimal"))
        {
            return simple_ast::Node::call("__std_bfloat_neg", { "__chali_v" },
                                          { LowerExpr(operandId) });
        }
    }
    if (op == "*")
    {
        const std::shared_ptr<MType> pointer = RawExprType(operandId);
        if (!pointer || pointer->kind != MType::Ptr)
        {
            Fail("`*` 解引用只能用在引用上");
            return Bad();
        }
        /*
         * 结构体的"值"本来就是**地址**（和结构体字段的存法一致，见 `StructField`），
         * 所以指向结构体的 `*p` 直接把引用当那个结构体的地址用，**不再 load** ——
         * 这样 `(*p).字段` 与 `p->字段` 才是同一条路（以前多 load 一次，读到的是
         * 引用目标第一个槽的值，再拿它当地址，结果是垃圾）。
         * 标量 / 其它目标仍然要 load：`*p` 就是那个值。
         */
        const std::shared_ptr<MType> target = pointer->elem;
        if (target && (target->kind == MType::Struct || target->kind == MType::Array || IsEnumType(target)))
            return LowerRawExpr(operandId);
        return simple_ast::Node::loadFrom(LowerRawExpr(operandId), LowerType(target, "reference value"));
    }
    /*
     * 用户自定义一元运算符（`-x` / `~x` / `+x`）：操作数是 struct / 枚举就查重载。
     * 走的是和二元同一条路 —— 实现 Negate / BitNot / Positive 接口，调用点发一次
     * 方法调用，接收者当 `self` 传进去。
     */
    const std::shared_ptr<MType> subject = ExprType(operandId);
    if (subject != nullptr && (subject->kind == MType::Struct || IsEnumType(subject)))
    {
        const MOperatorDecl *declaration = FindUnaryOperator(op, subject);
        if (declaration != nullptr)
        {
            auto value = OwnedAggregateTempFor(operandId, subject);
            if (declaration->callNames.size() > 1u && g_freshSink != nullptr)
            {
                const auto temp = AllocateName("__chali_unary_operand");
                g_freshSink->push_back(simple_ast::Node::declare(temp, LowerType(subject, temp)));
                g_freshSink->push_back(simple_ast::Node::assign(temp, value));
                value = simple_ast::Node::var(temp);
            }
            Memory::Vector<simple_ast::NodePtr> values(declaration->callNames.size(), value);
            return simple_ast::Node::call(declaration->label, declaration->callNames, values);
        }
        if (!g_error.empty()) return Bad(); /* 报了"多个重载同样匹配" */
        Fail("一元运算符 " + op + " 在 " + Describe(subject) + " 上没有实现（实现 Negate / BitNot / Positive 接口）");
        return Bad();
    }
    // ChaLi 整数字面量为 int64；simple_ast 的无类型字面量默认 int32，先保留前端位宽。
    auto operand = LowerExpr(operandId);
    if (subject && subject->kind == MType::Int && (op == "-" || op == "+" || op == "~"))
        operand = simple_ast::Node::cast(operand, LowerType(subject, "一元运算"));
    return simple_ast::Node::unary(op, operand);
}

/**
 * @brief 对象类类型（`bint` / 容器）的**临时对象落地**。
 *
 * `f(x).str()` / `(a + b) > c` 这种写法里，"当场造出来的对象"用完就没人管了 ——
 * 不占堆字节，但**对象表会一直涨**。这里将它保存为完整表达式的内部所有者，
 * 最终结果保存后回收并清空槽位；return / break 的提前退出沿用块登记兜底。
 *
 * @param exprId 表达式。
 * @param type 表达式类型（不是对象类就原样返回）。
 * @return 表达式节点（可能是那个临时变量）。
 */
simple_ast::NodePtr OwnedObjectTempNode(const simple_ast::NodePtr &value,
                                        const std::shared_ptr<MType> &type)
{
    if (g_freshSink == nullptr || (!IsObjectLike(type) && (!type || !IsStandardOpcode(type, "String"))))
        return value;
    const Memory::String temp = AllocateName("__chali_tmp_o");
    g_freshSink->push_back(simple_ast::Node::declare(temp, LowerType(type, temp)));
    g_freshSink->push_back(MakeDrop(temp));
    g_freshSink->push_back(simple_ast::Node::assign(temp, TakeExpressionTempValue(value, type)));
    MarkOwned(temp);
    NoteExpressionTemp(temp);
    return simple_ast::Node::var(temp);
}

/**
 * @brief 为拥有资源的对象建立临时值及清理记录。
 * @param exprId 表达式在节点池中的编号。
 * @param type 类型描述。
 * @retval 生成的低层语法节点。
 */
simple_ast::NodePtr OwnedObjectTemp(int exprId, const std::shared_ptr<MType> &type)
{
    const simple_ast::NodePtr value = LowerExpr(exprId);
    if (!IsFreshValue(exprId)) return value; /* 借来的（变量 / 字段）不落地 */
    return OwnedObjectTempNode(value, type);
}

/**
 * @brief 这个表达式是不是"**调用点拥有**的当场构造聚合"（比 `IsFreshAggregate` 宽）。
 *
 * 和上面那个（决定"搬还是深拷贝"）不同，这里还收**元组字面量**：元组走
 * `__chali_make__chali_tuple_*`，存储是数据区临时、叶子归调用点。用在"容器复制完内容后
 * 递归释放这个临时"这一条上，所以借来的变量 / 字段 / 下标不能算。
 * `Cast` 递归看内层（和 `IsFreshAggregate` 一个口径）。
 *
 * 用户自定义下标（实现了 `Index<R, K>` 的结构体 / 宿主类型，如 `Matrix<T>::index`）和
 * 函数调用一样**按值返回**：`Matrix<T> m; Vector<T> v = m[1];` 里 `m[1]` 的结果是调用点
 * 当场造出来的新值。当"借来的"处理会先深拷贝、再把原来那份带资源的临时丢掉（漏一个
 * 容器 / 串）。容器 / 运行期数组的下标仍是元素别名，照旧不算。
 */
bool IsOwnedAggregateTemporary(int exprId)
{
    if (exprId < 0) return false;
    if (IsBorrowedObjectValueCall(exprId)) return false;
    const MNode &node = N(exprId);
    if (RawExprType(exprId)->kind == MType::Ptr || (node.kind == MNode::Un && node.s == "*")) return false;
    if (node.kind == MNode::Try) return OwnedPropagation(exprId);
    if (node.kind == MNode::Tuple || node.kind == MNode::Call || node.kind == MNode::Loop) return true;
    if (node.kind == MNode::EnumCtor) return true;
    /* 运算符重载（`a + b` / `-a`）走方法调用，返回值归调用点。 */
    if (node.kind == MNode::Bin || node.kind == MNode::Un) return true;
    /* 用户 `Index<R, K>`：`index(k)` 是普通方法调用，返回值归调用点。 */
    if (node.kind == MNode::Index && node.kids.size() > 1u && FindIndex(ExprType(node.kids[0]), ExprType(node.kids[1])) != nullptr)
        return true;
    if (node.kind == MNode::Cast) return !node.kids.empty() && (HasDeclaredConversion(ExprType(node.kids[0]), T(node.type), true) || IsOwnedAggregateTemporary(node.kids[0]));
    return false;
}

/**
 * @brief 把"当场造出来的聚合"落进一个**不归本块所有**的临时变量，并把它的递归释放
 *        写进 `post`（调用方在消费完这个值之后补在语句尾部）。
 *
 * 不能登记成块作用域所有：数据区临时对象在循环里每轮被覆盖，块出口只释放一次会漏。
 * 消费点（容器写入 / 下标赋值）复制完内容就把叶子还掉，能对付循环内的重复构造。
 */
simple_ast::NodePtr AggregateTempNode(const simple_ast::NodePtr &value,
                                      const std::shared_ptr<MType> &type,
                                      Memory::Vector<simple_ast::NodePtr> *post)
{
    if (g_freshSink == nullptr || post == nullptr) return value;
    const Memory::String owner = StructNameOf(type);
    if (owner.empty()) return value;
    const Memory::String temp = AllocateName("__chali_tmp_agg");
    g_freshSink->push_back(simple_ast::Node::declare(temp, LowerType(type, temp)));
    g_freshSink->push_back(simple_ast::Node::assign(temp, value));
    if (const simple_ast::NodePtr release =
            MakeAggregateRelease(owner, simple_ast::Node::var(temp)))
        post->push_back(release);
    return simple_ast::Node::var(temp);
}

/**
 * @brief 要写进容器的元素值（`push` / `add` / `put` / 下标写）：容器会**复制**它。
 *
 * 所以当场构造的字符串 / 对象临时值先落地，并登记完整表达式回收，否则就漏了
 * （`split` 里的 `parts.push(text[start:])` 撞的正是这条）。字面量在数据区、
 * 已有变量是借用，都维持原样。
 *
 * 结构体 / 枚举这种**当场造出来的聚合**（元组字面量、枚举构造器、返回结构体的调用）
 * 走另一条：它自己的存储是数据区临时，但里面的字符串 / 对象叶子归它所有，容器复制
 * 完内容之后要把这些叶子还掉 —— 否则每个元素漏一块。释放语句不能放在调用**之前**，
 * 所以通过 `post` 回给调用方，由它拼成"先算值 → 再调用 → 再释放"的块。
 * 数组 / 资源句柄仍然不动。
 */
simple_ast::NodePtr ContainerStoreValue(int argId, const std::shared_ptr<MType> &element,
                                        Memory::Vector<simple_ast::NodePtr> *post = nullptr)
{
    const simple_ast::NodePtr value = LowerValueForType(argId, element);
    if (!element) return value;
    if (element->kind == MType::Struct)
    {
        if (post == nullptr || (!IsOwnedAggregateTemporary(argId) && !IsImplicitConversionValue(argId, element))) return value;
        const Memory::String owner = StructNameOf(element);
        if (owner.empty() || !StructHasOwnedLeaves(owner)) return value;
        return AggregateTempNode(value, element, post);
    }
    if (!IsObjectLike(element) && !IsStandardOpcode(element, "String"))
        return value;
    if (!IsFreshValue(argId) && !IsImplicitConversionValue(argId, element)) return value;
    if (IsStandardOpcode(element, "String") && argId >= 0 && N(argId).kind == MNode::Str)
        return value;
    return OwnedObjectTempNode(value, element);
}

/**
 * @brief 把"要写进去的新值"先算进一个**不归本块所有**的临时变量，返回引用它的表达式。
 *
 * 为什么需要这么一步：给"块作用域所有"的变量重新赋值时，得先把旧值还回去；
 * 而 `s = s + "x"` 这种写法里，右边还要**读**旧值 —— 顺序必须是
 *
 *     先算新值 → 再释放旧值 → 再写进变量
 *
 * 这个临时变量**不登记成所有者**：它装的就是"马上要赋进去的那个值"，块出口（或
 * break / continue / return 的自动释放）释放它，等于把刚赋进去的东西当场 free 掉。
 *
 * @param value 新值的表达式。
 * @param type 目标类型。
 * @param sink 前置语句写哪儿。
 * @return 引用临时变量的表达式。
 */
simple_ast::NodePtr DeferNewValue(const simple_ast::NodePtr &value,
                                  const std::shared_ptr<MType> &type,
                                  Memory::Vector<simple_ast::NodePtr> *sink)
{
    if (sink == nullptr) return value;
    const Memory::String temp = AllocateName("__chali_new_v");
    sink->push_back(simple_ast::Node::declare(temp, LowerType(type, temp)));
    sink->push_back(simple_ast::Node::assign(temp, value));
    return simple_ast::Node::var(temp);
}

/**
 * @brief 这个表达式是不是"当场造出来的**堆**串"（字面量、null 不算）。
 *
 * 字面量在数据区里（只读、不用释放）、null 是 0，两者都不需要临时变量。
 */
bool NeedsOwnedTemp(int exprId)
{
    if (!IsFreshValue(exprId)) return false;
    const MNode &node = N(exprId);
    return node.kind != MNode::Str && node.kind != MNode::Null;
}

/**
 * @brief 把当场构造的堆串保存为完整表达式的内部临时所有者，返回那个变量。
 *
 * 落地点由 `g_freshSink` 指（语句级别才有），没有就原样返回表达式（那种位置只能让它
 * 留着）。临时变量名带 `__chali_` 前缀，用户写不出来；它在循环里会被反复赋值，所以赋值
 * 前先把上一轮那块软释放掉（第一次是 0，空操作）。
 *
 * @param exprId 表达式。
 * @param sink 落地语句写哪儿（`nullptr` = 用语句级别设好的 `g_freshSink`）。
 * @return 表达式节点（可能是那个临时变量）。
 */
simple_ast::NodePtr OwnedStringTemp(int exprId, Memory::Vector<simple_ast::NodePtr> *sink = nullptr)
{
    const simple_ast::NodePtr value = LowerExpr(exprId);
    Memory::Vector<simple_ast::NodePtr> *target = (sink != nullptr) ? sink : g_freshSink;
    if (target == nullptr) return value;
    const Memory::String temp = AllocateName("__chali_tmp_s");
    const std::shared_ptr<MType> stringType = T(M_TypeString());
    target->push_back(simple_ast::Node::declare(temp, LowerType(stringType, temp)));
    target->push_back(MakeDrop(temp));
    target->push_back(simple_ast::Node::assign(temp, TakeExpressionTempValue(value, stringType)));
    MarkOwned(temp);
    NoteExpressionTemp(temp);
    return simple_ast::Node::var(temp);
}

/**
 * @brief 登记完整表达式拥有的新字符串，用于字符串格式化与大数文本转换。
 */
simple_ast::NodePtr OwnedGeneratedString(const simple_ast::NodePtr &value)
{
    if (g_freshSink == nullptr) return value;
    const Memory::String temp = AllocateName("__chali_tmp_s");
    const std::shared_ptr<MType> stringType = T(M_TypeString());
    g_freshSink->push_back(simple_ast::Node::declare(temp, LowerType(stringType, temp)));
    g_freshSink->push_back(MakeDrop(temp));
    g_freshSink->push_back(simple_ast::Node::assign(temp, TakeExpressionTempValue(value, stringType)));
    MarkOwned(temp);
    NoteExpressionTemp(temp);
    return simple_ast::Node::var(temp);
}

/**
 * @brief 把 `a + b + c` 这样的字符串拼接链摊平成一串操作数（按源码从左到右）。
 *
 * 只往下钻"两边都是字符串的 `+`"：`("x" + y) + "z"` 摊成 `x y z`。这样整条链只需要
 * **一次** `__std_str_join` 调用与**一次**堆分配。
 */
void CollectStringParts(int exprId, Memory::Vector<int> *out)
{
    if (exprId < 0)
    {
        out->push_back(exprId);
        return;
    }
    const MNode &node = N(exprId);
    if (node.kind == MNode::Bin && node.s == "+" && node.kids.size() >= 2u)
    {
        const std::shared_ptr<MType> type = ExprType(exprId);
        if (type && IsStandardOpcode(type, "String"))
        {
            CollectStringParts(node.kids[0], out);
            CollectStringParts(node.kids[1], out);
            return;
        }
    }
    out->push_back(exprId);
}

/**
 * @brief 二元运算：标量走内建运算，有 struct 操作数就必须有对应方向的重载。
 *
 * 重载是顶层自由函数，两个操作数都是普通实参：
 *
 *   a + b   →  `(param, a, _, left)` `(param, b, _, right)` `(call, operator+$…, _, t)`
 *
 * 签名里左右类型都写出来了，所以 `Money + int` 与 `int + Money` 是两个重载。
 */
simple_ast::NodePtr LowerBinary(const Memory::String &op, int leftId, int rightId,
                                int exprId = -1)
{
    const std::shared_ptr<MType> leftType = ExprType(leftId);
    const std::shared_ptr<MType> rightType = ExprType(rightId);
    if ((op == "&&" || op == "||") && (leftType->kind != MType::Bool || rightType->kind != MType::Bool))
    {
        Fail("逻辑运算 " + op + " 两侧都必须是 bool，现在是 " + Describe(leftType) + " 与 " + Describe(rightType));
        return Bad();
    }
    if ((leftType->kind == MType::Bool) != (rightType->kind == MType::Bool))
    {
        Fail("bool 与数值不能隐式混用：" + Describe(leftType) + " " + op + " " + Describe(rightType) + "（请显式强转）");
        return Bad();
    }
    if (leftType->kind == MType::Bool && rightType->kind == MType::Bool && op != "&&" && op != "||" && op != "==" && op != "!=")
    {
        Fail("bool 只支持 &&、||、==、!= 与一元 !，不支持 " + op);
        return Bad();
    }
    /* ---- 字符串内建：拼接（`+`）与比较（字典序）。
     *
     * 两件事都交给**虚拟机里那份 C++ 运行时**（`__std_str_cat` / `__std_str_cmp`）：
     * 语言侧不写逐字节的循环，拼接的结果是堆上的新串（"块作用域所有"，见 MarkOwned）。
     * ---- */
    if (IsStandardOpcode(leftType, "String") || IsStandardOpcode(rightType, "String"))
    {
        const bool nullCompare = (op == "==" || op == "!=") && ((rightType->kind == MType::Ptr && rightType->elem && rightType->elem->kind == MType::Void) || (leftType->kind == MType::Ptr && leftType->elem && leftType->elem->kind == MType::Void));
        if (nullCompare)
        {
            /* `s == null`：比的是地址（空串 / 空引用） */
            return simple_ast::Node::binary(op, LowerExpr(leftId), LowerExpr(rightId));
        }
        if (!IsStandardOpcode(leftType, "String") || !IsStandardOpcode(rightType, "String"))
        {
            Fail("字符串只能和字符串做 " + op + "（另一边先转成 std::lang::String）");
            return Bad();
        }
        if (op == "+")
        {
            /* 整条 `a + b + c` 链摊平成**一次** `__std_str_join`：只分配一个结果串。
             * 两两接的话中间串没人拥有（`("a"+"b") + "c"` 里前者是临时值），会漏。 */
            Memory::Vector<int> parts;
            CollectStringParts(leftId, &parts);
            CollectStringParts(rightId, &parts);
            Memory::Vector<Memory::String> names;
            Memory::Vector<simple_ast::NodePtr> values;
            for (std::size_t index = 0; index < parts.size(); ++index)
            {
                const std::shared_ptr<MType> partType = ExprType(parts[index]);
                if (!partType || !IsStandardOpcode(partType, "String"))
                {
                    Fail("字符串拼接里每一项都得是 std::lang::String：第 " + Memory::toString(index + 1u) + " 项是 " + (partType ? Describe(partType) : Memory::String("?")) + "（要拼数字先 `format` 或强转）");
                    return Bad();
                }
                names.push_back("__chali_s" + Memory::toString(index + 1u));
                /* 这一项本身是"造出来的串"（递归调用的返回值 / `format` / 切片）时，
                 * 先落进临时变量 —— 否则它没人释放（递归里会一直涨）。 */
                values.push_back(NeedsOwnedTemp(parts[index]) ? OwnedStringTemp(parts[index]) : LowerExpr(parts[index]));
            }
            return simple_ast::Node::call("__std_str_join", names, values);
        }
        if (op == "==" || op == "!=" || op == "<" || op == "<=" || op == ">" || op == ">=")
        {
            const simple_ast::NodePtr left = NeedsOwnedTemp(leftId) ? OwnedStringTemp(leftId) : LowerExpr(leftId);
            const simple_ast::NodePtr right = NeedsOwnedTemp(rightId) ? OwnedStringTemp(rightId) : LowerExpr(rightId);
            const simple_ast::NodePtr diff =
                simple_ast::Node::call("__std_str_cmp", { "__chali_sl", "__chali_sr" }, { left, right });
            return simple_ast::Node::binary(op, diff, simple_ast::Node::i(0));
        }
        Fail("字符串只支持 + 拼接与 == != < <= > >= 比较，不支持 " + op);
        return Bad();
    }
    /* ---- 大浮点数（`bfloat`）：十进制任意精度四则与比较。 ---- */
    if (IsStandardOpcode(leftType, "BigDecimal") || IsStandardOpcode(rightType, "BigDecimal"))
    {
        const auto toBigFloat = [&](int exprId) -> simple_ast::NodePtr {
            const std::shared_ptr<MType> type = ExprType(exprId);
            if (type && IsStandardOpcode(type, "BigDecimal")) return OwnedObjectTemp(exprId, type);
            const char *native = nullptr;
            if (type && IsStandardOpcode(type, "BigInteger"))
                native = "__std_bfloat_from_bint";
            else if (type && type->kind == MType::Double)
                native = "__std_bfloat_from_flt";
            else if (type && (type->kind == MType::Int || type->kind == MType::Bool))
                native = type->isUnsigned ? "__std_bfloat_from_uint" : "__std_bfloat_from_int";
            if (native != nullptr)
            {
                return OwnedObjectTempNode(simple_ast::Node::call(native, { "__chali_v" }, { LowerExpr(exprId) }),
                                           T(M_TypeBigFloat()));
            }
            Fail("大浮点数只能和整数 / 浮点 / 大整数 / 大浮点数做 " + op);
            return Bad();
        };
        const char *arith = nullptr;
        if (op == "+")
            arith = "__std_bfloat_add";
        else if (op == "-")
            arith = "__std_bfloat_sub";
        else if (op == "*")
            arith = "__std_bfloat_mul";
        else if (op == "/")
            arith = "__std_bfloat_div";
        if (arith != nullptr)
            return simple_ast::Node::call(arith, { "__chali_l", "__chali_r" },
                                          { toBigFloat(leftId), toBigFloat(rightId) });
        if (op == "==" || op == "!=" || op == "<" || op == "<=" || op == ">" || op == ">=")
        {
            const simple_ast::NodePtr diff = simple_ast::Node::call(
                "__std_bfloat_cmp", { "__chali_l", "__chali_r" }, { toBigFloat(leftId), toBigFloat(rightId) });
            return simple_ast::Node::binary(op, diff, simple_ast::Node::i(0));
        }
        Fail("大浮点数只支持 + - * / 与 == != < <= > >=，不支持 " + op);
        return Bad();
    }
    /* ---- 大整数（`bint`）：`+ - * / %` 与比较，全走运行期对象表里的 C++ 实现 ----
     *
     * 算术上 `bint` 和整数 / 浮点可以混着写：另一边先转成大整数（`__std_bint_from_int` /
     * `_from_flt`），结果还是 `bint`。比较走 `__std_bint_cmp`（-1 / 0 / 1）。 */
    if (IsStandardOpcode(leftType, "BigInteger") || IsStandardOpcode(rightType, "BigInteger"))
    {
        const bool nullCompare = (op == "==" || op == "!=") && ((rightType->kind == MType::Ptr && rightType->elem && rightType->elem->kind == MType::Void) || (leftType->kind == MType::Ptr && leftType->elem && leftType->elem->kind == MType::Void));
        if (nullCompare)
        {
            /* `a == null`：比的是句柄（对象表里没有 0 号对象，所以只有真没建过才是 null） */
            return simple_ast::Node::binary(op, LowerExpr(leftId), LowerExpr(rightId));
        }
        /**
         * 把一边转成 `bint`（已经是就原样）。
         */
        const auto toBigInt = [&](int exprId) -> simple_ast::NodePtr {
            const std::shared_ptr<MType> type = ExprType(exprId);
            /* 已经是 `bint` 的：如果是"当场造出来的"（`a + b` 这种），落进临时变量，
             * 免得算完就没人管（对象表会涨）。 */
            if (type && IsStandardOpcode(type, "BigInteger"))
            {
                return OwnedObjectTemp(exprId, type);
            }
            /* `int64` / `float64` → `bint` 的转换**每次都会造一个新对象**，落进临时变量
             * 让它随块释放 —— 不然 `r = r * i` 这种循环里每转一圈就漏一个。 */
            if (type && type->kind == MType::Double)
            {
                return OwnedObjectTempNode(
                    simple_ast::Node::call("__std_bint_from_flt", { "__chali_v" },
                                           { LowerExpr(exprId) }),
                    T(M_TypeBigInt()));
            }
            if (type && (type->kind == MType::Int || type->kind == MType::Bool))
            {
                return OwnedObjectTempNode(
                    simple_ast::Node::call(type->isUnsigned ? "__std_bint_from_uint" : "__std_bint_from_int", { "__chali_v" },
                                           { LowerExpr(exprId) }),
                    T(M_TypeBigInt()));
            }
            Fail("大整数只能和整数 / 浮点 / 大整数做 " + op + "：另一边是 " + (type ? Describe(type) : Memory::String("?")));
            return Bad();
        };
        const char *arith = nullptr;
        if (op == "+")
            arith = "__std_bint_add";
        else if (op == "-")
            arith = "__std_bint_sub";
        else if (op == "*")
            arith = "__std_bint_mul";
        else if (op == "/")
            arith = "__std_bint_div";
        else if (op == "%")
            arith = "__std_bint_mod";
        if (arith != nullptr)
        {
            return simple_ast::Node::call(arith, { "__chali_l", "__chali_r" },
                                          { toBigInt(leftId), toBigInt(rightId) });
        }
        if (op == "==" || op == "!=" || op == "<" || op == "<=" || op == ">" || op == ">=")
        {
            const simple_ast::NodePtr diff = simple_ast::Node::call(
                "__std_bint_cmp", { "__chali_l", "__chali_r" }, { toBigInt(leftId), toBigInt(rightId) });
            return simple_ast::Node::binary(op, diff, simple_ast::Node::i(0));
        }
        Fail("大整数只支持 + - * / % 与 == != < <= > >=，不支持 " + op);
        return Bad();
    }
    if ((leftType->kind == MType::Ptr || rightType->kind == MType::Ptr) && op != "==" && op != "!=")
        return Fail("引用不支持地址运算；请使用元素下标，地址仅由 std::lang::address 获取"), Bad();
    const MOperatorDecl *declaration = nullptr;
    MOperatorMatch match;
    /* 期望类型取这个节点自己钉住的类型 —— 它是推断期按上下文定的（见 InferExpression）。
     * 运算符接口允许「同一对操作数、不同返回类型」，那种情形要靠它才挑得出与推断一致的
     * 那份声明；没有上下文时这里为空，`FindOperator` 会报「多个实现同样匹配」。 */
    std::shared_ptr<MType> expected;
    if (exprId >= 0 && N(exprId).kind == MNode::Bin && N(exprId).type >= 0)
        expected = T(N(exprId).type);
    if (FindOperator(op, leftType, rightType, &declaration, &match, &expected))
    {
        /* 运算符和实例方法一样借用接收者；拥有的临时操作数须在完整表达式后清理。 */
        const simple_ast::NodePtr left = leftType->kind == MType::Struct ? OwnedAggregateTempFor(leftId, leftType) : LowerExpr(leftId);
        const simple_ast::NodePtr right = rightType->kind == MType::Struct ? OwnedAggregateTempFor(rightId, rightType) : LowerExpr(rightId);
        /* 命中的声明可能是"交换过"的（`a + b` 借用了 b 那侧的 `Add`）：接收者换一边。 */
        const bool receiverFirst = declaration->receiverLeft != match.swap;
        Memory::Vector<simple_ast::NodePtr> arguments{ left, right };
        if (!receiverFirst)
        {
            /* 换边会把实参顺序变成 {right, left}，而 VM 从左到右求值 —— 副作用顺序跟着
             * 反过来（`bump() + makeA()` 变成先求 `makeA()`）。把左操作数先钳进临时量、
             * 求值前缀挂在接收者实参上，位置就仍在右操作数之前，与源码一致。
             * 落地不了（对象类类型 / 没有语句槽）时保持原顺序。 */
            Memory::Vector<simple_ast::NodePtr> prologue;
            simple_ast::NodePtr held;
            if (HoldOperandBeforeReceiver(left, leftType, &prologue, &held))
            {
                arguments[0] = simple_ast::Node::sequence(prologue, right);
                arguments[1] = held;
            }
            else
                arguments = Memory::Vector<simple_ast::NodePtr>{ right, left };
        }
        simple_ast::NodePtr call =
            simple_ast::Node::call(declaration->label, declaration->callNames, arguments);
        /* 对偶推导来的（`a >= b` 用 `!(a < b)`）：结果取反再交出去。 */
        if (match.negate) call = simple_ast::Node::unary("!", call);
        return call;
    }
    if (leftType->kind == MType::Host || rightType->kind == MType::Host)
    {
        if ((op == "==" || op == "!=") && SameType(leftType, rightType))
            return simple_ast::Node::binary(op, LowerExpr(leftId), LowerExpr(rightId));
        FailNoOverload(op, leftType, rightType);
        return Bad();
    }
    if (!g_error.empty()) return Bad(); /* FindOperator 报了"多个重载同样匹配" */
    if ((leftType && leftType->kind == MType::Struct) || (rightType && rightType->kind == MType::Struct))
    {
        FailNoOverload(op, leftType, rightType);
        return Bad();
    }
    if (op == "&&" || op == "||")
    {
        const simple_ast::NodePtr left = LowerExpr(leftId);
        Memory::Vector<simple_ast::NodePtr> rightFresh;
        Memory::Vector<simple_ast::NodePtr> *const savedSink = g_freshSink;
        if (savedSink != nullptr) g_freshSink = &rightFresh;
        simple_ast::NodePtr right = LowerExpr(rightId);
        g_freshSink = savedSink;
        if (savedSink != nullptr)
        {
            Memory::Vector<simple_ast::NodePtr> conditional;
            for (simple_ast::NodePtr &statement : rightFresh)
            {
                if (statement->kind == simple_ast::Node::Kind::Declare)
                    savedSink->push_back(std::move(statement));
                else
                    conditional.push_back(std::move(statement));
            }
            rightFresh = std::move(conditional);
        }
        if (!rightFresh.empty())
            right = simple_ast::Node::sequence(std::move(rightFresh), right);
        return simple_ast::Node::binary(op, left, right);
    }
    return simple_ast::Node::binary(op, LowerExpr(leftId), LowerExpr(rightId));
}

/**
 * @brief 强转：(T)e —— 数值互转、引用 → 地址、地址 → 引用都走它。
 * @param typeId 类型池中的类型编号。
 * @param valueId 表示输入值的节点或类型编号。
 * @retval 生成的低层语法节点。
 */
simple_ast::NodePtr LowerCast(int typeId, int valueId)
{
    const std::shared_ptr<MType> target = T(typeId);
    const std::shared_ptr<MType> source = target && target->kind == MType::Ptr ? RawExprType(valueId) : ExprType(valueId);
    if ((target->kind == MType::Ptr && source->kind == MType::Int) || (target->kind == MType::Int && (source->kind == MType::Ptr || source->kind == MType::Array)))
        return Fail("引用不能与整数地址互转；查看地址请使用 std::lang::address"), Bad();
    if (const simple_ast::NodePtr converted = LowerDeclaredConversion(valueId, target, true))
        return converted;
    const auto basicNumeric = [](const std::shared_ptr<MType> &type) {
        return type && (type->kind == MType::Bool || type->kind == MType::Char || type->kind == MType::Int || type->kind == MType::Double);
    };
    /*
     * 下面几条"对象类 → 标量"的转换（`(int64)大数` / `(float64)大数` …）只把句柄交给
     * 运行期转一下，结果里**没有**任何东西接管源对象的所有权。源是"当场造出来的"
     * （`(int64)std::lang::BigDecimal("42.9")`、`(int64)(a * b)`）时先落进一个归本块所有的
     * 临时变量，随块释放；否则对象表里每次都漏一个。借来的变量 / 标量源原样返回。
     */
    /* `(int64)大整数` / `(float64)大整数`：走对象表里的转换（超 64 位按低 64 位截断，
     * 和 C 的整数转换一个脾气）。 */
    if (source && IsStandardOpcode(source, "BigInteger") && target)
    {
        if (target->kind == MType::Int || target->kind == MType::Bool)
        {
            const simple_ast::NodePtr value = simple_ast::Node::call(
                "__std_bint_to_int", { "__chali_v" }, { OwnedObjectTemp(valueId, source) });
            if (target->kind == MType::Bool)
                return simple_ast::Node::binary("!=", value, simple_ast::Node::i(0));
            if (target->bits == 64 && !target->isUnsigned) return value;
            return simple_ast::Node::cast(value, LowerType(target, "cast"));
        }
        if (target->kind == MType::Double)
        {
            return simple_ast::Node::call("__std_bint_to_flt", { "__chali_v" },
                                          { OwnedObjectTemp(valueId, source) });
        }
    }
    if (source && IsStandardOpcode(source, "BigDecimal") && target)
    {
        if (target->kind == MType::Int || target->kind == MType::Bool)
        {
            const simple_ast::NodePtr value = simple_ast::Node::call(
                "__std_bfloat_to_int", { "__chali_v" }, { OwnedObjectTemp(valueId, source) });
            if (target->kind == MType::Bool)
                return simple_ast::Node::binary("!=", value, simple_ast::Node::i(0));
            if (target->bits == 64 && !target->isUnsigned) return value;
            return simple_ast::Node::cast(value, LowerType(target, "cast"));
        }
        if (target->kind == MType::Double)
            return simple_ast::Node::call("__std_bfloat_to_flt", { "__chali_v" },
                                          { OwnedObjectTemp(valueId, source) });
    }
    /* 显式引用转换以目标类型的 mut/const 限定为准，不修改共享类型定义。 */
    /* **接口引用不能硬转**（两个方向都不行）：它是「地址 + 类型号」的胖引用，
     * 比裸地址多一个类型号 —— 拿裸地址凑出来的胖引用分派不了（类型号是假的），
     * 反过来把胖引用当裸地址用会读到"句柄"而不是对象的地址。两边都指向 obj：
     *
     *   `o.set(具体对象);`  →  `Shape mut& q = o.get();`   （obj.get 会按类型号核对）
     */
    if (IsInterfacePointer(target) && !(source && IsInterfacePointer(source)))
    {
        Fail("不能把 " + Describe(source) + " 强转成接口引用 " + Describe(target) + "：接口引用是「地址 + 类型号」的胖引用，凑不出来。要给对象一个接口视图就写 "
                                                                                    "`o.set(对象); "
             + Describe(target) + " q = o.get();`");
        return Bad();
    }
    if (source && IsInterfacePointer(source) && target && target->kind == MType::Ptr && !IsInterfacePointer(target))
    {
        Fail("不能把接口引用 " + Describe(source) + " 强转成 " + Describe(target) + "：它带着类型号，要取回具体类型得走 obj（`o.get()` 会按类型号核对）");
        return Bad();
    }
    if (!IsObjGetCall(valueId) && basicNumeric(target) && basicNumeric(source))
    {
        const simple_ast::NodePtr value = LowerExpr(valueId);
        if (target->kind == MType::Bool)
        {
            const simple_ast::NodePtr zero = source->kind == MType::Double ? simple_ast::Node::f(0.0) : simple_ast::Node::i(0);
            return simple_ast::Node::binary("!=", value, zero);
        }
        return simple_ast::Node::cast(value, LowerType(target, "cast"));
    }
    const bool arrayPointer = target && source && target->kind == MType::Ptr && source->kind == MType::Array && SameType(target->elem, source->elem);
    const bool structPointer = target && source && target->kind == MType::Ptr && source->kind == MType::Struct && SameType(target->elem, source);
    const bool pointerInteger = target && source && ((target->kind == MType::Ptr && source->kind == MType::Int) || (target->kind == MType::Int && source->kind == MType::Ptr) || (target->kind == MType::Int && source->kind == MType::Array) || (target->kind == MType::Ptr && source->kind == MType::Ptr));
    if (!arrayPointer && !structPointer && !pointerInteger && !SameType(target, source) && !IsObjGetCall(valueId))
    {
        Fail("不支持从 " + Describe(source) + " 强转成 " + Describe(target));
        return Bad();
    }
    /* `(int)o.get()`：强转这里也要当"上下文" —— 否则 `o.get()` 不知道自己该取成什么。 */
    const std::shared_ptr<MType> saved = g_expectedObjGet;
    g_expectedObjGet = target;
    const simple_ast::NodePtr node = target && target->kind == MType::Ptr ? LowerRawExpr(valueId) : LowerExpr(valueId);
    g_expectedObjGet = saved;
    return simple_ast::Node::cast(node, LowerType(target, "cast"));
}

/**
 * @brief 检查条件表达式是否具有布尔类型。
 * @param valueId 表示输入值的节点或类型编号。
 * @param where 用于错误定位的源码节点。
 * @retval 满足检查条件或操作完成时为 true，检查失败或无法完成时为 false。
 */
bool CheckBoolCondition(int valueId, const Memory::String &where)
{
    const std::shared_ptr<MType> type = ExprType(valueId);
    if (type && type->kind == MType::Bool) return true;
    return Fail(where + "必须是 bool，现在给的是 " + Describe(type) + "（数值条件请显式写成 `(bool)值` 或比较表达式）");
}

/**
 * @brief 检查左值及其来源能否修改，拒绝字面量和只读引用的写入。
 * @param targetId 赋值、递增或修改方法使用的目标表达式编号。
 * @return 来源可写时返回 true；只读来源返回 false 并登记编译诊断。
 * @note 只读来源的成员和元素也只读；独立复制得到的变量沿用变量自身的可变性。
 */
bool CheckWritable(int targetId)
{
    if (IsTupleElementAccess(targetId))
        return Fail(CHALI_TEXT("元组元素是只读的；请构造新元组后给整个变量赋值"));
    const MNode &target = N(targetId);
    for (int cursor = targetId; cursor >= 0;)
    {
        const auto &part = N(cursor);
        if (part.kind == MNode::Str)
            return Fail("字符串字面量只读，不能修改；请先复制到 String 变量");
        if ((part.kind == MNode::Member || part.kind == MNode::Arrow || part.kind == MNode::Index || (part.kind == MNode::Un && part.s == "*")) && !part.kids.empty())
        {
            const auto base = RawExprType(part.kids[0]);
            if (base->kind == MType::Ptr && base->readOnly)
                return Fail(CHALI_TEXT("只读引用（`T const&`）只能读，不能写入元素或成员"));
            cursor = part.kids[0];
        }
        else
            break;
    }
    if (target.kind == MNode::Arrow)
    {
        /* `p->字段 = v`：p 是可写引用就能写，是只读引用（`T*`）就不能。 */
        const std::shared_ptr<MType> base = RawExprType(target.kids[0]);
        if (base->kind != MType::Ptr || base->readOnly)
            return Fail(CHALI_TEXT("只读引用（`T const&`）只能读，不能写字段（要写就用可写引用 `T mut&`）"));
        return true;
    }
    if (target.kind == MNode::Index || target.kind == MNode::Member || target.kind == MNode::Un)
    {
        const std::shared_ptr<MType> base = RawExprType(target.kids[0]);
        if (base->kind == MType::Ptr && base->readOnly)
            return Fail(CHALI_TEXT("只读引用（`T const&`）只能读，不能通过引用写"
                                   "（要写就用可写引用 `T mut&`）"));
    }
    return true;
}

/**
 * @brief 检查修改方法是否能写入接收者及其来源。
 * @param receiverId 方法接收者节点。
 * @return 只读引用或只读来源返回 false 并报告错误。
 */
Memory::String ConstRootOf(int targetId);
/**
 * @brief 检查方法接收者是否允许可变访问。
 * @param receiverId 接收者表达式在节点池中的编号。
 * @retval 满足检查条件或操作完成时为 true，检查失败或无法完成时为 false。
 */
bool CheckMutableReceiver(int receiverId)
{
    const auto root = ConstRootOf(receiverId);
    if (!root.empty()) return Fail("不可变变量（const）不能调用修改方法：" + root);
    const auto type = RawExprType(receiverId);
    if (type->kind == MType::Ptr && type->readOnly)
        return Fail(CHALI_TEXT("只读引用（`T const&`）只能读，不能写入元素或成员"));
    return CheckWritable(receiverId);
}

/**
 * @brief 这个左值的"根"是不是 `const` 变量。
 *
 * `const Point p;` 之后 `p` 本身不能赋值，`p.x = 1`、`p[i] = 1` 也不行 ——
 * 对象整个是只读的。
 * @param targetId 左值节点。
 * @return 根是 const 变量返回它的名字，否则空串。
 */
Memory::String ConstRootOf(int targetId)
{
    int current = targetId;
    while (current >= 0)
    {
        const MNode &node = N(current);
        if ((node.kind == MNode::Member || node.kind == MNode::Arrow) && node.s == "opcode" && !node.kids.empty() && FindOpcodeDeclaration(ExprType(node.kids[0])))
            return "opcode";
        if (node.kind == MNode::Var)
        {
            const MVar *found = FindNodeVar(node);
            return (found != nullptr && found->isConst) ? node.s : Memory::String();
        }
        if (node.kind == MNode::EnumCtor)
        {
            const MVar *found = FindQualifiedGlobal(node.s, node.s2);
            return (found != nullptr && found->isConst) ? node.s + "::" + node.s2 : Memory::String();
        }
        if ((node.kind == MNode::Index || node.kind == MNode::Member || node.kind == MNode::Arrow || node.kind == MNode::Un) && !node.kids.empty())
        {
            current = node.kids[0];
            continue;
        }
        return Memory::String();
    }
    return Memory::String();
}

/**
 * @brief 判断读路径是否经过只读引用或不可变变量，用于方法和 foreach 来源传播。
 */
bool IsReadOnlyAccess(int id)
{
    if (IsTupleElementAccess(id) || !ConstRootOf(id).empty()) return true;
    while (id >= 0)
    {
        const auto type = RawExprType(id);
        if (type->kind == MType::Ptr && type->readOnly) return true;
        const MNode &node = N(id);
        if (node.kind == MNode::Cast && type->kind == MType::Ptr && !type->readOnly) return false;
        if ((node.kind == MNode::Member || node.kind == MNode::Arrow || node.kind == MNode::Index || node.kind == MNode::Cast || node.kind == MNode::Un) && !node.kids.empty())
            id = node.kids[0];
        else
            break;
    }
    return false;
}

/**
 * @brief 判断表达式是否以 self、接收者字段或其局部引用别名为根。
 */
bool ReceiverRooted(int id, const Memory::Set<Memory::String> &roots)
{
    if (id < 0) return false;
    const MNode &node = N(id);
    if (node.kind == MNode::Var) return roots.count(node.s) != 0u || (roots.count("self") && node.s.rfind("__chali_defer_receiver_", 0) == 0);
    if ((node.kind == MNode::Member || node.kind == MNode::Arrow || node.kind == MNode::Index || node.kind == MNode::Cast || node.kind == MNode::Un) && !node.kids.empty())
        return ReceiverRooted(node.kids[0], roots);
    return false;
}

bool MethodMutatesReceiver(const Memory::String &owner, const MMethod &method,
                           Memory::Set<Memory::String> *visiting);
/**
 * @brief 从接收者与字段声明取得转换源类型，避免依赖当前降低函数的局部符号表。
 */
std::shared_ptr<MType> ReceiverConversionType(int id, const Memory::String &owner,
                                              const Memory::Map<Memory::String, std::shared_ptr<MType>> &locals)
{
    if (id < 0) return nullptr;
    const MNode &node = N(id);
    if (node.kind == MNode::Var)
    {
        const auto local = locals.find(node.s);
        if (local != locals.end()) return local->second;
        if (node.s == "self") return NamedType(owner);
        const auto field = FindField(owner, node.s);
        return field == nullptr ? nullptr : field->type;
    }
    if ((node.kind == MNode::Member || node.kind == MNode::Arrow) && !node.kids.empty())
    {
        auto base = ReceiverConversionType(node.kids[0], owner, locals);
        if (base && base->kind == MType::Ptr) base = base->elem;
        const auto field = base ? FindField(StructNameOf(base), node.s) : nullptr;
        return field == nullptr ? nullptr : field->type;
    }
    if (node.kind == MNode::Cast && node.type >= 0) return T(node.type);
    if (node.kind == MNode::Un && !node.kids.empty())
        return ReceiverConversionType(node.kids[0], owner, locals);
    if (node.kind == MNode::Index && !node.kids.empty())
    {
        auto base = ReceiverConversionType(node.kids[0], owner, locals);
        if (base && base->kind == MType::Ptr) base = base->elem;
        return base ? base->elem : nullptr;
    }
    return nullptr;
}
/**
 * @brief 将方法体内隐藏在转换语法中的方法调用计入接收者可变性。
 */
bool ReceiverConversionMutates(int valueId, const std::shared_ptr<MType> &target,
                               bool allowExplicit, const Memory::String &owner,
                               const Memory::Set<Memory::String> &roots,
                               const Memory::Map<Memory::String, std::shared_ptr<MType>> &locals,
                               Memory::Set<Memory::String> *visiting)
{
    if (!target || !ReceiverRooted(valueId, roots)) return false;
    auto source = ReceiverConversionType(valueId, owner, locals);
    if (source && source->kind == MType::Ptr) source = source->elem;
    if (!source || SameType(source, target)) return false;
    const auto conversion = FindConversion(source, target, allowExplicit);
    if (conversion == nullptr) return false;
    const auto sourceOwner = StructNameOf(source);
    const auto definition = FindStruct(sourceOwner);
    if (definition != nullptr)
        for (const auto &method : definition->methods)
            if (sourceOwner + "." + method.name == conversion->label && MethodMutatesReceiver(sourceOwner, method, visiting)) return true;
    return false;
}
/**
 * @brief 扫描写入、修改方法与引用别名，递归跟踪接收者上的方法调用。
 */
bool BodyMutatesReceiver(int id, const Memory::String &owner, Memory::Set<Memory::String> &roots,
                         Memory::Set<Memory::String> *visiting,
                         Memory::Map<Memory::String, std::shared_ptr<MType>> &locals,
                         const std::shared_ptr<MType> &returnType)
{
    if (id < 0) return false;
    const MNode node = N(id);
    if (node.kind == MNode::Defer && !node.kids.empty())
    {
        const auto &callback = N(node.kids[0]);
        for (const auto &declaration : g_functionDecls)
            if (declaration.source == callback.s && declaration.module == callback.module)
                return BodyMutatesReceiver(declaration.bodyId, owner, roots, visiting, locals, returnType);
    }
    if (node.kind == MNode::Block)
    {
        auto nested = roots;
        auto nestedLocals = locals;
        for (int child : N(static_cast<int>(node.i)).kids)
            if (BodyMutatesReceiver(child, owner, nested, visiting, nestedLocals, returnType)) return true;
        return false;
    }
    if (node.kind == MNode::Assign && !node.kids.empty() && ReceiverRooted(node.kids[0], roots)) return true;
    if (node.kind == MNode::Foreach && node.kids.size() > 1u)
    {
        auto nested = roots;
        auto nestedLocals = locals;
        nested.erase(node.s);
        if (ReceiverRooted(node.kids[0], roots)) nested.insert(node.s);
        return BodyMutatesReceiver(node.kids[1], owner, nested, visiting, nestedLocals, returnType);
    }
    if (!node.kids.empty() && ((node.kind == MNode::Cast && node.type >= 0 && ReceiverConversionMutates(node.kids[0], T(node.type), true, owner, roots, locals, visiting)) || (node.kind == MNode::Declare && node.type >= 0 && ReceiverConversionMutates(node.kids[0], T(node.type), false, owner, roots, locals, visiting)) || (node.kind == MNode::Return && ReceiverConversionMutates(node.kids[0], returnType, false, owner, roots, locals, visiting)))) return true;
    if (node.kind == MNode::Assign && node.kids.size() > 1u && ReceiverConversionMutates(node.kids[1], ReceiverConversionType(node.kids[0], owner, locals), false, owner, roots, locals, visiting)) return true;
    if (node.kind == MNode::Declare && !node.s.empty() && node.s != "local")
    {
        const bool alias = node.type >= 0 && T(node.type)->kind == MType::Ptr && !node.kids.empty() && ReceiverRooted(node.kids[0], roots);
        if (node.type >= 0)
            locals[node.s] = T(node.type);
        else
            locals.erase(node.s);
        roots.erase(node.s);
        if (alias) roots.insert(node.s);
    }
    if (node.kind == MNode::Call && node.kids.size() > 1u)
    {
        const MNode &callee = N(node.kids[0]);
        const auto args = N(node.kids[1]).kids;
        if (StandardCallName(callee) == "__move" && !args.empty() && ReceiverRooted(args[0], roots)) return true;
        if ((callee.kind == MNode::Member || callee.kind == MNode::Arrow) && !callee.kids.empty() && ReceiverRooted(callee.kids[0], roots))
        {
            static const Memory::Set<Memory::String> mutators{
                "push", "insert", "pop", "remove", "clear", "add", "put", "set",
                "setPrecision", "drop", "close"
            };
            if (mutators.count(callee.s)) return true;
            // 根接收者上的用户方法沿调用图检查；字段接收者的方法按字段类型检查。
            Memory::String targetOwner = owner;
            const MNode &receiver = N(callee.kids[0]);
            if (receiver.kind == MNode::Member || receiver.kind == MNode::Arrow)
            {
                const MField *field = FindField(owner, receiver.s);
                if (field != nullptr) targetOwner = StructNameOf(field->type);
            }
            else if (receiver.kind == MNode::Var && receiver.s != "self")
            {
                const MField *field = FindField(owner, receiver.s);
                if (field != nullptr) targetOwner = StructNameOf(field->type);
            }
            const MStructDef *definition = FindStruct(targetOwner);
            if (definition != nullptr)
                for (const MMethod &called : definition->methods)
                {
                    if (called.superseded || called.isStatic || called.source != callee.s) continue;
                    if (MethodMutatesReceiver(targetOwner, called, visiting)) return true;
                    for (std::size_t i = 0; i < args.size() && i < called.params.size(); ++i)
                        if (ReceiverConversionMutates(args[i], called.params[i].type, false,
                                                      owner, roots, locals, visiting)) return true;
                }
        }
        // 传给可写引用形参的字段 / self 别名可以被被调者修改。
        if (callee.kind == MNode::Var || callee.kind == MNode::EnumCtor)
        {
            const auto *overloads = FindFunctionOverloads(callee.kind == MNode::Var ? callee.module : callee.s,
                                                          callee.kind == MNode::Var ? callee.s : callee.s2);
            if (overloads != nullptr)
                for (const auto &label : *overloads)
                {
                    const auto found = g_funcs.find(label);
                    if (found == g_funcs.end()) continue;
                    for (std::size_t i = 0; i < args.size() && i < found->second.params.size(); ++i)
                    {
                        const auto type = found->second.params[i].type;
                        if (type->kind == MType::Ptr && !type->readOnly && ReceiverRooted(args[i], roots)) return true;
                        if (ReceiverConversionMutates(args[i], type, false, owner, roots, locals, visiting)) return true;
                    }
                }
        }
    }
    for (int child : node.kids)
        if (BodyMutatesReceiver(child, owner, roots, visiting, locals, returnType)) return true;
    return false;
}
/**
 * @brief 推导用户方法是否修改接收者；递归调用用访问集合避免无限展开。
 */
bool MethodMutatesReceiver(const Memory::String &owner, const MMethod &method,
                           Memory::Set<Memory::String> *visiting)
{
    if (method.source == "drop") return true;  // 销毁本身需要可写接收者，即使方法体为空。
    if (method.result && method.result->kind == MType::Ptr && !method.result->readOnly) return true;
    const auto label = owner + "." + method.name;
    if (visiting->count(label)) return false;
    visiting->insert(label);
    Memory::Set<Memory::String> roots{ "self" };
    const MStructDef *definition = FindStruct(owner);
    if (definition != nullptr)
        for (const auto &field : definition->fields) roots.insert(field.name);
    Memory::Map<Memory::String, std::shared_ptr<MType>> locals;
    for (const auto &param : method.params)
    {
        roots.erase(param.source);
        locals[param.source] = param.type;
    }
    const bool result = BodyMutatesReceiver(method.bodyId, owner, roots, visiting, locals, method.result);
    visiting->erase(label);
    return result;
}
/**
 * @brief 只读结构体允许查询方法，拒绝修改接收者或导出可写引用的方法。
 */
/**
 * @brief 判断是否为核心 Option/Result 的隐藏 Any 接收者。
 * @param owner 所属类型。
 * @return 是否需要入口处约束隐藏借用。
 */
bool IsCoreObjectReceiver(const Memory::String &owner)
{
    const auto instance = chali_generic::session.made.find(owner);
    return instance != chali_generic::session.made.end() && instance->second.generic->module == "std::lang" && (instance->second.generic->name == "Option" || instance->second.generic->name == "Result") && HasOpaqueObjectBorrow(NamedType(owner));
}

/**
 * @brief 检查方法接收者类型及借用要求。
 * @param receiverId 接收者表达式在节点池中的编号。
 * @param label 诊断或跳转使用的标签。
 * @retval 满足检查条件或操作完成时为 true，检查失败或无法完成时为 false。
 */
bool CheckMethodReceiver(int receiverId, const Memory::String &label)
{
    const auto owner = StructNameOf(ExprType(receiverId));
    if (IsCoreObjectReceiver(owner)) RecordObjectArgumentBorrow(receiverId, NamedType(owner));
    if (!IsReadOnlyAccess(receiverId)) return true;
    const MStructDef *definition = FindStruct(owner);
    if (definition == nullptr) return true;
    for (const MMethod &method : definition->methods)
    {
        if (owner + "." + method.name != label || method.superseded) continue;
        Memory::Set<Memory::String> visiting;
        if (MethodMutatesReceiver(owner, method, &visiting))
            return Fail(CHALI_TEXT("只读接收者不能调用修改方法：") + label);
    }
    return true;
}

simple_ast::NodePtr LowerObjSet(const simple_ast::NodePtr &receiver, int valueId,
                                const Memory::String &where);

/**
 * @brief 将定长数组的独立副本搬入现有存储，保留目标数组及内联字段的地址。
 * @param address 目标首元素地址，仅求值一次。
 * @param valueId 源表达式编号，用于识别可移交的临时数组。
 * @param value 源数组首元素地址表达式，仅求值一次。
 * @param type 目标定长数组类型，决定元素个数与步长。
 * @return 先物化新值、清理旧元素，再搬入新元素并归还临时长度头的语句块。
 * @note 自赋值先复制后覆盖；原生对象由复制入口克隆一次，搬入阶段不重复克隆。
 */
simple_ast::NodePtr ReplaceFixedArrayValue(const simple_ast::NodePtr &address, int valueId,
                                           const simple_ast::NodePtr &value, const std::shared_ptr<MType> &type)
{
    using simple_ast::Node;
    const int count = static_cast<int>(FixedArrayElementCount(type->dims));
    const auto destination = AllocateName("__chali_fixed_destination");
    const auto replacement = AllocateName("__chali_fixed_replacement");
    const auto index = AllocateName("__chali_fixed_index");
    const auto loop = NewBlockLabel("fixed_array_move");
    const auto done = NewBlockLabel("fixed_array_move_done");
    const auto stored = IsFreshValue(valueId) ? TakeExpressionTempValue(value, type) : CopyRuntimeArrayValue(value, type);
    Memory::Vector<simple_ast::NodePtr> out{
        Node::declare(destination, simple_ast::Type::i64()), Node::assign(destination, address),
        Node::declare(replacement, LowerType(type, replacement)), Node::assign(replacement, stored),
        Node::ifThenElse(Node::binary("!=", Node::loadInt(Node::binary("-", Node::var(replacement), Node::i(8))), Node::i(count)),
                         Node::exprStmt(Node::call("__std_panic", { "message" }, { Node::str("定长数组赋值的元素个数不匹配") })), nullptr)
    };
    if (const auto release = MakeArrayElementRelease(destination, type->elem, count)) out.push_back(release);
    out.push_back(Node::declare(index, simple_ast::Type::i64()));
    out.push_back(Node::assign(index, Node::i(0)));
    out.push_back(Node::label(loop));
    out.push_back(Node::ifThenElse(Node::binary(">=", Node::var(index), Node::i(count)), Node::jump(done), nullptr));
    const auto from = Node::offset(Node::var(replacement), Node::var(index), ElementBytes(type->elem));
    const auto into = Node::offset(Node::var(destination), Node::var(index), ElementBytes(type->elem));
    if (type->elem->kind == MType::Struct)
        out.push_back(MoveStructValue(into, from, type->elem));
    else
        out.push_back(Node::storeAt(into, Node::loadFrom(from, LowerType(type->elem, replacement)), LowerType(type->elem, destination)));
    out.push_back(Node::assign(index, Node::binary("+", Node::var(index), Node::i(1))));
    out.push_back(Node::jump(loop));
    out.push_back(Node::label(done));
    out.push_back(Node::freeMemory(Node::binary("-", Node::var(replacement), Node::i(8))));
    return Node::block(std::move(out));
}

/**
 * @brief 把值按**值语义**写进一个地址槽（数组元素 / 引用指向的元素 / 解引用）。
 *
 * 和结构体字段同一条规则：字符串与对象句柄**借来的先复制一份**（当场造的、字面量各按
 * 自己的规矩物化），覆盖前**还掉旧值** —— 元素和字段一样是值，不是引用。
 * 标量（含结构体以外的类型）原样写。
 * @param address 接收拥有值的存储槽地址。
 * @param valueId 右侧表达式编号，用于判断复制或移交。
 * @param value 已降级的右侧值表达式。
 * @param type 存储槽的静态类型及元素布局。
 * @return 先物化新值、释放旧值，再写入目标的语句块。
 * @note 数组引用的整体赋值逐元素复制或移交，并清理旧数组的元素和长度头。
 */
simple_ast::NodePtr StoreOwnedValueAt(const simple_ast::NodePtr &address, int valueId,
                                      const simple_ast::NodePtr &value,
                                      const std::shared_ptr<MType> &type)
{
    if (TypeNonCopyable(type) && valueId >= 0 && !IsFreshValue(valueId))
        return Fail("不可复制的值禁止复制到字段或数组元素（实现 Closeable 或声明了 #NonCopyable），请显式 move"), Bad();
    if (IsRuntimeArray(type))
    {
        using simple_ast::Node;
        const auto destination = AllocateName("__chali_array_destination");
        const auto replacement = AllocateName("__chali_array_replacement");
        const auto previous = AllocateName("__chali_array_previous");
        const auto stored = IsFreshValue(valueId) ? TakeExpressionTempValue(value, type) : CopyRuntimeArrayValue(value, type);
        Memory::Vector<simple_ast::NodePtr> out{
            Node::declare(destination, simple_ast::Type::i64()),
            Node::assign(destination, address),
            Node::declare(replacement, LowerType(type, replacement)),
            Node::assign(replacement, stored),
            Node::declare(previous, LowerType(type, previous)),
            Node::assign(previous, Node::loadInt(Node::var(destination)))
        };
        AppendOwnedDrop(previous, type, &out);
        out.push_back(Node::storeAt(Node::var(destination), Node::var(replacement), simple_ast::Type::i64()));
        return Node::block(out);
    }
    if (!type || (!IsStandardOpcode(type, "String") && !IsObjectLike(type) && !OpcodeNonCopyable(type)))
        return simple_ast::Node::storeAt(address, value, LowerType(type, "assignment"));
    simple_ast::NodePtr stored = (IsFreshValue(valueId) || IsImplicitConversionValue(valueId, type)) ? TakeExpressionTempValue(value, type) : value;
    if (IsStandardOpcode(type, "String"))
    {
        if (valueId >= 0 && N(valueId).kind == MNode::Str)
            stored = simple_ast::Node::call("__std_str_lit", { "__chali_addr" }, { value });
        else if (!(IsFreshValue(valueId) || IsImplicitConversionValue(valueId, type)))
            stored = simple_ast::Node::call(ObjectCopyFunction(type), { "value" }, { value });
    }
    else if (!(IsFreshValue(valueId) || IsImplicitConversionValue(valueId, type)))
    {
        stored = simple_ast::Node::call(ObjectCopyFunction(type), { "__chali_v" }, { value });
    }
    /* 地址先落临时槽：`a[i]` / `p.next` 这类左值只许求值一次。 */
    const Memory::String slotValue = AllocateName("__chali_elem_value");
    const Memory::String slotAddress = AllocateName("__chali_elem");
    Memory::Vector<simple_ast::NodePtr> out;
    /*
     * `Node::index(base, i)` 是**右值**形式（算地址之后还补一次 load）；要当 store 的
     * 目标就得换成纯地址 `offset(base, i, 8)`（非结构体元素的槽位一律 8 字节）。
     * 结构体元素本来就返回 `offset`，不用动。
     */
    simple_ast::NodePtr target = address;
    if (target != nullptr && target->kind == simple_ast::Node::Kind::Index && target->kids.size() >= 2u)
        target = simple_ast::Node::offset(target->kids[0], target->kids[1], 8);
    out.push_back(simple_ast::Node::declare(slotValue, simple_ast::Type::i64()));
    out.push_back(simple_ast::Node::assign(slotValue, stored));
    out.push_back(simple_ast::Node::declare(slotAddress, simple_ast::Type::i64()));
    out.push_back(simple_ast::Node::assign(slotAddress, target));
    const auto previous = simple_ast::Node::loadInt(simple_ast::Node::var(slotAddress));
    const auto *opcode = FindOpcodeDefinition(type);
    out.push_back(simple_ast::Node::ifThenElse(simple_ast::Node::binary("!=", previous, simple_ast::Node::i(0)),
                                               opcode ? simple_ast::Node::exprStmt(simple_ast::Node::call(opcode->destroy, { "value" }, { previous })) : simple_ast::Node::release(previous), nullptr));
    out.push_back(simple_ast::Node::storeAt(simple_ast::Node::var(slotAddress),
                                            simple_ast::Node::var(slotValue),
                                            LowerType(type, "assignment")));
    return simple_ast::Node::block(out);
}

simple_ast::NodePtr DefaultInit(const Memory::String &emitted,
                                const std::shared_ptr<MType> &type,
                                bool initContainers = false, bool localStorage = true);

/**
 * @brief 把"**调用点拥有**的当场构造聚合"（元组字面量 / 函数返回的枚举或结构体）落进一个
 *        完整表达式拥有的临时变量：独立存储，表达式结束后析构并清空内部槽。
 *
 * 用途和方法调用里的对象实参一样：被调方只是**借用**接收者 / 实参。遇到
 * `stream.read().expect("…")` 这种"临时聚合上直接调方法"，临时那份叶子没有别的主人 ——
 * 不落地就漏。落地时给临时**自己的存储**（`DefaultInit`）再按位搬进来，免得多个同类型
 * 临时共用数据区槽位互相盖掉。
 */
simple_ast::NodePtr OwnedAggregateTemp(const simple_ast::NodePtr &value,
                                       const std::shared_ptr<MType> &type)
{
    if (g_freshSink == nullptr || !type || type->kind != MType::Struct) return value;
    const Memory::String owner = StructNameOf(type);
    if (owner.empty()) return value;
    const Memory::String temp = AllocateName("__chali_tmp_agg");
    g_freshSink->push_back(simple_ast::Node::declare(temp, LowerType(type, temp)));
    if (const simple_ast::NodePtr init = DefaultInit(temp, type))
        g_freshSink->push_back(init);
    MarkDestructor(temp, type);
    NoteExpressionTemp(temp);
    /* 表达式到达此处时才求值，保持实参从左到右的副作用顺序。 */
    return simple_ast::Node::sequence(
        { MoveStructValue(simple_ast::Node::var(temp), value, type) }, simple_ast::Node::var(temp));
}

/**
 * @brief 临时聚合（当场造的）才落地，借来的不动。
 */
simple_ast::NodePtr OwnedAggregateTempFor(int exprId, const std::shared_ptr<MType> &type)
{
    const simple_ast::NodePtr value = LowerExpr(exprId);
    if (!IsOwnedAggregateTemporary(exprId)) return value;
    return OwnedAggregateTemp(value, type);
}

/**
 * @brief 这个类型的值能不能安全地先落进一个临时量（换边保序用）。
 *
 * 结构体走 {@link OwnedAggregateTemp} 那套（独立存储 + 登记析构）；普通值直接赋值就行，
 * 本来就没有需要转移的所有权。**对象类类型**（`String` / `Any` / 容器 / `bignum`）各自
 * 的所有权规则不一样，这里不动 —— 断言写错会变成泄漏或重复释放，宁可不保序。
 */
bool CanHoldOperand(const std::shared_ptr<MType> &type)
{
    if (!type) return false;
    if (type->kind == MType::Struct) return true;
    if (IsStandardOpcode(type, "String") || IsStandardOpcode(type, "BigInteger") || IsStandardOpcode(type, "BigDecimal") || IsObjectLike(type))
        return false;
    return true; /* 整数 / 浮点 / 布尔 / 枚举 / 引用：赋值即可 */
}

/**
 * @brief 在修改接收者前保存操作数的求值结果。
 * @param value 待转换、保存或参与计算的值。
 * @param type 类型描述。
 * @param prologue 接收前置求值语句的列表。
 * @param held 接收保存后的临时操作数。
 * @retval 满足检查条件或操作完成时为 true，检查失败或无法完成时为 false。
 */
bool HoldOperandBeforeReceiver(const simple_ast::NodePtr &value,
                               const std::shared_ptr<MType> &type,
                               Memory::Vector<simple_ast::NodePtr> *prologue,
                               simple_ast::NodePtr *held)
{
    if (g_freshSink == nullptr || !CanHoldOperand(type)) return false;
    const Memory::String temp = AllocateName("__chali_operand_hold");
    g_freshSink->push_back(simple_ast::Node::declare(temp, LowerType(type, temp)));
    if (type->kind == MType::Struct)
    {
        if (const simple_ast::NodePtr init = DefaultInit(temp, type)) g_freshSink->push_back(init);
        MarkDestructor(temp, type);
        NoteExpressionTemp(temp);
        prologue->push_back(MoveStructValue(simple_ast::Node::var(temp), value, type));
    }
    else
        prologue->push_back(simple_ast::Node::assign(temp, value));
    *held = simple_ast::Node::var(temp);
    return true;
}

/**
 * @brief 通过已校验的转换方法调用，保持接收者只读性与临时源对象所有权。
 * @param valueId 源表达式。
 * @param target 目标类型。
 * @param allowExplicit 是否允许显式接口，显式实现优先于隐式实现。
 * @return 无转换返回空节点；存在转换时返回方法调用或诊断节点。
 */
simple_ast::NodePtr LowerDeclaredConversion(int valueId,
                                            const std::shared_ptr<MType> &target,
                                            bool allowExplicit)
{
    const auto source = ExprType(valueId);
    const auto conversion = FindConversion(source, target, allowExplicit);
    if (conversion == nullptr)
        return source && target && source->kind != MType::Void && IsStandardOpcode(target, "String") ? LowerDefaultValueText(valueId) : nullptr;
    if (!CheckMethodReceiver(valueId, conversion->label)) return Bad();
    const auto receiver = source->kind == MType::Struct ? OwnedAggregateTempFor(valueId, source) : (IsStandardOpcode(source, "String") && N(valueId).kind == MNode::Str) ? OwnedObjectTempNode(MaterializeStringValue(valueId, source), source) :
                                                                                               (IsObjectLike(source) || IsStandardOpcode(source, "String"))              ? OwnedObjectTemp(valueId, source) :
                                                                                                                                                                           LowerExpr(valueId);
    return simple_ast::Node::call(conversion->label, conversion->callNames, { receiver });
}

/**
 * @brief 替换借用聚合中的值，先复制右侧再释放旧叶子，保证自赋值与引用别名安全。
 */
simple_ast::NodePtr ReplaceBorrowedStruct(const simple_ast::NodePtr &address, int valueId,
                                          const std::shared_ptr<MType> &type)
{
    const auto temp = AllocateName("__chali_borrowed_value");
    const auto destination = AllocateName("__chali_borrowed_target");
    Memory::Vector<simple_ast::NodePtr> out{
        simple_ast::Node::declare(temp, LowerType(type, temp)), DefaultInit(temp, type),
        StoreStructValue(simple_ast::Node::var(temp), valueId, LowerValueForType(valueId, type), type),
        simple_ast::Node::declare(destination, simple_ast::Type::i64()),
        simple_ast::Node::assign(destination, address)
    };
    if (const auto release = MakeAggregateRelease(type->name, simple_ast::Node::var(destination)))
        out.push_back(release);
    out.push_back(MoveStructValue(simple_ast::Node::var(destination), simple_ast::Node::var(temp), type));
    return simple_ast::Node::block(out);
}

/**
 * @brief 实参是临时资源值时由调用点持有，被调函数复制自己的按值形参。
 * @param id 实参表达式。
 * @param type 目标形参类型。
 * @param checkObjectBorrow 是否要求隐藏 Any 借用满足普通调用边界；标准游标转送另行传播来源。
 * @return 已检查并登记临时资源的实参值。
 */
simple_ast::NodePtr LowerArgumentValue(int id, const std::shared_ptr<MType> &type,
                                       bool checkObjectBorrow = true)
{
    if (checkObjectBorrow) RecordObjectArgumentBorrow(id, type);
    if (OpcodeNonCopyable(type) && !IsFreshValue(id))
        return Fail("#NonCopyable opcode 禁止按值复制实参，请显式移动或使用引用"), Bad();
    if (OpcodeNonCopyable(type)) return TakeExpressionTempValue(LowerValueForType(id, type), type);
    if (HasNonCopyableValue(type) && !IsOwnedAggregateTemporary(id))
        return Fail("不可复制的值不能按值复制传参（实现 Closeable 或声明了 #NonCopyable），请显式 move 或改用引用"), Bad();
    if (IsImplicitConversionValue(id, type))
    {
        const auto converted = LowerValueForType(id, type);
        if (type->kind == MType::Struct) return OwnedAggregateTemp(converted, type);
        return OwnedObjectTempNode(converted, type);
    }
    if (type && IsStandardOpcode(type, "String") && SameType(ExprType(id), type) && NeedsOwnedTemp(id))
        return OwnedStringTemp(id);
    if (type && IsObjectLike(type) && SameType(ExprType(id), type))
    {
        // 临时函数值也须使用当前形参上下文，避免继承外层初始化或返回的函数签名。
        const auto value = LowerValueForType(id, type);
        return IsFreshValue(id) ? OwnedObjectTempNode(value, type) : value;
    }
    if (type && type->kind == MType::Struct && SameType(ExprType(id), type) && IsOwnedAggregateTemporary(id))
        return OwnedAggregateTempFor(id, type);
    return LowerValueForType(id, type);
}

/**
 * 赋值：目标是变量就直接 `:=`；是下标 / 成员 / `*p` 就走 simple_ast 的 `(store, 地址, 值, _)`。
 */
/**
 * @brief 按赋值目标的引用、聚合和资源类型生成写入操作。
 * @param targetId 赋值目标节点。
 * @param valueId 右侧表达式节点。
 * @return 写入语句；失败时登记诊断。
 */
bool g_namedConstructorBody = false;  ///< 仅构造体允许用完整值替换隐式 self。

/**
 * @brief < 仅构造体允许用完整值替换隐式 self。
 * @param targetId 赋值目标在节点池中的编号。
 * @param valueId 表示输入值的节点或类型编号。
 * @retval 生成的低层语法节点。
 */
simple_ast::NodePtr LowerAssignImpl(int targetId, int valueId)
{
    if (const int member = ExplicitReceiverField(targetId); member >= 0) return LowerAssignImpl(member, valueId);
    if (g_namedConstructorBody && !g_owner.empty() && N(targetId).kind == MNode::Var && N(targetId).s == "self")
        return ReplaceBorrowedStruct(simple_ast::Node::var("self"), valueId, NamedType(g_owner));
    InferExpression(valueId, ExprType(targetId));
    const MNode &target = N(targetId);
    const auto referenceType = RawExprType(targetId);
    if (referenceType && referenceType->kind == MType::Ptr && !IsInterfacePointer(referenceType))
    {
        const MNode &right = N(valueId);
        const bool binding = (right.kind == MNode::Un && right.s == "&") || right.kind == MNode::NewPtr || right.kind == MNode::Null;
        if (!binding)
        {
            if (!CheckWritable(targetId)) return Bad();
            if (referenceType->readOnly) return Fail(CHALI_TEXT("只读引用（`T const&`）不能写入元素")), Bad();
            const auto value = LowerValueForType(valueId, referenceType->elem);
            if (referenceType->elem->kind == MType::Array && !IsRuntimeArray(referenceType->elem))
                return ReplaceFixedArrayValue(LowerExpr(targetId), valueId, value, referenceType->elem);
            if (referenceType->elem->kind == MType::Struct)
            {
                // 新值先物化为独立聚合，确保自赋值与借用源不会被旧值释放影响。
                const auto temp = AllocateName("__chali_reference_value");
                const auto address = AllocateName("__chali_reference_target");
                Memory::Vector<simple_ast::NodePtr> out{
                    simple_ast::Node::declare(temp, LowerType(referenceType->elem, temp)),
                    DefaultInit(temp, referenceType->elem),
                    StoreStructValue(simple_ast::Node::var(temp), valueId, value, referenceType->elem),
                    simple_ast::Node::declare(address, simple_ast::Type::i64()),
                    simple_ast::Node::assign(address, LowerRawExpr(targetId))
                };
                if (const auto release = MakeAggregateRelease(referenceType->elem->name,
                                                              simple_ast::Node::var(address))) out.push_back(release);
                out.push_back(CopyStructValue(simple_ast::Node::var(address),
                                              simple_ast::Node::var(temp), referenceType->elem));
                return simple_ast::Node::block(out);
            }
            return StoreOwnedValueAt(LowerRawExpr(targetId), valueId, value, referenceType->elem);
        }
        if (target.kind == MNode::Member || target.kind == MNode::Arrow)
        {
            const MField *field = nullptr;
            Memory::String owner;
            const auto address = LowerFieldAddress(targetId, &field, &owner);
            if (!CheckWritable(targetId)) return Bad();
            return simple_ast::Node::storeAt(address, LowerValueForType(valueId, referenceType), LowerType(referenceType, "reference binding"));
        }
    }
    const auto targetType = ExprType(targetId);
    if (targetType && targetType->kind == MType::Array && !IsRuntimeArray(targetType))
    {
        if (!CheckWritable(targetId)) return Bad();
        return ReplaceFixedArrayValue(LowerExpr(targetId), valueId, LowerValueForType(valueId, targetType), targetType);
    }
    if (target.kind == MNode::EnumCtor && target.type2 != 1)
    {
        if (MVar *global = FindQualifiedGlobal(target.s, target.s2); global != nullptr)
        {
            if (global->isConst)
                return Fail("不可变变量（const）不能再赋值：" + target.s + "::" + target.s2), Bad();
            if (global->elementReference)
                return StoreOwnedValueAt(simple_ast::Node::var(global->emitted), valueId,
                                         LowerValueForType(valueId, global->type), global->type);
            return simple_ast::Node::assign(global->emitted,
                                            LowerValueForType(valueId, global->type));
        }
        const MStaticField *field = FindStaticField(target.s, target.s2);
        if (field == nullptr)
            return Fail("struct " + target.s + " 没有 static 字段 " + target.s2), Bad();
        if (field->isConst)
            return Fail(CHALI_TEXT("只读常数不能赋值：") + target.s + "::" + target.s2), Bad();
        return simple_ast::Node::assign(
            field->emitted, LowerValueForType(valueId, field->type));
    }
    if (target.kind == MNode::Var)
    {
        MVar *found = FindNodeVar(target);
        if (found != nullptr)
        {
            if (found->isConst)
            {
                Fail("不可变变量（const）不能再赋值：" + target.s);
                return Bad();
            }
            if (!found->elementList.empty())
            {
                Memory::Vector<simple_ast::NodePtr> post;
                const auto stored = ContainerStoreValue(valueId, found->type, &post);
                const auto call = simple_ast::Node::call("__std_vec_set",
                                                         { "__chali_v", "__chali_i", "__chali_x", "__chali_t", "__chali_o" },
                                                         { simple_ast::Node::var(found->elementList), simple_ast::Node::var(found->elementIndex),
                                                           stored, simple_ast::Node::i(IsStandardOpcode(found->type, "String") ? 1 : 0), ContainerStorageFlag(found->type) });
                post.insert(post.begin(), simple_ast::Node::exprStmt(call));
                return simple_ast::Node::block(post);
            }
            if (found->elementReference)
            {
                if (found->type->kind == MType::Obj && ExprType(valueId)->kind != MType::Obj && !IsImplicitConversionValue(valueId, found->type))
                {
                    const auto temp = AllocateName("__chali_boxed_assignment");
                    return simple_ast::Node::block({ simple_ast::Node::declare(temp, LowerType(found->type, temp)),
                                                     simple_ast::Node::assign(temp, simple_ast::Node::call("__std_obj_new", {}, {})),
                                                     simple_ast::Node::exprStmt(LowerObjSet(simple_ast::Node::var(temp), valueId, "Any 赋值")),
                                                     simple_ast::Node::release(simple_ast::Node::loadInt(simple_ast::Node::var(found->emitted))),
                                                     simple_ast::Node::storeAt(simple_ast::Node::var(found->emitted), simple_ast::Node::var(temp), LowerType(found->type, temp)) });
                }
                const auto value = LowerValueForType(valueId, found->type);
                if (found->type->kind == MType::Struct)
                    return ReplaceBorrowedStruct(simple_ast::Node::var(found->emitted), valueId, found->type);
                return StoreOwnedValueAt(simple_ast::Node::var(found->emitted), valueId, value,
                                         found->type);
            }
            if (found->type && found->type->kind == MType::Obj && ExprType(valueId)->kind != MType::Obj && !IsImplicitConversionValue(valueId, found->type))
            {
                const Memory::String temp = AllocateName("__chali_box");
                Memory::Vector<simple_ast::NodePtr> out;
                out.push_back(simple_ast::Node::declare(temp, LowerType(found->type, temp)));
                out.push_back(DefaultInit(temp, found->type));
                out.push_back(simple_ast::Node::exprStmt(
                    LowerObjSet(simple_ast::Node::var(temp), valueId, "obj 赋值")));
                if (IsOwnedNow(found->emitted)) out.push_back(MakeDrop(found->emitted, false));
                out.push_back(simple_ast::Node::assign(found->emitted,
                                                       simple_ast::Node::var(temp)));
                MarkOwned(found->emitted, false);
                return simple_ast::Node::block(out);
            }
            /* 结构体是**值语义**：`a = b;` 深拷贝（改 a 不会动到 b）。 */
            if (found->type && found->type->kind == MType::Struct && (IsObjGetCall(valueId) || SameStruct(found->type, ExprType(valueId)) || IsImplicitConversionValue(valueId, found->type)))
            {
                /* `a = a;`：自赋值什么也不做，别把旧字段析构完再把同一个值拷回来。 */
                if (N(valueId).kind == MNode::Var)
                {
                    const MVar *source = FindNodeVar(N(valueId));
                    if (source != nullptr && source->emitted == found->emitted)
                        return simple_ast::Node::block({});
                }
                const bool global = IsGlobalName(found->emitted);
                if (!global && !HasDestructorNow(found->emitted))
                    return StoreStructValue(simple_ast::Node::var(found->emitted), valueId,
                                            LowerValueForType(valueId, found->type), found->type);
                MOwnedScope::Destructor previous{ found->emitted, found->type->name,
                                                  StructHasInterface(found->type->name, "Drop"),
                                                  StructHasInterface(found->type->name, "Closeable"),
                                                  StructHasOwnedLeaves(found->type->name) };
                if (global)
                {
                    previous.drop = previous.close = previous.aggregate = false;
                    for (const auto &item : g_globalCleanupItems)
                        if (item.name == found->emitted)
                        {
                            previous.drop = item.drop;
                            previous.close = item.close;
                            previous.aggregate = item.aggregate;
                        }
                }
                /*
                 * 旧值挂着析构（实现 Drop / Closeable）时，赋值 = 先析构旧值，再搬新值。
                 * 顺序：**先把右边算进一个临时结构体**（`h = h.clone()` 里 clone 要读 h 的字段），
                 * 再跑旧值的析构，最后把新值搬进 h —— 否则"多余的副本"（旧的 h）就漏了。
                 * 临时结构体不登记析构：它只是搬运工，新值的所有权已经交给 h。
                 */
                Memory::Vector<simple_ast::NodePtr> out;
                const Memory::String temp = AllocateName("__chali_new_st");
                out.push_back(simple_ast::Node::declare(temp, LowerType(found->type, temp)));
                /* 临时结构体也要有自己的存储：`__chali_copy_struct` 会往它的槽里写，
                 * 少了这一步 `temp` 就是个没初始化的野引用，直接踩坏堆。 */
                if (const simple_ast::NodePtr init = DefaultInit(temp, found->type))
                    out.push_back(init);
                out.push_back(StoreStructValue(simple_ast::Node::var(temp), valueId,
                                               LowerValueForType(valueId, found->type), found->type));
                if (previous.drop || previous.close || previous.aggregate)
                    out.push_back(MakeDestructorCall(previous));
                out.push_back(CopyStructValue(simple_ast::Node::var(found->emitted),
                                              simple_ast::Node::var(temp), found->type));
                // 搬运工只在本次赋值内存在，分支未执行时不留下块级清理义务。
                out.push_back(MakeDrop(temp, false));
                UnmarkOwned(temp);
                if (global)
                    for (auto &item : g_globalCleanupItems)
                        if (item.name == found->emitted)
                        {
                            item.drop = StructHasInterface(found->type->name, "Drop");
                            item.close = StructHasInterface(found->type->name, "Closeable");
                            item.aggregate = StructHasOwnedLeaves(found->type->name);
                        }
                return simple_ast::Node::block(out);
            }
            /* 函数指针赋初值：`fp = add;` —— 有重载时按 fp 的类型挑；
             * `p = new;` —— 元素类型也从 p 的类型来。 */
            const std::shared_ptr<MType> savedFuncType = g_expectedFuncType;
            const std::shared_ptr<MType> savedNewType = g_expectedNewType;
            g_expectedFuncType = (found->type && found->type->kind == MType::Func) ? found->type : nullptr;
            g_expectedNewType = found->type;
            Memory::Vector<simple_ast::NodePtr> fresh;
            g_freshSink = &fresh;
            /* 对象类类型（`bint` / 容器）：值语义 —— 借来的值先复制一份。 */
            const simple_ast::NodePtr value = IsObjectLike(found->type) ? MaterializeObjectValue(valueId, found->type) : MaterializeStringValue(valueId, found->type);
            g_freshSink = nullptr;
            g_expectedFuncType = savedFuncType;
            g_expectedNewType = savedNewType;
            /* 顺序：**先算新值 → 再还旧值 → 再写**。
             * `s = s + "x"` 里右边要读旧值，先还就把读的东西 free 了；
             * `s = <递归调用的结果>` 里右边那些临时串也得先算完再还。 */
            Memory::Vector<simple_ast::NodePtr> out(fresh.begin(), fresh.end());
            /* 全局的**旧值**也要在覆盖前还掉（`s = s + "x"` 每轮都产生新串，不还就漏），
             * 但它不参加函数 / 出块的自动释放（`MarkOwned` 会挡掉，见 `IsGlobalName`）。 */
            const bool global = IsGlobalName(found->emitted);
            const bool owned =
                IsOwnedKind(found->type) && (global || IsOwnedNow(found->emitted));
            const simple_ast::NodePtr pending =
                owned ? DeferNewValue(value, found->type, &out) : value;
            if (owned)
            {
                AppendOwnedDrop(found->emitted, found->type, &out);
            }
            out.push_back(simple_ast::Node::assign(found->emitted, pending));
            /* 字符串走 `MaterializeStringValue`：借来的值已经复制过，赋值后归变量所有。 */
            if (IsOwnedKind(found->type) && (IsFreshValue(valueId) || IsStandardOpcode(found->type, "String")) && !global)
            {
                if (IsRuntimeArray(found->type))
                    MarkOwnedArray(found->emitted, found->type);
                else
                    MarkOwned(found->emitted);
            }
            return simple_ast::Node::block(out);
        }
        Fail("给没声明的变量赋值：" + target.s);
        return Bad();
    }
    if (target.kind == MNode::Index || target.kind == MNode::Member || target.kind == MNode::Arrow || target.kind == MNode::Un)
    {
        const Memory::String frozen = ConstRootOf(targetId);
        if (!frozen.empty())
        {
            Fail("不可变变量（const）的内容不能改：" + frozen);
            return Bad();
        }
        if (!CheckWritable(targetId)) return Bad();
        if (target.kind == MNode::Index && target.s != "__chali_buffer_index" && FindIndex(ExprType(target.kids[0]), ExprType(target.kids[1])) != nullptr)
        {
            Fail("Index<R,T> 只重载读取；索引写入需要显式方法");
            return Bad();
        }
        /* `v[i] = x;` / `m[k] = v;`：容器是运行期对象，写元素 / 键值都走原生函数。 */
        if (target.kind == MNode::Index && target.s != "__chali_buffer_index" && IsContainer(ExprType(target.kids[0])))
        {
            const int receiverId = target.kids[0];
            const int positionId = target.kids[1];
            const std::shared_ptr<MType> container = ExprType(receiverId);
            const simple_ast::NodePtr receiver = LowerExpr(receiverId);
            if (container->name == "set")
            {
                Fail("set 不能按下标写：用 `s.add(x)` / `s.remove(x)`");
                return Bad();
            }
            if (container->name == "vec")
            {
                if (!CheckContainerIndex(positionId, "vec 下标") || !CheckContainerArgument(container->elem, valueId, "vec 元素", true))
                    return Bad();
                const bool elementText = IsStandardOpcode(container->elem, "String");
                Memory::Vector<simple_ast::NodePtr> post;
                const simple_ast::NodePtr stored =
                    ContainerStoreValue(valueId, container->elem, &post);
                const simple_ast::NodePtr call = simple_ast::Node::call(
                    "__std_vec_set", { "__chali_v", "__chali_i", "__chali_x", "__chali_t", "__chali_o" },
                    { receiver, LowerExpr(positionId),
                      stored,
                      simple_ast::Node::i(elementText ? 1 : 0),
                      ContainerStorageFlag(container->elem) });
                if (post.empty()) return simple_ast::Node::exprStmt(call);
                Memory::Vector<simple_ast::NodePtr> block{ simple_ast::Node::exprStmt(call) };
                for (const simple_ast::NodePtr &statement : post) block.push_back(statement);
                return simple_ast::Node::block(block);
            }
            if (!CheckContainerArgument(container->elem, positionId, "map 键", true) || !CheckContainerArgument(container->elem2, valueId, "map 值", true))
                return Bad();
            const bool keyText = IsStandardOpcode(container->elem, "String");
            const bool valueText = IsStandardOpcode(container->elem2, "String");
            Memory::Vector<simple_ast::NodePtr> post;
            const simple_ast::NodePtr key =
                ContainerStoreValue(positionId, container->elem, &post);
            const simple_ast::NodePtr stored =
                ContainerStoreValue(valueId, container->elem2, &post);
            const simple_ast::NodePtr call = simple_ast::Node::call(
                "__std_map_put",
                { "__chali_v", "__chali_k", "__chali_t", "__chali_o", "__chali_x", "__chali_t", "__chali_o" },
                { receiver, key,
                  simple_ast::Node::i(keyText ? 1 : 0),
                  ContainerStorageFlag(container->elem),
                  stored,
                  simple_ast::Node::i(valueText ? 1 : 0),
                  ContainerStorageFlag(container->elem2) });
            if (post.empty()) return simple_ast::Node::exprStmt(call);
            Memory::Vector<simple_ast::NodePtr> block{ simple_ast::Node::exprStmt(call) };
            for (const simple_ast::NodePtr &statement : post) block.push_back(statement);
            return simple_ast::Node::block(block);
        }
        /* `s[i] = 字符;`：字符串按字节写 —— 交给运行时（越界会报错，和数组下标同一个
         * 脾气）。`s[i]` 当右值时是"取第 i 个字节（& 255）"，当不了赋值目标，
         * 所以这里单独走一条：`__std_str_set(串, 下标, 字符码)`。 */
        if (target.kind == MNode::Index && target.s != "__chali_buffer_index" && IsStandardOpcode(ExprType(target.kids[0]), "String"))
        {
            const int baseId = target.kids[0];
            const int positionId = target.kids[1];
            if (!CheckContainerIndex(positionId, "下标")) return Bad();
            const std::shared_ptr<MType> codeType = ExprType(valueId);
            if (!codeType || codeType->kind != MType::Char)
            {
                Fail("std::lang::String 下标只能写 char（整数请显式强转）：现在写的是 " + Describe(codeType));
                return Bad();
            }
            return simple_ast::Node::exprStmt(simple_ast::Node::call(
                "__std_str_set", { "__chali_s", "__chali_i", "__chali_c" },
                { LowerExpr(baseId), LowerExpr(positionId), LowerExpr(valueId) }));
        }
        /* 结构体元素 / 字段同样是值语义：`arr[i] = b;` / `s.inner = b;` 深拷贝。 */
        const std::shared_ptr<MType> targetType = ExprType(targetId);
        if (targetType && targetType->kind == MType::Struct && (SameStruct(targetType, ExprType(valueId)) || IsImplicitConversionValue(valueId, targetType)))
        {
            /* 字段覆盖也须释放旧叶子；先复制右侧，兼容自赋值和引用别名。 */
            return ReplaceBorrowedStruct(LowerExpr(targetId), valueId, targetType);
        }
        /* `arr[i] = new;` / `s.f = new;`：元素 / 字段的类型就是 new 的元素类型。 */
        /*
         * 右边表达式里可能夹着"当场造出来的临时串"（切片 / 递归调用 / `format`）。
         * 它们要先落进**归本块所有**的临时变量，否则没人释放（`NeedsOwnedTemp` 只认得
         * 有 sink 的位置）。这一步以前漏了：`s.f = a + b[0:2];` 里那个切片就会漏。
         */
        Memory::Vector<simple_ast::NodePtr> pending;
        Memory::Vector<simple_ast::NodePtr> *const savedSink = g_freshSink;
        g_freshSink = &pending;
        const std::shared_ptr<MType> savedNewType = g_expectedNewType;
        g_expectedNewType = targetType;
        const simple_ast::NodePtr value = LowerValueForType(valueId, targetType);
        g_expectedNewType = savedNewType;
        g_freshSink = savedSink;
        /* 成员字段：地址自己按槽位算（`LowerExpr(Member)` 取的是**值**，当不了左值），
         * 再按字段类型写——浮点字段就按 IEEE-754 写那 8 个字节。 */
        if (target.kind == MNode::Member || target.kind == MNode::Arrow)
        {
            const MField *declared = nullptr;
            Memory::String owner;
            const simple_ast::NodePtr address = LowerFieldAddress(targetId, &declared, &owner);
            if (!address || !declared) return Bad();
            const simple_ast::TypePtr fieldType =
                LowerType(declared->type, owner + "." + declared->name);
            if (!LeafOwnedByAggregate(declared->type))
            {
                pending.push_back(simple_ast::Node::storeAt(address, value, fieldType));
                return simple_ast::Node::block(pending);
            }
            Memory::Vector<simple_ast::NodePtr> out(pending.begin(), pending.end());
            out.push_back(StoreOwnedValueAt(address, valueId, value, declared->type));
            return simple_ast::Node::block(out);
        }
        /* 下标 / 解引用：simple_ast 对 Assign 的左值只算地址、不补那次 load。
         * 字符串 / 对象元素同样按值语义写（借来的复制一份、覆盖前还旧值）。 */
        if (LeafOwnedByAggregate(targetType))
        {
            Memory::Vector<simple_ast::NodePtr> out(pending.begin(), pending.end());
            /* 这里的左值要的是**地址**：`LowerExpr(a[i])` 会把元素读出来（load），
             * 拿去当 store 的目标就写到 0 地址去了。 */
            out.push_back(StoreOwnedValueAt(LowerRawExpr(targetId), valueId, value, targetType));
            return simple_ast::Node::block(out);
        }
        if (targetType && targetType->kind != MType::Array && target.kind == MNode::Un && target.s == "*")
        {
            pending.push_back(StoreOwnedValueAt(LowerRawExpr(target.kids[0]), valueId, value, targetType));
            return simple_ast::Node::block(std::move(pending));
        }
        if (IsByteStorageType(targetType) && target.kind == MNode::Index)
        {
            pending.push_back(StoreOwnedValueAt(LowerAddressOf(targetId), valueId, value, targetType));
            return simple_ast::Node::block(std::move(pending));
        }
        return simple_ast::Node::assign(LowerExpr(targetId), value);
    }
    Fail("赋值目标只能是变量、下标、成员或 `*p`");
    return Bad();
}

/**
 * @brief 为赋值表达式中的新值安排块级生命周期，覆盖引用及普通变量路径。
 * @param targetId 赋值目标节点。
 * @param valueId 右侧表达式节点。
 * @return 含临时值前置操作的赋值语句。
 */
simple_ast::NodePtr LowerAssign(int targetId, int valueId)
{
    InferExpression(valueId, ExprType(targetId));
    CaptureBorrowExpression(targetId, RawExprType(targetId));
    CaptureBorrowExpression(valueId, nullptr);
    RecordBorrowStore(targetId, valueId);
    Memory::Vector<simple_ast::NodePtr> fresh;
    auto *const saved = g_freshSink;
    g_freshSink = &fresh;
    const auto assignment = LowerAssignImpl(targetId, valueId);
    g_freshSink = saved;
    if (fresh.empty()) return assignment;
    fresh.push_back(assignment);
    return simple_ast::Node::block(fresh);
}

/**
 * @brief 在声明作用域初始化闭包成员，隔离构造调用者的局部同名绑定。
 * @param initializer 默认闭包表达式。
 * @return 拥有独立环境的闭包值。
 */
simple_ast::NodePtr LowerMemberClosureInitializer(int initializer)
{
    struct Scope
    {
        decltype(g_scope) saved;
        Memory::String owner;
        /**
         * @brief 暂存调用者的符号作用域，只保留模块全局名称。
         */
        Scope() : saved(std::move(g_scope)), owner(std::move(g_owner))
        {
            g_scope.push_back(saved.front());
            g_owner.clear();
        }
        /**
         * @brief 在正常返回或语义重放异常时恢复调用者的符号作用域。
         */
        ~Scope()
        {
            g_scope = std::move(saved);
            g_owner = std::move(owner);
        }
    } scope;
    return TakeExpressionTempValue(LowerExpr(initializer), ExprType(initializer));
}

/**
 * @brief 初始化默认值中实际存在的容器字段，跳过枚举未激活的载荷。
 * @param address 已清零的聚合地址。
 * @param owner 聚合类型名称。
 * @param out 接收初始化语句。
 */
void InitializeDefaultContainers(const simple_ast::NodePtr &address, const Memory::String &owner,
                                 Memory::Vector<simple_ast::NodePtr> *out)
{
    const auto definition = FindStruct(owner);
    if (!definition) return;
    const auto enumeration = g_enums.find(owner);
    for (const auto &field : definition->fields)
    {
        if (enumeration != g_enums.end())
        {
            bool active = false;
            for (const auto &variant : enumeration->second.variants)
                if (variant.tag == 0)
                    for (const auto &payload : variant.payloads)
                        if (payload.storageName == field.name) active = true;
            if (!active) continue;
        }
        const auto target = simple_ast::Node::offset(address, simple_ast::Node::i(field.slot), 8);
        if (field.initializer >= 0)
            out->push_back(simple_ast::Node::storeAt(target, LowerMemberClosureInitializer(field.initializer), simple_ast::Type::i64()));
        else if (IsContainer(field.type))
            out->push_back(simple_ast::Node::storeAt(
                target, NewContainerValue(field.type), LowerType(field.type, owner + "." + field.name)));
        else if (field.type->kind == MType::Struct)
            InitializeDefaultContainers(target, field.type->name, out);
        else if (field.type->kind == MType::Array && !IsRuntimeArray(field.type))
        {
            const int count = static_cast<int>(FixedArrayElementCount(field.type->dims));
            out->push_back(simple_ast::Node::storeAt(
                simple_ast::Node::offset(target, simple_ast::Node::i(-1), 8), simple_ast::Node::i(count), simple_ast::Type::i64()));
            const auto element = field.type->elem;
            for (int index = 0; index < count; ++index)
            {
                const auto item = simple_ast::Node::offset(target, simple_ast::Node::i(index), ElementBytes(element));
                if (IsContainer(element))
                    out->push_back(simple_ast::Node::storeAt(item, NewContainerValue(element), LowerType(element, field.name)));
                else if (element->kind == MType::Struct)
                    InitializeDefaultContainers(item, element->name, out);
            }
        }
    }
}

/**
 * @brief 在数组现有载荷中初始化聚合生命周期及其默认容器，不另建共享元素存储。
 * @param address 数组首元素地址。
 * @param element 元素类型，非聚合且非容器时不生成操作。
 * @param length 已求值的元素个数。
 * @param out 接收初始化循环的语句序列。
 */
void InitializeArrayValues(const simple_ast::NodePtr &address, const std::shared_ptr<MType> &element,
                           const simple_ast::NodePtr &length, Memory::Vector<simple_ast::NodePtr> *out)
{
    if (element->kind != MType::Struct && !IsContainer(element)) return;
    using simple_ast::Node;
    const auto index = AllocateName("__chali_array_init_index");
    const auto loop = NewBlockLabel("array_init");
    const auto done = NewBlockLabel("array_init_done");
    out->push_back(Node::declare(index, simple_ast::Type::i64()));
    out->push_back(Node::assign(index, Node::i(0)));
    out->push_back(Node::label(loop));
    out->push_back(Node::ifThenElse(Node::binary(">=", Node::var(index), length), Node::jump(done), nullptr));
    const auto item = Node::offset(address, Node::var(index), ElementBytes(element));
    if (element->kind == MType::Struct)
    {
        out->push_back(BeginAggregateLifetime(item, StructStride(element)));
        InitializeDefaultContainers(item, element->name, out);
    }
    else
        out->push_back(Node::storeAt(item, NewContainerValue(element), LowerType(element, "array element")));
    out->push_back(Node::assign(index, Node::binary("+", Node::var(index), Node::i(1))));
    out->push_back(Node::jump(loop));
    out->push_back(Node::label(done));
}

/**
 * @brief 准备独立的局部存储或静态暂存，真正默认构造时初始化活动容器字段。
 * @param emitted 目标变量名称。
 * @param type 声明类型。
 * @param initContainers 是否执行默认容器字段构造；复制和移动缓冲区保持 false。
 * @param localStorage 是否为局部值分配独立存储；返回暂存为 false。
 * @return 默认初始化语句。
 */
simple_ast::NodePtr DefaultInit(const Memory::String &emitted, const std::shared_ptr<MType> &type,
                                bool initContainers, bool localStorage)
{
    using simple_ast::Node;
    /* 槽里填的"零"要按槽位类型给：浮点槽给 `0.0`（带小数点），整型给 `0` ——
     * 数据区正文里 `0` 和 `0.0` 写进 .data.bin 的解释方式不同（整数 / IEEE-754）。 */
    const auto zeroOf = [](const std::shared_ptr<MType> &slot) {
        return IsFloatType(slot) ? simple_ast::Node::f(0.0) : simple_ast::Node::i(0);
    };
    if (type->kind == MType::Array)
    {
        const int count = static_cast<int>(FixedArrayElementCount(type->dims));
        const int stride = StructStride(type->elem);
        const int slots = (stride > 0) ? count * (stride / 8) : count;
        const int unit = IsByteStorageType(type->elem) ? 1 : 8;
        Memory::Vector<simple_ast::NodePtr> items;
        for (int index = 0; index < slots; ++index) items.push_back(zeroOf(type->elem));
        if (localStorage && !g_ownedScopes.empty() && !IsGlobalName(emitted))
        {
            g_usesNewArray = true;
            Memory::Vector<simple_ast::NodePtr> out{
                Node::assign(emitted, Node::call("__chali_new_array", { "__chali_len", "__chali_stride" },
                                                 { Node::i(count), Node::i(ElementBytes(type->elem)) }))
            };
            for (int index = 0; index < slots; ++index)
                out.push_back(Node::storeAt(Node::offset(Node::var(emitted), Node::i(index), unit),
                                            items[index], unit == 1 ? LowerType(type->elem, emitted) : simple_ast::Type::i64()));
            if (initContainers) InitializeArrayValues(Node::var(emitted), type->elem, Node::i(count), &out);
            MarkOwnedArray(emitted, type);
            return Node::block(std::move(out));
        }
        Memory::Vector<simple_ast::NodePtr> out{ Node::assign(emitted, Node::arrayNew(LowerType(type, emitted), items)) };
        if (initContainers) InitializeArrayValues(Node::var(emitted), type->elem, Node::i(count), &out);
        return Node::block(std::move(out));
    }
    if (type->kind == MType::Struct)
    {
        const MStructDef *found = FindStruct(type->name);
        if (found == nullptr) return nullptr;
        /* 一个槽一个零：值字段摊开之后槽位比"声明里写的字段"多。 */
        Memory::Vector<simple_ast::NodePtr> values;
        for (const auto &leaf : found->leaves) values.push_back(zeroOf(leaf.second));
        Memory::Vector<simple_ast::NodePtr> out;
        // 局部聚合不能复用声明点的数据区模板：递归和重入调用必须各有存储。
        // 返回暂存仍使用数据区，调用点立即复制其值，不将局部块地址带出作用域。
        const bool independent = localStorage && !g_ownedScopes.empty() && !IsGlobalName(emitted);
        if (independent)
        {
            out.push_back(Node::assign(emitted, Node::alloc(Node::i(std::max(8, StructStride(type))))));
            // 窄数值仍占八字节槽，按完整块清零，不能留下上半槽的旧字节。
            out.push_back(Node::exprStmt(Node::call("__std_memset", { "__chali_addr", "__chali_val", "__chali_n" },
                                                    { Node::var(emitted), Node::i(0), Node::i(std::max(8, StructStride(type))) })));
            MarkOwned(emitted);
            g_parameterValueStorage.insert(emitted);
        }
        else
            out.push_back(Node::assign(emitted, Node::structNew(type->name, values)));
        out.push_back(BeginAggregateLifetime(Node::var(emitted), StructStride(type)));
        /* 容器字段不能停在 0 句柄上：默认构造时当场建一个空容器填进那个槽。
         * 否则 `struct Bag { std::lang::List<int64> items; }; Bag b; b.items.push(7);`
         * 会直到运行期才报"拿到的是 0"。只在**真正默认构造**时建（拷贝 / 搬运用的
         * 临时结构体传 false），免得刚建出来的容器马上被覆盖、白白漏一个。 */
        if (initContainers)
        {
            // 数据区模板可能由下一次调用复用；真正默认初始化须同时复位标量与标签。
            if (!independent)
                for (std::size_t slot = 0; slot < found->leaves.size(); ++slot)
                    out.push_back(Node::storeAt(Node::offset(Node::var(emitted), Node::i(slot), 8),
                                                zeroOf(found->leaves[slot].second), LowerType(found->leaves[slot].second, emitted)));
            InitializeDefaultContainers(Node::var(emitted), type->name, &out);
        }
        if (out.size() == 1u) return out[0];
        return Node::block(out);
    }
    if (IsStandardOpcode(type, "BigInteger"))
    {
        /* 声明出来就是一个 0（新对象）：后面直接 `a + b` 也不用先判断是不是空的。 */
        return Node::assign(emitted, Node::call(FindOpcodeDefinition(type) ? FindOpcodeDefinition(type)->create : Memory::String("__std_bint_from_int"), { "__chali_v" },
                                                { simple_ast::Node::i(0) }));
    }
    if (IsStandardOpcode(type, "BigDecimal"))
    {
        return Node::assign(emitted, Node::call(FindOpcodeDefinition(type) ? FindOpcodeDefinition(type)->create : Memory::String("__std_bfloat_from_int"), { "__chali_v" },
                                                { simple_ast::Node::i(0) }));
    }
    if (IsContainer(type))
    {
        /* 容器声明出来就是**建好的空容器**：`std::lang::List<int> v;` 之后直接 `v.push(1)`。 */
        return Node::assign(emitted, NewContainerValue(type));
    }
    if (type->kind == MType::Obj || IsInterfacePointer(type))
    {
        /* `obj b;` 声明出来是个**空格子**（类型号 0）；接口引用声明出来是**空胖引用** ——
         * 用的时候 `obj.get` 会报"还没 set 过"、接口分派会报"没指向任何对象"。 */
        return Node::assign(emitted, Node::call("__std_obj_new", {}, {}));
    }
    if (type->kind == MType::Double)
        return Node::assign(emitted, Node::f(0.0));
    if (type->kind == MType::Ptr || type->kind == MType::Func || type->kind == MType::Int || IsStandardOpcode(type, "String") || type->kind == MType::Opcode || IsResourceLike(type))
        return Node::assign(emitted, Node::i(0));
    return nullptr;
}

/**
 * @brief 分配并默认初始化单对象，旧堆块的字节不能被误当成拥有型字段再释放。
 * @param type 对象类型。
 * @return 独立对象地址；对象的释放仍由引用所有者负责。
 */
simple_ast::NodePtr AllocateDefaultObject(const std::shared_ptr<MType> &type)
{
    using simple_ast::Node;
    const auto address = AllocateName("__chali_new_object");
    const int bytes = ElementBytes(type);
    Memory::Vector<simple_ast::NodePtr> out{
        Node::declare(address, simple_ast::Type::i64()),
        Node::assign(address, Node::alloc(Node::i(bytes))),
        Node::exprStmt(Node::call("__std_memset", { "__chali_addr", "__chali_val", "__chali_n" },
                                  { Node::var(address), Node::i(0), Node::i(bytes) }))
    };
    if (type->kind == MType::Struct)
    {
        out.push_back(BeginAggregateLifetime(Node::var(address), bytes));
        InitializeDefaultContainers(Node::var(address), type->name, &out);
    }
    return Node::sequence(std::move(out), Node::var(address));
}

/**
 * @brief 在定义模块的全局作用域解析默认值，防止调用方局部变量遮蔽。
 */
struct MDefaultScope
{
    decltype(g_scope) saved;
    Memory::String module;
    Memory::String owner;
    /**
     * @brief 暂存调用方作用域。
     * @param expr 默认表达式。
     */
    explicit MDefaultScope(int expr) : saved(g_scope), module(g_currentModule), owner(g_owner)
    {
        for (std::size_t i = 1; i < g_scope.size(); ++i) g_scope[i].clear();
        if (g_scope.size() == 1) PushScope();
        g_currentModule = N(expr).module;
        g_owner.clear();
    }
    /**
     * @brief 恢复调用方作用域。
     */
    ~MDefaultScope()
    {
        g_scope = std::move(saved);
        g_currentModule = std::move(module);
        g_owner = std::move(owner);
    }
};
/**
 * @brief 降级省略实参的默认值。
 * @param param 形参及默认表达式。
 * @return 实参值。
 */
simple_ast::NodePtr LowerDefaultArgument(const MParam &param)
{
    static unsigned depth = 0;
    if (depth >= 64)
    {
        Fail("默认参数展开存在递归或超过限制");
        return Bad();
    }
    struct Guard
    {
        /**
         * @brief 增加表达式递归处理深度。
         */
        Guard() { ++depth; }
        /**
         * @brief 恢复表达式递归处理深度。
         */
        ~Guard() { --depth; }
    } guard;
    MDefaultScope scope(param.defaultExpr);
    return LowerArgumentValue(param.defaultExpr, param.type);
}

/**
 * @brief 打包变长尾参。
 * @param args 实参列表。
 * @param begin 尾参起点。
 * @param parameter 变长形参。
 * @return 列表值。
 */
simple_ast::NodePtr PackVariadicArguments(const Memory::Vector<int> &args, std::size_t begin,
                                          const MParam &parameter);

/**
 * @brief `Construct` 节点的声明初始化：`T __x = new(实参);`。
 *
 * 两条来源共用本函数，区别只在 `node.s`：
 *   * 用户语法 `T v = new(实参);` —— `node.s` 为空，按**实参类型**在
 *     `g_structs[T].methods` 里挑那条 `isConstructor && !isNamedConstructor`
 *     的重载，标签是 `T.<name>`（`name` 是修饰过的 `__ctor` 序列）；
 *   * `static T create(...)` 工厂内部展开 —— `node.s` 是那条工厂自己的修饰名，
 *     它对应的 `$body` 就是 `T.<名>$body`（返回 void、带隐式 self）。
 *     直接调它本身，**不再去别的构造函数里挑**：同一个 struct 里完全可以有
 *     `static T new()` 与 `static T new(A, B)` 两条工厂，按"参数个数匹配"会挑错人。
 *
 * 两条路都是「先建默认值，再在它上面跑构造体」。
 *
 * @param emitted 已经声明好的目标变量名。
 * @param type 目标类型。
 * @param initId 初始化式节点。
 * @param out 追加生成的语句。
 * @return 是不是本函数处理掉的（false = 交给别的初始化分支）。
 */
bool LowerConstructInit(const Memory::String &emitted, const std::shared_ptr<MType> &declared,
                        int initId, Memory::Vector<simple_ast::NodePtr> *out)
{
    const MNode &node = N(initId);
    if (node.kind != MNode::Construct) return false;
    /* `T mut& p = new(实参);` 的声明类型是引用，构造口径按被指向的结构体走。 */
    std::shared_ptr<MType> type = declared;
    if (type && type->kind == MType::Ptr && !IsInterfacePointer(type) && type->elem) type = type->elem;
    if (!type || type->kind != MType::Struct)
    {
        Fail("构造的目标类型必须是结构体，现在是 " + (type ? Describe(type) : Memory::String("<未定>")));
        return true;
    }
    const Memory::Vector<int> args =
        node.kids.empty() ? Memory::Vector<int>() : N(node.kids[0]).kids;

    Memory::String label;
    const MFuncSig *body = nullptr;
    if (!node.s.empty())
    {
        /* `static T new(形参)` 工厂自己的构造体：调用标签是 `<T>.<修饰名>$body`
         * （`Function::quadLabel()` = `owner + "." + name`，`owner` 由 addMethod 设成结构体名），
         * 形参与返回类型也登记在同一键上。 */
        const auto found = g_funcs.find(type->name + "." + node.s + "$body");
        if (found == g_funcs.end())
        {
            Fail("构造调用缺少要展开的构造函数：struct " + type->name);
            return true;
        }
        body = &found->second;
        label = type->name + "." + node.s + "$body";
    }
    else
    {
        /* 用户写的 `T v = new(实参);`：按实参个数挑一条构造函数。
         * 优先级：① 实构造函数（无名的 `T(args)`，体标签 `<T>.<修饰名>`，不带 `$body`）；
         *          ② 实在没有时，落到同名工厂 `create`（体标签 `<T>.<修饰名>$body`）。
         * 以前工厂叫 `new`，现在改名 `create`，`new T(args)` 这类糖要能找到它。 */
        const auto definition = g_structs.find(type->name);
        const MMethod *chosen = nullptr;
        bool named = false;
        /* 同档候选的体标签：多于一条且省略了默认实参时，不能静默挑第一条。 */
        Memory::Vector<Memory::String> matched;
        if (definition != g_structs.end())
        {
            for (const MMethod &candidate : definition->second.methods)
            {
                if (candidate.superseded || !candidate.isConstructor || candidate.isNamedConstructor) continue;
                if (!AcceptsArgumentCount(candidate.params, args.size())) continue;
                if (chosen == nullptr) chosen = &candidate;
                matched.push_back(type->name + "." + candidate.name);
            }
            if (matched.empty())
            {
                for (const MMethod &candidate : definition->second.methods)
                {
                    if (candidate.superseded || !candidate.isNamedConstructor) continue;
                    if (!AcceptsArgumentCount(candidate.params, args.size())) continue;
                    if (chosen == nullptr)
                    {
                        chosen = &candidate;
                        named = true;
                    }
                    matched.push_back(type->name + "." + candidate.name + "$body");
                }
            }
        }
        if (chosen == nullptr)
        {
            Memory::String shown;
            for (std::size_t index = 0; index < args.size(); ++index)
                shown += (index == 0 ? "" : ", ") + Describe(ExprType(args[index]));
            Fail("struct " + type->name + " 没有匹配的构造函数：(" + shown + ")");
            return true;
        }
        /* 多条候选都接受同一实参个数时，省略默认实参造成的歧义必须报错，
         * 文案与普通函数重载共用 `DefaultArgumentAmbiguity`。 */
        if (matched.size() > 1u)
        {
            const Memory::String ambiguity = DefaultArgumentAmbiguity(matched, args.size());
            if (!ambiguity.empty())
            {
                Fail("构造调用有多个重载同样匹配：" + JoinLabels(matched) + ambiguity);
                return true;
            }
        }
        label = type->name + "." + chosen->name + (named ? "$body" : "");
        const auto found = g_funcs.find(label);
        if (found == g_funcs.end())
        {
            Fail("构造函数体没有登记：struct " + type->name);
            return true;
        }
        body = &found->second;
    }

    if (const simple_ast::NodePtr init = DefaultInit(emitted, type, true)) out->push_back(init);
    const Memory::Vector<MParam> &params = body->params;
    Memory::Vector<Memory::String> names{ "self" };
    Memory::Vector<simple_ast::NodePtr> values{ simple_ast::Node::var(emitted) };
    const bool variadic = !params.empty() && params.back().variadic;
    const std::size_t fixed = params.size() - (variadic ? 1u : 0u);
    for (std::size_t index = 0; index < fixed; ++index)
    {
        names.push_back(params[index].emitted);
        values.push_back(index < args.size() ? LowerArgumentValue(args[index], params[index].type) : LowerDefaultArgument(params[index]));
    }
    if (variadic)
    {
        names.push_back(params.back().emitted);
        values.push_back(PackVariadicArguments(args, fixed, params.back()));
    }
    out->push_back(simple_ast::Node::exprStmt(simple_ast::Node::call(label, names, values)));
    return true;
}

#include "expression_effects.cpp"
#include "borrow_facts.cpp"
#include "borrow_flow.cpp"

/**
 * @brief 局部声明组：`int i = 0, m[3][4];` → 一条 `decl` + 初始化，逐个名字来。
 * @param id 待查找或操作的对象编号。
 * @retval 生成的低层语法节点。
 */
simple_ast::NodePtr LowerLocalDecl(int id)
{
    const MNode &group = N(id);
    Memory::Vector<simple_ast::NodePtr> out;
    for (const int declId : N(group.kids[0]).kids)
    {
        MFullExpressionTemps initializerTemps;
        Memory::Vector<simple_ast::NodePtr> initializerFresh;
        MExpressionFreshScope initializerSink(&initializerFresh);
        const auto initializerStart = out.size();
        if (group.returnExpression && g_returnType)
        {
            const auto typeId = M_AddType(g_returnType);
            g_nodes[declId].type = typeId;
            if (!N(declId).kids.empty()) g_nodes[N(declId).kids[0]].type = typeId;
        }
        const MNode &decl = N(declId);
        if (!decl.kids.empty() && decl.type >= 0) InferExpression(decl.kids[0], T(decl.type));
        if (decl.type < 0 && decl.kids[0] < 0)
        {
            Fail("let 变量必须给初值：" + decl.s);
            return nullptr;
        }
        std::shared_ptr<MType> type = (decl.type < 0) ? ExprType(decl.kids[0]) : T(decl.type);
        Memory::Vector<int> dims;
        if (decl.i >= 0)
        {
            if (!ReadDims(static_cast<int>(decl.i), &dims)) return nullptr;
            /* 定长数组的槽数在编译期就定死：维数乘积超出运行内存预算时直接拒绝。
             * 否则它会一路走到运行期分配失败（arena 用尽后 bad_alloc 逃逸成 terminate），
             * 而 struct 字段的同类声明早就有这条预算检查，两者口径要一致。 */
            Memory::String shape;
            for (const int dimension : dims) shape += "[" + Memory::toString(dimension) + "]";
            const long long count = FixedArrayElementCount(dims);
            if (count > static_cast<long long>(g_runtimeMemory))
            {
                Fail("定长数组超出当前内存预算：" + decl.s + shape + "（" + Memory::toString(count) + " 个元素，预算 " + Memory::toString(g_runtimeMemory) + "）");
                return nullptr;
            }
            type = ArrayType(type, dims);
        }
        else if (decl.i == -2)
        {
            /* `T a[]` 写法：运行时长度数组（长度靠 `new T[N]` 初始化出来）。 */
            type = RuntimeArrayType(type);
        }
        if (!CheckDeclaredType(type, decl.s)) return nullptr;
        const bool isConst = (decl.type2 != 0);
        if (type->kind == MType::Ptr && decl.kids[0] < 0)
            return Fail(CHALI_TEXT("引用声明必须初始化，缺省值请使用 Option<T mut&>")), Bad();
        if (IsRuntimeArray(type) && decl.kids[0] < 0)
        {
            Fail("运行时数组（`T a[]`）必须用 `new T[N]` 初始化：" + decl.s);
            return nullptr;
        }
        if (IsRuntimeArray(type) && !InitIsRuntimeArray(decl.kids[0]))
        {
            Fail("运行时数组（`T a[]`）只能用 `new T[N]` 初始化（这样长度才跟着对象走）：" + decl.s);
            return nullptr;
        }
        if (isConst && decl.kids[0] < 0)
        {
            Fail("不可变变量（const）必须给初值：" + decl.s);
            return nullptr;
        }
        const auto previous = g_scope.back().find(NodeBindingKey(decl));
        if (previous != g_scope.back().end() && g_borrowFlowCapture)
        {
            // 先保存旧槽，合法性由控制流的所有到达路径共同决定。
            g_borrowFlowCapture->redeclarations[declId] = { previous->second.emitted, {} };
            g_scope.back().erase(previous);
        }
        const Memory::String emitted = DeclareVar(decl.s, type, Memory::String(), isConst, NodeBindingKey(decl), decl.bindingIdentity);
        if (decl.type < 0) RecordEditorTypeHint(declId, type);
        if (decl.kids[0] >= 0)
        {
            Memory::Set<Memory::String> borrowedTypes;
            if (ContainsBorrowedReferences(type, &borrowedTypes))
                FindNodeVar(decl)->borrowDepth = BorrowSourceDepth(decl.kids[0]);
            if (type->kind == MType::Ptr)
            {
                const auto root = BorrowRootVariable(decl.kids[0]);
                FindNodeVar(decl)->allocatedReference = N(decl.kids[0]).kind == MNode::NewPtr || (root && root->allocatedReference);
                if (root) FindNodeVar(decl)->borrowOwner = root->type->kind == MType::Ptr ? root->borrowOwner : root->emitted;
                g_effectAliases[emitted] = EffectPlace(decl.kids[0]);
            }
        }
        if (isConst && FindNodeVar(decl))
        {
            FindNodeVar(decl)->constantExpression = decl.kids[0];
            BindConstantReferences(decl.kids[0]);
        }

        /* 先落类型（元组会在这里补出 `__chali_tuple_*` 的布局），再登记析构 ——
         * 反过来登记时布局还没建，`StructHasOwnedLeaves` 查不到元组里的字符串叶子，
         * `auto t = (1, "x");` 的 t 就不会被回收。 */
        out.push_back(simple_ast::Node::declare(emitted, LowerType(type, emitted)));
        MarkDestructor(emitted, type);
        Memory::Set<Memory::String> referenceTypes;
        if (ContainsBorrowedReferences(type, &referenceTypes))
            initializerTemps.extendToBlock = true;
        /* 函数指针：初始化时先告诉求值"目标是这个签名"，重载靠它挑；
         * `new`：告诉它元素类型从哪个类型来。 */
        const std::shared_ptr<MType> savedFuncType = g_expectedFuncType;
        const std::shared_ptr<MType> savedNewType = g_expectedNewType;
        g_expectedFuncType = (type->kind == MType::Func) ? type : nullptr;
        g_expectedNewType = type;
        if (decl.kids[0] >= 0 && LowerConstructInit(emitted, type, decl.kids[0], &out))
        {
            /* `static T new(...)` 工厂体的构造初始化已经发出。 */
        }
        else if (decl.kids[0] >= 0 && type->kind == MType::Obj && ExprType(decl.kids[0])->kind != MType::Obj && !IsImplicitConversionValue(decl.kids[0], type))
        {
            out.push_back(DefaultInit(emitted, type));
            out.push_back(simple_ast::Node::exprStmt(
                LowerObjSet(simple_ast::Node::var(emitted), decl.kids[0], "obj 初始化")));
            MarkOwned(emitted, false);
        }
        else if (decl.kids[0] >= 0 && type->kind == MType::Struct && (IsObjGetCall(decl.kids[0]) || SameStruct(type, ExprType(decl.kids[0])) || IsImplicitConversionValue(decl.kids[0], type)))
        {
            /* `Point a = b;`：a 有自己的块（值语义），把 b 的槽逐个搬过来。 */
            const simple_ast::NodePtr block = DefaultInit(emitted, type);
            if (block) out.push_back(block);
            Memory::Vector<simple_ast::NodePtr> fresh;
            Memory::Vector<simple_ast::NodePtr> *const previousSink = g_freshSink;
            g_freshSink = &fresh;
            const simple_ast::NodePtr source = LowerValueForType(decl.kids[0], type);
            g_freshSink = previousSink;
            out.insert(out.end(), fresh.begin(), fresh.end());
            out.push_back(StoreStructValue(simple_ast::Node::var(emitted), decl.kids[0],
                                           source, type));
        }
        else
        {
            /* 初始化式里"当场造出来的临时串"（`name + "！" + format(…)` 里那个
             * `format` 结果）先落进归本块所有的临时变量，免得没人释放。 */
            Memory::Vector<simple_ast::NodePtr> fresh;
            g_freshSink = &fresh;
            const simple_ast::NodePtr init =
                (decl.kids[0] >= 0) ? simple_ast::Node::assign(
                                          emitted, IsObjectLike(type) ? MaterializeObjectValue(decl.kids[0], type) : MaterializeStringValue(decl.kids[0], type)) :
                                      DefaultInit(emitted, type, true);
            g_freshSink = nullptr;
            out.insert(out.end(), fresh.begin(), fresh.end());
            if (init) out.push_back(init);
            /* 声明即所有：初值是"当场造出来的"就归这个变量，离开块时自动释放。
             * 对象类类型（`bint` / 容器）**声明处一定有新对象**：没有初值就是
             * `DefaultInit` 建的那个空对象，有初值但"不是当场造的"也已经
             * `MaterializeObjectValue` 复制了一份 —— 两种情况都归这个变量。
             * 字符串同样：`MaterializeStringValue` 对借来的值会复制一份，
             * `string s = 别的变量;` 之后 s 也是自己那块串的主人。 */
            if (IsOwnedKind(type) && (IsFreshValue(decl.kids[0]) || IsObjectLike(type) || IsStandardOpcode(type, "String") || type->kind == MType::Array))
            {
                if (type->kind == MType::Array)
                    MarkOwnedArray(emitted, type);
                else
                    MarkOwned(emitted);
            }
            /* 定长数组按编译期个数释放拥有元素，再归还当前调用的数组存储。 */
            if (type->kind == MType::Array && !IsRuntimeArray(type) && ArrayElementNeedsRelease(type->elem))
                MarkOwnedArray(emitted, type);
        }
        if (g_addressTakenNames.count(decl.s) && NeedsReferenceStorage(type))
        {
            const auto storage = AllocateName("__chali_reference_storage");
            out.push_back(simple_ast::Node::declare(storage, simple_ast::Type::i64()));
            out.push_back(simple_ast::Node::assign(storage, simple_ast::Node::alloc(simple_ast::Node::i(ElementBytes(type)))));
            out.push_back(simple_ast::Node::storeAt(simple_ast::Node::var(storage), simple_ast::Node::var(emitted), LowerType(type, storage)));
            MarkOwned(storage);
            // 引用槽必须晚于其当前拥有值释放：清理原变量时还需从槽中读取最新值。
            for (auto &scope : g_ownedScopes)
            {
                const auto position = std::find_if(scope.names.begin(), scope.names.end(),
                                                   [&](const MOwnedScope::Item &item) { return item.name == storage; });
                if (position == scope.names.end()) continue;
                const auto item = *position;
                scope.names.erase(position);
                scope.names.insert(scope.names.begin(), item);
                break;
            }
            g_referenceStorage[emitted] = storage;
            FindNodeVar(decl)->emitted = storage;
            FindNodeVar(decl)->elementReference = true;
        }
        CaptureBorrowDeclaration(declId, FindNodeVar(decl));
        g_expectedFuncType = savedFuncType;
        g_expectedNewType = savedNewType;
        const auto cleanup = initializerTemps.finish();
        out.insert(out.end(), cleanup.begin(), cleanup.end());
        out.insert(out.begin() + initializerStart, initializerFresh.begin(), initializerFresh.end());
    }
    return simple_ast::Node::block(out);
}

/**
 * @brief 保存复合赋值目标的位置，保证容器、下标和地址各求值一次。
 * @param target 原始赋值目标。
 * @param prefix 保存目标所需的前置语句。
 * @return 保存位置后的目标；错误时返回负值。
 */
int FreezeCompoundTarget(int target, Memory::Vector<simple_ast::NodePtr> &prefix)
{
    const auto placeKind = N(target).kind;
    // 具名槽位无需捕获，沿用普通赋值对 const 和只读引用的诊断。
    if (placeKind == MNode::Var || placeKind == MNode::EnumCtor) return target;
    if (placeKind != MNode::Index && placeKind != MNode::Member && placeKind != MNode::Arrow && !(placeKind == MNode::Un && N(target).s == "*"))
    {
        Fail("赋值目标只能是变量、下标、成员或 `*p`");
        return -1;
    }
    const auto type = ExprType(target);
    const auto frozen = ConstRootOf(target);
    if (!frozen.empty())
    {
        Fail("不可变变量（const）的内容不能改：" + frozen);
        return -1;
    }
    if (!CheckWritable(target)) return -1;
    const auto raw = RawExprType(target);
    if (raw->kind == MType::Ptr && raw->readOnly)
    {
        Fail(CHALI_TEXT("只读引用（`T const&`）不能写入元素"));
        return -1;
    }
    // 保存值时沿用声明的复制/清理路径，大数和 Map 键也有完整表达式所有者。
    const auto capture = [&](int source, const std::shared_ptr<MType> &valueType) {
        const auto name = AllocateName("__chali_update_value");
        const int value = M_Var(M_Name(name.c_str()));
        const int declaration = M_Decl(M_Name(name.c_str()), -1, source);
        const int typeId = M_AddType(valueType);
        g_nodes[declaration].type = typeId;
        prefix.push_back(LowerLocalDecl(M_LocalDecl(typeId, M_List1(declaration), 0)));
        const auto variable = Find(name);
        if (variable)
        {
            if (g_freshSink && (IsOwnedKind(valueType) || valueType->kind == MType::Struct))
            {
                g_freshSink->push_back(simple_ast::Node::declare(variable->emitted, LowerType(valueType, variable->emitted)));
                g_freshSink->push_back(simple_ast::Node::assign(variable->emitted, simple_ast::Node::i(0)));
            }
            NoteExpressionTemp(variable->emitted);
        }
        return value;
    };
    // 地址和容器句柄仅借用，不能复制容器后修改副本。
    const auto bind = [&](const std::shared_ptr<MType> &valueType, const simple_ast::NodePtr &value) {
        const auto name = AllocateName("__chali_update_target");
        const auto emitted = DeclareVar(name, valueType);
        prefix.push_back(simple_ast::Node::declare(emitted, LowerType(valueType, emitted)));
        prefix.push_back(simple_ast::Node::assign(emitted, value));
        return M_Var(M_Name(name.c_str()));
    };
    const auto &place = N(target);
    if (place.kind == MNode::Index && place.s != "__chali_buffer_index" && IsContainer(ExprType(place.kids[0])))
    {
        const int base = place.kids[0], index = place.kids[1];
        const auto container = ExprType(base);
        const bool ownsReceiver = IsFreshValue(base) && RawExprType(base)->kind != MType::Ptr;
        const int receiver = ownsReceiver ? capture(base, container) : bind(container, LowerExpr(base));
        const int position = capture(index, container->name == "map" ? container->elem : ExprType(index));
        target = M_Index(receiver, position);
    }
    else if (place.kind != MNode::Var && place.kind != MNode::EnumCtor)
    {
        auto reference = Memory::makeShared<MType>();
        reference->kind = MType::Ptr;
        reference->elem = type;
        simple_ast::NodePtr address;
        if (place.kind == MNode::Index && type->kind != MType::Char)
        {
            // 完整下标链先折平，不能单独读取多维数组的中间行。
            address = LowerIndexChain(place.kids[0], place.kids[1], place.s == "__chali_buffer_index");
            if (address && address->kind == simple_ast::Node::Kind::Index)
                address = simple_ast::Node::offset(address->kids[0], address->kids[1], 8);
        }
        else
        {
            address = place.kind == MNode::Un && place.s == "*" ? LowerRawExpr(place.kids[0]) : LowerAddressOf(target);
        }
        target = bind(reference, address);
    }
    return target;
}

/**
 * @brief 自增减只计算一次目标位置，保存旧值和新值后写回。
 * @param id 自增减节点。
 * @param discard 是否丢弃表达式结果。
 * @return 前置取新值、后置取旧值的序列表达式。
 */
simple_ast::NodePtr LowerUpdate(int id, bool discard = false)
{
    const auto update = N(id);
    int target = update.kids[0];
    const auto type = ExprType(target);
    if (!type || (type->kind != MType::Int && type->kind != MType::Double && type->kind != MType::Char && !IsStandardOpcode(type, "BigInteger") && !IsStandardOpcode(type, "BigDecimal")))
        return Fail("自增和自减只支持数值或字符类型"), Bad();
    const auto frozen = ConstRootOf(target);
    if (!frozen.empty()) return Fail("不可变变量（const）的内容不能改：" + frozen), Bad();
    if (!CheckWritable(target)) return Bad();
    const auto raw = RawExprType(target);
    if (raw->kind == MType::Ptr && raw->readOnly)
        return Fail(CHALI_TEXT("只读引用（`T const&`）不能写入元素")), Bad();
    if (discard && N(target).kind == MNode::Var && (type->kind == MType::Int || type->kind == MType::Double))
    {
        const auto binding = Find(N(target).s);
        if (binding && !binding->elementReference && !g_referenceStorage.count(binding->emitted))
        {
            const int one = M_Cast(M_AddType(type), M_Int(1));
            const int arithmetic = M_BinOp(update.i > 0 ? 8 : 9, target, one);
            return LowerAssign(target, M_Cast(M_AddType(type), arithmetic));
        }
    }
    Memory::Vector<simple_ast::NodePtr> prefix;
    // 保存值时沿用声明的复制/清理路径，大数和 Map 键也有完整表达式所有者。
    const auto capture = [&](int source, const std::shared_ptr<MType> &valueType) {
        const auto name = AllocateName("__chali_update_value");
        const int value = M_Var(M_Name(name.c_str()));
        const int declaration = M_Decl(M_Name(name.c_str()), -1, source);
        const int typeId = M_AddType(valueType);
        g_nodes[declaration].type = typeId;
        prefix.push_back(LowerLocalDecl(M_LocalDecl(typeId, M_List1(declaration), 0)));
        const auto variable = Find(name);
        if (variable)
        {
            if (g_freshSink && (IsOwnedKind(valueType) || valueType->kind == MType::Struct))
            {
                g_freshSink->push_back(simple_ast::Node::declare(variable->emitted, LowerType(valueType, variable->emitted)));
                g_freshSink->push_back(simple_ast::Node::assign(variable->emitted, simple_ast::Node::i(0)));
            }
            NoteExpressionTemp(variable->emitted);
        }
        return value;
    };
    target = FreezeCompoundTarget(target, prefix);
    if (target < 0) return Bad();
    const int oldValue = capture(target, type);
    int one = M_Int(1), operand = oldValue;
    if (type->kind == MType::Int || type->kind == MType::Double)
        one = M_Cast(M_AddType(type), one);
    if (type->kind == MType::Char) operand = M_Cast(M_TypeInt(), oldValue);
    int arithmetic = M_BinOp(update.i > 0 ? 8 : 9, operand, one);
    if (type->kind == MType::Int || type->kind == MType::Double || type->kind == MType::Char)
        arithmetic = M_Cast(M_AddType(type), arithmetic);
    const int newValue = capture(arithmetic, type);
    prefix.push_back(LowerAssign(target, newValue));
    return simple_ast::Node::sequence(std::move(prefix), LowerExpr(update.type2 ? newValue : oldValue));
}

/**
 * @brief 元组解构先保存完整来源，再声明绑定或向已有目标赋值。
 * @param id 解构声明或赋值节点。
 * @return 按目标顺序写入的语句块；来源仅求值一次。
 */
simple_ast::NodePtr LowerDestructure(int id)
{
    const auto node = N(id);
    if (node.i)
    {
        std::function<std::shared_ptr<MType>(int)> targetType;
        targetType = [&](int target) {
            const auto pattern = N(target);
            if (pattern.kind != MNode::List && pattern.kind != MNode::Tuple) return ExprType(target);
            Memory::Vector<std::shared_ptr<MType>> elements;
            for (int child : pattern.kids) elements.push_back(targetType(child));
            return MakeTupleType(elements);
        };
        // 已声明目标的类型也参与泛型返回值和空集合的上下文推导。
        InferExpression(node.kids[1], targetType(node.kids[0]));
    }
    const auto type = ExprType(node.kids[1]);
    if (!IsTupleType(type)) return Fail("解构初始化式必须是元组"), Bad();
    const auto name = AllocateName("__chali_destructure");
    int decl = M_Decl(M_Name(name.c_str()), -1, node.kids[1]);
    const int typeId = M_AddType(type);
    g_nodes[decl].type = typeId;
    Memory::Vector<simple_ast::NodePtr> out;
    out.push_back(LowerLocalDecl(M_LocalDecl(typeId, M_List1(decl), 0)));
    auto owner = Find(name);
    if (owner) NoteExpressionTemp(owner->emitted);
    Memory::Set<Memory::String> names;
    std::function<bool(int, int, const std::shared_ptr<MType> &)> bind;
    bind = [&](int patternId, int value, const std::shared_ptr<MType> &valueType) {
        const auto pattern = N(patternId);
        if (pattern.kind == MNode::List || (node.i && pattern.kind == MNode::Tuple))
        {
            if (!IsTupleType(valueType) || pattern.kids.size() != valueType->paramTypes.size()) return Fail("元组解构的元素数量或嵌套形状不匹配");
            for (std::size_t i = 0; i < pattern.kids.size(); ++i)
                if (!bind(pattern.kids[i], M_TupleMember(value, M_Int(static_cast<long long>(i))), valueType->paramTypes[i])) return false;
            return true;
        }
        if (node.i)
        {
            out.push_back(LowerAssign(patternId, value));
            return g_error.empty();
        }
        if (pattern.s == "_") return true;
        if (!names.insert(pattern.s).second) return Fail("解构绑定名称重复：" + pattern.s);
        int child = M_Decl(M_Name(pattern.s.c_str()), -1, value);
        int childType = M_AddType(valueType);
        g_nodes[child].type = childType;
        // 与普通 auto 声明一致；M_Decl 的未设置标记不能被误读成 const。
        g_nodes[child].type2 = 0;
        g_nodes[child].syntaxContext = pattern.syntaxContext;
        g_nodes[child].bindingIdentity = pattern.bindingIdentity;
        g_nodes[child].bindingContext = pattern.bindingContext;
        g_nodes[child].module = pattern.module;
        g_nodes[child].sourceLine = pattern.sourceLine;
        g_nodes[child].sourceColumn = pattern.sourceColumn;
        g_nodes[child].macroExpansion = pattern.macroExpansion;
        out.push_back(LowerLocalDecl(M_LocalDecl(childType, M_List1(child), 0)));
        CaptureBorrowDeclaration(patternId, FindNodeVar(pattern));
        return g_error.empty();
    };
    if (!bind(node.kids[0], M_Var(M_Name(name.c_str())), type)) return Bad();
    return simple_ast::Node::block(out);
}

/**
 * @brief 条件里的新字符串也要落地，循环条件的前置语句每轮重新执行。
 * @param expressionId 条件表达式在节点池中的编号。
 * @param fresh 接收本次条件求值创建的临时状态。
 * @retval 生成的低层语法节点。
 */
simple_ast::NodePtr LowerConditionWithFresh(int expressionId,
                                            Memory::Vector<simple_ast::NodePtr> *fresh)
{
    Memory::Vector<simple_ast::NodePtr> *const savedSink = g_freshSink;
    g_freshSink = fresh;
    MFullExpressionTemps temporaries;
    const simple_ast::NodePtr value = LowerExpr(expressionId);
    g_freshSink = savedSink;
    auto cleanup = temporaries.finish();
    if (cleanup.empty()) return value;
    const auto result = AllocateName("__chali_condition_result");
    fresh->push_back(simple_ast::Node::declare(result, simple_ast::Type::boolean()));
    cleanup.insert(cleanup.begin(), simple_ast::Node::assign(result, value));
    return simple_ast::Node::sequence(std::move(cleanup), simple_ast::Node::var(result));
}

/**
 * @brief if / elif / else：elif 链从最内层往外包成嵌套的 ifThenElse。
 * @param id 待查找或操作的对象编号。
 * @retval 生成的低层语法节点。
 */
simple_ast::NodePtr LowerIf(int id)
{
    const MNode node = N(id);
    if (!CheckBoolCondition(node.kids[0], "if 条件")) return Bad();
    Memory::Vector<MNode> elifs;
    for (const int elifId : N(node.kids[2]).kids) elifs.push_back(N(elifId));
    simple_ast::NodePtr tail;
    if (node.kids[3] >= 0) tail = LowerStmt(node.kids[3]);
    for (auto it = elifs.rbegin(); it != elifs.rend(); ++it)
    {
        if (!CheckBoolCondition(it->kids[0], "elif 条件")) return Bad();
        Memory::Vector<simple_ast::NodePtr> fresh;
        const simple_ast::NodePtr condition = LowerConditionWithFresh(it->kids[0], &fresh);
        fresh.push_back(simple_ast::Node::ifThenElse(condition, LowerStmt(it->kids[1]), tail));
        tail = simple_ast::Node::block(fresh);
    }
    Memory::Vector<simple_ast::NodePtr> fresh;
    const simple_ast::NodePtr condition = LowerConditionWithFresh(node.kids[0], &fresh);
    fresh.push_back(simple_ast::Node::ifThenElse(condition, LowerStmt(node.kids[1]), tail));
    return simple_ast::Node::block(fresh);
}

/**
 * @brief `match (e) { 模式 => { … } _ => { … } }` 降级成一条 if 链。
 *
 * 语义是**不穿透**：命中哪个模式就只跑那一段。
 * 被匹配的表达式只求值一次：先存进一个编译器生成的临时变量，再逐个比较，
 * 所以 `match (f())` 不会把 `f()` 调好几遍。
 * `break` / `continue` 都不归它管 —— `match` 不截获循环控制，里面写的
 * `break` / `continue` 继续往外找循环。
 */
bool ReadSwitchConstant(int expressionId, bool *isBool, long long *value)
{
    if (expressionId < 0) return false;
    const MNode &expression = N(expressionId);
    if (expression.kind == MNode::Int)
    {
        *isBool = false;
        *value = expression.i;
        return true;
    }
    if (expression.kind == MNode::Bool)
    {
        *isBool = true;
        *value = expression.i != 0 ? 1 : 0;
        return true;
    }
    if (expression.kind == MNode::Char)
    {
        const Memory::String inner = expression.s.size() >= 2u ? expression.s.substr(1u, expression.s.size() - 2u) : Memory::String();
        const Memory::String text = UnescapeText(inner);
        if (text.size() != 1u) return false;
        *isBool = false;
        *value = static_cast<long long>(static_cast<unsigned char>(text[0]));
        return true;
    }
    if (expression.kind == MNode::Un && expression.s == "-" && !expression.kids.empty())
    {
        if (!ReadSwitchConstant(expression.kids[0], isBool, value) || *isBool) return false;
        *value = -*value;
        return true;
    }
    return false;
}

/**
 * @brief 判断分支常量是否可由匹配对象类型表示。
 * @param type 类型描述。
 * @param value 待转换、保存或参与计算的值。
 * @retval 分支常量可由匹配对象类型表示时为 true，否则为 false。
 */
bool SwitchConstantFits(const std::shared_ptr<MType> &type, long long value)
{
    if (!type || (type->kind != MType::Int && type->kind != MType::Char)) return false;
    if (type->kind == MType::Char) return value >= 0 && value <= 255;
    if (type->isUnsigned)
    {
        if (value < 0) return false;
        if (type->bits >= 64) return true;
        const unsigned long long maximum = (1ULL << type->bits) - 1ULL;
        return static_cast<unsigned long long>(value) <= maximum;
    }
    if (type->bits >= 64) return true;
    const long long limit = 1LL << (type->bits - 1);
    return value >= -limit && value < limit;
}

/**
 * @brief 分支守卫沿用完整表达式清理，失败跳转到下一模式。
 */
simple_ast::NodePtr LowerConditionWithFresh(int expressionId, Memory::Vector<simple_ast::NodePtr> *fresh);
/**
 * @brief 为模式匹配分支追加守卫条件。
 * @param item 待追加或处理的节点编号。
 * @param next 下一个匹配分支的标签。
 * @param body 待处理的函数体或循环体。
 */
void AppendMatchGuard(const MNode &item, const Memory::String &next, Memory::Vector<simple_ast::NodePtr> *body)
{
    if (item.guard < 0) return;
    if (ExprType(item.guard)->kind != MType::Bool)
    {
        Fail("match 守卫必须是 bool");
        return;
    }
    Memory::Vector<simple_ast::NodePtr> fresh;
    auto condition = LowerConditionWithFresh(item.guard, &fresh);
    body->insert(body->end(), fresh.begin(), fresh.end());
    body->push_back(simple_ast::Node::ifThenElse(simple_ast::Node::unary("!", condition), simple_ast::Node::jump(next), nullptr));
}

/**
 * @brief 为枚举匹配生成标签判断及载荷绑定。
 * @param id 待查找或操作的对象编号。
 * @param subjectType 被匹配表达式的类型。
 * @param cases 匹配分支的节点列表编号。
 * @retval 生成的低层语法节点。
 */
simple_ast::NodePtr LowerEnumSwitch(int id, const std::shared_ptr<MType> &subjectType,
                                    const Memory::Vector<int> &cases)
{
    const auto enumIt = g_enums.find(subjectType->name);
    if (enumIt == g_enums.end()) return Fail("没有声明的 enum：" + subjectType->name), Bad();
    bool hasDefault = false;
    Memory::Set<Memory::String> covered;
    Memory::Map<int, const MEnumVariantDef *> patterns;
    Memory::Map<int, MNode> normalizedPatterns;
    Memory::Map<int, Memory::Vector<std::pair<Memory::String, std::shared_ptr<MType>>>> groupBindings;
    for (const int caseId : cases)
    {
        const MNode &item = N(caseId);
        if (item.i < 0)
        {
            if (item.guard < 0)
            {
                if (hasDefault) return Fail("match 只能有一个 `_` 兜底分支"), Bad();
                hasDefault = true;
            }
            continue;
        }
        if (!NormalizeAstTypeQualifier(static_cast<int>(item.i))) return Bad();
        const MNode &parsedPattern = N(static_cast<int>(item.i));
        MNode pattern = parsedPattern;
        if (pattern.kind == MNode::Call && pattern.kids.size() == 2u)
        {
            const MNode &callee = N(pattern.kids[0]);
            if (callee.kind == MNode::EnumCtor || callee.kind == MNode::Var)
            {
                pattern = callee;
                pattern.kids = N(parsedPattern.kids[1]).kids;
            }
        }
        if (pattern.kind == MNode::Var)
        {
            pattern.kind = MNode::EnumCtor;
            pattern.s2 = pattern.s;
            pattern.s = subjectType->name;
        }
        if (pattern.kind == MNode::EnumCtor && pattern.s.compare(0u, 11u, "std::lang::") == 0)
            pattern.s.erase(0u, 11u);
        if (pattern.kind != MNode::EnumCtor)
            return Fail("enum match 的分支必须写成 `" + subjectType->name + "::变体` 或 `" + subjectType->name + "::变体(绑定名)`；实际解析为 " + pattern.s + (pattern.s2.empty() ? Memory::String() : "::" + pattern.s2) + "（节点 " + Memory::toString(static_cast<int>(pattern.kind)) + "）"), Bad();
        /* 模式的枚举类型由主表达式唯一确定。泛型枚举允许把
         * `Option<int>::Value` 简写为 `Option::Value`；普通构造表达式仍要求完整类型。
         * 另外，`std::模块::枚举::变体` 这种限定写法按**裸类型名**归一：
         * 类型在所有表里都以裸名登记（`std::io::SeekOrigin` == `SeekOrigin`）。 */
        if (pattern.s.find("::") != Memory::String::npos && FindEnumVariant(BareTypeName(pattern.s), pattern.s2) != nullptr)
            pattern.s = BareTypeName(pattern.s);
        const Memory::String genericPrefix = pattern.s + "__";
        if (subjectType->name.compare(0u, genericPrefix.size(), genericPrefix) == 0u)
            pattern.s = subjectType->name;
        if (pattern.s != subjectType->name)
            return Fail("分支的枚举类型必须与 match 主表达式一致：主表达式是 " + subjectType->name + "，分支是 " + pattern.s + (pattern.s2.empty() ? Memory::String() : "::" + pattern.s2)), Bad();
        const MEnumVariantDef *variant = FindEnumVariant(pattern.s, pattern.s2);
        if (variant == nullptr)
            return Fail("enum " + pattern.s + " 没有变体 " + pattern.s2), Bad();
        if (covered.count(variant->name))
            return Fail("match 中存在重复的 enum 变体：" + pattern.s + "::" + variant->name), Bad();
        if (pattern.kids.size() != variant->payloads.size())
            return Fail("enum 模式 " + pattern.s + "::" + variant->name + " 需要 " + Memory::toString(variant->payloads.size()) + " 个绑定，实际给了 " + Memory::toString(pattern.kids.size()) + " 个"), Bad();
        if (item.guard < 0) covered.insert(variant->name);
        Memory::Set<Memory::String> binders;
        for (const int binderId : pattern.kids)
        {
            const MNode &binder = N(binderId);
            if (binder.kind != MNode::Var)
                return Fail("enum 分支的载荷位置只能写绑定变量名，不能写表达式"), Bad();
            if (!CheckUserName(binder.s)) return Bad();
            if (!binders.insert(binder.s).second)
                return Fail("同一个 enum 模式里不能重复绑定变量：" + binder.s), Bad();
        }
        if (item.type2 > 0)
        {
            Memory::Vector<std::pair<Memory::String, std::shared_ptr<MType>>> bindings;
            for (std::size_t i = 0; i < pattern.kids.size(); ++i)
                bindings.emplace_back(N(pattern.kids[i]).s, variant->payloads[i].type);
            const auto previous = groupBindings.find(item.type2);
            if (previous == groupBindings.end())
                groupBindings[item.type2] = bindings;
            else
            {
                bool same = previous->second.size() == bindings.size();
                for (std::size_t i = 0; same && i < bindings.size(); ++i)
                    same = previous->second[i].first == bindings[i].first && SameType(previous->second[i].second, bindings[i].second);
                if (!same) return Fail("或模式的载荷绑定名称、顺序和类型必须一致"), Bad();
            }
        }
        patterns[caseId] = variant;
        normalizedPatterns[static_cast<int>(item.i)] = std::move(pattern);
    }
    if (!hasDefault && covered.size() != enumIt->second.variants.size())
    {
        Memory::String missing;
        for (const MEnumVariantDef &variant : enumIt->second.variants)
        {
            if (covered.count(variant.name) != 0u) continue;
            if (!missing.empty()) missing += "、";
            missing += subjectType->name + "::" + variant.name;
        }
        return Fail(Memory::String("enum match") + " 必须覆盖所有变体或提供 `_` 兜底分支；缺少：" + missing), Bad();
    }

    const MNode &switchNode = N(id);
    const Memory::String subject = AllocateName("__chali_switch_enum");
    const Memory::String breakLabel = NewBlockLabel("brk");
    Memory::Vector<simple_ast::NodePtr> setup;
    setup.push_back(simple_ast::Node::declare(subject, LowerType(subjectType, subject)));
    /*
     * `match (<当场造出来的枚举>)`：这份聚合没有别的所有者，subject 就是它的主人，
     * 出块时递归释放它槽里的字符串 / 对象；主体是变量时变量自己管，别重复释放。
     *
     * 先把结果搬进 subject **自己的存储**再登记，不能只抄句柄：枚举构造器返回的是
     * **每个构造器一份的静态对象**，连着 match 同类型枚举会盖掉前一次的槽
     * （`match (f(8)) …; match (f(2)) …;` 里第一份载荷会漏）。
     */
    if (IsFreshAggregate(switchNode.kids[0]) && subjectType && subjectType->kind == MType::Struct)
    {
        if (const simple_ast::NodePtr init = DefaultInit(subject, subjectType))
            setup.push_back(init);
        setup.push_back(MoveStructValue(simple_ast::Node::var(subject),
                                        LowerExpr(switchNode.kids[0]), subjectType));
        MarkDestructor(subject, subjectType);
    }
    else
    {
        setup.push_back(simple_ast::Node::assign(subject, LowerExpr(switchNode.kids[0])));
    }
    const MField *tagField = FindField(subjectType->name, "__tag");
    if (tagField == nullptr) return Fail("enum " + subjectType->name + " 缺少标签槽"), Bad();

    const auto lowerBody = [&](const MNode &item, const MEnumVariantDef *variant, const Memory::String &next) {
        PushScope();
        PushOwnedScope();
        Memory::Vector<simple_ast::NodePtr> body;
        if (variant != nullptr)
        {
            const MNode &pattern = normalizedPatterns.at(static_cast<int>(item.i));
            for (std::size_t index = 0; index < variant->payloads.size(); ++index)
            {
                const MEnumPayload &payload = variant->payloads[index];
                const auto &bindingSyntax = N(pattern.kids[index]);
                const Memory::String sourceName = bindingSyntax.s;
                /*
                 * 载荷绑定默认是"只看不改"的：值载荷本来就是副本，`T const &` 载荷也只能读。
                 * 但 `Value(T mut & v)` 这种**可变引用载荷**要放开 —— 否则透过它写回原对象会被
                 * `ConstRootOf` 当成"改 const 变量的内容"拦掉，"用引用载荷拿可变引用"就没意义了。
                 */
                const bool valueMatch = switchNode.type2 == 1;
                const bool frozen = valueMatch ? payload.type->kind == MType::Ptr : payload.type->kind != MType::Ptr || payload.type->readOnly;
                const Memory::String emitted = DeclareVar(sourceName, payload.type, Memory::String(), frozen, NodeBindingKey(bindingSyntax), bindingSyntax.bindingIdentity);
                if (HasBorrowedValue(payload.type))
                    FindNodeVar(bindingSyntax)->borrowDepth = BorrowSourceDepth(switchNode.kids[0]);
                // 被闭包借用的载荷直接使用主体的稳定槽，避免建立指向临时标量的引用。
                const bool captureStorage = !valueMatch && g_addressTakenNames.count(sourceName) && NeedsReferenceStorage(payload.type);
                FindNodeVar(bindingSyntax)->elementReference = captureStorage;
                body.push_back(simple_ast::Node::declare(emitted,
                                                         captureStorage ? simple_ast::Type::i64() : LowerType(payload.type, emitted)));
                const MField *field = FindField(subjectType->name, payload.storageName);
                if (field == nullptr)
                {
                    Fail("enum " + subjectType->name + " 缺少载荷槽 " + payload.storageName);
                    continue;
                }
                const simple_ast::NodePtr address = simple_ast::Node::offset(
                    simple_ast::Node::var(subject), simple_ast::Node::i(field->slot), 8);
                /*
                 * 载荷绑定是**主体槽的别名**（字符串 / 对象都不复制）：分支里读它是安全的
                 * —— 主体的递归释放发生在分支结束之后。复制一份会改变"绑定可以带出分支"
                 * 的既有语义（`result = error;` 里 result 会随绑定一起被释放），所以不复制。
                 */
                auto value = payload.type->kind == MType::Struct || captureStorage ? address : simple_ast::Node::loadFrom(address, LowerType(payload.type, emitted));
                const bool transferPayload = HasNonCopyableValue(payload.type) && IsFreshAggregate(switchNode.kids[0]);
                if (valueMatch && HasNonCopyableValue(payload.type) && !transferPayload)
                    return Fail("不可复制枚举的值匹配需要显式移动主体"), Bad();
                if (transferPayload)
                {
                    FindNodeVar(bindingSyntax)->elementReference = false;
                    if (payload.type->kind == MType::Struct)
                    {
                        body.push_back(DefaultInit(emitted, payload.type, false, true));
                        body.push_back(MoveStructValue(simple_ast::Node::var(emitted), address, payload.type));
                        MarkDestructor(emitted, payload.type);
                    }
                    else
                    {
                        body.push_back(simple_ast::Node::assign(emitted, simple_ast::Node::loadInt(address)));
                        body.push_back(simple_ast::Node::storeAt(address, simple_ast::Node::i(0), simple_ast::Type::i64()));
                        MarkOwned(emitted);
                        if (g_addressTakenNames.count(sourceName) && NeedsReferenceStorage(payload.type))
                        {
                            // 表达式分支的载荷副本与普通局部值一样，需要稳定槽供 break 隐式移动。
                            const auto storage = AllocateName("__chali_match_value_storage");
                            body.push_back(simple_ast::Node::declare(storage, simple_ast::Type::i64()));
                            body.push_back(simple_ast::Node::assign(storage, simple_ast::Node::alloc(simple_ast::Node::i(ElementBytes(payload.type)))));
                            body.push_back(simple_ast::Node::storeAt(simple_ast::Node::var(storage), simple_ast::Node::var(emitted), LowerType(payload.type, storage)));
                            MarkOwned(storage);
                            auto &names = g_ownedScopes.back().names;
                            const auto slot = names.back();
                            names.pop_back();
                            names.insert(names.begin(), slot);
                            g_referenceStorage[emitted] = storage;
                            FindNodeVar(bindingSyntax)->emitted = storage;
                            FindNodeVar(bindingSyntax)->elementReference = true;
                        }
                    }
                    continue;
                }
                if (valueMatch && payload.type->kind == MType::Struct)
                {
                    body.push_back(DefaultInit(emitted, payload.type, false, true));
                    body.push_back(DeepCopyStructValue(simple_ast::Node::var(emitted), value, payload.type));
                    MarkDestructor(emitted, payload.type);
                }
                else
                {
                    if (valueMatch && IsStandardOpcode(payload.type, "String"))
                    {
                        value = simple_ast::Node::call("__std_str_sub", { "__chali_s", "__chali_a", "__chali_b" },
                                                       { value, simple_ast::Node::i(0), simple_ast::Node::i(-1) });
                        MarkOwned(emitted);
                    }
                    else if (valueMatch && IsObjectLike(payload.type))
                    {
                        value = simple_ast::Node::call(ObjectCopyFunction(payload.type), { "__chali_v" }, { value });
                        MarkOwned(emitted);
                    }
                    body.push_back(simple_ast::Node::assign(emitted, value));
                }
                if (valueMatch && g_addressTakenNames.count(sourceName) && NeedsReferenceStorage(payload.type))
                {
                    // 表达式分支的载荷副本与普通局部值一样，需要稳定槽供 break 隐式移动。
                    const auto storage = AllocateName("__chali_match_value_storage");
                    body.push_back(simple_ast::Node::declare(storage, simple_ast::Type::i64()));
                    body.push_back(simple_ast::Node::assign(storage, simple_ast::Node::alloc(simple_ast::Node::i(ElementBytes(payload.type)))));
                    body.push_back(simple_ast::Node::storeAt(simple_ast::Node::var(storage), simple_ast::Node::var(emitted), LowerType(payload.type, storage)));
                    MarkOwned(storage);
                    auto &names = g_ownedScopes.back().names;
                    const auto slot = names.back();
                    names.pop_back();
                    names.insert(names.begin(), slot);
                    g_referenceStorage[emitted] = storage;
                    FindNodeVar(bindingSyntax)->emitted = storage;
                    FindNodeVar(bindingSyntax)->elementReference = true;
                }
            }
        }
        AppendMatchGuard(item, next, &body);
        const Memory::Vector<simple_ast::NodePtr> statements = LowerStmtList(item.kids[0]);
        body.insert(body.end(), statements.begin(), statements.end());
        const MOwnedScope owned = g_ownedScopes.back();
        g_ownedScopes.pop_back();
        PopScope();
        AppendScopeDrops(owned, Memory::String(), &body);
        body.push_back(simple_ast::Node::jump(breakLabel));
        return simple_ast::Node::block(body);
    };

    int fallback = -1;
    for (const int caseId : cases)
    {
        const auto &item = N(caseId);
        if (item.i < 0 && item.guard < 0)
        {
            fallback = caseId;
            continue;
        }
        const auto next = NewBlockLabel("match_next");
        if (item.i < 0)
            setup.push_back(lowerBody(item, nullptr, next));
        else
        {
            const auto *variant = patterns.at(caseId);
            auto tag = simple_ast::Node::loadFrom(simple_ast::Node::offset(simple_ast::Node::var(subject), simple_ast::Node::i(tagField->slot), 8), LowerType(T(M_TypeIntBits(32, 0)), subjectType->name + ".tag"));
            auto condition = simple_ast::Node::binary("==", tag, simple_ast::Node::i(variant->tag));
            setup.push_back(simple_ast::Node::ifThenElse(condition, lowerBody(item, variant, next), nullptr));
        }
        setup.push_back(simple_ast::Node::label(next));
    }
    if (fallback >= 0) setup.push_back(lowerBody(N(fallback), nullptr, breakLabel));
    setup.push_back(simple_ast::Node::label(breakLabel));
    return simple_ast::Node::block(setup);
}

/**
 * @brief 识别区间常量并校验十进制词素，供模式和遍历共享。
 * @param id 表达式节点。
 * @param active 本次递归已访问的节点。
 * @param negativeLiteral 是否位于直接取负路径，允许 int64 最小值的幅值。
 * @return 可无副作用求值时返回真。
 */
bool IntervalConstantTree(int id, Memory::Set<int> &active, bool negativeLiteral = false);

/**
 * @brief 生成区间条件：`lo <cmp> subject && subject <cmp> hi`。
 *
 * `loClosed` / `hiClosed` 决定两端用 `<=` 还是 `<`。模式里的 `lo` / `hi` 直接求值
 * （区间两端通常写常量；写成表达式时按普通表达式语义求值）。
 */
simple_ast::NodePtr MakeRangeCondition(const Memory::String &subject, int loId, int hiId,
                                       bool loClosed, bool hiClosed)
{
    const simple_ast::NodePtr lower = simple_ast::Node::binary(
        loClosed ? "<=" : "<", LowerExpr(loId), simple_ast::Node::var(subject));
    const simple_ast::NodePtr upper = simple_ast::Node::binary(
        hiClosed ? "<=" : "<", simple_ast::Node::var(subject), LowerExpr(hiId));
    return simple_ast::Node::binary("&&", lower, upper);
}

/**
 * @brief 判断是否为左闭区间模式。
 * @param pattern 模式。
 * @param loClosed 左端闭合标志。
 * @param hiClosed 右端闭合标志。
 * @return 支持的区间模式返回真。
 */
bool IsRangePattern(const MNode &pattern, bool *loClosed, bool *hiClosed)
{
    if (pattern.kind == MNode::Range)
    {
        *loClosed = (pattern.i & 2) != 0;
        *hiClosed = (pattern.i & 1) != 0;
        return true;
    }
    return false;
}

/**
 * @brief 读取字符串模式常量；单引号在字符串模式中按文本解释。
 * @param id 模式或区间端点节点。
 * @param text 解码后的 UTF-8 文本。
 * @return 是否为字符串或单引号文本常量。
 */
bool ReadStringPattern(int id, Memory::String *text)
{
    const MNode &pattern = N(id);
    if (pattern.kind == MNode::Str)
    {
        *text = UnescapeText(pattern.s);
        return true;
    }
    if (pattern.kind == MNode::Char)
    {
        *text = UnescapeText(pattern.s.substr(1u, pattern.s.size() - 2u));
        return true;
    }
    return false;
}

/**
 * @brief 使用运行时字典序比较生成字符串模式条件。
 * @param subject 已经求值一次的主体变量。
 * @param pattern 已校验的字符串常量或区间。
 * @return 精确匹配或开闭区间的比较条件。
 */
simple_ast::NodePtr MakeStringPatternCondition(const Memory::String &subject, const MNode &pattern)
{
    const auto compare = [&](const Memory::String &text, const char *op) {
        const auto order = simple_ast::Node::call("__std_str_cmp", { "__chali_sl", "__chali_sr" },
                                                  { simple_ast::Node::var(subject), simple_ast::Node::str(text) });
        return simple_ast::Node::binary(op, order, simple_ast::Node::i(0));
    };
    bool loClosed = false, hiClosed = false;
    if (IsRangePattern(pattern, &loClosed, &hiClosed))
    {
        Memory::String lo, hi;
        ReadStringPattern(pattern.kids[0], &lo);
        ReadStringPattern(pattern.kids[1], &hi);
        return simple_ast::Node::binary("&&", compare(lo, loClosed ? ">=" : ">"),
                                        compare(hi, hiClosed ? "<=" : "<"));
    }
    const auto text = pattern.kind == MNode::Char ? UnescapeText(pattern.s.substr(1u, pattern.s.size() - 2u)) : UnescapeText(pattern.s);
    return compare(text, "==");
}

/**
 * @brief 用符号与绝对值保存完整 64 位整数，避免比较时丢失精度。
 */
struct MMatchInteger
{
    bool negative = false;
    std::uint64_t magnitude = 0;
};
/**
 * @brief 比较两个数学整数，兼容负数与 uint64 的完整范围。
 * @param left 左值。
 * @param right 右值。
 * @retval 小于返回负一，等于返回零，大于返回一。
 */
int CompareMatchInteger(const MMatchInteger &left, const MMatchInteger &right)
{
    if (left.negative != right.negative) return left.negative ? -1 : 1;
    const int order = left.magnitude < right.magnitude ? -1 : left.magnitude > right.magnitude ? 1 :
                                                                                                 0;
    return left.negative ? -order : order;
}

/**
 * @brief 静态读取整数模式端点；运行时表达式不参与重叠判断。
 * @param id 端点表达式。
 * @param subject 匹配主体类型，用于解释完整 uint64 字面量。
 * @param out 输出无损整数。
 * @retval 可求出的整数常量返回真，动态或非整数端点返回假。
 */
bool ReadMatchInteger(int id, const std::shared_ptr<MType> &subject, MMatchInteger *out)
{
    const auto type = ExprType(id);
    if (!type || (type->kind != MType::Int && type->kind != MType::Char)) return false;
    Memory::Set<int> active;
    if (!IntervalConstantTree(id, active)) return false;
    MConstant value;
    if (!EvaluateConstant(id, &value) || value.kind != MConstant::Integer) return false;
    const bool unsignedValue = value.isUnsigned || (subject->isUnsigned && subject->bits == 64 && N(id).kind == MNode::Int && N(id).i < 0);
    out->negative = !unsignedValue && value.integer < 0;
    out->magnitude = out->negative ? std::uint64_t(0) - std::uint64_t(value.integer) : std::uint64_t(value.integer);
    return true;
}
/**
 * @brief 保存整数单值或区间模式的静态覆盖范围。
 */
struct MMatchIntegerRange
{
    MMatchInteger lo, hi;
    bool hiClosed = true;
};
/**
 * @brief 检查当前整数模式与此前模式是否有共同整数，登记非致命警告。
 * @param patternId 模式节点编号。
 * @param subject 匹配主体的整数类型。
 * @param previous 同一 match 中此前可静态确定的覆盖范围。
 * @note 空区间和动态端点不参与检查；有守卫的模式仍检查覆盖范围重叠。
 */
void CheckMatchIntegerOverlap(int patternId, const std::shared_ptr<MType> &subject, Memory::Vector<MMatchIntegerRange> *previous)
{
    const MNode pattern = N(patternId);
    MMatchIntegerRange current;
    bool loClosed = true;
    if (IsRangePattern(pattern, &loClosed, &current.hiClosed))
    {
        if (pattern.kids.size() != 2 || !ReadMatchInteger(pattern.kids[0], subject, &current.lo) || !ReadMatchInteger(pattern.kids[1], subject, &current.hi)) return;
    }
    else
    {
        if (!ReadMatchInteger(patternId, subject, &current.lo)) return;
        current.hi = current.lo;
    }
    // 将整数半开区间归一为闭区间，不对最大整数做加一运算。
    if (!current.hiClosed)
    {
        if (!current.hi.negative && current.hi.magnitude > 0)
            --current.hi.magnitude;
        else
        {
            current.hi.negative = true;
            ++current.hi.magnitude;
        }
        current.hiClosed = true;
    }
    const unsigned bits = subject->kind == MType::Char ? 8 : subject->bits;
    const bool unsignedSubject = subject->isUnsigned || subject->kind == MType::Char;
    const MMatchInteger minimum{ !unsignedSubject, unsignedSubject ? 0 : std::uint64_t(1) << (bits - 1) };
    const MMatchInteger maximum{ false, unsignedSubject ? (bits == 64 ? UINT64_MAX : (std::uint64_t(1) << bits) - 1) : (std::uint64_t(1) << (bits - 1)) - 1 };
    if (CompareMatchInteger(current.lo, minimum) < 0) current.lo = minimum;
    if (CompareMatchInteger(current.hi, maximum) > 0) current.hi = maximum;
    if (CompareMatchInteger(current.lo, current.hi) > 0) return;
    for (const auto &range : *previous)
    {
        if (CompareMatchInteger(current.lo, range.hi) <= 0 && CompareMatchInteger(range.lo, current.hi) <= 0)
        {
            Memory::AllocationScope persistent(Memory::defaultRuntimeAllocator());
            const Memory::String message = "[CHL1601] match 整数模式与前面的模式重叠；仍按分支顺序和守卫选择匹配项";
            bool duplicate = false;
            for (const auto &warning : g_warnings)
                if (warning.message == message && warning.line == pattern.sourceLine && warning.column == pattern.sourceColumn) duplicate = true;
            if (!duplicate && g_warnings.size() < 100u) g_warnings.push_back({ message, pattern.sourceLine, pattern.sourceColumn });
            break;
        }
    }
    previous->push_back(current);
}

/**
 * @brief 将 switch 或 match 分支降级为条件跳转。
 * @param id 待查找或操作的对象编号。
 * @retval 生成的低层语法节点。
 */
simple_ast::NodePtr LowerSwitch(int id)
{
    const MNode node = N(id);
    const std::shared_ptr<MType> subjectType = ExprType(node.kids[0]);
    const Memory::Vector<int> cases = N(node.kids[1]).kids;
    if (IsEnumType(subjectType)) return LowerEnumSwitch(id, subjectType, cases);
    const bool numeric = subjectType && (subjectType->kind == MType::Int || subjectType->kind == MType::Char || subjectType->kind == MType::Bool || subjectType->kind == MType::Double);
    const bool strings = subjectType && IsStandardOpcode(subjectType, "String");
    const bool equality = !numeric && !strings;
    const bool builtinEquality = IsStandardOpcode(subjectType, "BigInteger") || IsStandardOpcode(subjectType, "BigDecimal");
    if (equality && !builtinEquality)
    {
        bool implemented = false;
        for (const auto &operation : g_operators)
            if (operation.op == "==" && operation.result && operation.result->kind == MType::Bool
                && (SameType(operation.left, subjectType) || SameType(operation.right, subjectType))) implemented = true;
        if (!implemented) return Fail("match 主表达式必须支持 Equal<T>：" + Describe(subjectType)), Bad();
    }
    bool hasDefault = false;
    Memory::Set<Memory::String> constants;
    Memory::Set<double> floatingConstants;
    Memory::Vector<MMatchIntegerRange> integerRanges;
    for (const int caseId : cases)
    {
        const MNode item = N(caseId);
        if (item.i < 0)
        {
            if (item.guard >= 0) continue;
            if (hasDefault)
            {
                Fail("match 只能有一个 `_` 兜底分支");
                return Bad();
            }
            hasDefault = true;
            continue;
        }
        RewriteRangeCall(static_cast<int>(item.i), true);
        if (!g_error.empty()) return Bad();
        const MNode pattern = N(static_cast<int>(item.i));
        if (pattern.kind == MNode::Tuple)
            return Fail("match 区间使用 std::lang::range<T>(a,b) 或 std::lang::rangeEq<T>(a,b)"), Bad();
        bool loClosed = false;
        bool hiClosed = false;
        if (IsRangePattern(pattern, &loClosed, &hiClosed)
            && subjectType->kind != MType::Int && subjectType->kind != MType::Char)
            return Fail("match 的 range 和 rangeEq 模式只支持整数或 char"), Bad();
        if (equality)
        {
            const auto patternType = ExprType(static_cast<int>(item.i));
            const MOperatorDecl *operation = nullptr;
            if (!builtinEquality && (!FindOperator("==", subjectType, patternType, &operation) || !operation || operation->result->kind != MType::Bool))
                return Fail("match 模式与主表达式没有返回 bool 的 Equal<T> 实现"), Bad();
            continue;
        }
        if (strings)
        {
            Memory::String lo, hi;
            if (IsRangePattern(pattern, &loClosed, &hiClosed))
            {
                if (!ReadStringPattern(pattern.kids[0], &lo) || !ReadStringPattern(pattern.kids[1], &hi))
                    return Fail("字符串区间的两个端点必须是字符串常量"), Bad();
                if (lo > hi) return Fail("字符串区间的下界不能大于上界"), Bad();
            }
            else
            {
                if (!ReadStringPattern(static_cast<int>(item.i), &lo))
                    return Fail("字符串 match 分支必须是字符串常量或字符串区间"), Bad();
                if (item.guard < 0 && !constants.insert(lo).second)
                    return Fail("match 中存在重复的字符串模式：" + lo), Bad();
            }
            continue;
        }
        if (IsRangePattern(pattern, &loClosed, &hiClosed))
        {
            if (pattern.kids.size() != 2u)
            {
                Fail("区间模式需要两个端点：std::lang::range(a,b) 或 std::lang::rangeEq(a,b)");
                return Bad();
            }
            for (const int endpoint : pattern.kids)
            {
                Memory::Set<int> active;
                IntervalConstantTree(endpoint, active);
                if (!g_error.empty()) return Bad();
                const auto type = ExprType(endpoint);
                if (!type || (type->kind != MType::Int && type->kind != MType::Char))
                    return Fail("match 区间端点必须是整数或 char"), Bad();
            }
            if (subjectType->kind == MType::Int || subjectType->kind == MType::Char) CheckMatchIntegerOverlap(static_cast<int>(item.i), subjectType, &integerRanges);
            continue;
        }
        if (subjectType->kind == MType::Double)
        {
            MNode constant = pattern;
            bool negative = false;
            if (constant.kind == MNode::Un && constant.s == "-" && !constant.kids.empty())
            {
                negative = true;
                constant = N(constant.kids[0]);
            }
            if (constant.kind == MNode::Float || constant.kind == MNode::Int)
            {
                double value = constant.kind == MNode::Float ? constant.d : static_cast<double>(constant.i);
                if (negative) value = -value;
                if (item.guard < 0 && !floatingConstants.insert(value).second)
                    return Fail("match 中存在重复的浮点分支常量"), Bad();
                continue;
            }
        }
        bool caseIsBool = false;
        long long caseValue = 0;
        if (!ReadSwitchConstant(static_cast<int>(item.i), &caseIsBool, &caseValue))
        {
            Fail("match 分支必须是整数、char、浮点、bool 常量，或 std::lang::range / rangeEq 区间模式");
            return Bad();
        }
        const bool subjectIsBool = subjectType->kind == MType::Bool;
        if (subjectIsBool != caseIsBool)
        {
            Fail("分支的类型必须与 match 主表达式一致：主表达式是 " + Describe(subjectType) + "，分支是 " + Memory::String(caseIsBool ? "bool" : "int64"));
            return Bad();
        }
        if (!caseIsBool && !SwitchConstantFits(subjectType, caseValue))
        {
            Fail("分支常量 " + Memory::toString(caseValue) + " 超出 " + Describe(subjectType) + " 的取值范围");
            return Bad();
        }
        if (subjectType->kind == MType::Int || subjectType->kind == MType::Char) CheckMatchIntegerOverlap(static_cast<int>(item.i), subjectType, &integerRanges);
        const Memory::String key = Memory::String(caseIsBool ? "bool:" : "int:") + Memory::toString(caseValue);
        if (item.guard < 0 && !constants.insert(key).second)
        {
            Fail("match 中存在重复的分支常量：" + Memory::toString(caseValue));
            return Bad();
        }
    }
    const Memory::String subject = AllocateName("__chali_switch");
    const Memory::String breakLabel = NewBlockLabel("brk");
    Memory::Vector<simple_ast::NodePtr> setup;
    setup.push_back(simple_ast::Node::declare(subject, LowerType(subjectType, subject)));
    setup.push_back(simple_ast::Node::assign(subject,
                                             strings                                        ? MaterializeStringValue(node.kids[0], subjectType) :
                                             equality && subjectType->kind == MType::Struct ? OwnedAggregateTempFor(node.kids[0], subjectType) :
                                             equality && IsObjectLike(subjectType)          ? OwnedObjectTemp(node.kids[0], subjectType) :
                                                                                              LowerExpr(node.kids[0])));
    /* 字符串主题一定会落到 subject 自己那块串上（借来的已复制），归这个临时所有。 */
    if (strings) MarkOwned(subject);
    int subjectExpression = -1;
    if (equality)
    {
        g_scope.back()[subject] = MVar{ subjectType, subject, true };
        subjectExpression = M_New(MNode::Var, subject);
        g_nodes[subjectExpression].module = node.module;
    }

    const auto lowerBody = [&](const MNode &item, const Memory::String &next) {
        PushScope();
        PushOwnedScope();
        Memory::Vector<simple_ast::NodePtr> body;
        AppendMatchGuard(item, next, &body);
        auto statements = LowerStmtList(item.kids[0]);
        body.insert(body.end(), statements.begin(), statements.end());
        const MOwnedScope owned = g_ownedScopes.back();
        g_ownedScopes.pop_back();
        PopScope();
        AppendScopeDrops(owned, Memory::String(), &body);
        body.push_back(simple_ast::Node::jump(breakLabel));
        return simple_ast::Node::block(body);
    };

    int fallback = -1;
    for (int caseId : cases)
    {
        const auto &item = N(caseId);
        if (item.i < 0 && item.guard < 0)
        {
            fallback = caseId;
            continue;
        }
        const auto next = NewBlockLabel("match_next");
        if (item.i < 0)
        {
            setup.push_back(lowerBody(item, next));
            setup.push_back(simple_ast::Node::label(next));
            continue;
        }
        const MNode pattern = N(static_cast<int>(item.i));
        if (pattern.kind == MNode::Tuple)
            return Fail("match 区间使用 std::lang::range<T>(a,b) 或 std::lang::rangeEq<T>(a,b)"), Bad();
        bool loClosed = false;
        bool hiClosed = false;
        simple_ast::NodePtr condition;
        if (equality)
            condition = LowerBinary("==", subjectExpression, static_cast<int>(item.i));
        else if (strings)
            condition = MakeStringPatternCondition(subject, pattern);
        else if (IsRangePattern(pattern, &loClosed, &hiClosed))
        {
            condition = MakeRangeCondition(subject, pattern.kids[0], pattern.kids[1], loClosed,
                                           hiClosed);
        }
        else
        {
            condition = simple_ast::Node::binary(
                "==", simple_ast::Node::var(subject), LowerExpr(static_cast<int>(item.i)));
        }
        setup.push_back(simple_ast::Node::ifThenElse(condition, lowerBody(item, next), nullptr));
        setup.push_back(simple_ast::Node::label(next));
    }
    if (fallback >= 0) setup.push_back(lowerBody(N(fallback), breakLabel));
    setup.push_back(simple_ast::Node::label(breakLabel)); /* 分支表的出口 */
    return simple_ast::Node::block(setup);
}

/**
 * @brief 循环的标签自己发（不用 simple_ast 的 `whileLoop` / `doWhileLoop`）。
 *
 * 那两句话的标签是它内部现生成的，外面拿不到；而 `break` / `continue` 要的正是
 * "**具体跳到哪一行**"。所以这里三个循环（while / do-while / for）都手写标签：
 * 进循环时往 `g_blockStack` 压一条 `{break → 出口, continue → 下一轮的起点}`，
 * 循环体降级完再弹掉 —— 于是嵌套循环天然各认自己那层（最内层生效）。
 *
 * 三条共用一张形状：**出口在最后一行**，中间是"条件不成立就跳出口"。
 * 只有 `do-while` 的判定在循环体**后面**（先做一遍），所以它的 continue 目标
 * 是"紧接着判条件"的那个标签。
 */
/**
 * @brief `loop body`：无条件回跳；有值 break 写入返回槽后跳到出口。
 * @param id 待查找或操作的对象编号。
 * @param valueName 保存循环结果值的临时变量名称。
 * @retval 生成的低层语法节点。
 */
simple_ast::NodePtr LowerLoop(int id, Memory::String *valueName)
{
    const MNode node = N(id);
    // 语句位置的 loop 在外层已有值出口时不再截获带值 break。
    const bool transparent = valueName == nullptr && node.s.empty() && FindBreakContext(true) != nullptr;
    const std::shared_ptr<MType> type = transparent ? T(M_TypeVoid()) : LoopValueType(id);
    // 结果收集已报告名称或分支错误时，不再进入块体制造后续诊断。
    if (!g_error.empty()) return Bad();
    const Memory::String loopLabel = NewBlockLabel("loop");
    const Memory::String breakLabel = NewBlockLabel("brk");
    Memory::Vector<simple_ast::NodePtr> out;
    Memory::String slot;
    if (type && type->kind != MType::Void)
    {
        slot = AllocateName("__chali_loop_value");
        out.push_back(simple_ast::Node::declare(slot, LowerType(type, slot)));
        if (type->kind == MType::Struct) out.push_back(DefaultInit(slot, type));
    }
    if (g_borrowFlowCapture && !slot.empty()) g_borrowFlowCapture->valueLoops.insert(id);

    g_blockStack.push_back(
        MBlockContext{ breakLabel, !node.s.empty() ? Memory::String() : loopLabel, g_ownedScopes.size(), slot, type, !node.s.empty(), g_scope.size() - 1u, node.returnExpression });
    const simple_ast::NodePtr body = LowerStmt(node.kids[0]);
    g_blockStack.pop_back();

    out.push_back(simple_ast::Node::label(loopLabel));
    out.push_back(body);
    if (node.s.empty()) out.push_back(simple_ast::Node::jump(loopLabel));
    out.push_back(simple_ast::Node::label(breakLabel));
    if (valueName != nullptr) *valueName = slot;
    return simple_ast::Node::block(out);
}

/**
 * @brief 判断端点是否为可折叠整型常量，不把动态表达式误报为常量错误。
 * @param id 表达式。
 * @param active 已访问节点。
 * @param negativeLiteral 是否位于直接取负路径，允许 int64 最小值的幅值。
 * @return 可无副作用求值时返回真。
 */
bool IntervalConstantTree(int id, Memory::Set<int> &active, bool negativeLiteral)
{
    if (id < 0 || active.size() >= 128 || !active.insert(id).second) return false;
    const auto node = N(id);
    bool result = node.kind == MNode::Int;
    if (result && !node.unsignedLiteral && !node.s.empty() && node.s.find_first_not_of("0123456789") == Memory::String::npos)
    {
        errno = 0;
        const auto magnitude = std::strtoull(node.s.c_str(), nullptr, 10);
        const auto maximum = static_cast<std::uint64_t>(LLONG_MAX) + (negativeLiteral ? 1u : 0u);
        if (errno == ERANGE || magnitude > maximum)
        {
            active.erase(id);
            return Fail("区间端点的十进制字面量超出 int64 取值范围：" + node.s);
        }
        // 词法层的 strtoll 会饱和；直接取负的最小值必须保留真实位模式。
        if (negativeLiteral && magnitude == static_cast<std::uint64_t>(LLONG_MAX) + 1u)
            g_nodes[id].i = LLONG_MIN;
    }
    if (node.kind == MNode::Var || node.kind == MNode::EnumCtor)
    {
        const auto bound = g_constantBindings.find(id);
        const auto variable = bound != g_constantBindings.end() ? &bound->second :
                              node.kind == MNode::Var           ? FindNodeVar(node) :
                                                                  FindQualifiedGlobal(node.s, node.s2);
        result = variable && variable->isConst && variable->constantExpression >= 0 && IntervalConstantTree(variable->constantExpression, active);
    }
    else if (node.kind == MNode::Un || node.kind == MNode::Bin || node.kind == MNode::Cast)
    {
        result = !node.kids.empty();
        const bool negativeChild = node.kind == MNode::Un && (node.s == "-" ? !negativeLiteral : node.s == "+" && negativeLiteral);
        for (const int child : node.kids) result = result && IntervalConstantTree(child, active, negativeChild);
    }
    active.erase(id);
    return result;
}

/**
 * @brief 创建按整数类型检查端点的内部惰性迭代器。
 * @param id 区间节点。
 * @return 检查后调用类型专用工厂，端点从左到右各求值一次。
 */
simple_ast::NodePtr LowerInterval(int id)
{
    using simple_ast::Node;
    using simple_ast::Type;
    const auto node = N(id);
    const auto element = IntervalElementType(id);
    if (!element || (element->kind != MType::Int && element->kind != MType::Char))
        return Fail("内建区间的元素必须是整数或 char"), Bad();
    const auto signedType = Type::i64();
    const auto unsignedType = Type::make(Type::Kind::Uint, 64);
    const bool unsignedElement = element->isUnsigned || element->kind == MType::Char;
    const int elementBits = element->kind == MType::Char ? 8 : element->bits;
    const auto maximum = unsignedElement ? (elementBits == 64 ? UINT64_MAX : (std::uint64_t(1) << elementBits) - 1) :
                                           (element->bits == 64 ? std::uint64_t(LLONG_MAX) : (std::uint64_t(1) << (element->bits - 1)) - 1);
    const long long minimum = unsignedElement ? 0 : element->bits == 64 ? LLONG_MIN :
                                                                          -(1ll << (element->bits - 1));
    const auto message = "for-in 区间端点超出 " + Describe(element) + " 的取值范围（" + (element->kind == MType::Char ? Memory::String("0..255") : IntegerRangeText(element)) + "）";
    const auto reverseMessage = Memory::String("for-in 区间下界不能大于上界");
    Memory::Vector<simple_ast::NodePtr> prefix;
    Memory::Vector<Memory::String> bounds;
    Memory::Vector<MConstant> constants(2);
    bool constantBounds[2] = { false, false };
    for (std::size_t index = 0; index < 2; ++index)
    {
        const int endpoint = node.kids[index];
        auto sourceType = ExprType(endpoint);
        if (!sourceType || (sourceType->kind != MType::Int && sourceType->kind != MType::Char))
            return Fail("内建区间端点必须是整数或 char"), Bad();
        const bool sourceUnsigned = sourceType->isUnsigned || sourceType->kind == MType::Char || (unsignedElement && elementBits == 64 && N(endpoint).kind == MNode::Int && N(endpoint).i < 0);
        Memory::Set<int> active;
        constantBounds[index] = IntervalConstantTree(endpoint, active);
        if (!g_error.empty()) return Bad();
        if (constantBounds[index])
        {
            if (!EvaluateConstant(endpoint, &constants[index])) return Bad();
            const auto value = constants[index].integer;
            const bool fits = sourceUnsigned ? std::uint64_t(value) <= maximum :
                                               (unsignedElement ? value >= 0 && std::uint64_t(value) <= maximum : value >= minimum && value <= static_cast<long long>(maximum));
            if (!fits) return Fail(message), Bad();
        }
        const auto raw = AllocateName("__chali_interval_bound");
        const auto converted = AllocateName("__chali_interval_value");
        const auto comparisonType = sourceUnsigned ? unsignedType : signedType;
        prefix.push_back(Node::declare(raw, comparisonType));
        prefix.push_back(Node::assign(raw, Node::cast(LowerExpr(endpoint), comparisonType)));
        simple_ast::NodePtr valid;
        if (sourceUnsigned)
            valid = Node::binary("<=", Node::var(raw), Node::cast(Node::i(ConstantBits(maximum)), unsignedType));
        else
        {
            valid = Node::binary(">=", Node::var(raw), Node::i(minimum));
            if (!(unsignedElement && elementBits == 64))
                valid = Node::binary("&&", valid, Node::binary("<=", Node::var(raw), Node::i(static_cast<long long>(maximum))));
        }
        prefix.push_back(Node::ifThenElse(valid, Node::block({}),
                                          Node::exprStmt(Node::call("__std_panic", { "message" }, { Node::str(message) }))));
        prefix.push_back(Node::declare(converted, LowerType(element, converted)));
        prefix.push_back(Node::assign(converted, Node::cast(Node::var(raw), LowerType(element, converted))));
        bounds.push_back(converted);
    }
    if (constantBounds[0] && constantBounds[1])
    {
        const bool reverse = unsignedElement ? std::uint64_t(constants[0].integer) > std::uint64_t(constants[1].integer) :
                                               constants[0].integer > constants[1].integer;
        if (reverse) return Fail(reverseMessage), Bad();
    }
    prefix.push_back(Node::ifThenElse(Node::binary("<=", Node::var(bounds[0]), Node::var(bounds[1])), Node::block({}),
                                      Node::exprStmt(Node::call("__std_panic", { "message" }, { Node::str(reverseMessage) }))));
    const auto factory = "std::lang::__interval_" + Describe(element);
    const auto signature = g_funcs.find(factory);
    if (signature == g_funcs.end()) return Fail("内部区间迭代器工厂没有登记：" + factory), Bad();
    Memory::Vector<Memory::String> names;
    for (const auto &parameter : signature->second.params) names.push_back(parameter.emitted);
    return Node::sequence(std::move(prefix), Node::call(factory, names,
                                                        { Node::var(bounds[0]), Node::var(bounds[1]), Node::boolean((node.i & 1) != 0) }));
}

/**
 * @brief 为可迭代对象生成遍历循环。
 * @param id 待查找或操作的对象编号。
 * @retval 生成的低层语法节点。
 */
simple_ast::NodePtr LowerForeach(int id)
{
    const MNode node = N(id);
    RewriteRangeCall(node.kids[0]);
    const bool intervalSource = N(node.kids[0]).kind == MNode::Range;
    if (intervalSource && node.type >= 0 && N(node.kids[0]).s != "typed") g_nodes[node.kids[0]].type = node.type;
    const std::shared_ptr<MType> sourceType = ExprType(node.kids[0]);
    if ((IsContainer(sourceType) && sourceType->name == "vec") || sourceType->kind == MType::Array)
    {
        const std::shared_ptr<MType> element = node.type >= 0 ? T(node.type) : sourceType->elem;
        if (!SameType(element, sourceType->elem))
            return Fail("for-in 变量类型与 Vector<T> 元素不一致：写的是 " + Describe(element) + "，容器元素是 " + Describe(sourceType->elem)), Bad();
        const Memory::String source = AllocateName("__chali_foreach_vector");
        const Memory::String index = AllocateName("__chali_foreach_index");
        const Memory::String loopLabel = NewBlockLabel("foreach_vector");
        const Memory::String continueLabel = NewBlockLabel("foreach_next");
        const Memory::String breakLabel = NewBlockLabel("foreach_end");
        Memory::Vector<simple_ast::NodePtr> out{
            simple_ast::Node::declare(source, sourceType->kind == MType::Array ? simple_ast::Type::i64() : LowerType(sourceType, source)),
            simple_ast::Node::assign(source, LowerExpr(node.kids[0])),
            simple_ast::Node::declare(index, simple_ast::Type::i64()),
            simple_ast::Node::assign(index, simple_ast::Node::i(0)),
        };
        PushScope();
        PushOwnedScope();
        if (IsFreshValue(node.kids[0]))
        {
            if (sourceType->kind == MType::Array)
                MarkOwnedArray(source, sourceType);
            else
                MarkOwned(source);
        }
        const Memory::String item = DeclareVar(node.s, element, Memory::String(), false, NodeBindingKey(node), node.bindingIdentity);
        MVar *binding = FindNodeVar(node);
        binding->elementReference = true;
        binding->isConst = IsReadOnlyAccess(node.kids[0]);
        out.push_back(simple_ast::Node::declare(item, simple_ast::Type::i64()));
        const std::size_t loopDepth = g_ownedScopes.size();
        PushOwnedScope();
        if (sourceType->kind != MType::Array)
            g_ownedScopes.back().finalizers.push_back(simple_ast::Node::exprStmt(simple_ast::Node::call(
                "__std_vec_unref", { "__chali_v", "__chali_address" },
                { simple_ast::Node::var(source), simple_ast::Node::var(item) })));
        g_blockStack.push_back(MBlockContext{ breakLabel, continueLabel, loopDepth, {}, nullptr });
        const simple_ast::NodePtr body = LowerStmt(node.kids[1]);
        g_blockStack.pop_back();
        const MOwnedScope iteration = g_ownedScopes.back();
        g_ownedScopes.pop_back();
        const MOwnedScope owned = g_ownedScopes.back();
        g_ownedScopes.pop_back();
        PopScope();
        simple_ast::NodePtr length;
        simple_ast::NodePtr value;
        if (sourceType->kind == MType::Array)
        {
            std::int64_t count = 1;
            for (const int dim : sourceType->dims) count *= dim;
            length = IsRuntimeArray(sourceType) ? simple_ast::Node::loadInt(simple_ast::Node::binary("-", simple_ast::Node::var(source), simple_ast::Node::i(8))) : simple_ast::Node::i(count);
            const auto address = simple_ast::Node::offset(simple_ast::Node::var(source),
                                                          simple_ast::Node::var(index), ElementBytes(element));
            value = address;
        }
        else
        {
            length = simple_ast::Node::call("__std_vec_len", { "__chali_v" }, { simple_ast::Node::var(source) });
            value = simple_ast::Node::call("__std_vec_ref", { "__chali_v", "__chali_i", "__chali_t", "__chali_o", "__chali_f" },
                                           { simple_ast::Node::var(source), simple_ast::Node::var(index),
                                             simple_ast::Node::i(IsStandardOpcode(element, "String") ? 1 : 0), ContainerStorageFlag(element),
                                             simple_ast::Node::i(element->kind == MType::Double ? 1 : 0) });
        }
        out.push_back(simple_ast::Node::label(loopLabel));
        out.push_back(simple_ast::Node::ifThenElse(
            simple_ast::Node::binary("<", simple_ast::Node::var(index), length),
            simple_ast::Node::block({}), simple_ast::Node::jump(breakLabel)));
        out.push_back(simple_ast::Node::assign(item, value));
        out.push_back(body);
        AppendScopeDrops(iteration, Memory::String(), &out);
        out.push_back(simple_ast::Node::label(continueLabel));
        out.push_back(simple_ast::Node::assign(
            index, simple_ast::Node::binary("+", simple_ast::Node::var(index),
                                            simple_ast::Node::i(1))));
        out.push_back(simple_ast::Node::jump(loopLabel));
        out.push_back(simple_ast::Node::label(breakLabel));
        AppendScopeDrops(owned, Memory::String(), &out);
        return simple_ast::Node::block(out);
    }
    const MIteratorDecl *iterator = FindIterator(sourceType);
    if (iterator == nullptr)
        return Fail("for-in 的源表达式没有实现 Iterator<R>：" + Describe(sourceType)), Bad();
    const bool pointerIterator = iterator->element->kind == MType::Ptr;
    const auto iteratedType = pointerIterator ? iterator->element->elem : iterator->element;
    const std::shared_ptr<MType> element = node.type >= 0 ? T(node.type) : iteratedType;
    const bool reference = pointerIterator && SameType(element, iteratedType);
    if (!reference && !SameType(element, iterator->element))
        return Fail("for-in 变量类型与 Iterator<R> 不一致：写的是 " + Describe(element) + "，迭代器给出 " + Describe(iterator->element)), Bad();
    const MEnumVariantDef *value = nullptr;
    if (!OptionVariants(iterator->result, &value, nullptr))
        return Fail("Iterator<R> 的返回类型不是 Option<R>"), Bad();
    const MField *tagField = FindField(iterator->result->name, "__tag");
    const MField *valueField = FindField(iterator->result->name, value->payloads[0].storageName);
    if (tagField == nullptr || valueField == nullptr)
        return Fail("Iterator<R> 的 Option 布局不完整"), Bad();

    const Memory::String source = AllocateName("__chali_foreach_source");
    const Memory::String index = AllocateName("__chali_foreach_index");
    const Memory::String result = AllocateName("__chali_foreach_result");
    const Memory::String loopLabel = NewBlockLabel("foreach_loop");
    const Memory::String continueLabel = NewBlockLabel("foreach_next");
    const Memory::String breakLabel = NewBlockLabel("foreach_end");
    const Memory::String breakCleanup = NewBlockLabel("foreach_break_cleanup");
    PushScope();
    PushOwnedScope();
    Memory::Vector<simple_ast::NodePtr> out{
        simple_ast::Node::declare(source, LowerType(sourceType, source))
    };
    if (sourceType->kind == MType::Struct && IsFreshValue(node.kids[0]))
    {
        // 返回暂存属于被调函数，嵌套迭代可能再次调用同一工厂；立即接管到独立存储。
        out.push_back(DefaultInit(source, sourceType, false));
        out.push_back(MoveStructValue(simple_ast::Node::var(source), LowerExpr(node.kids[0]), sourceType));
    }
    else
        out.push_back(simple_ast::Node::assign(source, LowerExpr(node.kids[0])));
    out.push_back(simple_ast::Node::declare(index, simple_ast::Type::i64()));
    out.push_back(simple_ast::Node::assign(index, simple_ast::Node::i(0)));
    out.push_back(simple_ast::Node::declare(result, LowerType(iterator->result, result)));
    if (IsFreshValue(node.kids[0])) MarkDestructor(source, sourceType);
    const Memory::String item = DeclareVar(node.s, element, Memory::String(), false, NodeBindingKey(node), node.bindingIdentity);
    const bool captureStorage = !reference && element->kind == MType::Int && g_addressTakenNames.count(node.s);
    FindNodeVar(node)->elementReference = reference || captureStorage;
    FindNodeVar(node)->isConst = reference && iterator->element->readOnly;
    out.push_back(simple_ast::Node::declare(item, reference || captureStorage ? simple_ast::Type::i64() : LowerType(element, item)));
    if (captureStorage)
    {
        out.push_back(simple_ast::Node::assign(item, simple_ast::Node::alloc(simple_ast::Node::i(ElementBytes(element)))));
        MarkOwned(item);
    }
    if (intervalSource && g_borrowFlowCapture) g_borrowFlowCapture->foreachBindings[id] = { item, {} };
    if (!reference && element->kind == MType::Struct)
        out.push_back(DefaultInit(item, element));
    // Option 的载荷移交给循环变量；每轮结束清理，return item 时允许转移所有权。
    PushOwnedScope();
    if (!reference)
    {
        MarkDestructor(item, element);
        if (IsOwnedKind(element))
        {
            if (element->kind == MType::Array)
                MarkOwnedArray(item, element);
            else
                MarkOwned(item);
        }
    }
    g_blockStack.push_back(
        MBlockContext{ breakCleanup, continueLabel, g_ownedScopes.size(), {}, nullptr });
    const simple_ast::NodePtr body = LowerStmt(node.kids[1]);
    g_blockStack.pop_back();
    const MOwnedScope iterationOwned = g_ownedScopes.back();
    g_ownedScopes.pop_back();
    const MOwnedScope owned = g_ownedScopes.back();
    g_ownedScopes.pop_back();
    PopScope();

    out.push_back(simple_ast::Node::label(loopLabel));
    out.push_back(simple_ast::Node::assign(
        result, simple_ast::Node::call(iterator->label, iterator->callNames,
                                       { simple_ast::Node::var(source),
                                         simple_ast::Node::var(index) })));
    const simple_ast::NodePtr tag = simple_ast::Node::loadFrom(
        simple_ast::Node::offset(simple_ast::Node::var(result),
                                 simple_ast::Node::i(tagField->slot), 8),
        LowerType(T(M_TypeIntBits(32, 0)), "foreach tag"));
    out.push_back(simple_ast::Node::ifThenElse(
        simple_ast::Node::binary("==", tag, simple_ast::Node::i(value->tag)),
        simple_ast::Node::block({}), simple_ast::Node::jump(breakLabel)));
    const simple_ast::NodePtr payloadAddress = simple_ast::Node::offset(
        simple_ast::Node::var(result), simple_ast::Node::i(valueField->slot), 8);
    if (reference)
        out.push_back(simple_ast::Node::assign(item, simple_ast::Node::loadInt(payloadAddress)));
    else if (element->kind == MType::Struct)
        /* 载荷是借来的（下一轮迭代就换 / 释放），绑进循环变量时深拷贝一份。 */
        out.push_back(DeepCopyStructValue(simple_ast::Node::var(item), payloadAddress, element));
    else if (captureStorage)
        out.push_back(simple_ast::Node::storeAt(simple_ast::Node::var(item),
                                                simple_ast::Node::loadFrom(payloadAddress, LowerType(element, item)), LowerType(element, item)));
    else
        out.push_back(simple_ast::Node::assign(item,
                                               simple_ast::Node::loadFrom(payloadAddress, LowerType(element, item))));
    /*
     * 载荷已经**按值**拷进循环变量，迭代器返回的那个 Option 里那份是该由我们收尾的
     * （不然每轮漏一个字符串 / 对象 / 结构体叶子）。引用迭代器例外：`item` 拿的是
     * 载荷的**地址**，释放它就等于把目标还掉了。
     */
    if (!reference && StructHasOwnedLeaves(iterator->result->name))
        if (const simple_ast::NodePtr release =
                MakeAggregateRelease(iterator->result->name, simple_ast::Node::var(result)))
            out.push_back(release);
    out.push_back(body);
    out.push_back(simple_ast::Node::label(continueLabel));
    AppendScopeDrops(iterationOwned, Memory::String(), &out);
    out.push_back(simple_ast::Node::assign(
        index, simple_ast::Node::binary("+", simple_ast::Node::var(index),
                                        simple_ast::Node::i(1))));
    out.push_back(simple_ast::Node::jump(loopLabel));
    out.push_back(simple_ast::Node::label(breakCleanup));
    AppendScopeDrops(iterationOwned, Memory::String(), &out);
    out.push_back(simple_ast::Node::label(breakLabel));
    AppendScopeDrops(owned, Memory::String(), &out);
    return simple_ast::Node::block(out);
}

/**
 * @brief `while (cond) body`：continue → 重新判条件。
 * @param id 待查找或操作的对象编号。
 * @retval 生成的低层语法节点。
 */
simple_ast::NodePtr LowerWhile(int id)
{
    const MNode &node = N(id);
    if (!CheckBoolCondition(node.kids[0], "while 条件")) return Bad();
    const Memory::String loopLabel = NewBlockLabel("loop");
    const Memory::String breakLabel = NewBlockLabel("brk");

    g_blockStack.push_back(
        MBlockContext{ breakLabel, loopLabel, g_ownedScopes.size(), {}, nullptr });
    const simple_ast::NodePtr body = LowerStmt(node.kids[1]);
    g_blockStack.pop_back();

    Memory::Vector<simple_ast::NodePtr> conditionFresh;
    const simple_ast::NodePtr condition = LowerConditionWithFresh(node.kids[0], &conditionFresh);
    Memory::Vector<simple_ast::NodePtr> out = { simple_ast::Node::label(loopLabel) };
    out.insert(out.end(), conditionFresh.begin(), conditionFresh.end());
    out.insert(out.end(), {
                              /* 条件为假 → 出口；为真 → then 分支（循环体），完了自然落到 `j loop`。 */
                              simple_ast::Node::ifThenElse(condition, body,
                                                           simple_ast::Node::jump(breakLabel)),
                              simple_ast::Node::jump(loopLabel),
                              simple_ast::Node::label(breakLabel),
                          });
    return simple_ast::Node::block(out);
}

/**
 * @brief `do body while (cond);`：先做一遍，continue → 判条件。
 * @param id 待查找或操作的对象编号。
 * @retval 生成的低层语法节点。
 */
simple_ast::NodePtr LowerDoWhile(int id)
{
    const MNode &node = N(id);
    if (!CheckBoolCondition(node.kids[1], "do-while 条件")) return Bad();
    const Memory::String loopLabel = NewBlockLabel("loop");
    const Memory::String contLabel = NewBlockLabel("cont");
    const Memory::String breakLabel = NewBlockLabel("brk");

    g_blockStack.push_back(
        MBlockContext{ breakLabel, contLabel, g_ownedScopes.size(), {}, nullptr });
    const simple_ast::NodePtr body = LowerStmt(node.kids[0]);
    g_blockStack.pop_back();

    Memory::Vector<simple_ast::NodePtr> conditionFresh;
    const simple_ast::NodePtr condition = LowerConditionWithFresh(node.kids[1], &conditionFresh);
    Memory::Vector<simple_ast::NodePtr> out = {
        simple_ast::Node::label(loopLabel),
        body,
        simple_ast::Node::label(contLabel), /* continue 落这儿：紧接着就是判条件 */
    };
    out.insert(out.end(), conditionFresh.begin(), conditionFresh.end());
    out.insert(out.end(), {
                              /* 条件为真回开头（then = 回跳），为假就落到出口。 */
                              simple_ast::Node::ifThenElse(condition,
                                                           simple_ast::Node::jump(loopLabel), nullptr),
                              simple_ast::Node::label(breakLabel),
                          });
    return simple_ast::Node::block(out);
}

/**
 * @brief `for (init; cond; inc) body`：四段都可能有，都可能有空（`for (;;)` 那种）。
 *
 * 顺序是 init → [loop: cond 假就出 → body → cont: inc] → 回 loop。
 * `continue` 落在 **cont**（自增那一步）上 —— 这就是 for 不展开成 while 的原因：
 * 展开的话 continue 会把自增一起跳过去，`for (i = 0; i < n; i = i + 1)` 就成了死循环。
 * @param id 待查找或操作的对象编号。
 * @retval 生成的低层语法节点。
 */
simple_ast::NodePtr LowerFor(int id)
{
    const MNode &node = N(id);
    const int initId = node.kids[0];
    const int condId = node.kids[1];
    const int incId = node.kids[2];
    const int bodyId = node.kids[3];
    const Memory::String loopLabel = NewBlockLabel("loop");
    const Memory::String contLabel = NewBlockLabel("cont");
    const Memory::String breakLabel = NewBlockLabel("brk");

    Memory::Vector<simple_ast::NodePtr> out;
    /* `for (int i = 0; …)` 里的 `i` 归**这一层循环**（外面看不见，同名可以遮蔽）。 */
    PushScope();
    if (initId >= 0) out.push_back(LowerStmt(initId)); /* 初始化：只走一遍 */
    out.push_back(simple_ast::Node::label(loopLabel));
    if (condId >= 0)
    {
        if (!CheckBoolCondition(condId, "for 条件"))
        {
            PopScope();
            return Bad();
        }
        /* "条件不成立就跳出口"：then 空着，else 才是跳转。 */
        Memory::Vector<simple_ast::NodePtr> conditionFresh;
        const simple_ast::NodePtr condition = LowerConditionWithFresh(condId, &conditionFresh);
        out.insert(out.end(), conditionFresh.begin(), conditionFresh.end());
        out.push_back(simple_ast::Node::ifThenElse(condition, nullptr,
                                                   simple_ast::Node::jump(breakLabel)));
    }

    g_blockStack.push_back(
        MBlockContext{ breakLabel, contLabel, g_ownedScopes.size(), {}, nullptr });
    const simple_ast::NodePtr body = LowerStmt(bodyId);
    g_blockStack.pop_back();
    out.push_back(body);

    out.push_back(simple_ast::Node::label(contLabel));
    if (incId >= 0) out.push_back(LowerStmt(incId)); /* 自增：continue 也走这儿 */
    out.push_back(simple_ast::Node::jump(loopLabel));
    out.push_back(simple_ast::Node::label(breakLabel));
    PopScope();
    return simple_ast::Node::block(out);
}

/**
 * @brief 解析旧式百分号格式串，供返回字符串的 `format` 使用。
 *
 * 只支持 `%d`（int64）与 `%f`（float64，三位小数）与 `%%`；输出**全部落在文本流**上：
 * 排版交给虚拟机里的 C++ 运行时（`__std_print`），语言侧只检查格式串与实参类型。
 * 这么绕是因为 stackvm 把 `write` 的数值与 `print` / `putc` 的文本分成两个桶，
 * 混着用会让标准输出顺序乱掉。
 */
/**
 * @brief 字符串 / 格式串里的转义：`\n \t \r \0 \a \b \f \v \\ \" \'`。
 *
 * 字符串字面量与格式串走**同一个**函数，避免"格式串里 `\n` 生效、
 * 普通字符串里不生效"这种不一致。不认识的转义直接报错（别悄悄吞掉）。
 * 注意 `%` 不在这儿处理：`%%` 是 `format` 的格式语法，字符串层面原样保留。
 *
 * @param raw 源码里的原文（不含两侧引号）。
 * @return 转义处理后的文本。
 */
// ABI paths accept raw separators as well as escaped backslashes. Unlike
// ordinary CHALI strings, \n / \t in a path never become control characters.
/**
 * @brief 解码外部 ABI 库路径，保留路径中的反斜杠。
 * @param raw 未解码的字面量文本。
 * @retval 生成或读取的结果文本。
 */
Memory::String DecodeAbiLibraryPath(const Memory::String &raw)
{
    Memory::String path;
    std::size_t index = 0;
    if (raw.size() >= 2 && raw[0] == '\\' && raw[1] == '\\')
    {
        path = "//";
        index = raw.size() >= 4 && raw[2] == '\\' && raw[3] == '\\' ? 4 : 2;
    }
    for (; index < raw.size(); ++index)
    {
        if (raw[index] != '\\')
        {
            path.push_back(raw[index]);
            continue;
        }
        if (index + 1 < raw.size() && raw[index + 1] == '"')
        {
            path.push_back('"');
            ++index;
        }
        else
        {
            path.push_back('/');
            if (index + 1 < raw.size() && raw[index + 1] == '\\') ++index;
        }
    }
    return path;
}

/**
 * @brief 解码字符串字面量中的转义序列。
 * @param raw 未解码的字面量文本。
 * @retval 生成或读取的结果文本。
 */
Memory::String UnescapeText(const Memory::String &raw)
{
    std::string output, error;
    if (!chali_literal::decodeString(std::string_view(raw.data(), raw.size()), output, error))
        Fail(Memory::String(error.data(), error.size()));
    return Memory::String(output.data(), output.size());
}

/**
 * @brief 实参表 + 格式串一起包成一次内部原生调用。
 * @param native 目标运行时辅助函数的名称。
 * @param format 格式字符串或格式描述节点。
 * @param values 输入值列表或接收读取值的输出列表。
 * @retval 生成的低层语法节点。
 */
simple_ast::NodePtr BuildFormatCall(const char *native, const Memory::String &format,
                                    const Memory::Vector<simple_ast::NodePtr> &values)
{
    Memory::Vector<Memory::String> names;
    Memory::Vector<simple_ast::NodePtr> arguments;
    names.emplace_back("__chali_fmt");
    arguments.push_back(simple_ast::Node::str(format)); /* 第一个实参永远是格式串 */
    for (std::size_t index = 0; index < values.size(); ++index)
    {
        names.emplace_back("__chali_fa" + Memory::toString(index + 1u));
        arguments.push_back(values[index]);
    }
    return simple_ast::Node::call(native, names, arguments);
}

/**
 * @brief 将格式化参数转换为字符串值。
 * @param argId 格式化实参在节点池中的编号。
 * @param format 格式字符串或格式描述节点。
 * @retval 生成的低层语法节点。
 */
simple_ast::NodePtr BraceStringValue(int argId, const Memory::String &format)
{
    const std::shared_ptr<MType> type = ExprType(argId);
    if (IsStandardOpcode(type, "String"))
        return NeedsOwnedTemp(argId) ? OwnedStringTemp(argId) : LowerExpr(argId);
    if (IsStandardOpcode(type, "BigInteger"))
        return OwnedGeneratedString(
            simple_ast::Node::call("__std_bint_str", { "__chali_v" }, { OwnedObjectTemp(argId, type) }));
    if (IsStandardOpcode(type, "BigDecimal"))
        return OwnedGeneratedString(
            simple_ast::Node::call("__std_bfloat_str", { "__chali_v" }, { OwnedObjectTemp(argId, type) }));
    if (type->kind != MType::Struct)
    {
        return OwnedGeneratedString(LowerValueText(argId));
    }
    const auto owner = g_structs.find(type->name);
    if (owner == g_structs.end())
    {
        Fail("找不到类型 " + type->name);
        return Bad();
    }
    const MMethod *plain = nullptr;
    const MMethod *formatted = nullptr;
    for (const MMethod &method : owner->second.methods)
    {
        if (method.superseded || method.source != "toString" || !method.result || !IsStandardOpcode(method.result, "String")) continue;
        if (method.params.empty()) plain = &method;
        if (method.params.size() == 1u && IsStandardOpcode(method.params[0].type, "String"))
            formatted = &method;
    }
    const MMethod *chosen = !format.empty() && formatted != nullptr ? formatted : plain;
    if (chosen == nullptr && formatted != nullptr) chosen = formatted;
    if (chosen == nullptr)
    {
        return OwnedGeneratedString(LowerValueText(argId));
    }
    if (!CheckMethodReceiver(argId, type->name + "." + chosen->name)) return Bad();
    Memory::Vector<Memory::String> names{ "self" };
    Memory::Vector<simple_ast::NodePtr> values{ OwnedAggregateTempFor(argId, type) };
    if (!chosen->params.empty())
    {
        names.push_back(chosen->params[0].emitted);
        values.push_back(simple_ast::Node::str(format));
    }
    return OwnedGeneratedString(
        simple_ast::Node::call(type->name + "." + chosen->name, names, values));
}

/**
 * @brief 把大括号格式串翻成百分号格式串，并把实参降级（`std::lang::print` / `std::lang::format` 共用）。
 *
 * 语言侧只认 `{}` 占位符：`{}` / `{1}` 顺序或显式编号，`{1:2F}` 给浮点保留位数，
 * `{1:b}` / `{1:x}` / `{1:X}` 以二进制 / 十六进制输出整数，
 * `{{` / `}}` 输出字面量大括号。底层 `__std_print` / `__std_str_fmt` 仍是百分号实现，
 * 两者在**编译期**对上：占位符的个数、索引、类型不对都在这里报错，运行时不再检查。
 *
 * @param args 实参表达式（第一个必须是格式串字面量）。
 * @param who 报错文案里怎么称呼调用者。
 * @param target 输出：百分号格式串。
 * @param values 输出：降级后的实参（不含格式串）。
 * @return 没问题返回 true。
 */
bool BuildFormatTarget(const Memory::Vector<int> &args, const char *who,
                       Memory::String &target, Memory::Vector<simple_ast::NodePtr> &values)
{
    if (args.empty() || N(args[0]).kind != MNode::Str)
    {
        Fail(Memory::String(who) + " 的第一个参数必须是字符串字面量");
        return false;
    }
    const Memory::String source = UnescapeText(N(args[0]).s);
    Memory::Vector<bool> used(args.size(), false);
    used[0] = true; /* 0 号是格式串本身，不参与"有没有被用上"的检查 */
    std::size_t sequential = 1u;
    for (std::size_t at = 0; at < source.size();)
    {
        if (source[at] == '{' && at + 1u < source.size() && source[at + 1u] == '{')
        {
            target += '{';
            at += 2u;
            continue;
        }
        if (source[at] == '}' && at + 1u < source.size() && source[at + 1u] == '}')
        {
            target += '}';
            at += 2u;
            continue;
        }
        if (source[at] != '{')
        {
            if (source[at] == '}')
            {
                Fail("格式串存在没有配对的 `}`");
                return false;
            }
            /* 底层复用百分号格式化器；用户文本里的 `%` 必须转义，不能被误认成占位符。 */
            if (source[at] == '%')
                target += "%%";
            else
                target += source[at];
            ++at;
            continue;
        }
        const std::size_t close = source.find('}', at + 1u);
        if (close == Memory::String::npos)
        {
            Fail("格式串占位符缺少 `}`");
            return false;
        }
        const Memory::String field = source.substr(at + 1u, close - at - 1u);
        const std::size_t colon = field.find(':');
        const Memory::String indexText = field.substr(0u, colon);
        const Memory::String spec = colon == Memory::String::npos ? Memory::String() : field.substr(colon + 1u);
        std::size_t index = sequential++;
        if (!indexText.empty())
        {
            for (const unsigned char ch : indexText)
                if (!std::isdigit(ch))
                {
                    Fail("占位符索引必须是从 1 开始的整数");
                    return false;
                }
            index = static_cast<std::size_t>(std::strtoull(indexText.c_str(), nullptr, 10));
        }
        if (index == 0u || index >= args.size())
        {
            Fail("占位符 {" + field + "} 没有对应参数");
            return false;
        }
        used[index] = true;
        const int argId = args[index];
        const std::shared_ptr<MType> type = ExprType(argId);
        if (type->kind == MType::Double)
        {
            if (spec.empty())
                target += "%g";
            else
            {
                Memory::String digits = spec;
                if (!digits.empty() && (digits.back() == 'F' || digits.back() == 'f'))
                    digits.pop_back();
                if (digits.empty() || !std::all_of(digits.begin(), digits.end(), [](unsigned char ch) {
                        return std::isdigit(ch) != 0;
                    }))
                {
                    Fail("浮点格式应写成 `{1:2F}`（2 是保留位数）");
                    return false;
                }
                target += "%." + digits + "f";
            }
            values.push_back(LowerExpr(argId));
        }
        else if (type->kind == MType::Int || type->kind == MType::Bool)
        {
            if (spec == "b")
                target += "%b";
            else if (spec == "x")
                target += "%x";
            else if (spec == "X")
                target += "%X";
            else if (spec.empty())
                target += type->kind == MType::Int && type->isUnsigned ? "%u" : "%d";
            else
            {
                Fail("整数和 bool 的格式仅支持 `:b`、`:x`、`:X`：收到 `:" + spec + "`");
                return false;
            }
            values.push_back(LowerExpr(argId));
        }
        else if (type->kind == MType::Char)
        {
            if (!spec.empty())
            {
                Fail("char 不接受格式说明");
                return false;
            }
            target += "%c";
            values.push_back(LowerExpr(argId));
        }
        else
        {
            target += "%s";
            values.push_back(BraceStringValue(argId, spec));
        }
        at = close + 1u;
    }
    for (std::size_t index = 1u; index < used.size(); ++index)
    {
        if (!used[index])
        {
            Fail(Memory::String(who) + " 的第 " + Memory::toString(index) + " 个参数没有被占位符用到");
            return false;
        }
    }
    return true;
}

/**
 * @brief 为花括号格式字符串生成输出表达式。
 * @param args 按接口约定顺序传入的实参列表。
 * @param newline 是否在输出末尾追加换行。
 * @retval 生成的低层语法节点。
 */
simple_ast::NodePtr BuildBracePrint(const Memory::Vector<int> &args, bool newline)
{
    if (args.size() == 1u && N(args[0]).kind != MNode::Str && IsStandardOpcode(ExprType(args[0]), "String"))
    {
        Memory::Vector<simple_ast::NodePtr> values{ BraceStringValue(args[0], "") };
        return BuildFormatCall("__std_print", newline ? "%s\n" : "%s", values);
    }
    Memory::String target;
    Memory::Vector<simple_ast::NodePtr> values;
    if (!BuildFormatTarget(args, "std::lang::print/std::lang::println", target, values)) return Bad();
    if (newline) target += '\n';
    return BuildFormatCall("__std_print", target, values);
}

/**
 * @brief `std::lang::format("…", 参数…)`：按 `{}` 占位符拼出一个**新串**（规则与 `std::lang::print` 完全一致）。
 *
 * 结果是"当场造出来的新串"（`IsFreshValue` 认 `MNode::Call`），所有权交给通用的那套
 * 机制（`NeedsOwnedTemp` / `OwnedStringTemp` / `MarkOwned`）——**不要**在这儿自己开临时
 * 变量：那样会在拼接链（`text = text + std::lang::format(…)`）里把临时串提前还掉。
 */
simple_ast::NodePtr BuildBraceFormat(const Memory::Vector<int> &args)
{
    Memory::String target;
    Memory::Vector<simple_ast::NodePtr> values;
    if (!BuildFormatTarget(args, "std::lang::format", target, values)) return Bad();
    return BuildFormatCall("__std_str_fmt", target, values);
}

/**
 * @brief 将可变参数封装为运行时参数列表。
 * @param args 按接口约定顺序传入的实参列表。
 * @param begin 范围起点或首个可变实参的索引。
 * @param parameter 可变形参的类型描述。
 * @retval 生成的低层语法节点。
 */
simple_ast::NodePtr PackVariadicArguments(const Memory::Vector<int> &args, std::size_t begin,
                                          const MParam &parameter)
{
    if (!parameter.type || !IsContainer(parameter.type) || parameter.type->name != "vec" || !parameter.type->elem)
    {
        Fail("内部错误：变长参数没有降为 std::lang::List<T>：" + parameter.source);
        return Bad();
    }
    if (g_freshSink == nullptr)
    {
        Fail("变长函数调用必须出现在可管理临时值的语句中");
        return Bad();
    }
    const std::shared_ptr<MType> element = parameter.type->elem;
    Memory::Vector<Memory::String> names{ "__chali_text", "__chali_object" };
    Memory::Vector<simple_ast::NodePtr> values{
        simple_ast::Node::i(IsStandardOpcode(element, "String") ? 1 : 0),
        ContainerStorageFlag(element)
    };
    for (std::size_t index = begin; index < args.size(); ++index)
    {
        names.push_back("__chali_item" + Memory::toString(index - begin));
        /*
         * `__std_vec_pack` 会把每个元素复制进容器，实参自己那份临时值归调用点：
         * 当场造出来的串 / 对象先落地、随块释放，否则变长调用的每个临时元素漏一块
         * （`g("a" + "b")`、`g(std::lang::BigDecimal("2.5"))`）。借来的变量与标量原样传。
         */
        values.push_back(LowerArgumentValue(args[index], element));
    }
    return OwnedObjectTempNode(simple_ast::Node::call("__std_vec_pack", names, values),
                               parameter.type);
}

/**
 * @brief `s[起:止]`：字符串切片（半开区间，越界自动夹到范围内，`止` 省了 = 到末尾）。
 *
 * 切出来的也是**新串**（堆上），所以 `s[0:3] + s[3:]` 这种拼法很自然。
 */
simple_ast::NodePtr LowerSlice(int id)
{
    const MNode &node = N(id);
    const std::shared_ptr<MType> base = ExprType(node.kids[0]);
    if (!base || !IsStandardOpcode(base, "String"))
    {
        Fail("切片（`s[起:止]`）只能用在 std::lang::String 上：现在切的是 " + (base ? Describe(base) : Memory::String("?")));
        return Bad();
    }
    if (!CheckContainerIndex(node.kids[1], "切片起点") || (node.kids[2] >= 0 && !CheckContainerIndex(node.kids[2], "切片终点"))) return Bad();
    Memory::Vector<simple_ast::NodePtr> values{ LowerExpr(node.kids[0]), LowerExpr(node.kids[1]) };
    /* `止` 省了就是"到末尾"：给 -1，运行时按串长收尾。 */
    values.push_back(node.kids[2] >= 0 ? LowerExpr(node.kids[2]) : simple_ast::Node::i(-1));
    return simple_ast::Node::call("__std_str_sub", { "__chali_s", "__chali_b", "__chali_e" }, values);
}

/**
 * 调用：内置函数（Add/Index/…）走运算符那条路，普通函数 / 方法走 `(param …, call …)`。
 */
