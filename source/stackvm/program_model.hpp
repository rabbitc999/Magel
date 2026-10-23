/**
 * @file program_model.hpp
 * @brief 栈指令、源码定位和可执行程序模型。
 */
#pragma once
#include "quad_model.hpp"
#include "symbol_table.hpp"
#include "reflection.hpp"

namespace stackvm
{
/**
 * @brief 栈指令操作码。
 */
enum class Op
{
    PushInt,       ///< 压入整数立即数。
    PushDouble,    ///< 压入浮点立即数。
    PushText,      ///< 压入字符串。
    Load,          ///< 把变量压栈（`name`）。
    Store,         ///< 弹栈写入变量（`name`）。
    Declare,       ///< 在当前调用帧里开一个变量槽（`name`；`(decl, T, local, x)` 用它）。
    Alloc,         ///< 堆分配：弹栈取字节数，把地址压回栈（失败就终止运行）。
    Free,          ///< 堆释放：弹栈取地址（`name` 是源操作数名，仅用于诊断）。
    Drop,          ///< 拥有值清理：跳过空槽和数据区；失效堆地址及已释放对象报错。
    Check,         ///< 下标越界陷阱：弹栈取下标与上限，越界就终止（`name` 是给诊断看的标签）。
    CheckPointer,  ///< 按区块容量查下标：弹栈取下标与基址，`intValue` 是元素步长。
    CallIndirect,  ///< 间接调用：弹栈取"函数指针"（数据区里一串标签字节的地址），再按它调用。
    Neg,           ///< 取负。
    Not,           ///< 按位取反。
    Add,
    Sub,
    Mul,
    Div,
    Mod,  ///< 算术。
    Shl,
    Shr,
    ShrU,  ///< 移位（算术 / 逻辑右移）。
    BitAnd,
    BitOr,
    BitXor,  ///< 位运算。
    Eq,
    Ne,
    Lt,
    Le,
    Gt,
    Ge,  ///< 有符号比较。
    LtU,
    LeU,
    GtU,
    GeU,  ///< 无符号比较。
    ToDouble,
    ToInt,    ///< 类型转换。
    MulWide,  ///< 双结果乘法：压入 lo，再压 hi。
    DivWide,  ///< 双结果除法：压入 q，再压 r。
    Jmp,
    Jz,
    Jnz,        ///< 跳转（`name` 是标签）。
    Print,      ///< 打印栈顶（`format`）。
    PrintText,  ///< 打印字符串。
    Read,       ///< 读一项到变量（`name`）。
    Getc,       ///< 读一个字符到变量（`name`）。
    Putc,       ///< 输出栈顶字符。
    Param,      ///< 记一个实参（`name` 是形参名）。
    Call,       ///< 调用（`name` 是标签，`name2` 是返回值目的地）。
    Ret,        ///< 返回：弹栈顶作为返回值。
    Halt,       ///< 停机。
    AddrOf,     ///< 把数据区符号的基址压栈（`name` 是符号名）。
    LoadAt,     ///< 弹地址，按 intValue 指定的 u8 或旧八字节宽度读内存。
    StoreAt,    ///< 弹值和地址，按 intValue 指定的 u8 或旧八字节宽度写内存。
};

/**
 * @brief 一条栈指令。
 */
struct Instruction
{
    Op op = Op::Halt;                          ///< 操作码。
    Memory::String name;                       ///< 变量名 / 标签 / 形参名。
    Memory::String name2;                      ///< `Call` 的返回值目的地。
    std::int64_t intValue = 0;                 ///< 整数立即数；宽乘除时为位宽，LoadAt/StoreAt 为 1 时访问一个 u8，0 保持八字节。
    double doubleValue = 0.0;                  ///< 立即数（浮点）。
    Memory::String textValue;                  ///< 立即数（字符串）。
    bool unsignedOp = false;                   ///< `MulWide` / `DivWide` 是否按无符号做。
    bool isFloat = false;                      ///< `LoadAt` / `StoreAt`：这 8 个字节按 IEEE-754 双精度解释。
    PrintFormat format = PrintFormat::Signed;  ///< 打印格式。
    std::size_t quadIndex = 0;                 ///< 来自第几条四元式（从 0 开始）。

    /**
     * @brief 反汇编成一行文本。
     * @return 文本。
     */
    [[nodiscard]] Memory::String toString() const;
};

namespace execution_detail
{
/**
 * @brief 内部寄存器操作；保留原始指令用于逐步观察和异常回退。
 */
enum class RegisterOp
{
    None,
    IntegerStore,   ///< 整数二元运算，结果写一个槽。
    IntegerBranch,  ///< 整数比较（可带结果写回），按结果跳转。
    FloatStore,     ///< 浮点二元运算，结果写一个槽。
    FloatBranch,    ///< 浮点比较（可带结果写回），按结果跳转。
    Wide,           ///< 双结果乘除：`mul` 写 lo / hi，`div` 写 q / r。
    Unary,          ///< 一元运算（取负、按位取反），结果写一个槽。
    Copy            ///< 装载或立即数直接写入槽。
};
/**
 * @brief 指令选择的统计，供 `--stats` 与回归断言使用。
 */
struct SelectionStats
{
    std::size_t instructions = 0;      ///< 选出的目标指令条数。
    std::size_t covered = 0;           ///< 被目标指令覆盖的栈指令条数。
    std::size_t total = 0;             ///< 栈指令总条数。
    std::size_t byKind[7] = {};        ///< 按 `RegisterOp` 分类的条数。
    std::size_t byOperation[64] = {};  ///< 按基础操作码分类的条数。
};

/**
 * @brief 一次分派的内部指令，操作数引用预绑定槽位，PC 映射保持原样。
 */
struct RegisterInstruction
{
    bool floatingLiteral = false;  ///< 立即数是否为浮点。
    double realImmediate = 0.0;    ///< 浮点立即数。
    RegisterOp kind = RegisterOp::None;
    Op operation = Op::Add;
    std::size_t left = static_cast<std::size_t>(-1), right = static_cast<std::size_t>(-1), destination = static_cast<std::size_t>(-1), target = static_cast<std::size_t>(-1);
    std::size_t destination2 = static_cast<std::size_t>(-1);  ///< 双结果乘除的第二个目的地（hi / r）。
    std::int64_t immediate = 0;
    /* 位宽不超过 64、跨度不超过一个模式窗口（≤ 6 条），用窄类型把这条热结构压回 88 字节：
     * 一条产物要按指令存两份（选择结果 + 加载时重建的对照），五十万条指令就是几十 MB。 */
    std::uint8_t bits = 0;
    std::uint8_t span = 4;
    bool literal = false, unsignedResult = false, jumpOnZero = false, storeResult = true;
    bool floatOperation = false;  ///< 运算与结果按浮点处理。
};
}  // namespace execution_detail

/**
 * @brief 一条指令对应的源码位置（调试信息）。
 *
 * `file` / `line` 指向**语言源码**，`quadLine` 指向四元式文本的行。CHALI 前端通过四元式来源元数据填写前者，旧四元式仍保留后者
 * （四元式行），所以两个都留着：谁有位置就填谁，`toString()` 按"越接近源码越优先"挑一个显示。
 */
struct SourceLocation
{
    Memory::String function;   ///< 所属函数 / 标签；空表示根或未知。
    Memory::String file;       ///< 源文件名（`.chl` 或 `.quad`）；空表示不知道。
    std::int32_t line = 0;     ///< 源文件行号（1 基）；0 = 未知。
    std::size_t quadLine = 0;  ///< 四元式文本行号（1 基）；0 = 未知。

    /**
     * @brief 有没有任何可展示的位置信息。
     */
    [[nodiscard]] bool known() const { return line > 0 || quadLine > 0 || !function.empty(); }

    /**
     * @brief 位置文本：优先 `file:line`，退而 `.quad:行`（四元式文本行），再退到函数名。
     * @return 短文本；什么都没有时返回 `?`。
     */
    [[nodiscard]] Memory::String toString() const;
};

/**
 * @brief 稀疏的「指令下标 → 源码位置」表。
 *
 * 不必每条指令都登记：查询取**不超过目标下标的最近一条**，所以一条语句里的几十条指令
 * 共用一条记录就够了（函数入口、语句起点各一条即可）。
 */
struct DebugInfo
{
    /**
     * @brief 指令下标（递增）与对应位置，一一对应。
     */
    Memory::Vector<std::pair<std::size_t, SourceLocation>> entries;

    /**
     * @brief 条数。
     */
    [[nodiscard]] std::size_t size() const { return entries.size(); }
    /**
     * @brief 是否为空。
     */
    [[nodiscard]] bool empty() const { return entries.empty(); }

    /**
     * @brief 追加一条记录；与上一条位置完全相同则跳过（省地方）。
     * @param instruction 指令下标；必须不小于已有的最后一条。
     * @param location 位置。
     */
    void add(std::size_t instruction, SourceLocation location);

    /**
     * @brief 取不超过 instruction 的最近一条记录。
     * @param instruction 指令下标。
     * @return 命中返回指针；表里没有更早的记录返回 nullptr。
     */
    [[nodiscard]] const SourceLocation *lookup(std::size_t instruction) const;
};

/**
 * @brief 组装好的栈程序。
 */
struct Program
{
    reflection::Table reflection;            ///< 编译产物拥有的可选反射元数据。
    std::uint32_t requiredFeatures = 0;      ///< 程序要求的运行时能力位，随二进制保存。
    bool compactSymbols = false;             ///< 从无名紧凑格式加载，运行时用稳定编号寻址。
    Memory::Set<Memory::String> symbolKeys;  ///< 加载后的编号键集合，避免线性查找。
    Memory::Vector<SymbolRecord> symbols;    ///< 按物理索引排列的符号。
    /**
     * @brief 将外部原名转换为程序寻址键。
     * @param value 原名或已有编号键。
     * @return 与当前程序兼容的键。
     */
    [[nodiscard]] Memory::String symbolKey(const Memory::String &value) const
    {
        if (!compactSymbols || value.empty()) return value;
        if (value.front() == '#')
        {
            if (symbolKeys.count(value)) return value;
        }
        return symbols::key(symbols::logicalId(value));
    }
    Memory::String name;                                                 ///< 名字（取 `# 名称:`）。
    Memory::Vector<execution_detail::RegisterInstruction> internalCode;  ///< 已校验的内部执行指令。
    execution_detail::SelectionStats selectionStats;                     ///< 指令选择的覆盖统计。
    bool preparedInternal = false;                                       ///< 从新版产物加载后直接使用内部执行序列。
    Memory::Vector<Instruction> code;                                    ///< 指令序列。
    Memory::Map<Memory::String, std::size_t> labels;                     ///< 标签 → 指令下标。
    Memory::Map<Memory::String, TypeInfo> types;                         ///< 变量 → 类型。
    /**
     * @brief 标签 → 形参名（按声明顺序）。
     *
     * 由 `assemble` 扫出来：**紧挨着 `(label …)` 之前那一段连续的 `(decl …)`**
     * 就是这个函数的形参（前端把形参声明排在函数标签前面）。函数指针的间接调用
     * 按"第几个实参"绑定，靠的就是这张表 —— 直接调用是从调用点写着的形参名绑的。
     */
    Memory::Map<Memory::String, Memory::Vector<Memory::String>> parameters;
    /**
     * @brief 可供二进制模块调用者链接的公开标签；裸汇编默认全公开，模块前端可收窄。
     */
    Memory::Set<Memory::String> exports;
    /**
     * 调试信息：指令下标 → 源码位置。组装时按"四元式行的来源"自动填，
     * 前端如果能给出 `.chl` 的文件与行号，也会一起存进来。
     */
    DebugInfo debug;
    Memory::Vector<Memory::String> diagnostics;  ///< 组装诊断（不支持的指令等）。

    /**
     * @brief 是否组装成功。
     * @return 没有诊断返回 true。
     */
    [[nodiscard]] bool ok() const { return diagnostics.empty(); }
};

/**
 * @brief 检查能力位及元数据的完整性，供保存、组装和直接执行共用。
 * @param program 程序。
 * @param diagnostics 可选诊断输出。
 * @return 合法且当前构建支持返回真。
 */
bool validateProgramMetadata(const Program &program, Memory::Vector<Memory::String> *diagnostics = nullptr);

/**
 * @brief stackvm 二进制程序文件的推荐后缀。
 */
inline constexpr std::string_view kProgramBinaryExtension = ".svmb";

/**
 * @brief 判断文件是否带 stackvm 二进制程序魔数。
 * @param path 文件路径。
 * @return 是二进制程序返回 true；打不开或不是返回 false。
 */
[[nodiscard]] bool isProgramBinary(const Memory::String &path);

/**
 * @brief 把已经组装好的程序写成稳定的小端二进制格式。
 * @param program 程序。
 * @param path 输出路径。
 * @param diagnostics 失败诊断（可为空）。
 * @param compressSymbols 写无名符号表及变长物理索引。
 * @return 成功返回 true。
 */
bool saveProgramBinary(const Program &program, const Memory::String &path,
                       Memory::Vector<Memory::String> *diagnostics = nullptr,
                       bool compressSymbols = false);
/**
 * @brief 写仅供 PC 保存的符号映射，内容与压缩输出的排序一致。
 * @param program 原始程序。
 * @param path JSON 输出。
 * @param module 模块名。
 * @param version 模块版本字符串。
 * @param diagnostics 错误诊断。
 * @return 成功与否。
 */
bool saveSymbolMap(const Program &program, const Memory::String &path,
                   const Memory::String &module = {}, const Memory::String &version = {},
                   Memory::Vector<Memory::String> *diagnostics = nullptr);

/**
 * @brief 从 stackvm 二进制文件直接恢复组装后的程序。
 * @param path 输入路径。
 * @param program 输出程序。
 * @param diagnostics 失败诊断（可为空）。
 * @return 格式完整且版本受支持时返回 true。
 */
bool loadProgramBinary(const Memory::String &path, Program &program,
                       Memory::Vector<Memory::String> *diagnostics = nullptr);

/**
 * @brief 从**内存字节**直接恢复组装后的程序（目标宿主：映像在 flash 里，没有文件系统）。
 *
 * 与路径版共用同一套解码、校验与能力检查，区别只是不碰文件系统；`label` 只用于诊断文案。
 * @param bytes 映像首地址。
 * @param size 字节数。
 * @param program 输出程序。
 * @param diagnostics 失败诊断（可为空）。
 * @return 成功返回 true。
 */
bool loadProgramBinary(const void *bytes, std::size_t size, Program &program,
                       Memory::Vector<Memory::String> *diagnostics = nullptr);

/**
 * @brief 把四元式组装成栈指令。
 * @param file 四元式文件。
 * @param out 组装结果。
 * @return 成功返回 true（`out.diagnostics` 里给出不支持的原因）。
 */
bool assemble(const QuadFile &file, Program &out);
}  // namespace stackvm
