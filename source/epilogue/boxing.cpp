/* ---------- 装箱（`obj.set` / `obj.get`） ---------- */

/**
 * @brief 这个表达式的类型能不能装进 `obj`。
 *
 * 能装的是"**一个槽就装得下、而且不归别的块所有**"的那些：
 *   * 标量（`int64` / `uint64` / `bool` / `float64`）；
 *   * `string`（装进去按**副本**存，所以盒子里那根串不依赖外面谁活着）；
 *   * 引用（`T const&` / `T mut&`，值就是地址）；函数指针也是地址。
 *
 * 装不了的都给一句人话（**编译期**就报，不用等运行）。现在结构体 / 数组按地址装箱，
 * `bint` / 容器 / `obj` / 接口引用装进去的是一份对象副本，仍然按值语义走。
 *
 * @param valueId 要装的值。
 * @param reason 输出：不能装的原因（给用户看的那句话）。
 * @return 能装返回 true。
 */
bool CheckObjValue(int valueId, Memory::String *reason)
{
    const std::shared_ptr<MType> type = ExprType(valueId);
    if (!type)
    {
        *reason = "拿不到值的类型";
        return false;
    }
    /* 诊断文案里点名这个值：能叫出名字（`p` / `row`）就别写"表达式"。 */
    const Memory::String name = NameOfExpr(valueId);
    const Memory::String who =
        (name.rfind("表达式", 0u) == 0u) ? Memory::String("这个值") : ("`" + name + "`");
    switch (type->kind)
    {
    case MType::Int:
    case MType::Char:
    case MType::Bool:
    case MType::Double:
    case MType::String:
        return true;
    case MType::Func: return true; /* 函数指针：值就是地址 */
    case MType::Ptr:
        if (type->elem && type->elem->kind == MType::Void)
        {
            *reason = "引用缺少元素类型，请先在具体类型的引用声明中完成初始化";
            return false;
        }
        return true;
    case MType::Struct:
    case MType::Array:
        return true; /* 多槽值按地址装箱：obj 里存这块数据的引用。 */
    case MType::Container:
    case MType::BigInt:
    case MType::BigFloat:
    case MType::Obj:
        return true;
    default: break;
    }
    *reason = Describe(type) + " 不能装进 obj";
    return false;
}

/**
 * @brief 将对象字段写入降级为运行时调用。
 * @param receiver 方法调用的接收者。
 * @param valueId 表示输入值的节点或类型编号。
 * @param where 用于错误定位的源码节点。
 * @retval 生成的低层语法节点。
 */
simple_ast::NodePtr LowerObjSet(const simple_ast::NodePtr &receiver, int valueId,
                                const Memory::String &where)
{
    Memory::String why;
    if (!CheckObjValue(valueId, &why))
    {
        Fail("装不进 obj：" + why);
        return Bad();
    }
    if (!CheckStoredAddress(valueId, where)) return Bad();
    const std::shared_ptr<MType> valueType = ExprType(valueId);
    const bool objectValue = IsObjectLike(valueType);
    const simple_ast::NodePtr value =
        objectValue                                                 ? MaterializeObjectValue(valueId, valueType) :
        valueType->kind == MType::String && NeedsOwnedTemp(valueId) ? OwnedStringTemp(valueId) :
                                                                      LowerExpr(valueId);
    const auto call = simple_ast::Node::call(
        "__vm_obj_set", { "__magel_o", "__magel_v", "__magel_t", "__magel_s", "__magel_obj", "__magel_n", "__magel_shape" },
        { receiver, value, simple_ast::Node::i(TypeIdOf(valueType)),
          simple_ast::Node::i(valueType->kind == MType::String ? 1 : 0),
          simple_ast::Node::i(valueType->kind == MType::Func ? -1 : objectValue ? 1 :
                                                                                  0),
          simple_ast::Node::str(Describe(valueType)), simple_ast::Node::str(ValueTextLayout(valueType)) });
    for (const auto &callback : ValueTextCallbacks(valueType))
    {
        call->names.push_back("callback" + Memory::toString(call->names.size()));
        call->kids.push_back(callback);
    }
    return call;
}

/**
 * @brief 为 Any 目标或直接接口绑定建立拥有独立句柄的临时值。
 * @param valueId 待装箱的源表达式节点。
 * @param target 目标 Any 或接口引用类型。
 * @retval 完整表达式结束时清理、需要时可移交所有权的装箱结果。
 * @note 沿用显式 set 的类型与地址寿命检查，源表达式只求值一次。
 */
simple_ast::NodePtr LowerBoxedValue(int valueId, const std::shared_ptr<MType> &target)
{
    using simple_ast::Node;
    const auto name = AllocateName("__magel_box_value");
    Memory::Vector<simple_ast::NodePtr> prefix;
    prefix.push_back(Node::declare(name, LowerType(target, name)));
    prefix.push_back(Node::assign(name, Node::call("__vm_obj_new", {}, {})));
    prefix.push_back(Node::exprStmt(LowerObjSet(Node::var(name), valueId, "Any 自动装箱")));
    return OwnedObjectTempNode(Node::sequence(std::move(prefix), Node::var(name)), target);
}

/**
 * @brief 这个表达式是不是 `某个obj.get()`。
 *
 * `obj.get()` 可产出接口引用（胖引用），它也需要根据上下文决定"取成什么类型"
 * 的地方 —— 所以这两处判断都要认得它：`LowerValueForType` 里给它放行，
 * `ExprType` 里按上下文给类型。
 */
bool IsObjGetCall(int id)
{
    if (id < 0) return false;
    const MNode &node = N(id);
    if (node.kind != MNode::Call || node.kids.size() < 2u) return false;
    const MNode &callee = N(node.kids[0]);
    if (callee.kind != MNode::Member && callee.kind != MNode::Arrow) return false;
    if ((callee.s != "getValue" && callee.s != "getReference" && callee.s != "getOptional") || callee.kids.empty())
        return false;
    const std::shared_ptr<MType> base = ExprType(callee.kids[0]);
    return base && base->kind == MType::Obj;
}

/**
 * @brief `obj.get()` 允许取出来的**类型号表**：盒子里是这些类型号之一就算对得上。
 *
 *   * 目标是**接口引用**（`Shape mut&`）→ 所有"实现了这个接口的结构体"的引用
 *     （`S const&` 与 `S mut&` 两种都收）—— 这就是"按接口取对象"；
 *   * 目标是别的引用 → 同一个目标结构的 `const&` / `mut&`（可写性由接它的那个
 *     变量说了算，不在盒子里拦）；
 *   * 其余（标量 / 字符串 / 函数指针）→ 就是它自己。
 *
 * @param target 目标类型。
 * @param out 输出：类型号表。
 * @param reason 输出：算不出来时的原因。
 * @return 成功返回 true。
 */
bool ObjGetAllowedTags(const std::shared_ptr<MType> &target, Memory::Vector<std::int64_t> *out,
                       Memory::String *reason, bool allowEmpty = false)
{
    if (target->kind == MType::Ptr)
    {
        if (IsInterfacePointer(target))
        {
            const Memory::String &iface = target->elem->name;
            for (const auto &item : g_structs)
            {
                if (!ImplementsInterface(item.first, iface)) continue;
                const std::shared_ptr<MType> named = NamedType(item.first);
                if (target->readOnly) out->push_back(TypeIdOf(PointerType(named, true)));
                out->push_back(TypeIdOf(PointerType(named, false)));
            }
            if (out->empty() && !allowEmpty)
            {
                *reason = "没有任何结构体实现接口 " + iface + "：取不出 " + Describe(target);
                return false;
            }
            return true;
        }
        if (target->readOnly) out->push_back(TypeIdOf(PointerType(target->elem, true)));
        out->push_back(TypeIdOf(PointerType(target->elem, false)));
        return true;
    }
    if (target->kind == MType::Container || target->kind == MType::BigInt || target->kind == MType::BigFloat || target->kind == MType::Obj)
    {
        out->push_back(TypeIdOf(target));
        return true;
    }
    if (target->kind == MType::Struct || target->kind == MType::Array)
    {
        out->push_back(TypeIdOf(target));
        return true;
    }
    if (target->kind == MType::Void)
    {
        *reason = Describe(target) + " 是多槽 / 归块所有的类型，一个格子装不下、也取不出来 —— "
                                     "盒子里装的是它的引用时就按引用取：`"
                  + Describe(target) + " const& p = o.get();`（结构体 / 数组），"
                                       "或者先把引用装进去再取出来";
        return false;
    }
    out->push_back(TypeIdOf(target));
    return true;
}

/**
 * @brief `map` 的结果元素在容器里的存放种类；-1 = 这个类型放不进容器。
 *
 * 与 `LowerAlgorithmCall` 传给 `__vm_vec_map` 的第 3 个实参一一对应
 * （回调只能回传整数与浮点，字符串是地址、对象是句柄）。
 */
int AlgoElementKind(const std::shared_ptr<MType> &type)
{
    if (!type) return -1;
    if (type->kind == MType::Struct) return 4;
    if (type->kind == MType::String) return 2;
    if (IsObjectLike(type)) return 3;
    if (type->kind == MType::Double) return 1;
    if (type->kind == MType::Int || type->kind == MType::Char || type->kind == MType::Bool)
        return 0;
    return -1;
}

/**
 * @brief 检查算法参数是否为支持的列表类型。
 * @param args 按接口约定顺序传入的实参列表。
 * @param index 待访问的元素索引。
 * @param name 名称文本。
 * @retval 对应的类型描述对象。
 */
std::shared_ptr<MType> RequireAlgorithmList(const Memory::Vector<int> &args, std::size_t index,
                                            const Memory::String &name)
{
    if (index >= args.size()) return nullptr;
    const std::shared_ptr<MType> type = ExprType(args[index]);
    if (!type || type->kind != MType::Container || type->name != "vec" || !type->elem)
    {
        Fail("std::algorithms::" + name + " 的第 " + Memory::toString(index + 1u) + " 个参数必须是 std::lang::List<T>，现在是 " + Describe(type));
        return nullptr;
    }
    return type;
}

/**
 * @brief 检查算法回调的参数与返回类型。
 * @param callbackId 回调表达式的节点编号。
 * @param element 数组或容器的元素类型。
 * @param parameterCount 回调要求的形参数量。
 * @param boolResult 是否要求回调返回布尔值。
 * @param name 名称文本。
 * @retval 满足检查条件或操作完成时为 true，检查失败或无法完成时为 false。
 */
bool CheckAlgorithmCallback(int callbackId, const std::shared_ptr<MType> &element,
                            std::size_t parameterCount, bool boolResult,
                            const Memory::String &name)
{
    const std::shared_ptr<MType> callback = ExprType(callbackId);
    if (!callback || callback->kind != MType::Func || callback->paramTypes.size() != parameterCount || (boolResult ? (!callback->elem || callback->elem->kind != MType::Bool) : !SameType(callback->elem, element)))
    {
        Fail("std::algorithms::" + name + " 的回调签名不匹配");
        return false;
    }
    for (const std::shared_ptr<MType> &parameter : callback->paramTypes)
    {
        if (!SameType(parameter, element))
        {
            Fail("std::algorithms::" + name + " 的回调参数必须是 " + Describe(element));
            return false;
        }
    }
    return true;
}

/**
 * @brief 将列表算法调用降级为运行时操作。
 * @param name 名称文本。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 生成的低层语法节点。
 */
simple_ast::NodePtr LowerAlgorithmCall(const Memory::String &name, const Memory::Vector<int> &args)
{
    const auto arity = [&](std::size_t wanted) {
        if (args.size() == wanted) return true;
        Fail("std::algorithms::" + name + " 需要 " + Memory::toString(wanted) + " 个参数，给了 " + Memory::toString(args.size()) + " 个");
        return false;
    };
    if (name == "minValue" || name == "maxValue")
    {
        if (args.empty()) return Fail("std::algorithms::" + name + " 至少需要一个参数"), Bad();
        const std::shared_ptr<MType> element = ExprType(args[0]);
        for (std::size_t index = 1u; index < args.size(); ++index)
            if (!Acceptable(element, ExprType(args[index]), true))
                return Fail("std::algorithms::" + name + " 的所有参数必须兼容 " + Describe(element)), Bad();
        auto list = Memory::makeShared<MType>();
        list->kind = MType::Container;
        list->name = "vec";
        list->elem = element;
        const MParam pack{ "rest", "__magel_rest", list, true };
        return simple_ast::Node::call(
            name == "minValue" ? "__vm_vec_min" : "__vm_vec_max",
            { "__magel_v", "__magel_first", "__magel_text", "__magel_object" },
            { PackVariadicArguments(args, 1u, pack), LowerValueForType(args[0], element),
              simple_ast::Node::i(element->kind == MType::String ? 1 : 0),
              ContainerStorageFlag(element) });
    }

    /* `map` / `filter` / `reduce` / `sum` / `count` 的实参个数与上面那批不同，
     * 所以必须在统一的 arity 检查**之前**分流。 */
    if (name == "sumValue")
    {
        if (!arity(1u)) return Bad();
        const std::shared_ptr<MType> list = RequireAlgorithmList(args, 0u, name);
        if (!list) return Bad();
        const std::shared_ptr<MType> element = list->elem;
        if (element->kind != MType::Int && element->kind != MType::Char && element->kind != MType::Bool && element->kind != MType::Double)
            return Fail("std::algorithms::sum 只能求 int / float64 列表的和，现在是 " + Describe(list)), Bad();
        return simple_ast::Node::call(
            "__vm_vec_sum", { "__magel_v", "__magel_float" },
            { LowerExpr(args[0]),
              simple_ast::Node::i(element->kind == MType::Double ? 1 : 0) });
    }
    if (name == "mapValue")
    {
        if (!arity(2u)) return Bad();
        const std::shared_ptr<MType> list = RequireAlgorithmList(args, 0u, name);
        if (!list) return Bad();
        const std::shared_ptr<MType> element = list->elem;
        const std::shared_ptr<MType> callback = ExprType(args[1]);
        if (!callback || callback->kind != MType::Func || callback->paramTypes.size() != 1u || !callback->elem || !SameType(callback->paramTypes[0], element))
        {
            Fail("std::algorithms::map 的回调必须是 R fn(" + Describe(element) + ") 的函数值");
            return Bad();
        }
        const std::shared_ptr<MType> mapped = callback->elem;
        const int kind = AlgoElementKind(mapped);
        if (kind < 0)
            return Fail("std::algorithms::map 的回调返回值放不进 std::lang::List：" + Describe(mapped)), Bad();
        return OwnedObjectTempNode(
            simple_ast::Node::call("__vm_vec_map", { "__magel_v", "__magel_callback", "__magel_kind", "__magel_layout" },
                                   { LowerExpr(args[0]), LowerExpr(args[1]),
                                     simple_ast::Node::i(kind), ContainerStorageFlag(mapped) }),
            MakeListType(mapped));
    }
    if (name == "filterValue")
    {
        if (!arity(2u)) return Bad();
        const std::shared_ptr<MType> list = RequireAlgorithmList(args, 0u, name);
        if (!list) return Bad();
        const std::shared_ptr<MType> element = list->elem;
        if (!CheckAlgorithmCallback(args[1], element, 1u, true, name)) return Bad();
        return OwnedObjectTempNode(
            simple_ast::Node::call("__vm_vec_filter",
                                   { "__magel_v", "__magel_callback", "__magel_text", "__magel_object" },
                                   { LowerExpr(args[0]), LowerExpr(args[1]),
                                     simple_ast::Node::i(element->kind == MType::String ? 1 : 0),
                                     ContainerStorageFlag(element) }),
            MakeListType(element));
    }
    if (name == "countValue")
    {
        if (!arity(2u)) return Bad();
        const std::shared_ptr<MType> list = RequireAlgorithmList(args, 0u, name);
        if (!list) return Bad();
        const std::shared_ptr<MType> element = list->elem;
        const std::shared_ptr<MType> second = ExprType(args[1]);
        /* 第二参是 fptr 就按**谓词**数（`count(list, 判定)`），否则按**值**数
         * （`count(list, 值)`）—— 两种写法靠实参类型区分，不要另起名字。 */
        if (second && second->kind == MType::Func)
        {
            if (!CheckAlgorithmCallback(args[1], element, 1u, true, name)) return Bad();
            return simple_ast::Node::call("__vm_vec_count_if", { "__magel_v", "__magel_callback" },
                                          { LowerExpr(args[0]), LowerExpr(args[1]) });
        }
        if (!Acceptable(element, second, true))
            return Fail("std::algorithms::count 的目标值必须兼容 " + Describe(element)), Bad();
        return simple_ast::Node::call(
            "__vm_vec_count_value", { "__magel_v", "__magel_value", "__magel_text", "__magel_object" },
            { LowerExpr(args[0]), LowerValueForType(args[1], element),
              simple_ast::Node::i(element->kind == MType::String ? 1 : 0),
              ContainerStorageFlag(element) });
    }
    if (name == "reduceValue")
    {
        /* 两种写法都收：`reduce(list, f)` 拿第一个元素当初值（空表报错）；
         * `reduce(list, 初值, f)` 与 `fold` 完全等价，直接走同一个原生。 */
        const std::size_t reduceWanted = args.size() == 3u ? 3u : 2u;
        if (!arity(reduceWanted)) return Bad();
        const std::shared_ptr<MType> list = RequireAlgorithmList(args, 0u, name);
        if (!list) return Bad();
        const std::shared_ptr<MType> element = list->elem;
        if (reduceWanted == 3u)
        {
            if (!Acceptable(element, ExprType(args[1]), true))
                return Fail("std::algorithms::reduce 的初始值必须兼容 " + Describe(element)), Bad();
            if (!CheckAlgorithmCallback(args[2], element, 2u, false, name)) return Bad();
            return simple_ast::Node::call(
                "__vm_vec_fold", { "__magel_v", "__magel_initial", "__magel_callback" },
                { LowerExpr(args[0]), LowerValueForType(args[1], element), LowerExpr(args[2]) });
        }
        if (!CheckAlgorithmCallback(args[1], element, 2u, false, name)) return Bad();
        return simple_ast::Node::call("__vm_vec_reduce", { "__magel_v", "__magel_callback" },
                                      { LowerExpr(args[0]), LowerExpr(args[1]) });
    }

    const std::size_t wanted = (name == "sortValue") ? 1u : (name == "foldValue" || name == "clampValue" ? 3u : 2u);
    if (!arity(wanted)) return Bad();
    if (name == "clampValue")
    {
        const std::shared_ptr<MType> element = ExprType(args[0]);
        if (!Acceptable(element, ExprType(args[1]), true) || !Acceptable(element, ExprType(args[2]), true))
            return Fail("std::algorithms::clamp 的 value、low、high 必须是兼容类型"), Bad();
        auto list = Memory::makeShared<MType>();
        list->kind = MType::Container;
        list->name = "vec";
        list->elem = element;
        const MParam bounds{ "bounds", "__magel_bounds", list, true };
        return simple_ast::Node::call(
            "__vm_vec_clamp", { "__magel_v", "__magel_value", "__magel_text", "__magel_object" },
            { PackVariadicArguments(args, 1u, bounds), LowerValueForType(args[0], element),
              simple_ast::Node::i(element->kind == MType::String ? 1 : 0),
              ContainerStorageFlag(element) });
    }

    const std::shared_ptr<MType> list = RequireAlgorithmList(args, 0u, name);
    if (!list) return Bad();
    const std::shared_ptr<MType> element = list->elem;
    if (name == "sortValue")
    {
        return OwnedObjectTempNode(
            simple_ast::Node::call("__vm_vec_sorted_copy", { "__magel_v" }, { LowerExpr(args[0]) }),
            list);
    }
    if (name == "findValue" || name == "binarySearchValue")
    {
        if (!Acceptable(element, ExprType(args[1]), true))
            return Fail("std::algorithms::" + name + " 的目标值必须兼容 " + Describe(element)), Bad();
        return simple_ast::Node::call(
            name == "findValue" ? "__vm_vec_find" : "__vm_vec_binary_search",
            { "__magel_v", "__magel_value", "__magel_text", "__magel_object" },
            { LowerExpr(args[0]), LowerValueForType(args[1], element),
              simple_ast::Node::i(element->kind == MType::String ? 1 : 0),
              ContainerStorageFlag(element) });
    }
    if (name == "allValue" || name == "anyValue")
    {
        if (!CheckAlgorithmCallback(args[1], element, 1u, true, name)) return Bad();
        return simple_ast::Node::call(name == "allValue" ? "__vm_vec_all" : "__vm_vec_any",
                                      { "__magel_v", "__magel_callback" },
                                      { LowerExpr(args[0]), LowerExpr(args[1]) });
    }
    if (!Acceptable(element, ExprType(args[1]), true))
        return Fail("std::algorithms::fold 的初始值必须兼容 " + Describe(element)), Bad();
    if (!CheckAlgorithmCallback(args[2], element, 2u, false, name)) return Bad();
    return simple_ast::Node::call(
        "__vm_vec_fold", { "__magel_v", "__magel_initial", "__magel_callback" },
        { LowerExpr(args[0]), LowerValueForType(args[1], element), LowerExpr(args[2]) });
}

/**
 * @brief 这个声明落在**编译器内嵌的标准库**里吗（`lang` / `std::*` / 文法登记的标准模块）？
 *
 * 标准库里那些"只有签名"的宿主函数由 VM 内建提供（`std::coroutine::yield`、
 * `std::io::__fileTryRead`、`std::memory::__memoryLiveBytes`…），不一定挂在
 * `NativeRegistry` 上，所以不做"注册表里必须有"的校验。用户 / 胶水层声明落在入口模块，
 * 照常校验（`Declarations::hostFunction` 追加的也正是入口模块）。
 */
bool IsStandardLibraryModule(const Memory::String &module)
{
    if (module.empty()) return false;
    if (module == "std::lang" || module == "std") return true;
    if (module.compare(0u, 5u, "std::") == 0) return true;
    return IsStandardModule(module); /* 文法登记的标准模块 */
}

/**
 * @brief 降级一次调用：`f(…)` / `对象.方法(…)` / `fp(…)`。
 *
 * 派发顺序（前四条都是"被调者是成员访问、但基类型不是结构体"的情况）：
 *   ① `bint` 的 `.str()`；
 *   ② **`obj` 的 `set` / `get` / `name`**（装箱格子）；
 *   ③ **接口引用的任意方法**（`Shape mut& q` → `q.area()`，运行期按类型号分派）；
 *   ④ 容器的方法（`v.push` / `m.put`…）；
 *   然后才是函数指针调用、内存内建、普通函数 / 结构体方法。
 */
/**
 * @brief 这个调用标签是不是"纯宿主函数"（`T name(...);`，实现从 NativeRegistry 取）。
 *
 * `abi "…" { … }` 里的签名也走 `ExternFuncDecl`，但实现由链接进来的 C ABI 提供、
 * 不在注册表里，所以用 `abiLibrary` 排除掉。
 */
bool IsRegistryHostFunctionLabel(const Memory::String &label)
{
    for (const MFunctionDecl &declaration : g_functionDecls)
        if (!declaration.superseded && declaration.external && declaration.abiLibrary.empty() && !IsStandardLibraryModule(declaration.module) && declaration.label == label)
            return true;
    return false;
}

/**
 * @brief std::lang::move 转移拥有资源，清空源存储但不执行源值的析构。
 * @param args 唯一可写引用实参；只求值一次。
 * @param implicitExit 是否为编译器在内部变量生命周期结束处生成的消费。
 * @return 独立的拥有型值；原绑定禁止读写，须重新声明新绑定。
 */
simple_ast::NodePtr LowerLanguageMove(const Memory::Vector<int> &args, bool implicitExit = false)
{
    using simple_ast::Node;
    if (args.size() != 1u) return Fail("std::lang::move 需要一个 T mut& 参数"), Bad();
    const auto reference = RawExprType(args[0]);
    if (!reference || reference->kind != MType::Ptr || (!implicitExit && (reference->readOnly || IsReadOnlyAccess(args[0]))))
        return Fail("std::lang::move 只接受可写引用（T mut&），不能接收值或 const &"), Bad();
    const auto type = reference->elem;
    if (!type || type->kind == MType::Void || (type->kind == MType::Array && !(implicitExit && IsRuntimeArray(type))) || IsInterfaceType(type) || type->kind == MType::Host)
        return Fail("std::lang::move 的目标必须是具有具体布局的可移动值"), Bad();
    // 内建只借用源槽，移动值的内部借用由结果存储与返回边界继续检查。
    RecordObjectArgumentBorrow(args[0], reference);
    g_usesLanguageMove = true;
    const auto address = AllocateName("__magel_move_address");
    const auto result = AllocateName("__magel_move_result");
    const int bytes = type->kind == MType::Struct ? std::max(8, StructStride(type)) : 8;
    Memory::Vector<simple_ast::NodePtr> out{
        Node::declare(address, simple_ast::Type::i64()), Node::assign(address, LowerRawExpr(args[0])),
        Node::exprStmt(Node::call("__vm_move_require", { "address", "bytes", "type" },
                                  { Node::var(address), Node::i(type->kind == MType::Struct ? bytes : 0), Node::i(TypeIdOf(type)) })),
        Node::declare(result, LowerType(type, result))
    };
    if (type->kind == MType::Struct)
    {
        out.push_back(DefaultInit(result, type, false));
        out.push_back(CopyStructValue(Node::var(result), Node::var(address), type));
        // 返回聚合存储由调用点立即接管；仅释放搬运用的堆块，不释放返回叶子。
        // 默认初始化的独立块继续由本作用域回收，拥有叶子由接收值负责。
    }
    else
    {
        auto value = type->kind == MType::Char ? Node::call("__vm_char_load", { "address" }, { Node::var(address) }) : Node::loadFrom(Node::var(address), LowerType(type, result));
        out.push_back(Node::assign(result, value));
    }
    if (type->kind == MType::Char)
        out.push_back(Node::exprStmt(Node::call("__vm_char_store", { "address", "value" }, { Node::var(address), Node::i(0) })));
    else
        out.push_back(Node::exprStmt(Node::call("__vm_memset", { "address", "value", "bytes" },
                                                { Node::var(address), Node::i(0), Node::i(bytes) })));
    if (type->kind == MType::Struct)
        AppendMovedAggregate(type->name, Node::var(address), &out);
    return Node::sequence(std::move(out), Node::var(result));
}

/**
 * @brief 解析函数调用并生成对应的低层表达式。
 * @param id 待查找或操作的对象编号。
 * @retval 生成的低层语法节点。
 */
simple_ast::NodePtr LowerCall(int id)
{
    const MNode node = N(id);
    if (node.kids.size() < 2u)
    {
        Fail("调用缺少参数表");
        return Bad();
    }
    MNode callee = N(node.kids[0]);
    const Memory::String standardName = StandardCallName(callee);
    if (!standardName.empty()) callee.s = standardName;
    const Memory::Vector<int> args = N(node.kids[1]).kids;
    if (standardName == "move") return LowerLanguageMove(args, N(id).implicitExitMove);
    // 反射只读取已校验接口胖引用的实际身份，不开放任意对象地址装箱。
    if (g_reflectionEnabled && CallSymbol(callee).find("std::reflect::__reflect_actualType__") == 0)
    {
        if (!node.reflectionBridge) return Fail("内部接口身份入口仅供编译器反射桥接使用"), Bad();
        if (args.size() != 1) return Fail("反射实际类型查询需要一个接口引用"), Bad();
        const auto type = RawExprType(args[0]);
        if (!type || type->kind != MType::Ptr || !g_interfaces.count(type->elem->name))
            return Fail("反射实际类型查询只接受具体接口引用"), Bad();
        return simple_ast::Node::call("__vm_obj_name", { "__magel_o" }, { LowerRawExpr(args[0]) });
    }
    if (g_reflectionEnabled && CallSymbol(callee).find("std::reflect::__reflect_unary__") == 0)
    {
        if (!node.reflectionBridge) return Fail("内部一元入口仅供编译器反射桥接使用"), Bad();
        if (args.size() != 2 || N(args[1]).kind != MNode::Str)
            return Fail("一元反射桥接要求静态方法名"), Bad();
        const auto type = RawExprType(args[0]);
        if (!type || !IsInterfacePointer(type)) return Fail("一元反射桥接要求具体接口引用"), Bad();
        const auto method = UnescapeText(N(args[1]).s);
        bool left = true;
        const auto spec = OperatorInterfaceSpec(type->elem->name, method, &left);
        if (!spec || !spec->unary) return Fail("一元反射桥接要求一元运算符接口方法"), Bad();
        std::shared_ptr<MType> expected;
        Memory::Vector<std::shared_ptr<MType>> operands;
        FindInterfaceMethod(type->elem->name, method, &expected, &operands);
        if (!SameType(ExprType(id), expected)) return Fail("一元反射桥接返回类型必须与接口精确匹配"), Bad();
        g_usedInterfaceMethods.insert(type->elem->name + "|" + method + "|unary");
        return simple_ast::Node::call(InterfaceLabel(type->elem->name, method) + "$reflection_unary",
                                      { "__magel_self", "__magel_readonly" },
                                      { LowerRawExpr(args[0]), simple_ast::Node::i(IsReadOnlyAccess(args[0]) ? 1 : 0) });
    }
    if (callee.kind == MNode::Var && callee.s == "__magel_default_to_string")
    {
        if (args.size() != 1u) return Fail("内部默认字符串转换需要一个值"), Bad();
        return LowerDefaultValueText(args[0]);
    }
    const Memory::String algorithmName = AlgorithmCallName(callee);
    if (!algorithmName.empty())
    {
        for (const int argument : args)
            RecordObjectArgumentBorrow(argument, ExprType(argument));
        return LowerAlgorithmCall(algorithmName, args);
    }

    /* ---- 容器方法（`v.push(x)` / `m.put(k, v)`）与 `bint.str()` ----
     *
     * 被调者是成员访问，但基类型是**运行期对象**（不是结构体）：按方法表派发到
     * `__vm_*` 原生函数。`shape` 里的字母决定"这个形参怎么来"（见 MContainerMethod）。 */
    if (callee.kind == MNode::Member || callee.kind == MNode::Arrow)
    {
        /* 注意：`callee` / `args` 都是**指向节点池的引用**，而下面每次 `LowerExpr` 都可能
         * 让池子扩容（vector 重新分配）—— 要用的东西先抄成局部量。 */
        const int receiverId = callee.kids.empty() ? -1 : callee.kids[0];
        const Memory::String methodName = callee.s;
        if (methodName == "drop" && IsReadOnlyAccess(receiverId))
            return Fail("只读引用（T const&）或 const 来源不能调用 drop；需要 T mut&"), Bad();
        const Memory::Vector<int> argIds = args;
        const std::shared_ptr<MType> baseType = ExprType(receiverId);
        if (methodName == "toString" && argIds.empty())
        {
            std::shared_ptr<MType> result;
            Memory::Vector<std::shared_ptr<MType>> parameters;
            if (!IsInterfacePointer(baseType)) return LowerValueText(receiverId);
            if (!FindInterfaceMethod(baseType->elem->name, methodName, &result, &parameters)) return LowerDefaultValueText(receiverId);
        }
        /* ---- 运行时数组 `T[]` 的 `.len()` ----
         * 元素个数存在对象块头的前 8 字节（`new[n]` 建的时候写进去的），
         * 也就是"对象首地址 - 8"处那个槽。1.0.0 删掉 `sizeof` 之后，
         * 这是问"数组有几个元素"的唯一写法。 */
        if (baseType && IsRuntimeArray(baseType))
        {
            if (methodName != "len" || !argIds.empty())
            {
                if (methodName != "len")
                    return Fail("运行时数组 `" + Describe(baseType) + "` 只有 .len() 方法（元素个数）；现在是 ." + methodName), Bad();
                return Fail("数组 .len() 不带参数"), Bad();
            }
            return simple_ast::Node::loadInt(simple_ast::Node::binary(
                "-", LowerExpr(receiverId), simple_ast::Node::i(8)));
        }
        Memory::Vector<std::shared_ptr<MType>> hostArgumentTypes;
        for (const int argument : argIds) hostArgumentTypes.push_back(RawExprType(argument));
        const MInterfaceMethod *hostRequired = nullptr;
        Memory::String hostSymbol;
        const MHostInterfaceImpl *hostImpl =
            FindHostInterfaceImplementation(baseType, methodName, hostArgumentTypes,
                                            &hostRequired, &hostSymbol);
        if (hostImpl != nullptr)
        {
            /* 解释 / 内嵌路径这次编译完就要跑，宿主接口成员的绑定现在就得查得到：
             * 只声明没绑定的成员在编译期报错，而不是等到运行期"函数入口不存在"。
             * `--emit` 只产出 .quad（绑定由目标程序的 bindMagelGlue 负责），不查。 */
            if (g_executeNow && g_nativeRegistry != nullptr && g_nativeRegistry->find(hostSymbol) == nullptr)
            {
                Fail("宿主接口方法没有绑定：" + Describe(baseType) + "." + methodName + " → " + hostSymbol + "（宿主用 registry.bindTypedImplementation / "
                                                                                                             "bindTypedMethod 注册）");
                return Bad();
            }
            Memory::Vector<Memory::String> names{ "__magel_self" };
            Memory::Vector<simple_ast::NodePtr> values{ OwnedObjectTemp(receiverId, baseType) };
            for (std::size_t index = 0; index < argIds.size(); ++index)
            {
                names.push_back("__magel_value" + Memory::toString(index));
                values.push_back(LowerArgumentValue(argIds[index], hostRequired->params[index]));
            }
            /* 显式调用胶水实现的 `drop()` / `close()`：从自动析构名单里摘掉，别再跑一次。
             * `drop()` 是终结操作：先补 `close()`，之后系统不再自动回收（见 FinishExplicitDrop）。 */
            const simple_ast::NodePtr call = simple_ast::Node::call(hostSymbol, names, values);
            if (argIds.empty() && receiverId >= 0 && N(receiverId).kind == MNode::Var)
            {
                const MVar *variable = FindIn(N(receiverId).module, N(receiverId).s);
                if (variable != nullptr)
                {
                    if (methodName == "close" && hostImpl->interfaceName == "Closeable")
                        UnmarkDestructor(variable->emitted, false, true);
                    else if (methodName == "drop" && hostImpl->interfaceName == "Drop")
                        return FinishExplicitDrop(StructNameOf(baseType), variable->emitted,
                                                  values.front(), call);
                }
            }
            return call;
        }
        if (baseType && (baseType->kind == MType::BigInt || baseType->kind == MType::BigFloat))
        {
            /* 把方法实参转成 `bint`（整数 / 浮点自动造临时大数，大数原样）。 */
            const auto bigIntArg = [&](int index) -> simple_ast::NodePtr {
                const std::shared_ptr<MType> argumentType = ExprType(argIds[index]);
                if (argumentType && argumentType->kind == MType::BigInt)
                    return OwnedObjectTemp(argIds[index], argumentType);
                if (argumentType && argumentType->kind == MType::Double)
                    return OwnedObjectTempNode(
                        simple_ast::Node::call("__vm_bint_from_flt", { "__magel_v" },
                                               { LowerExpr(argIds[index]) }),
                        T(M_TypeBigInt()));
                if (argumentType && (argumentType->kind == MType::Int || argumentType->kind == MType::Bool))
                    return OwnedObjectTempNode(
                        simple_ast::Node::call(argumentType->isUnsigned ? "__vm_bint_from_uint" : "__vm_bint_from_int",
                                               { "__magel_v" }, { LowerExpr(argIds[index]) }),
                        T(M_TypeBigInt()));
                Fail("大数方法的参数只能是整数 / 大整数：实际是 " +
                     (argumentType ? Describe(argumentType) : Memory::String("?")));
                return Bad();
            };
            const auto intArg = [&](int index, const char *what) -> simple_ast::NodePtr {
                const std::shared_ptr<MType> argumentType = ExprType(argIds[index]);
                if (argumentType == nullptr || argumentType->kind != MType::Int)
                {
                    Fail(Memory::String(what) + " 需要一个整数参数");
                    return Bad();
                }
                return LowerExpr(argIds[index]);
            };
            const bool isBigFloat = baseType->kind == MType::BigFloat;

            if (isBigFloat && methodName == "setPrecision")
            {
                if (!CheckMutableReceiver(receiverId)) return Bad();
                if (argIds.size() != 1u || ExprType(argIds[0])->kind != MType::Int)
                {
                    Fail("BigDecimal.setPrecision 需要一个整数参数");
                    return Bad();
                }
                return simple_ast::Node::call("__vm_bfloat_set_value_precision",
                                              { "__magel_v", "__magel_digits" }, { LowerExpr(receiverId), LowerExpr(argIds[0]) });
            }
            if (methodName == "copy")
            {
                if (!argIds.empty())
                {
                    Fail("大整数的 `.copy()` 不带参数");
                    return Bad();
                }
                return simple_ast::Node::call(
                    "__vm_object_copy", { "__magel_v" }, { LowerExpr(receiverId) });
            }
            if (methodName == "str")
            {
                if (!argIds.empty())
                {
                    Fail("大整数的 `.str()` 不带参数；要别的进制用 `.strRadix(base)`");
                    return Bad();
                }
                return simple_ast::Node::call(
                    isBigFloat ? "__vm_bfloat_str" : "__vm_bint_str", { "__magel_v" },
                    { OwnedObjectTemp(receiverId, baseType) });
            }

            /* ---- 大整数方法 ---- */
            if (!isBigFloat)
            {
                if (methodName == "pow")
                {
                    if (argIds.size() != 1u) return Fail("BigInteger.pow 需要一个整数指数"), Bad();
                    return simple_ast::Node::call("__vm_bint_pow", { "__magel_v", "__magel_e" },
                                                  { OwnedObjectTemp(receiverId, baseType), intArg(0, "BigInteger.pow") });
                }
                if (methodName == "powmod")
                {
                    if (argIds.size() != 2u) return Fail("BigInteger.powmod 需要 (指数, 模) 两个参数"), Bad();
                    return simple_ast::Node::call("__vm_bint_powmod", { "__magel_v", "__magel_e", "__magel_m" },
                                                  { OwnedObjectTemp(receiverId, baseType), bigIntArg(0), bigIntArg(1) });
                }
                if (methodName == "gcd" || methodName == "lcm")
                {
                    if (argIds.size() != 1u)
                        return Fail(Memory::String("BigInteger.") + methodName + " 需要一个参数"), Bad();
                    return simple_ast::Node::call(methodName == "gcd" ? "__vm_bint_gcd" : "__vm_bint_lcm",
                                                  { "__magel_l", "__magel_r" },
                                                  { OwnedObjectTemp(receiverId, baseType), bigIntArg(0) });
                }
                if (methodName == "mod")
                {
                    if (argIds.size() != 1u) return Fail("BigInteger.mod 需要一个模数参数"), Bad();
                    return simple_ast::Node::call("__vm_bint_mod_nonneg", { "__magel_v", "__magel_m" },
                                                  { OwnedObjectTemp(receiverId, baseType), bigIntArg(0) });
                }
                if (methodName == "modInverse")
                {
                    if (argIds.size() != 1u) return Fail("BigInteger.modInverse 需要一个模数参数"), Bad();
                    return simple_ast::Node::call("__vm_bint_mod_inverse", { "__magel_v", "__magel_m" },
                                                  { OwnedObjectTemp(receiverId, baseType), bigIntArg(0) });
                }
                if (methodName == "isProbablePrime")
                {
                    if (argIds.size() != 1u) return Fail("BigInteger.isProbablePrime 需要一个轮数参数"), Bad();
                    return simple_ast::Node::call("__vm_bint_is_probable_prime", { "__magel_v", "__magel_rounds" },
                                                  { OwnedObjectTemp(receiverId, baseType), intArg(0, "BigInteger.isProbablePrime") });
                }
                if (methodName == "nextProbablePrime")
                {
                    if (!argIds.empty()) return Fail("BigInteger.nextProbablePrime 不带参数"), Bad();
                    return simple_ast::Node::call("__vm_bint_next_probable_prime", { "__magel_v" },
                                                  { OwnedObjectTemp(receiverId, baseType) });
                }
                if (methodName == "bitLength")
                {
                    if (!argIds.empty()) return Fail("BigInteger.bitLength 不带参数"), Bad();
                    return simple_ast::Node::call("__vm_bint_bit_length", { "__magel_v" },
                                                  { OwnedObjectTemp(receiverId, baseType) });
                }
                if (methodName == "bitAnd" || methodName == "bitOr" || methodName == "bitXor")
                {
                    if (argIds.size() != 1u)
                        return Fail(Memory::String("BigInteger.") + methodName + " 需要一个参数"), Bad();
                    return simple_ast::Node::call(
                        methodName == "bitAnd" ? "__vm_bint_bit_and" :
                        methodName == "bitOr" ? "__vm_bint_bit_or" : "__vm_bint_bit_xor",
                        { "__magel_l", "__magel_r" },
                        { OwnedObjectTemp(receiverId, baseType), bigIntArg(0) });
                }
                if (methodName == "strRadix")
                {
                    if (argIds.size() != 1u) return Fail("BigInteger.strRadix 需要一个进制参数"), Bad();
                    return simple_ast::Node::call("__vm_bint_str_radix", { "__magel_v", "__magel_base" },
                                                  { OwnedObjectTemp(receiverId, baseType), intArg(0, "BigInteger.strRadix") });
                }
                if (methodName == "iroot")
                {
                    if (argIds.size() != 1u) return Fail("BigInteger.iroot 需要一个根次参数"), Bad();
                    return simple_ast::Node::call("__vm_bint_iroot", { "__magel_v", "__magel_k" },
                                                  { OwnedObjectTemp(receiverId, baseType), intArg(0, "BigInteger.iroot") });
                }
            }
            else
            {
                /* ---- 大浮点方法 ---- */
                if (methodName == "pow")
                {
                    if (argIds.size() != 1u) return Fail("BigDecimal.pow 需要一个整数指数"), Bad();
                    return simple_ast::Node::call("__vm_bfloat_pow", { "__magel_v", "__magel_e" },
                                                  { OwnedObjectTemp(receiverId, baseType), intArg(0, "BigDecimal.pow") });
                }
                if (methodName == "abs")
                {
                    if (!argIds.empty()) return Fail("BigDecimal.abs 不带参数"), Bad();
                    return simple_ast::Node::call("__vm_bfloat_abs", { "__magel_v" },
                                                  { OwnedObjectTemp(receiverId, baseType) });
                }
                if (methodName == "isInteger")
                {
                    if (!argIds.empty()) return Fail("BigDecimal.isInteger 不带参数"), Bad();
                    return simple_ast::Node::call("__vm_bfloat_is_integer", { "__magel_v" },
                                                  { OwnedObjectTemp(receiverId, baseType) });
                }
                if (methodName == "floor" || methodName == "ceil" || methodName == "trunc")
                {
                    if (!argIds.empty())
                        return Fail(Memory::String("BigDecimal.") + methodName + " 不带参数"), Bad();
                    return simple_ast::Node::call(
                        methodName == "floor" ? "__vm_bfloat_floor" :
                        methodName == "ceil" ? "__vm_bfloat_ceil" : "__vm_bfloat_trunc",
                        { "__magel_v" }, { OwnedObjectTemp(receiverId, baseType) });
                }
                if (methodName == "sqrt")
                {
                    if (argIds.size() != 1u) return Fail("BigDecimal.sqrt 需要一个有效位数参数"), Bad();
                    return simple_ast::Node::call("__vm_bfloat_sqrt", { "__magel_v", "__magel_digits" },
                                                  { OwnedObjectTemp(receiverId, baseType), intArg(0, "BigDecimal.sqrt") });
                }
                if (methodName == "root")
                {
                    if (argIds.size() != 2u) return Fail("BigDecimal.root 需要 (根次, 有效位数) 两个参数"), Bad();
                    return simple_ast::Node::call("__vm_bfloat_root", { "__magel_v", "__magel_k", "__magel_digits" },
                                                  { OwnedObjectTemp(receiverId, baseType), intArg(0, "BigDecimal.root"), intArg(1, "BigDecimal.root") });
                }
            }
            Fail("大数没有这个方法：" + methodName);
            return Bad();
        }
        /* ---- 装箱格子：`b.set(x)` / `b.get()` / `b.name()` / `b.copy()` ----
         *
         * 类型号由前端在**编译期**算好一并传下去：`name()` 拿回来的就是它，
         * 和 `__typeTag(类型)` 比一比就是泛型分支。`get()` 的目标类型由上下文给。 */
        if (baseType && baseType->kind == MType::Obj)
        {
            const simple_ast::NodePtr receiver = LowerExpr(receiverId);
            if (methodName == "copy")
            {
                if (!argIds.empty())
                {
                    Fail("obj.copy() 不带参数");
                    return Bad();
                }
                return simple_ast::Node::call("__vm_object_copy", { "__magel_o" }, { receiver });
            }
            if (methodName == "isEmpty")
            {
                if (!argIds.empty()) return Fail("Any.isEmpty() 不接受参数"), Bad();
                return simple_ast::Node::binary("==",
                                                simple_ast::Node::call("__vm_obj_name", { "__magel_o" }, { receiver }),
                                                simple_ast::Node::i(0));
            }
            if (methodName == "clear")
            {
                if (!CheckMutableReceiver(receiverId)) return Bad();
                if (!argIds.empty()) return Fail("Any.clear() 不接受参数"), Bad();
                return simple_ast::Node::call("__vm_obj_set",
                                              { "__magel_o", "__magel_v", "__magel_t", "__magel_s", "__magel_obj", "__magel_n" },
                                              { receiver, simple_ast::Node::i(0), simple_ast::Node::i(0),
                                                simple_ast::Node::i(0), simple_ast::Node::i(0), simple_ast::Node::str("") });
            }
            if (methodName == "__isTypeTag")
            {
                if (node.module != "std::lang") return Fail("内部 Any 类型标签仅供核心库使用；请使用 isType<T>"), Bad();
                if (argIds.size() != 1u) return Fail("内部 Any 类型判断需要一个标签"), Bad();
                return simple_ast::Node::call("__vm_obj_has_tag", { "__magel_o", "__magel_t" },
                                              { receiver, LowerExpr(argIds[0]) });
            }
            if (methodName == "set")
            {
                if (!CheckMutableReceiver(receiverId)) return Bad();
                if (argIds.size() != 1u)
                {
                    Fail("obj.set(值) 要一个参数，给了 " + Memory::toString(argIds.size()) + " 个");
                    return Bad();
                }
                RecordBorrowStore(receiverId, argIds[0]);
                /* 注意：这里返回的是**裸调用**（不再套 `exprStmt`）—— 语句那一层
                 * （`LowerStmt` 的 ExprStmt）会统一套一次；套两次的话内层那个
                 * `ExprStmt` 节点在 codegen 里没人认，整条语句会**静默消失**。 */
                return LowerObjSet(receiver, argIds[0], "obj.set");
            }
            if (methodName == "getValue" || methodName == "getReference" || methodName == "getOptional")
            {
                if (!argIds.empty())
                {
                    Fail("std::lang::Any." + methodName + "() 不带参数（要存进去用 set）");
                    return Bad();
                }
                if (!g_expectedObjGet)
                {
                    Fail("std::lang::Any." + methodName + "() 的类型要从返回值上下文来，例如 `int x = o.getValue();`"
                                                          "（盒子里装着什么是运行期才知道的，所以「取出来当什么用」要写明白）");
                    return Bad();
                }
                const std::shared_ptr<MType> target = g_expectedObjGet;
                if (methodName == "getOptional")
                {
                    const MEnumVariantDef *some = nullptr;
                    const MEnumVariantDef *none = nullptr;
                    if (!OptionVariants(target, &some, &none) || some->payloads.empty() || some->payloads[0].type->kind != MType::Ptr)
                    {
                        Fail("getOptional() 的返回目标必须是 std::lang::Option<T const&> 或 std::lang::Option<T mut&>");
                        return Bad();
                    }
                    if (g_freshSink == nullptr)
                    {
                        Fail("getOptional() 只能用于可接收返回值的表达式上下文");
                        return Bad();
                    }
                    const std::shared_ptr<MType> pointer = some->payloads[0].type;
                    if (!pointer->readOnly && !CheckMutableReceiver(receiverId)) return Bad();
                    Memory::Vector<std::int64_t> optionalTags;
                    Memory::String optionalWhy;
                    if (!ObjGetAllowedTags(pointer, &optionalTags, &optionalWhy, true))
                    {
                        Fail("getOptional() 取不出 " + Describe(pointer) + "：" + optionalWhy);
                        return Bad();
                    }
                    Memory::Vector<Memory::String> testNames{ "__magel_o" };
                    Memory::Vector<simple_ast::NodePtr> testValues{ receiver };
                    Memory::Vector<Memory::String> getNames{ "__magel_o", "__magel_n", "__magel_s", "__magel_f",
                                                             "__magel_obj" };
                    Memory::Vector<simple_ast::NodePtr> getValues{
                        receiver, simple_ast::Node::str(Describe(pointer)), simple_ast::Node::i(0),
                        simple_ast::Node::i(0), simple_ast::Node::i(0)
                    };
                    for (const int tag : optionalTags)
                    {
                        testNames.emplace_back("__magel_t");
                        testValues.push_back(simple_ast::Node::i(tag));
                        getNames.emplace_back("__magel_t");
                        getValues.push_back(simple_ast::Node::i(tag));
                    }
                    const char *getNative = IsInterfacePointer(pointer) ? "__vm_obj_get_ptr" : "__vm_obj_get";
                    const simple_ast::NodePtr raw = simple_ast::Node::call(
                        getNative, getNames, getValues);
                    const simple_ast::NodePtr someValue = simple_ast::Node::call(
                        "__magel_enum_" + target->name + "_" + some->name,
                        { EnumPayloadParameterName(target->name, some->name, 0u) }, { raw });
                    const simple_ast::NodePtr noneValue = simple_ast::Node::call(
                        "__magel_enum_" + target->name + "_" + none->name, {}, {});
                    const Memory::String temporary = AllocateName("__magel_optional_pointer");
                    g_freshSink->push_back(
                        simple_ast::Node::declare(temporary, LowerType(target, temporary)));
                    g_freshSink->push_back(simple_ast::Node::ifThenElse(
                        simple_ast::Node::call("__vm_obj_has_tag", testNames, testValues),
                        simple_ast::Node::assign(temporary, someValue),
                        simple_ast::Node::assign(temporary, noneValue)));
                    return simple_ast::Node::var(temporary);
                }
                if (methodName == "getReference" && target->kind != MType::Ptr)
                {
                    Fail("getReference() 的返回目标必须是引用，现在是 " + Describe(target));
                    return Bad();
                }
                if (methodName == "getValue" && target->kind == MType::Ptr)
                {
                    Fail("引用请使用 std::lang::Any.getReference()，不要使用 getValue()");
                    return Bad();
                }
                if (methodName == "getReference" && target->kind == MType::Ptr && !target->readOnly && !CheckMutableReceiver(receiverId)) return Bad();
                Memory::Vector<std::int64_t> allowed;
                Memory::String why;
                if (!ObjGetAllowedTags(target, &allowed, &why))
                {
                    Fail("std::lang::Any." + methodName + "() 取不出 " + Describe(target) + "：" + why);
                    return Bad();
                }
                Memory::Vector<Memory::String> names{ "__magel_o", "__magel_n", "__magel_s", "__magel_f",
                                                      "__magel_obj" };
                Memory::Vector<simple_ast::NodePtr> values{
                    receiver, simple_ast::Node::str(Describe(target)),
                    simple_ast::Node::i(target->kind == MType::String ? 1 : 0),
                    simple_ast::Node::i(target->kind == MType::Double ? 1 : 0),
                    simple_ast::Node::i(IsObjectLike(target) ? 1 : 0)
                };
                for (const int tag : allowed)
                {
                    names.emplace_back("__magel_t");
                    values.push_back(simple_ast::Node::i(tag));
                }
                /* 目标是**接口引用**：取出来的是胖引用（地址 + 类型号），
                 * 走 `__vm_obj_get_ptr` 复制一份；别的类型取出来就是裸值
                 * （字符串那条给的是**堆上的新串**，归变量所有、出块自动释放）。 */
                const char *native = IsInterfacePointer(target) ? "__vm_obj_get_ptr" : "__vm_obj_get";
                return simple_ast::Node::call(native, names, values);
            }
            Fail("std::lang::Any 只有 set / getValue / getReference / getOptional / isEmpty / clear / copy 方法，没有 " + methodName);
            return Bad();
        }
        /* ---- 接口引用上的方法：运行期按"胖引用里的类型号"分派到具体实现 ----
         *
         * `q.area()` → `__magel_iface.Shape.area(q, 实参…)`，那个分派函数是编译期生成的
         * （见 MakeInterfaceDispatcher）：它读出 q 的类型号，挑到 `Circle.area` / `Square.area`…
         * 再把 q 里的地址当 `self` 传下去。 */
        if (IsInterfacePointer(baseType))
        {
            const Memory::String iface = baseType->elem->name;
            std::shared_ptr<MType> interfaceResult;
            Memory::Vector<std::shared_ptr<MType>> interfaceParams;
            if (!FindInterfaceMethod(iface, methodName, &interfaceResult, &interfaceParams))
            {
                Fail("接口 " + iface + " 没有方法 " + methodName + "（接口只声明函数；调用要走接口里声明过的名字）");
                return Bad();
            }
            if (argIds.size() != interfaceParams.size())
            {
                Fail("接口 " + iface + "." + methodName + " 需要 " + Memory::toString(interfaceParams.size()) + " 个参数，给了 " + Memory::toString(argIds.size()) + " 个");
                return Bad();
            }
            Memory::Vector<Memory::String> names{ "__magel_self", "__magel_readonly" };
            Memory::Vector<simple_ast::NodePtr> values{
                LowerExpr(receiverId), simple_ast::Node::i(IsReadOnlyAccess(receiverId) ? 1 : 0)
            };
            for (std::size_t index = 0; index < argIds.size(); ++index)
            {
                names.push_back("__magel_a" + Memory::toString(index));
                values.push_back(LowerArgumentValue(argIds[index], interfaceParams[index]));
            }
            MarkInterfaceMethodUsed(iface, methodName);
            return simple_ast::Node::call(InterfaceLabel(iface, methodName), names, values);
        }
        if (IsContainer(baseType))
        {
            const bool mutates = methodName == "push" || methodName == "insert" || methodName == "pop" || methodName == "remove" || methodName == "clear" || methodName == "add" || methodName == "put";
            if (mutates && !CheckMutableReceiver(receiverId)) return Bad();
            const MContainerMethod *method = FindContainerMethod(baseType, methodName);
            if (method == nullptr)
            {
                Fail("容器 " + Describe(baseType) + " 没有方法 " + methodName + "（支持的方法见 README 的「容器」一节）");
                return Bad();
            }
            if (argIds.size() != static_cast<std::size_t>(method->arity))
            {
                Fail(Describe(baseType) + "." + methodName + " 需要 " + Memory::toString(method->arity) + " 个参数，给了 " + Memory::toString(argIds.size()) + " 个");
                return Bad();
            }
            if (methodName == "push" || methodName == "insert" || methodName == "add" || methodName == "put")
                for (const int argument : argIds)
                    RecordBorrowStore(receiverId, argument, true);
            const bool elementText = baseType->elem && baseType->elem->kind == MType::String;
            const bool valueText =
                baseType->elem2 ? (baseType->elem2->kind == MType::String) : elementText;
            const std::shared_ptr<MType> secondType =
                baseType->elem2 ? baseType->elem2 : baseType->elem;
            Memory::Vector<Memory::String> names{ "__magel_v" };
            Memory::Vector<simple_ast::NodePtr> values{ OwnedObjectTemp(receiverId, baseType) };
            /* 写进容器的"当场造出来的聚合"要先落临时、调用后再递归释放它的叶子。 */
            Memory::Vector<simple_ast::NodePtr> post;
            const Memory::String &shape = method->shape;
            if (shape.find('f') != Memory::String::npos)
                RecordObjectArgumentBorrow(receiverId, baseType);
            for (std::size_t index = 1u; index < shape.size(); ++index)
            {
                switch (shape[index])
                {
                case 'h': break; /* 接收者已经在最前面 */
                case 'i':
                    if (!CheckContainerIndex(argIds[0], methodName + " 下标")) return Bad();
                    names.emplace_back("__magel_i");
                    values.push_back(LowerExpr(argIds[0]));
                    break;
                case 'v':
                case 'k':
                    if (!CheckContainerArgument(baseType->elem, argIds[0],
                                                methodName + " 的元素/键",
                                                methodName == "push" || methodName == "add" || methodName == "put")) return Bad();
                    names.emplace_back("__magel_k");
                    values.push_back(ContainerStoreValue(argIds[0], baseType->elem, &post));
                    break;
                case 'w':
                    if (!CheckContainerArgument(secondType, argIds[1],
                                                methodName + " 的值", true)) return Bad();
                    names.emplace_back("__magel_x");
                    values.push_back(ContainerStoreValue(argIds[1], secondType, &post));
                    break;
                case 'f':
                    if (!ExprType(argIds.back()) || ExprType(argIds.back())->kind != MType::Func)
                    {
                        Fail(methodName + " 的回调参数必须是函数指针");
                        return Bad();
                    }
                    names.emplace_back("__magel_callback");
                    values.push_back(LowerExpr(argIds.back()));
                    break;
                case 'n':
                case 't':
                    /* `n` = 元素是不是字符串；`t` = map 的**键**是不是字符串
                     * （map 的 `elem` 就是键类型，所以两者是同一个判断）。 */
                    names.emplace_back("__magel_t");
                    values.push_back(simple_ast::Node::i(elementText ? 1 : 0));
                    break;
                case 'm':
                    names.emplace_back("__magel_t");
                    values.push_back(simple_ast::Node::i(valueText ? 1 : 0));
                    break;
                case 'o':
                    names.emplace_back("__magel_o");
                    values.push_back(ContainerStorageFlag(baseType->elem));
                    break;
                case 'p':
                    names.emplace_back("__magel_o");
                    values.push_back(ContainerStorageFlag(secondType));
                    break;
                default: break;
                }
            }
            const simple_ast::NodePtr call = simple_ast::Node::call(method->native, names, values);
            if (!post.empty())
            {
                const auto result = AllocateName("__magel_container_result");
                Memory::Vector<simple_ast::NodePtr> sequence{
                    simple_ast::Node::declare(result, LowerType(ContainerMethodType(baseType, *method), result)),
                    simple_ast::Node::assign(result, call)
                };
                sequence.insert(sequence.end(), post.begin(), post.end());
                const auto value = method->indexTest ? simple_ast::Node::binary("!=", simple_ast::Node::var(result), simple_ast::Node::i(-1)) : simple_ast::Node::var(result);
                return simple_ast::Node::sequence(std::move(sequence), value);
            }
            if (!method->indexTest) return call;
            /* `vec.contains(x)` = "找到的下标 != -1" */
            return simple_ast::Node::binary("!=", call, simple_ast::Node::i(-1));
        }
    }

    /* 函数指针调用：`fp(3, 4)` —— 被调者是**值**（变量 / 字段），不是函数名。 */
    if (IsIndirectCallee(node.kids[0]))
    {
        const std::shared_ptr<MType> targetType = ExprType(node.kids[0]);
        {
            const bool variadic = targetType->variadic;
            const std::size_t fixed = variadic ? targetType->paramTypes.size() - 1u : targetType->paramTypes.size();
            if ((!variadic && args.size() != fixed) || (variadic && args.size() < fixed))
            {
                Fail("函数指针调用的实参个数不对：需要 " + Memory::toString(fixed) + (variadic ? " 个以上" : " 个") + "，给了 " + Memory::toString(args.size()) + " 个");
                return Bad();
            }
            Memory::Vector<Memory::String> names;
            Memory::Vector<simple_ast::NodePtr> values;
            for (std::size_t index = 0; index < fixed; ++index)
            {
                names.push_back("__magel_p" + Memory::toString(index)); /* 按位置绑定，名字只是占位 */
                values.push_back(LowerArgumentValue(args[index], targetType->paramTypes[index]));
            }
            if (variadic)
            {
                MParam pack{ "__magel_variadic", "__magel_p" + Memory::toString(fixed),
                             targetType->paramTypes.back(), true };
                names.push_back(pack.emitted);
                values.push_back(PackVariadicArguments(args, fixed, pack));
            }
            // 容器下标或函数返回的闭包是独立拥有值；调用结束后随完整表达式清理。
            // 变量和字段继续借用原值，不能释放其闭包环境。
            return simple_ast::Node::callPointer(OwnedObjectTemp(node.kids[0], targetType), names, values);
        }
    }

    if (!standardName.empty())
    {
        if (callee.s == "print" || callee.s == "println")
            return BuildBracePrint(args, callee.s == "println");
        /* `std::lang::format("…", 参数…)`：与 `std::lang::print` 共用同一套 `{}` 占位符规则，
         * 区别只在结果 —— 它拼出一个**新串**而不是写输出流。 */
        if (callee.s == "format") return BuildBraceFormat(args);

        /*
         * `std::lang::pause()`：调试用"临时停一下"，等到一行输入再继续。
         * 非交互式输入（管道 / CI / 嵌入式宿主）时 VM 直接返回，所以批量跑样例不会挂住。
         * 没参数，返回 0（和 `println` 一样，可以当表达式语句写）。
         */
        if (callee.s == "pause")
        {
            if (!args.empty())
            {
                Fail("std::lang::pause 不接受参数");
                return Bad();
            }
            return simple_ast::Node::call("__vm_pause", {}, {});
        }

        /*
         * `std::lang::BigInteger(x)` / `std::lang::BigDecimal(x)`：按名字构造大数。
         *
         * 原来靠 `STD_BINT` / `STD_BFLOAT` 两个拼写 token 的专用文法规则；token 删掉之后
         * 在调用点按名字认，并且**复用同一套构造节点**（`M_BigIntCtor` / `M_BigFloatCtor`），
         * 所以类型、降级、所有权规则与 `bint(x)` 完全一致。
         */
        if (callee.s == "BigInteger" || callee.s == "BigDecimal")
        {
            if (args.size() != 1u)
            {
                Fail("std::lang::" + callee.s + " 需要一个参数：std::lang::" + callee.s + "(值)");
                return Bad();
            }
            /* 复用**原来的** `M_StdNumberCtor`：它按实参类型派发"从文本 / 整数 / 浮点 /
             * 已有大数"四条路（从已有大数走的是转发而不是重建），与 `bint(x)` 语义一致。 */
            const int constructed = M_StdNumberCtor(M_Name("std::lang"),
                                                    callee.s == "BigInteger" ? 0 : 1, args[0]);
            return LowerExpr(constructed);
        }
        if (callee.s == "address")
        {
            if (args.size() != 1u || RawExprType(args[0])->kind != MType::Ptr)
                return Fail("std::lang::address 需要一个引用参数"), Bad();
            if (IsInterfacePointer(RawExprType(args[0])))
                return simple_ast::Node::call("__vm_obj_addr", { "__magel_o" }, { LowerRawExpr(args[0]) });
            // address 的语言结果为 uint64；不能把引用节点的底层类型带进整数运算。
            return simple_ast::Node::cast(LowerRawExpr(args[0]), simple_ast::Type::u64());
        }
        if (callee.s == "putChar")
        {
            if (args.size() != 1u || ExprType(args[0])->kind != MType::Char)
            {
                Fail("std::lang::putChar 只接受 char；整数请显式写 `(char)value`");
                return Bad();
            }
            return simple_ast::Node::call("__vm_put_char", { "__magel_char" },
                                          { LowerExpr(args[0]) });
        }
        if (callee.s == "sweep")
        {
            if (!args.empty()) return Fail("std::lang::sweep 不接受参数"), Bad();
            return simple_ast::Node::call("__vm_heap_sweep", {}, {});
        }
        if (callee.s == "inputLine" || callee.s == "inputInt" || callee.s == "inputUInt" || callee.s == "inputFloat" || callee.s == "inputBool" || callee.s == "inputChar" || callee.s == "inputBigInteger" || callee.s == "inputBigDecimal")
        {
            if (!args.empty())
            {
                Fail("std::lang::" + callee.s + " 不接受参数");
                return Bad();
            }
            const Memory::Map<Memory::String, Memory::String> natives{
                { "inputLine", "__vm_input_line" }, { "inputInt", "__vm_input_int" }, { "inputUInt", "__vm_input_uint" }, { "inputFloat", "__vm_input_float" }, { "inputBool", "__vm_input_bool" }, { "inputChar", "__vm_input_char" }, { "inputBigInteger", "__vm_input_bint" }, { "inputBigDecimal", "__vm_input_bfloat" }
            };
            return simple_ast::Node::call(natives.at(callee.s), {}, {});
        }
        /*
         * `panic(message)`：唯一的不可恢复终止原语，直接降到 StackVM 的 `__vm_panic`。
         *
         * `exit(n)` 与内建 `assert(x)` 都已删除 —— 需要断言时用标准库的
         * `std::lang::assert(condition[, message])`，它自己调 `std::lang::panic`；语言层不再有
         * 第二条"绕开 panic 直接停机"的路，停机清理因此只有一处入口。
         */
        if (callee.s == "panic")
        {
            if (args.size() != 1u)
            {
                Fail("panic 需要一个参数：panic(message)");
                return Bad();
            }
            const std::shared_ptr<MType> type = ExprType(args[0]);
            if (!type || type->kind != MType::String)
            {
                Fail("panic 的参数必须是 std::lang::String，现在给的是 " + Describe(type));
                return Bad();
            }
            return simple_ast::Node::call(g_hasDefer ? "__magel_defer_panic" : "__vm_panic",
                                          { "__magel_value" }, { LowerExpr(args[0]) });
        }
        /* ---- 内存内建：new / delete / memcpy / memset（数据区上的单调堆 + 逐字节操作）---- */
        if (callee.s == "memcpy" || callee.s == "memset")
        {
            const Memory::String &name = callee.s;
            const std::size_t wanted = 3u;
            if (args.size() != wanted)
            {
                Fail(name + " 需要 " + Memory::toString(wanted) + " 个参数");
                return Bad();
            }
            Memory::Vector<simple_ast::NodePtr> values;
            for (const int argId : args) values.push_back(LowerExpr(argId));
            /* 逐字节搬 / 填：交给虚拟机里那份 C++ 运行时（整段必须落在一块内存里，
             * 越界会报错）。语言侧不写"读 8 字节 → 拼最低字节 → 写回"那种循环了。 */
            if (name == "memcpy")
            {
                return simple_ast::Node::call("__vm_memcpy", { "__magel_tgt", "__magel_src", "__magel_n" },
                                              values);
            }
            return simple_ast::Node::call("__vm_memset", { "__magel_addr", "__magel_val", "__magel_n" },
                                          values);
        }
        /* ---- `str(x)`：转成字符串（大整数 / 数值 / 字符串本身） ---- */
        if (callee.s == "str")
        {
            if (args.size() != 1u)
            {
                Fail("str 需要一个参数：str(值)");
                return Bad();
            }
            const std::shared_ptr<MType> type = ExprType(args[0]);
            if (type->kind == MType::BigInt)
            {
                return simple_ast::Node::call("__vm_bint_str", { "__magel_v" },
                                              { LowerExpr(args[0]) });
            }
            if (type->kind == MType::BigFloat)
            {
                return simple_ast::Node::call("__vm_bfloat_str", { "__magel_v" },
                                              { LowerExpr(args[0]) });
            }
            if (type->kind == MType::String) return LowerExpr(args[0]);
            if (type->kind == MType::Double)
            {
                return simple_ast::Node::call("__vm_str_of_flt", { "__magel_v", "__magel_p" },
                                              { LowerExpr(args[0]), simple_ast::Node::i(3) });
            }
            if (type->kind == MType::Int || type->kind == MType::Bool)
            {
                return simple_ast::Node::call("__vm_str_of_int", { "__magel_v" },
                                              { LowerExpr(args[0]) });
            }
            Fail("str 只能用在 BigInteger / BigDecimal / 整数 / 浮点 / 字符串上，现在给的是 " + Describe(type));
            return Bad();
        }
        if (callee.s == "fopen" || callee.s == "__fileTryOpen")
        {
            if (args.size() != 2u)
            {
                Fail("fopen 需要两个参数：fopen(路径字符串, 模式字符串)");
                return Bad();
            }
            if (ExprType(args[0])->kind != MType::String || ExprType(args[1])->kind != MType::String)
            {
                Fail("fopen 的路径和模式都必须是 std::lang::String");
                return Bad();
            }
            return simple_ast::Node::call(callee.s == "fopen" ? "__vm_file_open" : "__vm_file_try_open",
                                          { "__magel_path", "__magel_mode" },
                                          { LowerExpr(args[0]), LowerExpr(args[1]) });
        }
        if (callee.s == "__fileValid")
        {
            if (args.size() != 1u || ExprType(args[0])->kind != MType::File)
            {
                Fail("__fileValid 需要一个 file 参数");
                return Bad();
            }
            return simple_ast::Node::call("__vm_file_valid", { "__magel_f" },
                                          { LowerExpr(args[0]) });
        }
        if (callee.s == "fread" || callee.s == "fileRead" || callee.s == "__fileRead")
        {
            if (args.size() != 1u)
            {
                Fail("fread 需要一个参数：fread(file)");
                return Bad();
            }
            if (ExprType(args[0])->kind != MType::File)
            {
                Fail("fread 需要 file 参数，现在给的是 " + Describe(ExprType(args[0])));
                return Bad();
            }
            return simple_ast::Node::call("__vm_file_read", { "__magel_f" }, { LowerExpr(args[0]) });
        }
        if (callee.s == "fwrite" || callee.s == "fileWrite" || callee.s == "__fileWrite")
        {
            if (args.size() != 2u)
            {
                Fail("fwrite 需要两个参数：fwrite(file, 字符串)");
                return Bad();
            }
            if (ExprType(args[0])->kind != MType::File || ExprType(args[1])->kind != MType::String)
            {
                Fail("fwrite 需要 file 和 std::lang::String 参数");
                return Bad();
            }
            return simple_ast::Node::call("__vm_file_write", { "__magel_f", "__magel_s" },
                                          { LowerExpr(args[0]), LowerExpr(args[1]) });
        }
        if (callee.s == "feof" || callee.s == "fileEof" || callee.s == "__fileEof" || callee.s == "fclose" || callee.s == "fileClose" || callee.s == "__fileClose")
        {
            if (args.size() != 1u)
            {
                Fail(callee.s + " 需要一个 file 参数");
                return Bad();
            }
            if (ExprType(args[0])->kind != MType::File)
            {
                Fail(callee.s + " 需要 file 参数，现在给的是 " + Describe(ExprType(args[0])));
                return Bad();
            }
            return simple_ast::Node::call((callee.s == "feof" || callee.s == "fileEof" || callee.s == "__fileEof") ? "__vm_file_eof" : "__vm_file_close",
                                          { "__magel_f" }, { LowerExpr(args[0]) });
        }
        if (callee.s == "prun" || callee.s == "processRun" || callee.s == "__processRun")
        {
            if (args.size() != 1u)
            {
                Fail("prun 需要一个参数：prun(命令字符串)");
                return Bad();
            }
            if (ExprType(args[0])->kind != MType::String)
            {
                Fail("prun 需要 std::lang::String 参数，现在给的是 " + Describe(ExprType(args[0])));
                return Bad();
            }
            return simple_ast::Node::call("std::process::__processRun", { "__magel_cmd" },
                                          { LowerExpr(args[0]) });
        }
        if (callee.s == "pread" || callee.s == "processRead" || callee.s == "__processRead" || callee.s == "pclose" || callee.s == "processClose" || callee.s == "__processClose")
        {
            if (args.size() != 1u)
            {
                Fail(callee.s + " 需要一个 proc 参数");
                return Bad();
            }
            if (ExprType(args[0])->kind != MType::Proc)
            {
                Fail(callee.s + " 需要 proc 参数，现在给的是 " + Describe(ExprType(args[0])));
                return Bad();
            }
            return simple_ast::Node::call((callee.s == "pread" || callee.s == "processRead" || callee.s == "__processRead") ? "std::process::__processRead" : "std::process::__processClose",
                                          { "__magel_p" }, { LowerExpr(args[0]) });
        }
        if (callee.s == "psystem" || callee.s == "system" || callee.s == "__system")
        {
            if (args.size() != 1u)
            {
                Fail("psystem 需要一个参数：psystem(命令字符串)");
                return Bad();
            }
            if (ExprType(args[0])->kind != MType::String)
            {
                Fail("psystem 需要 std::lang::String 参数，现在给的是 " + Describe(ExprType(args[0])));
                return Bad();
            }
            return simple_ast::Node::call("std::process::__system", { "__magel_cmd" },
                                          { LowerExpr(args[0]) });
        }
        const int builtin = BuiltinOpIndex(callee.s);
        {
            /* `len(a)`：元素个数。静态数组是常量（第一维），运行时数组读对象里的长度。 */
            if (callee.s == "len")
            {
                if (args.size() != 1u)
                {
                    Fail("len 需要一个参数：len(数组)");
                    return Bad();
                }
                const std::shared_ptr<MType> type = ExprType(args[0]);
                if (IsRuntimeArray(type))
                {
                    return simple_ast::Node::loadInt(simple_ast::Node::binary(
                        "-", LowerExpr(args[0]), simple_ast::Node::i(8)));
                }
                if (type->kind == MType::Array)
                    return simple_ast::Node::i(type->dims.empty() ? 0 : type->dims[0]);
                if (type->kind == MType::String)
                {
                    /* 字符串：直接读取堆上的显式长度头。实参是"造出来的串"时先落地，
                     * 否则 `len(x.str())` 里那个临时串没人释放。 */
                    return simple_ast::Node::call("__vm_str_len", { "__magel_s" },
                                                  { NeedsOwnedTemp(args[0]) ? OwnedStringTemp(args[0]) : LowerExpr(args[0]) });
                }
                if (IsContainer(type))
                {
                    const char *native = (type->name == "map") ? "__vm_map_len" : (type->name == "set") ? "__vm_set_len" :
                                                                                                          "__vm_vec_len";
                    return simple_ast::Node::call(native, { "__magel_v" }, { LowerExpr(args[0]) });
                }
                Fail("len 只能用在数组、字符串或容器上，现在给的是 " + Describe(type));
                return Bad();
            }
        }
        if (builtin == -2)
        {
            if (args.size() != 2u)
            {
                Fail("index 需要两个参数：index(基址, 下标)");
                return Bad();
            }
            return LowerIndexChain(args[0], args[1], true);
        }
        if (builtin >= 0)
        {
            if (args.size() != 2u)
            {
                Fail("内置函数 " + callee.s + " 需要两个参数");
                return Bad();
            }
            return LowerBinary(kOperatorText[builtin], args[0], args[1]);
        }
    }
    Memory::String label;
    Memory::Vector<Memory::String> params;
    std::shared_ptr<MType> result;
    if (!ResolveCall(id, &label, &params, &result)) return Bad();
    if (g_borrowFlowCapture) g_borrowFlowCapture->calls[id] = label;
    if ((callee.kind == MNode::Member || callee.kind == MNode::Arrow) && !CheckMethodReceiver(callee.kids[0], label)) return Bad();
    const auto signature = g_funcs.find(label);
    if (signature == g_funcs.end()) return Fail("内部错误：调用签名不存在 " + label), Bad();
    if (signature->second.staticCallbacks && g_currentModule != "std::event")
        for (std::size_t i = 0; i < args.size() && i < signature->second.params.size(); ++i)
            if (signature->second.params[i].type->kind == MType::Func && BorrowSourceDepth(args[i]) != 0u)
                return Fail("[MGL1204] 异步事件回调不能捕获局部变量，其生命周期必须覆盖事件登记"), Bad();
    const auto transferCall = [&](const Memory::String &name) {
        return label.rfind(name + "__", 0u) == 0u;
    };
    const bool sendThread = transferCall("std::message::sendThread");
    const bool receiveThread = transferCall("std::message::receiveThread");
    const bool sendCoroutine = transferCall("std::message::sendCoroutine");
    const bool receiveCoroutine = transferCall("std::message::receiveCoroutine");
    /* 共享槽原语是**编译器注入的泛型宿主签名**（`magel_generic.hpp` 里 `module lang` 下
     * 那四个 `__shared*<T>`），不是用户可见的标准库 API —— `Shared<T>` 这个 struct
     * 已经搬到 std::concurrency，但这些 `__` 前缀的 ABI 名字仍挂在 std::lang:: 下，改名要与
     * 注入正文里的声明一起动，别只改一边。 */
    const bool sharedCreate = transferCall("std::lang::__sharedCreate");
    const bool sharedGet = transferCall("std::lang::__sharedGet");
    const bool sharedSet = transferCall("std::lang::__sharedSet");
    const bool sharedSwap = transferCall("std::lang::__sharedSwap");
    if (sendThread || receiveThread || sendCoroutine || receiveCoroutine || sharedCreate || sharedGet || sharedSet || sharedSwap)
    {
        const std::shared_ptr<MType> type =
            (receiveThread || receiveCoroutine || sharedGet || sharedSwap) ? result :
                                                                             signature->second.params.back().type;
        Memory::Set<Memory::String> seen;
        Memory::Function<bool(const std::shared_ptr<MType> &)> supported =
            [&](const std::shared_ptr<MType> &item) -> bool {
            if (!item) return false;
            switch (item->kind)
            {
            case MType::Bool:
            case MType::Char:
            case MType::Int:
            case MType::Double:
            case MType::String:
            case MType::BigInt:
            case MType::BigFloat: return true;
            case MType::Container:
                return supported(item->elem) && (!item->elem2 || supported(item->elem2));
            case MType::Struct:
            {
                EnsureTupleLayout(item);
                const MStructDef *definition = FindStruct(item->name);
                if (!definition || !definition->flattened) return false;
                if (!seen.insert(item->name).second) return false;
                for (const auto &leaf : definition->leaves)
                    if (!supported(leaf.second)) return false;
                seen.erase(item->name);
                return true;
            }
            default: return false;
            }
        };
        if (!supported(type))
            return Fail("消息与 std::lang::Shared 不支持 " + Describe(type) + "：请改用标量、字符串、普通值结构体或容器；引用、数组、资源和 std::lang::Any 不能跨线程传递"), Bad();
        const int kind = type->kind == MType::String ? 2 :
                         type->kind == MType::Struct ? 4 :
                         IsObjectLike(type)          ? 3 :
                         type->kind == MType::Double ? 1 :
                                                       0;
        Memory::Vector<Memory::String> intrinsicNames;
        Memory::Vector<simple_ast::NodePtr> intrinsicValues;
        if (sendThread || sendCoroutine || sharedGet || sharedSet || sharedSwap)
        {
            intrinsicNames.push_back("__magel_target");
            intrinsicValues.push_back(LowerExpr(args[0]));
        }
        intrinsicNames.push_back("__magel_kind");
        intrinsicValues.push_back(simple_ast::Node::i(kind));
        intrinsicNames.push_back("__magel_layout");
        intrinsicValues.push_back(ContainerStorageFlag(type));
        intrinsicNames.push_back("__magel_type");
        intrinsicValues.push_back(simple_ast::Node::str(Describe(type)));
        if (sendThread || sendCoroutine || sharedCreate || sharedSet || sharedSwap)
        {
            const int valueId = args.back();
            intrinsicNames.push_back("__magel_value");
            intrinsicValues.push_back(type->kind == MType::String && NeedsOwnedTemp(valueId) ? OwnedStringTemp(valueId) : LowerValueForType(valueId, type));
        }
        const Memory::String native = sendThread       ? "__vm_message_send_thread" :
                                      receiveThread    ? "__vm_message_receive_thread" :
                                      sendCoroutine    ? "__vm_message_send_coroutine" :
                                      receiveCoroutine ? "__vm_message_receive_coroutine" :
                                      sharedCreate     ? "__vm_shared_create" :
                                      sharedGet        ? "__vm_shared_get" :
                                      sharedSet        ? "__vm_shared_set" :
                                                         "__vm_shared_swap";
        return simple_ast::Node::call(native, intrinsicNames, intrinsicValues);
    }
    // 引用迭代租约携带元素布局，绑定到统一 VM 原生操作。
    const auto &sourceName = label;
    if (sourceName.rfind("std::iter::__iter", 0) == 0)
    {
        if (sourceName.rfind("std::iter::__iterBox__", 0) == 0)
        {
            const auto stateType = ExprType(args[0]);
            if (!stateType || stateType->kind != MType::Struct)
            {
                Fail("迭代器状态必须是结构体");
                return Bad();
            }
            return simple_ast::Node::call("__vm_iter_box", { "state", "layout", "tag", "name" },
                                          { LowerExpr(args[0]), ContainerStorageFlag(stateType),
                                            simple_ast::Node::i(TypeIdOf(PointerType(stateType, false))),
                                            simple_ast::Node::str(Describe(PointerType(stateType, false))) });
        }
        if (sourceName.rfind("std::iter::__iterTake", 0) == 0)
            return simple_ast::Node::call("__vm_iter_take", { "source_address" }, { LowerRawExpr(args[0]) });
        if (sourceName.rfind("std::iter::__iterLease__", 0) == 0)
            return simple_ast::Node::call("__vm_iter_lease", { "source" }, { LowerExpr(args[0]) });
        if (sourceName == "std::iter::__iterLeaseLen")
            return simple_ast::Node::call("__vm_iter_lease_len", { "lease" }, { LowerExpr(args[0]) });
        if (sourceName.rfind("std::iter::__iterConstRef__", 0) == 0 || sourceName.rfind("std::iter::__iterMutRef__", 0) == 0)
        {
            const auto element = result->elem;
            return simple_ast::Node::call("__vm_iter_lease_ref", { "lease", "index", "text", "layout", "float" },
                                          { LowerExpr(args[0]), LowerExpr(args[1]), simple_ast::Node::i(element->kind == MType::String),
                                            ContainerStorageFlag(element), simple_ast::Node::i(IsFloatType(element)) });
        }
    }
    const bool variadic = !signature->second.params.empty() && signature->second.params.back().variadic;
    const std::size_t fixed = variadic ? params.size() - 1u : params.size();
    Memory::Vector<Memory::String> names;
    Memory::Vector<simple_ast::NodePtr> values;
    if (callee.kind == MNode::Member || callee.kind == MNode::Arrow)
    {
        /* 方法：接收者也是实参，simple_ast 按 `self` 绑定（方法体里自己声明过） */
        names.push_back("self");
        /* 链式调用由接收者的隐藏临时变量保存中间值，在完整表达式结束后清理。 */
        /* 接收者是"当场造出来的聚合"（`f().method()`）时，方法只**借用**它：先用一个
         * 内部临时所有者接住，结果保存后析构并释放其字段，否则这份临时没人收尾。 */
        const int receiverId = callee.kids[0];
        const std::shared_ptr<MType> receiverType = ExprType(receiverId);
        if (receiverType && receiverType->kind == MType::Struct)
            values.push_back(OwnedAggregateTempFor(receiverId, receiverType));
        else if (receiverType && IsObjectLike(receiverType))
            /* 容器 / 大数上的方法：接收者也是"当场造出来的"就保存为表达式临时所有者
             * （`Matrix<T>::zeros(3, 3).at(0, 0)` 里那个矩阵不能漏）。 */
            values.push_back(OwnedObjectTemp(receiverId, receiverType));
        else
            values.push_back(LowerExpr(receiverId));
    }
    for (std::size_t index = 0; index < fixed; ++index)
    {
        names.push_back(params[index]);
        // 核心枚举内部转送已有借用；标准游标不保存到全局，允许借用跨度在
        // 调用链上继续传播。间接用户回调仍严格检查，不能隐藏来源后逃逸。
        const bool libraryForward = (g_currentModule == "std::lang" && label.rfind("std::lang::", 0) == 0) || label.rfind("std::iter::", 0) == 0;
        values.push_back(index < args.size() ? LowerArgumentValue(args[index], signature->second.params[index].type, !libraryForward) : LowerDefaultArgument(signature->second.params[index]));
    }
    if (variadic)
    {
        names.push_back(params.back());
        values.push_back(PackVariadicArguments(args, fixed, signature->second.params.back()));
    }
    /* 纯宿主函数（`T name(...);` / `Declarations::hostFunction`）与接口成员用同一条规矩：
     * 解释 / 内嵌路径这次编译完就要跑，注册表里没有的宿主符号现在就该报错，而不是等到
     * 运行期"函数入口不存在"。放在所有语义检查（如消息类型、参数匹配）之后，免得把更
     * 贴切的诊断盖掉。`--emit` / `--svmb` 只产出 .quad，符号由目标程序的 `bindMagelGlue`
     * 稍后注册，那条路不查（与宿主接口方法一致）。 */
    if (g_executeNow && g_nativeRegistry != nullptr && IsRegistryHostFunctionLabel(label) && g_nativeRegistry->find(label) == nullptr)
    {
        return Fail("宿主函数没有绑定：" + label + "（宿主用 registry.bindTyped / bindTypedWithContext 注册）"),
               Bad();
    }
    /* 显式调用 MAGEL 实现的 `drop()` / `close()`：从自动析构名单里摘掉，别再跑一次。
     * 只看光秃秃的变量接收者；`self.drop()` / `p.close()` 这类没法在编译期归属到某个变量。
     * `drop()` 是终结操作：先补 `close()`，之后系统不再自动回收（见 FinishExplicitDrop）。 */
    const simple_ast::NodePtr call = simple_ast::Node::call(label, names, values);
    if ((callee.kind == MNode::Member || callee.kind == MNode::Arrow) && callee.s == "drop" && args.empty())
    {
        const auto owner = StructNameOf(ExprType(callee.kids[0]));
        if (!owner.empty() && StructHasInterface(owner, "Drop"))
        {
            const auto variable = N(callee.kids[0]).kind == MNode::Var ? FindIn(N(callee.kids[0]).module, N(callee.kids[0]).s) : nullptr;
            return FinishExplicitDrop(owner, variable ? variable->emitted : Memory::String(), values.front(), call);
        }
    }
    if ((callee.kind == MNode::Member || callee.kind == MNode::Arrow) && args.empty() && signature->second.params.empty() && callee.kids[0] >= 0 && N(callee.kids[0]).kind == MNode::Var)
    {
        const Memory::String ownerName = StructNameOf(ExprType(callee.kids[0]));
        const MVar *variable = FindIn(N(callee.kids[0]).module, N(callee.kids[0]).s);
        if (variable != nullptr && !ownerName.empty())
        {
            if (callee.s == "drop" && StructHasInterface(ownerName, "Drop"))
                return FinishExplicitDrop(ownerName, variable->emitted, values.front(), call);
            if (callee.s == "close" && StructHasInterface(ownerName, "Closeable"))
                UnmarkDestructor(variable->emitted, false, true);
        }
    }
    return call;
}

/**
 * @brief 元素字节数：堆上一个 `T` 占多少字节（数据区槽位一律 8 字节，结构体是字段数 × 8）。
 * @param type 元素类型。
 * @return 字节数（至少 8）。
 */
int ElementBytes(const std::shared_ptr<MType> &type)
{
    if (!type) return 8;
    if (type->kind == MType::Struct)
    {
        const int stride = StructStride(type);
        if (stride > 0) return stride;
    }
    return 8;
}

/**
 * @brief `new` / `new [n]`：在**虚拟机的堆**上要一块内存。
 *
 * **元素类型不写在 `new` 后面**（声明里已经有了），所以先看上下文
 * (`g_expectedNewType` = 声明 / 赋值的目标类型)：
 *   * `T mut& p = new;` → 可写引用 `T mut&`：`(alloc, 元素字节数, _, t)`；
 *   * `T a[] = new [N];` → 运行时长度数组 `T[]`：调运行时小函数
 *     `__magel_new_array(N, 步长)`，它在块头写下元素个数、返回元素起始，
 *     后面 `a[i]` 的越界检查就是读这个长度（`i < len`）。
 *
 * 1.0.0 起 `new(实参)` 已删除：带初值的对象走 `T v = T::new(实参);`（`static T new(...)` 工厂）。
 */
/**
 * @brief 生成私有执行期分派标签，不作为稳定元数据身份。
 * @param id 类型节点。
 * @return 编译期整数常量。
 */
simple_ast::NodePtr LowerTypeTag(int id)
{
    const auto &node = N(id);
    if (node.module != "std::lang" && node.module != "std::reflect" && !node.reflectionBridge)
        return Fail("内部类型标签仅供核心库与反射桥接使用；请使用 isType<T> 或 std::reflect"), Bad();
    if (node.type < 0) return Fail("内部类型标签缺少具体类型"), Bad();
    const auto type = T(node.type);
    // 接口没有值布局，但反射需要查询接口声明本身的身份。
    if (!(type && type->kind == MType::Struct && g_interfaces.count(type->name)) && !CheckDeclaredType(type, "内部类型标签")) return Bad();
    return simple_ast::Node::i(TypeIdOf(type));
}

/**
 * @brief 为动态指针分配表达式生成内存申请代码。
 * @param id 待查找或操作的对象编号。
 * @retval 生成的低层语法节点。
 */
simple_ast::NodePtr LowerNewPtr(int id)
{
    const MNode &node = N(id);
    const std::shared_ptr<MType> target = g_expectedNewType;
    if (!target)
    {
        Fail("`new` 的类型要从声明 / 赋值来：写成 `T mut& p = new;` 或 `T a[] = new [n];`"
             "（不能把 new 直接当实参 / 返回值用）");
        return Bad();
    }
    std::shared_ptr<MType> element;
    if (target->kind == MType::Ptr || target->kind == MType::Array) element = target->elem;
    if (!element || element->kind == MType::Void)
    {
        Fail("`new` 只能赋给引用（`T mut& p`）或运行时数组（`T a[]`），不能给 " + Describe(target));
        return Bad();
    }
    if (element->kind == MType::Struct && g_structs.count(element->name) == 0u)
    {
        Fail("`new` 的元素类型没有声明：" + element->name);
        return Bad();
    }
    const int bytes = ElementBytes(element);
    if (node.i < 0) return AllocateDefaultObject(element); /* 单个对象 */
    /* `new T[N]`：长度是运行期的表达式 → 运行时长度数组 */
    const auto lengthType = ExprType(static_cast<int>(node.i));
    if (!lengthType || lengthType->kind != MType::Int)
    {
        Fail("数组长度必须是整数，得到 " + (lengthType ? Describe(lengthType) : Memory::String("?")));
        return Bad();
    }
    g_usesNewArray = true;
    return simple_ast::Node::call("__magel_new_array", { "__magel_len", "__magel_stride" },
                                  { LowerExpr(static_cast<int>(node.i)), simple_ast::Node::i(bytes) });
}

/**
 * @brief 调用一个值上的 `drop()` / `close()` 析构（`std::lang::Drop` / `std::lang::Closeable` 协议）。
 *
 * `x` 是光秃秃的变量时顺手取消它本次作用域的自动析构（`UnmarkDestructor`）——
 * 显式来一次、出块再来一次就是双重析构。失败时已经 `Fail` 过，返回 `nullptr`。
 */
simple_ast::NodePtr MakeDestructorCall(int valueId)
{
    const std::shared_ptr<MType> type = ExprType(valueId);
    const Memory::String owner = StructNameOf(type);
    if (owner.empty()) return nullptr;
    const bool drop = StructHasInterface(owner, "Drop");
    const bool close = StructHasInterface(owner, "Closeable");
    const bool aggregate = StructHasOwnedLeaves(owner);
    if (!drop && !close && !aggregate) return nullptr;
    const simple_ast::NodePtr value = LowerExpr(valueId);
    Memory::Vector<simple_ast::NodePtr> stmts;
    if (close)
        if (const simple_ast::NodePtr call = MakeCloseCallAt(owner, value))
            stmts.push_back(call);
    if (drop)
        if (const simple_ast::NodePtr call = MakeDropCallAt(owner, value))
            stmts.push_back(call);
    if (aggregate)
        if (const simple_ast::NodePtr release = MakeAggregateRelease(owner, value))
            stmts.push_back(release);
    if (stmts.empty())
    {
        Fail("struct " + owner + " 实现了 Drop / Closeable 但缺少 drop() / close()");
        return nullptr;
    }
    if (N(valueId).kind == MNode::Var)
    {
        const MVar *variable = FindIn(N(valueId).module, N(valueId).s);
        if (variable != nullptr)
        {
            UnmarkDestructor(variable->emitted, drop, close);
            if (aggregate) UnmarkAggregate(variable->emitted);
        }
    }
    if (stmts.size() == 1u) return stmts.front();
    return simple_ast::Node::block(stmts);
}

/**
 * @brief 这个类型是不是"实现了 `std::lang::Drop` / `std::lang::Closeable` 的普通值"（`delete` 靠它决定要不要调析构）。
 * @param type 类型描述。
 * @retval 满足检查条件或操作完成时为 true，检查失败或无法完成时为 false。
 */
bool IsDestructibleValue(const std::shared_ptr<MType> &type)
{
    const Memory::String owner = StructNameOf(type);
    return !owner.empty() && (StructHasInterface(owner, "Drop") || StructHasInterface(owner, "Closeable") || StructHasOwnedLeaves(owner));
}

/**
 * @brief `delete x;`：**唯一**的"处理掉这个变量"语句（旧的 `drop x;` 已经并进这里）。
 *
 * 按 `x` 的类型分派：
 *   * **运行时数组** `T[]`：句柄前面 8 字节是块头（存长度），还回去的是块首 `句柄 - 8`；
 *   * **对象类 / 资源句柄**（`bint` / 容器 / `obj` / `file` / `proc`）：还的是对象表里那个对象；
 *   * **实现了 `std::lang::Drop` 的普通值**：调用它的 `drop()`（等于旧的 `drop x;`）；
 *   * **可写引用** `T mut&`：`new T` 拿到的地址本身就是块首，直接还；
 *   * 只读引用 `T*`、标量等不持有资源的类型 → 编译期报错。
 *
 * 作用域检测（`x` 是光秃秃的变量时）：
 *   * `const` 变量不能 delete；
 *   * 同一变量**中间没有重新赋值**就 delete 两次 → 编译期报错（赋值会清掉这个记号）；
 *   * 删过之后把它从"自动释放名单"里摘掉（`UnmarkOwned`）：离开块时不会再还一次，而变量里
 *     留着的仍是那个**已经失效的地址** —— 再用它访问照样报"堆越界读 / 已经 free 过"，
 *     诊断不会因为自动释放而变模糊。
 *
 * 删过之后再用它访问（`a[i]` / `p[0]` / `p->f`）会被虚拟机拦住 —— 那块内存已经不在
 * 已分配表里：下标检查会说"地址不在任何一块已分配内存里"，读 / 写会说"堆越界"。
 */
simple_ast::NodePtr LowerDelete(int id)
{
    const MNode &node = N(id);
    if (node.kids.empty()) return nullptr;
    const MNode &target = N(node.kids[0]);
    MVar *variable = target.kind == MNode::Var ? FindIn(target.module, target.s) : (target.kind == MNode::EnumCtor ? FindQualifiedGlobal(target.s, target.s2) : nullptr);
    if (variable != nullptr)
    {
        if (variable->isConst)
            return Fail("不能 delete 只读（const）变量：" + target.s), Bad();
    }

    const std::shared_ptr<MType> type = RawExprType(node.kids[0]);
    simple_ast::NodePtr removal;
    /* 对象表句柄（`bint` / 容器 / obj / 接口引用 / file / proc）的 `delete` 只"还走
     * 这一条路径"：同一个变量在别的分支上可能还活着，出块时仍要自动兜底。句柄单调
     * 递增、永不复用，出块再 `drop` 一次已经还过的句柄是空操作（`Op::Drop` 软释放）。 */
    bool keepAutoDrop = false;
    if (IsRuntimeArray(type))
    {
        using simple_ast::Node;
        const auto slot = AllocateName("__magel_delete_array");
        const auto value = Node::var(slot);
        const auto base = Node::binary("-", value, Node::i(8));
        Memory::Vector<simple_ast::NodePtr> actions;
        if (ArrayElementNeedsRelease(type->elem))
            if (const auto release = MakeArrayElementRelease(slot, type->elem)) actions.push_back(release);
        actions.push_back(Node::freeMemory(base));
        removal = Node::block({ Node::declare(slot, simple_ast::Type::i64()), Node::assign(slot, LowerRawExpr(node.kids[0])),
                                Node::ifThenElse(Node::call("__vm_heap_delete_live", { "address" }, { base }), Node::block(actions), nullptr) });
    }
    else if (IsObjectLike(type) || IsResourceLike(type))
    {
        /* `delete 大整数;` / `delete 容器;` / `delete file;` / `delete proc;`：
         * 把对象表里那个对象还回去；已释放的对象句柄重复回收无效果。 */
        keepAutoDrop = true;
        removal = simple_ast::Node::freeMemory(LowerRawExpr(node.kids[0]));
    }
    else if (type->kind == MType::Ptr && type->elem && type->elem->kind == MType::Struct)
    {
        if (type->readOnly) return Fail("delete 只能用在可写引用（`T mut&`）或运行时数组（`T[]`）上；只读引用（`T const&`）不持有对象"), Bad();
        // new 创建的拥有资源对象：先按活动载荷析构，再释放对象本身的分配块。
        const auto slot = AllocateName("__magel_delete_object");
        Memory::Vector<simple_ast::NodePtr> out{
            simple_ast::Node::declare(slot, simple_ast::Type::i64()),
            simple_ast::Node::assign(slot, LowerRawExpr(node.kids[0]))
        };
        const auto value = simple_ast::Node::var(slot);
        const auto owner = type->elem->name;
        AppendDestruction(&out, value, owner, StructHasInterface(owner, "Drop"), StructHasInterface(owner, "Closeable"));
        if (const auto release = MakeAggregateRelease(owner, value)) out.push_back(release);
        out.push_back(simple_ast::Node::freeMemory(value));
        const auto actions = simple_ast::Node::block(Memory::Vector<simple_ast::NodePtr>(out.begin() + 2, out.end()));
        out.resize(2);
        out.push_back(simple_ast::Node::ifThenElse(simple_ast::Node::call("__vm_heap_delete_live", { "address" }, { value }), actions, nullptr));
        removal = simple_ast::Node::block(out);
        if (variable)
        {
            UnmarkDestructor(variable->emitted, true, true);
            UnmarkAggregate(variable->emitted);
        }
    }
    else if (IsDestructibleValue(type))
    {
        removal = MakeDestructorCall(node.kids[0]);
        if (removal == nullptr) return Bad();
        if (variable && g_parameterValueStorage.count(variable->emitted))
            removal = simple_ast::Node::block({ removal, MakeDrop(variable->emitted, false) });
    }
    else if (type->kind != MType::Ptr)
    {
        Fail("delete 只能用在可写引用（`T mut&`）、运行时数组（`T[]`）、"
             "`bint` / 容器 / obj 或 file / proc 资源句柄，"
             "以及实现了 std::lang::Drop 的值上，现在给的是 "
             + Describe(type));
        return nullptr;
    }
    else if (type->readOnly)
    {
        Fail("delete 只能用在可写引用（`T mut&`）或运行时数组（`T[]`）上；只读引用（`T const&`）不持有对象");
        return nullptr;
    }
    else
    {
        removal = simple_ast::Node::freeMemory(LowerRawExpr(node.kids[0]));
    }

    /* `delete x;`（x 是变量）：
     *   * 对象表句柄（`delete 大数;` / `delete 容器;` / 资源）：**保留**自动释放登记 ——
     *     显式 `delete` 可能只在某条分支上跑（`if (…) { delete x; return; }`），别的分支上
     *     x 还活着，出块时要靠自动释放还回去。还过的那条路径出块再 `drop` 一次是空操作。
     *   * 其它（运行期数组 / 实现了 Drop 的值 / 弹性引用）：从自动名单里摘掉，出块不再来一次。
     *     （运行期数组是堆地址、分配器允许复用时不安全；不做基于非复用的假设。） */
    if (variable != nullptr)
    {
        if (!keepAutoDrop) UnmarkOwned(variable->emitted);
    }
    return removal;
}

/**
 * @brief `E::Variant(args...)`：生成一个“标签 + 全部载荷槽”的枚举值。
 * @param id 待查找或操作的对象编号。
 * @retval 生成的低层语法节点。
 */
simple_ast::NodePtr LowerEnumCtor(int id)
{
    const MNode &node = N(id);
    const auto enumIt = g_enums.find(node.s);
    if (enumIt == g_enums.end())
    {
        Fail("没有声明的 enum：" + node.s);
        return Bad();
    }
    const MEnumVariantDef *variant = FindEnumVariant(node.s, node.s2);
    if (variant == nullptr)
    {
        Fail("enum " + node.s + " 没有变体 " + node.s2);
        return Bad();
    }
    if (node.kids.size() != variant->payloads.size())
    {
        Fail("enum 构造器 " + node.s + "::" + node.s2 + " 需要 " + Memory::toString(variant->payloads.size()) + " 个载荷，实际给了 " + Memory::toString(node.kids.size()) + " 个");
        return Bad();
    }
    Memory::Vector<Memory::String> parameterNames;
    Memory::Vector<simple_ast::NodePtr> arguments;
    /*
     * 结构体载荷是**按值**进聚合的（构造器会深拷贝它槽里的字符串 / 对象）。
     * 实参如果是**当场造出来的聚合值**（嵌套的枚举构造器 / 工厂调用），它本身就是
     * 一份没人接的临时值：先落进临时变量，等构造器把它拷进结果之后，再把这份临时值收尾。
     */
    Memory::Vector<simple_ast::NodePtr> pre;
    Memory::Vector<simple_ast::NodePtr> post;
    for (std::size_t index = 0; index < variant->payloads.size(); ++index)
    {
        const MEnumPayload &payload = variant->payloads[index];
        parameterNames.push_back(EnumPayloadParameterName(node.s, node.s2, index));
        simple_ast::NodePtr argument = LowerValueForType(node.kids[index], payload.type);
        if (payload.type->kind == MType::Struct && AggregateNeedsDeepCopy(payload.type) && IsFreshAggregate(node.kids[index]))
        {
            const Memory::String temp = AllocateName("__magel_agg_arg");
            pre.push_back(simple_ast::Node::declare(temp, LowerType(payload.type, temp)));
            pre.push_back(simple_ast::Node::assign(temp, argument));
            for (const simple_ast::NodePtr &statement :
                 MakeAggregateCleanup(payload.type, simple_ast::Node::var(temp)))
                post.push_back(statement);
            argument = simple_ast::Node::var(temp);
        }
        else if ((payload.type->kind == MType::String || IsObjectLike(payload.type)) && IsOwnedKind(payload.type) && !IsRuntimeArray(payload.type) && !IsResourceLike(payload.type) && IsFreshValue(node.kids[index]) && !(payload.type->kind == MType::String && N(node.kids[index]).kind == MNode::Str))
        {
            /*
             * 构造器会把字符串 / 对象**深拷贝**进聚合，所以传进去的那份"当场造出来的"
             * 临时值构造完就没人要了；不释放它就漏一块（`Ok(f())` / `Ok(拼接)` 都在这）。
             * 字面量是数据区地址、不是堆值，跳过；运行时数组 / 资源句柄是借用，也跳过。
             */
            const Memory::String temp = AllocateName("__magel_ctor_arg");
            pre.push_back(simple_ast::Node::declare(temp, LowerType(payload.type, temp)));
            pre.push_back(simple_ast::Node::assign(temp, argument));
            post.push_back(MakeDrop(temp, false));
            argument = simple_ast::Node::var(temp);
        }
        arguments.push_back(argument);
    }
    const simple_ast::NodePtr call = simple_ast::Node::call(
        "__magel_enum_" + node.s + "_" + node.s2, parameterNames, arguments);
    if (post.empty()) return call;
    const Memory::String result = AllocateName("__magel_agg_val");
    pre.push_back(simple_ast::Node::declare(result, LowerType(NamedType(node.s), result)));
    pre.push_back(simple_ast::Node::assign(result, call));
    for (const simple_ast::NodePtr &statement : post) pre.push_back(statement);
    return simple_ast::Node::sequence(pre, simple_ast::Node::var(result));
}

/**
 * @brief 取得枚举变体载荷对应的值表达式。
 * @param value 待转换、保存或参与计算的值。
 * @param enumName 枚举类型的名称。
 * @param payload 枚举载荷或协议消息正文。
 * @retval 生成的低层语法节点。
 */
simple_ast::NodePtr EnumPayloadValue(const simple_ast::NodePtr &value,
                                     const Memory::String &enumName,
                                     const MEnumPayload &payload)
{
    const MField *field = FindField(enumName, payload.storageName);
    if (field == nullptr)
    {
        Fail("enum " + enumName + " 缺少载荷槽 " + payload.storageName);
        return Bad();
    }
    const simple_ast::NodePtr address = simple_ast::Node::offset(
        value, simple_ast::Node::i(field->slot), 8);
    return payload.type->kind == MType::Struct ? address : simple_ast::Node::loadFrom(address, LowerType(payload.type, payload.storageName));
}

/**
 * @brief 按显式协议降低自定义传播，调用和载荷均只求值一次。
 * @param id 传播节点。
 * @param info 已验证的协议。
 * @return 成功载荷值。
 */
simple_ast::NodePtr LowerProtocolTry(int id, const MPropagationInfo &info)
{
    auto sink = g_freshSink;
    const int valueId = N(id).kids[0];
    const auto sourceType = ExprType(valueId);
    const auto source = AllocateName("__magel_try_source");
    const auto sourceValue = LowerExpr(valueId);
    g_freshSink = sink;
    sink->push_back(simple_ast::Node::declare(source, LowerType(sourceType, source)));
    if (IsFreshAggregate(valueId))
    {
        sink->push_back(DefaultInit(source, sourceType));
        sink->push_back(StoreStructValue(simple_ast::Node::var(source), valueId, sourceValue, sourceType));
        MarkDestructor(source, sourceType);
    }
    else
        sink->push_back(simple_ast::Node::assign(source, sourceValue));
    const auto branch = AllocateName("__magel_try_branch");
    sink->push_back(simple_ast::Node::declare(branch, LowerType(info.branchType, branch)));
    sink->push_back(DefaultInit(branch, info.branchType));
    sink->push_back(CopyStructValue(simple_ast::Node::var(branch),
                                    simple_ast::Node::call(sourceType->name + "." + info.branch.name, { "self" }, { simple_ast::Node::var(source) }), info.branchType));
    MarkDestructor(branch, info.branchType);
    const auto yes = FindEnumVariant(info.branchType->name, "Continue");
    const auto no = FindEnumVariant(info.branchType->name, "Break");
    const auto tagField = FindField(info.branchType->name, "__tag");
    const auto tag = simple_ast::Node::loadFrom(simple_ast::Node::offset(simple_ast::Node::var(branch),
                                                                         simple_ast::Node::i(tagField->slot), 8),
                                                LowerType(T(M_TypeIntBits(32, 0)), branch + ".tag"));
    const auto returned = AllocateName("__magel_try_return");
    Memory::Vector<simple_ast::NodePtr> failure;
    failure.push_back(simple_ast::Node::declare(returned, LowerType(g_returnType, returned)));
    failure.push_back(DefaultInit(returned, g_returnType, false, false));
    const auto residual = EnumPayloadValue(simple_ast::Node::var(branch), info.branchType->name, no->payloads[0]);
    failure.push_back(CopyStructValue(simple_ast::Node::var(returned), simple_ast::Node::call(g_returnType->name + "." + info.receive.name, { info.receive.params[0].emitted }, { residual }), g_returnType));
    AppendDropsAll(Memory::String(), &failure);
    failure.push_back(simple_ast::Node::ret(simple_ast::Node::var(returned)));
    sink->push_back(simple_ast::Node::ifThenElse(simple_ast::Node::binary("==", tag, simple_ast::Node::i(no->tag)),
                                                 simple_ast::Node::block(failure), nullptr));
    auto output = EnumPayloadValue(simple_ast::Node::var(branch), info.branchType->name, yes->payloads[0]);
    if (info.output->kind == MType::Struct)
    {
        const auto owned = AllocateName("__magel_try_owned_value");
        sink->push_back(simple_ast::Node::declare(owned, LowerType(info.output, owned)));
        sink->push_back(DefaultInit(owned, info.output));
        sink->push_back(MoveStructValue(simple_ast::Node::var(owned), output, info.output));
        MarkDestructor(owned, info.output);
        NoteExpressionTemp(owned);
        return simple_ast::Node::var(owned);
    }
    if (info.output->kind == MType::String || IsObjectLike(info.output) || IsResourceLike(info.output))
    {
        const auto owned = AllocateName("__magel_try_owned_value");
        sink->push_back(simple_ast::Node::declare(owned, LowerType(info.output, owned)));
        sink->push_back(simple_ast::Node::assign(owned, output));
        const auto field = FindField(info.branchType->name, yes->payloads[0].storageName);
        sink->push_back(simple_ast::Node::storeAt(simple_ast::Node::offset(simple_ast::Node::var(branch), simple_ast::Node::i(field->slot), 8),
                                                  simple_ast::Node::i(0), LowerType(info.output, owned)));
        MarkOwned(owned);
        NoteExpressionTemp(owned);
        return simple_ast::Node::var(owned);
    }
    return output;
}

/**
 * @brief 将错误传播表达式降级为分支与提前返回。
 * @param id 待查找或操作的对象编号。
 * @retval 生成的低层语法节点。
 */
simple_ast::NodePtr LowerTry(int id)
{
    if (g_freshSink == nullptr)
    {
        Fail("`?` 需要出现在可前置语句的表达式中（声明、赋值、return 或表达式语句）");
        return Bad();
    }
    const int valueId = N(id).kids[0];
    MPropagationInfo info;
    if (!ResolvePropagation(valueId, true, &info)) return Bad();
    const std::shared_ptr<MType> sourceType = ExprType(valueId);
    Memory::Set<Memory::String> returnedBorrow, residualBorrow;
    if (ContainsBorrowedReferences(g_returnType, &returnedBorrow) && ContainsBorrowedReferences(info.residual, &residualBorrow))
    {
        const bool standard = StandardGeneric(sourceType->name, "Result") || StandardGeneric(sourceType->name, "Option");
        Memory::Set<Memory::String> references;
        const bool storageBorrow = !standard && PropagationBorrowsStorage(info.residual, references);
        auto depth = BorrowSourceDepth(valueId, storageBorrow);
        if (storageBorrow && IsFreshAggregate(valueId)) depth = g_scope.size();
        g_borrowFacts.push_back({ magel_borrow_checker::Fact::Return, depth, 0u, g_reductionLine, "（传播失败载荷不能借用已退出函数的局部存储）" });
    }
    if (info.moving || !((StandardGeneric(sourceType->name, "Result") && StandardGeneric(g_returnType->name, "Result")) || (StandardGeneric(sourceType->name, "Option") && StandardGeneric(g_returnType->name, "Option"))))
        return LowerProtocolTry(id, info);
    const MEnumVariantDef *sourceOk = nullptr;
    const MEnumVariantDef *sourceErr = nullptr;
    /*
     * 两种形状都收：
     *   * `Result<T,E>` —— `Err` 带载荷，传播时要保证**错误类型**一致；
     *   * **可空枚举**（`Option<T>` 这样"一个带载荷变体 + 一个空变体"）—— 空变体
     *     没有载荷，传播时原样返回同名空变体（函数返回类型也得是同样的可空枚举）。
     * 两种形状的"好消息变体"都放在 sourceOk 里。
     */
    const bool isResult = ResultVariants(sourceType, &sourceOk, &sourceErr);
    const bool isNullable = !isResult && OptionVariants(sourceType, &sourceOk, &sourceErr);
    if (!isResult && !isNullable)
    {
        Fail("`?` 只能用于 Result<T,E> 或可空枚举（Option<T> 这类），现在给的是 " + Describe(sourceType));
        return Bad();
    }
    const MEnumVariantDef *returnErr = nullptr;
    if (isResult && !ResultVariants(g_returnType, nullptr, &returnErr))
    {
        Fail("使用 `?` 的函数也必须返回 Result<T,E>，现在返回 " + Describe(g_returnType));
        return Bad();
    }
    if (isNullable && !OptionVariants(g_returnType, nullptr, &returnErr))
    {
        Fail("使用 `?` 的函数也必须返回同样的可空枚举，现在返回 " + Describe(g_returnType));
        return Bad();
    }
    if (isResult)
    {
        const std::shared_ptr<MType> sourceErrorType = sourceErr->payloads[0].type;
        const std::shared_ptr<MType> returnErrorType = returnErr->payloads[0].type;
        if (TypeKey(sourceErrorType) != TypeKey(returnErrorType))
        {
            Fail("`?` 传播的错误类型不一致：" + Describe(sourceErrorType) + " -> " + Describe(returnErrorType));
            return Bad();
        }
    }

    Memory::Vector<simple_ast::NodePtr> *const sink = g_freshSink;
    const simple_ast::NodePtr sourceValue = LowerExpr(valueId);
    g_freshSink = sink;
    const Memory::String source = AllocateName("__magel_try_result");
    sink->push_back(simple_ast::Node::declare(source, LowerType(sourceType, source)));
    /*
     * 源聚合是当场造出来的（`f()?`）：先把它搬进 `source` **自己的存储**再登记清理，
     * 不能只 `source = f()` 抄个句柄 —— 枚举构造器返回的是**每个构造器一份的静态对象**，
     * 紧接着再构造同类型枚举就会盖掉它的槽；`source` 有自己的槽后，释放的是它拷走的
     * 那批叶子，与之后的构造互不影响。`x?` 里 x 归声明它的块管，只抄句柄、不释放。
     */
    if (IsFreshAggregate(valueId) && sourceType->kind == MType::Struct)
    {
        if (const simple_ast::NodePtr init = DefaultInit(source, sourceType))
            sink->push_back(init);
        sink->push_back(CopyStructValue(simple_ast::Node::var(source), sourceValue, sourceType));
        MarkDestructor(source, sourceType);
    }
    else
    {
        sink->push_back(simple_ast::Node::assign(source, sourceValue));
    }

    const MField *tagField = FindField(sourceType->name, "__tag");
    if (tagField == nullptr) return Fail("Result 缺少标签槽"), Bad();
    const simple_ast::NodePtr tag = simple_ast::Node::loadFrom(
        simple_ast::Node::offset(simple_ast::Node::var(source),
                                 simple_ast::Node::i(tagField->slot), 8),
        LowerType(T(M_TypeIntBits(32, 0)), sourceType->name + ".tag"));
    const simple_ast::NodePtr isError = simple_ast::Node::binary(
        "==", tag, simple_ast::Node::i(sourceErr->tag));
    const Memory::String returned = AllocateName("__magel_try_error");
    Memory::Vector<simple_ast::NodePtr> propagate;
    propagate.push_back(simple_ast::Node::declare(returned,
                                                  LowerType(g_returnType, returned)));
    if (isNullable)
    {
        /* 空变体没有载荷：构造返回类型里**同名**的那个空变体，不传实参。 */
        propagate.push_back(simple_ast::Node::assign(
            returned,
            simple_ast::Node::call(
                "__magel_enum_" + g_returnType->name + "_" + returnErr->name, {}, {})));
    }
    else
    {
        const simple_ast::NodePtr errorValue = EnumPayloadValue(
            simple_ast::Node::var(source), sourceType->name, sourceErr->payloads[0]);
        propagate.push_back(simple_ast::Node::assign(
            returned, simple_ast::Node::call("__magel_enum_" + g_returnType->name + "_Err",
                                             { EnumPayloadParameterName(g_returnType->name, "Err", 0u) },
                                             { errorValue })));
    }
    AppendDropsAll(Memory::String(), &propagate);
    propagate.push_back(simple_ast::Node::ret(simple_ast::Node::var(returned)));
    sink->push_back(simple_ast::Node::ifThenElse(
        isError, simple_ast::Node::block(propagate), nullptr));
    const MEnumPayload &okPayload = sourceOk->payloads[0];
    simple_ast::NodePtr okValue = EnumPayloadValue(
        simple_ast::Node::var(source), sourceType->name, okPayload);
    /* 载荷若是源聚合占有的字符串 / 对象，复制一份给调用方 —— 源聚合随后就要释放。 */
    if (okPayload.type->kind == MType::String)
        okValue = simple_ast::Node::call(
            "__vm_str_sub", { "__magel_s", "__magel_a", "__magel_b" },
            { okValue, simple_ast::Node::i(0), simple_ast::Node::i(-1) });
    else if (IsObjectLike(okPayload.type))
        okValue = simple_ast::Node::call(ObjectCopyFunction(okPayload.type), { "__magel_v" }, { okValue });
    return okValue;
}

/**
 * @brief 值上下文自动解引用，结构体的地址本身就是其值表示。
 */
simple_ast::NodePtr LowerExpr(int id)
{
    RecordExpressionEffects(id);
    const auto type = RawExprType(id);
    CaptureBorrowExpression(id, type);
    const auto value = LowerRawExpr(id);
    if (AutoDereferences(id) && type && type->kind == MType::Ptr && type->elem && type->elem->kind != MType::Void && type->elem->kind != MType::Struct && !IsInterfacePointer(type))
        return type->elem->kind == MType::Char ? simple_ast::Node::call("__vm_char_load", { "__magel_address" }, { value }) : simple_ast::Node::loadFrom(value, LowerType(type->elem, "pointer value"));
    return value;
}
/**
 * @brief 生成表达式存储值，非空验证由统一入口执行。
 */
simple_ast::NodePtr LowerUncheckedExpr(int id);

/**
 * @brief 引用读取只求值一次；拒绝未绑定字段或宿主产生的零地址。
 */
simple_ast::NodePtr LowerRawExprImpl(int id);
/**
 * @brief 给降级节点附加原始来源。
 * @param value 降级节点。
 * @param id 原语法节点。
 * @return 带位置的节点。
 */
simple_ast::NodePtr LocateNode(simple_ast::NodePtr value, int id)
{
    if (value && id >= 0 && value->sourceLine == 0)
    {
        const auto line = N(id).sourceLine;
        if (line > 0)
        {
            value->sourceFile = line <= g_sourceFiles.size() && !g_sourceFiles[line - 1].empty() ? g_sourceFiles[line - 1] : g_sourceFallback;
            value->sourceLine = line <= g_sourceLines.size() ? g_sourceLines[line - 1] : line;
        }
    }
    return value;
}
/* `ReductionSourceScope` 已移到 `type_ids_shared.cpp`（那里排在 `type_ids.cpp` 之后、
 * `N()` 之后），登记遍与降级遍都要用它；这里只剩调用。 */
/**
 * @brief 将表达式降级为原始值并维护检查状态。
 * @param id 待查找或操作的对象编号。
 * @retval 生成的低层语法节点。
 */
simple_ast::NodePtr LowerRawExpr(int id)
{
    ReductionSourceScope source(id);
    ResolveBufferIndexShadowing(id);
    RecordExpressionEffects(id);
    return LocateNode(LowerRawExprImpl(id), id);
}
/**
 * @brief 递归生成表达式对应的原始值。
 * @param id 待查找或操作的对象编号。
 * @retval 生成的低层语法节点。
 */
simple_ast::NodePtr LowerRawExprImpl(int id)
{
    const auto value = LowerUncheckedExpr(id);
    const auto type = id >= 0 ? RawExprType(id) : nullptr;
    CaptureBorrowExpression(id, type);
    // 补全原生容器读取等节点的数值类型，保留变量、下标和成员的左值形态。
    if (value && type && (type->kind == MType::Int || type->kind == MType::Double))
        value->declared = LowerType(type, "");
    if (type && type->kind == MType::Ptr)
        return simple_ast::Node::cast(
            simple_ast::Node::call("__vm_reference", { "__magel_reference" }, { value }),
            LowerType(RawExprType(id), "reference"));
    return value;
}

/**
 * @brief 在关闭相应检查的上下文中降级表达式。
 * @param id 待查找或操作的对象编号。
 * @retval 生成的低层语法节点。
 */
simple_ast::NodePtr LowerUncheckedExpr(int id)
{
    if (id < 0) return Bad();
    InferExpression(id);
    const MNode &node = N(id);
    switch (node.kind)
    {
    case MNode::CollectionLiteral:
    {
        using simple_ast::Node;
        const auto type = ExprType(id);
        if (!IsContainer(type) || !CheckDeclaredType(type, "集合字面量")) return Bad();
        const auto name = "__magel_collection_" + Memory::toString(id) + "_" + Memory::toString(TypeIdOf(type));
        g_collectionConstructors[name] = MCollectionConstructor{ type, node.kids.size() };
        Memory::Vector<Memory::String> names;
        Memory::Vector<simple_ast::NodePtr> values, prefix, post;
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
        for (std::size_t i = 0; i < node.kids.size(); ++i)
        {
            const auto element = type->name == "map" && i % 2u ? type->elem2 : type->elem;
            if (!CheckContainerElement(element, "字面量元素/键值") || (element->kind != MType::Struct && !HasImplicitConversion(ExprType(node.kids[i]), element) && !CheckStoredAddress(node.kids[i], "字面量元素/键值"))) return Bad();
            const auto captured = AllocateName("__magel_literal_item");
            prefix.push_back(Node::declare(captured, LowerType(element, captured)));
            if (element->kind == MType::Struct)
            {
                // 先冻结聚合内容，避免下一次同类型构造复用返回存储覆盖前一项。
                const auto source = AllocateName("__magel_literal_source");
                prefix.push_back(Node::declare(source, simple_ast::Type::i64()));
                const auto value = LowerValueForType(node.kids[i], element);
                prefix.push_back(Node::assign(source, value));
                prefix.push_back(DefaultInit(captured, element, false));
                prefix.push_back(DeepCopyStructValue(Node::var(captured), Node::var(source), element));
                if (IsOwnedAggregateTemporary(node.kids[i]) || IsImplicitConversionValue(node.kids[i], element))
                    if (const auto release = MakeAggregateRelease(element->name, Node::var(source))) prefix.push_back(release);
                if (const auto release = MakeAggregateRelease(element->name, Node::var(captured))) post.push_back(release);
            }
            else
            {
                const auto value = IsObjectLike(element) ? MaterializeObjectValue(node.kids[i], element) : MaterializeStringValue(node.kids[i], element);
                prefix.push_back(Node::assign(captured, value));
                if (IsOwnedKind(element)) AppendOwnedDropByName(captured, false, &post);
            }
            names.push_back(name + "_arg_" + Memory::toString(i));
            values.push_back(Node::var(captured));
        }
        const auto result = AllocateName("__magel_literal_result");
        prefix.push_back(Node::declare(result, LowerType(type, result)));
        prefix.push_back(Node::assign(result, Node::call(name, names, values)));
        prefix.insert(prefix.end(), post.begin(), post.end());
        return Node::sequence(std::move(prefix), Node::var(result));
    }
    case MNode::Range: return LowerInterval(id);
    case MNode::Tuple:
    {
        const std::shared_ptr<MType> type = ExprType(id);
        EnsureTupleLayout(type);
        const Memory::Vector<std::shared_ptr<MType>> &elements = g_tupleElements[type->name];
        Memory::Vector<Memory::String> names;
        Memory::Vector<simple_ast::NodePtr> values;
        for (std::size_t index = 0; index < node.kids.size(); ++index)
        {
            names.push_back("__magel_tuple_arg_" + Memory::toString(index));
            values.push_back(LowerArgumentValue(node.kids[index], elements[index]));
        }
        return simple_ast::Node::call(TupleConstructorName(type->name), names, values);
    }
    case MNode::Int: return simple_ast::Node::cast(simple_ast::Node::i(node.i), LowerType(ExprType(id), ""));
    case MNode::Bool: return simple_ast::Node::boolean(node.i != 0);
    case MNode::Null: return simple_ast::Node::null();
    case MNode::Float: return simple_ast::Node::cast(simple_ast::Node::f(node.d), LowerType(ExprType(id), ""));
    case MNode::Str:
        /* 值是数据区里那串字节的基址；转义（`\n` / `\t` …）和格式串同一套。 */
        return simple_ast::Node::str(UnescapeText(node.s));
    case MNode::Char:
    {
        /* `'A'`：剥掉两侧引号再走同一套转义，值就是字符码（`int64`）。 */
        const Memory::String inner =
            (node.s.size() >= 2u) ? node.s.substr(1u, node.s.size() - 2u) : Memory::String();
        const Memory::String text = UnescapeText(inner);
        if (text.size() != 1u)
        {
            Fail("char 只能保存一个 UTF-8 字节；完整 Unicode 字符请使用 std::lang::String：'" + inner + "'");
            return Bad();
        }
        return simple_ast::Node::i(static_cast<long long>(static_cast<unsigned char>(text[0])));
    }
    case MNode::EnumCtor:
        if (const MVar *global = FindQualifiedGlobal(node.s, node.s2); global != nullptr)
            return global->elementReference ? simple_ast::Node::loadFrom(simple_ast::Node::var(global->emitted), LowerType(global->type, global->emitted)) : simple_ast::Node::var(global->emitted);
        if (node.type2 == 1 && FindEnumVariant(node.s, node.s2) == nullptr)
        {
            if (const auto owner = FindStruct(node.s))
                for (const auto &method : owner->methods)
                    if (!method.superseded && method.isStatic && method.source == node.s2)
                        return LowerStaticQualified(id);
        }
        if ((node.type2 != 1 && FindStaticField(node.s, node.s2) != nullptr) || (FindStruct(node.s) != nullptr && g_enums.count(node.s) == 0u))
            return LowerStaticQualified(id);
        return LowerEnumCtor(id);
    case MNode::Loop:
    {
        Memory::Vector<simple_ast::NodePtr> *const sink = g_freshSink;
        if (sink == nullptr)
        {
            Fail("有返回值的 loop 需要出现在声明、赋值、return 或表达式语句中");
            return Bad();
        }
        Memory::String value;
        const simple_ast::NodePtr loop = LowerLoop(id, &value);
        g_freshSink = sink;
        if (value.empty())
        {
            Fail("这个 loop 没有 `break value;`，不能作为值使用");
            return Bad();
        }
        Memory::Vector<simple_ast::NodePtr> inlineBody;
        if (loop && loop->kind == simple_ast::Node::Kind::Block)
        {
            for (const auto &part : loop->kids)
                if (part && part->kind == simple_ast::Node::Kind::Declare)
                    sink->push_back(part);
                else
                    inlineBody.push_back(part);
        }
        else
            inlineBody.push_back(loop);
        return simple_ast::Node::sequence(std::move(inlineBody), simple_ast::Node::var(value));
    }
    case MNode::Try:
    {
        if (!g_freshSink) return LowerTry(id);
        auto sink = g_freshSink;
        Memory::Vector<simple_ast::NodePtr> fresh, inlineBody;
        simple_ast::NodePtr value;
        {
            MExpressionFreshScope scope(&fresh);
            value = LowerTry(id);
        }
        for (const auto &part : fresh)
            if (part && part->kind == simple_ast::Node::Kind::Declare)
                sink->push_back(part);
            else
                inlineBody.push_back(part);
        return simple_ast::Node::sequence(std::move(inlineBody), value);
    }
    case MNode::Var:
    {
        const MVar *found = FindIn(node.module, node.s);
        if (found != nullptr)
        {
            if (!found->elementList.empty())
                return simple_ast::Node::call("__vm_vec_at", { "__magel_v", "__magel_i", "__magel_t", "__magel_o" },
                                              { simple_ast::Node::var(found->elementList), simple_ast::Node::var(found->elementIndex),
                                                simple_ast::Node::i(found->type->kind == MType::String ? 1 : 0), ContainerStorageFlag(found->type) });
            if (found->elementReference && found->type->kind == MType::Char)
                return simple_ast::Node::call("__vm_char_load", { "__magel_address" }, { simple_ast::Node::var(found->emitted) });
            if (found->elementReference && found->type->kind != MType::Struct)
                return simple_ast::Node::loadFrom(simple_ast::Node::var(found->emitted), LowerType(found->type, found->emitted));
            return simple_ast::Node::var(found->emitted);
        }
        /* 方法体里的保留名：`self` 是接收者，字段名等价于 `self.字段` */
        if (!g_owner.empty() && node.s == "self") return simple_ast::Node::var("self");
        if (const int member = ExplicitReceiverField(id); member >= 0)
            return LowerMember(N(member).kids[0], N(member).s);
        /* 函数名当值用（函数指针）：值 = 数据区里存着函数标签那串字节的地址。 */
        if (FindFunctionOverloads(node.module, node.s) != nullptr)
        {
            const Memory::String label =
                ResolveFunctionValue(node.module, node.s, g_expectedFuncType);
            if (label.empty()) return Bad();
            const auto signature = g_funcs.find(label);
            if (signature != g_funcs.end() && signature->second.staticCallbacks)
                for (const auto &param : signature->second.params)
                    if (param.type && param.type->kind == MType::Func)
                        return Fail("异步事件登记入口的回调生命周期契约不能通过普通函数指针擦除"), Bad();
            using simple_ast::Node;
            const auto key = ModuleSymbol(node.module, node.s);
            const auto captures = g_lambdaCaptures.find(key);
            const bool hasEnvironment = captures != g_lambdaCaptures.end() && !captures->second.empty();
            if (!hasEnvironment) return FunctionPointerValue(label);
            const auto local = AllocateName("__magel_closure_value");
            Memory::Vector<simple_ast::NodePtr> prefix{
                Node::declare(local, simple_ast::Type::i64()),
                Node::assign(local, Node::call("__vm_closure_new", { "__magel_target", "__magel_has_environment" },
                                               { FunctionPointerValue(label), Node::i(hasEnvironment ? 1 : 0) }))
            };
            if (hasEnvironment)
                for (const auto &capture : captures->second)
                {
                    const int source = M_New(MNode::Var, capture.source);
                    g_nodes[source].module = node.module;
                    const auto captureVariable = FindIn(node.module, capture.source);
                    if (g_borrowFlowCapture && captureVariable)
                        g_borrowFlowCapture->lambdaReads[id].push_back({ captureVariable->emitted, {} });
                    const bool owned = capture.captureMode >= 2;
                    const bool moving = capture.captureMode == 3;
                    if (owned && (capture.type->kind == MType::Array || (!moving && !CheckContainerElement(capture.type, "lambda copy 捕获"))))
                        return Fail("lambda 拥有捕获需要具有布局的值：" + capture.source), Bad();
                    auto layout = owned ? ContainerStorageFlag(capture.type, moving) : Node::i(0);
                    if (moving && IsResourceLike(capture.type)) layout = Node::i(1);
                    if (moving)
                    {
                        if (IsReadOnlyAccess(source)) return Fail("lambda move 不能移动只读变量：" + capture.source), Bad();
                        const auto variable = FindIn(node.module, capture.source);
                        if (g_borrowFlowCapture && variable)
                            g_borrowFlowCapture->lambdaMoves[id].push_back({ variable->emitted, {} });
                        if (capture.type->kind == MType::Ptr)
                        {
                            prefix.push_back(Node::exprStmt(Node::call("__vm_closure_capture_copy",
                                                                       { "environment", "value", "text", "layout" },
                                                                       { Node::var(local), LowerRawExpr(source), Node::i(0), Node::i(0) })));
                            prefix.push_back(variable->elementReference ?
                                                 Node::storeAt(Node::var(variable->emitted), Node::i(0), simple_ast::Type::i64()) :
                                                 Node::assign(variable->emitted, Node::i(0)));
                        }
                        else
                        {
                            const auto address = AllocateName("__magel_capture_source");
                            prefix.push_back(Node::declare(address, simple_ast::Type::i64()));
                            prefix.push_back(Node::assign(address, LowerAddressOf(source)));
                            prefix.push_back(Node::exprStmt(Node::call("__vm_closure_capture_move",
                                                                       { "environment", "address", "text", "layout", "float", "bytes" },
                                                                       { Node::var(local), Node::var(address), Node::i(capture.type->kind == MType::String), layout,
                                                                         Node::i(IsFloatType(capture.type)), Node::i(capture.type->kind == MType::Struct ? StructStride(capture.type) : capture.type->kind == MType::Char ? 1 :
                                                                                                                                                                                                                            8) })));
                            if (capture.type->kind == MType::Struct)
                                AppendMovedAggregate(capture.type->name, Node::var(address), &prefix);
                        }
                    }
                    else
                    {
                        prefix.push_back(Node::exprStmt(Node::call(owned ? "__vm_closure_capture_copy" : "__vm_closure_capture",
                                                                   { "environment", "value", "text", "layout" },
                                                                   { Node::var(local), owned ? LowerRawExpr(source) : LowerAddressOf(source),
                                                                     Node::i(owned && capture.type->kind == MType::String), layout })));
                    }
                    if (owned)
                    {
                        const auto index = &capture - captures->second.data();
                        prefix.push_back(Node::exprStmt(Node::call("__vm_closure_ref",
                                                                   { "environment", "index", "text", "layout", "float" },
                                                                   { Node::var(local), Node::i(index), Node::i(capture.type->kind == MType::String), layout,
                                                                     Node::i(IsFloatType(capture.type)) })));
                        if (capture.type->kind == MType::Struct)
                        {
                            g_needsCaptureDestructor.insert(capture.type->name);
                            prefix.push_back(Node::exprStmt(Node::call("__vm_closure_destructor", { "environment", "index", "callback" },
                                                                       { Node::var(local), Node::i(index), FunctionPointerValue("__magel_capture_drop_" + capture.type->name) })));
                        }
                    }
                }
            return OwnedObjectTempNode(Node::sequence(std::move(prefix), Node::var(local)), FuncTypeOf(g_funcs.at(label)));
        }
        Fail("引用了没声明的变量：" + node.s);
        return Bad();
    }
    case MNode::Index:
    {
        const std::shared_ptr<MType> owner = ExprType(node.kids[0]);
        const MIndexDecl *index = node.s == "__magel_buffer_index" ? nullptr : FindIndex(owner, ExprType(node.kids[1]));
        if (index != nullptr)
            return simple_ast::Node::call(
                index->label, index->callNames,
                { LowerExpr(node.kids[0]), LowerValueForType(node.kids[1], index->key) });
        return LowerIndexChain(node.kids[0], node.kids[1], node.s == "__magel_buffer_index");
    }
    case MNode::Slice:
        return LowerSlice(id);
    case MNode::Member:
    case MNode::Arrow:
        return LowerMember(node.kids[0], node.s);
    case MNode::Un:
        return LowerUnary(node.s, node.kids[0]);
    case MNode::Bin:
        return LowerBinary(node.s, node.kids[0], node.kids[1], id);
    case MNode::Cast:
        if (node.type2 == 2)
        {
            const auto expected = T(node.type);
            InferExpression(node.kids[0], expected);
            if (!Acceptable(expected, ExprType(node.kids[0]), true))
                return Fail("宏展开结果类型不匹配：期望 " + Describe(expected) + "，得到 " + Describe(ExprType(node.kids[0]))), Bad();
            return LowerValueForType(node.kids[0], expected);
        }
        return LowerCast(node.type, node.kids[0]);
    case MNode::BigIntCtor:
    {
        /* `bint(值)`：按实参类型挑构造方式（文本 / 整数 / 浮点）。 */
        const int valueId = node.kids.empty() ? -1 : node.kids[0];
        const std::shared_ptr<MType> type = ExprType(valueId);
        if (type && type->kind == MType::BigInt) return LowerExpr(valueId);
        if (type && type->kind == MType::String)
        {
            return simple_ast::Node::call("__vm_bint_from_str", { "__magel_v" },
                                          { LowerExpr(valueId) });
        }
        if (type && type->kind == MType::Double)
        {
            return simple_ast::Node::call("__vm_bint_from_flt", { "__magel_v" },
                                          { LowerExpr(valueId) });
        }
        if (type && (type->kind == MType::Int || type->kind == MType::Bool))
        {
            return simple_ast::Node::call(type->isUnsigned ? "__vm_bint_from_uint" : "__vm_bint_from_int", { "__magel_v" },
                                          { LowerExpr(valueId) });
        }
        Fail("bint 只接受十进制文本 / 整数 / 浮点，现在给的是 " + (type ? Describe(type) : Memory::String("?")));
        return Bad();
    }
    case MNode::BigFloatCtor:
    {
        const int valueId = node.kids.empty() ? -1 : node.kids[0];
        const std::shared_ptr<MType> type = ExprType(valueId);
        if (type && type->kind == MType::BigFloat) return LowerExpr(valueId);
        const char *native = nullptr;
        if (type && type->kind == MType::String)
            native = "__vm_bfloat_from_str";
        else if (type && type->kind == MType::BigInt)
            native = "__vm_bfloat_from_bint";
        else if (type && type->kind == MType::Double)
            native = "__vm_bfloat_from_flt";
        else if (type && (type->kind == MType::Int || type->kind == MType::Bool))
            native = type->isUnsigned ? "__vm_bfloat_from_uint" : "__vm_bfloat_from_int";
        if (native != nullptr) return simple_ast::Node::call(native, { "__magel_v" }, { LowerExpr(valueId) });
        Fail("BigDecimal 只接受十进制文本 / 整数 / 浮点 / BigInteger");
        return Bad();
    }
    case MNode::NewPtr:
        return LowerNewPtr(id);
    case MNode::TypeTag:
        return LowerTypeTag(id);
    case MNode::Call:
        return LowerCall(id);
    case MNode::Update:
        return LowerUpdate(id);
    case MNode::Assign:
    {
        // 赋值用作值时先保存右值，再写目标，避免整段赋值被当表达式忽略。
        // 返回保存值而非重新读取目标，保证有副作用的目标不会额外求值。
        int target = node.kids[0], right = node.kids[1];
        Memory::Vector<simple_ast::NodePtr> prefix;
        if (node.type2 == 1)
        {
            target = FreezeCompoundTarget(target, prefix);
            if (target < 0) return Bad();
            const auto operation = N(right);
            right = M_New(MNode::Bin);
            g_nodes[right] = operation;
            g_nodes[right].checkedExpression = false;
            g_nodes[right].kids[0] = target;
        }
        const auto name = AllocateName("__magel_assignment_result");
        const int result = M_Var(M_Name(name.c_str()));
        const int declaration = M_Decl(M_Name(name.c_str()), -1, right);
        const int resultType = M_AddType(ExprType(id));
        g_nodes[declaration].type = resultType;
        const int group = M_LocalDecl(resultType, M_List1(declaration), 0);
        const auto initialize = LowerLocalDecl(group);
        const auto assignment = LowerAssign(target, result);
        const auto variable = Find(name);
        if (variable)
        {
            // 短路分支可能不执行初始化；清理槽必须提前存在且为空。
            if (g_freshSink && (IsOwnedKind(variable->type) || variable->type->kind == MType::Struct))
            {
                g_freshSink->push_back(simple_ast::Node::declare(variable->emitted, LowerType(variable->type, variable->emitted)));
                g_freshSink->push_back(simple_ast::Node::assign(variable->emitted, simple_ast::Node::i(0)));
            }
            NoteExpressionTemp(variable->emitted);
        }
        prefix.push_back(initialize);
        prefix.push_back(assignment);
        return simple_ast::Node::sequence(std::move(prefix), LowerExpr(result));
    }
    case MNode::ParseBad:
        /* 解析期挂上来的说明：原样报出来。 */
        Fail(node.s);
        return Bad();
    default:
        Fail("这个表达式还不支持降级（节点种类 " + Memory::toString(static_cast<int>(node.kind)) + (node.s.empty() ? Memory::String() : "：" + node.s) + "）");
        return Bad();
    }
}

/**
 * @brief `return 表达式;` 里"被交出去的那个变量"（没有就返回空串）。
 *
 * 只认光秃秃的变量（外面套几层强转也算）：`return s;` / `return (string)s;`。
 * 这种情况下这个变量**不参与**本块的自动释放 —— 它的所有权交给调用方
 * （`string t = f();` 里 t 就是那块串的新主人）。
 * `return s[0:2];` 是**新造**出来的串，跟 `s` 那块没关系，所以 `s` 照常释放。
 */
Memory::String ReturnedVariableName(int returnId)
{
    if (returnId < 0) return Memory::String();
    const MNode &node = N(returnId);
    if (node.kind != MNode::Return || node.kids.empty()) return Memory::String();
    int exprId = node.kids[0];
    for (int guard = 0; guard < 8 && exprId >= 0; ++guard)
    {
        const MNode &expr = N(exprId);
        if (expr.kind != MNode::Cast) break;
        exprId = expr.kids.empty() ? -1 : expr.kids[0];
    }
    if (exprId < 0 || N(exprId).kind != MNode::Var) return Memory::String();
    const MVar *found = FindIn(N(exprId).module, N(exprId).s);
    return (found == nullptr) ? Memory::String() : found->emitted;
}

/**
 * @brief `return` 里"所有权直接交给调用方、本函数不能再释放"的变量。
 *
 * 两类：
 *   * `return x;`（x 是直接变量）—— 按 **move** 交出去，x 不参与本函数收尾；
 *   * `return Result::Err(arr);` 里**运行时数组**载荷 —— 聚合只借用数组，所有权
 *     还是被 `return` 出去的那个变量（字符串 / 对象载荷不在此列：构造器会各复制一份，
 *     原变量照常释放）。
 *
 * 枚举构造器里的字符串 / 对象实参**不再**保留：构造器已经深拷贝进聚合，
 * 原变量归本函数的块所有、正常释放。
 */
Memory::Set<Memory::String> ReturnedOwnedNames(int returnId)
{
    Memory::Set<Memory::String> names;
    const Memory::String direct = ReturnedVariableName(returnId);
    if (!direct.empty())
    {
        int exprId = N(returnId).kids[0];
        for (int guard = 0; guard < 8 && N(exprId).kind == MNode::Cast; ++guard)
            exprId = N(exprId).kids[0];
        const MVar *variable = FindIn(N(exprId).module, N(exprId).s);
        if (variable != nullptr && !variable->elementReference && (SameType(variable->type, g_returnType) || (IsRuntimeArray(variable->type) && g_returnType && g_returnType->kind == MType::Ptr)) && (IsOwnedNow(variable->emitted) || HasDestructorNow(variable->emitted)) && (IsOwnedKind(variable->type) || (variable->type && variable->type->kind == MType::Struct)))
            names.insert(variable->emitted);
    }
    if (returnId < 0 || N(returnId).kind != MNode::Return || N(returnId).kids.empty())
        return names;
    const MNode &expression = N(N(returnId).kids[0]);
    const Memory::Vector<int> *arguments = nullptr;
    if (expression.kind == MNode::EnumCtor && FindEnumVariant(expression.s, expression.s2))
        arguments = &expression.kids;
    else if (expression.kind == MNode::Call && expression.kids.size() >= 2u && N(expression.kids[0]).kind == MNode::EnumCtor && FindEnumVariant(N(expression.kids[0]).s, N(expression.kids[0]).s2))
        arguments = &N(expression.kids[1]).kids;
    if (arguments == nullptr) return names;
    for (int argumentId : *arguments)
    {
        for (int guard = 0; guard < 8 && argumentId >= 0 && N(argumentId).kind == MNode::Cast; ++guard)
            argumentId = N(argumentId).kids.empty() ? -1 : N(argumentId).kids[0];
        if (argumentId < 0 || N(argumentId).kind != MNode::Var) continue;
        const MVar *variable = FindIn(N(argumentId).module, N(argumentId).s);
        /*
         * 字符串 / 对象载荷构造器已经深拷贝，原变量照常释放；但聚合**借用**的那些
         * （运行时数组、`file` / `proc` 资源句柄、带 Drop / Closeable 的值结构体）
         * 还得把所有权带出去 —— 否则源变量一释放，聚合里的槽就悬了。
         */
        if (variable != nullptr && variable->type && !variable->elementReference && (IsOwnedNow(variable->emitted) || HasDestructorNow(variable->emitted)) && (IsRuntimeArray(variable->type) || IsResourceLike(variable->type) || (variable->type->kind == MType::Struct && (StructHasInterface(variable->type->name, "Drop") || StructHasInterface(variable->type->name, "Closeable")))))
            names.insert(variable->emitted);
    }
    return names;
}

/**
 * @brief 把"没人接的当场造出来的返回值"落进临时变量并释放掉。
 *
 * 覆盖三种独立表达式语句：
 *   * `f();` / `v.copy();` —— 返回堆上的字符串 / 运行时数组 / 对象句柄：软释放；
 *   * 返回实现 `Drop` / `Closeable` 的结构体：跑一次它的析构；
 *   * 其它（标量 / 借来的值）：原样写出来。
 *
 * 只在 `MNode::ExprStmt` 里对 `IsFreshValue` 的表达式调用，所以不会误回收借来的值。
 */
simple_ast::NodePtr DiscardFreshValue(int valueId)
{
    const std::shared_ptr<MType> type = ExprType(valueId);
    if (type && IsOwnedKind(type))
    {
        const Memory::String temp = AllocateName("__magel_discard_o");
        Memory::Vector<simple_ast::NodePtr> out;
        out.push_back(simple_ast::Node::declare(temp, LowerType(type, temp)));
        out.push_back(simple_ast::Node::assign(
            temp, IsObjectLike(type) ? MaterializeObjectValue(valueId, type) : LowerValueForType(valueId, type)));
        /* 丢掉的是"当场造出来的"值：数组的**元素**也是值，得先按元素收尾。 */
        AppendOwnedDrop(temp, type, &out);
        return simple_ast::Node::block(out);
    }
    if (type && type->kind == MType::Struct && StructNeedsCleanup(type->name))
    {
        const Memory::String temp = AllocateName("__magel_discard_s");
        Memory::Vector<simple_ast::NodePtr> out;
        out.push_back(simple_ast::Node::declare(temp, LowerType(type, temp)));
        if (const simple_ast::NodePtr init = DefaultInit(temp, type)) out.push_back(init);
        out.push_back(CopyStructValue(simple_ast::Node::var(temp), LowerExpr(valueId), type));
        const MOwnedScope::Destructor item{ temp, type->name,
                                            StructHasInterface(type->name, "Drop"),
                                            StructHasInterface(type->name, "Closeable"),
                                            StructHasOwnedLeaves(type->name) };
        out.push_back(MakeDestructorCall(item));
        return simple_ast::Node::block(out);
    }
    /*
     * 显式 `x.drop()` 会降级成**一整块**（先补 `close()` 再 `drop()`），已经是语句，
     * 不能再套一层 `ExprStmt`（那样整块会被当成"表达式"丢掉）。普通调用是单个
     * `Call` 节点，才需要包成语句。
     */
    const simple_ast::NodePtr value = LowerExpr(valueId);
    if (value == nullptr) return nullptr;
    if (value->kind == simple_ast::Node::Kind::Block) return value;
    return simple_ast::Node::exprStmt(value);
}

/**
 * @brief 为退出时结束生命周期的可写局部变量选择移动，外部变量及清理所需来源保留复制语义。
 * @param value 原始退出值节点编号。
 * @param scopeDepth 结果接收者外侧的名字作用域深度。
 * @param returnExpression 是否要求返回表达式内部拥有者在清理前移动。
 * @retval 适配后的退出值节点编号，元组按元素递归处理。
 */
int PrepareBreakValue(int value, std::size_t scopeDepth, bool returnExpression = false)
{
    if (value < 0) return value;
    const auto expression = N(value);
    if (expression.kind == MNode::Tuple)
    {
        Memory::Vector<int> items;
        for (const int child : expression.kids) items.push_back(PrepareBreakValue(child, scopeDepth, returnExpression));
        g_nodes[value].kids = std::move(items);
        return value;
    }
    if (expression.kind != MNode::Var) return value;
    const auto variable = FindIn(expression.module, expression.s);
    if (!variable || (variable->isConst && !returnExpression) || !variable->type || variable->type->kind == MType::Ptr || (variable->type->kind == MType::Array && !(returnExpression && IsRuntimeArray(variable->type))) || variable->type->kind == MType::Host)
        return value;
    if (variable->elementReference && std::none_of(g_referenceStorage.begin(), g_referenceStorage.end(), [&](const auto &entry) { return entry.second == variable->emitted; }))
        return value;
    // 清理回调可能通过别名访问来源；保留独立结果，随后按原顺序执行回调。
    for (const auto &scope : g_ownedScopes)
        if (!returnExpression && !scope.finalizers.empty()) return value;
    for (std::size_t index = g_scope.size(); index > scopeDepth + 1u; --index)
        for (const auto &entry : g_scope[index - 1u])
            if (&entry.second == variable)
            {
                if (returnExpression)
                    for (const auto &scope : g_ownedScopes)
                        if (scope.deferredReads.count(variable->emitted) || scope.deferredBorrowDepth >= index - 1u)
                        {
                            Fail("[MGL1205] return 表达式的延迟清理仍借用即将移动的来源：" + expression.s);
                            return value;
                        }
                const int moved = M_BreakValue(value);
                g_nodes[moved].implicitExitMove = returnExpression;
                return moved;
            }
    return value;
}

simple_ast::NodePtr LowerStmtImpl(int id);
/**
 * @brief 降级单条语句并维护诊断上下文。
 * @param id 待查找或操作的对象编号。
 * @retval 生成的低层语法节点。
 */
simple_ast::NodePtr LowerStmt(int id)
{
    ClearActiveError();
    ReductionSourceScope source(id);
    RecordExpressionEffects(id);
    MFullExpressionTemps temporaries;
    Memory::Vector<simple_ast::NodePtr> fresh;
    MExpressionFreshScope sink(&fresh);
    const auto statement = LowerStmtImpl(id);
    auto cleanup = temporaries.finish();
    if (cleanup.empty() && fresh.empty()) return LocateNode(statement, id);
    if (statement) fresh.push_back(statement);
    fresh.insert(fresh.end(), cleanup.begin(), cleanup.end());
    return LocateNode(simple_ast::Node::block(fresh), id);
}
/**
 * @brief 生成单条语句对应的低层表示。
 * @param id 待查找或操作的对象编号。
 * @retval 生成的低层语法节点。
 */
simple_ast::NodePtr LowerStmtImpl(int id)
{
    if (id < 0) return nullptr;
    const MNode &node = N(id);
    switch (node.kind)
    {
    case MNode::Defer:
    {
        using simple_ast::Node;
        if (g_ownedScopes.empty()) return Fail("defer 必须位于作用域内"), Bad();
        Memory::String receiverName;
        if (!g_owner.empty())
        {
            receiverName = "__magel_defer_receiver_" + Memory::toString(id);
            const auto receiver = [&]() { const int value = M_Var(M_Name(receiverName.c_str())); g_nodes[value].module = node.module; return value; };
            std::function<void(int, Memory::Set<Memory::String> &)> bind;
            bind = [&](int value, Memory::Set<Memory::String> &locals) {
                if (value < 0) return;
                const auto original = N(value);
                if (original.kind == MNode::Block)
                {
                    auto inner = locals;
                    for (const auto statement : N(static_cast<int>(original.i)).kids) bind(statement, inner);
                    return;
                }
                if (original.kind == MNode::Declare)
                {
                    for (const auto declaration : N(original.kids[0]).kids)
                    {
                        for (const auto initializer : N(declaration).kids) bind(initializer, locals);
                        locals.insert(N(declaration).s);
                    }
                    return;
                }
                if (original.kind == MNode::For || original.kind == MNode::Foreach || original.kind == MNode::Case)
                {
                    auto inner = locals;
                    if (original.kind == MNode::Foreach) inner.insert(original.s);
                    if (original.kind == MNode::Case && original.i >= 0)
                    {
                        auto pattern = N(static_cast<int>(original.i));
                        if (pattern.kind == MNode::Call && pattern.kids.size() == 2u) pattern = N(pattern.kids[1]);
                        for (const auto item : pattern.kids)
                            if (N(item).kind == MNode::Var) inner.insert(N(item).s);
                    }
                    for (const auto child : original.kids) bind(child, inner);
                    if (original.guard >= 0) bind(original.guard, inner);
                    return;
                }
                if (original.kind == MNode::Var && !locals.count(original.s))
                {
                    if (original.s == "self")
                    {
                        g_nodes[value].s = receiverName;
                        return;
                    }
                    if (!FindIn(original.module, original.s) && FieldType(g_owner, original.s))
                    {
                        g_nodes[value].kind = MNode::Member;
                        g_nodes[value].kids = { receiver() };
                        return;
                    }
                }
                if (original.kind == MNode::Call && original.kids.size() == 2u)
                {
                    const auto callee = N(original.kids[0]);
                    if (callee.kind == MNode::Var && !locals.count(callee.s) && !FindIn(callee.module, callee.s))
                    {
                        const auto definition = FindStruct(g_owner);
                        if (definition)
                            for (const auto &method : definition->methods)
                                if (method.source == callee.s && !method.isStatic)
                                {
                                    g_nodes[original.kids[0]].kind = MNode::Member;
                                    g_nodes[original.kids[0]].kids = { receiver() };
                                    break;
                                }
                    }
                }
                for (const auto child : original.kids) bind(child, locals);
            };
            for (const auto &declaration : g_functionDecls)
                if (declaration.source == N(node.kids[0]).s && declaration.module == node.module)
                {
                    Memory::Set<Memory::String> locals;
                    bind(declaration.bodyId, locals);
                    break;
                }
            g_scope.back()[receiverName] = MVar{ PointerType(NamedType(g_owner), false), "self", false };
        }
        const auto callbackType = ExprType(node.kids[0]);
        const auto callback = TakeExpressionTempValue(LowerExpr(node.kids[0]), callbackType);
        const auto captures = g_lambdaCaptures.find(ModuleSymbol(N(node.kids[0]).module, N(node.kids[0]).s));
        if (captures != g_lambdaCaptures.end())
            for (const auto &capture : captures->second)
            {
                if (capture.captureMode >= 2) continue;
                const int source = M_New(MNode::Var, capture.source);
                g_nodes[source].module = node.module;
                const auto binding = FindIn(node.module, capture.source);
                if (!binding) continue;
                g_ownedScopes.back().deferredReads.insert(binding->emitted);
                const auto place = EffectPlace(source);
                if (!place.root.empty()) g_ownedScopes.back().deferredReads.insert(place.root);
                if (HasBorrowedValue(binding->type))
                    g_ownedScopes.back().deferredBorrowDepth = std::max(g_ownedScopes.back().deferredBorrowDepth, binding->borrowDepth);
            }
        if (!receiverName.empty()) g_scope.back().erase(receiverName);
        const auto frame = AllocateName("__magel_defer_frame");
        g_ownedScopes.back().hasDeferred = true;
        g_ownedScopes.back().finalizers.push_back(Node::exprStmt(Node::call("__magel_defer_pop", { "frame" }, { Node::var(frame) })));
        return Node::block({ Node::declare(frame, simple_ast::Type::i64()),
                             Node::assign(frame, Node::call("__magel_defer_push", { "callback" }, { callback })) });
    }
    case MNode::Block:
    {
        /* 语句块同时是两件事：
         *   1. **一层名字作用域** —— 块里声明的同名变量遮蔽外层的（`{ int x = 2; }`）；
         *   2. **一层生命周期** —— 块里声明出来的所有者变量（运行时数组 / 堆上的 string）
         *      在**离开这个块**时按声明逆序软释放。 */
        PushScope();
        PushOwnedScope();
        for (const auto &storage : g_pendingParameterStorage) MarkOwned(storage);
        g_pendingParameterStorage.clear();
        for (const auto &owner : g_parameterReferenceOwners)
        {
            if (owner.second->kind == MType::Struct)
                MarkDestructor(owner.first, owner.second);
            else
                MarkOwned(owner.first);
        }
        g_parameterReferenceOwners.clear();
        Memory::Vector<simple_ast::NodePtr> out = LowerStmtList(static_cast<int>(node.i));
        const MOwnedScope scope = g_ownedScopes.back();
        g_ownedScopes.pop_back();
        PopScope();
        AppendScopeDrops(scope, Memory::String(), &out);
        return simple_ast::Node::block(out);
    }
    case MNode::Destructure:
        return LowerDestructure(id);
    case MNode::Declare:
        return LowerLocalDecl(id);
    case MNode::Delete:
        return LowerDelete(id);
    case MNode::If:
        return LowerIf(id);
    case MNode::While:
        return LowerWhile(id);
    case MNode::DoWhile:
        return LowerDoWhile(id);
    case MNode::For:
        return LowerFor(id);
    case MNode::Foreach:
        return LowerForeach(id);
    case MNode::Loop:
        return LowerLoop(id, nullptr);
    case MNode::Switch:
        return LowerSwitch(id);
    case MNode::Break:
    {
        /* 跳出去就不再回来：先把这个循环里**更深**的块中的所有者变量
         * 软释放掉，再跳（见"生命周期"一节）。 */
        MBlockContext *const context = FindBreakContext(!node.kids.empty(), node.i);
        if (context == nullptr)
        {
            Fail(node.i != 0       ? "break #level 指定的循环层数超出当前函数的嵌套循环范围" :
                 node.kids.empty() ? "break 只能用在循环里面" :
                                     "yield 值; 需要外层值表达式接收结果");
            return nullptr;
        }
        // break 的值可包含内层 loop，压栈可能使 g_blockStack 中的指针失效。
        const MBlockContext breakContext = *context;
        int valueId = node.kids.empty() ? -1 : node.kids[0];
        if (valueId >= 0 && node.type2 != 1)
        {
            valueId = PrepareBreakValue(valueId, breakContext.valueScopeDepth, breakContext.returnExpression);
            g_nodes[id].kids[0] = valueId;
        }
        Memory::Vector<simple_ast::NodePtr> out;
        if (valueId >= 0)
        {
            if (breakContext.breakValue.empty() || !breakContext.breakType || breakContext.breakType->kind == MType::Void)
            {
                Fail("`break value;` 只能用于 `loop` 或 if 表达式，while / for 只接受 `break;`");
                return nullptr;
            }
            Memory::Vector<simple_ast::NodePtr> fresh;
            Memory::Vector<simple_ast::NodePtr> *const savedSink = g_freshSink;
            g_freshSink = &fresh;
            const auto breakType = breakContext.breakType;
            const auto breakSlot = breakContext.breakValue;
            Memory::Set<Memory::String> borrowed;
            if (ContainsBorrowedReferences(breakType, &borrowed))
                g_borrowFacts.push_back({ magel_borrow_checker::Fact::Store, BorrowSourceDepth(valueId),
                                          breakContext.valueScopeDepth, g_reductionLine, "（分支退出值不能借用已结束的分支局部变量）" });
            const simple_ast::NodePtr value = IsObjectLike(breakType) ? MaterializeObjectValue(valueId, breakType) : MaterializeStringValue(valueId, breakType);
            g_freshSink = savedSink;
            out.insert(out.end(), fresh.begin(), fresh.end());
            if (breakType->kind == MType::Struct)
                out.push_back(StoreStructValue(simple_ast::Node::var(breakSlot), valueId, value, breakType));
            else
                out.push_back(simple_ast::Node::assign(breakSlot, value));
        }
        else if (!breakContext.breakValue.empty())
        {
            Fail("有返回值的 loop 必须使用 `break value;`，不能写空 `break;`");
            return nullptr;
        }
        AppendDropsAbove(breakContext.scopeDepth, Memory::String(), &out);
        out.push_back(simple_ast::Node::jump(breakContext.breakLabel));
        return simple_ast::Node::block(out);
    }
    case MNode::Continue:
    {
        /* `continue` 也是"离开这一轮"：同样先释放，再跳到下一轮的起点
         * （for 的下一轮起点在自增那一步，见 LowerFor）。 */
        std::size_t depth = 0;
        const Memory::String target = FindContinueTarget(&depth);
        if (target.empty())
        {
            Fail("continue 只能用在循环里面");
            return nullptr;
        }
        Memory::Vector<simple_ast::NodePtr> out;
        AppendDropsAbove(depth, Memory::String(), &out);
        out.push_back(simple_ast::Node::jump(target));
        return simple_ast::Node::block(out);
    }
    case MNode::ParseBad:
        /* 解析期挂上来的说明：降级时原样报出来。 */
        Fail(node.s);
        return nullptr;
    case MNode::Return:
    {
        if (!node.kids.empty()) InferExpression(node.kids[0], g_returnType);
        Memory::Set<Memory::String> borrowedTypes;
        if (!node.kids.empty() && ContainsBorrowedReferences(g_returnType, &borrowedTypes))
            g_borrowFacts.push_back({ magel_borrow_checker::Fact::Return, BorrowSourceDepth(node.kids[0]), 0u,
                                      g_reductionLine, "（" + Describe(g_returnType) + "，所属类型 " + g_owner + "，来源 " + NameOfExpr(node.kids[0]) + "）" });
        /* 返回值先算出来（字符串：`return "字面量"` 也先复制成堆串），再释放本函数里
         * 所有"还开着的块"里那些所有者变量 —— **除了**被 return 出去的那个：
         * 它的所有权交给调用方（`string t = f();` 的 t 就是那块串的新主人）。 */
        Memory::Set<Memory::String> keep = ReturnedOwnedNames(id);
        if (g_returnType && (g_returnType->kind == MType::String || IsObjectLike(g_returnType)) && std::any_of(g_ownedScopes.begin(), g_ownedScopes.end(), [](const MOwnedScope &scope) { return scope.hasDeferred; }))
            keep.clear();  // 保存独立返回值，回调随后可以修改原变量。
        Memory::Vector<simple_ast::NodePtr> out;
        Memory::Vector<simple_ast::NodePtr> fresh;
        g_freshSink = &fresh;
        const simple_ast::NodePtr value =
            node.kids.empty() ? simple_ast::Node::i(0) : MaterializeReturnStringValue(node.kids[0], g_returnType, keep);
        g_freshSink = nullptr;
        /* 返回值**只求值一次**，先落进一个**不归本块所有**的槽（`__magel_ret_v`）：
         * 这样"把临时串还回去"的那几步可以照常走，而返回值留给调用方。
         *
         * 为什么必须落槽：`ret` 的实参是在**前面的语句（含那些 drop）执行完之后**才求值的，
         * 而返回值往往要读某个马上就被释放的变量。以前这里把同一个表达式既当语句发一遍
         * （在 drop 之前）、又当 `ret` 的实参发一遍（在 drop 之后）—— 表达式被求值**两次**，
         * 第二次读的是已经释放的东西（`return len(obj.get());` 就撞在这上面：
         * 取出来的串被 drop 掉之后再数一次长度 → "读不到字符串"）。
         * 现在两个返回类型走同一条路：算一次 → 存槽 → 释放 → `ret` 那个槽。 */
        const Memory::String slot = AllocateName("__magel_ret_v");
        out.push_back(simple_ast::Node::declare(slot, LowerType(g_returnType, slot)));
        out.insert(out.end(), fresh.begin(), fresh.end());
        Memory::String returnedParameter;
        for (const auto &name : keep)
            if (g_parameterValueStorage.count(name)) returnedParameter = name;
        if (g_returnType && g_returnType->kind == MType::Struct)
        {
            out.push_back(DefaultInit(slot, g_returnType, false, false));
            if (!returnedParameter.empty())
                out.push_back(CopyStructValue(simple_ast::Node::var(slot), value, g_returnType));
            else if (!node.kids.empty() && (IsFreshAggregate(node.kids[0]) || IsImplicitConversionValue(node.kids[0], g_returnType)))
                out.push_back(MoveStructValue(simple_ast::Node::var(slot), value, g_returnType));
            else
                out.push_back(DeepCopyStructValue(simple_ast::Node::var(slot), value, g_returnType));
        }
        else
            out.push_back(simple_ast::Node::assign(slot, value));
        // 枚举载荷已复制拥有型叶子，Drop 契约移交给载荷，原局部副本的叶子仍须释放。
        for (const auto &scope : g_scope)
            for (const auto &entry : scope)
            {
                const auto &variable = entry.second;
                if (!keep.count(variable.emitted) || !variable.type || variable.type->kind != MType::Struct || SameType(variable.type, g_returnType)) continue;
                if (const auto release = MakeAggregateRelease(variable.type->name, simple_ast::Node::var(variable.emitted)))
                    out.push_back(release);
            }
        AppendDropsAllExcept(keep, &out);
        for (const auto &name : keep)
            if (g_parameterValueStorage.count(name)) out.push_back(MakeDrop(name, false));
        /* 入口 `main` 的全局停机清理不在这里插：函数体全部降级完后由
         * `InsertGlobalCleanupBeforeReturns` 统一补，免得后面别的函数又改了全局的登记项。 */
        out.push_back(simple_ast::Node::ret(simple_ast::Node::var(slot)));
        return simple_ast::Node::block(out);
    }
    case MNode::ExprStmt:
    {
        const int valueId = node.kids.empty() ? -1 : node.kids[0];
        // 独立花括号语句即使带尾分号也只是普通作用域，不创建可丢弃的隐式值出口。
        if (valueId >= 0 && N(valueId).kind == MNode::Loop && N(valueId).s == "block")
            return LowerStmt(N(valueId).kids[0]);
        /* 构造结果必须由值上下文接管，不能作为独立语句丢弃。 */
        if (valueId >= 0 && N(valueId).kind == MNode::EnumCtor && N(valueId).type2 == 1 && N(valueId).s2 == "create")
        {
            if (const auto *owner = FindStruct(N(valueId).s))
                for (const auto &method : owner->methods)
                    if (!method.superseded && method.isNamedConstructor)
                    {
                        Fail("构造结果必须被接收，不能丢弃 `" + N(valueId).s + "::" + N(valueId).s2 + "(...)` 的拥有值");
                        return Bad();
                    }
        }
        /* 外层有值出口时，语句 loop 透明；其它有值 loop 仍走表达式路径并丢弃结果。 */
        if (valueId >= 0 && N(valueId).kind == MNode::Loop && ((N(valueId).s.empty() && FindBreakContext(true) != nullptr) || LoopValueType(valueId)->kind == MType::Void))
            return LowerLoop(valueId, nullptr);
        /* 赋值当语句用（`x = 1;`、`a[i] = v;`、`p.x = v;`）；值上下文由 LowerUncheckedExpr 保存右值并返回。 */
        if (valueId >= 0 && N(valueId).kind == MNode::Assign)
        {
            const MNode assign = N(valueId);
            RecordExpressionEffects(valueId);
            if (assign.type2 == 1)
            {
                const int target = assign.kids[0];
                const auto type = ExprType(target);
                const auto binding = N(target).kind == MNode::Var ? Find(N(target).s) : nullptr;
                if (binding && !binding->elementReference && !g_referenceStorage.count(binding->emitted) && (type->kind == MType::Int || type->kind == MType::Double))
                    return LowerAssign(target, assign.kids[1]);
                return LowerExpr(valueId);
            }
            return LowerAssign(assign.kids[0], assign.kids[1]);
        }
        if (valueId >= 0 && N(valueId).kind == MNode::Update)
            return LowerUpdate(valueId, true);
        /* `f();` / `v.copy();` 等独立语句会创建新返回值，却没有变量接管所有权 ——
         * 字符串 / 容器 / 资源句柄软释放，实现 Drop / Closeable 的结构体跑一次析构。 */
        if (valueId >= 0 && IsFreshValue(valueId))
        {
            Memory::Vector<simple_ast::NodePtr> fresh;
            Memory::Vector<simple_ast::NodePtr> *const savedSink = g_freshSink;
            g_freshSink = &fresh;
            const simple_ast::NodePtr discard = DiscardFreshValue(valueId);
            g_freshSink = savedSink;
            fresh.push_back(discard);
            return simple_ast::Node::block(fresh);
        }
        Memory::Vector<simple_ast::NodePtr> fresh;
        Memory::Vector<simple_ast::NodePtr> *const savedSink = g_freshSink;
        g_freshSink = &fresh;
        const simple_ast::NodePtr value = LowerExpr(valueId);
        g_freshSink = savedSink;
        if (value != nullptr && value->kind == simple_ast::Node::Kind::Block)
            fresh.push_back(value); /* 已经是语句块（例如显式 `x.drop()`），别再包一层 */
        else
            fresh.push_back(simple_ast::Node::exprStmt(value));
        return simple_ast::Node::block(fresh);
    }
    default:
        return simple_ast::Node::exprStmt(LowerExpr(id));
    }
}

/**
 * @brief 标签列表拼成一句（报错文案用）。
 * @param labels 待拼接的标签列表。
 * @retval 生成或读取的结果文本。
 */
Memory::String JoinLabels(const Memory::Vector<Memory::String> &labels)
{
    Memory::String text;
    for (std::size_t index = 0; index < labels.size(); ++index)
    {
        if (index != 0u) text += "、";
        text += labels[index];
    }
    return text;
}

/**
 * @brief 在一组重载里挑一个：先要求形参类型**完全一致**，再放宽到 int64 → float64。
 *
 * 这一档"加宽"是有意留小的：只有整数能喂给 float64 形参（真实转换由四元式的
 * `(decl, float64, local, 形参)` 在绑定时做），其余类型必须一模一样。同一档里
 * 有两个都能接住时**不猜**，直接报"有多个同样匹配"。
 */
bool PickOverload(const Memory::Vector<Memory::String> &candidates,
                  const Memory::Vector<std::shared_ptr<MType>> &args, Memory::String *label,
                  Memory::String *reason)
{
    for (int tier = 0; tier < 2; ++tier)
    {
        const bool widening = (tier == 1);
        Memory::Vector<Memory::String> matched;
        for (const Memory::String &candidate : candidates)
        {
            const auto signature = g_funcs.find(candidate);
            if (signature == g_funcs.end()) continue;
            const Memory::Vector<MParam> &params = signature->second.params;
            const bool variadic = !params.empty() && params.back().variadic;
            const std::size_t fixed = variadic ? params.size() - 1u : params.size();
            if (!AcceptsArgumentCount(params, args.size()))
                continue;
            bool ok = true;
            for (std::size_t index = 0; index < fixed && index < args.size() && ok; ++index)
            {
                ok = Acceptable(params[index].type, args[index], widening);
            }
            if (variadic)
            {
                const std::shared_ptr<MType> element = params.back().type->elem;
                for (std::size_t index = fixed; index < args.size() && ok; ++index)
                    ok = Acceptable(element, args[index], widening);
            }
            if (ok) matched.push_back(candidate);
        }
        if (matched.size() == 1u)
        {
            *label = matched.front();
            return true;
        }
        if (matched.size() > 1u)
        {
            *reason = "有多个重载同样匹配：" + JoinLabels(matched) + DefaultArgumentAmbiguity(matched, args.size());
            return false;
        }
    }
    *reason = "没有参数类型能对上的重载（候选：" + JoinLabels(candidates) + "）";
    return false;
}

/**
 * @brief 解析调用：普通函数按实参类型选重载，方法按接收者的 struct 找同名方法再选重载。
 * @param id 待查找或操作的对象编号。
 * @param label 诊断或跳转使用的标签。
 * @param params 形参或类型参数列表。
 * @param result 接收处理结果的对象。
 * @retval 满足检查条件或操作完成时为 true，检查失败或无法完成时为 false。
 */
bool ResolveCall(int id, Memory::String *label, Memory::Vector<Memory::String> *params,
                 std::shared_ptr<MType> *result)
{
    InferExpression(id);
    const MNode node = N(id);
    if (node.kids.size() < 2u) return Fail("调用缺少被调者");
    const MNode callee = N(node.kids[0]);
    const Memory::Vector<int> args = N(node.kids[1]).kids;

    Memory::Vector<std::shared_ptr<MType>> argTypes;

    Memory::Vector<Memory::String> candidates;
    Memory::String subject;
    if (callee.kind == MNode::Var)
    {
        const auto *found = FindFunctionOverloads(callee.module, callee.s);
        if (found == nullptr)
        {
            return Fail("调用了没定义的函数：" + callee.s);
        }
        candidates = *found;
        subject = "函数 " + ModuleSymbol(callee.module, callee.s);
    }
    else if (callee.kind == MNode::EnumCtor && !callee.s.empty() && (std::islower(static_cast<unsigned char>(callee.s[0])) || (callee.s.compare(0u, 11u, "std::lang::") == 0u && IsStandardModule(callee.s.substr(11u)))))
    {
        const Memory::String key = ModuleSymbol(callee.s, callee.s2);
        const auto found = g_functionIndex.find(key);
        if (found == g_functionIndex.end())
            return Fail("调用了没导入或没定义的模块函数：" + key);
        candidates = found->second;
        subject = "函数 " + key;
    }
    else if (callee.kind == MNode::Member || callee.kind == MNode::Arrow)
    {
        const Memory::String owner = StructNameOf(ExprType(callee.kids[0]));
        if (owner.empty()) return Fail("方法只能调在 struct 上：" + callee.s);
        const auto found = g_structs.find(owner);
        if (found != g_structs.end())
        {
            for (const MMethod &method : found->second.methods)
            {
                if (method.superseded || method.isStatic || method.isConstructor || method.source != callee.s) continue;
                candidates.push_back(owner + "." + method.name);
            }
        }
        if (candidates.empty()) return Fail("struct " + owner + " 没有方法 " + callee.s);
        subject = "方法 " + owner + "." + callee.s;
    }
    else
    {
        return Fail("不支持的调用形式（只有 `函数(...)` 与 `对象.方法(...)`）");
    }

    Memory::String chosen;
    Memory::String reason;
    for (std::size_t index = 0; index < args.size(); ++index)
    {
        std::shared_ptr<MType> expected;
        bool same = true;
        for (const auto &candidate : candidates)
        {
            const auto found = g_funcs.find(candidate);
            if (found == g_funcs.end() || index >= found->second.params.size())
            {
                same = false;
                break;
            }
            const auto type = found->second.params[index].type;
            if (!expected)
                expected = type;
            else if (!SameType(expected, type))
            {
                same = false;
                break;
            }
        }
        if (same && expected) InferExpression(args[index], expected);
        argTypes.push_back(RawExprType(args[index]));
    }
    if (!PickOverload(candidates, argTypes, &chosen, &reason))
    {
        return Fail("调用 " + subject + " 找不到匹配的重载：" + DescribeTypes(argTypes) + "；" + reason);
    }
    const auto signature = g_funcs.find(chosen);
    if (signature == g_funcs.end()) return Fail("内部错误：重载标签没有签名 " + chosen);
    for (const MFunctionDecl &declaration : g_functionDecls)
    {
        if (declaration.label != chosen || declaration.superseded) continue;
        if (declaration.moduleLocal && declaration.module != g_currentModule)
            return Fail("模块 " + declaration.module + " 的 inline 函数 " + declaration.source + " 不能从模块 " + (g_currentModule.empty() ? Memory::String("<root>") : g_currentModule) + " 访问");
        break;
    }
    *label = chosen;
    *result = signature->second.result;
    for (const MParam &param : signature->second.params) params->push_back(param.emitted);
    return true;
}

/**
 * @brief 解析并降级静态限定名称访问。
 * @param id 待查找或操作的对象编号。
 * @retval 生成的低层语法节点。
 */
simple_ast::NodePtr LowerStaticQualified(int id)
{
    /* 这里要改 `s` 做归一，所以取一份副本（`N()` 给的是只读引用）。 */
    MNode node = N(id);
    /* `std::模块::类型::成员` 里的类型按**裸名**登记，这里统一归一，
     * 免得同一份声明因为写法不同（限定 / 裸名）在两张表里各查一次还查不到。 */
    if (node.s.find("::") != Memory::String::npos && (FindStaticField(BareTypeName(node.s), node.s2) != nullptr || FindEnumVariant(BareTypeName(node.s), node.s2) != nullptr))
        node.s = BareTypeName(node.s);
    const Memory::String key = node.s + "::" + node.s2;
    Memory::String ownerKey;
    if (node.type2 != 1)
    {
        const MStaticField *field = FindStaticField(node.s, node.s2);
        if (field == nullptr)
            return Fail("struct " + node.s + " 没有 static 字段 " + node.s2), Bad();
        return simple_ast::Node::var(field->emitted);
    }
    const MStructDef *owner = FindStruct(node.s, &ownerKey);
    Memory::Vector<Memory::String> candidates;
    if (owner != nullptr)
        for (const MMethod &method : owner->methods)
            if (method.isStatic && method.source == node.s2)
            {
                /* 用**注册键**拼 `g_funcs` 的键：写出名可能是限定形式
                 * （`custom::Box__int`），而实例化体是按裸名登记的。 */
                candidates.push_back(ownerKey + "." + method.name);
            }
    Memory::Vector<std::shared_ptr<MType>> argTypes;
    for (const int argument : node.kids) argTypes.push_back(RawExprType(argument));
    Memory::String chosen;
    Memory::String reason;
    if (!PickOverload(candidates, argTypes, &chosen, &reason))
        return Fail("调用 static 函数 " + key + " 找不到匹配的重载：" + DescribeTypes(argTypes) + "；" + reason), Bad();
    const MFuncSig &signature = g_funcs[chosen];
    if (signature.staticCallbacks && g_currentModule != "std::event")
        for (std::size_t i = 0; i < node.kids.size() && i < signature.params.size(); ++i)
            if (signature.params[i].type->kind == MType::Func && BorrowSourceDepth(node.kids[i]) != 0u)
                return Fail("[MGL1204] 异步事件回调不能捕获局部变量，其生命周期必须覆盖事件登记"), Bad();
    Memory::Vector<Memory::String> names;
    Memory::Vector<simple_ast::NodePtr> values;
    const bool variadic = !signature.params.empty() && signature.params.back().variadic;
    const std::size_t fixed = signature.params.size() - (variadic ? 1u : 0u);
    for (std::size_t index = 0; index < fixed; ++index)
    {
        names.push_back(signature.params[index].emitted);
        const std::shared_ptr<MType> paramType = signature.params[index].type;
        values.push_back(index < node.kids.size() ? LowerArgumentValue(node.kids[index], paramType) : LowerDefaultArgument(signature.params[index]));
    }
    if (variadic)
    {
        names.push_back(signature.params.back().emitted);
        values.push_back(PackVariadicArguments(node.kids, fixed, signature.params.back()));
    }
    return simple_ast::Node::call(chosen, names, values);
}

bool ValidateReturnForms(int statementId, const std::shared_ptr<MType> &result);

/**
 * @brief 校验语句列表中的返回形式。
 * @param listId 语句列表在节点池中的编号。
 * @param result 接收处理结果的对象。
 * @retval 满足检查条件或操作完成时为 true，检查失败或无法完成时为 false。
 */
bool ValidateReturnList(int listId, const std::shared_ptr<MType> &result)
{
    if (listId < 0) return true;
    for (const int statementId : N(listId).kids)
        if (!ValidateReturnForms(statementId, result)) return false;
    return true;
}

/**
 * @brief 校验语句内各分支的返回形式。
 * @param statementId 语句在节点池中的编号。
 * @param result 接收处理结果的对象。
 * @retval 满足检查条件或操作完成时为 true，检查失败或无法完成时为 false。
 */
bool ValidateReturnForms(int statementId, const std::shared_ptr<MType> &result)
{
    if (statementId < 0) return true;
    const MNode &statement = N(statementId);
    switch (statement.kind)
    {
    case MNode::Return:
        if (result && result->kind == MType::Void && !statement.kids.empty())
            return Fail(g_namedConstructorBody ? Memory::String("`static T new(...)` 构造入口体内不要写 `return`："
                                                                "往 `self`（或 `self.字段`）写值即可，返回值由调用点接住") :
                                                 Memory::String(MAGEL_TEXT("void 函数不能返回值")));
        if (result && result->kind != MType::Void && statement.kids.empty())
            return Fail("返回类型为 " + Describe(result) + " 的函数必须返回一个值");
        return true;
    case MNode::Block:
        return ValidateReturnList(static_cast<int>(statement.i), result);
    case MNode::If:
        if (!ValidateReturnForms(statement.kids[1], result)) return false;
        for (const int elifId : N(statement.kids[2]).kids)
            if (!ValidateReturnForms(N(elifId).kids[1], result)) return false;
        return statement.kids[3] < 0 || ValidateReturnForms(statement.kids[3], result);
    case MNode::While:
        return ValidateReturnForms(statement.kids[1], result);
    case MNode::DoWhile:
        return ValidateReturnForms(statement.kids[0], result);
    case MNode::For:
        return ValidateReturnForms(statement.kids[3], result);
    case MNode::Foreach:
        return ValidateReturnForms(statement.kids[1], result);
    case MNode::Loop:
        return ValidateReturnForms(statement.kids[0], result);
    case MNode::Switch:
        for (const int caseId : N(statement.kids[1]).kids)
            if (!ValidateReturnList(N(caseId).kids[0], result)) return false;
        return true;
    default:
        return true;
    }
}

enum MReturnFlow
{
    FlowNext = 1,
    FlowReturn = 2,
    FlowEscape = 4,
    FlowContinue = 8,
    FlowValueEscape = 16,
};

int StatementReturnFlowImpl(int statementId, bool transparentLoops, Memory::Set<long long> *exits, std::size_t depth);

/**
 * @brief 分析表达式求值期间的函数返回、值退出及层级退出路径。
 * @param id 表达式节点编号。
 * @param transparentLoops 是否让带值退出穿透语句循环。
 * @param exits 收集越出表达式的层级退出。
 * @param depth 当前分析根内已进入的循环层数。
 * @retval 返回表达式可能继续或提前退出的路径位掩码。
 */
int ExpressionReturnFlow(int id, bool transparentLoops, Memory::Set<long long> *exits, std::size_t depth)
{
    if (id < 0) return FlowNext;
    const MNode node = N(id);
    if (node.kind == MNode::Lambda) return FlowNext;
    if (node.kind == MNode::Loop)
    {
        if (node.s.empty()) return StatementReturnFlowImpl(id, transparentLoops, exits, depth);
        const int flow = StatementReturnFlowImpl(node.kids[0], true, exits, depth);
        return (flow & ~FlowValueEscape) | ((flow & FlowValueEscape) ? FlowNext : 0);
    }
    int flow = FlowNext;
    for (const int child : node.kids)
    {
        if ((flow & FlowNext) == 0) break;
        const int evaluated = ExpressionReturnFlow(child, transparentLoops, exits, depth);
        flow = (flow & ~FlowNext) | evaluated;
        if (node.kind == MNode::Bin && (node.s == "&&" || node.s == "||")) flow |= FlowNext;
    }
    return flow;
}

/**
 * @brief 分析语句列表是否在所有路径上返回。
 * @param listId 语句列表在节点池中的编号。
 * @param transparentLoops 是否让带值退出穿透语句循环。
 * @param exits 收集仍需向外传递的层级退出。
 * @param depth 当前分析根内已进入的循环层数。
 * @retval 返回正常继续、函数返回及各类退出路径的位掩码。
 */
int StatementListReturnFlow(int listId, bool transparentLoops, Memory::Set<long long> *exits, std::size_t depth)
{
    if (listId < 0) return FlowNext;
    int flow = FlowNext;
    for (const int statementId : N(listId).kids)
    {
        if ((flow & FlowNext) == 0) break;
        flow = (flow & ~FlowNext) | StatementReturnFlowImpl(statementId, transparentLoops, exits, depth);
    }
    return flow;
}

/**
 * @brief 分析单条语句的控制流返回情况。
 * @param statementId 语句在节点池中的编号。
 * @param transparentLoops 是否让带值退出穿透语句循环。
 * @param exits 收集仍需向外传递的层级退出。
 * @param depth 当前分析根内已进入的循环层数。
 * @retval 返回正常继续、函数返回及各类退出路径的位掩码。
 */
int StatementReturnFlowImpl(int statementId, bool transparentLoops, Memory::Set<long long> *exits, std::size_t depth)
{
    if (statementId < 0) return FlowNext;
    const MNode &statement = N(statementId);
    switch (statement.kind)
    {
    case MNode::ExprStmt:
        if (!statement.kids.empty())
        {
            const MNode &value = N(statement.kids[0]);
            if (value.kind == MNode::Call && !value.kids.empty() && StandardCallName(N(value.kids[0])) == "panic") return FlowReturn;
            if (value.kind == MNode::Loop)
            {
                if (value.s == "block") return StatementReturnFlowImpl(value.kids[0], transparentLoops, exits, depth);
                if (value.s.empty()) return StatementReturnFlowImpl(statement.kids[0], transparentLoops, exits, depth);
                const int flow = StatementReturnFlowImpl(value.kids[0], true, exits, depth);
                return (flow & ~FlowValueEscape) | ((flow & FlowValueEscape) ? FlowNext : 0);
            }
        }
        return statement.kids.empty() ? FlowNext : ExpressionReturnFlow(statement.kids[0], transparentLoops, exits, depth);
    case MNode::Declare:
    {
        int flow = FlowNext;
        for (const int declaration : N(statement.kids[0]).kids)
        {
            if ((flow & FlowNext) == 0) break;
            const auto item = N(declaration);
            if (!item.kids.empty()) flow = (flow & ~FlowNext) | ExpressionReturnFlow(item.kids[0], transparentLoops, exits, depth);
        }
        return flow;
    }
    case MNode::Destructure:
        return ExpressionReturnFlow(statement.kids[1], transparentLoops, exits, depth);
    case MNode::Return:
    {
        const int flow = statement.kids.empty() ? FlowNext : ExpressionReturnFlow(statement.kids[0], transparentLoops, exits, depth);
        return (flow & ~FlowNext) | ((flow & FlowNext) ? FlowReturn : 0);
    }
    case MNode::Break:
        if (statement.i != 0)
        {
            exits->insert(statement.i);
            return 0;
        }
        return statement.kids.empty() ? FlowEscape : FlowValueEscape;
    case MNode::Continue:
        return FlowContinue;
    case MNode::Block:
        return StatementListReturnFlow(static_cast<int>(statement.i), transparentLoops, exits, depth);
    case MNode::If:
    {
        int flow = StatementReturnFlowImpl(statement.kids[1], transparentLoops, exits, depth);
        for (const int elifId : N(statement.kids[2]).kids)
            flow |= StatementReturnFlowImpl(N(elifId).kids[1], transparentLoops, exits, depth);
        flow |= statement.kids[3] < 0 ? FlowNext : StatementReturnFlowImpl(statement.kids[3], transparentLoops, exits, depth);
        return flow;
    }
    case MNode::While:
    case MNode::For:
    case MNode::Foreach:
    case MNode::Loop:
    case MNode::DoWhile:
    {
        const int bodyId = statement.kind == MNode::For                                      ? statement.kids[3] :
                           statement.kind == MNode::Loop || statement.kind == MNode::DoWhile ? statement.kids[0] :
                                                                                               statement.kids[1];
        Memory::Set<long long> nested;
        int body = StatementReturnFlowImpl(bodyId, transparentLoops, &nested, depth + 1);
        for (const long long level : nested)
        {
            if (level == 1 || (level < 0 && depth == 0 && FindBreakContext(false, -1) == nullptr))
                body |= FlowEscape;
            else
                exits->insert(level < 0 ? level : level - 1);
        }
        const int propagated = body & (FlowReturn | (transparentLoops ? FlowValueEscape : 0));
        if (statement.kind == MNode::While || statement.kind == MNode::For || statement.kind == MNode::Foreach)
            return FlowNext | propagated;
        const int normal = FlowEscape | (transparentLoops ? 0 : FlowValueEscape) | (statement.kind == MNode::DoWhile ? FlowNext | FlowContinue : 0);
        return propagated | ((body & normal) ? FlowNext : 0);
    }
    case MNode::Switch:
    {
        bool hasDefault = false;
        const Memory::Vector<int> &cases = N(statement.kids[1]).kids;
        int flow = 0;
        for (const int caseId : cases)
        {
            const MNode &item = N(caseId);
            if (item.i < 0) hasDefault = true;
            int branch = StatementListReturnFlow(item.kids[0], transparentLoops, exits, depth);
            if (statement.s != "match" && (branch & FlowEscape) != 0)
                branch = (branch & ~FlowEscape) | FlowNext;
            flow |= branch;
        }
        if (!hasDefault && statement.s == "match")
        {
            Memory::String owner;
            Memory::Set<Memory::String> covered;
            bool consistent = true;
            for (const int caseId : cases)
            {
                const auto item = N(caseId);
                if (item.i < 0 || item.guard >= 0) continue;
                auto pattern = N(static_cast<int>(item.i));
                if (pattern.kind == MNode::Call && !pattern.kids.empty()) pattern = N(pattern.kids[0]);
                if (pattern.kind != MNode::EnumCtor)
                {
                    consistent = false;
                    continue;
                }
                if (owner.empty()) owner = pattern.s;
                if (owner != pattern.s) consistent = false;
                covered.insert(pattern.s2);
            }
            auto enumeration = g_enums.find(owner);
            if (consistent && enumeration != g_enums.end() && !enumeration->second.variants.empty())
            {
                hasDefault = true;
                for (const auto &variant : enumeration->second.variants)
                    if (!covered.count(variant.name)) hasDefault = false;
            }
        }
        if (!hasDefault) flow |= FlowNext;
        return flow;
    }
    default:
        return FlowNext;
    }
}

/**
 * @brief 分析语句返回路径并将越出分析根的循环退出报告给调用方。
 * @param statementId 分析根语句编号。
 * @param transparentLoops 是否让带值退出穿透语句循环。
 * @retval 返回正常继续、函数返回及退出路径的位掩码。
 */
int StatementReturnFlow(int statementId, bool transparentLoops)
{
    Memory::Set<long long> exits;
    const int flow = StatementReturnFlowImpl(statementId, transparentLoops, &exits, 0);
    return flow | (exits.empty() ? 0 : FlowEscape);
}

/**
 * @brief 降级一个函数 / 方法。
 *
 * 形参在四元式里是**按名字绑定**的（每个调用帧一份），调用点用同一份签名发
 * `(param, 实参, _, 形参名)`；但 stackvm 的 `decl` 类型表是**按名字全局唯一**的
 * （`program.types[name]`），所以形参名字也必须全程序唯一 —— 否则 `my_sin` 的
 * `float64 x` 会和别的函数里同名的 `i64 x` 抢类型槽，传参时被按 i64 截断成整数。
 * 所以形参和局部变量一样，重名时统一加 `$序号` 后缀。
 */
/**
 * @brief 把"这个函数累计登记的所有权变量"收成一段**帧清理块**。
 *
 * 内容：逆序跑**最外那层块**那些值的 `Closeable.close()` → `Drop.drop()`（`drop()` 是
 * 终结操作，必须在 `close()` 之后；里层块的不跑 —— 它们可能已经在出块时析构过了，
 * 再跑一次就是拿销毁过的值调用户代码），然后把每条都软释放一次
 * （`(drop, …)`：地址不是活跃堆块就当没这回事）。结尾的 `(ret)` 在展开时表示
 * "这一帧清完了，继续往外"。一条登记项都没有时返回 `nullptr`（不发块，也省一个标签）。
 *
 * 全局停机清理**不走这里**（见 `EmitGlobalCleanupStatements`）：停机时堆内存整体清空，
 * 只需对全局 `Closeable` 补一次 `close()`。
 *
 * @param label 清理块入口标签，约定是 `__magel_cleanup.<函数标签>`。
 */
simple_ast::NodePtr MakeCleanupBlock(const Memory::String &label)
{
    Memory::Vector<simple_ast::NodePtr> statements;
    for (auto it = g_cleanupItems.rbegin(); it != g_cleanupItems.rend(); ++it)
    {
        /*
         * 只处理**最外那层块**登记的值。里层块（`if` / 循环体里的块）的变量在正常流程里
         * 出块时就还过了；而那条分支**没走**时它压根没定义过，硬塞一句 `(drop, 变量)` 会在
         * 停机清理里读一个不存在的变量（`读未定义的变量：m`）。
         * 停机路径上少还一个临时值无所谓 —— 进程本来就要结束了。
         */
        if (!it->outermost) continue;
        if (it->close)
        {
            if (const simple_ast::NodePtr call = MakeInterfaceMethodCallAt(
                    it->owner, "close", simple_ast::Node::var(it->name)))
                statements.push_back(call);
        }
        if (it->drop)
        {
            if (const simple_ast::NodePtr call = MakeInterfaceMethodCallAt(
                    it->owner, "drop", simple_ast::Node::var(it->name)))
                statements.push_back(call);
        }
        if (it->aggregate)
        {
            if (const simple_ast::NodePtr release =
                    MakeAggregateRelease(it->owner, simple_ast::Node::var(it->name)))
                statements.push_back(release);
        }
        AppendOwnedDropByName(it->name, it->runtimeArray, &statements);
    }
    if (statements.empty()) return nullptr;
    statements.push_back(simple_ast::Node::ret(simple_ast::Node::i(0)));
    Memory::Vector<simple_ast::NodePtr> block;
    block.push_back(simple_ast::Node::label(label));
    for (const simple_ast::NodePtr &statement : statements) block.push_back(statement);
    return simple_ast::Node::block(block);
}

/**
 * @brief 把清理块挂到函数体末尾（正常流程跑不到：前面已经有自己的 `(ret)`）。
 * @param function 函数定义或函数指针。
 */
void AppendCleanupBlock(simple_ast::Function &function)
{
    const simple_ast::NodePtr cleanup =
        MakeCleanupBlock("__magel_cleanup." + function.quadLabel());
    if (cleanup == nullptr) return;
    Memory::Vector<simple_ast::NodePtr> body;
    if (function.body != nullptr && function.body->kind == simple_ast::Node::Kind::Block)
        body.assign(function.body->kids.begin(), function.body->kids.end());
    else if (function.body != nullptr)
        body.push_back(function.body);
    /* void 函数正常落到末尾时，已由函数体释放局部所有者；不能再进入停机清理块。 */
    body.push_back(simple_ast::Node::ret(simple_ast::Node::i(0)));
    body.push_back(cleanup);
    function.body = simple_ast::Node::block(body);
}

/**
 * @brief 为函数体建立作用域并生成低层函数定义。
 * @param name 名称文本。
 * @param owner 所属对象或类型信息。
 * @param result 接收处理结果的对象。
 * @param params 形参或类型参数列表。
 * @param bodyId 函数体在节点池中的编号。
 * @param module 当前模块的名称。
 * @retval 保存为函数体建立作用域并生成低层函数定义结果的对象。
 */
simple_ast::Function LowerFunction(const Memory::String &name, const Memory::String &owner,
                                   const std::shared_ptr<MType> &result,
                                   const Memory::Vector<MParam> &params,
                                   int bodyId, const Memory::String &module)
{
    ClearActiveError();
    simple_ast::Function function;
    function.name = name;
    // 清理块先于 addMethod 生成，必须提前设置所属类型，避免同名方法的标签冲突。
    function.owner = owner;
    if (!CheckDeclaredType(result, name)) return {}; /* 返回类型也得声明过 */
    if (!ValidateReturnForms(bodyId, result)) return {};
    if (result && result->kind != MType::Void && (StatementReturnFlow(bodyId) & (FlowNext | FlowEscape | FlowContinue | FlowValueEscape)) != 0)
    {
        Fail("返回类型为 " + Describe(result) + " 的函数 " + name + " 不是所有路径都有 return");
        return {};
    }
    const std::shared_ptr<MType> savedReturnType = g_returnType;
    g_returnType = result; /* `return 2;` → `return 2.0;` */
    function.result = LowerType(result, name);
    PushScope();
    const Memory::String savedOwner = g_owner;
    const Memory::String savedModule = g_currentModule;
    g_owner = owner;
    g_currentModule = module;
    Memory::Vector<simple_ast::NodePtr> parameterReferences;
    for (const MParam &param : params)
    {
        if (!CheckDeclaredType(param.type, param.source)) return {}; /* 报错后返回空函数，g_error 已记下 */
        MVar var;
        var.type = param.type;
        var.emitted = param.emitted;
        Memory::Set<Memory::String> borrowedTypes;
        if (ContainsBorrowedReferences(param.type, &borrowedTypes))
            var.borrowDepth = HasOpaqueObjectBorrow(param.type) ? 0u : 1u;
        // 事件模块入口已在调用点检查回调环境；内部转送保持静态寿命契约。
        if (module == "std::event" && param.type->kind == MType::Func) var.borrowDepth = 0u;
        g_scope.back()[param.source] = var;
        function.parameters.emplace_back(param.emitted, LowerType(param.type, param.emitted));
        const bool ownedValue = param.type->kind == MType::String || IsObjectLike(param.type);
        if (g_addressTakenNames.count(param.source) && NeedsReferenceStorage(param.type))
        {
            const auto storage = AllocateName("__magel_parameter_reference");
            parameterReferences.push_back(simple_ast::Node::declare(storage, simple_ast::Type::i64()));
            parameterReferences.push_back(simple_ast::Node::assign(storage, simple_ast::Node::alloc(simple_ast::Node::i(8))));
            g_pendingParameterStorage.push_back(storage);
            simple_ast::NodePtr value = simple_ast::Node::var(param.emitted);
            if (param.type->kind == MType::String)
                value = simple_ast::Node::call("__vm_str_sub", { "__magel_s", "__magel_a", "__magel_b" },
                                               { value, simple_ast::Node::i(0), simple_ast::Node::i(-1) });
            else if (IsObjectLike(param.type))
                value = simple_ast::Node::call(ObjectCopyFunction(param.type), { "__magel_v" }, { value });
            parameterReferences.push_back(simple_ast::Node::storeAt(simple_ast::Node::var(storage), value, LowerType(param.type, storage)));
            g_referenceStorage[param.emitted] = storage;
            if (ownedValue) g_parameterReferenceOwners.push_back({ param.emitted, param.type });
            g_scope.back()[param.source].emitted = storage;
            g_scope.back()[param.source].elementReference = true;
        }
        else if (ownedValue)
        {
            const auto value = param.type->kind == MType::String ? simple_ast::Node::call("__vm_str_sub", { "__magel_s", "__magel_a", "__magel_b" },
                                                                                          { simple_ast::Node::var(param.emitted), simple_ast::Node::i(0), simple_ast::Node::i(-1) }) :
                                                                   simple_ast::Node::call(ObjectCopyFunction(param.type), { "__magel_v" }, { simple_ast::Node::var(param.emitted) });
            parameterReferences.push_back(simple_ast::Node::assign(param.emitted, value));
            g_parameterReferenceOwners.push_back({ param.emitted, param.type });
        }
        else if (param.type->kind == MType::Struct)
        {
            const auto storage = AllocateName("__magel_parameter_value");
            parameterReferences.push_back(simple_ast::Node::declare(storage, LowerType(param.type, storage)));
            parameterReferences.push_back(simple_ast::Node::assign(storage,
                                                                   simple_ast::Node::alloc(simple_ast::Node::i(std::max(8, StructStride(param.type))))));
            g_pendingParameterStorage.push_back(storage);
            g_parameterValueStorage.insert(storage);
            parameterReferences.push_back(DeepCopyStructValue(simple_ast::Node::var(storage),
                                                              simple_ast::Node::var(param.emitted), param.type));
            g_scope.back()[param.source].emitted = storage;
            g_parameterReferenceOwners.push_back({ storage, param.type });
        }
    }
    /* 停机清理清单按函数重置：`g_cleanupBase` = 函数体那层块的下标（见 NoteCleanup）。 */
    for (const auto &declaration : g_functionDecls)
    {
        if (declaration.label != name) continue;
        const auto captures = g_lambdaCaptures.find(ModuleSymbol(declaration.module, declaration.source));
        if (captures == g_lambdaCaptures.end() || captures->second.empty()) break;
        const auto environment = AllocateName("__magel_environment");
        function.parameters.emplace_back(environment, simple_ast::Type::i64());
        for (std::size_t i = 0; i < captures->second.size(); ++i)
        {
            const auto &capture = captures->second[i];
            const auto local = AllocateName("__magel_capture_" + capture.source);
            MVar binding{ capture.type, local, capture.captureMode == 0 };
            binding.elementReference = (capture.captureMode >= 2 || capture.type->kind != MType::Ptr) && (capture.captureMode < 2 || capture.type->kind != MType::Struct) && (capture.type->kind != MType::Array || IsRuntimeArray(capture.type));
            binding.borrowDepth = 1u;
            g_scope.back()[capture.source] = binding;
            parameterReferences.push_back(simple_ast::Node::declare(local, simple_ast::Type::i64()));
            parameterReferences.push_back(simple_ast::Node::assign(local,
                                                                   simple_ast::Node::call(capture.captureMode >= 2 ? "__vm_closure_ref" : "__vm_closure_get",
                                                                                          capture.captureMode >= 2 ? Memory::Vector<Memory::String>{ "environment", "index", "text", "layout", "float" } :
                                                                                                                     Memory::Vector<Memory::String>{ "environment", "index" },
                                                                                          capture.captureMode >= 2 ? Memory::Vector<simple_ast::NodePtr>{ simple_ast::Node::var(environment),
                                                                                                                                                          simple_ast::Node::i(static_cast<std::int64_t>(i)), simple_ast::Node::i(capture.type->kind == MType::String),
                                                                                                                                                          ContainerStorageFlag(capture.type, capture.captureMode == 3), simple_ast::Node::i(IsFloatType(capture.type)) } :
                                                                                                                     Memory::Vector<simple_ast::NodePtr>{ simple_ast::Node::var(environment), simple_ast::Node::i(static_cast<std::int64_t>(i)) })));
        }
        break;
    }
    g_cleanupItems.clear();
    g_cleanupBase = g_ownedScopes.size();
    MBorrowFlowCapture borrowCapture;
    auto *savedBorrowCapture = g_borrowFlowCapture;
    struct RestoreBorrowCapture
    {
        MBorrowFlowCapture *previous;
        /**
         * @brief 恢复函数降级前的借用捕获记录。
         */
        ~RestoreBorrowCapture() { g_borrowFlowCapture = previous; }
    } restoreBorrowCapture{ savedBorrowCapture };
    g_borrowFlowCapture = &borrowCapture;
    function.body = LowerStmt(bodyId);
    FinishBorrowFlow(bodyId, borrowCapture);
    if (!owner.empty() && (name == "drop" || name == owner + ".drop") && StructHasInterface(owner, "Drop"))
    {
        using simple_ast::Node;
        const auto self = Node::var("self");
        const auto mark = Node::exprStmt(Node::call("__vm_move_end", { "address", "bytes", "type" },
                                                    { self, Node::i(std::max(8, StructStride(NamedType(owner)))), Node::i(TypeIdOf(NamedType(owner))) }));
        function.body = GuardLiveAggregate(owner, self, Node::block({ mark, function.body }));
    }
    g_borrowFlowCapture = savedBorrowCapture;
    if (!parameterReferences.empty())
    {
        parameterReferences.push_back(function.body);
        function.body = simple_ast::Node::block(parameterReferences);
    }
    AppendCleanupBlock(function);
    g_returnType = savedReturnType;
    g_owner = savedOwner;
    g_currentModule = savedModule;
    PopScope();
    return function;
}

/**
 * @brief 两个运行时小函数：把 int64 / float64 按十进制写到**文本流**。
 *
 * stackvm 把 `write` 的数值与 `print` / `putc` 的文本分成两个桶；这里统一用文本流输出。
 */
/**
 * @brief 把几条语句塞到 main 的最前面（内存内建的堆初始化用）。
 * @param program 待生成或执行的程序。
 * @param statements 待处理的语句列表。
 */
[[maybe_unused]] void PrependToMain(simple_ast::Program &program,
                                    const Memory::Vector<simple_ast::NodePtr> &statements)
{
    for (simple_ast::Function &function : program.functions)
    {
        if (function.name != "main" || !function.owner.empty()) continue;
        Memory::Vector<simple_ast::NodePtr> body(statements.begin(), statements.end());
        if (function.body && function.body->kind == simple_ast::Node::Kind::Block)
        {
            for (const simple_ast::NodePtr &statement : function.body->kids)
                body.push_back(statement);
        }
        function.body = simple_ast::Node::block(body);
        return;
    }
}

/**
 * @brief 为程序追加编译器所需的运行时辅助函数。
 * @param program 待生成或执行的程序。
 */
void AddRuntimeHelpers(simple_ast::Program &program)
{
    using namespace simple_ast;
    if (g_hasDefer)
    {
        // 回调栈由生成代码管理，正常出口和 panic 共用同一登记与摘除规则。
        program.addVariable("__magel_defer_top", Type::i64());
        const auto frame = Node::var("frame"), callback = Node::var("callback");
        const auto slot = [](const NodePtr &base, int index) { return Node::offset(base, Node::i(index), 8); };
        Function push;
        push.name = "__magel_defer_push";
        push.result = Type::i64();
        push.parameters = { { "callback", Type::i64() } };
        push.body = Node::block({ Node::declare("frame", Type::i64()), Node::assign("frame", Node::alloc(Node::i(16))),
                                  Node::storeAt(frame, Node::var("__magel_defer_top"), Type::i64()),
                                  Node::storeAt(slot(frame, 1), callback, Type::i64()), Node::assign("__magel_defer_top", frame), Node::ret(frame) });
        program.functions.push_back(std::move(push));
        Function pop;
        pop.name = "__magel_defer_pop";
        pop.result = LowerType(T(M_TypeVoid()), "defer");
        pop.parameters = { { "frame", Type::i64() } };
        pop.body = Node::block({ Node::declare("callback", Type::i64()), Node::declare("link", Type::i64()),
                                 Node::ifThenElse(Node::binary("==", Node::var("__magel_defer_top"), frame),
                                                  Node::assign("__magel_defer_top", Node::loadFrom(frame, Type::i64())),
                                                  Node::block({ Node::assign("link", Node::var("__magel_defer_top")),
                                                                Node::whileLoop(Node::binary("&&", Node::binary("!=", Node::var("link"), Node::i(0)),
                                                                                             Node::binary("!=", Node::loadFrom(Node::var("link"), Type::i64()), frame)),
                                                                                Node::assign("link", Node::loadFrom(Node::var("link"), Type::i64()))),
                                                                Node::ifThenElse(Node::binary("==", Node::var("link"), Node::i(0)), Node::ret(nullptr), nullptr),
                                                                Node::storeAt(Node::var("link"), Node::loadFrom(frame, Type::i64()), Type::i64()) })),
                                 Node::assign("callback", Node::loadFrom(slot(frame, 1), Type::i64())),
                                 Node::release(frame),
                                 Node::exprStmt(Node::callPointer(callback, {}, {})), Node::release(callback), Node::ret(nullptr) });
        program.functions.push_back(std::move(pop));
        Function panic;
        panic.name = "__magel_defer_panic";
        panic.result = LowerType(T(M_TypeVoid()), "panic");
        panic.parameters = { { "__magel_value", Type::i64() } };
        panic.body = Node::block({ Node::assign("__magel_value", Node::call("__vm_str_sub", { "string", "start", "end" },
                                                                            { Node::var("__magel_value"), Node::i(0), Node::i(-1) })),
                                   Node::whileLoop(Node::binary("!=", Node::var("__magel_defer_top"), Node::i(0)),
                                                   Node::exprStmt(Node::call("__magel_defer_pop", { "frame" }, { Node::var("__magel_defer_top") }))),
                                   Node::exprStmt(Node::call("__vm_panic", { "__magel_value" }, { Node::var("__magel_value") })), Node::ret(nullptr) });
        program.functions.push_back(std::move(panic));
    }
    const TypePtr i32 = Type::i32();
    const TypePtr f64 = Type::f64();
    /* ---------------- 内存内建：逐字节操作（memcpy / memset） ----------------
     * 下面这块是 `new` / `delete` / 字符串拼接的旧实现：人已经拍板删掉，
     * 用 `#if 0` 留着当参考（别再打开：`__magel_new` 的第一次调用返回 0 的坑还没修）。 */
#if 0
#endif /* 旧实现（new / delete / strcat） */
    /* ---------------- 结构体按值拷贝 ----------------
     *
     * 8 字节槽逐个搬（按位原样），所以整型 / 浮点字段都不会失真；
     * 槽数在调用点算好写死（结构体的字段数编译期就知道）。 */
    if (g_usesStructCopy)
    {
        Function function;
        function.name = "__magel_copy_struct";
        function.result = Type::i32();
        function.parameters = { { "__magel_dst", Type::i64() }, { "__magel_src", Type::i64() }, { "__magel_slots", Type::i32() } };
        function.body = Node::block({
            Node::declare("__magel_k", Type::i32()),
            Node::assign("__magel_k", Node::i(0)),
            Node::whileLoop(
                Node::binary("<", Node::var("__magel_k"), Node::var("__magel_slots")),
                Node::block({
                    Node::storeAt(
                        Node::offset(Node::var("__magel_dst"), Node::var("__magel_k"), 8),
                        Node::loadInt(Node::offset(Node::var("__magel_src"), Node::var("__magel_k"), 8)),
                        Type::i64()),
                    Node::assign("__magel_k",
                                 Node::binary("+", Node::var("__magel_k"), Node::i(1))),
                })),
            Node::ret(Node::i(0)),
        });
        program.functions.push_back(std::move(function));
    }

    /* ---------------- 运行时数组：`new T[N]` 的分配 ----------------
     *
     * 块布局：`[元素个数 i64][第 0 个元素][第 1 个元素]…`，返回的是**元素起始**
     * （也就是"数组句柄"）。因为长度就写在块头，所以 `a[i]` 的越界检查能查到
     * 运行期才知道的 N；`delete a` 还回去的是块首（`a - 8`）。
     *
     * 句柄声明成"两个 i64 的数组"是为了让按地址取值 / 存值都按 i64 整存整取
     * （长度不能被当成浮点解释）。 */
    if (g_usesNewArray)
    {
        const TypePtr blkHandle = Type::array(Type::i64(), "count=2"); /* 按 i64 整存整取 */
        Function function;
        function.name = "__magel_new_array";
        function.result = Type::i64();
        function.parameters = { { "__magel_len", Type::i64() }, { "__magel_stride", Type::i64() } };
        function.body = Node::block({
            Node::ifThenElse(Node::binary("||",
                                          Node::binary("<", Node::var("__magel_len"), Node::cast(Node::i(0), Type::i64())),
                                          Node::binary(">", Node::var("__magel_len"),
                                                       Node::binary("/", Node::cast(Node::i(INT64_MAX - 8), Type::i64()), Node::var("__magel_stride")))),
                             Node::block({ Node::exprStmt(Node::call("__vm_panic", { "message" },
                                                                     { Node::str("数组长度为负数或分配字节数溢出") })),
                                           Node::ret(Node::i(0)) }),
                             nullptr),
            Node::declare("__magel_blk", blkHandle),
            Node::assign("__magel_blk",
                         Node::alloc(Node::binary("+", Node::i(8),
                                                  Node::binary("*", Node::var("__magel_len"),
                                                               Node::var("__magel_stride"))))),
            Node::exprStmt(Node::call("__vm_memset", { "__magel_addr", "__magel_val", "__magel_n" },
                                      { Node::var("__magel_blk"), Node::i(0), Node::binary("+", Node::i(8), Node::binary("*", Node::var("__magel_len"), Node::var("__magel_stride"))) })),
            /* 块头写下元素个数（下标越界检查读的就是它） */
            Node::assign(Node::offset(Node::var("__magel_blk"), Node::i(0), 8),
                         Node::var("__magel_len")),
            /* 句柄 = 块首 + 8（元素起始） */
            Node::ret(Node::binary("+", Node::var("__magel_blk"), Node::i(8))),
        });
        program.functions.push_back(std::move(function));
    }
}
