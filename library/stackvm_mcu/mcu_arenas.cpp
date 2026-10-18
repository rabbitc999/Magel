/**
 * @file mcu_arenas.cpp
 * @brief arena 注入的门面：把 `mcuArenas::*` 接到具体实现上。
 */

#include "mcu_arenas.hpp"

#include "mcu_metadata_heap.hpp"

namespace mcuArenas
{
namespace
{
/* 字节池：编译期静态数组（片内 .bss）或运行期注入（外部 SDRAM）。 */
#ifndef MCU_VM_ARENA_BYTES
#define MCU_VM_ARENA_BYTES (256u * 1024u)
#endif
#ifdef MCU_VM_ARENA_SECTION
/* 链接脚本里映射到外部 SDRAM 的段名；不定义就走普通 .bss（片内 RAM）。 */
#define MCU_VM_ARENA_PLACE __attribute__((section(MCU_VM_ARENA_SECTION)))
#else
#define MCU_VM_ARENA_PLACE
#endif

alignas(8) unsigned char g_staticBytePool[MCU_VM_ARENA_BYTES] MCU_VM_ARENA_PLACE = {};

/* 运行时内存池：同样支持"编译期静态数组"与"运行期注入 SDRAM"两种给法。 */
#ifndef MCU_RUNTIME_ARENA_BYTES
#define MCU_RUNTIME_ARENA_BYTES (1024u * 1024u)
#endif

alignas(std::max_align_t) unsigned char g_staticRuntimeArena[MCU_RUNTIME_ARENA_BYTES] MCU_VM_ARENA_PLACE = {};
unsigned char *g_runtimeBuffer = nullptr;
std::uint32_t g_runtimeBytes = 0u;
Memory::BufferRuntimeArena *g_runtimeInstance = nullptr;

/**
 * @brief 取得板端运行时使用的静态分配 arena。
 * @retval 对应对象的引用。
 */
Memory::BufferRuntimeArena &RuntimeArena()
{
    /* 对象本体也不吃宿主堆：放在自己的静态存储里（板端单线程，静态初始化顺序无所谓）。 */
    alignas(std::max_align_t) static unsigned char storage[sizeof(Memory::BufferRuntimeArena)];
    if (g_runtimeInstance == nullptr)
    {
        void *buffer = g_runtimeBuffer != nullptr ? static_cast<void *>(g_runtimeBuffer) : static_cast<void *>(g_staticRuntimeArena);
        const std::uint32_t bytes = g_runtimeBytes != 0u ? g_runtimeBytes : static_cast<std::uint32_t>(sizeof(g_staticRuntimeArena));
        g_runtimeInstance = new (static_cast<void *>(storage)) Memory::BufferRuntimeArena(buffer, bytes);
    }
    return *g_runtimeInstance;
}

/**
 * @brief 取得板端虚拟机的字节内存池。
 * @retval 对应对象的引用。
 */
embed_stackvm::EmbedPoolAllocatorView &BytePool()
{
    static embed_stackvm::EmbedPoolAllocatorView pool;
    if (!pool.bound())
        pool.bind(static_cast<void *>(g_staticBytePool),
                  static_cast<std::uint32_t>(sizeof(g_staticBytePool)));
    return pool;
}
}  // namespace

/**
 * @brief 指定 VM 字节池（程序 `alloc`/`free`、字符串、容器）。
 * @param memory 起始地址（按 8 字节对齐）。
 * @param bytes 字节数。
 * @return 成功返回 true。
 */
bool SetBytePool(void *memory, std::uint32_t bytes)
{
    if (BytePool().peakBytes() != 0u || BytePool().liveBytes() != 0u)
        return false; /* 已经跑过：不允许中途换 arena */
    return BytePool().bind(memory, bytes);
}

/**
 * @brief 指定元数据 arena（求值栈、调用帧、全局表、对象表、协程上下文、`std::string`…）。
 * @param memory 起始地址（按 `std::max_align_t` 对齐）。
 * @param bytes 字节数。
 * @return 成功返回 true。
 */
bool SetMetadata(void *memory, std::uint32_t bytes)
{
    return mcuMetadataSetArena(memory, bytes);
}

/**
 * @brief 当前字节池（宿主拿它 `setHeapAllocator`）。
 */
embed_stackvm::EmbedPoolAllocatorView &CurrentBytePool() { return BytePool(); }

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
bool SetRuntimeArena(void *memory, std::uint32_t bytes)
{
    if (memory == nullptr || bytes == 0u) return false;
    /* 已经装成"当前分配器"就不能再换缓冲区：那会让上一轮拿到的指针立刻失效。 */
    if (g_runtimeInstance != nullptr && (Memory::currentRuntimeAllocator() == g_runtimeInstance || g_runtimeInstance->peakBytes() != 0u))
        return false;
    g_runtimeBuffer = static_cast<unsigned char *>(memory);
    g_runtimeBytes = bytes;
    RuntimeArena().rebind(memory, bytes);
    return true;
}

/**
 * @brief 把运行时 arena 装成"当前分配器"（`Memory::setRuntimeAllocator`）。
 *
 * 必须在创建任何 `Memory::` 容器之前调用一次。不调 `SetRuntimeArena` 时用编译期静态
 * 数组（`MCU_RUNTIME_ARENA_BYTES`，可用 `MCU_VM_ARENA_SECTION` 指到链接脚本的 SDRAM 段）。
 */
void InstallRuntimeArena() { Memory::setRuntimeAllocator(&RuntimeArena()); }

/**
 * @brief 当前生效的运行时 arena（软重启时用 `resetForRestart()` 整池重来）。
 * @retval 对应对象的引用。
 */
Memory::BufferRuntimeArena &CurrentRuntimeArena() { return RuntimeArena(); }

/**
 * @brief 运行时 arena 的总容量（字节）；0 = 还没有缓冲区。
 * @retval 以字节为单位的内存容量或占用量。
 */
std::uint32_t RuntimeArenaCapacityBytes()
{
    return static_cast<std::uint32_t>(CurrentRuntimeArena().capacity());
}
}  // namespace mcuArenas
