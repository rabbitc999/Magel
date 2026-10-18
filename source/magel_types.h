/**
 * @def MAGEL_WITH_REFLECTION
 * @brief 是否编入注解与反射前端支持；默认关闭，单次编译还需 --reflection。
 */
#ifndef MAGEL_WITH_REFLECTION
#define MAGEL_WITH_REFLECTION 0
#endif
#if MAGEL_WITH_REFLECTION != 0 && MAGEL_WITH_REFLECTION != 1
#error MAGEL_WITH_REFLECTION must be 0 or 1
#endif
#include "runtime_allocator.hpp"
#include "magel_borrow_checker.hpp"
#include "allocator/magel_allocator.hpp"
/* ---------------------------------------------------------------
 * 用户代码：C 语法树节点池 + 类型池 + 降级接口。
 * 语义值 ParserValue_T = int，全部是池下标（POD），状态在 %epilogue。
 * 前缀 M_：Parser_ / ParserValue 是生成器保留名，不要碰。
 * --------------------------------------------------------------- */
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

namespace stackvm
{
class NativeRegistry;
}

/**
 * 类型：void / int64 / float64 / string / struct / array / ptr
 * （ptr 表示两种非空引用：`T const&` 只读，`T mut&` 可写；
 *   string / struct / array 都是数据区句柄）
 */
struct MType
{
    /* 浮点只有 `Double` 一种（`float32` / `float64` 都指向它）。 */
    enum Kind
    {
        Void,
        Bool,
        Char,
        Int,
        Double,
        String,
        Struct,
        Host,
        Array,
        Ptr,
        Func,
        /**
         * 任意精度整数（`bint`）：值是**运行期对象表**里的句柄（后端是 library/big_number）。
         */
        BigInt,
        /**
         * 任意精度十进制浮点（`bfloat`）：值是运行期对象表里的句柄。
         */
        BigFloat,
        /**
         * 容器：`std::lang::List<T>` / `std::lang::Set<T>` / `std::lang::Map<K,V>`（`name` 区分是哪个）。
         * 值同样 = 对象表里的句柄，元素 / 键值类型放在 `elem` / `elem2` 里。
         */
        Container,
        /**
         * 宿主文件句柄：`fopen` 打开，`fread` / `fwrite` / `feof` / `fclose` 使用。
         */
        File,
        /**
         * 宿主进程句柄：`prun` 启动，`pread` 读取输出，`pclose` 收尾。
         */
        Proc,
        /**
         * **装箱格子** `obj`：一个值 + 它的类型号（运行期对象表里的句柄）。
         * `set` 装、`get` 取、`name` 问类型号（和 `__typeTag(类型)` 比就是泛型）。
         */
        Obj
    } kind = Void;
    Memory::String name;          /* struct 名 / 接口名 / 类型名 */
    Memory::String lookupModule;  ///< 未限定具名类型的声明作用域。
    bool unresolvedName = false;  ///< 解析结束后按导入与同名声明确定真实类型。
    /* 数值位宽：整数 8/16/32/64，浮点 32/64。 */
    int bits = 64;
    bool isUnsigned = false;     /* 整型有没有符号 */
    std::shared_ptr<MType> elem; /* 数组元素 / 引用目标 / **函数返回类型** */
    /**
     * 函数指针（`kind == Func`）的形参类型，按声明顺序。
     */
    Memory::Vector<std::shared_ptr<MType>> paramTypes;
    /**
     * 第二个类型参数：`std::lang::Map<K,V>` 的 V（`elem` 放 K）。
     * `std::lang::List<T>` / `std::lang::Set<T>` 只用 `elem`。
     */
    std::shared_ptr<MType> elem2;
    /**
     * 数组各维长度（行主序）。
     * **空 = 运行时长度数组**（`T[] a = new T[N];`）：长度不在类型里，而是跟着对象走
     * （句柄前 8 字节存元素个数），所以下标检查能查到运行期才知道的 N。
     */
    Memory::Vector<int> dims;
    /**
     * 引用是否只读：
     *   `T*` / `T const&` → true（≡ C 的 `T const&`，只能读、不能做引用算术）；
     *   `T mut&`          → false（**可写引用**：能写、能做引用算术、`new` 出来的归它，
     *                       用完自己 `delete`）。
     * 强转时"可写性"跟着被转的表达式走（与 C 去掉 const 同理），所以 `(int64*)` 数组
     * 出来的仍然只读，`(int64 mut&)` 转出来的可写。
     */
    bool readOnly = false;
    /**
     * 函数指针最后一个 paramTypes 项是否为 `T...` 打包后的 std::lang::List<T>。
     */
    bool variadic = false;
};

/* 语法树节点：kind + 名字 + 整数/浮点 + 两个类型下标 + 孩子 */
/**
 * @brief 已通过注解类型检查的编译期值；不保存运行时句柄或引用。
 */
struct MAnnotationValue
{
    int type = -1;          ///< 声明的精确类型池下标。
    long long integer = 0;  ///< 整数、布尔及字符槽。
    double floating = 0;    ///< 浮点槽。
    Memory::String text;    ///< 已解码的字符串值。
};

struct MNode
{
    enum Kind
    {
        Int,
        Float,
        Str,
        Var,
        Bin,
        Un,
        Cast,
        Index,
        Member,
        Arrow,
        Call,
        Method,
        Bool,
        Null,
        Assign,
        Block,
        If,
        While,
        DoWhile,
        For,
        Foreach,
        Loop,
        Switch,
        Case,
        Range,
        Return,
        ExprStmt,
        Declare,
        Field,
        MethodDef,
        FuncDef,
        ExternFuncDecl,
        HostTypeDecl,
        StructDecl,
        Defer,
        AssociatedDecl,
        AssociatedDef,
        OperatorDef,
        List,
        Params,
        AbiBlock,
        Lambda,
        /* 声明头（带 `const` 与否）：type = 类型池下标，i = 是否不可变 */
        DeclHead,
        /* `new T` / `new T[n]`：type = 元素类型，i = 元素个数表达式（-1 = 单个） */
        NewPtr,
        /* `T::new(实参)` 的构造体调用：type = 目标类型，kids = {实参列表节点}。
         * 只由 `static T new(...)` 工厂的内部展开生成，**没有对应的用户语法**
         * （`new(实参)` 这条路 1.0.0 起已删除）。 */
        Construct,
        /**
         * `List<T>(初值…)` / `Set<T>(…)` / `Map<K,V>(…)` 的显式构造：
         * s 为容器类别（`list` / `set` / `map`），kids = {实参列表节点}。
         */
        ContainerCtor,
        /* `delete p;`：kids = {引用表达式} */
        Delete,
        /* 解析期攒下的错的占位节点：`s` 里存着要报的那句话（声明校验
         * 写错、用了改过名的老关键字…）。解析期不能用 Fail：M_Compile 开头的
         * ResetState 会把 g_error 清掉，所以要挂在节点上、等降级时再报。 */
        ParseBad,
        /* `bint(值)`：构造大整数（kids = {实参}） */
        BigIntCtor,
        /* `bfloat(值)` */
        BigFloatCtor,
        Break,
        Continue,
        /* 字符字面量 `'A'`：`s` 里存着词法原文（**带两侧单引号**），降级时才剥引号 +
         * 走转义（和字符串同一套）。放这么"晚"处理是为了让 `'\q'` 这种错在编译期报
         * 得出来：解析期 Fail 会被 ResetState 清掉（见 MPathSyntaxError 的注释）。 */
        Char,
        /* `s[起:止]` 切片：kids = {串, 起, 止}，止 < 0 表示"到末尾"。值是新串。 */
        Slice,
        /* `interface 名字 { 函数声明… };`：`s` = 接口名，kids = {成员表}。 */
        InterfaceDecl,
        /* `__typeTag(类型)` / `__typeTag(表达式)`：编译期折成一个整数（类型号）。
         * type = 类型（类型那条规则），kids = {表达式}（另一条）。 */
        TypeTag,
        /* `enum E { A, B(int x) };` / `E::B(1)`：带标签联合体声明、变体与构造器。 */
        EnumDecl,
        EnumVariant,
        EnumCtor,
        HostImplDecl,
        Try,
        ModuleDecl,
        ImportDecl,
        /* `(a, b, ...)`：异构定长元组字面量；静态类型由各元素推导。 */
        Tuple,
        TupleTypeItem,  ///< 共享元组语法中的显式类型项。
        /**
         * Set / Map 字面量：s 为 set / map / empty，kids 按元素或键值交替存放。
         */
        CollectionLiteral,
        /**
         * 自增减：i 为步长正负一，type2 为前置标记，kids 保存目标。
         */
        Update,
        Destructure,
        AnnotationMarker,
        AnnotationDecl,
        AnnotationParam,
        AnnotationUse,
        AnnotationArg,
    } kind = List;
    std::size_t sourceLine = 0;                         ///< 预处理后语法节点的起始行。
    std::size_t sourceColumn = 0;                       ///< 注解起始 token 的一基列号。
    Memory::Vector<int> annotations;                    ///< 附着于本声明的注解使用节点。
    Memory::Vector<Memory::String> derives;             ///< 请求按字段派生的接口名称。
    bool derivesExpanded = false;                       ///< 已完成宏模板注入，泛型重放时不重复生成。
    Memory::Vector<MAnnotationValue> annotationValues;  ///< 按参数声明顺序归一后的常量。
    Memory::String s, s2;
    Memory::String abiLibrary;
    Memory::String abiSourceFile;
    Memory::String abiLength;
    Memory::String abiRelease;
    bool abiPacked = false;
    bool cExport = false;
    Memory::String module;
    /**
     * `inline` 顶层函数仅在声明它的模块内部可见，不进入模块导出表。
     */
    bool moduleLocal = false;
    bool reflectionBridge = false;      ///< 仅编译器生成的反射桥接节点可以读取内部接口身份。
    bool generatedDeclaration = false;  ///< 由编译器自身的派生模板生成，允许内部辅助符号。
    long long i = 0;
    double d = 0.0;
    int guard = -1;                  ///< match 分支的可选 bool 守卫。
    bool unsignedLiteral = false;    ///< 整数字面量是否显式带有小写 u 后缀。
    bool checkedExpression = false;  ///< 完整表达式的副作用检查边界。
    bool returnExpression = false;   ///< return 带值控制表达式的隐藏结果声明或值出口。
    bool implicitExitMove = false;   ///< 编译器在离开内部拥有者作用域时生成的消费操作。
    int type = -1, type2 = -1;
    Memory::Vector<int> kids;
};

int M_New(int kind, Memory::String s = Memory::String(), long long i = 0, double d = 0.0);
int M_TypeKind(int kind, const Memory::String &name = Memory::String());
int M_TypeInt();
int M_TypeIntBits(int bits, int unsignedFlag);
int M_TypeBool();
int M_TypeChar();
int M_TypeFloatBits(int bits);
int M_TypeDouble();
int M_TypeString();
int M_TypeVoid();
int M_TypeStruct(int nameId);
/**
 * `(T1, T2, ...)`：异构定长元组类型（至少两个元素）。
 */
int M_TypeTuple(int typeList);
/**
 * @brief 包装元组中的显式类型项。
 * @param type 类型下标。
 * @return 语法节点。
 */
int M_TupleTypeItem(int type);
/**
 * @brief 在类型上下文解释共享元组语法。
 * @param items 项列表。
 * @return 元组类型下标。
 */
int M_TypeTupleSyntax(int items);
/**
 * `T*` / `T const&`：只读引用（读得到、写不了）。
 */
int M_TypePtr(int baseId);
/**
 * @brief 旧的裸星号写法（`int64* p` / `(int64*)x`）：记一条"改用 T const& / T mut&"的错。
 *
 * 解析期记下来、编译开始时统一报（`ResetState` 会清 g_error，解析期不能直接 Fail）。
 * 返回的还是"只读引用"类型，让解析能继续走完、把后面的错一起找出来。
 * @param baseId 元素类型。
 * @return 只读引用类型。
 */
/**
 * `T mut&`：可写引用（能写、能算术、有所有权）。
 */
int M_TypePtrMut(int baseId);
/**
 * @brief `T[]`：**运行时长度数组**（`T[] a = new T[N];`）。
 *
 * 与 `T[N]`（编译期就把维度写死在数据区里）的区别：长度跟着对象走、下标检查
 * 读的是运行期存下来的元素个数，所以 `N` 可以是变量 —— 这就是"运行时可变的分配"。
 * @param elementId 元素类型。
 */
int M_TypeRuntimeArray(int elementId);
/**
 * @brief 构建匿名函数类型。
 * @param paramsId 无名称的形参类型列表。
 * @param returnTypeId 返回类型。
 * @return 类型池下标。
 */
int M_FunctionType(int paramsId, int returnTypeId);
/**
 * @brief 保存函数类型形参，变长形参使用列表表示。
 * @param typeId 形参类型。
 * @param variadic 是否为变长形参。
 * @return 节点池下标。
 */
int M_FunctionParameter(int typeId, int variadic);
/**
 * @brief 保存解析阶段发现的语义错误，交由检查阶段报告。
 */
int M_ParseBad(const Memory::String &message);
/**
 * `bint`：任意精度整数（值 = 运行期对象句柄）。
 */
int M_TypeBigInt();
/**
 * `bfloat`：任意精度十进制浮点（值 = 运行期对象句柄）。
 */
int M_TypeBigFloat();
/**
 * `obj`：**装箱格子**（值 = 运行期对象句柄：一个值 + 它的类型号）。
 */
int M_TypeObj();
/**
 * `file` / `proc`：宿主资源句柄。
 */
int M_TypeFile();
int M_TypeProc();
/**
 * `std::lang::List<T>` / `std::lang::Set<T>`：元素类型是 `elementId`。
 */
int M_TypeVec(int elementId);
int M_TypeSet(int elementId);
/**
 * `std::lang::Map<K,V>`：键 `keyId`、值 `valueId`。
 */
int M_TypeMap(int keyId, int valueId);
int M_Dims1(int a);
int M_Dims2(int a, int b);
int M_List();
/* 单元素也包成列表：`declarator_list` / `param_list` / `call_args` 的单项分支用它，
 * 否则 `$$ = $1` 会把"一个声明节点"当成列表，遍历 kids 时什么也拿不到。 */
int M_List1(int item);
/**
 * 两个元素包成列表（运算符重载的两个操作数用它）。
 */
int M_List2(int first, int second);
void M_ListAdd(int list, int item);
int M_BinOp(int kind, int left, int right);
int M_UnOp(int kind, int operand);
int M_Name(const char *text);
int M_YieldName();
/* 运算符名（OPNAME 用序号取，动作语言里不写字符串字面量） */
int M_Op(int which);
/**
 * @brief **关键字当名字用**时的名字池下标（同样用序号取，动作里不写字符串字面量）。
 *
 * 目前只有一处：`o.set(x)` —— `set` 是容器类型的关键字（`std::lang::Set<int>`），但它也是
 * `obj` 的方法名。**关键字 Token 的词法路径不填 `text` / `pName`**（固定字面量不碰
 * 词法器的 scratch 缓冲），所以在动作里拿 `$n` 只会得到零值 —— 必须走这张表。
 * @param which 序号（0 = `set`）。
 * @return 名字池下标。
 */
int M_KeywordName(int which);
int M_Int(long long value);
/* `true` / `false` / `null` 字面量 */
int M_Bool(int value);
int M_Null();
int M_Float(double value);
int M_Str(int stringId);
int M_ConcatStringLiterals(int leftId, int rightId);
/**
 * @brief 字符字面量 `'A'` / `'\n'`：一个独立的 `char` 字节值。
 *
 * `char` 与整数之间必须显式转换；输出使用 `std::putChar`。
 * @param textId 词法拿到的原文（带两侧单引号）在名字池里的下标。
 * @return Int 节点。
 */
int M_Char(int textId);
/**
 * @brief `s[起:止]`：字符串切片（半开区间，越界自动夹到范围内；止 < 0 = 到末尾）。
 *
 * 值是**新造出来的字符串**（堆上），所以语言侧不用写逐字节的循环 —— 拼接 / 切片 /
 * 格式化都交给虚拟机里那份 C++ 运行时（`__vm_str_*`）。
 * @param base 被切的字符串。
 * @param begin 起始下标表达式。
 * @param end 结束下标表达式；-1 = 到末尾。
 * @return Slice 节点。
 */
int M_Slice(int base, int begin, int end);
int M_Var(int nameId);
int M_Bin(const char *op, int left, int right);
int M_Un(const char *op, int operand);
int M_Cast(int typeId, int value);
int M_Index(int base, int position);
int M_Member(int base, int fieldName);
/**
 * `value.0` / `value.1`：元组按编译期常量位置读取。
 */
int M_TupleMember(int base, int numberNode);
/**
 * `(value1, value2, ...)`：元组字面量。
 */
int M_Tuple(int values);
/**
 * @brief 构造集合或映射字面量节点。
 * @param kind 0 为空字面量，1 为 Set，2 为 Map。
 * @param values 元素列表或交替排列的键值列表。
 * @return 新表达式节点编号。
 */
int M_CollectionLiteral(int kind, int values);
int M_Arrow(int base, int fieldName);
int M_Call(int calleeName, int args);
/**
 * @brief 构造后缀调用；显式 Index 调用产生可读写的缓冲区索引节点。
 * @param callee 被调用的表达式节点。
 * @param args 实参列表节点。
 * @return 调用、构造器或显式缓冲区索引节点。
 */
int M_CallOn(int callee, int args);
/**
 * @brief `bint(表达式)`：构造一个大整数（十进制文本 / 整数 / 浮点）。
 *
 * `bint` 是**类型关键字**（不是普通函数名），所以它有自己一条表达式规则 +
 * 一个专用节点，而不是走"按函数名解析"那条路（那样还得把关键字塞进名字池）。
 * @param value 实参。
 * @return BigIntCtor 节点。
 */
int M_BigIntCtor(int value);
int M_BigFloatCtor(int value);
int M_Assign(int target, int value);
/**
 * @brief 将复合赋值 A op= B 展开为普通赋值 A = A op B。
 * @param kind 对应 M_BinOp 的二元运算编号。
 * @param target 赋值目标，同时作为二元运算的左操作数。
 * @param value 二元运算的右操作数。
 * @return 普通赋值节点；目标表达式的两处使用各自求值。
 */
int M_CompoundAssign(int kind, int target, int value);
/**
 * @brief 构造自增减节点。
 * @param delta 步长正负一。
 * @param prefix 是否前置。
 * @param target 可写目标。
 * @return 表达式节点。
 */
int M_Update(int delta, int prefix, int target);
/**
 * @brief 标记完整表达式的副作用检查边界。
 * @param expression 表达式节点。
 * @return 原表达式节点。
 */
int M_CheckUpdates(int expression);
int M_Block(int statements);
/**
 * @brief 保存函数类型和 fn 匿名函数共用的签名。
 * @param result 返回类型编号。
 * @param params 形参列表编号。
 * @return 签名节点编号。
 */
int M_FnSignature(int result, int params);
/**
 * @brief 校验无名形参并将共用签名转换为函数类型。
 * @param signature 签名节点编号。
 * @return 函数类型编号。
 */
int M_FnSignatureType(int signature);
/**
 * @brief 校验具名形参并将签名和函数体转换为匿名函数。
 * @param signature 签名节点编号。
 * @param body 函数体节点编号。
 * @return 匿名函数节点编号。
 */
int M_FnSignatureLambda(int signature, int body);
/**
 * @brief 将语句块包装为只执行一次、使用 break 值退出的表达式。
 * @param block 语句块节点编号。
 * @return 带值出口的表达式节点编号。
 */
int M_BlockValue(int block);
int M_If(int cond, int thenBlock, int elifs, int elseBlock);
int M_Elif(int cond, int block);
int M_While(int cond, int body);
/* `do 块 while (条件);` */
int M_DoWhile(int body, int cond);
/* 分支表构造（`M_Match` 内部复用；`switch` 语法已删除，这里只服务 `match`）。 */
int M_Switch(int subject, int cases);
/* `match (表达式) { 模式 => { ... } }`：分支表是 Case 节点的列表，不要求显式 break。 */
int M_Match(int subject, int cases);
/**
 * @brief 构造循环或块退出，显式退出优先移动内部可写局部变量并复制外部变量。
 * @param value 退出表达式编号；负值表示退出最近循环而不产生结果。
 * @retval 生成的退出语句节点编号。
 */
int M_Break(int value);
/**
 * @brief 构造指定循环层数的无值退出语句。
 * @param level 正整数节点编号；负一表示 ALL。
 * @retval 返回退出语句节点编号。
 */
int M_BreakLevel(int level);
/* 语法糖辅助：break level N; / break level all; 的层级名拼写判定（不占用 level / all 作为关键字）。
 * @param nameId 名称池编号；text 为目标文本（ASCII，大小写无关比较）。
 * @retval 名字与 text 大小写无关相等时返回 true。 */
bool M_NameIs(int nameId, const char *text);
/* 语法糖辅助：break level / break level all 的层级名判定与错误节点（文法动作不支持字符串字面量）。 */
bool M_NameIsLevel(int nameId);
bool M_NameIsAll(int nameId);
int M_ParseBadLevelSugar();
int M_ParseBadLevelAllSugar();
int M_ParseBadBreakAllSugar();
/* 一个分支：value 是 case 的表达式（default 传 -1），stmts 是语句列表 */
int M_Case(int value, int stmts);
/**
 * 区间模式 `(a,b)` / `[a,b)` / `(a,b]` / `[a,b]`：`i` = 左闭*2 + 右闭。
 */
int M_Range(int lo, int hi, int loClosed, int hiClosed);
int M_For(int init, int cond, int inc, int body);
/**
 * `foreach ([T] v in source) { ... }`；typeId=-1 表示按 Iterator<R> 推导。
 */
int M_Foreach(int typeId, int nameId, int inNameId, int source, int body);
/**
 * `loop { ... }`：无限循环，也可作为由 `break value;` 定值的表达式。
 */
/**
 * @brief 构造单表达式分支。
 * @param value 分支值。
 * @return 块节点。
 */
int M_ValueBlock(int value);
/**
 * @brief 将条件语句转为有值表达式。
 * @param statement 条件节点。
 * @return 表达式节点。
 */
int M_IfValue(int statement);
/**
 * @brief 构造有值匹配表达式。
 * @param statement 匹配节点。
 * @return 带值出口节点。
 */
int M_MatchValue(int statement);
/**
 * @brief 从分支块构造匹配分支。
 * @param patterns 模式。
 * @param guard 守卫。
 * @param block 正文块。
 * @return 分支节点。
 */
int M_CaseBlock(int patterns, int guard, int block);
/**
 * @brief 插值复用核心 format 调用。
 * @param args 格式串与实参。
 * @return 调用节点。
 */
int M_FString(int args);
/**
 * @brief 构造作用域退出回调。
 * @param statement 延迟语句。
 * @return 登记节点。
 */
int M_Defer(int statement);
/**
 * @brief 校验 scope(exit) 并构造块式回调。
 * @param mode 退出模式。
 * @param block 正文。
 * @return 登记或诊断节点。
 */
int M_ScopeExit(int mode, int block);
/**
 * @brief 接口关联类型声明。
 * @param name 名称。
 * @return 声明节点。
 */
int M_AssociatedDecl(int name);
/**
 * @brief 实现类型提供关联类型。
 * @param name 名称。
 * @param type 具体类型。
 * @return 定义节点。
 */
int M_AssociatedDef(int name, int type);
/**
 * @brief 宏结果的目标类型检查入口。
 * @param type 目标类型。
 * @param value 结果表达式。
 * @return 检查节点。
 */
int M_CheckType(int type, int value);
/**
 * @brief 定长数组值字段。
 * @param type 元素类型。
 * @param name 字段名。
 * @param dims 常量维度。
 * @return 字段节点。
 */
int M_ArrayField(int type, int name, int dims);
int M_Loop(int body);
int M_Return(int value);
/**
 * @brief 将显式 return 的控制表达式脱糖为隐藏结果变量与普通返回。
 * @param value 返回表达式的节点编号。
 * @retval 普通返回或带隐藏声明的块节点。
 */
int M_ReturnValue(int value);
/**
 * @brief 把编译期派生白名单附着于结构体或枚举声明。
 * @param traits 请求派生的接口名称列表。
 * @param declaration 目标类型声明。
 * @retval 带派生元数据的声明节点；非法请求返回诊断节点。
 */
int M_Derived(int traits, int declaration);
/**
 * @brief 在语句位置声明具有确定返回类型的函数值。
 * @param resultType 返回类型编号。
 * @param name 函数绑定名称编号。
 * @param params 参数列表编号。
 * @param body 函数体编号；负一表示只有声明。
 * @param capture 默认捕获方式，与 fn 的捕获枚举一致。
 * @retval 局部函数值声明节点。
 */
int M_NamedFunction(int resultType, int name, int params, int body, int capture);
int M_ExprStmt(int value);
/**
 * @brief 从与位与共享的具名类型前缀构造只读引用声明。
 * @param typeExpr 类型名称表达式。
 * @param nameId 声明名。
 * @param value 初始化式。
 * @param rest 同一声明中的后续声明列表，缺省 -1。
 * @return 局部声明节点；非法类型名称由语义检查拒绝。
 */
int M_ReadOnlyReferenceDecl(int typeExpr, int nameId, int value, int rest = -1);
/**
 * @brief 将共享表达式前缀解析为默认只读引用类型。
 * @param typeExpr 具名类型表达式。
 * @return 规范只读引用类型编号。
 */
int M_ReadOnlyReferenceType(int typeExpr);
int M_Decl(int nameId, int arrayDims, int init);
int M_Decl2(int nameId, int dims, int init);
/**
 * @brief `new`（count = -1）或 `new [count]`。
 *
 * **元素类型不写在 new 后面**：降级时从"被赋值的变量 / 字段的类型"取
 * （`T mut& p = new;` / `T a[] = new [n];`），所以 `new` 只能出现在声明或赋值里。
 *
 * 1.0.0 起 `new(实参)` 已删除：需要初值就写 `T v = T::new(实参);`（`static T new(...)` 工厂）。
 * @param count 元素个数表达式；-1 表示单个对象。
 */
int M_NewPtr(int count);
/**
 * @brief 构造调用：按目标类型展开一个结构体的构造函数体。
 *
 * 1.0.0 起有两个来源：
 *   * 用户语法 `T v = new(实参);`（类型由声明目标推，实参重载在降级时挑）；
 *   * `static T new(...)` 工厂体内部的展开（`NamedConstructorFactoryBody`）。
 * @param typeId 目标类型（-1 = 由降级阶段的声明目标决定）。
 * @param args 实参列表节点。
 * @param skipName 正在展开的那条工厂的修饰名（-1 = 不指定）。
 * @return Construct 节点。
 */
int M_Construct(int typeId, int args, int skipName);
/**
 * 语法糖 `heap T` 与 `heap new T(args)` 的解析辅助（heap 是软关键字 %define，仅在 heap
 * 引导的分配位置识别为 HEAP token，普通变量名仍可使用 heap）。
 * @param typeName 类型名 T（仅作文法识别，分配 / 构造类型仍按左值推导）。
 * @param args 构造实参列表节点（仅 heap new T(args) 用）。
 */
int M_HeapSugar(int typeName);
int M_HeapNewSugar(int typeName, int args);
/**
 * `delete p;`：把可写引用指向的堆块还回去。
 */
int M_Delete(int pointer);
int M_Drop(int value);
/**
 * @brief 构造循环或块退出，显式退出优先移动内部可写局部变量并复制外部变量。
 * @param value 退出表达式编号；负值表示退出最近循环而不产生结果。
 * @retval 生成的退出语句节点编号。
 */
int M_Break(int value);
/**
 * @brief 构造指定循环层数的无值退出语句。
 * @param level 正整数节点编号；负一表示 ALL。
 * @retval 返回退出语句节点编号。
 */
int M_BreakLevel(int level);
/**
 * @brief `continue;`：直接进入最内层循环的下一轮。
 *
 * `match` 不截获 `break`，也不影响 `continue` 的目标（就是外面那层循环）。
 */
int M_Continue();
int M_Field(int typeId, int nameId);
int M_StaticField(int typeId, int nameId);
/* opName = -1 是普通方法；否则是运算符的序号（`loperator+` / `roperator+`）。 */
int M_Method(int resultType, int nameId, int opName, int params, int body);
int M_StaticMethod(int resultType, int nameId, int params, int body);
int M_Constructor(int nameId, int params, int body);
/* 运算符重载：`C operator+ (左 left, 右 right)` —— resultType = C，
 * nameId = 名字（必须叫 `operator`），opName = 运算符序号，params = 两个形参。 */
int M_OperatorDef(int resultType, int nameId, int opName, int params, int body);
int M_Func(int resultType, int nameId, int params, int body);
/**
 * @brief 建立 lambda 表达式。
 * @param resultType 返回类型。
 * @param params 形参列表。
 * @param body 函数体。
 * @param captureMode 0 只读引用、1 可写引用、2 拷贝、3 移动。
 * @return 表达式节点。
 */
int M_Lambda(int resultType, int params, int body, int captureMode);
/**
 * @brief 建立捕获列表分组。
 * @param mode 捕获方式。
 * @param names 名称列表。
 * @return 分组节点。
 */
int M_CaptureGroup(int mode, int names);
/**
 * @brief 建立混合捕获 lambda。
 * @param resultType 返回类型。
 * @param params 形参。
 * @param body 函数体。
 * @param groups 捕获列表。
 * @param defaultMode 未列出变量的默认捕获方式。
 * @return lambda 节点。
 */
int M_LambdaListed(int resultType, int params, int body, int groups, int defaultMode);
/**
 * `inline`：把普通函数标成模块内 API。
 */
int M_Inline(int declarationId);
/**
 * `T name(...);`：无函数体的宿主签名，运行时由 C++ 胶水表提供实现。
 */
int M_ExternFunc(int resultType, int nameId, int params);
int M_AbiFunc(int resultType, int nameId, int params, int symbolId);
/**
 * @brief 为 ABI 返回值登记复制完成后调用的宿主释放符号。
 * @param function ABI 函数节点编号。
 * @param release 释放符号字符串编号。
 * @return 原函数节点编号。
 */
int M_AbiOwned(int function, int release);
/**
 * @brief 标记只能由 AOT 提供的 C 导出函数。
 * @param function 普通函数定义节点。
 * @return 原节点编号。
 */
int M_ExportFunc(int function);
int M_AbiBufferFunc(int resultType, int nameId, int params, int symbolId, int lengthId);
int M_AbiBlock(int libraryId, int functionsId);
/**
 * `type Name = ABI类型;`：声明宿主类型的 VM 表示。
 */
int M_HostType(int nameId, int abiTypeId);
/**
 * `extern cast Target(Source) = cpp_name;`：声明一条显式宿主转换。
 */
/**
 * `impl Host : Interface = cpp_name;`：用一个胶水函数实现单方法接口。
 */
int M_HostImpl(int ownerTypeId, int interfaceNameId, int functionNameId);
/**
 * @brief `struct 名字 : 接口1, 接口2 { … };`。
 * @param nameId 结构体名。
 * @param membersId 成员表。
 * @param interfacesId 继承的接口名列表（`iface_list` 的产物）；-1 = 没有继承。
 * @return StructDecl 节点。
 */
int M_Struct(int nameId, int members, int interfacesId);
int M_PackedStruct(int nameId, int qualifierId, int members, int interfacesId);
/**
 * `enum E { A, B(int x) };`：variants 是 EnumVariant 节点列表。
 */
int M_Enum(int nameId, int variants);
int M_EnumWithMethods(int nameId, int variants, int members, int interfacesId);
/**
 * 一个枚举变体；paramsId < 0 表示无载荷，否则复用形参列表表示命名载荷。
 */
int M_EnumVariant(int nameId, int paramsId);
/**
 * `E::Variant(args...)`；无括号变体传空参数列表。
 */
int M_EnumCtor(int enumNameId, int variantNameId, int argsId);
int M_Module(int nameId);
int M_Import(int nameId);
int M_QualifiedName(int leftNameId, int rightNameId);
int M_QualifiedRef(int leftRefId, int rightNameId);
int M_QualifiedType(int refId);
/**
 * @brief 解析多段限定容器类型。
 * @param refId 限定名。
 * @param first 元素或键。
 * @param second 值类型或 -1。
 * @return 类型编号。
 */
int M_QualifiedContainer(int refId, int first, int second);
int M_StdType(int moduleNameId, int kind);
int M_StdNamedType(int moduleNameId, int typeNameId);
int M_StdContainer(int moduleNameId, int kind, int firstType, int secondType);
/**
 * @brief 解析省略 lang 的容器类型。
 * @param name 名字编号。
 * @param first 元素或键。
 * @param second 值或 -1。
 * @return 类型编号。
 */
int M_ImplicitContainer(int name, int first, int second);
/**
 * @brief 解析限定的标准容器。
 * @param moduleNameId 模块名。
 * @param typeNameId 类型名。
 * @param firstType 元素或键。
 * @param secondType 值或 -1。
 * @return 类型编号。
 */
int M_StdContainerNamed(int moduleNameId, int typeNameId, int firstType, int secondType);
/**
 * `__typeTag(std::X)`：类型名参数按名字解析标准类型。
 */
int M_StdNumberCtor(int moduleNameId, int kind, int value);
/**
 * `result?`：取 Result::Ok 的值，Result::Err 则从当前函数提前返回。
 */
int M_Try(int value);
/**
 * @brief `interface 名字 { 函数声明… };`：接口**只声明函数**（没有字段、没有函数体）。
 *
 * 接口是**编译期契约**：继承它的结构体必须把每个函数按同样的签名实现出来
 * （登记时逐个核对，漏了直接报错）。接口可以继承别的接口（见 `M_InterfaceInherit`）。
 * @param nameId 接口名。
 * @param membersId 函数声明表（`iface_member_list` 的产物）。
 * @return InterfaceDecl 节点。
 */
int M_Interface(int nameId, int membersId);
/**
 * @brief `interface A : B, C { … };` —— 带基接口的接口声明。
 * @param nameId 接口名。
 * @param basesId 基接口名列表（`iface_list` 的产物）。
 * @param membersId 函数声明表（`iface_member_list` 的产物）。
 * @return InterfaceDecl 节点（`i` 记基接口名列表）。
 */
int M_InterfaceInherit(int nameId, int basesId, int membersId);
/**
 * @brief 接口方法：`int area();` 或 `int area() { ... }`。
 * @param resultType 返回类型。
 * @param nameId 函数名。
 * @param params 形参表。
 * @param body 默认方法体的节点编号，抽象方法为 -1。
 * @return 一个 MethodDef 节点（`kids` = {形参表, 方法体}）。
 */
/**
 * @brief 构造编译期静态接口要求。
 * @param resultType 返回类型。
 * @param nameId 名称。
 * @param params 参数。
 * @return 方法节点。
 */
int M_StaticIfaceMethod(int resultType, int nameId, int params);
int M_IfaceMethod(int resultType, int nameId, int params, int body);
int M_InterfaceFieldRejected(int nameId);
/**
 * @brief `__typeTag(类型)`：这个类型的**编号**（编译期就折成一个整数）。
 * @param typeId 类型池下标。
 * @return NameOf 节点。
 */
int M_TypeTagType(int typeId);

int M_Param(int typeId, int nameId);
/**
 * @brief 保存形参默认表达式。
 * @param paramId 形参节点。
 * @param exprId 默认值表达式。
 * @return 形参节点。
 */
int M_DefaultParam(int paramId, int exprId);
int M_VariadicParam(int typeId, int nameId);
/**
 * 全局声明：typeId = 类型池下标，constFlag 非 0 表示不可变变量（declare 到全局）。
 */
int M_GlobalDecl(int typeId, int declarators, int constFlag);
/**
 * 局部声明：与全局同一套 declarator，但发成 `(decl, T, local, x)`（按调用帧开槽）。
 */
int M_LocalDecl(int typeId, int declarators, int constFlag);
void M_SetPending(int typeId);
int M_PendingType();
/**
 * 把类型（与 `const` 标记）补写到一组声明节点上（声明规则在**外层**才知道）。
 */
void M_SetDeclType(int declaratorList, int typeId, int constFlag);
void M_SetProgram(int topList);

/**
 * 降级 + 生成 + 执行（%main 调）。
 * @param moduleName 模块名（写进 .quad 头）。
 * @param outPrefix  产物前缀：生成 `<前缀>.quad` / `.data` / `.data.bin` / `.data.h` / `.layout`。
 * @param run        非 0 时再用 library/stackvm 组装执行一遍，把输出写到 stdout。
 * @param emitBinary 非 0 时把组装结果写成 `<前缀>.svmb`。
 * @param inputText  `input(变量);` 要读的输入项（空白分隔，按顺序取；空 = 没有输入）。
 * @return 0 成功；失败时 M_LastError() 给出原因。
 */
int M_Compile(const char *moduleName, const char *outPrefix, int run, int emitBinary,
              const char *inputText);
/**
 * 设置下一次 M_Compile 执行时传给 `main(std::lang::List<std::lang::String>)` 的参数。
 */
void M_SetSourceFiles(const Memory::Vector<Memory::String> &files, const Memory::String &fallback);
void M_SetProgramArguments(int argc, const char *const *argv);
void M_SetThreadsEnabled(int enabled);
/**
 * 开关 `--debug`：带上调试节，并在执行时按源码位置逐行打印轨迹（stderr）。
 */
void M_SetDebugEnabled(int enabled);
/**
 * 设置下一次 M_Compile 执行的指令条数上限；0 表示不设上限（缺省）。
 */
void M_SetStepLimit(std::size_t limit);
/**
 * 开关 `--no-debug`（兼容别名）：写 `.svmb` 时把调试节写成 0 条；这也是缺省。
 */
void M_SetStripDebug(int enabled);
/**
 * @brief 配置符号压缩。
 * @param enabled 开关。
 * @param mapPath PC 映射路径，可空。
 * @param moduleName 模块名，可空。
 * @param version 版本，可空。
 */
void M_SetSymbolCompression(int enabled, const char *mapPath,
                            const char *moduleName = nullptr, const char *version = nullptr);
void M_SetRuntimeMemory(std::size_t bytes);
std::size_t M_RuntimePeakBytes();
/**
 * @brief 配置可选软重启看门狗（下次 `M_Compile` 执行时生效；默认关闭）。
 *
 * 打开后宿主每轮结束量一次内存水位，并在分配失败 / 程序显式请求 / 超过水位时
 * 清干净、复用缓冲区重跑，最多 `maxRestarts` 次。
 * @param enabled 开关（0 = 关闭，行为与从前一致）。
 * @param maxRestarts 最多重启几次（负数按 0 处理）。
 * @param watermarkBytes 内存水位（字节；<= 0 = 只等分配失败 / 显式请求）。
 */
void M_SetWatchdog(int enabled, int maxRestarts, int watermarkBytes);
/**
 * 为后续 M_Compile 执行挂接 C++ 胶水函数；传 nullptr 取消。
 */
void M_SetNativeRegistry(const stackvm::NativeRegistry *registry);
const char *M_LastError();

/* Token → 语义值：数字 / 浮点看**词素原文**（`2.0` 这种浮点不能用 `value`，它是 int32），
 * 标识符与字符串登记进名字池后取下标。C++17 的 Token 有 type / value / text / name / pName。 */
#define PARSER_USER_TOKEN_VALUE(pToken)                            \
    M_TokenValue((int)((pToken)->type), (double)((pToken)->value), \
                 (pToken)->pName, Memory::String((pToken)->text), (pToken)->line, (pToken)->column)

int M_TokenValue(int tokenType, double number, const char *pName, Memory::String text,
                 std::size_t line = 0, std::size_t column = 0);

/**
 * @brief 设置当前归约起始行。
 * @param line 预处理后的行号。
 */
void M_SetReductionLine(std::size_t line);
#define PARSER_USER_REDUCTION_LINE(line) M_SetReductionLine(line)
/**
 * @brief 设置源码行映射。
 * @param lines 逐行原始行号。
 */
void M_SetSourceLines(const Memory::Vector<std::size_t> &lines);

/**
 * @brief 仅完成前端检查，不生成文件或执行。
 * @param enabled 是否启用。
 */
void M_SetCheckOnly(bool enabled);
/**
 * @brief 设置仅准备 ABI 桥的执行模式。
 * @param enabled 是否跳过程序入口。
 */
void M_SetAbiPrepareOnly(bool enabled);
/**
 * @brief 设置 ABI 调用桥和缓存的显式命令行选项。
 * @param cache 缓存目录。
 * @param bridge 预编译调用桥。
 * @param output 需要保存的桥路径。
 * @param noCache 是否禁用缓存。
 */
void M_SetAbiOptions(const Memory::String &cache, const Memory::String &bridge, const Memory::String &output, bool noCache);
/**
 * @brief 打开 DAP 调试模式（`--dap`）。
 *
 * 打开后编译器在解释执行前把 VM 挂给 DAP 服务端：断点由 VS Code 下发，
 * 停下一次就等一条命令再继续；程序的标准输出改走 `output` 事件，
 * 以免冲垮 stdio 上的协议流。
 * @param enabled 是否启用 DAP 模式。
 */
void M_SetDapEnabled(bool enabled);
/**
 * @brief 设置本次编译的注解与反射开关；失败时重置为关闭。
 * @param enabled 是否请求开启。
 * @return 前端和实际运行时均支持时返回 true。
 */
bool M_SetReflectionEnabled(bool enabled);
/**
 * @brief 首个语义错误的预处理行号；由驱动映射到实际源文件。
 * @return 一基行号。
 */
std::size_t M_LastErrorLine();

/**
 * @brief 声明局部元组解构。
 * @param bindings 绑定树。
 * @param value 元组表达式。
 * @return 语法节点。
 */
int M_Destructure(int bindings, int value);
/**
 * @brief 从表达式列表构造普通语句或元组解构赋值。
 * @param expressions 一个表达式，或以赋值表达式结束的目标列表。
 * @return 表达式语句或解构赋值节点编号。
 */
int M_AssignTuple(int expressions);
/**
 * @brief 创建忽略元素绑定。
 * @return 占位节点。
 */
int M_DestructureIgnore();
/**
 * @brief 创建带 bool 守卫的模式分支。
 * @param patterns 模式。
 * @param guard 条件。
 * @param statements 分支正文。
 * @return 分支节点。
 */
int M_CaseGuard(int patterns, int guard, int statements);

/**
 * @brief 创建注解声明。
 * @param marker 起始 token 节点。
 * @param name 限定名称。
 * @param params 参数表。
 * @return 声明节点。
 */
int M_AnnotationDecl(int marker, int name, int params);
/**
 * @brief 创建注解参数。
 * @param name 参数名。
 * @param type 类型。
 * @param value 默认表达式或 -1。
 * @return 参数节点。
 */
int M_AnnotationParam(int name, int type, int value);
/**
 * @brief 创建注解使用。
 * @param marker 起始 token 节点。
 * @param name 限定名称。
 * @param args 实参表。
 * @return 使用节点。
 */
int M_AnnotationUse(int marker, int name, int args);
/**
 * @brief 创建注解实参。
 * @param name 命名实参名或 -1。
 * @param value 表达式。
 * @return 实参节点。
 */
int M_AnnotationArg(int name, int value);
/**
 * @brief 将注解表挂到声明。
 * @param annotations 注解表。
 * @param declaration 声明。
 * @return 原声明节点。
 */
int M_Annotated(int annotations, int declaration);
/**
 * @brief 获取首条语义错误的精确列号，普通语义诊断为零。
 * @return 一基列或零。
 */
std::size_t M_LastErrorColumn();

/**
 * @brief 读取编译器 AST 节点；仅用于编译器内元数据生成与验证，不是 MAGEL 运行时反射接口。
 * @param id 已有语法节点下标。
 * @return 只读节点；非法下标抛出 out_of_range。
 */
const MNode &M_ParsedNode(int id);

/**
 * @brief 单条前端诊断，源码行列按预处理输入保存。
 */
struct MDiagnostic
{
    Memory::String message;
    std::size_t line = 0, column = 0;
};
/**
 * @brief 获取本次编译收集的全部诊断。
 * @return 按发现顺序保存的诊断列表。
 */
const Memory::Vector<MDiagnostic> &M_Diagnostics();

/**
 * @brief 获取本次编译的非致命警告。
 * @return 按发现顺序保存、已去重的警告列表。
 */
const Memory::Vector<MDiagnostic> &M_Warnings();
