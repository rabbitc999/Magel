/* ---------- 前端的符号表 ---------- */

/**
 * 结构体字段：名字 + 前端类型 + 摊平后的槽位。
 */
struct MField
{
    Memory::String name;
    std::shared_ptr<MType> type;
    /**
     * 这个字段在结构体里占的**起始槽号**与**槽数**（`BuildStructTypes` 里算好）。
     *
     * 结构体字段是**值语义**：`struct Box { Point inner; int tag; }` 里的 `inner`
     * 真占 Box 的 0、1 号槽（不是"一个槽存句柄、所有 Box 共用一份对象"），所以
     * 一个字段可能占好几个槽。
     */
    int slot = -1;
    int slots = 0;
};

/**
 * @brief 一个形参：源程序里的名字、四元式里的名字、前端类型。
 *
 * 两个名字分开记是因为 stackvm 的 `decl` 类型表**按名字全局唯一**：形参也得
 * 全程序唯一（重名的加 `$序号`），但函数体里还得按**源名字**查作用域。
 */
struct MParam
{
    Memory::String source;
    Memory::String emitted;
    std::shared_ptr<MType> type;
    bool variadic = false;
    int defaultExpr = -1;  ///< 定义模块中的默认表达式，调用时求值。
    int captureMode = 0;   ///< lambda 捕获：0 只读引用，1 可写引用，2 独立可写副本，3 移入的拥有值。
};

/**
 * @brief 检查默认尾参与变长参数的顺序。
 * @param params 形参列表。
 * @param where 声明位置。
 * @return 是否合法。
 */
bool ValidateVariadicParameters(const Memory::Vector<MParam> &params, const Memory::String &where)
{
    bool optional = false;
    for (std::size_t index = 0; index < params.size(); ++index)
    {
        if (params[index].defaultExpr >= 0)
            optional = true;
        else if (optional && !params[index].variadic)
            return Fail(where + " 的默认参数后不能有必填参数");
        if (!params[index].variadic) continue;
        if (index + 1u != params.size())
            return Fail(where + " 的变长参数必须是最后一个形参");
    }
    return true;
}

/**
 * @brief 判断实参数量能否绑定形参。
 * @param params 形参列表。
 * @param count 实参数量。
 * @return 是否匹配。
 */
bool AcceptsArgumentCount(const Memory::Vector<MParam> &params, std::size_t count)
{
    const bool variadic = !params.empty() && params.back().variadic;
    const std::size_t fixed = params.size() - (variadic ? 1u : 0u);
    if (!variadic && count > fixed) return false;
    for (std::size_t i = count; i < fixed; ++i)
        if (params[i].defaultExpr < 0) return false;
    return true;
}

/**
 * 方法签名（运算符重载是顶层自由函数，见 MOperatorDecl）。
 */
struct MMethod
{
    bool defaultValueText = false;  ///< 此方法为无用户回调的默认字段表示。
    bool superseded = false;        /* 同一个 struct 里重名同签名、被后面的定义顶掉了 */

    Memory::String source; /* 源程序里的方法名 */
    Memory::String name;   /* 四元式标签后缀（同名方法多于一个时带 `$类型` 后缀） */
    std::shared_ptr<MType> result;
    Memory::Vector<MParam> params;
    bool isStatic = false;
    bool isConstructor = false;
    bool isNamedConstructor = false;  ///< static T create 的公开工厂入口与隐式 self 构造体。
    int bodyId = -1;
    Memory::String module;
};

struct MStaticField
{
    std::shared_ptr<MType> type;
    Memory::String emitted;
    bool isConst = false;
    bool hasFloatInitializer = false;
    double floatInitializer = 0.0;
};

/**
 * 一个 struct 的字段与方法（顺序就是数据区槽位顺序）。
 */
struct MStructDef
{
    bool abiPacked = false;
    Memory::Vector<MField> fields; /* 声明里写的字段（按名字查） */
    /**
     * **摊平后的槽位表**：一个槽一项（`字段名` 或 `外层字段.内层字段`），
     * 类型是那个槽自己的类型。simple_ast 的数据区布局、`Node::member` 取字段
     * 都按这张表来 —— 长度就是 `slotCount`。
     */
    Memory::Vector<std::pair<Memory::String, std::shared_ptr<MType>>> leaves;
    int slotCount = 0;      /* 占几个 8 字节槽 */
    bool flattened = false; /* 摊平算过了没 */
    Memory::Vector<MMethod> methods;
    /**
     * 继承的接口名（`struct X : A, B`）。登记时用它核对"是不是都实现全了"；
     * 接口引用调方法时分派表也要按它找"哪些结构体能接住这个接口"。
     */
    Memory::Vector<Memory::String> interfaces;
};

/**
 * 枚举载荷在合成结构体中的一个槽；storageName 不暴露给源程序。
 */
struct MEnumPayload
{
    Memory::String name;
    Memory::String storageName;
    std::shared_ptr<MType> type;
};

struct MEnumVariantDef
{
    Memory::String name;
    int tag = 0;
    Memory::Vector<MEnumPayload> payloads;
};

/**
 * 枚举以“标签槽 + 各变体载荷槽”实现，外部只通过构造器和 `match` 模式访问。
 */
struct MEnumDef
{
    Memory::Vector<MEnumVariantDef> variants;
};

/**
 * @brief 接口要求的一个方法（签名及可选默认函数体）。
 *
 * 接口是**编译期契约**：`struct X : Shape` 就意味着"X 里一定有这么几个函数"。
 * 多个接口里同名同签名的要求会**去重**（只算一条）。
 */
struct MInterfaceMethod
{
    bool isStatic = false;  ///< 编译期静态契约，不进入动态分派。
    Memory::String name;
    std::shared_ptr<MType> result;
    Memory::Vector<std::shared_ptr<MType>> params; /* 只要类型：接口不关心形参叫什么 */
    Memory::Vector<MParam> bodyParams;
    int bodyId = -1;
    Memory::String module;
    /**
     * 声明它的那个接口（继承来的方法保留**原始**声明者）—— 判断两路默认方法是不是
     *  同一条继承路径带进来的重复项时用它，见 `AddInterfaceDefaults`。
     */
    Memory::String owner;
};

/**
 * 接口：名字 → 要求的函数表（**已展开继承**：自己的方法在前，继承来的按基接口顺序追加）。
 */
Memory::Map<Memory::String, Memory::Vector<MInterfaceMethod>> g_interfaces;

/**
 * 接口：名字 → 直接基接口名列表（`interface A : B, C`）。展开继承时用。
 */
Memory::Map<Memory::String, Memory::Vector<Memory::String>> g_interfaceBases;

/**
 * 接口名（`interface` 声明的顺序）：预先分配类型号时用，顺序稳定、好核对。
 */
Memory::Vector<Memory::String> g_interfaceOrder;

/**
 * 已经被调用过的接口方法（`接口|方法`）：`LowerProgram` 收尾时给它们各生成一个
 * **分派函数**（接口引用是"地址 + 类型号"的胖引用，分派函数按类型号挑到具体实现）。
 */
Memory::Set<Memory::String> g_usedInterfaceMethods;
