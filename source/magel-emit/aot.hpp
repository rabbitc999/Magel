#pragma once
/**
 * @file aot.hpp
 * @brief `magel-emit --target cpp-aot`：把组装好的 MAGEL 程序翻译成**基本块 + 局部变量 +
 *        `goto`** 的 C++ 源码（AOT），运行时（字符串 / 容器 / 大整数 / 对象表 /
 *        边界检查 / 宿主绑定）仍走内嵌的既有 VM 运行时（`stackvm::RuntimeBridge`）。
 */
#include "stackvm.hpp"

namespace magel_emit
{

/** 数据区里的一个符号（与 `.layout` 一行对应）。 */
struct AotDataSymbol
{
    Memory::String name; ///< 已经是 C++ 字符串字面量形式（含引号）。
    std::int64_t offset = 0;
    std::int64_t size = 0;
    std::int64_t stride = 0;
    bool hasSize = false;
    bool hasStride = false;
};

/** AOT 生成所需的一切输入。 */
struct AotRequest
{
    Memory::String declarationSources; ///< 模块编译单元共用的运行时声明。
    Memory::String embeddedSources; ///< 内嵌的 VM / 胶水 / 大数源码（已去本地 include）。
    Memory::String programName;     ///< 程序名。
    Memory::String dataBytes;       ///< `.data.bin` 原始字节。
    Memory::Vector<AotDataSymbol> symbols; ///< `.layout` 符号表。
    Memory::Vector<Memory::String> cExportEntries; ///< C 导出函数与上下文辅助函数入口。
    bool hasAbi = false;
    bool hasUser = false;
    bool threads = false;
    bool hasStackLimit = false;
    std::int64_t stackSlots = 0;
    bool debugInfo = true; ///< 生成 `#line`（打通 `.quad` 行号）。
    std::int64_t runtimeArena = 536870912;
    const stackvm::QuadFile *quads = nullptr;
    const stackvm::Program *program = nullptr;
};

/** @brief 模块化 AOT 输出：共享声明及各模块的独立编译单元。 */
struct AotModules
{
    Memory::String header; ///< 共享 ABI 声明，变化会使模块目标文件失效。
    Memory::Vector<std::pair<Memory::String, Memory::String>> units; ///< 模块名与 C++ 函数实现。
};

/**
 * @brief 生成完整的、自包含的 AOT C++ 源文件。
 * @param request 输入。
 * @param diagnostics 不支持的指令等诊断（非空 = 失败）。
 * @param modules 非空时输出共享声明和模块函数，返回值只保留运行时与入口。
 * @return 源码；失败时返回空串。
 */
[[nodiscard]] Memory::String generateAotFile(const AotRequest &request,
                                                Memory::Vector<Memory::String> *diagnostics,
                                                AotModules *modules = nullptr);

} // namespace magel_emit
