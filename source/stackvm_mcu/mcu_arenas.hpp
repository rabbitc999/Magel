#ifndef PARSE_ENGINE_MCU_VM_ARENAS_HPP
#define PARSE_ENGINE_MCU_VM_ARENAS_HPP
/**
 * @file mcu_arenas.hpp
 * @brief 两块 arena 的运行期注入（外部 SDRAM / PSRAM）。
 *
 * 带 SDRAM 的板子（例如 H743 + 16 MB SDRAM）不能把 arena 写成静态数组：外部内存一般
 * 进不了 `.bss`，必须先在启动代码里初始化 FMC/SDRAM，再把"指针 + 长度"交给 VM。
 *
 * 典型顺序（板端 `main()`）：
 * ```cpp
 * SDRAM_Init();                                  // 你自己的板级代码
 * mcuArenas::SetMetadata(sdram + 0x000000, 8u * 1024u * 1024u);   // 元数据 8 MB
 * mcuArenas::SetBytePool(sdram + 0x800000, 4u * 1024u * 1024u);   // 字节池 4 MB
 * // 之后照常 loadProgramBinary(...) + run(...)
 * ```
 *
 * 不注入时用编译期静态 arena（`MCU_VM_ARENA_BYTES` / `MCU_METADATA_BYTES`，落在片内
 * `.bss`；`MCU_VM_ARENA_SECTION` 可以把它们指到链接脚本里映射到 SDRAM 的段）。
 * 注入必须在**第一次分配之前**：已经在用的 arena 不允许换。
 */

#include "embed_pool_allocator.hpp"

#include <cstddef>
#include <cstdint>

namespace mcuArenas
{
/* 宿主用 CurrentBytePool() / CurrentRuntimeArena() 拿到两块 arena 交给 VM。 */
}  // namespace mcuArenas

namespace mcuArenas
{
/**
 * @brief 指定 VM 字节池（程序 `alloc`/`free`、字符串、容器）。
 * @param memory 起始地址（按 8 字节对齐）。
 * @param bytes 字节数。
 * @return 成功返回 true。
 */
bool SetBytePool(void *memory, std::uint32_t bytes);

/**
 * @brief 指定元数据 arena（求值栈、调用帧、全局表、对象表、协程上下文、`std::string`…）。
 * @param memory 起始地址（按 `std::max_align_t` 对齐）。
 * @param bytes 字节数。
 * @return 成功返回 true。
 */
bool SetMetadata(void *memory, std::uint32_t bytes);

/**
 * @brief 当前生效的 VM 字节池（宿主 `machine.setHeapAllocator(&mcuArenas::CurrentBytePool())`）。
 */
embed_stackvm::EmbedPoolAllocatorView &CurrentBytePool();

/**
 * @brief 指定 **VM 运行时内存池**（`Memory::String` / `Memory::Vector` / 容器等 C++ 结构）。
 *
 * 这一块与"VM 字节池"（程序的 `alloc`/`free`）是**两块不同的内存**：字节池管程序数据，
 * 这里管 VM 自己的 C++ 对象。板端务必在**创建任何 `Memory::` 容器之前**（也就是碰 VM
 * 任何东西之前）调它，否则会落到缺省的宿主堆式 arena（板端没有宿主堆）。
 * @param memory 起始地址（按 `std::max_align_t` 对齐）。
 * @param bytes 字节数。
 * @return 成功返回 true。
 */
bool SetRuntimeArena(void *memory, std::uint32_t bytes);

/**
 * @brief 把运行时 arena 装成"当前分配器"（`Memory::setRuntimeAllocator`）。
 *
 * 必须在创建任何 `Memory::` 容器之前调用一次。不调 `SetRuntimeArena` 时用编译期静态
 * 数组（`MCU_RUNTIME_ARENA_BYTES`，可用 `MCU_VM_ARENA_SECTION` 指到链接脚本的 SDRAM 段）。
 */
void InstallRuntimeArena();

/**
 * @brief 当前生效的运行时 arena（软重启时用 `resetForRestart()` 整池重来）。
 */
Memory::BufferRuntimeArena &CurrentRuntimeArena();
/**
 * @brief 运行时 arena 的总容量（字节）；0 = 还没有缓冲区。
 */
std::uint32_t RuntimeArenaCapacityBytes();
}  // namespace mcuArenas

#endif
