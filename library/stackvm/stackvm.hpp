#include "../../source/magel_i18n.hpp"
#include "../../source/allocator/magel_allocator.hpp"
/**
 * @file stackvm.hpp
 * @brief 栈式虚拟机：读 `library/transfer` 那套四元式，编译成栈指令并执行。
 *
 * ## 为什么要"接上栈虚拟机"
 *
 * `library/transfer` 那条线是「四元式 → 基本块 / CFG → 优化 → 寄存器型目标代码
 * （MIPS / 8086 / ARM）→ 模拟执行」。寄存器型后端要处理寄存器分配、调用约定、
 * 机器字长分片；而**栈式虚拟机**把这些都换成了"求值栈 + 变量槽"：
 *
 * ```text
 * 四元式             栈指令                          执行
 * (:=, 2, _, x)      PUSH 2; STORE x               取 2 存进 x
 * (+, x, 3, t1)      LOAD x; PUSH 3; ADD; STORE t1 压栈相加再存
 * (jz, t1, _, L)     LOAD t1; JZ L                 假就跳
 * (write, t1,_,_)    LOAD t1; PRINT                打印
 * ```
 *
 * 于是"后端"只需要一个栈和一个跳转表，代价是每条指令都要访存——这正是教学上
 * 最有价值的那组对照（`project/mini-basic` 里的栈式 VM 也是这个模型）。
 *
 * ## 支持的指令子集
 *
 * | 四元式 | 说明 |
 * | --- | --- |
 * | `decl` | 变量类型与作用域（`global` / `local`；`local` 会在当前调用帧里开槽） |
 * | `:=` | 赋值 |
 * | `+ - * / %`、`& \| ^`、`<< >>`、`neg` | 算术 / 位运算（按 `:类型` 后缀取符号与回绕） |
 * | `== != < <= > >=` | 比较（有符号 / 无符号按后缀） |
 * | `mul` / `div`（4~5 个参数） | 双结果：`lo/hi`、`q/r` |
 * | `cvt:` | 整数 ↔ 浮点 |
 * | `label` / `j` / `jz` / `jnz` | 标签与跳转 |
 * | `read` / `getc` | 从输入取一项 / 取一个字符 |
 * | `write` / `print` / `putc` | 输出数值 / 字符串 / 字符 |
 * | `param` / `call` / `ret` | 调用与返回（局部变量按帧存放，所以能递归） |
 * | `alloc` / `free` | 堆：`(alloc, 字节数, _, 目标)` 拿到一个地址，`(free, 地址, _, _)` 还回去 |
 * | `check` | 下标陷阱：`(check, 下标, 上限, 标签)`，越界就终止运行 |
 * | `checkptr` | 按区块容量查下标：`(checkptr, 下标, 基址, 步长, 标签)`（堆块 / 数据区符号的 extent） |
 * | `call 名字` | 名字不在标签表里时先查**原生函数表**（数学 / 字符串 / 文件 / 进程等 `__vm_*`） |
 * | `halt` | 停机 |
 *
 * 不支持的（宽类型分片 `+c`/`-b`、中断 `int`/`iret`、数据区等）会在组装期报一条
 * 诊断并跳过——它们属于"机器字长模型"和"目标代码生成"的范畴，本库不重复实现。
 */

#ifndef PARSE_ENGINE_STACKVM_HPP
#define PARSE_ENGINE_STACKVM_HPP

/**
 * @def STACKVM_WITH_REFLECTION
 * @brief 是否编入注解与反射运行时支持；默认关闭，只允许 0 或 1。
 */
#ifndef STACKVM_WITH_REFLECTION
#define STACKVM_WITH_REFLECTION 0
#endif
#if STACKVM_WITH_REFLECTION != 0 && STACKVM_WITH_REFLECTION != 1
#error STACKVM_WITH_REFLECTION must be 0 or 1
#endif

/**
 * @def STACKVM_WITH_ALLOCATION_GUARDS
 * @brief 默认校验完整访问范围属于活跃分配；AOT unchecked 可裁剪此额外护栏。
 * 分配器自身的底层边界检查不受此宏影响。
 */
#ifndef STACKVM_WITH_ALLOCATION_GUARDS
#define STACKVM_WITH_ALLOCATION_GUARDS 1
#endif
#if STACKVM_WITH_ALLOCATION_GUARDS != 0 && STACKVM_WITH_ALLOCATION_GUARDS != 1
#error STACKVM_WITH_ALLOCATION_GUARDS must be 0 or 1
#endif

/**
 * @def STACKVM_WITH_PANIC_TRACE
 * @brief 默认记录 panic/运行错误调用栈；设为 0 或定义 NO_MAGEL_PANIC_TRACE 可裁剪。
 * 关闭后不自动保留错误栈，显式调试回调仍可查看调试帧。
 */
#ifdef NO_MAGEL_PANIC_TRACE
#undef STACKVM_WITH_PANIC_TRACE
#define STACKVM_WITH_PANIC_TRACE 0
#elif !defined(STACKVM_WITH_PANIC_TRACE)
#define STACKVM_WITH_PANIC_TRACE 1
#endif

/**
 * @def STACKVM_WITH_THREADS
 * @brief 是否编入 **OS 线程 / 互斥 / 条件变量 / 线程共享值 / 宿主中断邮箱**；板端口径默认 0。
 *
 * `1`（缺省，桌面行为）= 编入 `std::thread` 那套线程运行时、`std::thread::*` 与
 * `std::concurrency::Shared` 原生函数、宿主投递中断的邮箱，以及运行内存池的互斥锁。
 * `0`（板端）= 整段裁掉：**不出现 `std::thread` / `std::mutex` / `std::condition_variable`
 * / `std::chrono`**，`setThreadsEnabled` / `setThreadHeapFactory` / `raiseInterrupt` /
 * `clearInterrupts` 不存在，`std::thread::*` 与 `Shared` 走"函数入口不存在"的普通错误。
 *
 * **协程不受影响**：`std::coroutine::*` 与 `__vm_message_*_coroutine` 是 VM 自己的状态机
 * （在求值循环里切换，不需要 OS 线程），板端照常可用 —— 这就是"板端不需要线程"的那条线。
 *
 * 关线程是**零成本裁剪**：桌面构建不用改代码，板端 `-DSTACKVM_WITH_THREADS=0` 即可
 * （`magel/source/stackvm_mcu/Makefile` 已经这么传）。默认字节堆同理走 `STACKVM_WITH_DEFAULT_HEAP`。
 */
#ifndef STACKVM_WITH_THREADS
#define STACKVM_WITH_THREADS 1
#endif
#if STACKVM_WITH_THREADS != 0 && STACKVM_WITH_THREADS != 1
#error STACKVM_WITH_THREADS must be 0 or 1
#endif

/**
 * @def STACKVM_WITH_DEFAULT_HEAP
 * @brief 是否编入**默认字节堆**（`parts/machine_natives.cpp` 的 `Heap`：STL 容器实现）。
 *
 * `1`（缺省，桌面行为）= 宿主没 `setHeapAllocator` 时用这个堆，分配失败靠 STL 扩容报错。
 * `0`（板端）= 整个类裁掉（连带它的互斥锁与 STL 容器），此时**必须**由宿主注入
 * `HeapAllocator`（`SetBytePool`）；没注入时所有堆分配明确失败，不会退化成宿主堆。
 */
#ifndef STACKVM_WITH_DEFAULT_HEAP
#define STACKVM_WITH_DEFAULT_HEAP 1
#endif
#if STACKVM_WITH_DEFAULT_HEAP != 0 && STACKVM_WITH_DEFAULT_HEAP != 1
#error STACKVM_WITH_DEFAULT_HEAP must be 0 or 1
#endif

/**
 * @def STACKVM_EMBEDDED
 * @brief 使用注入式板端控制台；桌面缺省为零。
 */
#ifndef STACKVM_EMBEDDED
#define STACKVM_EMBEDDED 0
#endif
/**
 * @def STACKVM_WITH_HOST_EXCEPTION_GUARD
 * @brief 宿主回调异常保护；无异常构建设为零。
 */
#ifndef STACKVM_WITH_HOST_EXCEPTION_GUARD
#define STACKVM_WITH_HOST_EXCEPTION_GUARD 1
#endif
/**
 * @def STACKVM_WITH_HOST_IO
 * @brief 是否编入按文件路径加载和保存的接口实现。
 */
#ifndef STACKVM_WITH_HOST_IO
#define STACKVM_WITH_HOST_IO 1
#endif
/**
 * @def STACKVM_WITH_AOT
 * @brief 是否编入 AOT 运行时桥；无线程构建缺省关闭。
 */
#ifndef STACKVM_WITH_AOT
#define STACKVM_WITH_AOT STACKVM_WITH_THREADS
#endif

#include "stackvm_glue.hpp"
#include "reflection.hpp"

/**
 * @def STACKVM_WITH_BIGINT
 * @brief **大整数（`bint`）是附加功能**：这个宏决定要不要把它编进虚拟机。
 *
 * * `1`（缺省）= 编进来：`__vm_bint_*` 与 `__vm_read_bint` 这批原生函数可用，
 *   本库需要 `library/big_number`（`bigint.hpp`）；
 * * `0` = 不编：不 include big_number、不链 bigint.o，`bint` 那批原生函数**不存在**
 *   —— 调它们会得到一句"这个构建没把大整数编进来"的运行期错误，而不是"函数入口不存在"。
 *
 * 命令行开关（Makefile 里已接）：
 *
 * ```sh
 * make -C library/stackvm STACKVM_WITH_BIGINT=0      # 不带大整数的轻量构建
 * ```
 *
 * 想在自己的宿主里看当前构建支不支持，用下面的 `kBigIntEnabled`（编译期常量，
 * `if constexpr` 用得）。
 */
#ifndef STACKVM_WITH_BIGINT
#define STACKVM_WITH_BIGINT 1
#endif

/**
 * @def STACKVM_WITH_BIGFLOAT
 * @brief 任意精度十进制浮点（`bfloat`）开关；缺省跟随大整数开关。
 *
 * BigFloat 的尾数由 BigInt 保存，因此不能在关闭 BigInt 时单独开启 BigFloat。
 */
#ifndef STACKVM_WITH_BIGFLOAT
#define STACKVM_WITH_BIGFLOAT STACKVM_WITH_BIGINT
#endif
#if STACKVM_WITH_BIGFLOAT && !STACKVM_WITH_BIGINT
#error "STACKVM_WITH_BIGFLOAT requires STACKVM_WITH_BIGINT"
#endif

#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace stackvm
{
/**
 * @brief 注解与反射能力位；普通格式的最低位保留给载荷加密。
 */
inline constexpr std::uint32_t kFeatureReflection = 0x0002u;
/**
 * @brief 已定义的程序能力位，未知位必须拒绝。
 */
inline constexpr std::uint32_t kKnownProgramFeatures = kFeatureReflection;
/**
 * @brief 查询实际链接的运行时能力，避免只检查调用者的头文件配置。
 * @return 当前运行时支持的能力位。
 */
[[nodiscard]] std::uint32_t supportedProgramFeatures();
/**
 * @brief 验证程序能力要求；未启用反射的普通程序可在两种运行时执行。
 * @param required 程序要求的能力位。
 * @param diagnostics 可选诊断输出。
 * @return 全部能力已定义且当前运行时支持时返回 true。
 */
[[nodiscard]] bool validateProgramFeatures(std::uint32_t required,
                                           Memory::Vector<Memory::String> *diagnostics = nullptr);

/**
 * @brief 稳定符号编号与严格 ULEB128 编解码。
 */
namespace symbols
{
/**
 * @brief 变长整数的明确错误码。
 */
enum class Error
{
    Ok,
    Truncated,
    Overflow,
    Overlong,
    NonCanonical
};
/**
 * @brief 写最短 ULEB128。
 * @param out 输出。
 * @param value 数值。
 */
template <class Vector>
void writeVarUInt(Vector &out, std::uint64_t value)
{
    do
    {
        auto byte = static_cast<std::uint8_t>(value & 127u);
        value >>= 7u;
        out.push_back(static_cast<std::uint8_t>(byte | (value ? 128u : 0u)));
    } while (value);
}
/**
 * @brief 严格读取 ULEB128，失败时保持游标与输出。
 * @param data 输入。
 * @param size 输入长度。
 * @param pos 游标。
 * @param value 结果。
 * @return 错误码。
 */
inline Error readVarUInt(const std::uint8_t *data, std::size_t size,
                         std::size_t &pos, std::uint64_t &value)
{
    std::size_t cursor = pos;
    std::uint64_t decoded = 0;
    for (unsigned i = 0; i < 10; ++i)
    {
        if (cursor >= size || data == nullptr) return Error::Truncated;
        const auto byte = data[cursor++];
        if (i == 9 && (byte & 128u)) return Error::Overlong;
        if (i == 9 && (byte & 126u)) return Error::Overflow;
        decoded |= std::uint64_t(byte & 127u) << (7u * i);
        if (!(byte & 128u))
        {
            if (i && (byte & 127u) == 0) return Error::NonCanonical;
            pos = cursor;
            value = decoded;
            return Error::Ok;
        }
    }
    return Error::Overlong;
}
/**
 * @brief 计算 UTF-8 名字的 FNV-1a 64 稳定编号。
 * @param name 原始名字。
 * @return 编号，编译端须检测碰撞。
 */
template <class String>
std::uint64_t logicalId(const String &name)
{
    std::uint64_t hash = 14695981039346656037ull;
    for (unsigned char c : name)
    {
        hash ^= c;
        hash *= 1099511628211ull;
    }
    return hash;
}
/**
 * @brief CRC32/IEEE。
 * @param data 数据。
 * @param size 长度。
 * @return 校验值。
 */
inline std::uint32_t crc32(const std::uint8_t *data, std::size_t size)
{
    std::uint32_t crc = ~0u;
    for (std::size_t i = 0; i < size; ++i)
    {
        crc ^= data[i];
        for (unsigned j = 0; j < 8; ++j) crc = (crc >> 1u) ^ (0xedb88320u & (0u - (crc & 1u)));
    }
    return ~crc;
}
/**
 * @brief 运行时不含原名的编号键。
 * @param id 编号。
 * @return 内部键。
 */
inline Memory::String key(std::uint64_t id) { return "#" + Memory::toString(id); }
}  // namespace symbols
/**
 * @brief 紧凑符号条目；original 仅在编译端映射表存在，加载器填编号键。
 */
struct SymbolRecord
{
    std::uint64_t logicalId = 0, physicalIndex = 0, offset = 0, size = 0, refCount = 0;
    std::uint8_t kind = 0, flags = 0;
    std::int32_t line = 0;
    Memory::String original;
};

struct ThreadRuntime;
struct SharedRuntime;
struct InterruptMailbox;
class Machine;

/**
 * VM 内存的最小寻址/分配单元：1 字节。宽值连续占用多个单元。
 */
inline constexpr std::size_t kRuntimeCellBytes = 1u;
/**
 * 当前整数、浮点、地址和对象句柄的存储宽度。
 */
inline constexpr std::size_t kScalarStorageBytes = 8u;

// ---------------------------------------------------------------------------
//  四元式（与 library/transfer 的文本格式兼容）
// ---------------------------------------------------------------------------

/**
 * @brief 一条四元式：`(op, arg1, arg2, result[, extra])`。
 */
struct Quad
{
    Memory::String sourceFile;            ///< 四元式对应的 MAGEL 源文件。
    std::int32_t sourceLine = 0;          ///< MAGEL 原始行号，零为未知。
    std::size_t line = 0;                 ///< 源码行号（1 基，诊断用）。
    Memory::String op;                    ///< 操作符，可能带类型后缀（`+:i8`）。
    Memory::Vector<Memory::String> args;  ///< 参数（`_` 表示空位）。

    /**
     * @brief 取下第 index 个参数；`_` 与不存在都算空。
     * @param index 下标。
     * @return 参数文本。
     */
    [[nodiscard]] Memory::String arg(std::size_t index) const;
};

/**
 * @brief 一个 `.quad` 文件解析出来的内容。
 */
struct QuadFile
{
    reflection::Table reflection;                ///< 可选反射元数据，优化不得裁剪。
    std::uint32_t requiredFeatures = 0;          ///< 程序要求的运行时能力位，不随优化裁剪。
    Memory::String path;                         ///< 文件路径。
    Memory::String name;                         ///< `# 名称:`。
    Memory::String expected;                     ///< `# 期望:`（示例套件用来比对）。
    Memory::String expectedText;                 ///< `# 期望文本:`。
    Memory::String input;                        ///< `# 输入:`（`read` / `getc` 的数据）。
    Memory::Vector<Quad> quads;                  ///< 四元式。
    Memory::Vector<Memory::String> diagnostics;  ///< 解析诊断。

    /**
     * @brief 是否解析成功。
     * @return 没有诊断返回 true。
     */
    [[nodiscard]] bool ok() const { return diagnostics.empty(); }
};

/**
 * @brief 解析 `.quad` 文本。
 * @param text 文本（UTF-8）。
 * @param label 诊断里显示的文件名。
 * @return 解析结果。
 */
[[nodiscard]] QuadFile parseQuads(std::string_view text, Memory::String label);

/**
 * @brief 读一个 `.quad` 文件。
 * @param path 路径。
 * @return 解析结果（打不开时诊断里给消息）。
 */
[[nodiscard]] QuadFile loadQuadFile(const Memory::String &path);

/**
 * @brief 编码带字符串表的二进制四元式，不写入宿主路径。
 * @param file 已解析的四元式。
 * @return 编码字节；输入含诊断时返回空。
 */
[[nodiscard]] Memory::String encodeQuadBinary(const QuadFile &file);
/**
 * @brief 解码二进制四元式，严格校验版本、引用和边界。
 * @param bytes 编码字节。
 * @param label 诊断路径。
 * @return 四元式或诊断。
 */
[[nodiscard]] QuadFile decodeQuadBinary(std::string_view bytes, Memory::String label);
/**
 * @brief 数据符号的二进制布局记录。
 */
struct LayoutRow
{
    Memory::String name;                           ///< 符号名称。
    std::size_t offset = 0, size = 0, stride = 0;  ///< 字节偏移、容量和步长。
    bool hasSize = false, hasStride = false;       ///< 可选字段是否存在。
};
/**
 * @brief 解析文本或二进制布局，失败不修改输出。
 * @param bytes 布局数据。
 * @param rows 输出记录。
 * @return 是否成功。
 */
bool decodeDataLayout(std::string_view bytes, Memory::Vector<LayoutRow> &rows);
/**
 * @brief 将布局编码为变长整数与长度前缀字符串。
 * @param rows 布局记录。
 * @return 二进制字节。
 */
[[nodiscard]] Memory::String encodeDataLayout(const Memory::Vector<LayoutRow> &rows);

// ---------------------------------------------------------------------------
//  类型
// ---------------------------------------------------------------------------

/**
 * @brief 变量类型。
 */
struct TypeInfo
{
    Memory::String name;      ///< `i32` / `u8` / `f64` …
    int bits = 16;            ///< 位宽。
    bool isUnsigned = false;  ///< 有无符号。
    bool isFloat = false;     ///< 是否浮点。
    std::size_t size = 2;     ///< 字节数（本库只用于展示）。
};

/**
 * @brief 名字 → 类型；没写 `decl` 时按 `i16`（与 transfer 的旧语义一致）。
 * @param name 类型名；空串表示"没声明"。
 * @return 类型信息。
 */
[[nodiscard]] TypeInfo typeOf(std::string_view name);

// ---------------------------------------------------------------------------
//  构建能力
// ---------------------------------------------------------------------------

/// 这次构建有没有把大整数（`bint`）编进来（见 `STACKVM_WITH_BIGINT`）。
inline constexpr bool kBigIntEnabled = (STACKVM_WITH_BIGINT != 0);
/// 这次构建有没有把任意精度十进制浮点（`bfloat`）编进来。
inline constexpr bool kBigFloatEnabled = (STACKVM_WITH_BIGFLOAT != 0);

// ---------------------------------------------------------------------------
//  栈指令
// ---------------------------------------------------------------------------

/**
 * @brief 打印格式（由 `write` 的类型后缀或变量声明决定）。
 */
enum class PrintFormat
{
    Signed,    ///< 有符号整数。
    Unsigned,  ///< 无符号整数。
    Float,     ///< 浮点。
    Text,      ///< 字符串（`print`）。
};

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
    Drop,          ///< **软释放**：地址是活跃堆块才还回去；0 / 数据区地址 / 已经释放过 → 什么都不做。
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
    LoadAt,     ///< 弹地址，读 8 字节（小端）压栈——数据区取值。
    StoreAt,    ///< 弹值、再弹地址，往数据区写 8 字节（小端）。
};

/**
 * @brief 一条栈指令。
 */
struct Instruction
{
    Op op = Op::Halt;                          ///< 操作码。
    Memory::String name;                       ///< 变量名 / 标签 / 形参名。
    Memory::String name2;                      ///< `Call` 的返回值目的地。
    std::int64_t intValue = 0;                 ///< 立即数（整数）；`MulWide` / `DivWide` 时是位宽。
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
    IntegerStore,
    IntegerBranch
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
    std::int64_t immediate = 0;
    int bits = 0;
    std::size_t span = 4;
    bool literal = false, unsignedResult = false, jumpOnZero = false, storeResult = true;
};
}  // namespace execution_detail

/**
 * @brief 一条指令对应的源码位置（调试信息）。
 *
 * `file` / `line` 指向**语言源码**，`quadLine` 指向四元式文本的行。MAGEL 前端通过四元式来源元数据填写前者，旧四元式仍保留后者
 * （四元式行），所以两个都留着：谁有位置就填谁，`toString()` 按"越接近源码越优先"挑一个显示。
 */
struct SourceLocation
{
    Memory::String function;   ///< 所属函数 / 标签；空表示根或未知。
    Memory::String file;       ///< 源文件名（`.mgl` 或 `.quad`）；空表示不知道。
    std::int32_t line = 0;     ///< 源文件行号（1 基）；0 = 未知。
    std::size_t quadLine = 0;  ///< 四元式文本行号（1 基）；0 = 未知。

    /**
     * 有没有任何可展示的位置信息。
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
     * 指令下标（递增）与对应位置，一一对应。
     */
    Memory::Vector<std::pair<std::size_t, SourceLocation>> entries;

    /**
     * 条数。
     */
    [[nodiscard]] std::size_t size() const { return entries.size(); }
    /**
     * 是否为空。
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
    bool preparedInternal = false;                                       ///< 从新版产物加载后直接使用内部执行序列。
    Memory::Vector<Instruction> code;                                    ///< 指令序列。
    Memory::Map<Memory::String, std::size_t> labels;                     ///< 标签 → 指令下标。
    Memory::Map<Memory::String, TypeInfo> types;                         ///< 变量 → 类型。
    /**
     * 标签 → 形参名（按声明顺序）。
     *
     * 由 `assemble` 扫出来：**紧挨着 `(label …)` 之前那一段连续的 `(decl …)`**
     * 就是这个函数的形参（前端把形参声明排在函数标签前面）。函数指针的间接调用
     * 按"第几个实参"绑定，靠的就是这张表 —— 直接调用是从调用点写着的形参名绑的。
     */
    Memory::Map<Memory::String, Memory::Vector<Memory::String>> parameters;
    /**
     * 可供二进制模块调用者链接的公开标签；裸汇编默认全公开，模块前端可收窄。
     */
    Memory::Set<Memory::String> exports;
    /**
     * 调试信息：指令下标 → 源码位置。组装时按"四元式行的来源"自动填，
     * 前端如果能给出 `.mgl` 的文件与行号，也会一起存进来。
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
inline bool validateProgramMetadata(const Program &program, Memory::Vector<Memory::String> *diagnostics = nullptr)
{
    if (!validateProgramFeatures(program.requiredFeatures, diagnostics)) return false;
    Memory::String error;
    if (!(program.requiredFeatures & kFeatureReflection))
    {
        if (program.reflection.empty()) return true;
        error = "反射元数据缺少 reflection 特征位";
    }
    else if (reflection::validate(program.reflection, &error))
        return true;
    if (diagnostics) diagnostics->push_back(error);
    return false;
}

/**
 * stackvm 二进制程序文件的推荐后缀。
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
 * @brief 从**内存字节**直接恢复组装后的程序（板端：映像在 flash 里，没有文件系统）。
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

/**
 * @brief 化简统计。
 */
struct OptimizationReport
{
    std::size_t rounds = 0;                    ///< 实际迭代了几轮。
    std::size_t constantsFolded = 0;           ///< 常量折叠了几处。
    std::size_t algebraicSimplified = 0;       ///< 保留类型语义的整数恒等式化简处数。
    std::size_t deadBranches = 0;              ///< 常量条件的分支折掉了几处。
    std::size_t deadCode = 0;                  ///< 删掉了几条不可达 / 无用四元式。
    std::size_t copiesPropagated = 0;          ///< 基本块内传播或合并的复制。
    std::size_t conversionsRemoved = 0;        ///< 删除的同类型转换。
    std::size_t crossBlockConstants = 0;       ///< 控制流分析传播的常量操作数，包含循环回边。
    std::size_t commonExpressionsRemoved = 0;  ///< 删除的局部重复表达式。
    Memory::Vector<Memory::String> trace;      ///< 常量、代数与控制流化简说明。

    /**
     * @brief 一共改了多少处。
     * @return 处数。
     */
    [[nodiscard]] std::size_t total() const
    {
        return constantsFolded + algebraicSimplified + deadBranches + deadCode + copiesPropagated + conversionsRemoved + crossBlockConstants + commonExpressionsRemoved;
    }
};

/**
 * @brief 化简四元式（**就地改写** `file.quads`）。
 *
 * 在基本块内传播常量与同类型复制、折叠常量运算、消除同类型转换，
 * 合并只使用一次的临时结果与紧随其后的复制，化简保留类型语义的整数恒等式；
 * 保留动态算术的实际数值类型、窄类型存储和可能失败的读取。
 * 折叠常量条件与跳转链，删除不可达代码和安全的无用临时复制。
 * 控制流分析在合流点与循环回边求整数常量不动点，并消除块内重复表达式。
 * 调用、内存操作及未知副作用构成传播边界，不改变声明元数据及隐式输出格式。
 *
 * @param file 四元式文件。
 * @param maxRounds 最多迭代几轮（缺省 4）。
 * @param collectTrace 是否收集逐项优化说明，默认关闭以减少字符串分配。
 * @return 化简统计。
 */
[[nodiscard]] OptimizationReport optimize(QuadFile &file, std::size_t maxRounds = 4u, bool collectTrace = false);

// ---------------------------------------------------------------------------
//  执行
// ---------------------------------------------------------------------------

/**
 * 一次运行结束时保留的协程上下文摘要。id=0 是主上下文。
 */
struct CoroutineInfo
{
    std::size_t id = 0;
    Memory::String state;
    std::size_t instruction = 0;
    std::size_t frameDepth = 0;
    std::size_t stackDepth = 0;
    std::size_t waitingFor = 0;
};

/**
 * 指令钩子收到的只读执行快照。
 */
struct InstructionHookInfo
{
    std::size_t steps = 0;        ///< 本次 run() 已完成的指令总数。
    std::size_t instruction = 0;  ///< 刚执行完的程序计数器。
    std::size_t coroutine = 0;    ///< 执行该指令的协程句柄；0 是主上下文。
    Memory::String opcode;        ///< 指令操作码。
};

using TimeCallback = Memory::Function<std::uint64_t()>;
using InstructionHook = Memory::Function<void(const InstructionHookInfo &)>;

/**
 * @brief 文本输出下沉：`print` / `println` / `putChar` 产出的字节被**实时**交给它。
 *
 * 板端把串口 / OLED / RTT 的写函数挂上来（见 `Machine::setOutputSink`），就不必等
 * `run()` 结束才看到输出；不挂时输出只累积进 `RunResult::text`（桌面行为不变）。
 * @param user 挂下沉时给的上下文。
 * @param bytes 字节。
 * @param count 字节数。
 */
using OutputSink = void (*)(void *user, const char *bytes, std::size_t count);

/**
 * @brief 一次待交付的中断事件（`Machine::raiseInterrupt` 投递）。
 *
 * `timer` 是 `std::event::Timer` 的句柄，`callback` 是事件自带的函数指针（`0` 表示用
 * 定时器槽里绑的那个），`thread` 是它归属的线程；`source` 原样透传给回调。
 * 事件**不在投递线程上执行**：VM 记住它，在下一条指令边界（安全点）交付，而且只由
 * `thread` 那个线程的 VM 交付。
 */
struct InterruptEvent
{
    std::int64_t timer = 0;     ///< 定时器句柄（`std::event::Timer` 的 `handle`）；0 = 无定时器。
    std::int64_t source = 0;    ///< 触发来源，交给回调。
    std::int64_t callback = 0;  ///< 事件自带的函数指针；0 = 用 `timer` 槽里绑的那个。
    std::size_t thread = 0u;    ///< 归属线程 id；只有这个线程的 VM 会交付它。
};

/**
 * @brief 中断句柄：函数指针 + 定时器 + **归属哪个线程**。
 *
 * 每个线程有**自己的** VM 求值循环与中断队列；`raiseInterrupt(句柄, source)` 只把事件
 * 投给句柄里记的那个线程的 VM —— 别的线程不受影响。`std::event::Timer` 在创建时就把
 * 创建者的线程 id 记进定时器槽（`Timer.thread()` 可读），所以"某一线程创建的中断默认
 * 在这个线程的 VM 里执行"。宿主胶水可以拿着这个句柄跨线程投递。
 *
 * 三个字段都能只用一部分：
 *   * `callback == 0`：投递时用 `timer` 槽里 `onInterrupt` 绑好的回调；
 *   * `callback != 0`：直接跑这个函数指针（`timer` 仍参与"释放过就丢弃"的判定）；
 *   * `timer == 0`：没有定时器，只靠 `callback` 交付（宿主自己在 C++ 侧抓到的函数指针）。
 */
struct InterruptHandle
{
    std::int64_t timer = 0;     ///< `std::event::Timer` 的句柄；0 = 不经过定时器。
    std::int64_t callback = 0;  ///< 中断事件函数指针；0 = 用定时器槽里绑的那个。
    std::size_t thread = 0u;    ///< 归属线程 id；0 = 入口线程。
};

/**
 * 调试回调请 VM 接下来怎么做。
 */
enum class DebugAction
{
    Continue,  ///< 继续跑到下一个断点（清掉单步状态）。
    StepInto,  ///< 单步：下一条指令执行前再回调一次。
    Abort,     ///< 中止本次运行；结果里给出中止位置与源码级调用栈。
};

/**
 * 调用栈里的一帧（调试用）。
 */
struct DebugFrame
{
    Memory::String function;      ///< 函数 / 标签名；空表示根。
    std::size_t threadId = 0;     ///< 发生调用的隔离线程编号；0 为主线程。
    std::size_t coroutineId = 0;  ///< VM 当前协程编号。
    std::size_t instruction = 0;  ///< VM 调用点/当前指令；没有源码表也有效。
    SourceLocation location;      ///< 该帧的位置：被调函数是"当前指令"，调用者记"调用点"。
};

/**
 * 调试回调收到的只读快照。
 */
struct DebugEvent
{
    std::size_t instruction = 0;        ///< **即将执行**的指令下标。
    std::size_t steps = 0;              ///< 本次 run 已经执行过的指令数。
    std::size_t coroutine = 0;          ///< 所属协程；0 是主上下文。
    Memory::String opcode;              ///< 指令操作码文本。
    SourceLocation location;            ///< 该指令的源码位置。
    Memory::Vector<DebugFrame> frames;  ///< 调用栈，从外到内；最后一帧是当前函数。
    bool breakpoint = false;            ///< 是断点停的（true）还是单步停的（false）。
};

using DebugHook = Memory::Function<DebugAction(const DebugEvent &)>;

/**
 * @brief 执行结果。
 */
struct RunResult
{
    bool ok = false;  ///< 是否正常停机。
    /**
     * @brief 是不是"可恢复终止"（分配失败等来自 VM 外部的停止原因）。
     *
     * 板端没有异常：分配失败在失败点只写标志（`Memory::restartSignal()`），VM 在
     * 下一条指令边界（安全点）看到它就停在这里并把本轮标成可恢复。宿主据此清标志、
     * 重置 arena（`Memory::BufferRuntimeArena::resetForRestart()` / `mcuMetadataResetForRestart()`）
     * 后重跑——这就是软重启触发器之一；`ok == false` 的普通运行错误不带这个标记。
     */
    bool recoverable = false;
    /**
     * @brief 可恢复终止的原因（`Memory::WatchdogReason` 的整数值）；不可恢复时为 0。
     */
    int recoverableReason = 0;
    int exitCode = 0;                          ///< 正常停机状态码（`exit(n)`；普通 halt 为 0）。
    Memory::String output;                     ///< `write` 的数值输出（多个值之间一个空格）。
    Memory::String text;                       ///< `print` / `putc` 的文本输出（按顺序拼接）。
    Memory::String error;                      ///< 运行时错误（越界、未定义变量等）。
    std::size_t registerDispatches = 0;        ///< 新寄存器核心实际执行的指令数。
    std::size_t registerEntries = 0;           ///< 新寄存器核心进入次数。
    std::size_t fusedGroups = 0;               ///< 执行的四指令融合组数；steps 仍按原始指令计数。
    std::size_t steps = 0;                     ///< 执行的指令条数。
    bool timingAvailable = false;              ///< 是否由宿主时钟提供了精确计时。
    std::uint64_t startedNanoseconds = 0;      ///< 开始时的宿主单调时钟值。
    std::uint64_t finishedNanoseconds = 0;     ///< 结束时的宿主单调时钟值。
    std::uint64_t elapsedNanoseconds = 0;      ///< 本次执行耗时；时钟倒退时保守记 0。
    std::size_t maxStackDepth = 0;             ///< 求值栈的最大深度。
    std::size_t heapPeakBytes = 0;             ///< 堆上同时在用字节数的峰值。
    std::size_t heapLiveBytes = 0;             ///< 停机时还有多少字节没 `free`（≠ 0 就是漏了）。
    std::size_t heapLiveBlocks = 0;            ///< 停机时还挂着几块（配合 `heapLiveBytes` 看）。
    std::size_t liveObjects = 0;               ///< 停机时还有几个运行期对象（`bint` / 容器 / `obj` / 资源句柄）没释放。
    Memory::Vector<CoroutineInfo> coroutines;  ///< 主上下文及各协程的最终状态摘要。
    Memory::Vector<Memory::String> trace;      ///< 打开 trace 时每条指令一行。
    SourceLocation failureLocation;            ///< 出错（或被调试器中止）处对应的源码位置。
    /**
     * 出错处的源码级调用栈，从外到内；最后一帧是出错函数。
     * STACKVM_WITH_PANIC_TRACE=1 时自动记录；无调试表则提供函数符号及指令位置。
     */
    Memory::Vector<DebugFrame> sourceTrace;
};

/**
 * @brief 可替换的 VM 字节堆。
 *
 * `Machine::run()` 每次执行前都会调用 `reset(base)`；实现必须清空上一轮状态，并把地址 0
 * 留给空指针。接口管理的是 MAGEL/四元式 `alloc`、字符串和按地址 load/store 使用的字节堆，
 * 不接管 C++ 调用帧、STL 容器或运行期对象表的宿主内存。
 * 默认堆与板端池会复用物理字节，但通过槽号和分配代次区分逻辑身份；旧引用不能访问
 * 新分配对象。释放之后不得再解引用，未清扫的旧身份重复释放仍幂等；清扫后重复释放失败。
 * 自定义堆应分别提供活跃区间、旧身份判定及显式清扫，不得把物理地址复用当作身份复用。
 */
class HeapAllocator
{
public:
    virtual ~HeapAllocator() = default;

    virtual void reset(std::size_t base) = 0;
    [[nodiscard]] virtual std::size_t base() const = 0;
    [[nodiscard]] virtual std::size_t allocate(std::size_t bytes) = 0;
    virtual bool release(std::size_t address, Memory::String *reason = nullptr) = 0;
    /**
     * @brief 清理已释放身份；自定义堆须覆盖此接口。
     * @return 清理身份数；未实现返回 size_t(-1)。
     */
    virtual std::size_t sweep() { return static_cast<std::size_t>(-1); }
    /**
     * @brief 查询未被清扫的释放身份；旧自定义堆沿用删除守卫的静默跳过行为，支持清扫的堆须覆盖。
     * @param address 精确块首。
     * @return 仍可幂等释放时为真。
     */
    [[nodiscard]] virtual bool retired(std::size_t address) const { return address != 0; }

    [[nodiscard]] virtual bool contains(std::size_t address) const = 0;
    [[nodiscard]] virtual bool capacityAt(std::size_t address,
                                          std::size_t &capacity) const = 0;
    [[nodiscard]] virtual std::size_t liveBytes() const = 0;
    [[nodiscard]] virtual std::size_t liveBlocks() const = 0;
    [[nodiscard]] virtual std::size_t peakBytes() const = 0;
    [[nodiscard]] virtual bool readBytes(std::size_t address, char *out,
                                         std::size_t count) const = 0;
    virtual bool writeBytes(std::size_t address, const char *data,
                            std::size_t count) = 0;

    [[nodiscard]] bool readInt(std::size_t address, std::int64_t &out) const;
    bool writeInt(std::size_t address, std::int64_t value);
    [[nodiscard]] bool readDouble(std::size_t address, double &out) const;
    bool writeDouble(std::size_t address, double value);
};

/**
 * C++ 胶水函数与虚拟机交换的单槽值。字符串、指针和宿主句柄使用整数地址/句柄。
 */
struct NativeValue
{
    enum class Kind
    {
        Integer,
        Floating,
        Text
    } kind = Kind::Integer;
    std::int64_t integer = 0;
    double floating = 0.0;
    Memory::String text;

    static NativeValue fromInt(std::int64_t value);
    static NativeValue fromFloat(double value);
    static NativeValue fromText(Memory::String value);
    [[nodiscard]] bool isFloat() const { return kind == Kind::Floating; }
    [[nodiscard]] bool isText() const { return kind == Kind::Text; }
    [[nodiscard]] std::int64_t asInt() const;
    [[nodiscard]] double asFloat() const;
};

/**
 * 一次 C++ 胶水调用可访问的 VM 上下文；对象仅在回调期间有效。
 */
class NativeCallContext
{
public:
    virtual ~NativeCallContext() = default;
    [[nodiscard]] virtual bool readBytes(std::int64_t address, char *out,
                                         std::size_t count) const = 0;
    virtual bool writeBytes(std::int64_t address, const char *data, std::size_t count) = 0;
    [[nodiscard]] virtual bool readString(std::int64_t address, Memory::String &out) const = 0;
    /**
     * 把 VM `std::lang::List<T>` 句柄读取为无所有权的胶水值序列。
     */
    [[nodiscard]] virtual bool readList(std::int64_t handle,
                                        Memory::Vector<NativeValue> &out) const = 0;
    /**
     * 新建一个空的 `std::lang::List<T>`，返回对象句柄（配合 listPush 往里填元素）。
     */
    virtual std::int64_t makeList()
    {
        fail("list creation unsupported");
        return 0;
    }
    /**
     * 往 `makeList` 造出的列表末尾追加一个标量/字符串元素（地址型元素按 int 处理）。
     */
    virtual bool listPush(std::int64_t listHandle, const NativeValue &value)
    {
        (void)listHandle;
        (void)value;
        fail("list push unsupported");
        return false;
    }
    /**
     * 往列表末尾追加一个**值结构体**元素：从 `structAddress` 拷贝 `slotCount` 个 8 字节槽。
     */
    virtual bool listPushStruct(std::int64_t listHandle, std::int64_t structAddress, std::size_t slotCount)
    {
        (void)listHandle;
        (void)structAddress;
        (void)slotCount;
        fail("list struct push unsupported");
        return false;
    }
    /**
     * 读取 `std::lang::List<结构体>`：每个元素按 `slotCount` 个 double 槽展平进 `out`。
     */
    virtual bool readListStruct(std::int64_t handle, std::size_t slotCount,
                                Memory::Vector<double> &out) const
    {
        (void)handle;
        (void)slotCount;
        (void)out;
        return false; /* 缺省不支持（const 方法不能 fail，由实现者自行报错） */
    }
    /**
     * 新建一个空的 `std::lang::Map<K,V>`，返回对象句柄（配合 mapSet 往里塞键值对）。
     */
    virtual std::int64_t makeMap()
    {
        fail("map creation unsupported");
        return 0;
    }
    /**
     * 新建一个空的 `std::lang::Set<T>`，返回对象句柄（配合 setInsert 往里塞元素）。
     */
    virtual std::int64_t makeSet()
    {
        fail("set creation unsupported");
        return 0;
    }
    /**
     * 往 `makeMap` 造出的映射里塞一对键值（键值都是标量/字符串）。
     */
    virtual bool mapSet(std::int64_t mapHandle, const NativeValue &key, const NativeValue &value)
    {
        (void)mapHandle;
        (void)key;
        (void)value;
        fail("map set unsupported");
        return false;
    }
    /**
     * 往 `makeSet` 造出的集合里塞一个元素（标量/字符串）。
     */
    virtual bool setInsert(std::int64_t setHandle, const NativeValue &value)
    {
        (void)setHandle;
        (void)value;
        fail("set insert unsupported");
        return false;
    }
    /**
     * 读 `std::lang::Map<K,V>`：键/值分别展平成两组 NativeValue（标量/字符串）。
     */
    virtual bool readMap(std::int64_t handle, Memory::Vector<NativeValue> &keys,
                         Memory::Vector<NativeValue> &values) const
    {
        (void)handle;
        (void)keys;
        (void)values;
        return false;
    }
    /**
     * 读 `std::lang::Set<T>`：元素展平成 NativeValue 序列（标量/字符串）。
     */
    virtual bool readSet(std::int64_t handle, Memory::Vector<NativeValue> &elements) const
    {
        (void)handle;
        (void)elements;
        return false;
    }
    /**
     * @brief 在当前 VM 或 AOT 运行时中同步调用 Magel 函数或捕获闭包。
     * @param functionPointer 同一运行时的函数地址或闭包句柄。
     * @param arguments 按声明顺序编码的参数槽，字符串等对象须使用其运行时地址。
     * @param result 成功时接收返回槽；void 函数的返回槽应忽略。
     * @return 函数正常返回时为 true；无效函数、参数数量错误或运行时失败时为 false。
     * @note 仅在宿主回调期间由执行线程同步使用；共享当前堆、对象表和全局状态。
     */
    virtual bool invoke(std::int64_t functionPointer,
                        const Memory::Vector<NativeValue> &arguments,
                        NativeValue &result) = 0;
    [[nodiscard]] virtual std::int64_t makeString(const Memory::String &text) = 0;
    /**
     * @brief 向**当前虚拟机**投递一次中断事件（交绑定在该定时器上的回调）。
     *
     * 这是胶水层触发 MAGEL 中断事件函数的入口，对应 `std::event::Timer` 的 `handle`。
     * **异步 + 安全点交付**：宿主只把事件挂进邮箱就返回，回调由 VM 在**下一条指令
     * 边界**用执行线程调用 —— 不会重入正在跑的宿主回调，也不用自己加锁保护 VM 状态。
     * 句柄无效 / 定时器已 `destroy` / 还没绑回调时事件被丢弃（空操作）。
     * @param timer `std::event::Timer` 的句柄。
     * @param source 原样交给回调的来源值。
     */
    virtual void raiseInterrupt(std::int64_t timer, std::int64_t source)
    {
        (void)timer;
        (void)source;
    }
    /**
     * @brief 取得可在本次宿主调用结束后使用的中断投递器。
     * @return 不支持时为空；仅可在所属运行时的执行线程及运行期内调用。
     */
    virtual Memory::Function<void(std::int64_t, std::int64_t)> interruptPoster() { return {}; }
    /**
     * @brief 安装执行边界钩子；VM 按指令、AOT 按基本块边界计数。
     * @param interval 调用间隔，零表示取消。
     * @param hook 同步执行的钩子，不得递归执行运行时。
     * @return 当前运行时支持钩子时返回 true。
     */
    virtual bool setInstructionHook(std::size_t interval, InstructionHook hook)
    {
        (void)interval;
        (void)hook;
        return false;
    }
    /**
     * @brief 当前正在执行 `run()` 的虚拟机；宿主回调之外（指令钩子 / Qt 事件循环）也能用。
     *
     * 胶水拿到它以后可以在**宿主回调返回之后**继续工作：投递中断
     * （`Machine::raiseInterrupt`）或挂每 N 条指令一次的钩子
     * （`Machine::setInstructionHook`）。只在 VM 执行线程上使用；指针在 `Machine`
     * 生命周期内有效，`run()` 结束后不要再碰。
     * @return 正在运行的虚拟机；没有（例如离线编解码）返回 nullptr。
     */
    virtual Machine *machine() { return nullptr; }
    /**
     * Temporary value storage, reclaimed at the end of run().
     */
    virtual std::int64_t makeTemporary(std::size_t)
    {
        fail("temporary storage unsupported");
        return 0;
    }
    virtual void fail(const Memory::String &message) = 0;
};

using NativeCallback =
    Memory::Function<NativeValue(NativeCallContext &, const Memory::Vector<NativeValue> &)>;

/**
 * 宿主原生函数表；可同时挂到源码即时执行和加载后的 `.svmb` 程序。
 */
class NativeRegistry
{
public:
    struct Entry
    {
        std::size_t arity = 0;
        NativeCallback callback;
    };

    bool add(const Memory::String &name, std::size_t arity, NativeCallback callback,
             Memory::String *error = nullptr);
    bool remove(const Memory::String &name);
    [[nodiscard]] const Entry *find(const Memory::String &name) const;
    /**
     * @brief 注册表修改版本，用于使预绑定入口缓存失效。
     * @return 单调版本号。
     */
    [[nodiscard]] std::size_t revision() const { return revision_; }

private:
    std::size_t revision_ = 0;  ///< 注册或移除成功后递增。
    Memory::Map<Memory::String, Entry> entries_;
    Memory::Map<Memory::String, Memory::String> compactNames_;  ///< 编号键到宿主原名。
};

/**
 * @brief 栈式虚拟机。
 */
/**
 * @brief 数据区：加载 `.data.bin` 与布局表（`simple_ast` 的 `exportDataLayout()` 输出）。
 *
 * 有了它，四元式里的 `(addr, 符号, _, t)` / `(load, 指针, _, 目标)` / `(store, 指针, 值, _)`
 * 就能真正读写数组 / 结构体所在的那片内存。
 */
struct DataArea
{
    Memory::Vector<std::uint8_t> binary;               ///< 数据区原始字节（不是文本）。
    Memory::Map<Memory::String, std::size_t> symbols;  ///< 符号 → 基址（字节偏移）。
    Memory::Map<Memory::String, std::size_t> strides;  ///< 符号 → 槽位步长（布局行里的 `stride=`）。
    Memory::Map<Memory::String, std::size_t> sizes;    ///< 符号 → 占多少字节（布局行里的 `size=`）。

    /**
     * @brief 加载二进制与布局表。
     * @param binaryPath `.data.bin` 路径。
     * @param layoutPath 布局文件：一行 `符号 offset=0 size=8 type=…`。
     * @param diagnostics 出错信息。
     * @return 成功返回 true。
     */
    bool load(const Memory::String &binaryPath, const Memory::String &layoutPath,
              Memory::Vector<Memory::String> *diagnostics = nullptr);

    /**
     * @brief 从**内存映像 + 布局文本**加载数据区（板端：映像与布局都在 flash 里）。
     *
     * 行格式与文件版一致：`符号 offset=0 size=8 type=… stride=8`；`layoutText` 可为空。
     * @param bytes 数据区字节。
     * @param size 字节数。
     * @param layoutText 布局文本。
     * @param diagnostics 诊断。
     * @return 成功返回 true。
     */
    bool loadFromBytes(const void *bytes, std::size_t size, const Memory::String &layoutText,
                       Memory::Vector<Memory::String> *diagnostics = nullptr);

    /**
     * 数据区大小。
     */
    [[nodiscard]] std::size_t size() const { return binary.size(); }

    /**
     * @brief 读一个以 `\0` 结尾的字符串。
     *
     * 函数指针就是"数据区里存着函数标签的那串字节的地址"，间接调用靠它拿标签。
     * @param address 地址（数据区内的偏移）。
     * @param limit 最多读多少字节（防越界）。
     * @return 字符串；地址越界返回空串。
     */
    [[nodiscard]] Memory::String readCString(std::size_t address, std::size_t limit = 256u) const;

    /**
     * @brief 读 8 字节小端。
     * @param address 地址（数据区内的偏移）。
     * @param out 结果。
     * @return 越界返回 false。
     */
    [[nodiscard]] bool readInt(std::size_t address, std::int64_t &out) const;

    /**
     * @brief 写 8 字节小端。
     * @param address 地址。
     * @param value 值。
     * @return 越界返回 false。
     */
    bool writeInt(std::size_t address, std::int64_t value);

    /**
     * @brief 读 8 字节小端，按 IEEE-754 双精度解释（`(load:f64 …)` 用它）。
     * @param address 地址。
     * @param out 结果。
     * @return 越界返回 false。
     */
    [[nodiscard]] bool readDouble(std::size_t address, double &out) const;

    /**
     * @brief 把双精度写成 8 字节小端（`(store:f64 …)` 用它）。
     * @param address 地址。
     * @param value 值。
     * @return 越界返回 false。
     */
    bool writeDouble(std::size_t address, double value);

    /**
     * @brief 按字节读一段（字符串是"以 `\0` 结尾的字节串"，跨 8 字节槽）。
     * @param address 地址（数据区内的偏移）。
     * @param out 输出缓冲（至少 count 字节）。
     * @param count 字节数。
     * @return 越界返回 false。
     */
    [[nodiscard]] bool readBytes(std::size_t address, char *out, std::size_t count) const;

    /**
     * @brief 按字节写一段。
     * @param address 地址。
     * @param in 输入缓冲（至少 count 字节）。
     * @param count 字节数。
     * @return 越界返回 false。
     */
    bool writeBytes(std::size_t address, const char *in, std::size_t count);
};

// ---------------------------------------------------------------------------
//  AOT 运行时桥（`magel-emit --target cpp-aot` 生成的代码调用它）
// ---------------------------------------------------------------------------

/**
 * @brief AOT 生成代码与 VM 运行时之间的单值：整数（含地址 / 句柄）、浮点、字符串。
 *
 * 与解释器内部的 `Slot` 一一对应，但不暴露 `Slot` 的私有实现。
 */
struct RuntimeValue
{
    bool isFloat = false;      ///< 浮点值。
    bool isText = false;       ///< 字符串值（`text` 有效）。
    std::int64_t integer = 0;  ///< 整数 / 地址 / 对象句柄。
    double real = 0.0;         ///< 浮点值。
    Memory::String text;       ///< 字符串内容（宿主胶水往返用）。

    static RuntimeValue ofInt(std::int64_t value);
    static RuntimeValue ofFloat(double value);
    static RuntimeValue ofText(Memory::String value);

    /**
     * 取出数值（整数或浮点都行，与解释器的 `Slot::number()` 一致）。
     */
    [[nodiscard]] double number() const { return isFloat ? real : static_cast<double>(integer); }
};

/**
 * @brief AOT 生成代码用的运行时桥：复用解释器同一套堆、对象表、`__vm_*` 内建与宿主注册表。
 *
 * AOT 只做计算与控制流；字符串、容器、大整数、BigDecimal、对象表、边界检查、指针失效
 * 检查、输入输出与宿主绑定全部落到这台桥后面的既有运行时上，不重写运行时。
 *
 * 生命周期：`begin()` 建环境（reset 堆、清空对象表 / 输出 / 错误），随后的若干次
 * `callNative` / `loadMemory` / `storeMemory` / `allocate` / `release` 共享同一套对象状态，
 * `end()` 收尾。
 */
class RuntimeBridge
{
public:
    RuntimeBridge();
    ~RuntimeBridge();
    RuntimeBridge(const RuntimeBridge &) = delete;
    RuntimeBridge &operator=(const RuntimeBridge &) = delete;

    void setData(DataArea *data) { data_ = data; }
    void setNativeRegistry(const NativeRegistry *registry) { registry_ = registry; }
    void setFileApi(FileApi *api) { fileApi_ = api; }
    void setThreadsEnabled(bool enabled) { threadsEnabled_ = enabled; }
    void setHeapAllocator(HeapAllocator *allocator);
    void setProgramArguments(Memory::Vector<Memory::String> arguments);
    /**
     * 宿主胶水回调函数指针（`NativeCallContext::invoke`）的 AOT 实现。
     */
    void setInvoker(Memory::Function<bool(std::int64_t, const Memory::Vector<RuntimeValue> &,
                                          RuntimeValue &)>
                        invoker);
    /**
     * @brief 生成代码的分派器：把函数指针（数据区里的标签地址）与实参交给 AOT 生成的
     *        `magel_aot_callptr`，`rt` 是**要跑的桥**。
     *
     * 线程创建与协程 / 中断交付都靠它：线程会新建一台桥再调回来（互不干扰的堆 /
     * 对象表 / 全局），协程与中断交付则直接用调用方的桥。
     */
    void setFunctionRunner(Memory::Function<bool(RuntimeBridge &, std::int64_t,
                                                 const Memory::Vector<RuntimeValue> &,
                                                 RuntimeValue &)>
                               runner);
    /**
     * 全局变量块大小（`sizeof(McGlobals)`）；`begin()` 按这个尺寸开一块零初始化内存。
     */
    void setGlobalsSize(std::size_t bytes);
    /**
     * 全局变量块地址（每个桥一份；线程各自独立，协程共享）。
     */
    [[nodiscard]] void *globalsBlock() const;

    bool begin(std::size_t heapBase = 0u);
    void end();

    /**
     * @brief 指令边界：交付到期软件定时器与队列里的中断事件（AOT 在每个基本块开头调用）。
     * @return 有运行期错误返回 false。
     */
    bool boundary();

    bool callNative(const Memory::String &name, const Memory::Vector<RuntimeValue> &args,
                    RuntimeValue &result);
    [[nodiscard]] bool hasNative(const Memory::String &name) const;
    /**
     * @brief 按静态标量类型读取虚拟地址，保留堆和数据区的有效性检查。
     * @param address 运行时虚拟地址。
     * @param out 读取结果，仅成功时更新。
     * @retval 成功返回 true，失败保留与通用内存访问一致的诊断并返回 false。
     */
    bool loadInteger(std::int64_t address, std::int64_t &out);
    /**
     * @brief 按静态标量类型读取虚拟地址，保留堆和数据区的有效性检查。
     * @param address 运行时虚拟地址。
     * @param out 读取结果，仅成功时更新。
     * @retval 成功返回 true，失败保留与通用内存访问一致的诊断并返回 false。
     */
    bool loadFloat(std::int64_t address, double &out);
    /**
     * @brief 按静态标量类型写入虚拟地址，保留堆和数据区的有效性检查。
     * @param address 运行时虚拟地址。
     * @param value 待写入的标量值。
     * @retval 成功返回 true，失败保留与通用内存访问一致的诊断并返回 false。
     */
    bool storeInteger(std::int64_t address, std::int64_t value);
    /**
     * @brief 按静态标量类型写入虚拟地址，保留堆和数据区的有效性检查。
     * @param address 运行时虚拟地址。
     * @param value 待写入的标量值。
     * @retval 成功返回 true，失败保留与通用内存访问一致的诊断并返回 false。
     */
    bool storeFloat(std::int64_t address, double value);
    bool loadMemory(std::int64_t address, bool isFloat, RuntimeValue &out);
    bool storeMemory(std::int64_t address, bool isFloat, const RuntimeValue &value);
    [[nodiscard]] bool dataAddress(const Memory::String &symbol, std::int64_t &address) const;
    [[nodiscard]] Memory::String readCString(std::int64_t address) const;
    /**
     * @brief 解析函数值及隐式环境。
     * @param pointer 函数值。
     * @param environment 环境输出。
     * @return 标签地址。
     */
    std::int64_t resolveFunction(std::int64_t pointer, std::int64_t &environment);
    bool checkIndex(std::int64_t index, std::int64_t limit, const Memory::String &label);
    bool checkPointer(std::int64_t index, std::int64_t base, std::int64_t stride,
                      const Memory::String &label);
    bool allocate(std::int64_t bytes, std::int64_t &address);
    bool release(std::int64_t address, bool soft);
    [[nodiscard]] bool alive(std::int64_t handle) const;

    [[nodiscard]] const Memory::String &output() const;
    [[nodiscard]] const Memory::String &error() const;
    [[nodiscard]] bool failed() const { return !error_.empty(); }
    /**
     * @brief 报告错误；panic 会请求整个线程组停止。
     * @param message 错误信息。
     */
    void fail(const Memory::String &message);
    void clearError() { error_.clear(); }
    /**
     * @brief AOT 进入函数时记录一帧。
     * @param function 函数标签。
     */
    void enterTrace(const Memory::String &function);
    /**
     * @brief 更新当前 AOT 帧的 MAGEL 位置。
     * @param file 原始文件。
     * @param line 原始行号。
     */
    void locateTrace(const Memory::String &file, std::int32_t line);
    /**
     * @brief 用生成代码的文件名字面量更新当前帧，避免每条四元式构造临时字符串。
     * @param file 以零结尾的 UTF-8 文件名；空指针按空文件名处理，变化时复制到帧中。
     * @param line 原始源码行号。
     * @note 保留线程同步及错误快照语义，不要求调用方长期保存文件名。
     */
    void locateTrace(const char *file, std::int32_t line);
    /**
     * @brief AOT 离开函数前保存首次错误快照并退栈。
     */
    void leaveTrace();
    /**
     * @brief 传播子线程的真实失败栈。
     * @param trace 子线程已保存的快照。
     */
    void adoptFailureTrace(const Memory::Vector<DebugFrame> &trace);
    /**
     * @brief 查询 AOT 首次错误的调用栈，从外到内。
     * @return 快照；宏关闭时为空。
     */
    [[nodiscard]] Memory::Vector<DebugFrame> callStack() const;
    /**
     * @brief 设置 AOT 文本输出下沉。
     * @param sink 字节回调，空指针只累积文本。
     * @param user 回调上下文。
     * @param keepText 下沉时是否仍保留完整文本。
     */
    void setOutputSink(OutputSink sink, void *user = nullptr, bool keepText = true);
    void clearOutput();
    void appendOutput(const Memory::String &text);
    void appendValue(const RuntimeValue &value);
    void appendChar(int code);
    bool readInput(bool getCharacter, bool isFloat, RuntimeValue &out);
    [[nodiscard]] std::int64_t heapPeak() const;

private:
    DataArea *data_ = nullptr;
    const NativeRegistry *registry_ = nullptr;
    FileApi *fileApi_ = nullptr;
    bool threadsEnabled_ = false;
    HeapAllocator *heapAllocator_ = nullptr;
    Memory::Function<bool(std::int64_t, const Memory::Vector<RuntimeValue> &, RuntimeValue &)>
        invoker_;
    Memory::Function<bool(RuntimeBridge &, std::int64_t,
                          const Memory::Vector<RuntimeValue> &, RuntimeValue &)>
        runner_;
    std::size_t globalsSize_ = 0u;
    mutable Memory::String error_;
    struct Impl;
    Memory::UniquePtr<Impl> impl_;

    /**
     * @brief 发布全程序 panic 并唤醒线程等待者。
     */
    void publishPanic();

    /**
     * 轮转找下一个可跑（Ready / Suspended）的协程；没有返回槽数。
     */
    std::size_t bridgeNextRunnable(std::size_t after);
    /**
     * 放行一个协程，跑到它 `yield` / 完成再回到调度方。
     */
    bool bridgeSchedule(std::size_t id);
};

/**
 * @brief AOT 函数作用域，离开前保留错误栈；关闭宏时不生成此对象。
 */
class RuntimeCallFrame
{
    RuntimeBridge &runtime_;

public:
    /**
     * @brief 进入函数。
     * @param runtime 所属桥。
     * @param function 函数标签。
     */
    RuntimeCallFrame(RuntimeBridge &runtime, const Memory::String &function) : runtime_(runtime) { runtime_.enterTrace(function); }
    /**
     * @brief 析构前保存错误快照并退栈。
     */
    ~RuntimeCallFrame()
    {
#if STACKVM_WITH_HOST_EXCEPTION_GUARD
        try
        {
            runtime_.leaveTrace();
        } catch (...) {}
#else
        runtime_.leaveTrace();
#endif
    }
    RuntimeCallFrame(const RuntimeCallFrame &) = delete;
    RuntimeCallFrame &operator=(const RuntimeCallFrame &) = delete;
};

class Machine
{
public:
    /**
     * @brief 构造。
     * @param trace 是否记录执行轨迹。
     */
    explicit Machine(bool trace = false) : trace_(trace) {}

    /**
     * @brief 挂上数据区；不挂时 `AddrOf` / `LoadAt` / `StoreAt` 会报"没有数据区"。
     * @param data 数据区（生命周期由调用方保证）。
     */
    void setData(DataArea *data) { data_ = data; }

    /**
     * 挂上宿主原生函数表；注册表生命周期须覆盖 run()。传 nullptr 可取消。
     */
    void setNativeRegistry(const NativeRegistry *registry) { nativeRegistry_ = registry; }

    /**
     * @brief 挂上自定义 VM 字节堆；分配器生命周期须覆盖 run()。传 nullptr 恢复默认实现。
     *
     * 同一对象可连续执行多个程序；每次 run() 都会先调用分配器的 reset(base)。
     */
    void setHeapAllocator(HeapAllocator *allocator) { heapAllocator_ = allocator; }

#if STACKVM_WITH_THREADS
    /**
     * 为每个隔离的宿主线程创建独立 VM 字节堆；子线程继续继承此工厂。
     */
    void setThreadHeapFactory(Memory::Function<Memory::UniquePtr<HeapAllocator>()> factory)
    {
        threadHeapFactory_ = std::move(factory);
    }
#endif

    /**
     * 设置每个协程的求值栈槽数上限；0 表示不限制。
     * 超限会让 run() 返回“求值栈超过上限”诊断，不会抛出宿主异常。
     */
    void setStackLimit(std::size_t slots) { stackLimit_ = slots; }
    /**
     * @brief 选择新寄存器核心；关闭后保留原有优化执行器用于对照。
     * @param enabled 是否启用。
     * @return 无。
     */
    void setRegisterExecution(bool enabled) { registerExecution_ = enabled; }
    /**
     * @brief 开关寄存器跳转折叠，便于差分和性能对照。
     * @param enabled 是否启用。
     * @return 无。
     */
    void setRegisterJumpFolding(bool enabled) { registerJumpFolding_ = enabled; }
    /**
     * @brief 开关整数专用操作码。
     * @param enabled 是否启用。
     * @return 无。
     */
    void setRegisterSpecialization(bool enabled) { registerSpecialization_ = enabled; }
    /**
     * @brief 开关基本块批量检查与计数。
     * @param enabled 是否启用。
     * @return 无。
     */
    void setRegisterBlockExecution(bool enabled) { registerBlockExecution_ = enabled; }
    /**
     * @brief 控制整数内存、下标、参数快速路径与已返回调用帧复用。
     * @param enabled 是否启用。
     * @return 无。
     */
    void setRegisterServices(bool enabled) { registerServices_ = enabled; }
    /**
     * @brief 选择标签地址分派；不支持的编译器使用 switch。
     * @param enabled 是否启用。
     * @return 无。
     */
    void setRegisterThreadedDispatch(bool enabled) { registerThreadedDispatch_ = enabled; }
    /**
     * @brief 开关执行准备与融合，关闭时用于逐指令对照验证。
     * @param enabled 是否启用。
     * @return 无。
     */
    void setExecutionOptimization(bool enabled) { optimizeExecution_ = enabled; }

    /**
     * 当前每协程求值栈槽数上限；0 表示不限制。
     */
    [[nodiscard]] std::size_t stackLimit() const { return stackLimit_; }

    /**
     * 挂上文件系统接口；传 nullptr 使用桌面端默认实现。
     */
    void setFileApi(FileApi *api) { fileApi_ = api; }

    /**
     * 设置本次程序入口可见的命令行参数。内容会在 run() 期间复制进 VM 的 List。
     */
    void setProgramArguments(Memory::Vector<Memory::String> arguments)
    {
        programArguments_ = std::move(arguments);
    }

    /**
     * @brief 挂上文本输出下沉：`print` / `println` / `putChar` 的文本**实时**推给宿主。
     *
     * 板端接串口 / OLED / RTT 只需在这里写一次；`keepText=false` 时不再往
     * `RunResult::text` 累积（省 RAM，也不用等 `run()` 结束才看到输出）。
     * 不挂下沉（`sink == nullptr`）＝桌面行为：文本只进 `RunResult::text`。
     * @param sink 下沉；空 = 只累积。
     * @param user 上下文。
     * @param keepText 是否同时累积进 `RunResult::text`。
     */
    void setOutputSink(OutputSink sink, void *user = nullptr, bool keepText = true)
    {
        outputSink_ = sink;
        outputUser_ = user;
        outputKeepText_ = keepText;
    }

#if STACKVM_WITH_THREADS
    /**
     * 线程默认关闭；开启后每个任务运行在独立的 VM 内，通过消息复制文本。
     */
    void setThreadsEnabled(bool enabled) { threadsEnabled_ = enabled; }
#endif

    /**
     * 注入返回单调纳秒值的宿主时钟；空回调禁用精确计时。
     */
    void setTimeCallback(TimeCallback callback) { timeCallback_ = std::move(callback); }

    /**
     * 每完成 interval 条指令调用一次钩子。interval=0 或空钩子表示禁用。
     * 回调在 VM 执行线程同步发生，不应在回调里递归调用同一个 Machine::run()。
     */
    void setInstructionHook(std::size_t interval, InstructionHook hook)
    {
        hookInterval_ = interval;
        instructionHook_ = std::move(hook);
    }

    /**
     * @brief 挂上调试回调；空回调表示不调试。
     *
     * 回调在**每条被停下的指令执行之前**同步发生（断点命中或单步模式），返回的动作决定
     * 下一步：继续、单步、或中止。回调里可以读 `currentLocation()` / `callStack()` 看现场，
     * 但**不要**递归调用同一个 Machine::run()。
     */
    void setDebugHook(DebugHook hook) { debugHook_ = std::move(hook); }

    /**
     * @brief 从任意线程投递一次中断事件，交给绑定在该定时器上的回调。
     *
     * **异步 + 安全点交付**：调用方（宿主 / 胶水）只把事件挂进邮箱就返回，真正的
     * 回调由 VM 在**下一条指令边界**用执行线程调用 —— 所以不会重入正在跑的宿主回调，
     * 也不用自己加锁保护 VM 状态。可以在 `run()` 之前或期间、甚至别的线程上调用。
     *
     * 句柄无效 / 定时器已经 `destroy` / 还没绑定回调时，事件被丢弃（空操作）。
     *
     * @param timer `std::event::Timer` 的句柄。
     * @param source 交给回调的来源值（程序自己触发时是 `raise(source)` 的实参）。
     */
#if STACKVM_WITH_THREADS
    void raiseInterrupt(std::int64_t timer, std::int64_t source = 0);

    /**
     * @brief 按中断句柄投递：函数指针 + 定时器 + 归属线程。
     *
     * `handle.thread` 不是本 VM 的线程时**忽略**（调用方不用自己判断，投错线程是空操作）；
     * `handle.callback` 非 0 时直接跑它，所以宿主不用先让 MAGEL 侧 `onInterrupt` 绑回调；
     * `handle.timer` 仍然参与"句柄无效 / 定时器已经 destroy"的判定（那种情况照样丢弃）。
     *
     * @param handle 中断句柄（见 @ref InterruptHandle）。
     * @param source 交给回调的来源值。
     */
    void raiseInterrupt(const InterruptHandle &handle, std::int64_t source = 0);

    /**
     * 丢弃还没交付的中断事件。
     */
    void clearInterrupts();
#endif /* STACKVM_WITH_THREADS */

    /**
     * @brief 设置断点集合（指令下标）。
     * @param instructions 断点位置；重复项会被忽略，空表等于清空。
     */
    void setBreakpoints(Memory::Vector<std::size_t> instructions);

    /**
     * 清空所有断点。
     */
    void clearBreakpoints() { breakpoints_.clear(); }

    /**
     * 当前断点个数。
     */
    [[nodiscard]] std::size_t breakpointCount() const { return breakpoints_.size(); }

    /**
     * 开关单步：开启后每条指令执行前都触发一次调试回调。
     */
    void setSingleStep(bool enabled) { singleStep_ = enabled; }

    /**
     * 单步是否开启。
     */
    [[nodiscard]] bool singleStep() const { return singleStep_; }

    /**
     * @brief 临时指定调试表，优先于 `program.debug`；传 nullptr 恢复用程序自带的那张。
     *
     * 表必须活到本次 run() 结束。
     */
    void attachDebugInfo(const DebugInfo *info) { debugOverride_ = info; }

    /**
     * 本次运行生效的调试表；没挂也没带就是 nullptr。
     */
    [[nodiscard]] const DebugInfo *debugInfo() const { return debugInfo_; }

    /**
     * @brief 控制错误详情；关闭后不记录路径或调用栈，压缩容器始终关闭。
     * @param enabled 是否允许。
     */
    void setPanicDetailsEnabled(bool enabled) { panicDetailsEnabled_ = enabled; }

    /**
     * 最近一次调试回调 / 停机时刻的源码位置。
     */
    [[nodiscard]] SourceLocation currentLocation() const { return currentLocation_; }

    /**
     * 最近一次调试回调 / 停机时刻的源码级调用栈（从外到内）。
     */
    [[nodiscard]] Memory::Vector<DebugFrame> callStack() const { return debugFrames_; }

    /**
     * @brief 执行一个组装好的程序。
     * @param program 程序。
     * @param input `read` / `getc` 的输入项（按顺序取）。
     * @param stepLimit 指令条数上限，防止死循环；传 `0` 表示不设上限（缺省为 0）。
     * @return 执行结果。
     */
    [[nodiscard]] RunResult run(const Program &program,
                                const Memory::Vector<Memory::String> &input = {},
                                std::size_t stepLimit = 0u);

private:
    bool panicDetailsEnabled_ = true;                 ///< 错误详情策略，不能覆盖压缩容器的隐私限制。
    bool trace_ = false;                              ///< 是否记录轨迹。
    OutputSink outputSink_ = nullptr;                 ///< 文本输出下沉；空 = 只累积进 `RunResult::text`。
    void *outputUser_ = nullptr;                      ///< 输出下沉的上下文。
    bool outputKeepText_ = true;                      ///< 有下沉时是否仍累积进 `RunResult::text`。
    DataArea *data_ = nullptr;                        ///< 可选的数据区。
    const NativeRegistry *nativeRegistry_ = nullptr;  ///< 可选的 C++ 胶水函数表。
    HeapAllocator *heapAllocator_ = nullptr;          ///< 可选的自定义 VM 字节堆。
#if STACKVM_WITH_THREADS
    Memory::Function<Memory::UniquePtr<HeapAllocator>()> threadHeapFactory_;
#endif
    bool registerExecution_ = true;  ///< 默认使用新寄存器分派核心。
    bool registerServices_ = true;
    bool registerJumpFolding_ = true, registerSpecialization_ = true, registerBlockExecution_ = true;
    bool registerThreadedDispatch_ = true;             ///< 支持的编译器默认使用标签分派，其余回退 switch。
    bool optimizeExecution_ = true;                    ///< 不改变字节码的运行期优化开关。
    std::size_t stackLimit_ = 0u;                      ///< 每个协程的求值栈槽数上限；0 表示不限制。
    FileApi *fileApi_ = nullptr;                       ///< 可选的嵌入式文件系统接口。
    Memory::Vector<Memory::String> programArguments_;  ///< 传给语言入口的命令行参数。
#if STACKVM_WITH_THREADS
    bool threadsEnabled_ = false;
    std::shared_ptr<ThreadRuntime> threadRuntime_;
    std::shared_ptr<SharedRuntime> sharedRuntime_;
    std::shared_ptr<InterruptMailbox> interruptMailbox_;  ///< 宿主投递的中断（跨线程，懒建）。
#endif
    std::size_t threadId_ = 0u;
    std::size_t parentThreadId_ = 0u;
    Memory::String entryLabel_;
    TimeCallback timeCallback_;                 ///< 可选的宿主单调纳秒时钟。
    std::size_t hookInterval_ = 0;              ///< 指令钩子间隔，0 表示禁用。
    InstructionHook instructionHook_;           ///< 可选的同步指令钩子。
    DebugHook debugHook_;                       ///< 可选的调试回调。
    Memory::Set<std::size_t> breakpoints_;      ///< 断点：指令下标集合。
    bool singleStep_ = false;                   ///< 单步开关。
    const DebugInfo *debugOverride_ = nullptr;  ///< 调用方临时指定的调试表。
    const DebugInfo *debugInfo_ = nullptr;      ///< 本次运行生效的调试表。
    SourceLocation currentLocation_;            ///< 最近一次回调 / 停机时刻的位置。
    Memory::Vector<DebugFrame> debugFrames_;    ///< 最近一次回调 / 停机时刻的调用栈快照。
};

}  // namespace stackvm

#endif  // PARSE_ENGINE_STACKVM_HPP
