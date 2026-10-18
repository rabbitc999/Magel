/**
 * @file mcu_metadata_heap.cpp
 * @brief 元数据 arena：给 VM 的 C++ 结构提供**固定容量**的 `operator new/delete`。
 *
 * 板端不能依赖宿主动态堆（H743 上也没有 `malloc` 兜底），但 VM 的求值栈、调用帧、
 * 全局表、对象表、容器与协程上下文都是 C++ 对象，`new` 会落到这里来。本文件用
 * 一份静态 arena + `embed_pool` 的首次适配池把这段内存**在编译期定容**：
 *
 *   * 用尽即**致命**：打印一行说明（含需要多少、共有多少）然后 `std::abort()`。
 *     这是"没有异常"时唯一诚实的选择——错在没给够预算，不该假装还能继续；
 *   * 不做线程安全静态初始化（板端单线程，`-fno-threadsafe-statics` 下也正确）；
 *   * 与 VM 字节池是**两块独立**的 arena：字节池（`EmbedPoolAllocator`，由 VM 的
 *     `alloc`/`free` 驱动）管程序数据；这里管运行时自己的结构。两块大小分别可调。
 *
 * 预算（H743，1 MB 片内 RAM 起步口径）：
 *   MCU_VM_ARENA_BYTES      256 KB  VM 字节池（`mcu_host.cpp` / 板端宿主里定）
 *   MCU_METADATA_BYTES      192 KB  元数据 arena（本文件）
 *   EMBED_POOL_MAX_BLOCKS   64      两张块表各自的条目数（每条约 12 字节）
 */

#include "embed_pool.h"
#include "mcu_metadata_heap.hpp"
#include "allocator/magel_allocator.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <new>

#ifndef MCU_METADATA_BYTES
#define MCU_METADATA_BYTES (192u * 1024u)
#endif

/* 应急预留：与运行时 arena 同一套语义（见 allocator/magel_allocator.hpp 的说明）。 */
#ifndef MCU_METADATA_RESCUE_BYTES
#define MCU_METADATA_RESCUE_BYTES (16u * 1024u)
#endif

namespace
{
/* 池对象与 arena 都是 POD / 静态数组：没有动态初始化，就不会有静态初始化顺序问题。 */
alignas(EMBED_POOL_ALIGN_SIZE) unsigned char g_metadataArena[MCU_METADATA_BYTES] = {};
EmbedPool_T g_metadataPool = {};
EmbedPool_T g_metadataRescuePool = {};
std::uint32_t g_metadataRescueBytes = 0u;  ///< 应急预留的实际尺寸（0 = 还没建池）。
bool g_metadataReady = false;

/**
 * @brief 注入的外部 arena（为空时用编译期静态数组）。
 */
unsigned char *g_metadataExternal = nullptr;
std::uint32_t g_metadataExternalBytes = 0u;

/**
 * @brief 确保板端元数据内存池已初始化。
 */
void EnsureMetadataPool()
{
    if (g_metadataReady) return;
    void *memory = g_metadataExternal != nullptr ? static_cast<void *>(g_metadataExternal) : static_cast<void *>(g_metadataArena);
    const std::uint32_t bytes = g_metadataExternal != nullptr ? g_metadataExternalBytes : static_cast<std::uint32_t>(sizeof(g_metadataArena));
    /* 尾部切一块应急预留：主池耗尽时用它兜住"当前这一次 new"，好让 VM 走到下一个安全点。 */
    const std::uint32_t rescue = bytes > 2u * MCU_METADATA_RESCUE_BYTES ? static_cast<std::uint32_t>(MCU_METADATA_RESCUE_BYTES) : bytes / 4u;
    unsigned char *const base = static_cast<unsigned char *>(memory);
    EmbedPool_Create(&g_metadataPool, base, bytes - rescue);
    EmbedPool_Create(&g_metadataRescuePool, base + (bytes - rescue), rescue);
    g_metadataRescueBytes = rescue;
    g_metadataReady = true;
}

/**
 * @brief 元数据 arena 用尽：没有异常可抛，只能明确终止。
 */
[[noreturn]] void OutOfMetadataMemory(std::size_t bytes)
{
    /* 说清是"字节不够"还是"块表满了"——两种要调的东西不一样。 */
    const std::uint32_t live = EmbedPool_LiveBytes(&g_metadataPool);
    const std::uint32_t peak = EmbedPool_PeakBytes(&g_metadataPool);
    const std::uint32_t blocks = EmbedPool_BlockCount(&g_metadataPool);
    const auto capacity = mcuMetadataCapacityBytes();
    const char *why = (live + bytes > capacity) ? "arena 字节不够" : "块表条目不够（碎片化）";
    std::fprintf(stderr,
                 "mcu-vm：元数据 %s（要 %lu 字节；容量 %u，在用 %u，峰值 %u，块 %u/%u）\n",
                 why, static_cast<unsigned long>(bytes), static_cast<unsigned>(capacity),
                 static_cast<unsigned>(live), static_cast<unsigned>(peak),
                 static_cast<unsigned>(blocks), static_cast<unsigned>(EMBED_POOL_MAX_BLOCKS));
    std::abort();
}

/**
 * @brief 归还一块元数据内存：先试主池，再试应急预留池。
 * @param block 块地址；空指针是空操作。
 */
void FreeMetadataBlock(void *block) noexcept
{
    if (block == nullptr) return;
#if defined(MCU_METADATA_BOOTSTRAP_MALLOC) && MCU_METADATA_BOOTSTRAP_MALLOC
    /* 静态构造期 SDRAM 尚未初始化；这些小块始终归还启动阶段的 newlib 堆。 */
    if (!g_metadataReady || (!EmbedPool_Contains(&g_metadataPool, block) && !EmbedPool_Contains(&g_metadataRescuePool, block)))
    {
        std::free(block);
        return;
    }
#endif
    if (EmbedPool_Free(&g_metadataPool, block) == EMBED_POOL_ERR_OK) return;
    EmbedPool_Free(&g_metadataRescuePool, block);
}

/**
 * @brief 从板端元数据池分配指定大小的内存。
 * @param bytes 内存区域或数据的字节数。
 * @retval 对应运行时内存区域的地址。
 */
void *MetadataAllocate(std::size_t bytes) noexcept
{
    if (bytes == 0u) bytes = 1u; /* `new` 必须返回互不相同的地址 */
    if (bytes > 0xffffffffu) return nullptr;
#if defined(MCU_METADATA_BOOTSTRAP_MALLOC) && MCU_METADATA_BOOTSTRAP_MALLOC
    /* 仅板级显式启用：启动构造阶段不触碰外部 RAM，不阻止随后注入 SDRAM。 */
    if (g_metadataExternal == nullptr) return std::malloc(bytes);
#endif
    EnsureMetadataPool();
    void *block = EmbedPool_Alloc(&g_metadataPool, static_cast<std::uint32_t>(bytes));
    if (block != nullptr) return block;
    /* 失败点：只记录标志 + 原因（记录本身不吃池），再拿应急预留兜住这一次 `new`。 */
    Memory::restartSignal().recordFailure(Memory::WatchdogReason::PoolExhausted, bytes,
                                          EmbedPool_LiveBytes(&g_metadataPool),
                                          EmbedPool_LiveBlocks(&g_metadataPool), mcuMetadataCapacityBytes());
    return EmbedPool_Alloc(&g_metadataRescuePool, static_cast<std::uint32_t>(bytes));
}
}  // namespace

/**
 * @brief 从板端元数据池分配对象内存。
 * @param bytes 内存区域或数据的字节数。
 * @retval 对应运行时内存区域的地址。
 */
void *operator new(std::size_t bytes)
{
    void *block = MetadataAllocate(bytes);
    if (block == nullptr) OutOfMetadataMemory(bytes);
    return block;
}

/**
 * @brief 从板端元数据池分配数组内存。
 * @param bytes 内存区域或数据的字节数。
 * @retval 对应运行时内存区域的地址。
 */
void *operator new[](std::size_t bytes)
{
    void *block = MetadataAllocate(bytes);
    if (block == nullptr) OutOfMetadataMemory(bytes);
    return block;
}

/**
 * @brief 从板端元数据池分配对象内存。
 * @param bytes 内存区域或数据的字节数。
 * @retval 对应运行时内存区域的地址。
 */
void *operator new(std::size_t bytes, const std::nothrow_t &) noexcept
{
    return MetadataAllocate(bytes);
}

/**
 * @brief 从板端元数据池分配数组内存。
 * @param bytes 内存区域或数据的字节数。
 * @retval 对应运行时内存区域的地址。
 */
void *operator new[](std::size_t bytes, const std::nothrow_t &) noexcept
{
    return MetadataAllocate(bytes);
}

/**
 * @brief 释放板端元数据对象占用的内存。
 * @param block 语句块编号或待释放的内存块地址。
 * @retval 运算后的值或迭代器。
 */
void operator delete(void *block) noexcept
{
    FreeMetadataBlock(block);
}

/**
 * @brief 释放板端元数据数组占用的内存。
 * @param block 语句块编号或待释放的内存块地址。
 * @retval 运算后的值或迭代器。
 */
void operator delete[](void *block) noexcept
{
    FreeMetadataBlock(block);
}

/**
 * @brief 释放板端元数据对象占用的内存。
 * @param block 语句块编号或待释放的内存块地址。
 * @retval 运算后的值或迭代器。
 */
void operator delete(void *block, std::size_t) noexcept
{
    FreeMetadataBlock(block);
}

/**
 * @brief 释放板端元数据数组占用的内存。
 * @param block 语句块编号或待释放的内存块地址。
 * @retval 运算后的值或迭代器。
 */
void operator delete[](void *block, std::size_t) noexcept
{
    FreeMetadataBlock(block);
}

/**
 * @brief 释放板端元数据对象占用的内存。
 * @param block 语句块编号或待释放的内存块地址。
 * @retval 运算后的值或迭代器。
 */
void operator delete(void *block, const std::nothrow_t &) noexcept
{
    FreeMetadataBlock(block);
}

/**
 * @brief 释放板端元数据数组占用的内存。
 * @param block 语句块编号或待释放的内存块地址。
 * @retval 运算后的值或迭代器。
 */
void operator delete[](void *block, const std::nothrow_t &) noexcept
{
    FreeMetadataBlock(block);
}

/**
 * @brief 元数据 arena 的总容量（字节）。
 * @retval 以字节为单位的内存容量或占用量。
 */
std::uint32_t mcuMetadataCapacityBytes()
{
    return g_metadataExternal != nullptr ? g_metadataExternalBytes : static_cast<std::uint32_t>(sizeof(g_metadataArena));
}

/**
 * @brief 元数据 arena 是否已经用上（上过就不能换 arena）。
 * @retval 满足检查条件或操作完成时为 true，检查失败或无法完成时为 false。
 */
bool mcuMetadataInUse() { return g_metadataReady; }

/**
 * @brief 指定元数据 arena 的内存（外部 SDRAM / PSRAM / 板级 arena）。
 *
 * 必须在第一次 `new` 之前调用；已经在用返回 false。见 `mcu_arenas.hpp`。
 * @param memory 起始地址（按 `std::max_align_t` 对齐）。
 * @param bytes 字节数。
 * @return 成功返回 true。
 */
bool mcuMetadataSetArena(void *memory, std::uint32_t bytes)
{
    if (g_metadataReady || memory == nullptr || bytes == 0u) return false;
    g_metadataExternal = static_cast<unsigned char *>(memory);
    g_metadataExternalBytes = bytes;
    return true;
}

/**
 * @brief 软重启用：整块元数据 arena（含应急预留）重来。
 *
 * 只允许在"可恢复终止"之后调用——上一轮 `new` 出来的对象已经不可信。调用方必须保证
 * 不再持有上一轮 `new` 出来的任何对象（板端宿主的跨轮状态应当是静态 / POD）。
 */
void mcuMetadataResetForRestart()
{
    if (!g_metadataReady) return;
    EmbedPool_Reset(&g_metadataPool);
    EmbedPool_Reset(&g_metadataRescuePool);
}

/**
 * @brief 元数据应急预留还剩多少字节（footprint / 排查用）。
 * @retval 以字节为单位的内存容量或占用量。
 */
std::uint32_t mcuMetadataRescueRemainingBytes()
{
    if (!g_metadataReady) return 0u;
    return g_metadataRescueBytes - EmbedPool_LiveBytes(&g_metadataRescuePool);
}

/**
 * @brief 查询指定元数据分配块可用的容量。
 * @retval 以字节为单位的内存容量或占用量。
 */
std::uint32_t mcuMetadataBlockCapacity()
{
    return static_cast<std::uint32_t>(EMBED_POOL_MAX_BLOCKS);
}

/**
 * @brief 元数据 arena 当前还在用的字节数。
 * @retval 以字节为单位的内存容量或占用量。
 */
std::uint32_t mcuMetadataLiveBytes()
{
    return g_metadataReady ? EmbedPool_LiveBytes(&g_metadataPool) : 0u;
}

/**
 * @brief 元数据 arena 的历史峰值（回调后不复位）——**这个数决定板端要留多少 RAM**。
 * @retval 以字节为单位的内存容量或占用量。
 */
std::uint32_t mcuMetadataPeakBytes()
{
    return g_metadataReady ? EmbedPool_PeakBytes(&g_metadataPool) : 0u;
}

/**
 * @brief 元数据 arena 的块表当前条目数（看碎片化）。
 * @retval 元数据 arena 的块表当前条目数（看碎片化）。
 */
std::uint32_t mcuMetadataBlockCount()
{
    return g_metadataReady ? EmbedPool_BlockCount(&g_metadataPool) : 0u;
}
