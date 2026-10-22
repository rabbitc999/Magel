/* ---------- 类型号（`__typeTag` 与 `obj` 共用） ---------- */

/**
 * @brief 请求生成普通泛型实例，由编译入口保存解析快照并重放语义阶段。
 */
struct MInstantiationRequest
{
    Memory::String name;
    Memory::Vector<Memory::String> args;
};

/**
 * @brief 宏新增闭包声明后请求重新登记程序，不执行或生成半成品。
 */
struct MMacroSemanticReplay
{
};

/**
 * @brief 提升宏新生成的闭包；存在新声明时请求重放语义登记。
 * @return 提升检查通过时返回真。
 */
bool PrepareExpandedMacroLambdas();
/**
 * @brief 为已被类型推导选中的实参提升闭包，不提升其他语法候选。
 * @param root 实参表达式根节点。
 * @return 所需闭包声明已登记时返回真。
 */
bool PrepareMacroArgumentLambdas(int root);

/**
 * @brief 在模板已经展开后解析其类型应用，保留未选中语法候选的语义隔离。
 * @param type 当前类型描述。
 * @param depth 类型树递归深度。
 * @return 类型应用已解析或本来就是普通类型时返回真。
 */
bool ResolveAstGenericType(const std::shared_ptr<MType> &type, std::size_t depth = 0);

/**
 * @brief 在静态类型上下文已建立后展开泛型表达式宏。
 * @param id 待处理的表达式。
 * @param expected 调用处期望结果类型；空表示没有结果约束。
 * @param templateRoot 非空时接收完整模板，由语句列表直接拼接。
 * @return 节点已成功转换为普通表达式时返回真。
 */
bool ExpandDeferredAstMacro(int id, const std::shared_ptr<MType> &expected, int *templateRoot = nullptr);

/**
 * @brief 造一个光秃秃的内建类型（不往类型池里放，纯粹为了算类型号）。
 * @param kind 待处理对象的类别。
 * @param isUnsigned 是否使用无符号数值类型。
 * @param bits 数值类型的位宽。
 * @retval 对应的类型描述对象。
 */
std::shared_ptr<MType> SimpleType(MType::Kind kind, bool isUnsigned = false, int bits = 64)
{
    auto type = Memory::makeShared<MType>();
    type->kind = kind;
    type->isUnsigned = isUnsigned;
    type->bits = bits;
    return type;
}

/**
 * @brief 造一个引用类型（`elem` 决定指向什么，`readOnly` = `const&` / `mut&`）。
 * @param elem 数组元素或指针目标的类型。
 * @param readOnly 是否使用只读指针访问。
 * @retval 对应的类型描述对象。
 */
std::shared_ptr<MType> PointerType(const std::shared_ptr<MType> &elem, bool readOnly)
{
    auto type = Memory::makeShared<MType>();
    type->kind = MType::Ptr;
    type->elem = elem;
    type->readOnly = readOnly;
    return type;
}

/**
 * @brief 按名字造一个结构体 / 接口类型（算类型号与建引用时用）。
 * @param name 名称文本。
 * @retval 对应的类型描述对象。
 */
std::shared_ptr<MType> NamedType(const Memory::String &name)
{
    auto type = Memory::makeShared<MType>();
    type->kind = MType::Struct;
    type->name = name;
    auto opcode = g_opcodeTypes.find(name);
    if (opcode == g_opcodeTypes.end() && name.find("::") != Memory::String::npos)
        opcode = g_opcodeTypes.find(name.substr(name.rfind("::") + 2));
    if (opcode != g_opcodeTypes.end())
    {
        type->kind = opcode->second.aggregate ? MType::Struct : MType::Opcode;
        type->name = opcode->first;
    }
    return type;
}

/**
 * @brief 类型的**规范写法**：同一个类型永远得到同一个串。
 *
 * 类型号就是拿它去查表的键，所以这里必须把"看起来一样、其实不是一回事"的分开写：
 * `int64` 与 `uint64`、`T const&` 与 `T mut&`、`T[3]` 与运行时长度数组 `T[]`、
 * `std::lang::List<int64>` 与 `std::lang::List<float64>`……都在串里能分出来。
 */
Memory::String TypeKey(const std::shared_ptr<MType> &type);

/**
 * @brief 类型号：查表，没有就接着往下发号。
 * @param type 类型描述。
 * @retval 类型号：查表，没有就接着往下发号。
 */
std::int64_t TypeIdOf(const std::shared_ptr<MType> &type)
{
    const Memory::String key = TypeKey(type);
    const auto found = g_typeIdIndex.find(key);
    if (found != g_typeIdIndex.end()) return found->second;
    if (g_nextTypeId >= kMaxTypeId)
    {
        /* 回绕比报错危险得多：号变成负数之后 `__typeTag` 出负数、`obj` 判等错乱、
         * 接口分派错位，全是静默的错误行为。这里直接停下。 */
        Fail("类型号用尽：已经发到 64 位上界（" + Memory::toString(kMaxTypeId) + "）。这个程序太大 / 泛型特化太多：减少类型特化，"
                                                                                 "或者把它拆成几个程序分别编译");
        return 0; /* 0 是保留的"没有类型"号；编译已经失败 */
    }
    const std::int64_t id = g_nextTypeId++;
    g_typeIdIndex[key] = id;
    return id;
}

/**
 * @brief 是不是**接口名**。
 *
 * 接口和结构体在类型系统里都写成 `MType::Struct`（都只写个名字），靠查表分开：
 * 名字落在 `g_interfaces` 里的就是接口。接口**不能当值类型**（它只描述函数、
 * 没有字段也没有大小），只能出现在引用的目标位置（那是"胖引用"）。
 */
bool IsInterfaceType(const std::shared_ptr<MType> &type)
{
    return type && type->kind == MType::Struct && g_interfaces.count(type->name) != 0u;
}

/**
 * @brief 判断类型描述是否为枚举类型。
 * @param type 类型描述。
 * @retval 类型描述为枚举类型时为 true，否则为 false。
 */
bool IsEnumType(const std::shared_ptr<MType> &type)
{
    return type && type->kind == MType::Struct && g_enums.count(type->name) != 0u;
}

const MEnumVariantDef *FindEnumVariant(const Memory::String &enumName,
                                       const Memory::String &variantName);

/**
 * @brief 查找 Result 类型的成功与错误变体。
 * @param type 类型描述。
 * @param ok 非空时接收操作是否成功的标志。
 * @param err 接收错误变体定义的输出指针。
 * @retval 满足检查条件或操作完成时为 true，检查失败或无法完成时为 false。
 */
bool ResultVariants(const std::shared_ptr<MType> &type, const MEnumVariantDef **ok,
                    const MEnumVariantDef **err)
{
    if (!IsEnumType(type) || type->name.rfind("Result__", 0u) != 0u) return false;
    const MEnumVariantDef *okVariant = FindEnumVariant(type->name, "Ok");
    const MEnumVariantDef *errVariant = FindEnumVariant(type->name, "Err");
    if (okVariant == nullptr || errVariant == nullptr || okVariant->payloads.size() != 1u || errVariant->payloads.size() != 1u)
        return false;
    if (ok != nullptr) *ok = okVariant;
    if (err != nullptr) *err = errVariant;
    return true;
}

/**
 * @brief 查找 Option 类型的有值与空值变体。
 * @param type 类型描述。
 * @param value 待转换、保存或参与计算的值。
 * @param none 接收空值变体定义的输出指针。
 * @retval 满足检查条件或操作完成时为 true，检查失败或无法完成时为 false。
 */
bool OptionVariants(const std::shared_ptr<MType> &type, const MEnumVariantDef **value,
                    const MEnumVariantDef **none)
{
    if (!IsEnumType(type) || type->name.rfind("Option__", 0u) != 0u) return false;
    const MEnumVariantDef *valueVariant = FindEnumVariant(type->name, "Value");
    const MEnumVariantDef *noneVariant = FindEnumVariant(type->name, "None");
    if (valueVariant == nullptr || noneVariant == nullptr || valueVariant->payloads.size() != 1u || !noneVariant->payloads.empty())
        return false;
    if (value != nullptr) *value = valueVariant;
    if (none != nullptr) *none = noneVariant;
    return true;
}

/**
 * @brief 按名称查找枚举变体定义。
 * @param enumName 枚举类型的名称。
 * @param variantName 枚举变体的名称。
 * @retval 对应对象的指针。
 */
const MEnumVariantDef *FindEnumVariant(const Memory::String &enumName,
                                       const Memory::String &variantName)
{
    const auto definition = g_enums.find(enumName);
    if (definition == g_enums.end()) return nullptr;
    for (const MEnumVariantDef &variant : definition->second.variants)
        if (variant.name == variantName) return &variant;
    return nullptr;
}

/**
 * @brief 为枚举载荷参数生成内部名称。
 * @param enumName 枚举类型的名称。
 * @param variantName 枚举变体的名称。
 * @param index 待访问的元素索引。
 * @retval 生成或读取的结果文本。
 */
Memory::String EnumPayloadParameterName(const Memory::String &enumName,
                                        const Memory::String &variantName,
                                        std::size_t index)
{
    return "__chali_enum_arg_" + enumName + "_" + variantName + "_" + Memory::toString(index);
}

/**
 * @brief 是不是**接口引用**（`Shape const&` / `Shape mut&`）：值是"地址 + 类型号"的胖引用。
 * @param type 类型描述。
 * @retval 满足检查条件或操作完成时为 true，检查失败或无法完成时为 false。
 */
bool IsInterfacePointer(const std::shared_ptr<MType> &type)
{
    return type && type->kind == MType::Ptr && IsInterfaceType(type->elem);
}

/**
 * @brief 接口 `iface` 是不是就是 `base`，或者**经继承链**能到 `base`。
 *
 * `interface Derived : Base` 之后，实现 `Derived` 的结构体也实现了 `Base`，
 * 所以接口引用（`Base mut&`）也要认得它。
 */
bool InterfaceIsOrExtends(const Memory::String &iface, const Memory::String &base)
{
    if (iface == base) return true;
    Memory::Vector<Memory::String> pending;
    pending.push_back(iface);
    Memory::Set<Memory::String> seen;
    while (!pending.empty())
    {
        const Memory::String current = pending.back();
        pending.pop_back();
        if (!seen.insert(current).second) continue;
        const auto found = g_interfaceBases.find(current);
        if (found == g_interfaceBases.end()) continue;
        for (const Memory::String &next : found->second)
        {
            if (next == base) return true;
            pending.push_back(next);
        }
    }
    return false;
}

/**
 * @brief 某个结构体是不是（直接或经接口继承）实现了某个接口。
 * @param owner 所属对象或类型信息。
 * @param iface 待查找或校验的接口名称。
 * @retval 满足检查条件或操作完成时为 true，检查失败或无法完成时为 false。
 */
bool ImplementsInterface(const Memory::String &owner, const Memory::String &iface)
{
    const auto found = g_structs.find(owner);
    if (found == g_structs.end()) return false;
    for (const Memory::String &declared : found->second.interfaces)
        if (InterfaceIsOrExtends(declared, iface)) return true;
    return false;
}

/**
 * @brief 结构体是不是实现了某个接口 —— 同时看 `struct X : I` 声明与胶水 `impl X : I = 符号`。
 *
 * 胶水声明不写进 `g_structs[X].interfaces`，所以只查 `ImplementsInterface` 会漏掉
 * 宿主实现的 `Drop` / `Closeable`。
 */
bool StructHasInterface(const Memory::String &owner, const Memory::String &iface)
{
    if (ImplementsInterface(owner, iface)) return true;
    for (const MHostInterfaceImpl &implementation : g_hostInterfaceImpls)
    {
        if (!InterfaceIsOrExtends(implementation.interfaceName, iface)) continue;
        if (implementation.owner && implementation.owner->kind == MType::Struct && implementation.owner->name == owner)
            return true;
    }
    return false;
}

/**
 * @brief 接口方法的**分派函数**名（编译器生成，`__chali_` 前缀用户用不了）。
 * @param iface 待查找或校验的接口名称。
 * @param method 压缩方法编号或目标方法名称。
 * @retval 生成或读取的结果文本。
 */
Memory::String InterfaceLabel(const Memory::String &iface, const Memory::String &method)
{
    return "__chali_iface." + iface + "." + method;
}

/**
 * @brief 接口方法在"被调用过"的名单里登记一下（收尾时给它生成分派函数）。
 * @param iface 待查找或校验的接口名称。
 * @param method 压缩方法编号或目标方法名称。
 */
void MarkInterfaceMethodUsed(const Memory::String &iface, const Memory::String &method)
{
    g_usedInterfaceMethods.insert(iface + "|" + method);
}

/**
 * @brief 收集一条独立诊断并去重。
 * @param message 消息。
 * @param line 行号。
 * @param column 列号。
 * @return 无。
 */
void AddDiagnostic(const Memory::String &message, std::size_t line, std::size_t column = 0)
{
    // 诊断在 VM 内存池销毁后仍需输出，字符串副本必须归宿主长期分配器所有。
    Memory::AllocationScope persistent(Memory::defaultRuntimeAllocator());
    Memory::String detailed = message;
    int frame = g_macroDiagnosticFrame;
    for (std::size_t depth = 0; frame >= 0 && depth < 256 && static_cast<std::size_t>(frame) < g_nodes.size(); ++depth)
    {
        const auto &origin = N(frame);
        if (origin.kind != MNode::MacroExpansionFrame) break;
        const auto location = [&](std::size_t sourceLine, std::size_t sourceColumn) {
            const auto file = sourceLine && sourceLine <= g_sourceFiles.size() && !g_sourceFiles[sourceLine - 1].empty() ? g_sourceFiles[sourceLine - 1] : g_sourceFallback;
            const auto originalLine = sourceLine && sourceLine <= g_sourceLines.size() && g_sourceLines[sourceLine - 1] ? g_sourceLines[sourceLine - 1] : sourceLine;
            return file + ":" + Memory::toString(originalLine) + ":" + Memory::toString(sourceColumn);
        };
        detailed += "\n  展开宏 $" + origin.s + "，调用于 " + location(origin.sourceLine, origin.sourceColumn);
        if (origin.i >= 0 && static_cast<std::size_t>(origin.i) < g_nodes.size())
            detailed += "，定义于 " + location(N(static_cast<int>(origin.i)).sourceLine, N(static_cast<int>(origin.i)).sourceColumn);
        frame = origin.kids.empty() ? -1 : origin.kids[0];
    }
    for (const auto &item : g_diagnostics)
        if (item.message == detailed && item.line == line && item.column == column) return;
    if (g_diagnostics.size() < 100u) g_diagnostics.push_back({ detailed, line, column });
}
/**
 * @brief 降级 / 登记时把「当前源码位置」恢复到某个语法节点上。
 *
 * 语义检查（pass1 登记、pass2 降级）是在**全部 token 都读完以后**跑的：这时
 * `g_reductionLine` / `g_reductionColumn` 还停在整个文件的最后一个 token 上，直接拿
 * 它会把出错的句子归到一个无关的行。给每一处"正在处理哪个节点"套一层
 * 这个作用域，报错的行列就跟着节点走。
 */
struct ReductionSourceScope
{
    std::size_t previous;
    std::size_t previousColumn;
    int previousMacroFrame;
    /**
     * @brief 保存并切换当前归约节点的源码位置。
     * @param id 待查找或操作的对象编号。
     */
    explicit ReductionSourceScope(int id) : previous(g_reductionLine), previousColumn(g_reductionColumn), previousMacroFrame(g_macroDiagnosticFrame)
    {
        if (id >= 0)
        {
            g_reductionLine = N(id).expansionLine ? N(id).expansionLine : N(id).sourceLine;
            g_reductionColumn = N(id).expansionLine ? N(id).expansionColumn : N(id).sourceColumn;
            g_macroDiagnosticFrame = N(id).macroExpansion;
        }
    }
    /**
     * @brief 恢复进入归约节点前的源码位置。
     */
    ~ReductionSourceScope()
    {
        g_reductionLine = previous;
        g_reductionColumn = previousColumn;
        g_macroDiagnosticFrame = previousMacroFrame;
    }
};

/**
 * @brief 开始下一个可独立恢复的语句或函数。
 * @return 无。
 */
void ClearActiveError()
{
    g_error.clear();
    g_errorLine = g_errorColumn = 0;
}
/**
 * @brief 恢复首条错误供兼容接口和编译失败返回使用。
 * @return 无。
 */
void RestoreFirstDiagnostic()
{
    if (g_diagnostics.empty()) return;
    g_error = g_diagnostics.front().message;
    g_errorLine = g_diagnostics.front().line;
    g_errorColumn = g_diagnostics.front().column;
}
/**
 * @brief 保存当前语句的首条错误，后续语句可继续检查。
 * @param message 诊断消息。
 * @return 始终为 false。
 */
bool Fail(const Memory::String &message)
{
    if (g_error.empty())
    {
        g_error.assign(message.data(), message.size());
        g_errorLine = g_reductionLine;
        /* 只有注解错误会显式给列（`g_errorColumn`），其余诊断的列用「最近一个 token」
         * 兜上；降级期 `ReductionSourceScope` 会把行与列一起恢复到正在降级的那个节点，
         * 所以这里落下的行列指向同一处源码。 */
        AddDiagnostic(message, g_errorLine, g_errorColumn ? g_errorColumn : g_reductionColumn);
    }
    return false;
}

/**
 * @brief 一条提示（走 stderr，**不影响编译结果**）。
 *
 * 现在用在"函数重复定义、后面的覆盖前面的"上 —— 这是允许的，但值得说一声。
 * @param message 消息。
 */
void Note(const Memory::String &message)
{
    std::fprintf(stderr, "chali: 提示：%s\n", message.c_str());
}

/**
 * 标签列表拼成一句（报错文案用）。
 */
Memory::String JoinLabels(const Memory::Vector<Memory::String> &labels);

/**
 * 结构体作为数组元素时的字节步长（字段数 × 8）；不是结构体返回 0。
 */
int StructStride(const std::shared_ptr<MType> &type);

/**
 * 堆上一个元素占多少字节（结构体 = 字段数 × 8，其余 8 字节槽）。
 */
int ElementBytes(const std::shared_ptr<MType> &type);

/**
 * 类型对应的结构体 / 枚举存储体名；不是返回空串。
 */
Memory::String StructNameOf(const std::shared_ptr<MType> &type);

/**
 * 两个前端类型是不是同一个（实现见下）。
 */
bool SameType(const std::shared_ptr<MType> &left, const std::shared_ptr<MType> &right);
bool Acceptable(const std::shared_ptr<MType> &param, const std::shared_ptr<MType> &argument,
                bool widening);
/**
 * @brief 查询已声明的直接转换，不进行隐式转换链推导。
 */
bool HasDeclaredConversion(const std::shared_ptr<MType> &source,
                           const std::shared_ptr<MType> &target, bool allowExplicit);
bool HasImplicitConversion(const std::shared_ptr<MType> &source,
                           const std::shared_ptr<MType> &target);
simple_ast::NodePtr LowerDeclaredConversion(int valueId,
                                            const std::shared_ptr<MType> &target,
                                            bool allowExplicit);
std::shared_ptr<MType> ExprType(int id);
bool InferAutoFunctionResult(int statementId, std::shared_ptr<MType> *result);
/**
 * @brief 查询语言类型对应的 opcode 生命周期声明。
 * @param type 待查询的语言类型。
 * @return 对应声明，无声明时为空。
 */
const MOpcodeDefinition *FindOpcodeDefinition(const std::shared_ptr<MType> &type)
{
    if (!type || type->kind != MType::Opcode || IsContainer(type)) return nullptr;
    const auto found = g_opcodeTypes.find(type->name);
    return found == g_opcodeTypes.end() ? nullptr : &found->second;
}
/**
 * @brief 查询 opcode 特殊结构体的声明，包括具有普通字段的聚合表示。
 * @param type 待查询的语言类型。
 * @return 对应的生命周期及布局声明，没有声明时为空。
 */
const MOpcodeDefinition *FindOpcodeDeclaration(const std::shared_ptr<MType> &type)
{
    if (!type || IsContainer(type) || (type->kind != MType::Opcode && type->kind != MType::Struct)) return nullptr;
    const auto found = g_opcodeTypes.find(type->name);
    return found == g_opcodeTypes.end() ? nullptr : &found->second;
}

/**
 * @brief 查询一个直接对象类型是否禁止复制。
 * @param type 待检查类型。
 * @return opcode 声明显式禁止复制时为 true。
 */
bool OpcodeNonCopyable(const std::shared_ptr<MType> &type)
{
    const auto *definition = FindOpcodeDefinition(type);
    return definition && definition->nonCopyable;
}

/**
 * @brief 查询一个直接对象类型是否禁止值复制。
 *
 * 两类来源：`#NonCopyable opcode`（宿主句柄）与实现 `Closeable`（含经接口继承）的
 * 结构体 —— 后者持有外部句柄，复制一份就等于两份所有权，`close()` 会跑两遍。
 * `move`、返回局部拥有值与借用不受影响。
 *
 * @param type 待检查类型。
 * @return 禁止按值复制时返回 true。
 */
bool TypeNonCopyable(const std::shared_ptr<MType> &type)
{
    if (OpcodeNonCopyable(type)) return true;
    if (type && type->kind == MType::Struct)
    {
        const MStructDef *definition = FindStruct(type->name);
        return definition != nullptr && definition->nonCopyable;
    }
    return false;
}

/**
 * @brief 函数值复制兼容静态标签与拥有型捕获环境，其他对象沿用对象表复制。
 */
const char *ObjectCopyFunction(const std::shared_ptr<MType> &type)
{
    if (const auto *definition = FindOpcodeDefinition(type); definition && !definition->copy.empty())
        return definition->copy.c_str();
    return type && type->kind == MType::Func ? "__std_function_copy" : "__std_object_copy";
}
std::shared_ptr<MType> RawExprType(int id);
/**
 * @brief 计算引用及含引用值的来源作用域。
 * @param id 表达式。
 * @param address 是否取得对象地址。
 * @return 来源深度。
 */
std::size_t BorrowSourceDepth(int id, bool address = false);
/**
 * @brief 记录含引用写入的来源与目标寿命。
 * @param target 目标表达式。
 * @param value 来源表达式。
 * @param container 是否写入容器元素。
 * @return 无；仅记录事实。
 */
void RecordBorrowStore(int target, int value, bool container = false);
/**
 * @brief 保存表达式的借用绑定事实。
 * @param id 表达式。
 * @param type 已解析类型。
 * @return 无。
 */
void CaptureBorrowExpression(int id, const std::shared_ptr<MType> &type);
/**
 * @brief 判断是否只经 Any 隐藏借用类型。
 * @param type 形参类型。
 * @return 无显式引用而可能含装箱借用时为 true。
 */
bool HasOpaqueObjectBorrow(const std::shared_ptr<MType> &type);
/**
 * @brief 记录隐藏借用的按值实参。
 * @param id 实参。
 * @param type 形参类型。
 * @return 无；仅记录事实。
 */
void RecordObjectArgumentBorrow(int id, const std::shared_ptr<MType> &type);
bool AutoDereferences(int id);
simple_ast::NodePtr LowerRawExpr(int id);
Memory::String UnescapeText(const Memory::String &raw);
bool CheckUserName(const Memory::String &name);
/**
 * 按名字找变量（从最内层作用域往外找），见下面的实现。
 */
MVar *Find(const Memory::String &name);
/**
 * 模块感知的变量解析：局部作用域按裸名，全局先看 `模块::名`，再兜底裸名（根模块 / prelude）。
 */
MVar *FindIn(const Memory::String &module, const Memory::String &name, bool globalOnly = false);
/**
 * @brief 按节点携带的卫生身份查找变量，不把宏辅助名称绑定到调用者。
 * @param node 变量或声明节点。
 * @return 对应变量，不存在时为空。
 */
MVar *FindNodeVar(const MNode &node);
/**
 * @brief 更新宏展开后实际可达的节点和类型，隔离未选中的语法候选。
 */
void RefreshMacroActiveSyntax();
/**
 * 只认**模块限定全局**（`模块::名`），不兜底裸名 —— 给 `mod::x` 这种限定写法用。
 */
MVar *FindQualifiedGlobal(const Memory::String &module, const Memory::String &name);
/**
 * 模块感知的函数重载解析（先 `模块::名`，再根模块裸名）；找不到返回 nullptr。
 */
const Memory::Vector<Memory::String> *FindFunctionOverloads(const Memory::String &module,
                                                            const Memory::String &name);
/**
 * 模块感知的 auto 推导函数表查找；找不到返回 nullptr。
 */
const MFuncSig *FindInferenceFunction(const Memory::String &module, const Memory::String &name);

/**
 * @brief 这次调用的"被调者"是不是**函数指针**（而不是函数名）。
 *
 * `fp(3, 4)`（fp 是变量 / 字段）走间接调用；`add(3, 4)`（add 是函数名）走直接调用 ——
 * 名字被函数值变量遮住时（`fn(int64) -> int64 add`）也算间接。
 * @param calleeId 被调者节点。
 * @return 是函数指针对返回 true。
 */
bool IsIndirectCallee(int calleeId)
{
    const MNode &callee = N(calleeId);
    if (callee.kind == MNode::Var && callee.s.rfind("__chali_lambda_", 0) == 0) return true;
    if ((callee.kind == MNode::Var) && (FindNodeVar(callee) == nullptr) && (FindFunctionOverloads(callee.module, callee.s) != nullptr))
    {
        return false; /* 函数名：直接调用 */
    }
    return ExprType(calleeId)->kind == MType::Func;
}

/**
 * @brief 从函数声明造"函数指针类型"（返回类型 + 形参类型表）。
 * @param signature 函数签名。
 * @return 函数指针类型。
 */
std::shared_ptr<MType> FuncTypeOf(const MFuncSig &signature)
{
    auto type = Memory::makeShared<MType>();
    type->kind = MType::Func;
    type->elem = signature.result;
    for (const MParam &param : signature.params) type->paramTypes.push_back(param.type);
    type->variadic = !signature.params.empty() && signature.params.back().variadic;
    return type;
}

/**
 * @brief 访问元组或载荷模式中的标识符叶子，忽略表达式及构造器头。
 * @param id 绑定模式根。
 * @param action 每个绑定叶子的处理函数。
 * @param depth 当前模式深度。
 */
void VisitAstPatternBindings(int id, const Memory::Function<void(int)> &action, std::size_t depth = 0)
{
    if (id < 0 || depth >= 256) return;
    const auto &node = N(id);
    if (node.kind == MNode::Var)
        action(id);
    else if (node.kind == MNode::List || node.kind == MNode::Tuple)
        for (const int child : node.kids) VisitAstPatternBindings(child, action, depth + 1);
}

/**
 * @brief 访问声明解构和枚举分支的局部绑定，不把标量模式名称当成声明。
 * @param node 当前结构节点。
 * @param action 每个局部模式声明的处理函数。
 */
void VisitAstLocalPatternBindings(const MNode &node, const Memory::Function<void(int)> &action)
{
    if (node.kind == MNode::Destructure && !node.i) VisitAstPatternBindings(node.kids[0], action);
    if (node.kind != MNode::Case || node.i < 0) return;
    const auto pattern = N(static_cast<int>(node.i));
    if (pattern.kind == MNode::EnumCtor || pattern.kind == MNode::TypeQualifiedRef)
        for (const int payload : pattern.kids) VisitAstPatternBindings(payload, action);
    else if (pattern.kind == MNode::Call && pattern.kids.size() == 2)
        VisitAstPatternBindings(pattern.kids[1], action);
}

/**
 * @brief AST 泛型闭包的原始模板、自由引用和具体实现，不存储展开后的源码。
 */
struct MAstGenericClosure
{
    int syntax = -1;
    Memory::Vector<int> references;
};
Memory::Map<Memory::String, MAstGenericClosure> g_astGenericClosures;
Memory::Map<Memory::String, Memory::String> g_astGenericImplementations;
/**
 * @brief 将可达的泛型闭包 AST 登记为具有独立环境入口的模板。
 * @param id 原始泛型闭包节点。
 * @return 新建的环境入口函数节点。
 */
int LiftAstGenericClosure(int id);
/**
 * @brief 从具体参数实例化 AST 泛型闭包并保留同一捕获环境。
 * @param id 调用节点。
 * @param seed 泛型闭包环境入口。
 * @return 该调用处理成功时返回真。
 */
bool ResolveAstGenericClosureCall(int id, const Memory::String &seed);

/**
 * @brief 解析闭包的声明身份，并为外层泛型实例生成独立的环境类型。
 * @param name 原闭包入口或其具体外层实例名称。
 * @return 相关声明；普通类型与函数返回空指针。
 */
const chali_lambda::GenericClosure *FindGenericClosureDefinition(const Memory::String &name)
{
    const auto found = chali_lambda::genericClosures.find(name);
    if (found != chali_lambda::genericClosures.end()) return &found->second;
    const auto expansion = chali_generic::session.made.find(name);
    if (expansion == chali_generic::session.made.end()) return nullptr;
    const auto source = chali_lambda::genericClosures.find(expansion->second.generic->name);
    if (source == chali_lambda::genericClosures.end() || source->second.outerNames.size() != expansion->second.args.size()) return nullptr;
    auto definition = source->second;
    definition.seed = name;
    definition.outerArguments = expansion->second.args;
    return &chali_lambda::genericClosures.emplace(name, std::move(definition)).first->second;
}

/**
 * @brief 构造保留泛型声明身份的闭包类型，运行时仍使用普通闭包句柄。
 * @param seed 环境构造入口的唯一名称。
 * @return 泛型闭包类型。
 */
std::shared_ptr<MType> GenericClosureType(const Memory::String &seed)
{
    auto type = Memory::makeShared<MType>();
    type->kind = MType::Func;
    type->name = seed;
    type->elem = T(M_TypeVoid());
    return type;
}

/**
 * @brief 从实参推导泛型闭包并选择唯一最具体的实现。
 * @param id 调用表达式节点。
 * @return 被调用者属于泛型闭包时返回 true，包括已报告的错误。
 */
bool ResolveGenericClosureCall(int id);

/**
 * @brief 函数名当值用（函数指针）：挑出该用哪个重载的标签。
 *
 * 有目标签名时始终精确匹配，包括只有一个候选的情况。
 * 无目标签名时仅允许唯一候选；多个重载不能任意选择。
 * @param module 函数名所在模块（模块限定名优先）。
 * @param name 源函数名。
 * @param expected 期望的函数指针类型（可为空）。
 * @return 标签；失败返回空串。
 */
Memory::String ResolveFunctionValue(const Memory::String &module, const Memory::String &name,
                                    const std::shared_ptr<MType> &expected)
{
    const Memory::Vector<Memory::String> *found = FindFunctionOverloads(module, name);
    if (found == nullptr || found->empty()) return Memory::String();
    if (found->size() == 1u && expected && expected->name == name && chali_lambda::genericClosures.count(name)) return (*found)[0];
    if (found->size() == 1u && (!expected || expected->kind != MType::Func)) return (*found)[0];
    if (expected != nullptr && expected->kind == MType::Func)
    {
        for (const Memory::String &label : *found)
        {
            const auto signature = g_funcs.find(label);
            if (signature == g_funcs.end()) continue;
            if (SameType(FuncTypeOf(signature->second), expected)) return label;
        }
        Fail("函数 " + name + " 与目标函数类型不匹配");
        return Memory::String();
    }
    Fail("函数 " + name + " 有多个重载，取函数指针时用带签名的目标类型说明用哪一个");
    return Memory::String();
}

/**
 * @brief 函数指针的值：数据区里**存着函数标签那串字节**的地址。
 *
 * 栈机侧 `(callptr, 地址, _, 目标)` 就是去那儿读标签。数据区从偏移 8 开始排
 * （地址 0 留给空引用），所以任何真实的函数指针都不为 0 —— `fp == null` 才有意义。
 *
 * 同一个标签**只发一份数据项**（`Node::namedStr` + 这里的缓存），所以"同一个函数取址"
 * 每次拿到的地址都一样，`p == 函数名` 可以按地址比。
 * @param label 函数标签（降级后的名字）。
 * @return 表达式节点（值是地址）。
 */
simple_ast::NodePtr FunctionPointerValue(const Memory::String &label)
{
    static Memory::Map<Memory::String, simple_ast::NodePtr> cache;
    const auto found = cache.find(label);
    if (found != cache.end()) return found->second;
    simple_ast::NodePtr value = simple_ast::Node::namedStr("fn_" + label, label);
    cache[label] = value;
    return value;
}

/**
 * 正在初始化 / 赋值的函数类型：`fn(int64) -> int64 fp = add;` 里挑重载要用它。
 * 由 LowerLocalDecl / LowerGlobals / LowerAssign 在求值初始化表达式前后临时设置。
 */
bool OwnedPropagation(int id);
std::shared_ptr<MType> g_expectedFuncType;

/**
 * 正在初始化 / 赋值的**目标类型**：`T mut& p = new;` / `T a[] = new [n];` 里
 * `new` 的元素类型从这儿取（所以 `new` 只能出现在声明 / 赋值里）。
 */
std::shared_ptr<MType> g_expectedNewType;

/**
 * @brief 在目标类型明确时补齐枚举限定名及泛型调用的类型实参。
 */
void InferExpression(int id, const std::shared_ptr<MType> &expected = nullptr);
std::shared_ptr<MType> InferCollectionLiteral(int id, const std::shared_ptr<MType> &expected);

/**
 * @brief 当前这条语句"可以插在前面执行的语句"收集处（临时串落地用）。
 *
 * 表达式里**当场造出来的堆串**（拼出来的、切出来的、`format` 出来的、函数返回的）
 * 在"它是不是某个变量的值"这件事上没人管 —— 不落进一个归**当前块所有**的临时变量，
 * 就没人释放（循环 / 递归里一直涨）。语句级别（`x = …` / `return …` / `std::lang::print(…)`）
 * 有地方插语句，就把这些临时串先算出来、登记成当前块所有；表达式级别（比如
 * `format("…", s[0:2])` 里那个切片）没法插，就只能是"留着"。
 */
Memory::Vector<simple_ast::NodePtr> *g_freshSink = nullptr;

/**
 * 当前函数的返回类型：`return 2;` 在返回 float64 的函数里要写成 `return 2.0;`
 * （隐式转换），由 LowerFunction 在降级函数体前设置。
 */
std::shared_ptr<MType> g_returnType;

/**
 * `break` / `continue` 的跳转目标栈（进入循环或值表达式时压一条）。
 *
 * `continueLabel` 为空表示这一层不接收 continue；值表达式中的 continue 继续寻找外层循环。
 */
struct MBlockContext
{
    Memory::String breakLabel;    /* break 跳到哪儿 */
    Memory::String continueLabel; /* continue 跳到哪儿（空 = 只能 break） */
    /**
     * 进这一层循环时 `g_ownedScopes` 有多深：`break` / `continue` 要把**比它深**
     * 的那些块里的所有者变量先释放掉（跳出去就不再回来了）。
     */
    std::size_t scopeDepth = 0;
    Memory::String breakValue; /* 值出口接收 break value 的槽；无值循环为空并允许带值退出穿透。 */
    std::shared_ptr<MType> breakType;
    bool valueOnly = false;            ///< 非循环值表达式不截获无值循环退出。
    std::size_t valueScopeDepth = 0u;  ///< 值出口的名字作用域深度。
    bool returnExpression = false;     ///< 返回表达式的内部拥有者必须在清理前消费。
};
Memory::Vector<MBlockContext> g_blockStack;
/**
 * 生成循环 / match 分支用的标签名（`__chali_loop_7` 这种，和用户标签不撞）。
 */
int g_blockSerial = 0;
/**
 * @brief 分配新的语句块标签。
 * @param kind 待处理对象的类别。
 * @retval 新生成的语句块标签。
 */
Memory::String NewBlockLabel(const Memory::String &kind)
{
    return "__chali_" + kind + "_" + Memory::toString(++g_blockSerial);
}

/**
 * @brief 生成默认文本表示的内部布局。
 * @param type 待描述的具体类型。
 * @param customRoot 是否包含根值的覆盖方法。
 * @retval 静态布局文本。
 */
Memory::String ValueTextLayout(const std::shared_ptr<MType> &type, bool customRoot = true);
/**
 * @brief 取得布局中只读自定义方法对应的调用实参。
 * @param type 具体类型。
 * @param customRoot 是否包含根值的覆盖方法。
 * @retval 函数值与存储布局组成的成对实参。
 */
Memory::Vector<simple_ast::NodePtr> ValueTextCallbacks(const std::shared_ptr<MType> &type, bool customRoot = true);
/**
 * @brief 生成不调用用户覆盖方法的默认文本转换。
 * @param valueId 源表达式。
 * @retval 拥有结果字符串的表达式。
 */
simple_ast::NodePtr LowerDefaultValueText(int valueId);
/**
 * @brief 生成优先使用用户方法的统一字符串转换。
 * @param valueId 源表达式。
 * @retval 拥有结果字符串的表达式。
 */
simple_ast::NodePtr LowerValueText(int valueId);
