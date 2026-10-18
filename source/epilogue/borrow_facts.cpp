/* 类型和借用来源的事实适配层：只记录和传播来源信息；检查器在语义结束后运行。 */
/**
 * @brief 判断具名结果是否含借用引用，以延长其临时来源的生命周期。
 * @param type 结果类型。
 * @param visited 已检查的聚合，避免重复递归。
 * @param includeObject 是否将 Any 的隐藏引用也计入借用。
 * @return 引用或包含引用元素 / 字段时为 true。
 */
bool ContainsBorrowedReferences(const std::shared_ptr<MType> &type,
                                Memory::Set<Memory::String> *visited, bool includeObject = true)
{
    if (!type) return false;
    // Any 可以装入非拥有引用，复制盒子并不会延长引用来源的生命周期。
    if (type->kind == MType::Func || type->kind == MType::Ptr || (includeObject && type->kind == MType::Obj)) return true;
    if (IsTupleType(type))
    {
        for (const auto &element : type->paramTypes)
            if (ContainsBorrowedReferences(element, visited, includeObject)) return true;
        return false;
    }
    if (type->kind == MType::Struct)
    {
        if (!visited->insert(type->name).second) return false;
        const auto definition = g_structs.find(type->name);
        if (definition != g_structs.end())
            for (const auto &field : definition->second.fields)
                if (ContainsBorrowedReferences(field.type, visited, includeObject)) return true;
        return false;
    }
    if (IsContainer(type) || type->kind == MType::Array)
        return ContainsBorrowedReferences(type->elem, visited, includeObject) || ContainsBorrowedReferences(type->elem2, visited, includeObject);
    return false;
}

/**
 * @brief 判断类型是否含引用，每次查询使用独立的递归访问集。
 * @param type 类型。
 * @return 是否包含借用。
 */
bool HasBorrowedValue(const std::shared_ptr<MType> &type)
{
    Memory::Set<Memory::String> visited;
    return ContainsBorrowedReferences(type, &visited);
}

/**
 * @brief 判断类型是否只能经 Any 隐藏借用。
 * @param type 形参类型。
 * @return 没有显式引用契约的装箱借用返回 true。
 */
bool HasOpaqueObjectBorrow(const std::shared_ptr<MType> &type)
{
    Memory::Set<Memory::String> explicitTypes;
    return HasBorrowedValue(type) && !ContainsBorrowedReferences(type, &explicitTypes, false);
}

/**
 * @brief 记录装箱实参的引用来源，不在语义分析中判断合法性。
 * @param id 实参。
 * @param type 形参类型。
 * @return 无。
 */
void RecordObjectArgumentBorrow(int id, const std::shared_ptr<MType> &type)
{
    if (HasOpaqueObjectBorrow(type))
        g_borrowFacts.push_back({ magel_borrow_checker::Fact::ObjectArgument, BorrowSourceDepth(id), 0u,
                                  g_reductionLine, "（" + NameOfExpr(id) + "，目标 " + Describe(type) + "，所属类型 " + g_owner + "）" });
}

/**
 * @brief 查找表达式的具名存储根。
 * @param id 成员或下标表达式。
 * @return 对应变量，非具名来源返回空。
 */
MVar *BorrowRootVariable(int id)
{
    if (id < 0) return nullptr;
    const auto &node = N(id);
    if (node.kind == MNode::Var) return FindIn(node.module, node.s);
    if ((node.kind == MNode::Member || node.kind == MNode::Arrow || node.kind == MNode::Index || node.kind == MNode::Cast || node.kind == MNode::Un) && !node.kids.empty()) return BorrowRootVariable(node.kids[0]);
    return nullptr;
}

/**
 * @brief 解析直接别名指向的具名拥有值。
 * @param variable 引用变量。
 * @return 来源仍可见时返回变量。
 */
MVar *BorrowAliasOwner(const MVar *variable)
{
    if (!variable || variable->borrowOwner.empty()) return nullptr;
    for (auto &scope : g_scope)
        for (auto &entry : scope)
            if (entry.second.emitted == variable->borrowOwner) return &entry.second;
    return nullptr;
}

/**
 * @brief 取得变量存储寿命，按值形参和局部槽不能跨函数返回。
 * @param variable 变量。
 * @return 作用域深度。
 */
std::size_t BorrowVariableDepth(const MVar *variable)
{
    if (!variable) return 2u;
    for (std::size_t index = 0; index < g_scope.size(); ++index)
        for (const auto &entry : g_scope[index])
            if (&entry.second == variable) return index == 0u ? 0u : std::max<std::size_t>(2u, index);
    return 2u;
}

/**
 * @brief 计算引用实际指向的存储寿命。
 * @param variable 变量。
 * @return 来源深度。
 */
std::size_t BorrowStorageDepth(const MVar *variable)
{
    if (variable && variable->type && variable->type->kind == MType::Ptr) return variable->borrowDepth;
    return BorrowVariableDepth(variable);
}

/**
 * @brief 判断值是否存储真实引用，排除拥有型盒子和闭包本身。
 * @param type 类型。
 * @param seen 递归去重。
 * @return 存储引用。
 */
bool PropagationBorrowsStorage(const std::shared_ptr<MType> &type, Memory::Set<Memory::String> &seen)
{
    if (!type) return false;
    if (type->kind == MType::Ptr) return true;
    if (type->kind == MType::Func || type->kind == MType::Obj) return false;
    if (type->kind == MType::Container || type->kind == MType::Array)
        return PropagationBorrowsStorage(type->elem, seen) || PropagationBorrowsStorage(type->elem2, seen);
    if (type->kind != MType::Struct || !seen.insert(type->name).second) return false;
    const auto definition = FindStruct(type->name);
    if (definition)
        for (const auto &field : definition->fields)
            if (PropagationBorrowsStorage(field.type, seen)) return true;
    return false;
}

std::size_t ControlValueBorrowDepth(int id);
/**
 * @brief 查询借用来源所在的作用域深度。
 * @param id 待查找或操作的对象编号。
 * @param address 运行时内存地址。
 * @retval 借用来源对应的词法作用域深度。
 */
std::size_t BorrowSourceDepth(int id, bool address)
{
    if (id < 0) return 0u;
    // 类型推导可能扩展节点池，递归期间不得保留池内节点或实参列表的引用。
    const auto node = N(id);
    if (node.kind == MNode::Var)
    {
        const auto variable = FindIn(node.module, node.s);
        if (!variable)
        {
            const auto captures = g_lambdaCaptures.find(ModuleSymbol(node.module, node.s));
            if (captures != g_lambdaCaptures.end())
            {
                std::size_t depth = 0u;
                for (const auto &capture : captures->second)
                {
                    const auto source = FindIn(node.module, capture.source);
                    depth = std::max(depth, capture.captureMode >= 2 ?
                                                (HasBorrowedValue(capture.type) && source ? source->borrowDepth : 0u) :
                                                BorrowStorageDepth(source));
                }
                return depth;
            }
            const bool receiver = !g_owner.empty() && (node.s == "self" || FieldType(g_owner, node.s));
            if (receiver && !address && IsCoreObjectReceiver(g_owner)) return 0u;
            return receiver ? 1u : 0u;
        }
        if (address) return BorrowStorageDepth(variable);
        if (HasBorrowedValue(variable->type)) return variable->borrowDepth;
        return variable->elementReference ? BorrowStorageDepth(variable) : 0u;
    }
    if (node.kind == MNode::Un && node.s == "&" && !node.kids.empty())
        return BorrowSourceDepth(node.kids[0], true);
    if (node.kind == MNode::Cast && !node.kids.empty()) return BorrowSourceDepth(node.kids[0], address);
    if ((node.kind == MNode::Member || node.kind == MNode::Arrow || node.kind == MNode::Index) && !node.kids.empty())
    {
        if (address) return BorrowSourceDepth(node.kids[0], true);
        if (HasBorrowedValue(RawExprType(id)))
        {
            const auto root = BorrowRootVariable(node.kids[0]);
            if (const auto owner = BorrowAliasOwner(root)) return owner->borrowDepth;
            return root ? root->borrowDepth : BorrowSourceDepth(node.kids[0]);
        }
        return 0u;
    }
    if (node.kind == MNode::Call && node.kids.size() >= 2u)
    {
        const auto resultType = RawExprType(id);
        if (address && resultType && resultType->kind != MType::Ptr)
            return std::max<std::size_t>(2u, g_scope.size() - 1u);
        if (!address && !HasBorrowedValue(resultType)) return 0u;
        const auto callee = N(node.kids[0]);
        const auto arguments = N(node.kids[1]).kids;
        std::size_t depth = 0u;
        if (StandardCallName(callee) == "move" && arguments.size() == 1u)
        {
            // 转移的是值内部已有的借用，而不是源值存储的地址。
            const auto root = BorrowRootVariable(arguments[0]);
            if (const auto owner = BorrowAliasOwner(root)) return owner->borrowDepth;
            return root ? root->borrowDepth : BorrowSourceDepth(arguments[0]);
        }
        const auto symbol = CallSymbol(callee);
        const bool lease = symbol == "std::iter::listIter" || symbol == "std::iter::listIterMut" || symbol.rfind("std::iter::listIter__", 0) == 0 || symbol.rfind("std::iter::listIterMut__", 0) == 0 || symbol.rfind("std::iter::listIntoIter", 0) == 0 || symbol.rfind("std::iter::setIntoIter", 0) == 0 || symbol.rfind("std::iter::mapIntoIter", 0) == 0;
        if (lease)
        {
            // 标准列表租约持有源存储；元素本身包含的借用仍沿原来源传播。
            for (const int argument : arguments)
            {
                const auto root = BorrowRootVariable(argument);
                if (const auto owner = BorrowAliasOwner(root))
                    depth = std::max(depth, owner->borrowDepth);
                else if (root)
                    depth = std::max(depth, root->borrowDepth);
            }
            return depth;
        }
        if ((callee.kind == MNode::Member || callee.kind == MNode::Arrow) && !callee.kids.empty())
        {
            const auto receiver = RawExprType(callee.kids[0]);
            // Any 的引用查询借用所装入对象，不借用 Any 句柄槽本身。
            if (receiver && receiver->kind == MType::Obj)
            {
                depth = BorrowSourceDepth(callee.kids[0]);
            }
            else if (receiver && IsContainer(receiver) && callee.s == "copy")
            {
                // 容器副本保留元素已有借用，不借用形参或局部的容器句柄槽。
                depth = BorrowSourceDepth(callee.kids[0]);
            }
            else if (receiver)
            {
                const auto valueType = receiver->kind == MType::Ptr ? receiver->elem : receiver;
                const bool enumeration = valueType && valueType->kind == MType::Struct && g_enums.count(valueType->name);
                const auto instance = valueType ? magel_generic::session.made.find(valueType->name) : magel_generic::session.made.end();
                const bool stateIterator = instance != magel_generic::session.made.end() && instance->second.generic->module == "std::iter" && instance->second.generic->name == "Iter";
                // 标准游标返回状态中的元素，来源是状态已有借用而非游标局部槽。
                depth = BorrowSourceDepth(callee.kids[0], !enumeration && !stateIterator);
            }
        }
        for (const int argument : arguments)
            if (HasBorrowedValue(RawExprType(argument)))
                depth = std::max(depth, BorrowSourceDepth(argument));
        return depth;
    }
    if (node.kind == MNode::Loop) return ControlValueBorrowDepth(id);
    if (node.kind == MNode::Try && !node.kids.empty())
    {
        const auto sourceType = ExprType(node.kids[0]);
        const auto instance = magel_generic::session.made.find(sourceType->name);
        const bool standard = instance != magel_generic::session.made.end() && instance->second.generic->module == "std::lang" && (instance->second.generic->name == "Result" || instance->second.generic->name == "Option");
        Memory::Set<Memory::String> references;
        if (!standard && PropagationBorrowsStorage(RawExprType(id), references))
        {
            if (IsFreshAggregate(node.kids[0])) return g_scope.size();
            return BorrowSourceDepth(node.kids[0], true);
        }
        return BorrowSourceDepth(node.kids[0]);
    }
    if (node.kind == MNode::EnumCtor)
    {
        // 静态方法、命名构造入口和枚举变体共用节点；借用其拥有型结果仍借用局部临时槽。
        // 限定全局与静态字段是既有存储，不能当成新构造的临时对象。
        if (const auto global = FindQualifiedGlobal(node.s, node.s2))
            return address ? BorrowStorageDepth(global) : global->borrowDepth;
        if (node.type2 != 1 && FindStaticField(node.s, node.s2)) return 0u;
        const auto resultType = RawExprType(id);
        if (address && resultType && resultType->kind != MType::Ptr)
            return std::max<std::size_t>(2u, g_scope.size() - 1u);
    }
    if (node.kind == MNode::Tuple || node.kind == MNode::EnumCtor || node.kind == MNode::List || node.kind == MNode::CollectionLiteral)
    {
        std::size_t depth = 0u;
        for (const int child : node.kids) depth = std::max(depth, BorrowSourceDepth(child));
        return depth;
    }
    // 新分配的拥有型引用没有局部槽来源，仍按既有显式释放规则管理。
    return address ? 2u : 0u;
}

/**
 * @brief 记录赋值操作造成的借用关系变化。
 * @param target 待访问或修改的目标。
 * @param value 待转换、保存或参与计算的值。
 * @param container 容器类型或目标表达式。
 */
void RecordBorrowStore(int target, int value, bool container)
{
    const auto rawTarget = RawExprType(target);
    // Any 装入自动解引用后的普通值时保存副本，不保存原引用。
    if (ExprType(target)->kind == MType::Obj && !HasBorrowedValue(ExprType(value))) return;
    const auto &right = N(value);
    const bool binding = !container && rawTarget && rawTarget->kind == MType::Ptr && ((right.kind == MNode::Un && right.s == "&") || right.kind == MNode::NewPtr);
    if (!binding && !HasBorrowedValue(ExprType(target))) return;
    const auto depth = BorrowSourceDepth(value);
    // Interrupt 的回调可能由宿主延后执行；公开字段赋值同样遵守登记入口的静态契约。
    if ((N(target).kind == MNode::Member || N(target).kind == MNode::Arrow) && N(target).s == "callback" && !N(target).kids.empty())
    {
        const auto owner = StructNameOf(ExprType(N(target).kids[0]));
        const auto definition = g_structs.find(owner);
        const bool eventInterrupt = definition != g_structs.end() && std::any_of(definition->second.methods.begin(), definition->second.methods.end(), [](const MMethod &method) { return method.module == "std::event"; });
        if (eventInterrupt && depth != 0u)
            g_borrowFacts.push_back({ magel_borrow_checker::Fact::Store, depth, 0u, g_reductionLine,
                                      "（异步事件回调必须具有静态来源）" });
    }
    auto variable = BorrowRootVariable(target);
    auto targetDepth = binding && N(target).kind == MNode::Var ? BorrowVariableDepth(variable) : BorrowStorageDepth(variable);
    if (!variable) targetDepth = BorrowSourceDepth(target, true);
    if (variable && variable->allocatedReference && N(target).kind != MNode::Var)
        targetDepth = std::max<std::size_t>(1u, targetDepth);
    g_borrowFacts.push_back({ magel_borrow_checker::Fact::Store, depth, targetDepth, g_reductionLine, {} });
    // 分支按来源寿命的交集保守合并，后续赋值不能洗掉较短的借用。
    if (variable)
    {
        variable->borrowDepth = std::max(variable->borrowDepth, depth);
        if (binding && N(target).kind == MNode::Var)
        {
            g_effectAliases[variable->emitted] = EffectPlace(value);
            const auto source = BorrowRootVariable(value);
            variable->borrowOwner = source ? (source->type->kind == MType::Ptr ? source->borrowOwner : source->emitted) : Memory::String();
        }
        else if (auto owner = BorrowAliasOwner(variable))
            owner->borrowDepth = std::max(owner->borrowDepth, depth);
    }
    return;
}
