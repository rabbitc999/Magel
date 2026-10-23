/* ---------- 内置函数 ---------- */

/* 运算符文本表：下标与 M_BinOp / M_Op 的序号一一对应（两处都用这张表）。 */
const char *const kOperatorText[] = { "||", "&&", "==", "!=", "<", "<=", ">", ">=",
                                      "+", "-", "*", "/", "%",
                                      "&", "|", "^", "<<", ">>" };

/**
 * @brief 内置函数名 → 运算符。
 * @param name 函数名。
 * @return -1 = 不是内置函数；-2 = `Index`（下标）；其余是 kOperatorText 的下标。
 */
int BuiltinOpIndex(const Memory::String &name)
{
    static const Memory::Map<Memory::String, int> table = {
        { "eq", 2 },
        { "ne", 3 },
        { "lt", 4 },
        { "le", 5 },
        { "gt", 6 },
        { "ge", 7 },
        { "add", 8 },
        { "sub", 9 },
        { "mul", 10 },
        { "div", 11 },
        { "mod", 12 },
    };
    const auto found = table.find(name);
    if (found != table.end()) return found->second;
    if (name == "index") return -2;
    return -1;
}

bool IsReadOnlyAccess(int id);
simple_ast::NodePtr LowerExpr(int id);
simple_ast::NodePtr LowerStmt(int id);
simple_ast::NodePtr LowerStaticQualified(int id);
Memory::String TupleConstructorName(const Memory::String &tupleName);
std::shared_ptr<MType> ExprType(int id);
simple_ast::NodePtr OwnedObjectTemp(int exprId, const std::shared_ptr<MType> &type);

/* 解析一次调用（实现在下面）：标签 / 形参名 / 返回类型。 */
bool ResolveCall(int id, Memory::String *label, Memory::Vector<Memory::String> *params,
                 std::shared_ptr<MType> *result);

/* 接口方法查找（实现在下面）：`ExprType` 要按它给接口引用上的调用定结果类型。 */
bool FindInterfaceMethod(const Memory::String &iface, const Memory::String &methodName,
                         std::shared_ptr<MType> *result,
                         Memory::Vector<std::shared_ptr<MType>> *params);

/**
 * @brief 解析声明中的固定数组维度，实现在普通降低分片。
 */
bool ReadDims(int dimsId, Memory::Vector<int> *out);

/**
 * @brief 分析语句的正常完成、返回和循环退出路径，不生成运行时代码。
 * @param statementId 语句节点。
 * @param transparentLoops 是否将语句循环中的带值退出归入外层值出口。
 * @retval 返回各类可达控制流路径的位掩码。
 */
int StatementReturnFlow(int statementId, bool transparentLoops = false);
void PinLiteralInExpression(int id, const std::shared_ptr<MType> &target);
/**
 * @brief 收集循环内 break 携带的结果表达式。
 * @param statementId 语句在节点池中的编号。
 * @param values 输入值列表或接收读取值的输出列表。
 * @param hasPlain 接收是否存在不带值退出分支的标志。
 * @param expected 期望的类型或匹配规则。
 * @param borrowDepth 借用来源的作用域深度。
 * @param loopDepth 穿透的语句循环层数；内层无值 break 不属于当前值出口。
 */
void CollectLoopBreakValues(int statementId, Memory::Vector<std::shared_ptr<MType>> *values, bool *hasPlain,
                            const std::shared_ptr<MType> &expected = nullptr, std::size_t *borrowDepth = nullptr,
                            std::size_t loopDepth = 0)
{
    if (statementId < 0) return;
    const MNode statement = N(statementId);
    switch (statement.kind)
    {
    case MNode::Break:
        if (statement.kids.empty())
            *hasPlain = *hasPlain || (statement.i == 0 && loopDepth == 0);
        else
        {
            const MNode value = N(statement.kids[0]);
            // 值出口先验证宏辅助变量，避免占位整数类型掩盖名字错误；调用目标另走函数解析。
            if (value.kind == MNode::Var && NodeUsesDefinitionLookup(value) && !FindNodeVar(value)
                && !FindInferenceFunction(value.module, value.s) && !FindFunctionOverloads(value.module, value.s))
            {
                Fail("引用了没声明的变量：" + value.s);
                return;
            }
            if (expected)
            {
                InferExpression(statement.kids[0], expected);
                PinLiteralInExpression(statement.kids[0], expected);
            }
            // 引用结果上下文保留引用身份，与普通 return 的引用转换保持一致。
            values->push_back(expected && expected->kind == MType::Ptr ? RawExprType(statement.kids[0]) : ExprType(statement.kids[0]));
            if (borrowDepth) *borrowDepth = std::max(*borrowDepth, BorrowSourceDepth(statement.kids[0]));
        }
        return;
    case MNode::Declare:
        for (const int declId : N(statement.kids[0]).kids)
        {
            const MNode decl = N(declId);
            const int initializer = decl.kids.empty() ? -1 : decl.kids[0];
            if (initializer >= 0) InferExpression(initializer, decl.type < 0 ? nullptr : T(decl.type));
            if (decl.type < 0 && initializer < 0)
            {
                Fail("let 变量必须给初值：" + decl.s);
                return;
            }
            auto type = decl.type < 0 ? ExprType(initializer) : T(decl.type);
            if (decl.i >= 0)
            {
                Memory::Vector<int> dims;
                if (!ReadDims(static_cast<int>(decl.i), &dims)) return;
                type = ArrayType(type, dims);
            }
            else if (decl.i == -2)
                type = RuntimeArrayType(type);
            MVar binding{ type, decl.s, decl.type2 != 0 };
            if (initializer >= 0) binding.borrowDepth = BorrowSourceDepth(initializer);
            if (binding.isConst) binding.constantExpression = initializer;
            binding.bindingIdentity = decl.bindingIdentity;
            g_scope.back()[NodeBindingKey(decl)] = binding;
        }
        return;
    case MNode::Destructure:
    {
        if (statement.i) return;
        const auto type = ExprType(statement.kids[1]);
        std::function<void(int, const std::shared_ptr<MType> &)> bind;
        bind = [&](int id, const std::shared_ptr<MType> &itemType) {
            const MNode pattern = N(id);
            if (pattern.kind == MNode::List)
            {
                if (!IsTupleType(itemType) || pattern.kids.size() != itemType->paramTypes.size()) return;
                for (std::size_t i = 0; i < pattern.kids.size(); ++i) bind(pattern.kids[i], itemType->paramTypes[i]);
            }
            else if (pattern.s != "_")
            {
                MVar binding{ itemType, pattern.s, false };
                binding.bindingIdentity = pattern.bindingIdentity;
                g_scope.back()[NodeBindingKey(pattern)] = binding;
            }
        };
        bind(statement.kids[0], type);
        return;
    }
    case MNode::Block:
        PushScope();
        for (const int child : Memory::Vector<int>(N(static_cast<int>(statement.i)).kids))
            CollectLoopBreakValues(child, values, hasPlain, expected, borrowDepth, loopDepth);
        PopScope();
        return;
    case MNode::If:
        CollectLoopBreakValues(statement.kids[1], values, hasPlain, expected, borrowDepth, loopDepth);
        for (const int elifId : N(statement.kids[2]).kids)
            CollectLoopBreakValues(N(elifId).kids[1], values, hasPlain, expected, borrowDepth, loopDepth);
        if (statement.kids[3] >= 0) CollectLoopBreakValues(statement.kids[3], values, hasPlain, expected, borrowDepth, loopDepth);
        return;
    case MNode::Switch:
        /* `match` 不截获 break，里面的 break 仍属于外层 loop。 */
        if (statement.s != "match") return;
        for (const int caseId : Memory::Vector<int>(N(statement.kids[1]).kids))
        {
            PushScope();
            const MNode item = N(caseId);
            const auto subject = ExprType(statement.kids[0]);
            if (item.i >= 0 && subject->kind == MType::Struct)
            {
                MNode pattern = N(static_cast<int>(item.i));
                if (pattern.kind == MNode::Call && pattern.kids.size() == 2u)
                {
                    const auto args = N(pattern.kids[1]).kids;
                    pattern = N(pattern.kids[0]);
                    pattern.kids = args;
                }
                const auto name = pattern.kind == MNode::Var ? pattern.s : pattern.s2;
                if (const auto variant = FindEnumVariant(subject->name, name))
                    if (pattern.kids.size() == variant->payloads.size())
                        for (std::size_t i = 0; i < pattern.kids.size(); ++i)
                        {
                            const auto binder = N(pattern.kids[i]);
                            const auto type = variant->payloads[i].type;
                            const bool frozen = statement.type2 == 1 ? type->kind == MType::Ptr : type->kind != MType::Ptr || type->readOnly;
                            MVar binding{ type, binder.s, frozen };
                            binding.bindingIdentity = binder.bindingIdentity;
                            g_scope.back()[NodeBindingKey(binder)] = binding;
                        }
            }
            for (const int child : Memory::Vector<int>(N(item.kids[0]).kids))
                CollectLoopBreakValues(child, values, hasPlain, expected, borrowDepth, loopDepth);
            PopScope();
        }
        return;
    case MNode::While:
    case MNode::DoWhile:
        CollectLoopBreakValues(statement.kids[statement.kind == MNode::While ? 1 : 0], values, hasPlain, expected, borrowDepth, loopDepth + 1);
        return;
    case MNode::For:
        PushScope();
        CollectLoopBreakValues(statement.kids[0], values, hasPlain, expected, borrowDepth, loopDepth);
        CollectLoopBreakValues(statement.kids[3], values, hasPlain, expected, borrowDepth, loopDepth + 1);
        PopScope();
        return;
    case MNode::Foreach:
    {
        const auto sourceType = ExprType(statement.kids[0]);
        auto element = statement.type >= 0 ? T(statement.type) : nullptr;
        if (!element && (IsContainer(sourceType) || sourceType->kind == MType::Array))
            element = sourceType->elem;
        if (!element)
            if (const auto iterator = FindIterator(sourceType))
                element = iterator->element->kind == MType::Ptr ? iterator->element->elem : iterator->element;
        if (!element) return;
        PushScope();
        MVar binding{ element, statement.s, false };
        binding.bindingIdentity = statement.bindingIdentity;
        g_scope.back()[NodeBindingKey(statement)] = binding;
        CollectLoopBreakValues(statement.kids[1], values, hasPlain, expected, borrowDepth, loopDepth + 1);
        PopScope();
        return;
    }
    case MNode::ExprStmt:
        if (!statement.kids.empty() && N(statement.kids[0]).kind == MNode::Loop && N(statement.kids[0]).s == "block")
            CollectLoopBreakValues(N(statement.kids[0]).kids[0], values, hasPlain, expected, borrowDepth, loopDepth);
        else if (!statement.kids.empty() && N(statement.kids[0]).kind == MNode::Loop && N(statement.kids[0]).s.empty())
            CollectLoopBreakValues(N(statement.kids[0]).kids[0], values, hasPlain, expected, borrowDepth, loopDepth + 1);
        return;
    case MNode::Loop:
        if (statement.s.empty()) CollectLoopBreakValues(statement.kids[0], values, hasPlain, expected, borrowDepth, loopDepth + 1);
        return; /* 值位置的内层表达式在各自类型推导中接收 break。 */
    default:
        return;
    }
}

/**
 * @brief 在值出口的词法作用域中计算最短借用来源寿命。
 * @param id 控制表达式。
 * @return 最大来源深度。
 */
std::size_t ControlValueBorrowDepth(int id)
{
    std::size_t depth = 0;
    bool plain = false;
    Memory::Vector<std::shared_ptr<MType>> values;
    CollectLoopBreakValues(N(id).kids[0], &values, &plain, nullptr, &depth);
    return depth;
}

/**
 * @brief 已解析的错误传播契约，类型检查与降低共用。
 */
struct MPropagationInfo
{
    std::shared_ptr<MType> output, residual, branchType;
    MMethod branch, receive;
    bool moving = false;
};
/**
 * @brief 判断实例是否来自指定核心泛型声明。
 * @param name 实例名。
 * @param generic 泛型名。
 * @return 是否同一标准声明。
 */
bool StandardGeneric(const Memory::String &name, const Memory::String &generic)
{
    const auto made = chali_generic::session.made.find(name);
    return made != chali_generic::session.made.end() && made->second.generic->module == "std::lang" && made->second.generic->name == generic;
}
/**
 * @brief 展示传播类型的源码名称，避免诊断泄露实例化符号。
 * @param type 类型。
 * @return 可读名称。
 */
Memory::String PropagationTypeText(const std::shared_ptr<MType> &type)
{
    if (!type) return "<未知>";
    std::function<Memory::String(const Memory::String &, int)> render;
    render = [&](const Memory::String &name, int depth) -> Memory::String {
        const auto instance = chali_generic::session.made.find(name);
        if (depth > 16 || instance == chali_generic::session.made.end()) return name;
        auto text = instance->second.generic->module;
        if (!text.empty()) text += "::";
        text += instance->second.generic->name + "<";
        for (std::size_t i = 0; i < instance->second.args.size(); ++i)
        {
            if (i) text += ",";
            text += render(instance->second.args[i], depth + 1);
        }
        return text + ">";
    };
    return type->kind == MType::Struct ? render(type->name, 0) : Describe(type);
}

/**
 * @brief 收集继承闭包中的传播协议。
 * @param owner 类型名。
 * @param generic 协议名。
 * @return 去重后的协议实例。
 */
Memory::Set<Memory::String> PropagationInterfaces(const Memory::String &owner, const Memory::String &generic)
{
    Memory::Set<Memory::String> out, seen;
    Memory::Vector<Memory::String> pending;
    const auto definition = FindStruct(owner);
    if (definition) pending = definition->interfaces;
    while (!pending.empty())
    {
        const auto name = pending.back();
        pending.pop_back();
        if (!seen.insert(name).second) continue;
        if (StandardGeneric(name, generic)) out.insert(name);
        const auto bases = g_interfaceBases.find(name);
        if (bases != g_interfaceBases.end()) pending.insert(pending.end(), bases->second.begin(), bases->second.end());
    }
    return out;
}
/**
 * @brief 解析 ? 的来源与接收契约。
 * @param value 来源表达式。
 * @param target 是否核对当前函数出口。
 * @param info 解析结果。
 * @return 契约有效。
 */
bool MethodMutatesReceiver(const Memory::String &owner, const MMethod &method, Memory::Set<Memory::String> *visiting);
/**
 * @brief 解析错误传播使用的枚举变体与结果类型。
 * @param value 待转换、保存或参与计算的值。
 * @param target 待访问或修改的目标。
 * @param info 调试信息或类型传播规则。
 * @retval 满足检查条件或操作完成时为 true，检查失败或无法完成时为 false。
 */
bool ResolvePropagation(int value, bool target, MPropagationInfo *info)
{
    auto source = ExprType(value);
    if (!source || source->kind != MType::Struct) return Fail("? 的来源类型必须显式实现 Try<T,R> 或 TryMove<T,R>，现在给的是 " + Describe(source));
    const auto node = N(value);
    const bool explicitMove = node.kind == MNode::Call && !node.kids.empty() && ((N(node.kids[0]).kind == MNode::Var && N(node.kids[0]).s == "__move") || (N(node.kids[0]).kind == MNode::EnumCtor && N(node.kids[0]).s2 == "__move"));
    info->moving = explicitMove || IsFreshAggregate(value);
    auto protocols = PropagationInterfaces(source->name, info->moving ? "TryMove" : "Try");
    if (protocols.empty() && !explicitMove)
    {
        info->moving = false;
        protocols = PropagationInterfaces(source->name, "Try");
    }
    if (protocols.size() != 1u) return Fail("? 的来源类型必须具有唯一传播协议：" + Describe(source));
    const auto &requirements = g_interfaces[*protocols.begin()];
    if (requirements.size() != 1u) return Fail("传播协议必须只有一个拆分方法");
    const auto &required = requirements[0];
    const auto definition = FindStruct(source->name);
    bool found = false;
    for (const auto &method : definition->methods)
        if (!method.superseded && SameInterfaceSignature(required, method))
        {
            info->branch = method;
            found = true;
            break;
        }
    Memory::Set<Memory::String> visiting;
    if (found && !info->moving && MethodMutatesReceiver(source->name, info->branch, &visiting))
        return Fail("Try.branch 必须是只读拆分，移动传播请实现 TryMove.intoBranch");
    if (!found || !StandardGeneric(required.result->name, "TryBranch")) return Fail("传播拆分方法必须返回 TryBranch<T,R>");
    info->branchType = required.result;
    const auto yes = FindEnumVariant(required.result->name, "Continue");
    const auto no = FindEnumVariant(required.result->name, "Break");
    if (!yes || !no || yes->payloads.size() != 1u || no->payloads.size() != 1u) return Fail("TryBranch 必须具有 Continue 与 Break 单载荷变体");
    info->output = yes->payloads[0].type;
    info->residual = no->payloads[0].type;
    if (!target) return true;
    auto receivers = g_returnType ? PropagationInterfaces(g_returnType->name, "FromResidual") : Memory::Set<Memory::String>();
    found = false;
    for (const auto &receiver : receivers)
        for (const auto &contract : g_interfaces[receiver])
        {
            if (!contract.isStatic || !SameType(contract.result, g_returnType) || contract.params.size() != 1u || !SameType(contract.params[0], info->residual)) continue;
            const auto destination = FindStruct(g_returnType->name);
            for (const auto &method : destination->methods)
                if (!method.superseded && SameInterfaceSignature(contract, method))
                {
                    if (found) return Fail("错误传播接收协议重复");
                    info->receive = method;
                    found = true;
                }
        }
    if (!found) return Fail("无法传播 " + PropagationTypeText(info->residual) + "：返回类型 " + PropagationTypeText(g_returnType) + " 未实现匹配的 FromResidual<F,R>");
    return true;
}

/**
 * @brief 查询传播是否产生可转移的拥有型成功值。
 * @param id 传播节点。
 * @return 使用移动协议。
 */
bool OwnedPropagation(int id)
{
    if (id < 0 || N(id).kind != MNode::Try) return false;
    MPropagationInfo info;
    if (!ResolvePropagation(N(id).kids[0], false, &info)) return false;
    const auto source = ExprType(N(id).kids[0]);
    const bool optimized = !info.moving && g_returnType && ((StandardGeneric(source->name, "Result") && StandardGeneric(g_returnType->name, "Result")) || (StandardGeneric(source->name, "Option") && StandardGeneric(g_returnType->name, "Option")));
    return !optimized;
}

/**
 * @brief 推导循环表达式中各 break 结果的公共类型。
 * @param id 待查找或操作的对象编号。
 * @retval 对应的类型描述对象。
 */
std::shared_ptr<MType> LoopValueType(int id)
{
    Memory::Vector<std::shared_ptr<MType>> values;
    bool hasPlain = false;
    const auto node = N(id);
    const auto contextual = (!node.s.empty() || node.returnExpression) && node.type >= 0 ? T(node.type) : nullptr;
    // Any 在整个值表达式求值后装箱；分支仍须先确定一致的具体结果类型。
    const auto expected = contextual && contextual->kind != MType::Obj && !IsStandardOpcode(contextual, "String") ? contextual : nullptr;
    if (node.s == "block")
    {
        if (StatementReturnFlow(node.kids[0], true) & 1)
            Fail("块表达式的每条正常路径必须使用 yield 值; 产生结果");
    }
    if (node.s == "if")
    {
        const auto conditional = N(N(node.kids[0]).i).kids[0];
        if (N(conditional).kids[3] < 0) Fail("if 表达式必须包含 else 分支");
        if (StatementReturnFlow(conditional, true) & 1) Fail("if 表达式的每条正常路径必须产生结果；简单分支不写分号，复杂分支使用 yield 值;");
    }
    if (node.s == "match")
    {
        const auto matching = N(N(node.kids[0]).i).kids[0];
        const auto subject = ExprType(N(matching).kids[0]);
        bool fallback = false;
        for (const auto branch : N(N(matching).kids[1]).kids)
        {
            const auto item = N(branch);
            if (item.i < 0 && item.guard < 0) fallback = true;
            const int block = M_Block(item.kids[0]);
            if (StatementReturnFlow(block, true) & 1)
                Fail("match 表达式的每条正常分支必须产生结果；简单分支不写分号，复杂分支使用 yield 值;");
        }
        if (!fallback && (subject->kind != MType::Struct || !g_enums.count(subject->name)))
            Fail("非枚举 match 表达式必须包含 `_` 兜底分支");
    }
    CollectLoopBreakValues(node.kids[0], &values, &hasPlain, expected);
    if (node.s == "block" && hasPlain)
        Fail("块表达式必须使用 yield 值;，不能使用无值 break;");
    if (values.empty()) return expected ? expected : T(M_TypeVoid());
    if (hasPlain && node.s.empty())
    {
        Fail("同一个 loop 不能混用 `break;` 与 `break value;`");
        return T(M_TypeVoid());
    }
    const std::shared_ptr<MType> type = expected ? expected : values[0];
    for (std::size_t index = expected ? 0u : 1u; index < values.size(); ++index)
    {
        const std::shared_ptr<MType> other = values[index];
        if (expected && !node.returnExpression ? !Acceptable(type, other, true) : TypeKey(type) != TypeKey(other))
        {
            Fail((!node.s.empty() ? node.s + " 表达式的分支结果类型必须一致：" : Memory::String("同一个 loop 的 yield 值类型必须一致：")) + Describe(type) + " 与 " + Describe(other));
            return T(M_TypeVoid());
        }
    }
    return type;
}

/**
 * @brief 表达式的静态类型。
 *
 * 只关心四件事：是不是标量、struct 叫什么、数组几维、引用指向什么。降级时靠它决定
 * "走内建运算还是运算符重载"、"下标怎么折成一维"、"写引用内容要不要报错"。
 */
/**
 * @brief 普通调用和重载运算的引用结果在值上下文自动读取；取引用与显式转换保留引用。
 */
bool AutoDereferences(int id)
{
    if (id < 0) return false;
    const MNode &node = N(id);
    return node.kind == MNode::Var || node.kind == MNode::Member || node.kind == MNode::Arrow || node.kind == MNode::Call || node.kind == MNode::Try || node.kind == MNode::Index || node.kind == MNode::EnumCtor || node.kind == MNode::Bin || (node.kind == MNode::Un && (node.s == "-" || node.s == "+" || node.s == "~"));
}
/**
 * @brief 推导表达式的语义类型。
 * @param id 待查找或操作的对象编号。
 * @retval 对应的类型描述对象。
 */
std::shared_ptr<MType> ExprType(int id)
{
    auto type = RawExprType(id);
    if (AutoDereferences(id) && type && type->kind == MType::Ptr && type->elem && type->elem->kind != MType::Void && !IsInterfacePointer(type)) return type->elem;
    return type;
}
/**
 * @brief 判断访问路径是否借用元组元素，显式可写引用转换结束只读传播。
 * @param id 访问表达式节点。
 * @return 访问元组位置或其子字段、子元素时返回 true。
 */
bool IsTupleElementAccess(int id)
{
    while (id >= 0)
    {
        const MNode &node = N(id);
        if (node.kind == MNode::Cast)
        {
            const auto type = T(node.type);
            if (type->kind == MType::Ptr && !type->readOnly) return false;
        }
        if ((node.kind == MNode::Member || node.kind == MNode::Arrow) && !node.kids.empty() && IsTupleType(ExprType(node.kids[0]))) return true;
        if ((node.kind == MNode::Member || node.kind == MNode::Arrow || node.kind == MNode::Index || node.kind == MNode::Cast || (node.kind == MNode::Un && (node.s == "*" || node.s == "&"))) && !node.kids.empty())
            id = node.kids[0];
        else
            break;
    }
    return false;
}

/**
 * @brief 按括号种类推导 List、Set、Map 字面量并向嵌套元素传递上下文类型。
 * @param id 字面量节点。
 * @param expected 已知目标类型；空字面量必须提供与括号种类相符的容器上下文。
 * @return 推导所得的容器类型。
 */
std::shared_ptr<MType> InferCollectionLiteral(int id, const std::shared_ptr<MType> &expected)
{
    const MNode node = N(id);
    auto target = expected;
    if (target && target->kind == MType::Ptr) target = target->elem;
    const bool emptyBraces = node.s == "empty";
    if (target && IsContainer(target)
        && (emptyBraces ? target->name == "vec" : node.s != target->name))
    {
        Fail("集合字面量类型不匹配：" + node.s + " 不能初始化 " + Describe(target));
        return target;
    }
    if (target && IsContainer(target))
    {
        const bool map = target->name == "map";
        g_nodes[static_cast<std::size_t>(id)].type = M_AddType(target);
        for (std::size_t i = 0; i < node.kids.size(); ++i)
            InferExpression(node.kids[i], map && i % 2u ? target->elem2 : target->elem);
        return target;
    }
    if (node.type >= 0) return T(node.type);
    if (node.kids.empty())
    {
        Fail(node.s == "vec" ? "空列表字面量 `[]` 需要 std::lang::List<T> 上下文" : "空集合字面量 `{}` 需要 std::lang::Set<T> 或 std::lang::Map<K,V> 上下文");
        return T(M_TypeInt());
    }
    auto type = Memory::makeShared<MType>();
    type->kind = MType::Opcode;
    type->name = node.s;
    const std::size_t step = node.s == "map" ? 2u : 1u;
    for (std::size_t part = 0; part < step; ++part)
    {
        std::shared_ptr<MType> element;
        for (std::size_t i = part; i < node.kids.size(); i += step)
        {
            const auto &child = N(node.kids[i]);
            if (child.kind == MNode::CollectionLiteral && child.kids.empty() && child.type < 0) continue;
            const auto actual = ExprType(node.kids[i]);
            if (!element)
                element = actual;
            else if ((element->kind == MType::Int || IsFloatType(element)) && (actual->kind == MType::Int || IsFloatType(actual)))
                element = NumericResult(element, actual);
            else if (!SameType(element, actual))
            {
                Fail("集合字面量的" + Memory::String(part ? "值" : node.s == "map" ? "键" :
                                                                                     "元素")
                     + "类型不一致：" + Describe(element) + " 与 " + Describe(actual));
            }
        }
        if (!element)
        {
            Fail("集合字面量的嵌套空值无法推导类型，请声明容器类型");
            return T(M_TypeInt());
        }
        if (part == 0)
            type->elem = element;
        else
            type->elem2 = element;
        for (std::size_t i = part; i < node.kids.size(); i += step) InferExpression(node.kids[i], element);
    }
    g_nodes[static_cast<std::size_t>(id)].type = M_AddType(type);
    return type;
}

/**
 * @brief 推导内部区间迭代器的元素类型。
 * @param id 区间节点。
 * @return 显式整型或端点公共整型。
 */
std::shared_ptr<MType> IntervalElementType(int id)
{
    const auto node = N(id);
    if (node.type >= 0) return T(node.type);
    auto lo = ExprType(node.kids[0]), hi = ExprType(node.kids[1]);
    if (SameType(lo, hi) && (lo->kind == MType::Int || lo->kind == MType::Char)) return lo;
    const bool wideUnsigned = (lo->kind == MType::Int && lo->isUnsigned && lo->bits == 64) || (hi->kind == MType::Int && hi->isUnsigned && hi->bits == 64);
    return T(M_TypeIntBits(64, wideUnsigned));
}

/**
 * @brief 运算符会不会让「整数字面量」按上下文定型：算术与位运算。
 *
 * 这几类的**结果类型由操作数类型决定**，所以字面量跟着上下文走是有意义的；
 * 比较 / 逻辑的结果是 bool，与操作数类型无关，不参与这层定型。
 *
 * @param op 运算符文本。
 * @return 是算术 / 位运算则返回 true。
 */
bool LiteralAdaptingOperator(const Memory::String &op)
{
    return op == "+" || op == "-" || op == "*" || op == "/" || op == "%" || op == "&" || op == "|" || op == "^" || op == "<<" || op == ">>";
}

/**
 * @brief 这个表达式是不是**还没定型**的整数字面量（十进制 / 十六进制 / 二进制）。
 */
bool UntypedIntegerLiteral(int id)
{
    return id >= 0 && N(id).kind == MNode::Int && N(id).type < 0;
}

/**
 * @brief 把整数字面量按目标类型定型（`int32 x = 1;` 里的 `1` 就是 int32）。
 *
 * 原来所有整数字面量一律按 int64 走，于是 `int32 x = 1; int32 y = x + 1;` 会把 `1`
 * 当 int64、把表达式提升成 int64，赋值时反而要用户写 `(int32)(1)`。这里让字面量
 * 落到上下文给的类型上。
 *
 * 只对**还没定型**的字面量动手；十进制字面量要求值落在目标类型范围内，装不下就
 * 保持原样（int64），让外层按「缩窄要强转」报错 —— 绝不静默截断。十六进制 /
 * 二进制按**位模式**看待（`0xFF` 就是「低 8 位全 1」），不查范围。
 *
 * @param id 表达式节点。
 * @param target 目标类型；不是整型就不动。
 */
void PinIntegerLiteral(int id, const std::shared_ptr<MType> &target)
{
    if (!target || target->kind != MType::Int) return;
    if (!UntypedIntegerLiteral(id)) return;
    if (N(id).unsignedLiteral)
    {
        if (!target->isUnsigned) return;
        const auto value = static_cast<std::uint64_t>(N(id).i);
        if (target->bits < 64 && value > ((std::uint64_t(1) << target->bits) - 1)) return;
    }
    else
    {
        long long value = 0;
        if (DecimalIntegerLiteral(id, &value) && !IntegerInRange(target, value)) return;
    }
    g_nodes[static_cast<std::size_t>(id)].type = M_AddType(target);
}

/**
 * @brief 把目标类型下传给表达式里的未定型字面量（`int32 c = ff() + 1;` 里的 `1`）。
 *
 * 只穿过算术 / 位运算的二元节点与 `-` / `+` / `~` 一元节点：调用、下标、成员访问
 * 的结果类型不由外层目标决定，传下去只会把无关的字面量定型错。
 *
 * @param id 表达式节点。
 * @param target 目标类型。
 */
void PinLiteralInExpression(int id, const std::shared_ptr<MType> &target)
{
    if (id < 0 || !target || target->kind != MType::Int) return;
    const MNode &node = N(id);
    if (node.kind == MNode::Int)
    {
        PinIntegerLiteral(id, target);
        return;
    }
    if (node.kind == MNode::Loop && !node.s.empty())
    {
        g_nodes[id].type = M_AddType(target);
        return;
    }
    if (node.kind == MNode::Un)
    {
        if ((node.s == "-" || node.s == "+" || node.s == "~") && !node.kids.empty())
            PinLiteralInExpression(node.kids[0], target);
        return;
    }
    if (node.kind != MNode::Bin || !LiteralAdaptingOperator(node.s)) return;
    for (const int kid : node.kids) PinLiteralInExpression(kid, target);
}

/**
 * @brief 二元算术里，未定型的字面量向**另一侧**的类型看齐。
 *
 * `x + 1`（`x` 是 int32）里的 `1` 取 int32，整个表达式也还是 int32，不必再强转；
 * 另一侧同样是字面量（`1 + 2`）时不定型 —— 交给默认 int64，或者由外层的目标类型
 * 经 {@link PinLiteralInExpression} 一次定掉。
 *
 * @param literalId 可能是字面量的一侧。
 * @param otherId 另一侧。
 */
void PinLiteralTowards(int literalId, int otherId)
{
    if (!UntypedIntegerLiteral(literalId) || otherId < 0) return;
    if (UntypedIntegerLiteral(otherId)) return;
    PinIntegerLiteral(literalId, ExprType(otherId));
}

/**
 * @brief 用户函数或回调遮蔽 index 时恢复原调用。
 * @param id 待检查表达式节点。
 */
void ResolveBufferIndexShadowing(int id)
{
    if (id < 0 || N(id).kind != MNode::Index || N(id).s2 != "__chali_bare_index") return;
    const MNode node = N(id);
    if (CanUseImplicitLang(node.module, "index")) return;
    const int arguments = M_List();
    g_nodes[static_cast<std::size_t>(arguments)].kids = node.kids;
    g_nodes[static_cast<std::size_t>(id)].kind = MNode::Call;
    g_nodes[static_cast<std::size_t>(id)].kids = { static_cast<int>(node.i), arguments };
    g_nodes[static_cast<std::size_t>(id)].i = 0;
    g_nodes[static_cast<std::size_t>(id)].s.clear();
    g_nodes[static_cast<std::size_t>(id)].s2.clear();
}

/**
 * @brief 判断无实参 drop() 是否由编译器完成拥有存储或聚合的终结。
 * @param id 待识别的调用表达式。
 * @return 堆引用、数组、函数对象及可析构聚合的 drop() 返回 true；普通同名方法返回 false。
 */
bool IsIntrinsicDropCall(int id)
{
    const auto node = N(id);
    if (node.kind != MNode::Call || node.kids.size() < 2u || !N(node.kids[1]).kids.empty()) return false;
    const auto callee = N(node.kids[0]);
    if ((callee.kind != MNode::Member && callee.kind != MNode::Arrow) || callee.s != "drop" || callee.kids.empty()) return false;
    const auto type = RawExprType(callee.kids[0]);
    if (!type) return false;
    if ((type->kind == MType::Ptr && !IsInterfacePointer(type)) || IsRuntimeArray(type) || type->kind == MType::Opcode || type->kind == MType::Func || type->kind == MType::Obj) return true;
    if (!IsDestructibleValue(type) || StructHasInterface(StructNameOf(type), "Drop")) return false;
    const auto definition = FindStruct(StructNameOf(type));
    if (definition)
        for (const auto &method : definition->methods)
            if (!method.superseded && !method.isStatic && method.source == "drop") return false;
    return true;
}

/**
 * @brief 推导表达式在原始值上下文中的类型。
 * @param id 待查找或操作的对象编号。
 * @retval 对应的类型描述对象。
 */
std::shared_ptr<MType> RawExprType(int id)
{
    if (id < 0) return T(M_TypeInt());
    NormalizeFormattedOutputCall(id);
    ResolveBufferIndexShadowing(id);
    InferExpression(id);
    const MNode node = N(id);
    switch (node.kind)
    {
    case MNode::Range:
    {
        const auto element = IntervalElementType(id);
        if (!element || (element->kind != MType::Int && element->kind != MType::Char))
        {
            Fail("内建区间的元素必须是整数或 char");
            return T(M_TypeVoid());
        }
        return NamedType("std::lang::__Interval_" + Describe(element));
    }
    case MNode::CollectionLiteral: return node.type >= 0 ? T(node.type) : InferCollectionLiteral(id, nullptr);
    case MNode::TupleTypeItem:
        Fail("元组值不能包含类型名称");
        return T(M_TypeVoid());
    case MNode::Tuple:
    {
        if (node.type >= 0)
        {
            EnsureTupleDefinition(T(node.type));
            return T(node.type);
        }
        Memory::Vector<std::shared_ptr<MType>> elements;
        for (const int valueId : node.kids) elements.push_back(ExprType(valueId));
        const std::shared_ptr<MType> type = MakeTupleType(elements);
        EnsureTupleDefinition(type);
        return type;
    }
    /* 整数字面量：被上下文定过型就按那个类型走（`int32 x = 1;` 里的 `1`），
     * 否则才是默认的 int64（见 PinIntegerLiteral）。 */
    case MNode::Int: return node.type >= 0 ? T(node.type) : T(M_TypeIntBits(64, node.unsignedLiteral ? 1 : 0));
    case MNode::Bool: return T(M_TypeBool());
    case MNode::Null:
    {
        /* `null`：给一个"空引用"类型，能喂给任何 `T*`（比不了别的）。 */
        auto type = Memory::makeShared<MType>();
        type->kind = MType::Ptr;
        type->elem = T(M_TypeVoid());
        return type;
    }
    case MNode::Float: return node.type >= 0 ? T(node.type) : T(M_TypeDouble()); /* f 后缀固定为 float32。 */
    case MNode::Str: return T(M_TypeString());                                   /* 字符串字面量：数据区句柄 */
    case MNode::Char: return T(M_TypeChar());
    /* 切片的结果也是**新串**（堆上），类型同样是 `string`。 */
    case MNode::Slice: return T(M_TypeString());
    case MNode::EnumCtor:
    {
        /* `模块::全局变量`：限定写法先认模块全局，再走 static 字段 / 枚举构造。 */
        if (const MVar *global = FindQualifiedGlobal(node.s, node.s2); global != nullptr)
            return global->type;
        const MStaticField *field = FindStaticField(node.s, node.s2);
        if (node.type2 != 1 && field != nullptr) return field->type;
        /* `Type::method`：类型名两种写法都认（裸名按当前模块补全，见 FindStruct）。 */
        const MStructDef *owner = FindStruct(node.s);
        if (node.type2 != 0 && owner != nullptr)
        {
            for (const MMethod &method : owner->methods)
            {
                if (!method.isStatic || method.source != node.s2 || !AcceptsArgumentCount(method.params, node.kids.size())) continue;
                bool matches = true;
                for (std::size_t index = 0; index < node.kids.size(); ++index)
                {
                    const auto &param = method.params[std::min(index, method.params.size() - 1u)];
                    const auto type = param.variadic ? param.type->elem : param.type;
                    if (!Acceptable(type, RawExprType(node.kids[index]), true)) matches = false;
                }
                if (matches) return method.result;
            }
        }
        return NamedType(node.s);
    }
    case MNode::Loop: return LoopValueType(id);
    case MNode::Try:
    {
        MPropagationInfo info;
        return ResolvePropagation(node.kids[0], false, &info) ? info.output : T(M_TypeVoid());
    }
    case MNode::GenericClosureSelect: return ExprType(node.kids[0]);
    case MNode::Var:
    {
        if (!NodeUsesDefinitionLookup(node) && FindGenericClosureDefinition(node.s)) return GenericClosureType(node.s);
        const MVar *found = FindNodeVar(node);
        if (found != nullptr) return found->type;
        /* 方法体里可以直接写字段名（等价于 `self.字段`），`self` 就是接收者 */
        if (!NodeUsesDefinitionLookup(node) && !g_owner.empty())
        {
            if (node.s == "self")
            {
                return NamedType(g_owner);
            }
            const std::shared_ptr<MType> field = FieldType(g_owner, node.s);
            if (field) return field;
        }
        /* 函数名当值用：类型是"函数指针"（重载时先取第一个，赋值/初始化时按目标类型再挑）。 */
        const MFuncSig *inferredFunction = FindInferenceFunction(node.module, node.s);
        if (inferredFunction != nullptr) return FuncTypeOf(*inferredFunction);
        const Memory::Vector<Memory::String> *function = FindFunctionOverloads(node.module, node.s);
        if (function != nullptr && !function->empty())
        {
            const auto signature = g_funcs.find((*function)[0]);
            if (signature != g_funcs.end()) return FuncTypeOf(signature->second);
        }
        return T(M_TypeInt());
    }
    case MNode::Index:
    {
        auto base = RawExprType(node.kids[0]);
        if (node.s == "__chali_buffer_index" && base->kind == MType::Ptr) return base->elem;
        if (base->kind == MType::Ptr && base->elem && (IsStandardOpcode(base->elem, "String") || base->elem->kind == MType::Array || IsContainer(base->elem))) base = base->elem;
        if (const MIndexDecl *index = FindIndex(base, ExprType(node.kids[1])))
            return index->result;
        if (IsInterfacePointer(base)) return T(M_TypeInt()); /* 会报错，给个占位 */
        if (base->kind == MType::Array || base->kind == MType::Ptr) return base->elem;
        if (IsStandardOpcode(base, "String")) return T(M_TypeChar());
        if (IsContainer(base))
        {
            /* `v[i]` 是元素；`m[k]` 是值（`std::lang::Map<K,V>` 的 V）。`set` 没有下标。 */
            if (base->name == "map") return base->elem2 ? base->elem2 : base->elem;
            return base->elem;
        }
        return T(M_TypeInt());
    }
    case MNode::Member:
    case MNode::Arrow:
    {
        const std::shared_ptr<MType> base = ExprType(node.kids[0]);
        if (node.s == "opcode")
            if (const auto *declaration = FindOpcodeDeclaration(base))
            {
                if (node.module != declaration->module)
                    return Fail("opcode 内部句柄仅允许在声明模块中访问"), T(M_TypeVoid());
                return T(M_TypeInt());
            }
        /* 容器 / `bint` 上的 `.xxx` 只可能是**方法调用**的一部分（`v.push(…)`），
         * 类型由调用点（方法表）决定，这里给个占位就行。 */
        if (IsObjectLike(base)) return T(M_TypeInt());
        const std::shared_ptr<MType> field = FieldType(StructNameOf(base), node.s);
        if (field && field->kind == MType::Ptr && (IsTupleType(base) || IsTupleElementAccess(node.kids[0])))
        {
            auto readOnly = Memory::makeShared<MType>(*field);
            readOnly->readOnly = true;
            return readOnly;
        }
        return field ? field : T(M_TypeInt());
    }
    case MNode::BigIntCtor: return T(M_TypeBigInt()); /* `bint(值)` */
    case MNode::BigFloatCtor: return T(M_TypeBigFloat());
    case MNode::Cast: return T(node.type);
    case MNode::NewPtr:
    {
        /* `new` / `new [n]` 的类型：就是"被赋值的那个东西的类型"
         * （`T mut& p = new;` → `T mut&`；`T a[] = new [n];` → `T[]`）。
         * 拿不到上下文（比如把 new 当函参传）就报错，见 LowerNewPtr。 */
        if (g_expectedNewType) return g_expectedNewType;
        auto fallback = Memory::makeShared<MType>();
        fallback->kind = MType::Ptr;
        fallback->elem = T(M_TypeVoid());
        fallback->readOnly = false;
        return fallback;
    }
    case MNode::Construct: return T(node.type); /* `T::new(...)` 工厂体内的构造 → 目标类型本身 */
    case MNode::TypeTag: return T(M_TypeInt()); /* __typeTag → int（类型号，编译期折好） */
    case MNode::Un:
    {
        if (node.s == "&") /* 取得元素引用，沿用来源的可写性。 */
        {
            auto pointer = Memory::makeShared<MType>();
            pointer->kind = MType::Ptr;
            const auto raw = RawExprType(node.kids[0]);
            if (raw->kind == MType::Ptr)
            {
                if (!IsTupleElementAccess(node.kids[0])) return raw;
                auto readOnly = Memory::makeShared<MType>(*raw);
                readOnly->readOnly = true;
                return readOnly;
            }
            pointer->elem = ExprType(node.kids[0]);
            const MVar *variable = N(node.kids[0]).kind == MNode::Var ? FindNodeVar(N(node.kids[0])) : nullptr;
            pointer->readOnly = (variable != nullptr && variable->isConst) || IsTupleElementAccess(node.kids[0]);
            int baseId = node.kids[0];
            while (baseId >= 0)
            {
                const auto baseType = RawExprType(baseId);
                if (baseType->kind == MType::Ptr && baseType->readOnly) pointer->readOnly = true;
                const auto &baseNode = N(baseId);
                if (baseNode.kind == MNode::Str) pointer->readOnly = true;
                if ((baseNode.kind == MNode::Member || baseNode.kind == MNode::Arrow) && baseNode.s == "opcode" && !baseNode.kids.empty() && FindOpcodeDeclaration(ExprType(baseNode.kids[0])))
                    pointer->readOnly = true;
                if (baseNode.kind == MNode::EnumCtor)
                {
                    const MVar *global = FindQualifiedGlobal(baseNode.s, baseNode.s2);
                    if (global && global->isConst) pointer->readOnly = true;
                }
                if (baseNode.kind != MNode::Member && baseNode.kind != MNode::Arrow && baseNode.kind != MNode::Index) break;
                baseId = baseNode.kids[0];
            }
            return pointer;
        }
        if (node.s == "*") /* 显式解引用兼容写法。 */
        {
            const std::shared_ptr<MType> base = RawExprType(node.kids[0]);
            if (base->kind == MType::Ptr) return base->elem;
        }
        if (node.s == "!") return T(M_TypeBool());
        /*
         * `-` / `+` / `~`：类型**跟着操作数走**。
         *
         * 这里以前一律返回 int，于是 `-2.5` 被当成 int：`auto a = -2.5;` 推成 int、
         * 重载挑到 int 那份（`abs(-2.5)` 算出 2）、`{:3F}` 报"整数不接受浮点格式"、
         * `println("{}", -0.5)` 打成 0。值本身一直是对的（降级走的是浮点取负），
         * 错的只是**类型**，所以只在类型相关的位置露出来。
         */
        if (node.kids.empty()) return T(M_TypeInt());
        const std::shared_ptr<MType> operand = ExprType(node.kids[0]);
        if (operand != nullptr && (operand->kind == MType::Int || operand->kind == MType::Double || IsStandardOpcode(operand, "BigInteger") || IsStandardOpcode(operand, "BigDecimal")))
            return operand;
        /* 用户自定义一元运算符：结果类型取重载的返回类型。 */
        if (operand != nullptr && (operand->kind == MType::Struct || IsEnumType(operand)))
        {
            const MOperatorDecl *declaration = FindUnaryOperator(node.s, operand);
            if (declaration != nullptr) return declaration->result;
        }
        return T(M_TypeInt());
    }
    case MNode::Call:
    {
        /* 调用结果类型已钉在节点上：链式调用的 receiver 在推断、降级、所有权
         * 各阶段会被反复查询，没有这个快路径的话每次查询都完整重推整条链，
         * 代价随链长指数增长。类型由下方 ResolveCall 成功后回写，语义与
         * MNode::Bin 的写回惯例一致。 */
        if (IsIntrinsicDropCall(id)) return T(M_TypeVoid());
        if (ResolveGenericClosureCall(id)) return N(id).type >= 0 ? T(N(id).type) : T(M_TypeVoid());
        if (node.type >= 0) return T(node.type);
        if (!node.kids.empty() && N(node.kids[0]).kind == MNode::Var && N(node.kids[0]).s == "__chali_default_to_string") return T(M_TypeString());
        /* 容器方法（`v.push(…)` / `m.valAt(i)`）与 `bint` 的 `.str()`：被调者是成员访问，
         * 但基类型不是结构体 —— 结果类型查方法表。 */
        if (node.kids.size() >= 2u)
        {
            const MNode calleeNode = N(node.kids[0]);
            if (calleeNode.kind == MNode::Member || calleeNode.kind == MNode::Arrow)
            {
                const std::shared_ptr<MType> baseType = ExprType(calleeNode.kids[0]);
                if (calleeNode.s == "toString" && N(node.kids[1]).kids.empty()) return T(M_TypeString());
                if (IsContainer(baseType))
                {
                    const MContainerMethod *method = FindContainerMethod(baseType, calleeNode.s);
                    if (method != nullptr) return ContainerMethodType(baseType, *method);
                    /* 容器上不存在的名字在这里就报清楚：否则会一路掉到重载解析，
                     * 报成"方法只能调在 struct 上"，看不出到底是哪个容器没有哪个方法。 */
                    return Fail("容器 " + Describe(baseType) + " 没有方法 " + calleeNode.s + "（支持的方法见 README 的「容器」一节）"),
                           T(M_TypeInt());
                }
                /* 运行时数组 `T[]` 的 `.len()`：元素个数存在对象块头里。
                 * 0.20.7 删掉 `sizeof` 之后，这是"数组有几个元素"的唯一写法。 */
                if (baseType && IsRuntimeArray(baseType))
                {
                    if (calleeNode.s == "len") return T(M_TypeInt());
                    return Fail("运行时数组 `" + Describe(baseType) + "` 只有 .len() 方法（元素个数）；"
                                                                      "现在是 ."
                                + calleeNode.s),
                           T(M_TypeInt());
                }
                if (FindOpcodeDeclaration(baseType))
                    if (const auto *owner = FindStruct(baseType->name))
                        for (const auto &method : owner->methods)
                            if (!method.isStatic && !method.superseded && method.source == calleeNode.s && AcceptsArgumentCount(method.params, N(node.kids[1]).kids.size()))
                                return method.result;
                /* 大整数的 `.str()` 返回字符串，`.copy()` 返回新大整数；
                 * 扩展方法见下（幂 / 数论 / 素性 / 位运算 / 进制 / 开方）。 */
                if (baseType && IsStandardOpcode(baseType, "BigInteger"))
                {
                    if (calleeNode.s == "str") return T(M_TypeString());
                    if (calleeNode.s == "copy") return baseType;
                    if (calleeNode.s == "pow" || calleeNode.s == "powmod" || calleeNode.s == "gcd" || calleeNode.s == "lcm" || calleeNode.s == "mod" || calleeNode.s == "modInverse" || calleeNode.s == "nextProbablePrime" || calleeNode.s == "bitAnd" || calleeNode.s == "bitOr" || calleeNode.s == "bitXor" || calleeNode.s == "iroot")
                        return baseType;
                    if (calleeNode.s == "isProbablePrime") return T(M_TypeBool());
                    if (calleeNode.s == "bitLength") return T(M_TypeInt());
                    if (calleeNode.s == "strRadix") return T(M_TypeString());
                }
                if (baseType && IsStandardOpcode(baseType, "BigDecimal"))
                {
                    if (calleeNode.s == "str") return T(M_TypeString());
                    if (calleeNode.s == "copy") return baseType;
                    if (calleeNode.s == "setPrecision") return T(M_TypeInt());
                    if (calleeNode.s == "pow" || calleeNode.s == "abs" || calleeNode.s == "sqrt" || calleeNode.s == "root")
                        return baseType;
                    if (calleeNode.s == "isInteger") return T(M_TypeBool());
                    if (calleeNode.s == "floor" || calleeNode.s == "ceil" || calleeNode.s == "trunc")
                        return T(M_TypeBigInt());
                }
                /* `obj`：`copy()` 给独立盒子；`name()` 给类型号，`get()` 看上下文。 */
                if (baseType && baseType->kind == MType::Obj)
                {
                    if (calleeNode.s == "copy") return baseType;
                    if (calleeNode.s == "clear") return T(M_TypeVoid());
                    if (calleeNode.s == "isEmpty" || calleeNode.s == "__isTypeTag") return T(M_TypeBool());
                    if (calleeNode.s == "getValue" || calleeNode.s == "getReference" || calleeNode.s == "getOptional")
                        return node.type >= 0 ? T(node.type) : g_expectedObjGet ? g_expectedObjGet :
                                                                                  T(M_TypeInt());
                    return T(M_TypeInt());
                }
                /* 接口引用上的方法：结果类型就是**接口里声明的返回类型**。 */
                if (IsInterfacePointer(baseType))
                {
                    std::shared_ptr<MType> interfaceResult;
                    Memory::Vector<std::shared_ptr<MType>> interfaceParams;
                    if (FindInterfaceMethod(baseType->elem->name, calleeNode.s, &interfaceResult,
                                            &interfaceParams))
                    {
                        return interfaceResult;
                    }
                    return T(M_TypeInt());
                }
                Memory::Vector<std::shared_ptr<MType>> argumentTypes;
                for (const int argument : N(node.kids[1]).kids)
                    argumentTypes.push_back(RawExprType(argument));
                const MInterfaceMethod *required = nullptr;
                if (FindHostInterfaceImplementation(baseType, calleeNode.s, argumentTypes,
                                                    &required)
                    != nullptr)
                    return required->result;
            }
        }
        const Memory::String algorithm = AlgorithmCallName(N(node.kids[0]));
        if (!algorithm.empty())
        {
            const Memory::Vector<int> &args = N(node.kids[1]).kids;
            if (algorithm == "findValue" || algorithm == "binarySearchValue" || algorithm == "countValue")
                return T(M_TypeInt());
            if (algorithm == "allValue" || algorithm == "anyValue") return T(M_TypeBool());
            if (algorithm == "sortValue" || algorithm == "filterValue")
                return args.empty() ? T(M_TypeInt()) : ExprType(args[0]);
            if (algorithm == "foldValue")
                return args.size() < 2u ? T(M_TypeInt()) : ExprType(args[1]);
            if (algorithm == "reduceValue")
            {
                /* 两参写法 `reduce(list, f)` 的结果就是回调的返回值。 */
                if (args.size() < 2u) return T(M_TypeInt());
                if (args.size() >= 3u) return ExprType(args[1]);
                const std::shared_ptr<MType> callback = ExprType(args[1]);
                return callback && callback->kind == MType::Func && callback->elem ? callback->elem : T(M_TypeInt());
            }
            if (algorithm == "sumValue")
            {
                if (args.empty()) return T(M_TypeInt());
                const std::shared_ptr<MType> list = ExprType(args[0]);
                return list && list->elem ? list->elem : T(M_TypeInt());
            }
            if (algorithm == "mapValue")
            {
                /* 结果表是 `std::lang::List<R>`，R 从回调的返回类型来。 */
                if (args.size() < 2u) return T(M_TypeInt());
                const std::shared_ptr<MType> callback = ExprType(args[1]);
                return callback && callback->kind == MType::Func && callback->elem ? MakeListType(callback->elem) : T(M_TypeInt());
            }
            return args.empty() ? T(M_TypeInt()) : ExprType(args[0]);
        }
        /* 内置函数：与运算符同一条路（add(a,b) 就是 a + b） */
        if (node.kids.size() >= 2u && !StandardCallName(N(node.kids[0])).empty())
        {
            const Memory::String name = StandardCallName(N(node.kids[0]));
            if (name == "__move")
            {
                /* `move x`：指针 / 引用自动解引用一层，移动的是它指向 / 引用的那个对象。
                 * 这里只管类型与非只读校验，具体地址由降级阶段算（见 LowerLanguageMove）。 */
                const auto &args = N(node.kids[1]).kids;
                if (args.size() != 1u)
                {
                    Fail("move 需要一个操作数");
                    return T(M_TypeInt());
                }
                const auto source = RawExprType(args[0]);
                if (source && source->kind == MType::Ptr)
                {
                    if (!node.implicitExitMove && (source->readOnly || IsReadOnlyAccess(args[0])))
                    {
                        Fail("move 不能作用于只读引用（T const&）");
                        return T(M_TypeInt());
                    }
                    return source->elem;
                }
                const auto operand = N(args[0]);
                const bool place = operand.kind == MNode::Var || operand.kind == MNode::Member || operand.kind == MNode::Arrow ||
                                   operand.kind == MNode::Index;
                if (!place)
                {
                    Fail("move 的目标必须是可写的变量、self、成员、下标或指针目标");
                    return T(M_TypeInt());
                }
                if (!node.implicitExitMove && IsReadOnlyAccess(args[0]))
                {
                    Fail("move 不能作用于只读绑定（let / T const&）");
                    return T(M_TypeInt());
                }
                return ExprType(args[0]);
            }
            /* `str(x)`：转成字符串（`bint` / 数值 / 字符串本身） */
            if (name == "str") return T(M_TypeString());
            if (name == "address") return T(M_TypeIntBits(64, 1));
            /* `bint(x)`：构造大整数（整数 / 浮点 / 十进制文本）；
             * `std::lang::BigInteger(x)` / `std::lang::BigDecimal(x)` 是同一个东西的限定名写法。 */
            if (name == "bint" || name == "BigInteger") return T(M_TypeBigInt());
            if (name == "bfloat" || name == "BigDecimal") return T(M_TypeBigFloat());
            /* 内存内建：memcpy / memset 是过程（`new` / `drop` 是关键字，不走这里） */
            if (name == "memcpy" || name == "memset") return T(M_TypeInt());
            if (name == "print" || name == "println" || name == "putChar")
                return T(M_TypeInt());
            /* `std::lang::pause()`：调试暂停，降成 VM 内建 `__std_pause`，返回 0。 */
            if (name == "pause") return T(M_TypeInt());
            /* `std::lang::format("…", …)`：按 `{}` 占位符拼出的**新串**（和 `std::lang::print` 同规则）。 */
            if (name == "format") return T(M_TypeString());
            if (name == "inputLine") return T(M_TypeString());
            if (name == "inputInt") return T(M_TypeInt());
            if (name == "inputUInt") return T(M_TypeIntBits(64, 1));
            if (name == "inputFloat") return T(M_TypeDouble());
            if (name == "inputBool") return T(M_TypeBool());
            if (name == "inputChar") return T(M_TypeChar());
            if (name == "inputBigInteger") return T(M_TypeBigInt());
            if (name == "inputBigDecimal") return T(M_TypeBigFloat());
            if (name == "fopen" || name == "__fileTryOpen") return T(M_TypeFile());
            if (name == "__fileValid") return T(M_TypeBool());
            if (name == "fread" || name == "fileRead" || name == "__fileRead" || name == "processRead" || name == "__processRead")
                return T(M_TypeString());
            if (name == "fwrite" || name == "fileWrite" || name == "__fileWrite" || name == "feof" || name == "fileEof" || name == "__fileEof" || name == "fclose" || name == "fileClose" || name == "__fileClose")
                return T(M_TypeInt());
            if (name == "prun" || name == "processRun" || name == "__processRun")
                return T(M_TypeProc());
            if (name == "pread") return T(M_TypeString());
            if (name == "pclose" || name == "processClose" || name == "__processClose" || name == "psystem" || name == "system" || name == "__system")
                return T(M_TypeInt());
            if (name == "panic") return T(M_TypeInt());
            /* `len(a)`：元素个数（int） */
            if (name == "len") return T(M_TypeInt());
            const int builtin = BuiltinOpIndex(name);
            const Memory::Vector<int> &args = N(node.kids[1]).kids;
            if (builtin == -2) /* index(基址, 下标) */
            {
                if (args.size() == 2u)
                {
                    const std::shared_ptr<MType> base = ExprType(args[0]);
                    if (base->kind == MType::Array || base->kind == MType::Ptr) return base->elem;
                }
                return T(M_TypeInt());
            }
            if (builtin >= 0)
            {
                if (args.size() == 2u)
                {
                    /* 有 struct 操作数时，结果类型由重载方法的返回类型决定 */
                    const std::shared_ptr<MType> leftType = ExprType(args[0]);
                    const std::shared_ptr<MType> rightType = ExprType(args[1]);
                    const MOperatorDecl *operatorDecl = nullptr;
                    if (FindOperator(kOperatorText[builtin], leftType, rightType, &operatorDecl))
                        return operatorDecl->result;
                    // 比较内建与对应运算符一样返回 bool，供逻辑表达式和条件使用。
                    if (builtin >= 2 && builtin <= 7) return T(M_TypeBool());
                    /* 算术：任一边是 float64 就按 float64 算。 */
                    if (builtin >= 8 && builtin <= 12 && (leftType->kind == MType::Double || rightType->kind == MType::Double))
                    {
                        return T(M_TypeDouble());
                    }
                }
                return T(M_TypeInt());
            }
        }
        /* 函数指针调用：返回类型就是那个签名里的返回类型。 */
        if (node.kids.size() >= 2u && IsIndirectCallee(node.kids[0]))
        {
            return ExprType(node.kids[0])->elem;
        }
        Memory::String label;
        Memory::Vector<Memory::String> params;
        std::shared_ptr<MType> result;
        if (ResolveCall(id, &label, &params, &result))
        {
            /* 重载解析成功后结果类型确定（方法调用在推断阶段不做上下文定型），
             * 钉在节点上供后续查询直接命中，避免链式调用指数级重推。 */
            g_nodes[static_cast<std::size_t>(id)].type = M_AddType(result);
            return result;
        }
        return T(M_TypeInt());
    }
    case MNode::Bin:
    {
        /* 未定型的整数字面量先向**另一侧**的类型看齐（两侧都试一次，`1 + x` 与 `x + 1`
         * 一个待遇）：`x + 1`（`x` 是 int32）的结果是 int32，而不是默认的 int64。 */
        if (LiteralAdaptingOperator(node.s))
        {
            PinLiteralTowards(node.kids[0], node.kids[1]);
            PinLiteralTowards(node.kids[1], node.kids[0]);
        }
        /* 推断期已经按上下文把运算符声明钉住过（见 InferExpression）：直接用它，别再重挑。
         * 运算符接口允许「同一对操作数、不同返回类型」（`Add<int64,int64>` 与
         * `Add<Vec,int64>` 共存），那种情形光看操作数挑不出来。 */
        if (node.type >= 0) return T(node.type);
        const std::shared_ptr<MType> left = ExprType(node.kids[0]);
        const std::shared_ptr<MType> right = ExprType(node.kids[1]);
        /* 字符串：`+` 拼接成**新串**（类型还是 string），比较按字典序（结果是 int）。 */
        if (IsStandardOpcode(left, "String") || IsStandardOpcode(right, "String"))
        {
            return (node.s == "+") ? T(M_TypeString()) : T(M_TypeBool());
        }
        if (IsStandardOpcode(left, "BigDecimal") || IsStandardOpcode(right, "BigDecimal"))
        {
            const bool arithmetic = (node.s == "+" || node.s == "-" || node.s == "*" || node.s == "/");
            return arithmetic ? T(M_TypeBigFloat()) : T(M_TypeBool());
        }
        /* 大整数：`+ - * / %` 的结果还是 `bint`（另一边会自动转过来），比较是 int。 */
        if (IsStandardOpcode(left, "BigInteger") || IsStandardOpcode(right, "BigInteger"))
        {
            const bool arithmetic = (node.s == "+" || node.s == "-" || node.s == "*" || node.s == "/" || node.s == "%");
            return arithmetic ? T(M_TypeBigInt()) : T(M_TypeBool());
        }
        const MOperatorDecl *operatorDecl = nullptr;
        if (FindOperator(node.s, left, right, &operatorDecl)) return operatorDecl->result;
        if (left->kind == MType::Host || right->kind == MType::Host)
        {
            if ((node.s == "==" || node.s == "!=") && SameType(left, right))
                return T(M_TypeBool());
            FailNoOverload(node.s, left, right);
            return T(M_TypeInt());
        }
        /* struct 操作数没有对应方向的重载：这里就报出来（报错顺序才不会跑到外层去）。 */
        if (left->kind == MType::Struct || right->kind == MType::Struct)
        {
            FailNoOverload(node.s, left, right);
            return T(M_TypeInt());
        }
        /* 引用算术：结果还是那个引用类型（`p + 1` / `p - 1` / `1 + p`） */
        if ((node.s == "+" || node.s == "-") && left->kind == MType::Ptr) return left;
        if (node.s == "+" && right->kind == MType::Ptr) return right;
        const bool compare = (node.s == "==" || node.s == "!=" || node.s == "<" || node.s == "<=" || node.s == ">" || node.s == ">=" || node.s == "&&" || node.s == "||");
        if (compare) return T(M_TypeBool());
        /* 算术结果的类型：有 float64 就是 float64，否则取更宽的整型 */
        return NumericResult(left, right);
    }
    case MNode::Update:
    case MNode::Assign: return ExprType(node.kids[0]);
    default: return T(M_TypeInt());
    }
}
