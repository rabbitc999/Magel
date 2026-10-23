/**
 * @file chali_aot.hpp
 * @brief `chali aot`：把四元式翻译成**独立的 C 源文件**并调 C 编译器编成原生可执行文件。
 *
 * 与 VM 那条路（四元式 → 栈指令 → 解释执行）不同，AOT 走的是：
 *
 *   四元式 → 基本块 + 局部变量 + `goto` 的 C 源码 → `cc -O2` → 原生可执行文件
 *
 * 产物**不链接 stackvm、不带字节码、没有分派循环**：它自带一份很小的 C 运行时
 * （内存、字符串、格式化输出、panic），所以能脱离编译器与虚拟机单独运行。
 * 这是"尽量减少对 VM 的依赖"的落实——VM 只在编译期参与（给出四元式与数据区），
 * 运行期完全不在场。
 *
 * 两条互调能力（沿用被删那一版 AOT 的对外契约）：
 *  - **AOT 调用外部 C**：`abi "库" { ... }` 声明的函数在产物里直接生成 `extern` 声明，
 *    编译期链接，不再经过 VM 的运行时 `dlopen`；
 *  - **外部 C 调用 AOT**：`--shared` 产出动态库，被 `--export` 点名的 ChaLi 函数
 *    导出为 C 可见符号，并同时写一个配套头文件。
 *
 * **默认 UNCHECKED**：`check` / `CheckPointer` 这类运行期检查默认不生成
 * （AOT 用于部署，要的是速度），`--checked` 才生成。
 */
#pragma once
#include "stackvm.hpp"

namespace chali_aot
{

/**
 * @brief 一个由 `abi "库" { ... }` 声明、要链接到外部 C 的函数。
 */
struct ExternFunction
{
    Memory::String label;                      ///< 四元式里的调用标签。
    Memory::String symbol;                     ///< C 侧真正的符号名（`= "名字"`）。
    Memory::String library;                    ///< 库路径（生成注释与链接参数用，可空）。
    Memory::String resultType = "long long";   ///< C 返回类型名。
    Memory::Vector<Memory::String> parameterTypes;  ///< C 形参类型名，按序。
};

/**
 * @brief AOT 输出的形态与开关。
 */
struct Options
{
    bool checked = false;         ///< 生成运行期检查（越界 / 空指针）。**默认 false = UNCHECKED**。
    bool shared = false;          ///< 产出动态库而不是可执行文件。
    bool emitSourceOnly = false;  ///< `--emit-c`：只写 C 源码，不调 C 编译器。
    bool keepSource = false;      ///< 编译完保留 `.c` / `.h`（默认随产物一起留在输出前缀旁）。
    Memory::String compiler;      ///< C 编译器（空 = 按平台挑 `cc` / `gcc` / `clang`）。
    Memory::String outputPrefix;  ///< 产物前缀（`.c` / 可执行 / `.so` 都挂在它上面）。
    Memory::String programName = "chali";  ///< 程序名（写进注释与导出符号前缀）。
    Memory::Vector<Memory::String> exports;  ///< 要导出给 C 的 ChaLi 函数名（默认 `main`）。
    Memory::Vector<Memory::String> linkLibraries;  ///< 额外的链接库（`abi` 里的库路径）。
    std::size_t memoryBytes = 512u * 1024u * 1024u;  ///< 运行时内存（数据区 + 堆）。
};

/**
 * @brief 一次 AOT 编译的全部输入。
 */
struct Request
{
    const stackvm::QuadFile *quads = nullptr;    ///< 已优化的四元式。
    const stackvm::Program *program = nullptr;   ///< 组装结果（取标签、形参与类型）。
    Memory::String dataBytes;                    ///< `.data.bin` 原始字节。
    Memory::String dataLayout;                   ///< 布局文本（每行 `名 offset=… size=…`）。
    Memory::Vector<ExternFunction> externs;      ///< `abi` 声明的外部 C 函数。
    Options options;
};

/**
 * @brief 取当前编译单元里 `abi "库" { ... }` 声明的外部 C 函数。
 *
 * 由前端（`source/semantics/artifacts.cpp`）注入——那里才有函数声明表与 C ABI 类型映射；
 * AOT 这边只消费结果，不反过来依赖前端，这样 `chali_aot` 始终是个干净的独立模块。
 * 没注入时返回空表（纯 ChaLi 程序本来就没有 abi 声明）。
 */
[[nodiscard]] Memory::Vector<ExternFunction> collectAbiExterns();

/**
 * @brief 前端注册"怎么取 abi 声明"的办法（进程启动时注入一次即可）。
 * @param provider 返回外部函数清单的函数指针；nullptr 表示清空。
 */
void setAbiExternProvider(Memory::Vector<ExternFunction> (*provider)());

/**
 * @brief 生成自包含的 C 源码。
 * @param request 输入。
 * @param diagnostics 不支持的构造等诊断（非空 = 失败）。
 * @param header 给外部 C 用的头文件；不需要可以传 nullptr。
 * @return 源码；失败时返回空串。
 */
[[nodiscard]] Memory::String generateSource(const Request &request, Memory::Vector<Memory::String> *diagnostics,
                                            Memory::String *header = nullptr);

/**
 * @brief 生成 C 源码、写盘、并调 C 编译器产出可执行文件 / 动态库。
 * @param request 输入。
 * @param diagnostics 诊断（含编译器的报错输出）。
 * @return 0 成功，非 0 失败。
 */
[[nodiscard]] int run(const Request &request, Memory::Vector<Memory::String> *diagnostics);

}  // namespace chali_aot
