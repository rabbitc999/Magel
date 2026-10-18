/* ---------- 类型 ---------- */

/**
 * @brief 前端类型 → simple_ast 类型：复合 / 数组是指向数据区的 i64 句柄，引用是数据区地址。
 * @param type 类型描述。
 * @param label 诊断或跳转使用的标签。
 * @retval 对应的类型描述对象。
 */
simple_ast::TypePtr LowerType(const std::shared_ptr<MType> &type, const Memory::String &label)
{
    using simple_ast::Type;
    if (!type) return Type::i32();
    switch (type->kind)
    {
    case MType::Void: return Type::i32();     /* simple_ast 没有 void，用 i32 占位 */
    case MType::Bool: return Type::boolean(); /* i8：0 / 1 */
    case MType::Char: return Type::make(Type::Kind::Uint, 8);
    case MType::Int:
        return type->isUnsigned ? Type::make(Type::Kind::Uint, type->bits) : Type::make(Type::Kind::Int, type->bits);
    case MType::Double: return Type::make(Type::Kind::Float, type->bits);
    case MType::String: return Type::stringType(); /* 字符串 = 堆上显式长度的 UTF-8 字节串 */
    case MType::Host:
        return LowerType(type->elem, label); /* 名义宿主类型按声明的 ABI 落槽 */
    /* `bint` 与容器：值都是**运行期对象表里的句柄**，按 i64 整存整取。 */
    case MType::BigInt:
    case MType::BigFloat:
    case MType::Container:
    case MType::File:
    case MType::Proc:
    case MType::Obj: return Type::i64();
    case MType::Struct:
    {
        if (IsTupleType(type) && g_structType.count(type->name) == 0u)
            EnsureTupleLayout(type);
        const auto found = g_structType.find(type->name);
        /* 接口名不在 `g_structType` 里（它没有字段、没有大小）→ 退化成 i64：
         * 接口名只当引用的目标类型用，而接口引用的值是胖引用（对象表句柄）。 */
        return (found == g_structType.end()) ? Type::i64() : found->second;
    }
    case MType::Ptr:
        /* 引用就是"数据区里的地址"，用 simple_ast 的可空引用承载：quadType = i64。
         * **接口引用**的值是胖引用（对象表里的句柄），同样是 i64。 */
        {
            auto reference = Type::ref(LowerType(type->elem, label));
            reference->meta = "nonnull";
            return reference;
        }
    case MType::Func:
        /* 函数指针 = "数据区里存着函数标签的那串字节"的地址：也是个 i64 句柄。 */
        return Type::i64();
    case MType::Array:
    {
        int count = 1;
        for (const int dim : type->dims) count *= dim;
        /* 元素是结构体：数据区里按字段铺开（每个元素占"字段数 × 8"字节），
         * 所以 count 要乘上字段数 —— 与下标用的 stride（StructStride）对齐。 */
        const int stride = StructStride(type->elem);
        if (stride > 0) count *= (stride / 8);
        const Memory::String meta = "count=" + Memory::toString(count) + ";align=8;label=" + (label.empty() ? Memory::String("array") : label);
        return Type::array(LowerType(type->elem, label), meta);
    }
    }
    return Type::i32();
}

/**
 * @brief 根据元素类型及维度构造数组类型。
 * @param elem 数组元素或指针目标的类型。
 * @param dims 数组维度节点列表的编号。
 * @retval 对应的类型描述对象。
 */
std::shared_ptr<MType> ArrayType(const std::shared_ptr<MType> &elem, const Memory::Vector<int> &dims)
{
    auto type = Memory::makeShared<MType>();
    type->kind = MType::Array;
    type->elem = elem;
    type->dims = dims;
    return type;
}

/**
 * @brief `T[]`：运行时长度数组（维度为空 = 长度不写在类型里，跟着对象走）。
 * @param elem 元素类型。
 * @return 类型。
 */
std::shared_ptr<MType> RuntimeArrayType(const std::shared_ptr<MType> &elem)
{
    return ArrayType(elem, Memory::Vector<int>());
}

/**
 * @brief 是不是"运行时长度数组"（`T[]`；静态数组 `T[N]` 的 dims 非空）。
 * @param type 类型描述。
 * @retval 满足检查条件或操作完成时为 true，检查失败或无法完成时为 false。
 */
bool IsRuntimeArray(const std::shared_ptr<MType> &type)
{
    return type && type->kind == MType::Array && type->dims.empty();
}

/**
 * @brief 取 `struct 名` 的字段类型；没有这个字段返回空。
 * @param owner 所属对象或类型信息。
 * @param field 待访问的字段名称或描述。
 * @retval 对应的类型描述对象。
 */
std::shared_ptr<MType> FieldType(const Memory::String &owner, const Memory::String &field)
{
    if (owner.empty()) return nullptr;
    const auto found = g_structs.find(owner);
    if (found == g_structs.end()) return nullptr;
    for (const MField &item : found->second.fields)
    {
        if (item.name == field) return item.type;
    }
    return nullptr;
}

/**
 * @brief 结构体作为数组元素时的字节步长（字段数 × 8）；不是结构体返回 0。
 * @param type 类型描述。
 * @retval 结构体作为数组元素时的字节步长；非结构体为 0。
 */
int StructStride(const std::shared_ptr<MType> &type)
{
    if (!type || type->kind != MType::Struct) return 0;
    const MStructDef *found = FindStruct(type->name);
    if (found == nullptr) return 0;
    /* 槽数按**摊平后**算：值字段（嵌套结构体）各占自己那几个槽（见 MStructDef）。 */
    return found->slotCount * 8;
}

/**
 * @brief 引用 / 结构体都归到"结构体名"；不是结构体相关就返回空串。
 * @param type 类型描述。
 * @retval 生成或读取的结果文本。
 */
Memory::String StructNameOf(const std::shared_ptr<MType> &type)
{
    if (!type) return Memory::String();
    if (type->kind == MType::Struct) return type->name;
    if (type->kind == MType::Ptr) return StructNameOf(type->elem);
    return Memory::String();
}

/**
 * @brief 判断初始化表达式是否提供运行时数组长度。
 *
 * Runtime arrays need a length-bearing allocation or array-returning call.
 * @param id 待查找或操作的对象编号。
 * @retval 初始化表达式提供运行时数组长度时为 true，否则为 false。
 */
bool InitIsRuntimeArray(int id)
{
    if (id < 0) return false;
    const MNode &node = N(id);
    if (node.kind == MNode::NewPtr) return node.i >= 0; /* 带长度才是数组 */
    if (node.kind == MNode::Call || node.kind == MNode::Loop || (node.kind == MNode::EnumCtor && node.type2 == 1))
        return IsRuntimeArray(ExprType(id));
    return false;
}

/**
 * @brief 一个运算符重载：顶层自由函数 `C operator+(左 left, 右 right)`。
 *
 * 左右两个操作数的类型都在这里，所以 `Money + int` 与 `int + Money` 是两个
 * 独立签名；挑选重载就是"两个实参类型和哪一对形参类型对得上"。
 */
struct MOperatorDecl
{
    Memory::String op;                        /* 运算符文本（`+` / `==` …） */
    std::shared_ptr<MType> left;              /* 左操作数类型 */
    std::shared_ptr<MType> right;             /* 右操作数类型 */
    std::shared_ptr<MType> result;            /* 返回类型 = 整个表达式的类型 */
    Memory::String label;                     /* 四元式标签 */
    Memory::Vector<Memory::String> callNames; /* self + 另一操作数，或胶水函数的两个形参 */
    bool receiverLeft = true;
};
Memory::Vector<MOperatorDecl> g_operators;

struct MConversionDecl
{
    std::shared_ptr<MType> source;
    std::shared_ptr<MType> target;
    Memory::String label;
    Memory::Vector<Memory::String> callNames;
    bool implicit = false;
};
Memory::Vector<MConversionDecl> g_conversions;

struct MIndexDecl
{
    std::shared_ptr<MType> owner;
    std::shared_ptr<MType> key;
    std::shared_ptr<MType> result;
    Memory::String label;
    Memory::Vector<Memory::String> callNames;
};
Memory::Vector<MIndexDecl> g_indexes;

struct MIteratorDecl
{
    std::shared_ptr<MType> owner;
    std::shared_ptr<MType> element;
    std::shared_ptr<MType> result;
    Memory::String label;
    Memory::Vector<Memory::String> callNames;
};
Memory::Vector<MIteratorDecl> g_iterators;

/**
 * @brief 已验证的可选 Hash<T> 协议实现。
 */
struct MHashDecl
{
    std::shared_ptr<MType> owner;
    Memory::String label;
    Memory::Vector<Memory::String> callNames;
};
Memory::Vector<MHashDecl> g_hashes;
/**
 * @brief Map 键比较包装函数；hashLabel 为空时直接按 Equal 查找。
 */
struct MMapKeyPolicy
{
    MOperatorDecl equal;
    MHashDecl hash;
    Memory::String equalLabel, hashLabel;
};
Memory::Map<Memory::String, MMapKeyPolicy> g_mapKeyPolicies;

/**
 * @brief 检查 Map 键或自定义 Set 元素的 Equal<bool,T> 协议。
 * @param type 键或元素类型。
 * @param collection 用于错误诊断的集合名称。
 * @return 合法返回 true；Hash<T> 是可选协议。
 */
bool CheckMapKey(const std::shared_ptr<MType> &type, const Memory::String &collection)
{
    if (!type) return Fail("Map 键类型无法推导");
    if (type->kind == MType::Int || type->kind == MType::Double || type->kind == MType::Bool || type->kind == MType::Char || type->kind == MType::String || type->kind == MType::Obj)
        return true;
    for (const auto &op : g_operators)
        if (op.op == "==" && SameType(op.left, type) && SameType(op.right, type) && op.result && op.result->kind == MType::Bool) return true;
    return Fail(collection + " 自定义键 " + Describe(type) + " 必须实现 Equal<bool,T> 等于重载接口；Hash<T> 哈希接口可选");
}

/**
 * @brief 查找类型的索引访问实现。
 * @param owner 所属对象或类型信息。
 * @param key 查找或写入使用的键。
 * @retval 对应对象的指针。
 */
const MIndexDecl *FindIndex(const std::shared_ptr<MType> &owner,
                            const std::shared_ptr<MType> &key)
{
    for (const MIndexDecl &index : g_indexes)
        if (SameType(index.owner, owner) && Acceptable(index.key, key, true)) return &index;
    return nullptr;
}

/**
 * @brief 查找类型的迭代器实现。
 * @param owner 所属对象或类型信息。
 * @retval 对应对象的指针。
 */
const MIteratorDecl *FindIterator(const std::shared_ptr<MType> &owner)
{
    for (const MIteratorDecl &iterator : g_iterators)
        if (SameType(iterator.owner, owner)) return &iterator;
    return nullptr;
}

struct MOperatorInterfaceSpec
{
    const char *op;
    const char *leftInterface;
    const char *rightInterface;
    const char *leftMethod;
    const char *rightMethod;
    /**
     * 一元运算符：只有 leftInterface / leftMethod 有意义，右边的两份留空。
     */
    bool unary = false;
    /**
     * 交换律：`a op b` 与 `b op a` 语义等价（`+` / `*` / `==` / `!=` / 位运算）。
     *
     * 类型只实现了左接口时，右操作数把该类型放右边也能算（自动交换两个操作数）；
     * 显式声明了 `RightXxx` 的类型不再走这层兜底（右向归作者自己负责），声明了
     * `NoRightXxx` 标记接口的类型则明令禁止 —— 两者都由 {@link RightDirectionOf} 判定。
     * 不对称的运算符（`-` / `/` / `%` / 大小比较 / 移位）不参与这层推导，必须显式写
     * `RightXxx`。
     */
    bool symmetric = false;
};

/**
 * @brief 取得运算符与内建接口的对应关系表。
 * @retval 按处理顺序保存的结果元素集合。
 */
const Memory::Vector<MOperatorInterfaceSpec> &OperatorInterfaceSpecs()
{
    static const Memory::Vector<MOperatorInterfaceSpec> specs = {
        /* 第 6 个字段是 `unary`、第 7 个是 `symmetric`：满足交换律的四则 / 位运算与
         * 相等比较标 true，减法 / 除法 / 取模 / 大小比较 / 移位都不满足，留空。 */
        { "+", "Add", "RightAdd", "leftAdd", "rightAdd", false, true },
        { "-", "Subtract", "RightSubtract", "leftSub", "rightSub" },
        { "*", "Multiply", "RightMultiply", "leftMul", "rightMul", false, true },
        { "/", "Divide", "RightDivide", "leftDiv", "rightDiv" },
        { "%", "Modulo", "RightModulo", "leftMod", "rightMod" },
        { "==", "Equal", "RightEqual", "leftEqual", "rightEqual", false, true },
        { "!=", "NotEqual", "RightNotEqual", "leftNotEqual", "rightNotEqual", false, true },
        { "<", "Less", "RightLess", "leftLess", "rightLess" },
        { "<=", "LessOrEqual", "RightLessOrEqual", "leftLessEqual", "rightLessEqual" },
        { ">", "Greater", "RightGreater", "leftGreater", "rightGreater" },
        { ">=", "GreaterOrEqual", "RightGreaterOrEqual", "leftGreaterEqual", "rightGreaterEqual" },
        { "&", "And", "RightAnd", "leftBitAnd", "rightBitAnd", false, true },
        { "|", "Or", "RightOr", "leftBitOr", "rightBitOr", false, true },
        { "^", "Xor", "RightXor", "leftBitXor", "rightBitXor", false, true },
        { "<<", "ShiftLeft", "RightShiftLeft", "leftShiftLeft", "rightShiftLeft" },
        { ">>", "ShiftRight", "RightShiftRight", "leftShiftRight", "rightShiftRight" },
        /* 一元：`-x` / `~x` / `+x`。接口方法的唯一参数就是操作数本身。 */
        { "-", "Negate", "", "negate", "", true },
        { "~", "BitNot", "", "bitNot", "", true },
        { "+", "Positive", "", "positive", "", true },
    };
    return specs;
}

/**
 * @brief 运算符文本是不是满足交换律（见 {@link MOperatorInterfaceSpec::symmetric}）。
 * @param op 运算符文本。
 * @return 对称返回 true；一元运算符与未知文本返回 false。
 */
bool OperatorIsSymmetric(const Memory::String &op)
{
    for (const MOperatorInterfaceSpec &spec : OperatorInterfaceSpecs())
        if (!spec.unary && op == spec.op) return spec.symmetric;
    return false;
}

/**
 * @brief 比较运算符的**对偶推导**：手上有哪个接口，就能推出哪几个写法。
 *
 * 三条规则（`op'(L,R)` 是声明自己的两个操作数）：
 *
 * - **对偶**：交换两个操作数、结果原样 —— `a < b` ⟺ `b > a`、`a >= b` ⟺ `b <= a`。
 * - **取反**：操作数原序、结果取反 —— `a >= b` = `!(a < b)`、`a != b` = `!(a == b)`。
 * - **对偶 + 取反**：`a <= b` = `!(b < a)`。
 *
 * 推导只在**直接匹配与交换律都没命中**时才用（见 FindOperator），而且换过操作数之后
 * 仍然要对得上声明的形参 —— 所以只实现 `Cmp < int64` 时，`5 > a` 能推出来（= `a < 5`），
 * `a > 5` 推不出来（那要 `int64 < Cmp`）：要么写成 `5 < a`，要么给 `Cmp` 补 `Greater`。
 *
 * 表里靠前的先试，命中即返回；同一对操作数不会两条都算。
 */
struct MOperatorDerivation
{
    const char *op;   ///< 用户写的运算符。
    const char *via;  ///< 借用的运算符声明。
    bool swap;        ///< 借用时两个操作数要不要交换。
    bool negate;      ///< 借用后结果要不要取反。
};

/**
 * @brief 取得运算符派生关系配置。
 * @retval 按处理顺序保存的结果元素集合。
 */
const Memory::Vector<MOperatorDerivation> &OperatorDerivations()
{
    static const Memory::Vector<MOperatorDerivation> derivations = {
        /* 对偶：交换两个操作数就行（`a < b` 就是 `b > a`，`a >= b` 就是 `b <= a`）。 */
        { "<", ">", true, false },
        { ">", "<", true, false },
        { "<=", ">=", true, false },
        { ">=", "<=", true, false },
        /* 取反：原序调用，结果取反（`a >= b` 就是 `!(a < b)`）。 */
        { ">=", "<", false, true },
        { "<=", ">", false, true },
        { ">", "<=", false, true },
        { "<", ">=", false, true },
        { "!=", "==", false, true },
        { "==", "!=", false, true },
        /* 对偶 + 取反：换操作数再取反（`a <= b` 就是 `!(b < a)`）。 */
        { "<=", "<", true, true },
        { ">=", ">", true, true },
        { "<", "<=", true, true },
        { ">", ">=", true, true },
    };
    return derivations;
}

/**
 * @brief `interfaceName` 本身或它（传递）继承的接口里，名字以 `prefix` 开头的那个。
 *
 * 用户接口可以继承**系统接口**（`interface Doubler : Multiply<int64,int64> { … }`），
 * 这时运算符 / 转换 / 下标 / 迭代这些能力要顺着继承链认出来 —— 只看直接接口名会漏。
 * 返回空串表示继承链上没有这样的接口。
 */
Memory::String InheritedInterfaceWithPrefix(const Memory::String &interfaceName,
                                            const char *prefix)
{
    const Memory::String want(prefix);
    Memory::Vector<Memory::String> pending;
    pending.push_back(interfaceName);
    Memory::Set<Memory::String> seen;
    while (!pending.empty())
    {
        const Memory::String current = pending.back();
        pending.pop_back();
        if (!seen.insert(current).second) continue;
        if (current.rfind(want, 0u) == 0u) return current;
        const auto found = g_interfaceBases.find(current);
        if (found == g_interfaceBases.end()) continue;
        for (const Memory::String &base : found->second) pending.push_back(base);
    }
    return Memory::String();
}

/**
 * @brief 方法 `methodName` 是不是 `interfaceName`（含继承链）上某个运算符接口的方法。
 *
 * 按**方法名**匹方向：`Add` 的方法是 `leftAdd`、`RightAdd` 的是 `rightAdd`、
 * 一元 `Negate` 的是 `negate`。这样同一个接口同时继承 `Add` 与 `RightAdd`
 * 时两边都能认出来，也不会把接口里别的普通方法当成运算符。
 */
const MOperatorInterfaceSpec *OperatorInterfaceSpec(const Memory::String &interfaceName,
                                                    const Memory::String &methodName,
                                                    bool *receiverLeft)
{
    for (const MOperatorInterfaceSpec &spec : OperatorInterfaceSpecs())
    {
        if (Memory::String(spec.leftMethod) == methodName)
        {
            const Memory::String left = Memory::String(spec.leftInterface) + "__";
            if (!InheritedInterfaceWithPrefix(interfaceName, left.c_str()).empty())
            {
                *receiverLeft = true;
                return &spec;
            }
        }
        if (!spec.unary && Memory::String(spec.rightMethod) == methodName)
        {
            const Memory::String right = Memory::String(spec.rightInterface) + "__";
            if (!InheritedInterfaceWithPrefix(interfaceName, right.c_str()).empty())
            {
                *receiverLeft = false;
                return &spec;
            }
        }
    }
    return nullptr;
}

/**
 * @brief 运算符的二元条目（`-` 既有二元也有一元，这里只要二元那条）。
 * @param op 运算符文本或类别。
 * @retval 对应对象的指针。
 */
const MOperatorInterfaceSpec *BinaryOperatorSpec(const Memory::String &op)
{
    for (const MOperatorInterfaceSpec &spec : OperatorInterfaceSpecs())
        if (!spec.unary && op == spec.op) return &spec;
    return nullptr;
}

/**
 * @brief 方法名是不是某个运算符接口的方法名（`leftAdd` / `rightAdd` / `negate` …）。
 *
 * 运算符接口方法享受一条 VIP：**只差返回类型也算重载**。同一个右操作数类型配不同返回
 * 类型（`Add<int64,int64>` 与 `Add<Vec,int64>`）在数学上合法，但普通方法做不到 ——
 * C++ 也一样，光靠返回类型分不开。所以只在运算符接口方法上开这个口子。
 */
bool IsOperatorInterfaceMethod(const Memory::String &methodName)
{
    for (const MOperatorInterfaceSpec &spec : OperatorInterfaceSpecs())
    {
        if (Memory::String(spec.leftMethod) == methodName) return true;
        if (!spec.unary && Memory::String(spec.rightMethod) == methodName) return true;
    }
    return false;
}

/**
 * @brief 类型 `owner` 是不是实现了接口 `iface`（含经用户接口间接继承来的）。
 *
 * 两类名字走的机制不一样，所以两条都试：
 * - `NoRightAdd` 这类**空标记接口没有类型实参**，不会被实例化，名字就是原名 —— 交给
 *   `StructHasInterface`，它顺着 `g_interfaceBases` 认「`interface MyNo : NoRightAdd`」
 *   这种间接实现。
 * - `RightAdd` 这类**泛型接口**在 `g_structs[X].interfaces` 里存的是实例化后的名字
 *   （`RightAdd__int64__int64`），整串比不出来 —— 按「名字 + `__`」前缀沿继承链找，
 *   与 {@link OperatorInterfaceSpec} 认方法方向的做法一致。
 *
 * @param owner 类型名（struct 名，不是显示名）。
 * @param iface 接口名；泛型接口写基名，不带类型实参。
 * @return 实现了返回 true。
 */
bool TypeImplementsInterface(const Memory::String &owner, const Memory::String &iface)
{
    if (StructHasInterface(owner, iface)) return true;
    const Memory::String prefix = iface + "__";
    const auto found = g_structs.find(owner);
    if (found != g_structs.end())
        for (const Memory::String &declared : found->second.interfaces)
            if (!InheritedInterfaceWithPrefix(declared, prefix.c_str()).empty()) return true;
    for (const MHostInterfaceImpl &implementation : g_hostInterfaceImpls)
        if (implementation.owner && implementation.owner->kind == MType::Struct && implementation.owner->name == owner && !InheritedInterfaceWithPrefix(implementation.interfaceName, prefix.c_str()).empty())
            return true;
    return false;
}

/**
 * @brief 右操作数那侧对「写出来的这个运算符」表了什么态。
 *
 * 交换律与对偶推导都会把右操作数借来当接收者（`5 + a` 借 `a + 5` 的实现）。但类型
 * 自己有话说时就不该再兜底 —— 否则「我明明写了 `RightAdd`，`5 + a` 却走了别的路」
 * 会很难查。三种状态：
 *
 * - `Open`：没表态 —— 推导照旧补（保持向后兼容）。
 * - `ExplicitRight`：声明了 `RightXxx` —— 右向由作者自己负责，形参接不上就报错，
 *   不再借左接口兜底。
 * - `Forbidden`：声明了 `NoRightXxx` —— 明令禁止。
 *
 * 判定的对象是**右操作数的类型**：`a + 5` 里右操作数是 `int64`，它没有运算符接口，
 * 所以照样开放；`5 + a` 里右操作数是那个 struct，它的表态才算数。
 */
enum class MOperatorRightDirection
{
    Open,
    ExplicitRight,
    Forbidden,
};

/**
 * @brief 查询运算符右操作数方向的接口规则。
 * @param spec 运算符接口规则描述。
 * @param rightType 运算符右操作数的类型。
 * @retval 保存查询运算符右操作数方向的接口规则结果的对象。
 */
MOperatorRightDirection RightDirectionOf(const MOperatorInterfaceSpec &spec,
                                         const std::shared_ptr<MType> &rightType)
{
    if (!rightType || (rightType->kind != MType::Struct && rightType->kind != MType::Host))
        return MOperatorRightDirection::Open;
    const Memory::String &owner = rightType->name;
    if (owner.empty()) return MOperatorRightDirection::Open;
    /* 标记接口就是「No」+ 右接口名：`RightAdd` → `NoRightAdd`。 */
    if (TypeImplementsInterface(owner, "No" + Memory::String(spec.rightInterface)))
        return MOperatorRightDirection::Forbidden;
    if (TypeImplementsInterface(owner, spec.rightInterface))
        return MOperatorRightDirection::ExplicitRight;
    return MOperatorRightDirection::Open;
}

/**
 * @brief 查找宿主类型的接口实现绑定。
 * @param owner 所属对象或类型信息。
 * @param methodName 目标方法的名称。
 * @param arguments 按调用顺序保存的实参列表。
 * @param requiredOut 接收所需接口方法签名。
 * @param symbolOut 接收匹配实现的符号名称。
 * @retval 对应对象的指针。
 */
const MHostInterfaceImpl *FindHostInterfaceImplementation(
    const std::shared_ptr<MType> &owner, const Memory::String &methodName,
    const Memory::Vector<std::shared_ptr<MType>> &arguments,
    const MInterfaceMethod **requiredOut, Memory::String *symbolOut = nullptr)
{
    for (int tier = 0; tier < 2; ++tier)
    {
        Memory::Vector<std::pair<const MHostInterfaceImpl *, const MInterfaceMethod *>> matches;
        for (const MHostInterfaceImpl &implementation : g_hostInterfaceImpls)
        {
            if (!SameType(implementation.owner, owner)) continue;
            const auto found = g_interfaces.find(implementation.interfaceName);
            if (found == g_interfaces.end()) continue;
            for (const MInterfaceMethod &required : found->second)
            {
                if (required.name != methodName || required.params.size() != arguments.size()) continue;
                bool compatible = true;
                for (std::size_t index = 0; index < arguments.size(); ++index)
                    if (!Acceptable(required.params[index], arguments[index], tier != 0))
                        compatible = false;
                if (compatible) matches.push_back({ &implementation, &required });
            }
        }
        if (matches.size() == 1u)
        {
            const auto &match = matches.front();
            *requiredOut = match.second;
            if (symbolOut != nullptr)
                *symbolOut = match.first->methodQualified ? match.first->function + "." + match.second->name : match.first->function;
            return match.first;
        }
        if (matches.size() > 1u)
        {
            Fail("胶水接口方法有多个实现同样匹配：" + Describe(owner) + "." + methodName);
            return nullptr;
        }
    }
    return nullptr;
}

/**
 * @brief 查找源类型到目标类型的转换实现。
 * @param source 提供输入数据的来源。
 * @param target 待访问或修改的目标。
 * @param allowExplicit 是否允许显式转换实现。
 * @retval 对应对象的指针。
 */
const MConversionDecl *FindConversion(const std::shared_ptr<MType> &source,
                                      const std::shared_ptr<MType> &target,
                                      bool allowExplicit)
{
    const MConversionDecl *implicit = nullptr;
    for (const MConversionDecl &conversion : g_conversions)
    {
        if (!SameType(conversion.source, source) || !SameType(conversion.target, target)) continue;
        if (conversion.implicit)
            implicit = &conversion;
        else if (allowExplicit)
            return &conversion;
    }
    return implicit;
}

/**
 * @brief 查询直接转换；显式转换优先，缺省允许复用隐式实现。
 */
bool HasDeclaredConversion(const std::shared_ptr<MType> &source,
                           const std::shared_ptr<MType> &target, bool allowExplicit)
{
    return FindConversion(source, target, allowExplicit) != nullptr || (source && target && source->kind != MType::Void && target->kind == MType::String);
}

/**
 * @brief 判断两个类型之间是否存在隐式转换。
 * @param source 提供输入数据的来源。
 * @param target 待访问或修改的目标。
 * @retval 两个类型之间存在隐式转换时为 true，否则为 false。
 */
bool HasImplicitConversion(const std::shared_ptr<MType> &source,
                           const std::shared_ptr<MType> &target)
{
    return FindConversion(source, target, false) != nullptr || (source && target && source->kind != MType::Void && target->kind == MType::String);
}

/* 内存内建 `memcpy` / `memset` 用到哪几个（用到才生成运行时小函数）。
 * `new` / `delete` 现在是语言关键字（走虚拟机的堆），不再是函数式内建。 */
/* 运行时数组用到才生成 `__magel_new_array`。 */
bool g_usesNewArray = false;
/* 结构体按值拷贝用到才生成 `__magel_copy_struct`。 */
bool g_usesStructCopy = false;

/**
 * @brief 类型的**规范写法**：同一个类型永远得到同一个串（实现见上面 `TypeIdOf` 的说明）。
 */
Memory::String TypeKey(const std::shared_ptr<MType> &type)
{
    if (!type) return "void";
    switch (type->kind)
    {
    case MType::Void: return "void";
    case MType::Bool: return "bool";
    case MType::Char: return "char";
    case MType::Int:
        if (type->bits == 64) return type->isUnsigned ? "uint64" : "int64";
        return Memory::String(type->isUnsigned ? "uint" : "int") + Memory::toString(type->bits);
    case MType::Double: return type->bits == 64 ? "float64" : "float32";
    case MType::String: return "std::lang::String";
    case MType::Host: return type->name;
    case MType::BigInt: return "bint";
    case MType::BigFloat: return "bfloat";
    case MType::File: return "file";
    case MType::Proc: return "proc";
    case MType::Obj: return "std::lang::Any";
    case MType::Struct: return "struct " + type->name;
    case MType::Ptr:
        return (type->readOnly ? "const& " : "mut& ") + TypeKey(type->elem);
    case MType::Container:
    {
        Memory::String key = type->name + "<" + TypeKey(type->elem);
        if (type->elem2) key += ", " + TypeKey(type->elem2);
        return key + ">";
    }
    case MType::Array:
    {
        /* 运行时长度数组（`T[]`）与静态数组（`T[N]`）**不是**同一个类型。 */
        if (type->dims.empty()) return "rt[] " + TypeKey(type->elem);
        Memory::String key = "arr " + TypeKey(type->elem);
        for (const int dim : type->dims) key += "[" + Memory::toString(dim) + "]";
        return key;
    }
    case MType::Func:
    {
        Memory::String key = "fn(";
        for (std::size_t index = 0; index < type->paramTypes.size(); ++index)
        {
            if (index != 0u) key += ", ";
            if (type->variadic && index + 1u == type->paramTypes.size()) key += "...";
            key += TypeKey(type->paramTypes[index]);
        }
        return key + ") -> " + TypeKey(type->elem);
    }
    }
    return "void";
}

/**
 * @brief 预先给**内建类型**发号：`__typeTag(int)` 永远是 1，在哪份程序里都一样。
 *
 * `ResetState` 里调（每次编译都从这张固定的号表开始）。
 */
void RegisterBuiltinTypeIds()
{
    TypeIdOf(SimpleType(MType::Int, false, 64));    /* 1  int64 */
    TypeIdOf(SimpleType(MType::Int, true, 64));     /* 2  uint64 */
    TypeIdOf(SimpleType(MType::Double, false, 64)); /* 3  float64 */
    TypeIdOf(SimpleType(MType::Bool));              /* 4 */
    TypeIdOf(SimpleType(MType::String));            /* 5 */
    TypeIdOf(SimpleType(MType::Void));              /* 6 */
    TypeIdOf(SimpleType(MType::BigInt));            /* 7 */
    TypeIdOf(SimpleType(MType::Obj));               /* 8 */
    TypeIdOf(SimpleType(MType::Char));              /* 9 */
    /* 10 内建接口 `Closeable`：号写死在预留区里，不随用户声明排队 ——
     * 否则往标准前置里加一个接口会把所有既有 `__typeTag` 号整体推后。 */
    TypeIdOf(NamedType("Closeable"));
    /* `10..32` 留给以后的内建：直接跳到用户区的起点，别让后面的号紧贴着内建。 */
    g_nextTypeId = kFirstUserTypeId;
}

/**
 * @brief 给声明过的结构体 / 接口及其引用发号（顺序固定，`__typeTag` 的结果才好核对）。
 *
 * 顺序：内建（`1..9`，`10..32` 预留）→ 用户区从 `33` 起 → 结构体（按声明顺序）→
 * 接口（按声明顺序）→ 各结构体 / 接口的
 * `const&` 与 `mut&`（按同样的顺序）。别的复合类型（容器 / 数组 / 函数指针…）
 * 在第一次用到时接着往下排。
 */
void RegisterNamedTypeIds()
{
    /* 结构体按**声明顺序**（`g_structs` 是按名字排的 map，这里另取一遍顺序）。 */
    Memory::Vector<Memory::String> structOrder;
    for (const int topId : g_top)
    {
        if (N(topId).kind == MNode::StructDecl) structOrder.push_back(N(topId).s);
    }
    for (const Memory::String &name : structOrder) TypeIdOf(NamedType(name));
    for (const Memory::String &name : g_interfaceOrder) TypeIdOf(NamedType(name));
    for (const Memory::String &name : structOrder)
    {
        const std::shared_ptr<MType> named = NamedType(name);
        TypeIdOf(PointerType(named, true));
        TypeIdOf(PointerType(named, false));
    }
    for (const Memory::String &name : g_interfaceOrder)
    {
        const std::shared_ptr<MType> named = NamedType(name);
        TypeIdOf(PointerType(named, true));
        TypeIdOf(PointerType(named, false));
    }
}

/**
 * @brief 为省略默认实参导致的重载歧义列出候选签名和消歧建议。
 * @param labels 同一匹配档中的候选标签。
 * @param count 已提供的实参数量。
 * @return 存在默认实参省略时返回诊断后缀，否则返回空串。
 */
Memory::String DefaultArgumentAmbiguity(const Memory::Vector<Memory::String> &labels,
                                        std::size_t count)
{
    bool omitted = false;
    for (const auto &label : labels)
    {
        const auto found = g_funcs.find(label);
        if (found == g_funcs.end()) continue;
        for (std::size_t i = count; i < found->second.params.size(); ++i)
            if (found->second.params[i].defaultExpr >= 0) omitted = true;
    }
    if (!omitted) return {};
    Memory::String message = "；默认参数歧义：省略尾部实参后，多个候选仍然匹配；候选签名：";
    for (std::size_t c = 0; c < labels.size(); ++c)
    {
        if (c) message += "、";
        const auto found = g_funcs.find(labels[c]);
        if (found == g_funcs.end()) continue;
        const auto &params = found->second.params;
        Memory::String name = labels[c].substr(0, labels[c].find('$'));
        for (const auto &declaration : g_functionDecls)
            if (declaration.label == labels[c])
            {
                name = ModuleSymbol(declaration.module, declaration.source);
                break;
            }
        const auto dot = name.rfind('.');
        if (dot != Memory::String::npos)
        {
            const auto ownerName = name.substr(0, dot);
            if (const auto *owner = FindStruct(ownerName))
                for (const auto &method : owner->methods)
                    if (ownerName + "." + method.name == labels[c])
                    {
                        name = method.isConstructor && !method.isNamedConstructor ? ownerName : ownerName + (method.isStatic ? "::" : ".") + method.source;
                        break;
                    }
        }
        message += name + "(";
        Memory::String missing;
        for (std::size_t i = 0; i < params.size(); ++i)
        {
            if (i) message += ", ";
            const auto &param = params[i];
            message += Describe(param.variadic ? param.type->elem : param.type);
            if (param.variadic) message += "...";
            message += " " + param.source;
            if (param.defaultExpr >= 0)
            {
                message += " = 默认值";
                if (i >= count)
                {
                    if (!missing.empty()) missing += ", ";
                    missing += param.source;
                }
            }
        }
        message += ")（" + (missing.empty() ? Memory::String("未省略默认参数") : "省略：" + missing) + "）";
    }
    return message + "；请显式补充能区分候选的实参，或调整重载签名；不会按省略默认参数的数量或声明顺序选择重载";
}
