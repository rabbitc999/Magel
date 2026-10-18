/* ---------- 第二遍：降级 ---------- */

/**
 * @brief 把结构体的**值字段摊平成槽位**：每个槽一个"叶子"，并记下每个字段的起始槽。
 *
 * 结构体字段是**值语义**：`struct Box { Point inner; int64 tag; }` 里的 `inner` 就是
 * Box 自己的两个槽（`inner.x` / `inner.y`），不是"一个槽存着某个共享对象的句柄"
 * —— 那样两个 Box 会共用同一份 inner（改一个另一个也变）。摊平以后：
 *
 *   * 数据区里每个 Box 都真的有一份 inner 的地方（`arr[3]` 里各一份）；
 *   * `a.inner = p` 是一次深拷贝（和结构体变量赋值同一条路）；
 *   * `sizeof(Box)` 也就对了（3 个槽）。
 *
 * 叶子名带上外层字段名（`inner.x`）：数据区正文 / 反汇编里看得懂，也不会和外层撞。
 * 引用字段（`Point mut&`）**不往下钻**：它本来就只占一个槽（存地址），指哪是哪。
 *
 * @param name struct 名。
 * @param stack 正在展开的 struct 名（用来报"值字段绕回自己"）。
 * @return 没问题返回 true。
 */
bool FlattenStructFields(const Memory::String &name, Memory::Vector<Memory::String> *stack)
{
    auto found = g_structs.find(name);
    if (found == g_structs.end()) return Fail("没有声明的结构体：" + name);
    MStructDef &definition = found->second;
    if (definition.flattened) return true;
    if (std::find(stack->begin(), stack->end(), name) != stack->end())
    {
        return Fail("struct " + name + " 的值字段绕回了自己：值字段要占地方，装不下自己；"
                                       "要指自己就写引用字段（`"
                    + name + " mut&`）");
    }
    stack->push_back(name);
    int slot = 0;
    definition.leaves.clear();
    for (MField &field : definition.fields)
    {
        if (!field.type) return Fail("struct " + name + " 的字段 " + field.name + " 没有类型");
        field.slot = slot;
        if (field.type->kind == MType::Struct)
        {
            if (!FlattenStructFields(field.type->name, stack)) return false;
            /* 只读查找：不能用 `operator[]`（键不存在会建一条空壳，见 FindStruct 注释）。 */
            const MStructDef *inner = FindStruct(field.type->name);
            if (inner == nullptr) return Fail("找不到结构体 " + field.type->name), false;
            field.slots = inner->slotCount;
            for (const auto &leaf : inner->leaves)
            {
                definition.leaves.emplace_back(field.name + "." + leaf.first, leaf.second);
            }
        }
        else if (field.type->kind == MType::Array && !field.type->dims.empty())
        {
            long long count = 1;
            for (const int dimension : field.type->dims)
            {
                count *= dimension;
                if (count > static_cast<long long>(g_runtimeMemory / 8u)) return Fail("定长数组字段超过当前内存预算：" + name + "." + field.name);
            }
            const auto element = field.type->elem;
            if (element->kind == MType::Struct && !FlattenStructFields(element->name, stack)) return false;
            const auto inner = element->kind == MType::Struct ? FindStruct(element->name) : nullptr;
            const auto slots = count * (inner ? inner->slotCount : 1);
            if (slots > static_cast<long long>(g_runtimeMemory / 8u)) return Fail("定长数组字段超过当前内存预算：" + name + "." + field.name);
            field.slots = static_cast<int>(slots);
            for (int index = 0; index < count; ++index)
            {
                const auto name = field.name + "." + Memory::toString(index);
                if (inner)
                    for (const auto &leaf : inner->leaves) definition.leaves.emplace_back(name + "." + leaf.first, leaf.second);
                else
                    definition.leaves.emplace_back(name, element);
            }
        }
        else
        {
            field.slots = 1;
            definition.leaves.emplace_back(field.name, field.type);
        }
        slot += field.slots;
    }
    definition.slotCount = slot;
    definition.flattened = true;
    stack->pop_back();
    return true;
}

/**
 * @brief 建立元组类型所需的字段布局。
 * @param type 类型描述。
 * @retval 满足检查条件或操作完成时为 true，检查失败或无法完成时为 false。
 */
bool EnsureTupleLayout(const std::shared_ptr<MType> &type)
{
    if (!IsTupleType(type)) return true;
    EnsureTupleDefinition(type);
    if (g_structType.count(type->name) != 0u) return true;
    Memory::Vector<Memory::String> stack;
    if (!FlattenStructFields(type->name, &stack)) return false;
    const MStructDef *definition = FindStruct(type->name);
    if (definition == nullptr) return Fail("找不到结构体 " + type->name), false;
    Memory::Vector<std::pair<Memory::String, simple_ast::TypePtr>> fields;
    for (const auto &leaf : definition->leaves)
        fields.emplace_back(leaf.first, LowerType(leaf.second, type->name + "." + leaf.first));
    const simple_ast::TypePtr lowered = simple_ast::Type::structOf(type->name, fields);
    g_structType[type->name] = lowered;
    if (g_loweringProgram != nullptr) g_loweringProgram->types.push_back(lowered);
    return true;
}

/**
 * @brief 为已登记结构体构造低层类型布局。
 * @param program 待生成或执行的程序。
 * @retval 满足检查条件或操作完成时为 true，检查失败或无法完成时为 false。
 */
bool BuildStructTypes(simple_ast::Program &program)
{
    for (auto &item : g_structs)
    {
        for (MField &field : item.second.fields)
        {
            if (!CheckDeclaredType(field.type, item.first + "." + field.name)) return false;
        }
        Memory::Vector<Memory::String> stack;
        if (!FlattenStructFields(item.first, &stack)) return false;
    }
    for (const auto &item : g_structs)
    {
        /* 登记给 simple_ast 的字段表就是**摊平后的槽位表**：数据区大小、字段偏移
         * （`Node::member` 的 `index * 8`）全都自动对齐。 */
        Memory::Vector<std::pair<Memory::String, simple_ast::TypePtr>> fields;
        for (const auto &leaf : item.second.leaves)
        {
            fields.emplace_back(leaf.first,
                                LowerType(leaf.second, item.first + "." + leaf.first));
        }
        g_structType[item.first] = simple_ast::Type::structOf(item.first, fields);
    }
    for (const auto &item : g_structType) program.types.push_back(item.second);
    for (const auto &item : g_staticFields)
    {
        if (!CheckDeclaredType(item.second.type, item.first)) return false;
        program.addVariable(item.second.emitted,
                            LowerType(item.second.type, item.second.emitted));
        const simple_ast::NodePtr init = item.second.hasFloatInitializer ? simple_ast::Node::assign(item.second.emitted,
                                                                                                    simple_ast::Node::f(item.second.floatInitializer)) :
                                                                           DefaultInit(item.second.emitted, item.second.type);
        if (init) g_globalInit.push_back(init);
    }
    /* 运算符重载不用 simple_ast 的 `a.方法(b)` 脱糖：那是"左操作数当 self"的成员形式，
     * 表达不了 `5 + a` 这种右操作数是 struct 的方向。magel 自己在 LowerBinary 里
     * 按左 / 右找出重载方法再发一次普通调用（四元式里同样看不到 `+`）。 */
    return true;
}

/**
 * @brief 为全局变量生成初始化及清理代码。
 * @param program 待生成或执行的程序。
 * @retval 满足检查条件或操作完成时为 true，检查失败或无法完成时为 false。
 */
bool LowerGlobals(simple_ast::Program &program)
{
    for (const int topId : g_top)
    {
        const MNode &top = N(topId);
        ReductionSourceScope source(topId);
        if (top.kind != MNode::Declare) continue;
        for (const int declId : N(top.kids[0]).kids)
        {
            const MNode &decl = N(declId);
            g_reductionLine = decl.sourceLine;
            if (!decl.kids.empty() && decl.type >= 0) InferExpression(decl.kids[0], T(decl.type));
            if (decl.kind != MNode::Declare) continue;
            /* 名字在 RegisterTop 里已经分配好（全局先占，形参 / 局部变量排在后面）。 */
            const Memory::String emitted = (g_globalNames.count(declId) != 0u) ? g_globalNames[declId] : decl.s;
            if (decl.type < 0 && decl.kids[0] < 0)
                return Fail("auto 变量必须给初值：" + decl.s);
            std::shared_ptr<MType> type = (decl.type < 0) ? ExprType(decl.kids[0]) : T(decl.type);
            Memory::Vector<int> dims;
            if (decl.i >= 0)
            {
                if (!ReadDims(static_cast<int>(decl.i), &dims)) return false;
                type = ArrayType(type, dims);
            }
            else if (decl.i == -2)
            {
                type = RuntimeArrayType(type); /* `T a[]` 写法 */
            }
            if (!CheckDeclaredType(type, decl.s)) return false;
            if (type->kind == MType::Ptr && decl.kids[0] < 0)
                return Fail(MAGEL_TEXT("引用声明必须初始化，缺省值请使用 Option<T mut&>"));
            if (IsRuntimeArray(type) && !InitIsRuntimeArray(decl.kids[0]))
            {
                Fail("全局的运行时数组只能用 `new [N]` 初始化：" + decl.s);
                return false;
            }

            program.addVariable(emitted, LowerType(type, emitted));
            const bool isConst = (decl.type2 != 0);
            if (isConst && decl.kids[0] < 0)
            {
                Fail("不可变变量（const）必须给初值：" + decl.s);
                return false;
            }
            /* 函数指针 / `new`：初始化时先告诉求值，目标是这个类型。 */
            const std::shared_ptr<MType> savedFuncType = g_expectedFuncType;
            const std::shared_ptr<MType> savedNewType = g_expectedNewType;
            g_expectedFuncType = (type->kind == MType::Func) ? type : nullptr;
            g_expectedNewType = type;
            MVar var;
            var.type = type;
            var.emitted = emitted;
            var.isConst = isConst;
            if (isConst) var.constantExpression = decl.kids[0];
            g_scope[0][ModuleSymbol(decl.module, decl.s)] = var;
            /*
             * 全局变量的所有权登记。
             *
             * 全局**不进**"出块自动释放"名单（`g_ownedScopes` 里根本没有全局那一层，
             * 硬塞进去会让每个 `return` 的 `AppendDropsAll` 都去还全局），只登记到
             * 停机清理清单：它是最外层（深度 = `g_cleanupBase`），所以停机时会软释放
             * 一次，并跑 `Drop` 析构。`T mut&` 这类"程序自己 `delete`"的仍然不登记。
             */
            if (IsOwnedKind(type))
                NoteCleanup(emitted, g_cleanupBase, IsRuntimeArray(type), false, false,
                            Memory::String());
            else if (type->kind == MType::Struct && StructNeedsCleanup(type->name))
            {
                if (StructHasOwnedLeaves(type->name)) MarkStructNeedsCleanup(type->name);
                NoteCleanup(emitted, g_cleanupBase, false,
                            StructHasInterface(type->name, "Drop"),
                            StructHasInterface(type->name, "Closeable"), type->name, StructHasOwnedLeaves(type->name));
            }
            if (decl.kids[0] >= 0 && type->kind == MType::Obj && ExprType(decl.kids[0])->kind != MType::Obj && !IsImplicitConversionValue(decl.kids[0], type))
            {
                g_globalInit.push_back(DefaultInit(emitted, type));
                g_globalInit.push_back(simple_ast::Node::exprStmt(
                    LowerObjSet(simple_ast::Node::var(emitted), decl.kids[0], "obj 全局初始化")));
            }
            else if (decl.kids[0] >= 0 && type->kind == MType::Struct && (SameStruct(type, ExprType(decl.kids[0])) || IsImplicitConversionValue(decl.kids[0], type)))
            {
                const simple_ast::NodePtr block = DefaultInit(emitted, type);
                if (block) g_globalInit.push_back(block);
                g_globalInit.push_back(StoreStructValue(simple_ast::Node::var(emitted),
                                                        decl.kids[0], LowerValueForType(decl.kids[0], type), type));
            }
            else
            {
                const simple_ast::NodePtr init =
                    (decl.kids[0] >= 0) ? simple_ast::Node::assign(
                                              emitted, IsObjectLike(type) ? MaterializeObjectValue(decl.kids[0], type) : MaterializeStringValue(decl.kids[0], type)) :
                                          DefaultInit(emitted, type, true);
                if (init) g_globalInit.push_back(init);
            }
            if (g_addressTakenNames.count(decl.s) && NeedsReferenceStorage(type))
            {
                const auto storage = AllocateName("__magel_global_reference");
                program.addVariable(storage, simple_ast::Type::i64());
                g_globalInit.push_back(simple_ast::Node::assign(storage, simple_ast::Node::arrayNew(
                                                                             LowerType(ArrayType(type, { 1 }), storage), { simple_ast::Node::i(0) })));
                g_globalInit.push_back(simple_ast::Node::storeAt(simple_ast::Node::var(storage), simple_ast::Node::var(emitted), LowerType(type, storage)));
                auto &reference = g_scope[0][ModuleSymbol(decl.module, decl.s)];
                g_referenceStorage[emitted] = storage;
                reference.emitted = storage;
                reference.elementReference = true;
            }
            g_expectedFuncType = savedFuncType;
            g_expectedNewType = savedNewType;
        }
    }
    return true;
}

/**
 * @brief 生成一个**接口方法的分派函数**：`q.area()` 就是调它。
 *
 * 接口引用是「地址 + 类型号」的**胖引用**（值在运行期对象表里，和 `obj` 同一个表示），
 * 所以分派只要读一下类型号就能挑到具体实现 —— 生成出来的函数长这样（伪码）：
 *
 *     int __magel_iface.Shape.area(胖引用 __magel_self) {
 *         __magel_t = __vm_obj_name(__magel_self)            // 读出盒子里那个类型号
 *         if (__magel_t == __typeTag(Circle mut&)) { return Circle.area(__vm_obj_addr(__magel_self)) }
 *         if (__magel_t == __typeTag(Square mut&)) { return Square.area(__vm_obj_addr(__magel_self)) }
 *         __vm_iface_fail(__magel_t, "Shape.area")        // 空引用 / 没实现 → 运行期错误
 *         return 0
 *     }
 *
 * 每个 `(接口, 方法)` 只生成一份（第一次被调用时登记，收尾时统一生成）。
 * 实现者列表来自 `g_structs` 的 `interfaces` —— 也就是"谁写了 `struct X : Shape`"。
 *
 * @param iface 接口名。
 * @param methodName 方法名。
 * @param implicitOperand 反射一元调用隐式以接收者提供唯一操作数。
 * @return 分派函数。
 */
simple_ast::Function MakeInterfaceDispatcher(const Memory::String &iface,
                                             const Memory::String &methodName, bool implicitOperand = false)
{
    using namespace simple_ast;
    std::shared_ptr<MType> declaredResult;
    Memory::Vector<std::shared_ptr<MType>> declaredParams;
    FindInterfaceMethod(iface, methodName, &declaredResult, &declaredParams);
    MInterfaceMethod required;
    required.name = methodName;
    required.result = declaredResult;
    required.params = declaredParams;

    Function function;
    function.name = InterfaceLabel(iface, methodName);
    if (implicitOperand) function.name += "$reflection_unary";
    function.result = LowerType(declaredResult, function.name);
    function.parameters.emplace_back("__magel_self", Type::i64());
    function.parameters.emplace_back("__magel_readonly", Type::i64());
    Memory::Vector<Memory::String> argNames;
    for (std::size_t index = 0; !implicitOperand && index < declaredParams.size(); ++index)
    {
        const Memory::String name = "__magel_a" + Memory::toString(index);
        argNames.push_back(name);
        function.parameters.emplace_back(name, LowerType(declaredParams[index], name));
    }

    Memory::Vector<NodePtr> body;
    body.push_back(Node::declare("__magel_t", Type::i64()));
    body.push_back(Node::assign(
        "__magel_t", Node::call("__vm_obj_name", { "__magel_o" }, { Node::var("__magel_self") })));
    for (const auto &item : g_structs)
    {
        if (!ImplementsInterface(item.first, iface)) continue;
        if (implicitOperand && (declaredParams.size() != 1 || !SameType(declaredParams[0], NamedType(item.first)))) continue;
        const MMethod *implementation = nullptr;
        for (const MMethod &method : item.second.methods)
        {
            if (method.superseded) continue;
            if (!SameInterfaceSignature(required, method)) continue;
            implementation = &method;
            break;
        }
        if (implementation == nullptr) continue; /* 上面核对过，不该发生 */
        /* 调用点：`self` = 胖引用里那个地址，后面按位置传 —— 和方法调用一个规矩。 */
        Memory::Vector<Memory::String> callNames{ "self" };
        Memory::Vector<NodePtr> callValues{
            Node::call("__vm_obj_addr", { "__magel_o" }, { Node::var("__magel_self") })
        };
        if (implicitOperand)
        {
            callNames.push_back(implementation->params[0].emitted);
            callValues.push_back(Node::call("__vm_obj_addr", { "__magel_o" }, { Node::var("__magel_self") }));
        }
        for (std::size_t index = 0; index < argNames.size(); ++index)
        {
            callNames.push_back(implementation->params[index].emitted);
            callValues.push_back(Node::var(argNames[index]));
        }
        const std::shared_ptr<MType> named = NamedType(item.first);
        /* `const&` 与 `mut&` 两种都认（可写性由接它的变量说了算）。 */
        NodePtr condition = Node::binary("==", Node::var("__magel_t"),
                                         Node::i(TypeIdOf(PointerType(named, false))));
        condition = Node::binary(
            "||", condition,
            Node::binary("==", Node::var("__magel_t"), Node::i(TypeIdOf(PointerType(named, true)))));
        Memory::Vector<NodePtr> dispatch;
        Memory::Set<Memory::String> visiting;
        if (methodName == "drop" || MethodMutatesReceiver(item.first, *implementation, &visiting))
        {
            dispatch.push_back(Node::ifThenElse(Node::var("__magel_readonly"),
                                                Node::exprStmt(Node::call("__vm_panic", { "__magel_message" },
                                                                          { Node::str("只读接收者不能调用修改方法：" + iface + "." + methodName) })),
                                                nullptr));
        }
        if (methodName == "drop" && declaredParams.empty() && StructHasInterface(item.first, "Drop"))
        {
            // 接口显式销毁与具体类型调用一致，必须清理资源叶子，不能只执行用户析构体。
            dispatch.push_back(FinishExplicitDrop(item.first, {}, callValues.front(), nullptr));
            dispatch.push_back(Node::ret(nullptr));
        }
        else
        {
            dispatch.push_back(Node::ret(Node::call(item.first + "." + implementation->name,
                                                    callNames, callValues)));
        }
        body.push_back(Node::ifThenElse(
            condition,
            Node::block(dispatch),
            nullptr));
    }
    body.push_back(Node::exprStmt(Node::call("__vm_iface_fail", { "__magel_t", "__magel_name" },
                                             { Node::var("__magel_t"),
                                               Node::str(iface + "." + methodName) })));
    body.push_back(Node::ret(IsFloatType(declaredResult) ? Node::f(0.0) : Node::i(0)));
    function.body = Node::block(body);
    return function;
}

/**
 * @brief 收尾：给每个被调用过的接口方法补上分派函数（没被调用的不生成，产物才干净）。
 * @param program 待生成或执行的程序。
 */
void AppendInterfaceDispatchers(simple_ast::Program &program)
{
    for (const Memory::String &key : g_usedInterfaceMethods)
    {
        const std::size_t bar = key.find('|');
        if (bar == Memory::String::npos) continue;
        const Memory::String iface = key.substr(0u, bar);
        auto method = key.substr(bar + 1u);
        const auto suffix = method.find('|');
        const bool implicitOperand = suffix != Memory::String::npos;
        if (implicitOperand) method = method.substr(0, suffix);
        program.functions.push_back(MakeInterfaceDispatcher(iface, method, implicitOperand));
    }
}

/**
 * @brief 把"一个值"按叶子类型存进聚合槽：字符串 / 对象复制一份，标量原样写。
 *
 * 字符串 / 对象的空值（0）原样写 0：空值没有资源可复制，也不能让
 * `__vm_object_copy(0)` / `__vm_str_sub(0, …)` 报错或白分配。
 */
void AppendLeafStore(Memory::Vector<simple_ast::NodePtr> *body,
                     const simple_ast::NodePtr &dst, const simple_ast::NodePtr &value,
                     const std::shared_ptr<MType> &leaf)
{
    using simple_ast::Node;
    using simple_ast::Type;
    if (leaf && (leaf->kind == MType::String || IsObjectLike(leaf)))
    {
        const Memory::String temp = AllocateName("__magel_agg_leaf");
        body->push_back(Node::declare(temp, Type::i64()));
        body->push_back(Node::assign(temp, value));
        const simple_ast::NodePtr copied =
            leaf->kind == MType::String ? Node::call("__vm_str_sub", { "__magel_s", "__magel_a", "__magel_b" },
                                                     { Node::var(temp), Node::i(0), Node::i(-1) }) :
                                          Node::call(ObjectCopyFunction(leaf), { "__magel_v" }, { Node::var(temp) });
        body->push_back(Node::ifThenElse(
            Node::binary("!=", Node::var(temp), Node::i(0)),
            Node::block({ Node::storeAt(dst, copied, Type::i64()) }),
            Node::block({ Node::storeAt(dst, Node::i(0), Type::i64()) })));
        return;
    }
    const simple_ast::TypePtr slot = IsFloatType(leaf) ? Type::f64() : Type::i64();
    body->push_back(Node::storeAt(dst, value, slot));
}

/**
 * @brief 从"槽地址"深拷贝一个叶子：先按整数把槽读出来，再按叶子类型存进目标槽。
 * @param body 待处理的函数体或循环体。
 * @param dst 接收复制结果的目标表达式。
 * @param src 压缩数据的起始地址。
 * @param leaf 待复制叶子字段的布局描述。
 */
void AppendLeafCopy(Memory::Vector<simple_ast::NodePtr> *body,
                    const simple_ast::NodePtr &dst, const simple_ast::NodePtr &src,
                    const std::shared_ptr<MType> &leaf)
{
    using simple_ast::Node;
    if (leaf && (leaf->kind == MType::String || IsObjectLike(leaf)))
    {
        AppendLeafStore(body, dst, Node::loadInt(src), leaf);
        return;
    }
    const simple_ast::TypePtr slot =
        IsFloatType(leaf) ? simple_ast::Type::f64() : simple_ast::Type::i64();
    body->push_back(Node::storeAt(dst, Node::loadFrom(src, slot), slot));
}

/**
 * @brief `__magel_agg_rel_<T>(addr)`：按叶子递归软释放聚合槽里归它所有的字符串 / 对象。
 * @param owner 所属对象或类型信息。
 * @retval 保存`__magel_agg_rel_<T>(addr)`：按叶子递归软释放聚合槽里归它所有的字符串 / 对象结果的对象。
 */
simple_ast::Function MakeAggregateReleaseFunction(const Memory::String &owner)
{
    using simple_ast::Node;
    using simple_ast::Type;
    simple_ast::Function function;
    function.name = AggregateReleaseName(owner);
    function.result = Type::i32();
    function.parameters = { { "__magel_agg", Type::i64() } };
    Memory::Vector<simple_ast::NodePtr> body;
    const MStructDef *definition = FindStruct(owner);
    if (definition != nullptr)
    {
        /* 逆序释放（和字段声明的逆序一致）。 */
        for (std::size_t index = definition->leaves.size(); index > 0u; --index)
        {
            if (!LeafOwnedByAggregate(definition->leaves[index - 1u].second)) continue;
            const simple_ast::NodePtr address = Node::offset(
                Node::var("__magel_agg"), Node::i(static_cast<std::int64_t>(index - 1u)), 8);
            /* 释放后把槽清 0：同一个静态聚合对象被两次释放（或两个别名指向它）时，
             * 第二次 `drop 0` 是软释放空操作，不会二次释放同一块。 */
            body.push_back(Node::release(Node::loadInt(address)));
            body.push_back(Node::storeAt(address, Node::i(0), Type::i64()));
        }
    }
    body.push_back(Node::ret(Node::i(0)));
    function.body = Node::block(body);
    return function;
}

/**
 * @brief `__magel_agg_cpy_<T>(dst, src)`：聚合值的深拷贝（字符串 / 对象各复制一份）。
 * @param owner 所属对象或类型信息。
 * @retval 保存`__magel_agg_cpy_<T>(dst, src)`：聚合值的深拷贝（字符串 / 对象各复制一份）结果的对象。
 */
simple_ast::Function MakeAggregateCopyFunction(const Memory::String &owner)
{
    using simple_ast::Node;
    using simple_ast::Type;
    simple_ast::Function function;
    function.name = AggregateCopyName(owner);
    function.result = Type::i32();
    function.parameters = { { "__magel_agg_dst", Type::i64() }, { "__magel_agg_src", Type::i64() } };
    Memory::Vector<simple_ast::NodePtr> body;
    const MStructDef *definition = FindStruct(owner);
    if (definition != nullptr)
    {
        for (std::size_t index = 0; index < definition->leaves.size(); ++index)
        {
            const simple_ast::NodePtr dst = Node::offset(
                Node::var("__magel_agg_dst"), Node::i(static_cast<std::int64_t>(index)), 8);
            const simple_ast::NodePtr src = Node::offset(
                Node::var("__magel_agg_src"), Node::i(static_cast<std::int64_t>(index)), 8);
            AppendLeafCopy(&body, dst, src, definition->leaves[index].second);
        }
    }
    body.push_back(Node::ret(Node::i(0)));
    function.body = Node::block(body);
    return function;
}

/**
 * @brief 收尾：把用到过的聚合递归释放 / 深拷贝函数补进产物。
 * @param program 待生成或执行的程序。
 */
void AppendAggregateHelpers(simple_ast::Program &program)
{
    for (const Memory::String &owner : g_needsAggregateRelease)
        program.functions.push_back(MakeAggregateReleaseFunction(owner));
    for (const Memory::String &owner : g_needsAggregateCopy)
        program.functions.push_back(MakeAggregateCopyFunction(owner));
}

/**
 * @brief `__magel_arr_rel_<T>(arr, count)`：按元素释放运行时数组里的值。
 *
 * 数组元素和字段 / 容器元素一样是**值**：字符串、对象句柄逐个还回去，结构体走
 * `__magel_agg_rel_<T>` 递归释放它槽里的叶子。释放后把槽清 0（软释放重入是空操作）。
 * @param element 数组或容器的元素类型。
 * @retval 保存`__magel_arr_rel_<T>(arr, count)`：按元素释放运行时数组里的值。
 *
 * 数组元素和字段 / 容器元素一样是**值**：字符串、对象句柄逐个还回去，结构体走
 * `__magel_agg_rel_<T>` 递归释放它槽里的叶子。释放后把槽清 0（软释放重入是空操作）结果的对象。
 */
simple_ast::Function MakeArrayReleaseFunction(const std::shared_ptr<MType> &element)
{
    using simple_ast::Node;
    using simple_ast::Type;
    simple_ast::Function function;
    function.name = ArrayReleaseName(element);
    function.result = Type::i32();
    function.parameters = { { "__magel_arr", Type::i64() }, { "__magel_count", Type::i64() } };
    Memory::Vector<simple_ast::NodePtr> body;
    /* label / 局部变量按类型取名：四元式里的 label 是**全局**名字，两个助手共用
     * `__magel_arr_loop` 会互相串跳（同一次编译里生成多个数组释放函数时踩过）。 */
    const Memory::String stem = ArrayReleaseName(element);
    const Memory::String index = stem + "__i";
    const Memory::String loopLabel = stem + "__loop";
    const Memory::String doneLabel = stem + "__done";
    const int stride = (element->kind == MType::Struct) ? StructStride(element) : 8;
    body.push_back(Node::declare(index, Type::i64()));
    body.push_back(Node::assign(index, Node::i(0)));
    body.push_back(Node::label(loopLabel));
    body.push_back(Node::ifThenElse(
        Node::binary(">=", Node::var(index), Node::var("__magel_count")),
        Node::block({ Node::jump(doneLabel) }), Node::block({})));
    const simple_ast::NodePtr address =
        Node::offset(Node::var("__magel_arr"), Node::var(index), stride);
    if (element->kind == MType::Struct)
    {
        const auto cleanup = MakeAggregateCleanup(element, address);
        body.insert(body.end(), cleanup.begin(), cleanup.end());
    }
    else
    {
        body.push_back(Node::release(Node::loadInt(address)));
        body.push_back(Node::storeAt(address, Node::i(0), Type::i64()));
    }
    body.push_back(Node::assign(index, Node::binary("+", Node::var(index), Node::i(1))));
    body.push_back(Node::jump(loopLabel));
    body.push_back(Node::label(doneLabel));
    body.push_back(Node::ret(Node::i(0)));
    function.body = Node::block(body);
    return function;
}

/**
 * @brief 收尾：补运行时数组的按元素释放助手（要在聚合助手之前，它会登记用到的聚合类型）。
 * @param program 待生成或执行的程序。
 */
void AppendArrayReleaseHelpers(simple_ast::Program &program)
{
    for (const auto &item : g_needsArrayRelease)
        program.functions.push_back(MakeArrayReleaseFunction(item.second));
}

/**
 * @brief 生成枚举变体的构造函数。
 * @param enumName 枚举类型的名称。
 * @param variant 枚举变体名称或定义。
 * @retval 保存生成枚举变体的构造函数结果的对象。
 */
simple_ast::Function MakeEnumConstructor(const Memory::String &enumName,
                                         const MEnumVariantDef &variant)
{
    using simple_ast::Node;
    simple_ast::Function function;
    function.name = "__magel_enum_" + enumName + "_" + variant.name;
    function.result = LowerType(NamedType(enumName), function.name);
    Memory::Vector<Memory::String> parameterNames;
    for (std::size_t index = 0; index < variant.payloads.size(); ++index)
    {
        const Memory::String name = EnumPayloadParameterName(enumName, variant.name, index);
        parameterNames.push_back(name);
        function.parameters.emplace_back(name, LowerType(variant.payloads[index].type, name));
    }
    const MStructDef *storage = FindStruct(enumName);
    if (storage == nullptr)
        return Fail("找不到枚举的存储结构体 " + enumName), simple_ast::Function{};
    Memory::Vector<simple_ast::NodePtr> values;
    values.reserve(storage->leaves.size());
    for (const auto &leaf : storage->leaves)
        values.push_back(IsFloatType(leaf.second) ? Node::f(0.0) : Node::i(0));
    const MField *tagField = FindField(enumName, "__tag");
    if (tagField != nullptr)
        values[static_cast<std::size_t>(tagField->slot)] = Node::i(variant.tag);
    const Memory::String resultName = "__magel_enum_value";
    Memory::Vector<simple_ast::NodePtr> body;
    body.push_back(Node::declare(resultName, LowerType(NamedType(enumName), resultName)));
    body.push_back(Node::assign(resultName, Node::structNew(enumName, values)));
    for (std::size_t index = 0; index < variant.payloads.size(); ++index)
    {
        const MEnumPayload &payload = variant.payloads[index];
        const MField *field = FindField(enumName, payload.storageName);
        if (field == nullptr) continue;
        if (payload.type->kind == MType::Struct)
        {
            const auto inner = g_structs.find(payload.type->name);
            if (inner == g_structs.end()) continue;
            for (int slot = 0; slot < field->slots; ++slot)
            {
                const std::shared_ptr<MType> leaf =
                    inner->second.leaves[static_cast<std::size_t>(slot)].second;
                const simple_ast::NodePtr source = Node::offset(
                    Node::var(parameterNames[index]), Node::i(slot), 8);
                const simple_ast::NodePtr target = Node::offset(
                    Node::var(resultName), Node::i(field->slot + slot), 8);
                AppendLeafCopy(&body, target, source, leaf);
            }
        }
        else
        {
            const simple_ast::NodePtr address = Node::offset(
                Node::var(resultName), Node::i(field->slot), 8);
            AppendLeafStore(&body, address, Node::var(parameterNames[index]), payload.type);
        }
    }
    body.push_back(Node::ret(Node::var(resultName)));
    function.body = Node::block(body);
    return function;
}

/**
 * @brief 将枚举变体构造函数追加到程序中。
 * @param program 待生成或执行的程序。
 */
void AppendEnumConstructors(simple_ast::Program &program)
{
    for (const auto &item : g_enums)
        for (const MEnumVariantDef &variant : item.second.variants)
            program.functions.push_back(MakeEnumConstructor(item.first, variant));
}

/**
 * @brief 生成元组类型构造函数的内部符号名。
 * @param tupleName 元组类型的内部名称。
 * @retval 生成或读取的结果文本。
 */
Memory::String TupleConstructorName(const Memory::String &tupleName)
{
    return "__magel_make" + tupleName;
}

/**
 * @brief 为元组字段生成构造函数。
 * @param tupleName 元组类型的内部名称。
 * @param elements 元素类型或元素值列表。
 * @retval 保存为元组字段生成构造函数结果的对象。
 */
simple_ast::Function MakeTupleConstructor(
    const Memory::String &tupleName,
    const Memory::Vector<std::shared_ptr<MType>> &elements)
{
    using simple_ast::Node;
    simple_ast::Function function;
    const std::shared_ptr<MType> tuple = NamedType(tupleName);
    function.name = TupleConstructorName(tupleName);
    function.result = LowerType(tuple, function.name);
    Memory::Vector<Memory::String> parameters;
    for (std::size_t index = 0; index < elements.size(); ++index)
    {
        const Memory::String name = "__magel_tuple_arg_" + Memory::toString(index);
        parameters.push_back(name);
        function.parameters.emplace_back(name, LowerType(elements[index], name));
    }
    const MStructDef *storage = FindStruct(tupleName);
    if (storage == nullptr)
        return Fail("找不到元组的存储结构体 " + tupleName), simple_ast::Function{};
    Memory::Vector<simple_ast::NodePtr> zeros;
    for (const auto &leaf : storage->leaves)
        zeros.push_back(IsFloatType(leaf.second) ? Node::f(0.0) : Node::i(0));
    const Memory::String result = "__magel_tuple_value";
    Memory::Vector<simple_ast::NodePtr> body{
        Node::declare(result, function.result),
        Node::assign(result, Node::structNew(tupleName, zeros)),
    };
    for (std::size_t index = 0; index < elements.size(); ++index)
    {
        const MField *field = FindField(tupleName, "__" + Memory::toString(index));
        if (field == nullptr) continue;
        if (elements[index]->kind == MType::Struct)
        {
            const MStructDef *inner = FindStruct(elements[index]->name);
            if (inner == nullptr) continue; /* 同 FindField：找不到就跳过，不建空壳 */
            for (int slot = 0; slot < field->slots; ++slot)
            {
                const std::shared_ptr<MType> leaf =
                    inner->leaves[static_cast<std::size_t>(slot)].second;
                AppendLeafCopy(
                    &body,
                    Node::offset(Node::var(result), Node::i(field->slot + slot), 8),
                    Node::offset(Node::var(parameters[index]), Node::i(slot), 8), leaf);
            }
        }
        else
        {
            AppendLeafStore(&body,
                            Node::offset(Node::var(result), Node::i(field->slot), 8),
                            Node::var(parameters[index]), elements[index]);
        }
    }
    body.push_back(Node::ret(Node::var(result)));
    function.body = Node::block(std::move(body));
    return function;
}

/**
 * @brief 生成字面量构造函数；元素按源码顺序插入并复制进独立容器。
 * @param program 接收构造函数的程序。
 */
void AppendCollectionConstructors(simple_ast::Program &program)
{
    using simple_ast::Node;
    for (const auto &item : g_collectionConstructors)
    {
        const auto type = item.second.type;
        const bool map = type->name == "map";
        /* `List<T>` 的 `{a, b, c}` 走 `__vm_vec_push`（顺序追加）；`Set<T>` 走
         * `__vm_set_add`（去重）、`Map<K,V>` 走 `__vm_map_put`（覆盖同键）。 */
        const bool list = type->name == "vec";
        simple_ast::Function function;
        function.name = item.first;
        function.result = LowerType(type, item.first);
        const auto result = item.first + "_result";
        Memory::Vector<simple_ast::NodePtr> body{
            Node::declare(result, function.result), Node::assign(result, NewContainerValue(type))
        };
        for (std::size_t i = 0; i < item.second.count; ++i)
        {
            const auto element = map && i % 2u ? type->elem2 : type->elem;
            const auto name = item.first + "_arg_" + Memory::toString(i);
            function.parameters.emplace_back(name, LowerType(element, name));
        }
        const std::size_t step = map ? 2u : 1u;
        for (std::size_t i = 0; i < item.second.count; i += step)
        {
            if (list)
            {
                Memory::Vector<Memory::String> names{ item.first + "_native_container", item.first + "_native_value",
                                                      item.first + "_native_text", item.first + "_native_object" };
                Memory::Vector<simple_ast::NodePtr> values{ Node::var(result),
                                                            Node::var(item.first + "_arg_" + Memory::toString(i)), Node::i(type->elem->kind == MType::String),
                                                            ContainerStorageFlag(type->elem) };
                body.push_back(Node::exprStmt(Node::call("__vm_vec_push", names, values)));
                continue;
            }
            Memory::Vector<Memory::String> names{ item.first + "_native_container", item.first + "_native_key", item.first + "_native_text", item.first + "_native_object" };
            Memory::Vector<simple_ast::NodePtr> values{ Node::var(result),
                                                        Node::var(item.first + "_arg_" + Memory::toString(i)), Node::i(type->elem->kind == MType::String),
                                                        ContainerStorageFlag(type->elem) };
            if (map)
            {
                names.insert(names.end(), { item.first + "_native_value", item.first + "_native_value_text", item.first + "_native_value_object" });
                values.push_back(Node::var(item.first + "_arg_" + Memory::toString(i + 1u)));
                values.push_back(Node::i(type->elem2->kind == MType::String));
                values.push_back(ContainerStorageFlag(type->elem2));
            }
            body.push_back(Node::exprStmt(Node::call(map ? "__vm_map_put" : "__vm_set_add", names, values)));
        }
        body.push_back(Node::ret(Node::var(result)));
        function.body = Node::block(std::move(body));
        program.functions.push_back(std::move(function));
    }
}

/**
 * @brief 生成运行时调用的 Map 键 Equal / Hash 包装函数。
 * @param program 接收包装函数的程序。
 */
void AppendMapKeyPolicies(simple_ast::Program &program)
{
    using simple_ast::Node;
    for (const auto &item : g_mapKeyPolicies)
    {
        const auto &policy = item.second;
        simple_ast::Function equal;
        equal.name = policy.equalLabel;
        const auto left = equal.name + "_left", right = equal.name + "_right";
        equal.parameters = { { left, simple_ast::Type::i64() }, { right, simple_ast::Type::i64() } };
        equal.result = LowerType(policy.equal.result, equal.name);
        const auto first = Node::var(policy.equal.receiverLeft ? left : right);
        const auto second = Node::var(policy.equal.receiverLeft ? right : left);
        equal.body = Node::block({ Node::ret(Node::call(policy.equal.label, policy.equal.callNames, { first, second })) });
        program.functions.push_back(std::move(equal));
        if (policy.hashLabel.empty()) continue;
        simple_ast::Function hash;
        hash.name = policy.hashLabel;
        const auto value = hash.name + "_value";
        hash.parameters = { { value, simple_ast::Type::i64() } };
        hash.result = simple_ast::Type::i64();
        hash.body = Node::block({ Node::ret(Node::call(policy.hash.label, policy.hash.callNames,
                                                       { Node::var(value), Node::var(value) })) });
        program.functions.push_back(std::move(hash));
    }
}

/**
 * @brief 将元组构造函数追加到程序中。
 * @param program 待生成或执行的程序。
 */
void AppendTupleConstructors(simple_ast::Program &program)
{
    for (const auto &item : g_tupleElements)
    {
        EnsureTupleLayout(NamedType(item.first));
        program.functions.push_back(MakeTupleConstructor(item.first, item.second));
    }
}

/**
 * @brief 为带参数入口保留无参 `main` 引导，并在返回前释放参数 List。
 * @param program 待生成或执行的程序。
 */
void AppendArgumentsMain(simple_ast::Program &program)
{
    using simple_ast::Node;
    for (const MFunctionDecl &declaration : g_functionDecls)
    {
        if (declaration.superseded || declaration.source != "main" || declaration.signature.params.empty())
            continue;
        simple_ast::Function wrapper;
        wrapper.name = "main";
        const bool voidMain = declaration.signature.result->kind == MType::Void;
        wrapper.result = voidMain ? simple_ast::Type::i64() : LowerType(declaration.signature.result, "main");
        const Memory::String arguments = "__magel_program_arguments";
        const Memory::String result = "__magel_program_result";
        Memory::Vector<simple_ast::NodePtr> body{
            Node::declare(arguments, LowerType(declaration.signature.params[0].type, arguments)),
            Node::assign(arguments, Node::call("__vm_program_arguments", {}, {})),
        };
        const auto call = Node::call(declaration.label, { declaration.signature.params[0].emitted }, { Node::var(arguments) });
        if (voidMain)
            body.push_back(Node::exprStmt(call));
        else
        {
            body.push_back(Node::declare(result, wrapper.result));
            body.push_back(Node::assign(result, call));
        }
        body.push_back(Node::exprStmt(Node::call("__vm_object_free", { "__magel_o" }, { Node::var(arguments) })));
        /* 带参数入口的引导也是"正常停机"的出口；全局清理由之后统一跑一次的
         * `InsertGlobalCleanupBeforeReturns` 补在 `ret` 前面。 */
        body.push_back(Node::ret(voidMain ? Node::i(0) : Node::var(result)));
        wrapper.body = Node::block(std::move(body));
        program.functions.push_back(std::move(wrapper));
        return;
    }
}

/**
 * @brief 把全局停机清理插进入口 `main` 的每条 `return` 之前。
 *
 * 放在**所有函数体都降级完**之后做：那时任何地方对全局写的 `g.close()` 都已经通过
 * `UnmarkDestructor` 反映到 `g_globalCleanupItems` 上，不会漏也不会重。
 * `return` 可能嵌在 `if` / 循环的块里，所以这里递归整棵语句树。
 */
simple_ast::NodePtr InsertGlobalCleanupBeforeReturns(
    const simple_ast::NodePtr &node,
    const Memory::Vector<simple_ast::NodePtr> &cleanup)
{
    if (node == nullptr || cleanup.empty()) return node;
    if (node->kind == simple_ast::Node::Kind::Return)
    {
        Memory::Vector<simple_ast::NodePtr> block(cleanup.begin(), cleanup.end());
        block.push_back(node);
        return simple_ast::Node::block(block);
    }
    for (simple_ast::NodePtr &kid : node->kids)
    {
        const simple_ast::NodePtr replaced = InsertGlobalCleanupBeforeReturns(kid, cleanup);
        if (replaced != kid) kid = replaced;
    }
    return node;
}

/**
 * @brief 给程序里每个入口 `main` 的每条出口补上全局停机清理（只对无主的 `main`）。
 * @param program 待生成或执行的程序。
 */
void AppendGlobalCleanupToEntryMain(simple_ast::Program &program)
{
    const Memory::Vector<simple_ast::NodePtr> cleanup = EmitGlobalCleanupStatements();
    if (cleanup.empty()) return;
    for (simple_ast::Function &function : program.functions)
    {
        if (function.name != "main" || !function.owner.empty()) continue;
        function.body = InsertGlobalCleanupBeforeReturns(function.body, cleanup);
    }
}

/**
 * @brief 为 static T new 生成返回拥有值的公开入口，构造体经 Construct 节点调用。
 * @param owner 所属具体类型。
 * @param method 构造签名。
 * @return 普通局部值返回的 AST 块。
 */
int NamedConstructorFactoryBody(const Memory::String &owner, const MMethod &method)
{
    const int type = M_AddType(NamedType(owner));
    const int arguments = M_List();
    for (const auto &param : method.params)
        M_ListAdd(arguments, M_Var(M_Name(param.source.c_str())));
    const auto local = AllocateName("__new_result");
    const int name = M_Name(local.c_str());
    /* 排除正在展开的这条工厂（`method.name` 是它的修饰名），免得工厂体递归调自己。 */
    const int declaration =
        M_Decl(name, -1, M_Construct(type, arguments, M_Name(method.name.c_str())));
    g_nodes[declaration].type = type;
    const int statements = M_List();
    M_ListAdd(statements, M_LocalDecl(type, M_List1(declaration), 0));
    M_ListAdd(statements, M_Return(M_Var(name)));
    return M_Block(statements);
}

/**
 * @brief 检查默认值是否非法引用形参或 self。
 * @param id 表达式。
 * @param params 形参列表。
 * @return 是否合法。
 */
bool ValidateDefaultNames(int id, const Memory::Vector<MParam> &params)
{
    const auto node = N(id);
    if (node.kind == MNode::Var)
    {
        if (node.s == "self") return Fail("默认参数不能引用 self");
        for (const auto &param : params)
            if (node.s == param.source) return Fail("默认参数不能引用形参：" + node.s);
        if (!FindIn(node.module, node.s) && !FindFunctionOverloads(node.module, node.s))
            return Fail("默认参数引用了未定义的名称：" + node.s);
    }
    if (node.kind == MNode::Call && !node.kids.empty() && N(node.kids[0]).kind == MNode::Var)
    {
        for (std::size_t i = 1; i < node.kids.size(); ++i)
            if (node.kids[i] >= 0 && !ValidateDefaultNames(node.kids[i], params)) return false;
        return true;  // 被调函数交给调用表达式的类型检查，允许内建函数。
    }
    for (int child : node.kids)
        if (child >= 0 && !ValidateDefaultNames(child, params)) return false;
    return true;
}
/**
 * @brief 在函数未被调用时也检查默认值的类型与名称。
 * @param params 形参列表。
 * @return 检查成功。
 */
bool ValidateDefaultArguments(const Memory::Vector<MParam> &params)
{
    for (const auto &param : params)
    {
        if (param.defaultExpr < 0) continue;
        MDefaultScope scope(param.defaultExpr);
        if (!ValidateDefaultNames(param.defaultExpr, params)) return false;
        InferExpression(param.defaultExpr, param.type);
        if (!Acceptable(param.type, RawExprType(param.defaultExpr), true))
            return Fail("默认参数 " + param.source + " 的类型与形参不匹配");
        // 借用普通函数降级检查完整表达式语义；检查函数不加入输出，也不会运行。
        const auto cleanup = g_cleanupItems;
        const auto globalCleanup = g_globalCleanupItems;
        const auto cleanupBase = g_cleanupBase;
        const int body = M_Block(M_List1(M_Return(param.defaultExpr)));
        LowerFunction("__magel_default_check", {}, param.type, {}, body, N(param.defaultExpr).module);
        g_cleanupItems = cleanup;
        g_globalCleanupItems = globalCleanup;
        g_cleanupBase = cleanupBase;
        if (!g_error.empty()) return false;
    }
    return g_error.empty();
}

/**
 * @brief 将已登记的完整程序降级为 simple_ast 表示。
 * @param program 待生成或执行的程序。
 * @retval 满足检查条件或操作完成时为 true，检查失败或无法完成时为 false。
 */
bool LowerProgram(simple_ast::Program &program)
{
    /* 解析期攒下的错（`typedef` 写错 / 别名重名）：挂成了 FptrDefBad 节点，这里报出来。 */
    for (const int topId : g_top)
    {
        const MNode &top = N(topId);
        ReductionSourceScope source(topId);
        if (top.kind == MNode::ParseBad) return Fail(top.s);
    }
    g_loweringProgram = nullptr;
    if (!BuildStructTypes(program)) return false;
    g_loweringProgram = &program;
    AppendEnumConstructors(program);
    /* 全局变量的停机清理清单（`g_ownedScopes[0]` 就是全局那层）。 */
    g_cleanupItems.clear();
    g_cleanupBase = g_ownedScopes.size();
    if (!LowerGlobals(program)) return false;
    for (const auto &entry : g_funcs)
        if (!ValidateDefaultArguments(entry.second.params)) return false;
    /*
     * 全局变量的停机清理项**先存快照**：`g_cleanupItems` 会被后面的函数降级
     * 反复清空，而 `g_globalCleanupItems` 要一直留到函数体全降级完。
     * 函数体里对全局写的 `g.close()` / `delete g;` 会通过 `UnmarkDestructor`
     * 改这份快照，所以展开语句必须等所有函数都降级完（见 `AppendGlobalCleanupToEntryMain`）。
     */
    g_globalCleanupItems = g_cleanupItems;
    Memory::Vector<int> pendingLambdas;
    /* 函数与方法：方法体里的 `self` 由 simple_ast 自动声明（`(decl, i64, local, self)`） */
    for (const int topId : g_top)
    {
        const MNode &top = N(topId);
        ReductionSourceScope source(topId);
        if (top.kind == MNode::FuncDef)
        {
            if (top.s.rfind("__magel_lambda_", 0) == 0)
            {
                pendingLambdas.push_back(topId);
                continue;
            }
            const std::size_t index = static_cast<std::size_t>(top.i);
            if (index >= g_functionDecls.size()) continue;
            const MFunctionDecl &declaration = g_functionDecls[index];
            if (declaration.superseded) continue; /* 被后面的定义覆盖掉了 */
            program.functions.push_back(LowerFunction(declaration.label, Memory::String(),
                                                      declaration.signature.result,
                                                      declaration.signature.params,
                                                      declaration.bodyId, declaration.module));
            continue;
        }
        if (top.kind != MNode::StructDecl && top.kind != MNode::EnumDecl) continue;
        const auto definition = g_structs.find(top.s);
        if (definition == g_structs.end()) continue;
        for (const MMethod &signature : definition->second.methods)
        {
            if (signature.superseded) continue; /* 被后面的定义覆盖掉了 */
            if (signature.bodyId < 0) continue;
            /* 方法名（`plus` / `loperator+`，重载时带 `$类型`）就是标签后缀：`结构体名.方法名` */
            if (signature.isNamedConstructor)
            {
                g_namedConstructorBody = true;
                auto body = LowerFunction(signature.name + "$body", top.s, SimpleType(MType::Void),
                                          g_funcs.at(top.s + "." + signature.name + "$body").params,
                                          signature.bodyId, signature.module);
                g_namedConstructorBody = false;
                program.addMethod(top.s, std::move(body));
                const int factory = NamedConstructorFactoryBody(top.s, signature);
                program.functions.push_back(LowerFunction(top.s + "." + signature.name,
                                                          Memory::String(), signature.result, signature.params, factory, signature.module));
            }
            else if (signature.isStatic)
                program.functions.push_back(
                    LowerFunction(top.s + "." + signature.name, Memory::String(), signature.result,
                                  signature.params, signature.bodyId, signature.module));
            else
                program.addMethod(top.s, LowerFunction(signature.name, top.s, signature.result,
                                                       signature.params,
                                                       signature.bodyId, signature.module));
        }
    }
    while (!pendingLambdas.empty())
    {
        bool progress = false;
        for (auto it = pendingLambdas.begin(); it != pendingLambdas.end();)
        {
            const auto &top = N(*it);
            const auto &declaration = g_functionDecls.at(static_cast<std::size_t>(top.i));
            if (!g_lambdaPrepared.count(ModuleSymbol(declaration.module, declaration.source)))
            {
                ++it;
                continue;
            }
            program.functions.push_back(LowerFunction(declaration.label, {}, declaration.signature.result,
                                                      declaration.signature.params, declaration.bodyId, declaration.module));
            it = pendingLambdas.erase(it);
            progress = true;
            if (!g_error.empty()) return false;
        }
        if (!progress) return Fail("lambda 的词法捕获环境无法确定");
    }
    AppendCollectionConstructors(program);
    AppendMapKeyPolicies(program);
    AppendTupleConstructors(program);
    AppendArrayReleaseHelpers(program);
    for (const auto &owner : g_needsCaptureDestructor)
    {
        simple_ast::Function function;
        function.name = "__magel_capture_drop_" + owner;
        function.result = simple_ast::Type::i64();
        function.parameters = { { "__magel_capture_address", simple_ast::Type::i64() } };
        Memory::Vector<simple_ast::NodePtr> body;
        AppendDestruction(&body, simple_ast::Node::var("__magel_capture_address"), owner,
                          StructHasInterface(owner, "Drop"), StructHasInterface(owner, "Closeable"));
        body.push_back(simple_ast::Node::ret(simple_ast::Node::i(0)));
        function.body = simple_ast::Node::block(body);
        program.functions.push_back(std::move(function));
    }
    AppendAggregateHelpers(program);
    if (g_functionIndex.find("main") == g_functionIndex.end() && std::any_of(g_functionDecls.begin(), g_functionDecls.end(), [](const MFunctionDecl &function) { return function.cExport; }))
    {
        simple_ast::Function main;
        main.name = "main";
        main.result = simple_ast::Type::i64();
        main.body = simple_ast::Node::block({ simple_ast::Node::ret(simple_ast::Node::i(0)) });
        program.functions.push_back(std::move(main));
    }
    AppendArgumentsMain(program);
    /* 函数体全降级完了，这时才展开全局停机清理（`g.close()` 之类的注销都已经反映到快照上）。 */
    AppendGlobalCleanupToEntryMain(program);
    if (std::any_of(g_functionDecls.begin(), g_functionDecls.end(), [](const MFunctionDecl &function) { return function.cExport; }))
    {
        simple_ast::Function initialize;
        initialize.name = "__magel_c_init";
        initialize.result = simple_ast::Type::i64();
        auto body = g_globalInit;
        body.push_back(simple_ast::Node::ret(simple_ast::Node::i(0)));
        initialize.body = simple_ast::Node::block(body);
        program.functions.push_back(std::move(initialize));
        simple_ast::Function destroy;
        destroy.name = "__magel_c_destroy";
        destroy.result = simple_ast::Type::i64();
        body = EmitGlobalCleanupStatements();
        body.push_back(simple_ast::Node::ret(simple_ast::Node::i(0)));
        destroy.body = simple_ast::Node::block(body);
        program.functions.push_back(std::move(destroy));
    }

    /* 接口引用的方法调用：给每个用过的 `(接口, 方法)` 补一个分派函数。
     * 放在这里是因为"用过哪些"要等函数体全降级完才知道。 */
    AppendInterfaceDispatchers(program);
    /* 全局初始化塞进 main 的最前面：stackvm 从第一条四元式开始执行，引导的 `call main` 在最前 */
    if (!g_globalInit.empty())
    {
        for (simple_ast::Function &function : program.functions)
        {
            if (function.name != "main" || !function.owner.empty()) continue;
            Memory::Vector<simple_ast::NodePtr> body(g_globalInit.begin(), g_globalInit.end());
            if (function.body && function.body->kind == simple_ast::Node::Kind::Block)
            {
                for (const simple_ast::NodePtr &statement : function.body->kids)
                    body.push_back(statement);
            }
            function.body = simple_ast::Node::block(body);
            break;
        }
    }
    return true;
}
