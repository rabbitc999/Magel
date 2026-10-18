/* ---------- 内部执行期类型标签 ---------- */

/**
 * @brief 单次编译的内部标签，只供 Any 校验、接口分派与反射桥接使用。
 *
 * 内建标签与匹配的 VM 保持一致；用户类型和复合类型在编译期登记。
 * 不保证跨程序、版本或增删声明后的编号稳定；持久化身份使用 std::reflect 的元数据 ID。
 */
Memory::Map<Memory::String, std::int64_t> g_typeIdIndex;
/**
 * @brief 下一个内部标签，运行时与编译器都以 64 位有符号整数存储。
 */
std::int64_t g_nextTypeId = 1;
/**
 * 内建固定占 `1..9`，`10..32` 预留给**以后新增的内建** —— 新内建只能往这段空号里加，
 *  保持编译器与匹配 VM 的内建标签一致。
 */
constexpr std::int64_t kFirstUserTypeId = 33;
/**
 * 号用尽的上界（实际任何程序都远远到不了，这是防回绕的兜底）。
 */
constexpr std::int64_t kMaxTypeId = 9223372036854775807LL; /* INT64_MAX */

/**
 * 普通函数签名。
 */
struct MFuncSig
{
    std::shared_ptr<MType> result;
    Memory::Vector<MParam> params;
    bool staticCallbacks = false;  ///< 异步事件入口只接受不借用局部环境的回调。
};

/**
 * 一个普通函数声明（重载时每个签名一份，标签重名会带 `$类型` 后缀）。
 */
struct MFunctionDecl
{
    Memory::String source; /* 源程序里的函数名 */
    Memory::String module; /* 空字符串表示兼容的全局模块 */
    Memory::String label;  /* 四元式标签 */
    MFuncSig signature;
    int bodyId = -1;          /* 函数体（Block 节点） */
    bool external = false;    /* 只有签名；实现从 stackvm::NativeRegistry 取得 */
    bool moduleLocal = false; /* `inline`：只允许声明模块内部调用，不导出。 */
    bool superseded = false;  /* 同名同签名又被定义了一次：这一条不降级（后面的覆盖前面的） */
    Memory::String abiLibrary;
    Memory::String abiSymbol;
    int abiResultCountIndex = -1;
    Memory::String abiRelease;
    bool cExport = false;
};

/**
 * @brief 为模块内符号生成限定名称。
 * @param module 当前模块的名称。
 * @param name 名称文本。
 * @retval 生成或读取的结果文本。
 */
Memory::String ModuleSymbol(const Memory::String &module, const Memory::String &name)
{
    return module.empty() ? name : module + "::" + name;
}

/**
 * @brief 为调用目标生成内部符号名称。
 * @param callee 被调用函数的名称或表达式。
 * @retval 生成或读取的结果文本。
 */
Memory::String CallSymbol(const MNode &callee)
{
    if (callee.kind == MNode::Var) return ModuleSymbol(callee.module, callee.s);
    if (callee.kind == MNode::EnumCtor && !callee.s.empty() && std::islower(static_cast<unsigned char>(callee.s[0])))
        return ModuleSymbol(callee.s, callee.s2);
    return {};
}

/**
 * @brief 裸核心库调用是否未被当前作用域变量或用户函数遮蔽。
 */
bool CanUseImplicitLang(const Memory::String &module, const Memory::String &name);

/**
 * @brief 将标准库调用名转换为规范名称。
 * @param callee 被调用函数的名称或表达式。
 * @retval 生成或读取的结果文本。
 */
Memory::String StandardCallName(const MNode &callee)
{
    if (callee.kind == MNode::Var && CanUseImplicitLang(callee.module, callee.s))
        return callee.s;
    /* `__` 前缀是**编译器内部名字**（`__fileTryOpen`、`__processRun`、`__stringBytes` …）：
     * 标准库模块（`magel/source/std/*.mgl`）里也要能直接调，所以不按模块过滤，
     * 这样"文件/进程包装搬进 std::io / std::process"之后它们仍然认得出来。
     * 用户侧不可能声明 `__` 开头的公开 API，所以放宽这一步不会让用户拿到不该有的名字。 */
    if (callee.kind == MNode::Var && callee.s.compare(0u, 2u, "__") == 0)
        return callee.s;
    if (callee.kind == MNode::EnumCtor && callee.s == "std::lang") return callee.s2;
    return {};
}

/**
 * @brief `std::algorithms::name<T>` 经泛型预处理后会成为 `name__T`；这里还原公开名。
 * @param callee 被调用函数的名称或表达式。
 * @retval 生成或读取的结果文本。
 */
Memory::String AlgorithmCallName(const MNode &callee)
{
    if (callee.kind != MNode::EnumCtor || callee.s != "std::algorithms") return {};
    Memory::String name = callee.s2;
    const std::size_t specialization = name.find("__");
    if (specialization != Memory::String::npos) name.resize(specialization);
    static const Memory::Set<Memory::String> names{
        "findValue", "sortValue", "binarySearchValue", "minValue", "maxValue", "clampValue",
        "allValue", "anyValue", "foldValue",
        "mapValue", "filterValue", "reduceValue", "sumValue", "countValue"
    };
    return names.count(name) != 0u ? name : Memory::String();
}

/**
 * 变量：前端类型 + 降级后的名字（+ 是不是 `const` 不可变变量）。
 */
struct MVar
{
    std::shared_ptr<MType> type;   /* 前端类型（含数组维数 / 引用） */
    Memory::String emitted;        /* 降级后的名字（局部变量重名时改名） */
    bool isConst = false;          /* `const T x` 声明的：不能再赋值 */
    bool elementReference = false; /* foreach 隐藏引用，源码变量自动解引用。 */
    Memory::String elementList{};  /* 原生列表元素以容器和索引作为可写引用。 */
    Memory::String elementIndex{};
    int constantExpression = -1;      ///< 可按需求值的 const 初始化式；普通变量不登记。
    std::size_t borrowDepth = 0;      ///< 借用来源的最短作用域；0 为全局，1 为调用方，2 起为函数局部。
    bool allocatedReference = false;  ///< 显式 new 的引用存储可接管调用方借用，仍禁止保存函数局部来源。
    Memory::String borrowOwner{};     ///< 直接引用别名对应的局部拥有值，用于回传字段及容器写入的来源。
};

Memory::Map<Memory::String, Memory::Set<Memory::String>> g_moduleImports;  ///< 模块可省略限定的直接导入。
Memory::Map<Memory::String, MStructDef> g_structs;                         /* struct 名 → 定义 */

/**
 * @brief 类型名的"裸名"：去掉模块前缀（`custom::Box__int` → `Box__int`）。
 */
inline Memory::String BareTypeName(const Memory::String &name)
{
    const std::size_t scope = name.rfind("::");
    return scope == Memory::String::npos ? name : name.substr(scope + 2u);
}
/* 编译器匿名元组：内部 struct 名 → 按位置排列的元素类型。 */
Memory::Map<Memory::String, Memory::Vector<std::shared_ptr<MType>>> g_tupleElements;
Memory::Map<Memory::String, MStaticField> g_staticFields;                    /* `Type::field` → 全局槽 */
Memory::Map<Memory::String, MEnumDef> g_enums;                               /* enum 名 → 变体定义 */
Memory::Map<Memory::String, MFuncSig> g_funcs;                               /* 四元式标签 → 签名（重载后一个标签一份） */
Memory::Map<Memory::String, Memory::Vector<Memory::String>> g_functionIndex; /* 源函数名 → 重载标签 */
Memory::Map<Memory::String, MFuncSig> g_inferenceFunctions;                  /* auto 返回类型推导期间可见的函数 */
Memory::Vector<MFunctionDecl> g_functionDecls;                               /* 普通函数声明（降级按顺序取） */
Memory::Map<Memory::String, Memory::Vector<MParam>> g_lambdaCaptures;        ///< 词法作用域确定的只读、可写、拷贝或移动捕获。
Memory::Set<Memory::String> g_lambdaAutoReturns;                             ///< 等捕获类型确定后推导返回值。
Memory::Set<Memory::String> g_lambdaPrepared;                                ///< 已绑定环境的具体 lambda。
Memory::Map<Memory::String, std::shared_ptr<MType>> g_hostTypes;             /* 宿主类型名 → VM ABI 类型 */
struct MHostInterfaceImpl
{
    std::shared_ptr<MType> owner;
    Memory::String interfaceName;
    Memory::String function;
    /* 多方法接口：方法 m 查 `function.m`；单方法（含协议接口）沿用 `function`。 */
    bool methodQualified = false;
};
Memory::Vector<MHostInterfaceImpl> g_hostInterfaceImpls;
const stackvm::NativeRegistry *g_nativeRegistry = nullptr;
/* 这次编译完成后会**立刻在本进程执行**（`--emit` / `--svmb` 只产出文件时为 false）。
 * 立刻跑就必须现在绑好宿主符号；导出的程序等到目标进程自己 setNativeRegistry 再绑。 */
bool g_executeNow = false;
Memory::Vector<Memory::String> g_programArguments;
bool g_threadsEnabled = false;
/**
 * `--debug`：执行时打印源码位置轨迹、出错时给位置与调用栈（都在 stderr 上）。
 */
bool g_debugEnabled = false;
/**
 * `--step-limit N`：本次执行的指令条数上限；0 表示不设上限（缺省）。
 */
std::size_t g_stepLimit = 0u;
/**
 * `--no-debug`：写 `.svmb` 时丢掉调试节（节仍在、写成 0 条）。名字/类型表不受影响。
 */
bool g_stripDebug = false;
/**
 * @brief 是否写无名紧凑符号格式。
 */
bool g_compressSymbols = false;
/**
 * @brief PC 映射文件路径，空值使用输出前缀默认路径。
 */
Memory::String g_symbolMap;
/**
 * @brief PC 映射的模块及版本元数据。
 */
Memory::String g_symbolModule, g_symbolVersion;
/* 运算符重载表（`g_operators`）在 MOperatorDecl 之后定义。 */
Memory::Map<Memory::String, std::shared_ptr<simple_ast::Type>> g_structType; /* struct 名 → simple_ast 类型 */
/**
 * @brief 字面量构造函数的容器类型与扁平实参数量。
 */
struct MCollectionConstructor
{
    std::shared_ptr<MType> type;
    std::size_t count;
};
Memory::Map<Memory::String, MCollectionConstructor> g_collectionConstructors;
simple_ast::Program *g_loweringProgram = nullptr;          /* 降级期懒登记新推导出的元组形状 */
Memory::Vector<Memory::Map<Memory::String, MVar>> g_scope; /* 0 号是全局作用域 */
Memory::Vector<simple_ast::NodePtr> g_globalInit;          /* 全局初始化：塞到 main 最前面 */
Memory::Set<Memory::String> g_usedNames;                   /* 已经占用的降级名（局部变量去重） */
Memory::Map<int, Memory::String> g_globalNames;            /* 全局声明节点 → 降级名 */
Memory::String g_owner;                                    /* 正在降级的方法属于哪个 struct */
Memory::String g_currentModule;                            /* 正在检查/降级的函数所属模块 */

/**
 * @brief 合成一个 `std::lang::List<element>` 类型。
 *
 * 给 `map` / `filter` 这类"结果表"用：`TypeKey` 对 Container 会写成
 * `vec<T>`，所以合成的类型照样能发类型号、能 `LowerType` 成 i64 句柄。
 *
 * @param element 元素类型。
 * @return 列表类型。
 */
std::shared_ptr<MType> MakeListType(const std::shared_ptr<MType> &element)
{
    auto list = Memory::makeShared<MType>();
    list->kind = MType::Container;
    list->name = "vec";
    list->elem = element;
    return list;
}

/**
 * @brief 按名字找结构体：**裸名按当前模块补全**，再退裸名兜底。
 *
 * 类型名的规范形式是 `module::name`（模块里的类型就这么登记），所以查找顺序是：
 *   1. 写出来的名字（`custom::Box__int` 直接命中）；
 *   2. 裸名 → 补上当前模块（模块内写 `Box__int` 时命中）；
 *   3. 去掉模块前缀的裸名（注入标准库正文里的类型是裸名登记的，`FileStream` 之类）。
 *
 * @param name 类型名。
 * @param key  非空时回填**实际命中的注册键**。调用方要用它拼 `g_funcs` 的键
 *             （`键.方法名`）——写出名可能与注册键不同（`custom::Box__int` ↔ `Box__int`），
 *             用写出名拼会查不到签名，表现为"候选非空但参数类型对不上"。
 * @return 定义；找不到返回 nullptr。
 */
inline const MStructDef *FindStruct(const Memory::String &name, Memory::String *key = nullptr)
{
    if (name.empty()) return nullptr;
    auto found = g_structs.find(name);
    if (found != g_structs.end())
    {
        if (key != nullptr) *key = found->first;
        return &found->second;
    }
    const Memory::String bare = BareTypeName(name);
    if (bare == name && !g_currentModule.empty())
    {
        found = g_structs.find(g_currentModule + "::" + name);
        if (found != g_structs.end())
        {
            if (key != nullptr) *key = found->first;
            return &found->second;
        }
    }
    if (bare != name)
    {
        found = g_structs.find(bare);
        if (found != g_structs.end())
        {
            if (key != nullptr) *key = found->first;
            return &found->second;
        }
    }
    return nullptr;
}

/**
 * @brief 按名字找静态字段：探测顺序与 FindStruct 一致（写出名 → 补当前模块 → 去前缀裸名）。
 *
 * `g_staticFields` 的键是 `类型注册名::字段名`，而类型一律按**裸名**登记
 * （实测：`Point::origin`、`Box__int.of`）。所以 `custom::Point::origin` 这样的
 * 限定写法必须先去前缀再查，否则会落空。
 *
 * @param owner 静态字段所属的类型名（可能是限定写法）。
 * @param name  字段名。
 * @return 定义；找不到返回 nullptr。
 */
inline const MStaticField *FindStaticField(const Memory::String &owner, const Memory::String &name)
{
    if (owner.empty() || name.empty()) return nullptr;
    auto found = g_staticFields.find(owner + "::" + name);
    if (found != g_staticFields.end()) return &found->second;
    const Memory::String bare = BareTypeName(owner);
    if (bare == owner && !g_currentModule.empty())
    {
        found = g_staticFields.find(g_currentModule + "::" + owner + "::" + name);
        if (found != g_staticFields.end()) return &found->second;
    }
    if (bare != owner)
    {
        found = g_staticFields.find(bare + "::" + name);
        if (found != g_staticFields.end()) return &found->second;
    }
    return nullptr;
}
int g_localSerial = 0;

/**
 * `obj.get()` 的目标类型：**由上下文给**（`int x = b.get();` / `(int)b.get()`）。
 *
 * 盒子里装着什么是运行期才知道的，而"取出来当什么用"必须静态写上，所以 `get` 的类型
 * 从"接它的那个位置"取（和 `new` 从声明取元素类型是一个道理）。表达式里看到
 * `b.get()` 但上下文没给类型时，报一句人话让人补上。
 */
std::shared_ptr<MType> g_expectedObjGet;

/**
 * @brief 按节点编号取得语法节点池中的节点。
 * @param id 待查找或操作的对象编号。
 * @retval 节点、类型或失败占位表达式。
 */
const MNode &N(int id) { return g_nodes[static_cast<std::size_t>(id)]; }
/**
 * @brief 按类型编号取得类型池中的类型描述。
 * @param id 待查找或操作的对象编号。
 * @retval 对应的类型描述对象。
 */
std::shared_ptr<MType> T(int id) { return M_TypeAt(id); }
/**
 * @brief 构造表示语义处理失败的空表达式。
 * @retval 生成的低层语法节点。
 */
simple_ast::NodePtr Bad() { return simple_ast::Node::i(0); }

/**
 * @brief 判断类型描述是否为元组类型。
 * @param type 类型描述。
 * @retval 类型描述为元组类型时为 true，否则为 false。
 */
bool IsTupleType(const std::shared_ptr<MType> &type)
{
    return type && type->kind == MType::Struct && type->name.rfind("__magel_tuple_", 0u) == 0u;
}

/**
 * @brief 为元组类型确保存在对应的内部定义。
 * @param type 类型描述。
 */
void EnsureTupleDefinition(const std::shared_ptr<MType> &type)
{
    if (!IsTupleType(type)) return;
    auto known = g_tupleElements.find(type->name);
    if (known == g_tupleElements.end())
        known = g_tupleElements.emplace(type->name, type->paramTypes).first;
    if (g_structs.count(type->name) != 0u) return;
    MStructDef definition;
    for (std::size_t index = 0; index < known->second.size(); ++index)
        definition.fields.push_back(MField{ "__" + Memory::toString(index), known->second[index] });
    g_structs[type->name] = std::move(definition);
}

bool EnsureTupleLayout(const std::shared_ptr<MType> &type);

/**
 * @brief 需要可取引用存储的变量名，每次编译重新收集。
 */
Memory::Set<Memory::String> g_addressTakenNames;

/**
 * @brief 所有者变量到引用存储槽的映射，清理时读取槽内当前值。
 */
Memory::Map<Memory::String, Memory::String> g_referenceStorage;
/**
 * @brief 待登记到函数体生命周期中的按值形参副本。
 */
Memory::Vector<std::pair<Memory::String, std::shared_ptr<MType>>> g_parameterReferenceOwners;
/**
 * @brief 每次调用独立分配的形参存储单元，退出时释放。
 */
Memory::Vector<Memory::String> g_pendingParameterStorage;
/**
 * @brief 独立聚合存储的名称，包含按值形参、闭包实体化副本与局部值；返回时只移交内容。
 */
Memory::Set<Memory::String> g_parameterValueStorage;

/**
 * @brief 判断值是否需要隐藏单元素槽来支持变量引用。
 * @param type 被取引用的变量类型。
 * @return 标量、字符串、大数、容器和函数值返回 true；聚合借用其自身存储。
 */
bool NeedsReferenceStorage(const std::shared_ptr<MType> &type)
{
    return type && (type->kind == MType::Int || type->kind == MType::Double || type->kind == MType::Bool || type->kind == MType::Char || type->kind == MType::String || type->kind == MType::BigInt || type->kind == MType::BigFloat || type->kind == MType::Container || type->kind == MType::Func || type->kind == MType::Obj || type->kind == MType::File || type->kind == MType::Proc || IsRuntimeArray(type));
}
