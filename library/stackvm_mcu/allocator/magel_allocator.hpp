#ifndef MAGEL_CUSTOM_ALLOCATOR
#define MAGEL_CUSTOM_ALLOCATOR 1
#endif
#ifndef PARSEENGINE_MAGEL_ALLOCATOR_HPP
#define PARSEENGINE_MAGEL_ALLOCATOR_HPP
#if defined(MAGEL_USE_MALLOC) && MAGEL_USE_MALLOC
#error "MAGEL_USE_MALLOC has been removed; use the runtime memory pool"
#endif

/* 裁剪宏按是否定义生效；导出运行时和所有翻译单元必须使用相同配置。 */
#ifdef NO_MAGEL_BIG_INT
#undef STACKVM_WITH_BIGINT
#define STACKVM_WITH_BIGINT 0
#undef STACKVM_WITH_BIGFLOAT
#define STACKVM_WITH_BIGFLOAT 0
#endif
#ifdef NO_MAGEL_BIG_FLOAT
#undef STACKVM_WITH_BIGFLOAT
#define STACKVM_WITH_BIGFLOAT 0
#endif
#if defined(NO_MAGEL_STD) || defined(NO_MAGEL_STD_JSON)
#ifndef GLUE_NO_STD_JSON
#define GLUE_NO_STD_JSON 1
#endif
#endif
#if defined(NO_MAGEL_STD) || defined(NO_MAGEL_STD_TOML)
#ifndef GLUE_NO_STD_TOML
#define GLUE_NO_STD_TOML 1
#endif
#endif
#if defined(NO_MAGEL_STD) || defined(NO_MAGEL_STD_MATH)
#ifndef GLUE_NO_STD_MATH
#define GLUE_NO_STD_MATH 1
#endif
#endif
#if defined(NO_MAGEL_STD) || defined(NO_MAGEL_STD_RANDOM)
#ifndef GLUE_NO_STD_RANDOM
#define GLUE_NO_STD_RANDOM 1
#endif
#endif
#if defined(NO_MAGEL_STD) || defined(NO_MAGEL_STD_REGEX)
#ifndef GLUE_NO_STD_REGEX
#define GLUE_NO_STD_REGEX 1
#endif
#endif
#if defined(NO_MAGEL_STD) || defined(NO_MAGEL_STD_SIGNAL)
#ifndef GLUE_NO_STD_SIGNAL
#define GLUE_NO_STD_SIGNAL 1
#endif
#endif
#if defined(NO_MAGEL_STD) || defined(NO_MAGEL_STD_BINARY)
#ifndef GLUE_NO_STD_BINARY
#define GLUE_NO_STD_BINARY 1
#endif
#endif
#if defined(NO_MAGEL_STD) || defined(NO_MAGEL_STD_LINALG)
#ifndef GLUE_NO_STD_LINALG
#define GLUE_NO_STD_LINALG 1
#endif
#endif
#if defined(NO_MAGEL_STD) || defined(NO_MAGEL_STD_PARSE)
#ifndef GLUE_NO_STD_PARSE
#define GLUE_NO_STD_PARSE 1
#endif
#endif
#if defined(NO_MAGEL_STD) || defined(NO_MAGEL_STD_PROCESS)
#ifndef GLUE_NO_STD_PROCESS
#define GLUE_NO_STD_PROCESS 1
#endif
#endif
#if defined(NO_MAGEL_STD) || defined(NO_MAGEL_STD_IMAGE)
#ifndef GLUE_NO_STD_IMAGE
#define GLUE_NO_STD_IMAGE 1
#endif
#endif
#if defined(NO_MAGEL_STD) || defined(NO_MAGEL_STD_TREE)
#ifndef GLUE_NO_STD_TREE
#define GLUE_NO_STD_TREE 1
#endif
#endif
#if defined(NO_MAGEL_STD) || defined(NO_MAGEL_STD_GRAPH)
#ifndef GLUE_NO_STD_GRAPH
#define GLUE_NO_STD_GRAPH 1
#endif
#endif
#if defined(NO_MAGEL_STD) || defined(NO_MAGEL_STD_LINKEDLIST)
#ifndef GLUE_NO_STD_LINKEDLIST
#define GLUE_NO_STD_LINKEDLIST 1
#endif
#endif
#if defined(NO_MAGEL_STD) || defined(NO_MAGEL_STD_KMP)
#ifndef GLUE_NO_STD_KMP
#define GLUE_NO_STD_KMP 1
#endif
#endif

#include "magel_pool.h"
#include "magel_pool.c"
#include <cstddef>
#include <atomic>
#include <cstdio>
#include <cstdlib>
/* ---- 板端口径：没有异常可抛。分配/构造失败一律"记录原因 + 终止"，不 longjmp、不静默续跑。 ---- */
[[noreturn]] inline void McuAllocationFailed(const char *what)
{
    std::fprintf(stderr, "mcu-vm：%s\n", what);
    std::abort();
}

#include <deque>
#include <filesystem>
#include <functional>
#include <limits>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <new>
#include <set>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace Memory
{
/* 当前编译进来的池实现名，由 magel_pool.h 的 MAGEL_ALLOCATOR_TYPE 在编译期决定。 */
#if MAGEL_ALLOCATOR_TYPE == MAGEL_ALLOCATOR_LINKED
inline constexpr const char *allocatorTypeName = "linked";
#else
inline constexpr const char *allocatorTypeName = "linear";
#endif

/**
 * @def STACKVM_WITH_THREADS
 * @brief 是否编入 OS 线程 / 互斥（板端口径由 `magel/library/stackvm_mcu/Makefile` 传 0）。
 *
 * 板端单线程，arena 的那把互斥锁是纯粹的死重（还会牵进 pthread）：
 * `0` 时把 `ArenaMutex` / `ArenaLock` 换成空操作替身，**同一份代码、零成本裁剪**。
 * 没显式传宏时按桌面口径处理（保留 `std::mutex`）。
 */
#if !defined(STACKVM_WITH_THREADS) || STACKVM_WITH_THREADS
/**
 * @brief arena 的互斥锁类型（桌面口径：真锁）。
 */
using ArenaMutex = std::mutex;
/**
 * @brief arena 的作用域锁（桌面口径：真锁）。
 */
using ArenaLock = std::lock_guard<std::mutex>;
#else
/**
 * @brief 板端单线程：互斥的空操作替身。
 */
struct ArenaMutex
{
};
/**
 * @brief 板端单线程：作用域锁的空操作替身（接口与 `std::lock_guard` 一致）。
 */
struct ArenaLock
{
    explicit ArenaLock(ArenaMutex &) noexcept {}
};
#endif
class RuntimeAllocator
{
public:
    struct Stats
    { std::size_t liveBytes = 0, liveBlocks = 0, peakBytes = 0, totalBlocks = 0; };
    virtual ~RuntimeAllocator() = default;
    virtual void *allocate(std::size_t bytes) = 0;
    virtual void *allocateAligned(std::size_t bytes, std::size_t alignment)
    {
        return alignment <= alignof(std::max_align_t) ? allocate(bytes) : nullptr;
    }
    virtual bool release(void *pointer) = 0;
    virtual bool owns(const void *pointer) const = 0;
    virtual Stats stats() const = 0;
    /**
     * @brief 池的总容量（字节）；宿主堆模式没有容量概念时返回 0。诊断与软重启用。
     */
    virtual std::size_t capacity() const { return 0; }
    /**
     * @brief **应急预留**：主池耗尽时兜住"当前这一次申请"，好让 VM 走到下一条指令边界。
     *
     * 板端没有异常也没有 longjmp，失败点不能跳走；预留是"失败点照常返回、VM 在下一个
     * 安全点可恢复终止"能成立的前提。没预留或预留也用尽时返回 `nullptr`。
     * @param bytes 字节数。
     * @param alignment 对齐。
     * @return 拿得到返回指针（正常 release 归还）。
     */
    virtual void *rescue(std::size_t bytes, std::size_t alignment)
    {
        (void)bytes;
        (void)alignment;
        return nullptr;
    }
};
inline RuntimeAllocator *&currentRuntimeAllocator()
{
#if defined(STACKVM_WITH_THREADS) && !STACKVM_WITH_THREADS
    static RuntimeAllocator *allocator = nullptr;
#else
    static thread_local RuntimeAllocator *allocator = nullptr;
#endif
    return allocator;
}
inline RuntimeAllocator *defaultRuntimeAllocator();
inline RuntimeAllocator *runtimeAllocator() { return currentRuntimeAllocator() ? currentRuntimeAllocator() : defaultRuntimeAllocator(); }
inline void setRuntimeAllocator(RuntimeAllocator *allocator)
{
    // Allocator instances remember their owner; previous owners must outlive their allocations.
    currentRuntimeAllocator() = allocator;
}
inline constexpr bool runtimeMemoryStatsAvailable = true;
inline constexpr std::size_t defaultRuntimeCapacity = 512u * 1024u * 1024u;
inline std::atomic<RuntimeAllocator *> &defaultRuntimeAllocatorSlot()
{
    static std::atomic<RuntimeAllocator *> pointer{ nullptr };
    return pointer;
}
inline RuntimeAllocator *defaultRuntimeAllocatorIfCreated() { return defaultRuntimeAllocatorSlot().load(); }

/* ---------------------------------------------------------------------------
 *  可恢复的分配失败（板端口径）
 *
 *  板端没有异常、也没有 longjmp，所以在**失败点不能跳走**。这里的契约是：
 *    * 失败点**只记录**：写"类别 + 原因快照"，不吃池（记录本身是 POD）；
 *    * 当前这一次申请由 arena 的**应急预留**兜住（`RuntimeAllocator::rescue`），
 *      于是当前指令能正常跑完、容器保持自洽；
 *    * VM 在下一条**指令边界（安全点）**读到待处理标志，就以"可恢复终止"结束 `run()`
 *      并把结果标成 `recoverable`；宿主清标志、重置 arena 后重跑（软重启触发器之一）。
 *    * 连应急预留都用尽（申请比预留还大 / 预留也空了）才 `McuAllocationFailed` 终止——
 *      那是"预算根本没给够"，不该假装还能继续。
 *
 *  词汇与 PC 侧看门狗（`stash@{0}` 的 `AllocationFailureSink`）一致，只是板端不需要
 *  原子与"宿主安装"：单片一台机器，进程级一份、一直装着。
 * ------------------------------------------------------------------------- */

/**
 * @brief 一次软重启的**原因**（分配失败的两类 + 显式请求）。
 *
 * 这个词表与 PC 侧看门狗（`stash@{0}` 的 `WatchdogReason`）一致，所以"同一改动各打两遍"
 * 时两边对得上；`run()` 把它写进 `RunResult::recoverableReason`，宿主据此决定要不要重启、
 * 以及怎么报。
 */
enum class WatchdogReason
{
    None = 0,               ///< 没有触发。
    PoolExhausted = 1,      ///< Magel 运行内存池（arena / 元数据 arena / VM 字节池）用尽。
    HostHeapExhausted = 2,  ///< 保留历史重启原因编号，内存池不再使用宿主后备分配。
    MemoryWatermark = 3,    ///< 跑完一轮后发现内存水位超限（宿主量，VM 只带原因号）。
    ExplicitRequest = 4,    ///< 程序显式 `std::watchdog::requestRestart()`。
};

/**
 * @brief 一次分配失败的快照；只在失败点**记录**，不抛异常、不跳走。
 */
struct AllocationFailure
{
    WatchdogReason reason = WatchdogReason::None;  ///< 触发原因（分配失败 / 显式请求）。
    std::size_t requestedBytes = 0;                ///< 这次申请了多少字节（显式请求时为 0）。
    std::size_t liveBytes = 0;                     ///< 失败时池内还在用多少字节。
    std::size_t liveBlocks = 0;                    ///< 失败时池内还挂着几块。
    std::size_t capacity = 0;                      ///< 池总容量；宿主堆模式为 0。
    std::uint64_t sequence = 0;                    ///< 第几次触发（从 1 开始，跨重启不复位）。
};

/**
 * @brief 软重启的"信号位"：分配失败快照 + 显式重启请求 + 程序侧 `enable()` 的水位。
 *
 * 板端单线程，不需要原子；`pending` 置位后保持到 `clear()`，宿主每次重启尝试前清一次。
 * 本对象**不吃 Magel 池**（纯 POD 成员），所以池耗尽之后仍然可读 —— VM 就是靠这一点在
 * 安全点把"上一刻分配失败过 / 有人请求重启"变成可恢复终止。
 *
 * 两个触发共用一条通道是刻意的：VM 的检查只有一处（指令边界 `pending()`），无论原因是
 * 池耗尽还是 `requestRestart()`，都在同一个安全点以同一种方式停下。
 */
class RestartSignal
{
    bool enabled_ = false;             ///< 程序有没有 `enable()` 过（默认关闭）。
    std::size_t watermarkBytes_ = 0u;  ///< `enable(水位)` 给的水位；0 = 不按水位判。
    WatchdogReason reason_ = WatchdogReason::None;
    std::size_t requestedBytes_ = 0;
    std::size_t liveBytes_ = 0;
    std::size_t liveBlocks_ = 0;
    std::size_t capacity_ = 0;
    std::uint64_t sequence_ = 0;
    unsigned pending_ = 0;

    void note(WatchdogReason reason, std::size_t requestedBytes, std::size_t liveBytes,
              std::size_t liveBlocks, std::size_t capacity)
    {
        reason_ = reason;
        requestedBytes_ = requestedBytes;
        liveBytes_ = liveBytes;
        liveBlocks_ = liveBlocks;
        capacity_ = capacity;
        ++sequence_;
        pending_ = 1u;
    }

public:
    /**
     * @brief 程序侧 `std::watchdog::enable(watermarkBytes)`：打开看门狗并记下水位。
     *
     * 默认关闭：没 `enable()` 过时 `requestRestart()` 只报错、不产生重启；水位由宿主
     * 从这里读（宿主每轮量完自己判断，不重复在 VM 里计时）。
     * @param watermarkBytes 水位（字节）；0 = 只等显式请求 / 分配失败，不按水位判。
     */
    void enable(std::size_t watermarkBytes)
    {
        enabled_ = true;
        watermarkBytes_ = watermarkBytes;
    }
    /**
     * @brief 看门狗是否已经由程序打开。
     */
    [[nodiscard]] bool enabled() const { return enabled_; }
    /**
     * @brief 程序要的水位（字节）；0 = 不按水位判。
     */
    [[nodiscard]] std::size_t watermarkBytes() const { return watermarkBytes_; }

    /**
     * @brief 记一次分配失败；`pending` 置位后一直保持到 `clear()`。
     */
    void recordFailure(WatchdogReason reason, std::size_t requestedBytes, std::size_t liveBytes,
                       std::size_t liveBlocks, std::size_t capacity)
    {
        note(reason, requestedBytes, liveBytes, liveBlocks, capacity);
    }
    /**
     * @brief 记一次**显式重启请求**（`std::watchdog::requestRestart()`）。
     *
     * 请求不是分配失败：申请字节与池快照留 0。**只有 `enable()` 过才记**；没开启时返回
     * false（调用方据此报 `NotEnabled`），`pending` 不动、不会产生重启。
     * @return 记下了返回 true。
     */
    bool requestRestart()
    {
        if (!enabled_) return false;
        note(WatchdogReason::ExplicitRequest, 0u, 0u, 0u, 0u);
        return true;
    }
    /**
     * @brief 有没有待处理的触发。
     */
    [[nodiscard]] bool pending() const { return pending_ != 0u; }
    /**
     * @brief 待处理触发的原因（没有时 `WatchdogReason::None`）。
     */
    [[nodiscard]] WatchdogReason reason() const { return reason_; }
    /**
     * @brief 读取失败快照（分配失败时才有意义）。
     */
    [[nodiscard]] AllocationFailure peek() const
    {
        AllocationFailure failure;
        failure.reason = reason_;
        failure.requestedBytes = requestedBytes_;
        failure.liveBytes = liveBytes_;
        failure.liveBlocks = liveBlocks_;
        failure.capacity = capacity_;
        failure.sequence = sequence_;
        return failure;
    }
    /**
     * @brief 已累计的触发次数（跨重启不复位，用于"连续失败"判断）。
     */
    [[nodiscard]] std::uint64_t sequence() const { return sequence_; }
    /**
     * @brief 清掉待处理位（**不动** `sequence` / `enabled` / 水位）。每次重启尝试前调用。
     */
    void clear() { pending_ = 0u; }
};

/**
 * @brief 进程级软重启信号位：板端一直装着，不需要宿主手动安装。
 *
 * 单片一台机器（单线程）只有一份；PC 侧那份同源实现放在 `magel/source/allocator/magel_allocator.hpp`
 * 里，名字与语义逐字对应。
 */
inline RestartSignal &restartSignal()
{
    static RestartSignal signal;
    return signal;
}

/**
 * @brief 应急预留的默认尺寸：容量的 1/16，夹在 [1 KB, 64 KB]，且不超过容量的 1/4。
 */
inline std::size_t defaultRescueBytes(std::size_t capacity)
{
    std::size_t rescue = capacity / 16u;
    if (rescue < 1024u) rescue = capacity < 1024u ? capacity : 1024u;
    if (rescue > 64u * 1024u) rescue = 64u * 1024u;
    if (rescue > capacity / 4u) rescue = capacity / 4u;
    return rescue;
}
class BufferRuntimeArena : public RuntimeAllocator
{
    mutable ArenaMutex mutex_;
    void *buffer_;
    std::size_t bytes_;
    McPool pool_{};
    McPool rescuePool_{};          ///< 应急预留：主池耗尽后还能兜住几次申请。
    std::size_t rescueBytes_ = 0;  ///< 预留尺寸（从 buffer 尾部切出来）。
    Stats last_{};

    /**
     * @brief 按当前 `buffer_` / `bytes_` 重建主池与应急预留池（构造与重启共用）。
     */
    void InitPools()
    {
        rescueBytes_ = defaultRescueBytes(bytes_);
        const std::size_t mainBytes = bytes_ - rescueBytes_;
        if (!magel_pool_init(&pool_, buffer_, mainBytes))
            McuAllocationFailed("运行时内存池初始化失败：容量不足（板端无异常可抛）");
        if (!magel_pool_init(&rescuePool_, static_cast<unsigned char *>(buffer_) + mainBytes,
                             rescueBytes_))
            McuAllocationFailed("运行时应急预留初始化失败：容量不足（板端无异常可抛）");
    }

public:
    BufferRuntimeArena(void *buffer, std::size_t bytes) : buffer_(buffer), bytes_(bytes) { InitPools(); }
    BufferRuntimeArena(const BufferRuntimeArena &) = delete;
    BufferRuntimeArena &operator=(const BufferRuntimeArena &) = delete;
    std::size_t capacity() const override { return bytes_; }
    /**
     * @brief 应急预留还剩多少字节（footprint / 排查用）。
     */
    [[nodiscard]] std::size_t rescueRemainingBytes() const
    {
        return rescuePool_.capacity - rescuePool_.stats.live_bytes;
    }
    /**
     * @brief 应急预留的尺寸（footprint / 排查用）。
     */
    [[nodiscard]] std::size_t rescueBytes() const { return rescueBytes_; }
    void *allocate(std::size_t bytes) override { return allocateAligned(bytes, alignof(std::max_align_t)); }
    void *allocateAligned(std::size_t bytes, std::size_t alignment) override
    {
        ArenaLock lock(mutex_);
        return magel_pool_allocate(&pool_, bytes, alignment);
    }
    /**
     * @brief 主池耗尽时从应急预留里兜住这一次申请（板端"失败点不跳走"的前提）。
     * @param bytes 字节数。
     * @param alignment 对齐。
     * @return 拿得到返回指针；预留也用尽返回 nullptr。
     */
    void *rescue(std::size_t bytes, std::size_t alignment) override
    {
        ArenaLock lock(mutex_);

        if (rescuePool_.storage == nullptr) return nullptr;
        return magel_pool_allocate(&rescuePool_, bytes, alignment);
    }
    bool release(void *p) override
    {
        ArenaLock lock(mutex_);

        if (magel_pool_release(&pool_, p) != 0) return true;
        return magel_pool_release(&rescuePool_, p) != 0;
    }
    bool owns(const void *p) const override
    {
        return magel_pool_owns(&pool_, p) != 0 || magel_pool_owns(&rescuePool_, p) != 0;
    }
    Stats stats() const override
    {
        ArenaLock lock(mutex_);
        return { pool_.stats.live_bytes, pool_.stats.live_blocks, pool_.stats.peak_bytes, pool_.stats.total_blocks };
    }
    Stats lastRunStats() const { return last_; }
    std::size_t peakBytes() const { return last_.peakBytes; }
    /**
     * @brief 收尾清零：**要求没有活动块**，否则明确终止（错在调用方还拿着内存）。
     *
     * 正常一轮跑完（`run()` 自己把容器都释放了）才会走这里；池耗尽后的软重启不走它，
     * 走 `resetForRestart()`。
     */
    void finish()
    {
        ArenaLock lock(mutex_);
        if (!pool_.storage) return;
        if (pool_.stats.live_blocks)
        {
            std::fputs("运行时分配器：仍有活动块，不能清零\n", stderr);
            std::abort();
        }
        McPoolStats saved{};
        magel_pool_finish(&pool_, &saved);
        last_ = { saved.live_bytes, saved.live_blocks, saved.peak_bytes, saved.total_blocks };
        if (rescuePool_.storage)
        {
            McPoolStats ignored{};
            magel_pool_finish(&rescuePool_, &ignored);
        }
    }
    void begin()
    {
        ArenaLock lock(mutex_);
        if (pool_.stats.live_blocks || rescuePool_.stats.live_blocks)
            McuAllocationFailed("运行时内存池还有活动块，不能重新开始一轮（板端无异常可抛）");
        if (!pool_.storage) InitPools();
    }
    /**
     * @brief 软重启用：整池重来，**不**检查活动块。
     *
     * 只允许在"可恢复终止"之后调用——那一刻上一轮的 arena 内存已经不可信（程序状态被冻结、
     * 无法保证每个容器都被正确析构），继续复用只会更糟。调用方必须保证**不再持有**上一轮
     * 从本 arena 拿到的任何字符串 / 容器（它们立刻失效）。
     */
    void resetForRestart()
    {
        ArenaLock lock(mutex_);
        InitPools();
        last_ = Stats{};
    }
    /**
     * @brief 换一块缓冲区并重建两块池（板端："先在启动代码里初始化 SDRAM，再绑进来"）。
     *
     * 与构造同一套口径：`bytes` 是这块缓冲区的总容量，尾部按 `defaultRescueBytes` 切出
     * 应急预留。**只能在没人持有本 arena 的指针时调用**（换缓冲区会让旧指针失效）。
     * @param buffer 起始地址（按 `std::max_align_t` 对齐）。
     * @param bytes 总字节数。
     */
    void rebind(void *buffer, std::size_t bytes)
    {
        if (buffer == nullptr || bytes == 0u) return;
        ArenaLock lock(mutex_);
        buffer_ = buffer;
        bytes_ = bytes;
        InitPools();
        last_ = Stats{};
    }
};
class DynamicRuntimeArena final : public BufferRuntimeArena
{
    void *storage_;
    static void *reserve(std::size_t bytes)
    {
        if (bytes < 64) McuAllocationFailed("内存池分配失败：容量不足（板端无异常可抛）");
        void *p = std::malloc(bytes);
        if (!p) McuAllocationFailed("内存池分配失败：容量不足（板端无异常可抛）");
        return p;
    }
    DynamicRuntimeArena(void *storage, std::size_t bytes) : BufferRuntimeArena(storage, bytes), storage_(storage) {}

public:
    explicit DynamicRuntimeArena(std::size_t bytes = defaultRuntimeCapacity) : DynamicRuntimeArena(reserve(bytes), bytes) {}
    ~DynamicRuntimeArena() override
    {
        finish();
        std::free(storage_);
    }
};
inline RuntimeAllocator *defaultRuntimeAllocator()
{
#ifdef NO_MAGEL_DEFAULT_POOL
    McuAllocationFailed("内存池分配失败：容量不足（板端无异常可抛）");
#else
    // Deliberately survives static container destruction; no global new/delete interception.
    alignas(DynamicRuntimeArena) static unsigned char storage[sizeof(DynamicRuntimeArena)];
    static auto *arena = [] {
        auto *result = new (storage) DynamicRuntimeArena();
        defaultRuntimeAllocatorSlot().store(result);
        return result;
    }();
    return arena;
#endif
}
class RuntimeSession
{
    BufferRuntimeArena &arena_;
    RuntimeAllocator *previous_;

public:
    explicit RuntimeSession(BufferRuntimeArena &arena) : arena_(arena), previous_(currentRuntimeAllocator())
    {
        arena_.begin();
        setRuntimeAllocator(&arena_);
    }
    ~RuntimeSession()
    {
        setRuntimeAllocator(previous_);
        arena_.finish();
    }
    RuntimeSession(const RuntimeSession &) = delete;
    RuntimeSession &operator=(const RuntimeSession &) = delete;
};
class AllocationScope
{
    RuntimeAllocator *previous_;

public:
    explicit AllocationScope(RuntimeAllocator *owner) : previous_(currentRuntimeAllocator()) { setRuntimeAllocator(owner); }
    ~AllocationScope() { setRuntimeAllocator(previous_); }
    AllocationScope(const AllocationScope &) = delete;
    AllocationScope &operator=(const AllocationScope &) = delete;
};
template <std::size_t Capacity>
class StaticRuntimeArena final : public RuntimeAllocator
{
    static_assert(Capacity >= 64, "runtime arena capacity must be at least 64 bytes");
    alignas(std::max_align_t) unsigned char storage_[Capacity];
    McPool pool_{};
    McPool rescuePool_{};      ///< 应急预留（与 BufferRuntimeArena 同一套语义）。
    mutable ArenaMutex mutex_; /* 静态 arena 可能被并发的 VM 线程共用，和 BufferRuntimeArena 一样加锁 */
    void InitPools()
    {
        const std::size_t rescueBytes = defaultRescueBytes(Capacity);
        const std::size_t mainBytes = Capacity - rescueBytes;
        if (!magel_pool_init(&pool_, storage_, mainBytes))
            McuAllocationFailed("运行时内存池初始化失败：容量不足（板端无异常可抛）");
        if (!magel_pool_init(&rescuePool_, storage_ + mainBytes, rescueBytes))
            McuAllocationFailed("运行时应急预留初始化失败：容量不足（板端无异常可抛）");
    }

public:
    StaticRuntimeArena() { reset(); }
    void reset()
    {
        ArenaLock lock(mutex_);
        if (pool_.stats.live_blocks)
        {
            std::fputs("运行时分配器：仍有活动块，不能重置\n", stderr);
            std::abort();
        }
        InitPools();
    }
    std::size_t capacity() const override { return Capacity; }
    void *allocate(std::size_t bytes) override { return allocateAligned(bytes, alignof(std::max_align_t)); }
    void *allocateAligned(std::size_t bytes, std::size_t alignment) override
    {
        ArenaLock lock(mutex_);
        return magel_pool_allocate(&pool_, bytes, alignment);
    }
    void *rescue(std::size_t bytes, std::size_t alignment) override
    {
        ArenaLock lock(mutex_);

        if (rescuePool_.storage == nullptr) return nullptr;
        return magel_pool_allocate(&rescuePool_, bytes, alignment);
    }
    bool release(void *p) override
    {
        ArenaLock lock(mutex_);

        if (magel_pool_release(&pool_, p) != 0) return true;
        return magel_pool_release(&rescuePool_, p) != 0;
    }
    bool owns(const void *p) const override
    {
        ArenaLock lock(mutex_);
        return magel_pool_owns(&pool_, p) != 0 || magel_pool_owns(&rescuePool_, p) != 0;
    }
    Stats stats() const override
    {
        ArenaLock lock(mutex_);
        return { pool_.stats.live_bytes, pool_.stats.live_blocks, pool_.stats.peak_bytes, pool_.stats.total_blocks };
    }
};
}  // namespace Memory
namespace Memory
{
inline void *allocate(Memory::RuntimeAllocator *owner, std::size_t bytes, std::size_t alignment)
{
    if (!owner) owner = Memory::defaultRuntimeAllocator();
    void *p = owner->allocateAligned(bytes, alignment);
    if (!p)
    {
        /* 失败点：**只记录**标志 + 原因快照（本对象不吃池，池耗尽后仍然可读）。
         * 不在这里跳走 —— 板端没有异常，VM 会在下一条指令边界（安全点）消费它。 */
        const RuntimeAllocator::Stats stats = owner->stats();
        restartSignal().recordFailure(WatchdogReason::PoolExhausted,
                                      bytes, stats.liveBytes, stats.liveBlocks, owner->capacity());
        if (owner) std::fprintf(stderr, "运行时分配器：arena 用尽（要 %zu 字节）\n", bytes);
        /* 用应急预留兜住**当前这一次**申请：当前指令照常跑完、容器保持自洽，
         * VM 随即在下一个安全点可恢复终止。预留也用尽才真的没法继续。 */
        if (void *rescued = owner->rescue(bytes, alignment)) return rescued;
        McuAllocationFailed("内存池分配失败：连应急预留也不够（板端无异常可抛）");
    }
    return p;
}
inline void release(Memory::RuntimeAllocator *owner, void *p) noexcept
{
    if (!p) return;
    if (!owner) owner = Memory::defaultRuntimeAllocator();
    if (!owner->release(p))
    {
        std::fputs("运行时分配器：无效或重复释放 arena 内存\n", stderr);
        std::abort();
    }
}
template <class T>
class Allocator
{
public:
    using value_type = T;
    using propagate_on_container_move_assignment = std::true_type;
    using propagate_on_container_swap = std::true_type;
    using propagate_on_container_copy_assignment = std::false_type;
    Memory::RuntimeAllocator *owner = Memory::currentRuntimeAllocator();
    Allocator() noexcept = default;
    explicit Allocator(Memory::RuntimeAllocator *p) noexcept : owner(p) {}
    template <class U>
    Allocator(const Allocator<U> &other) noexcept : owner(other.owner)
    {
    }
    Allocator select_on_container_copy_construction() const noexcept { return Allocator(); }
    T *allocate(std::size_t count)
    {
        if (count > std::numeric_limits<std::size_t>::max() / sizeof(T)) McuAllocationFailed("分配尺寸溢出（板端无异常可抛）");
        return static_cast<T *>(Memory::allocate(owner, count * sizeof(T), alignof(T)));
    }
    void deallocate(T *p, std::size_t) noexcept { Memory::release(owner, p); }
    template <class U>
    bool operator==(const Allocator<U> &other) const noexcept
    { return owner == other.owner; }
    template <class U>
    bool operator!=(const Allocator<U> &other) const noexcept
    { return !(*this == other); }
};
template <class T>
struct Deleter
{
    Memory::RuntimeAllocator *owner = nullptr;
    void *allocation = nullptr;
    void (*destroy)(void *) = nullptr;
    Deleter() = default;
    Deleter(Memory::RuntimeAllocator *o, void *p, void (*d)(void *)) : owner(o), allocation(p), destroy(d) {}
    template <class U>
    Deleter(const Deleter<U> &d) noexcept : owner(d.owner), allocation(d.allocation), destroy(d.destroy)
    {
    }
    void operator()(T *p) const noexcept
    {
        if (!p) return;
        if (!destroy) std::abort();
        destroy(allocation);
        Memory::release(owner, allocation);
    }
};
#ifdef MAGEL_CUSTOM_ALLOCATOR
template <class T>
using ContainerAllocator = Allocator<T>;
template <class T>
using UniquePtr = std::unique_ptr<T, Deleter<T>>;
template <class T, class... Args>
UniquePtr<T> makeUnique(Args &&...args)
{
    auto *owner = Memory::runtimeAllocator();
    void *storage = Memory::allocate(owner, sizeof(T), alignof(T));
    /* 板端没有异常：构造失败就终止（上面的 allocate 已经保证拿得到存储）。 */
    T *object = new (storage) T(std::forward<Args>(args)...);
    return UniquePtr<T>(object, Deleter<T>(owner, storage, [](void *p) { static_cast<T *>(p)->~T(); }));
}
template <class T, class... Args>
std::shared_ptr<T> makeShared(Args &&...args)
{ return std::allocate_shared<T>(Allocator<T>(), std::forward<Args>(args)...); }
#else
template <class T>
using ContainerAllocator = std::allocator<T>;
template <class T>
using UniquePtr = std::unique_ptr<T>;
template <class T, class... Args>
UniquePtr<T> makeUnique(Args &&...args)
{ return std::make_unique<T>(std::forward<Args>(args)...); }
template <class T, class... Args>
std::shared_ptr<T> makeShared(Args &&...args)
{ return std::make_shared<T>(std::forward<Args>(args)...); }
#endif
#ifdef MAGEL_CUSTOM_ALLOCATOR
template <class Signature>
class Function;
template <class R, class... Args>
class Function<R(Args...)>
{
    struct Callable
    {
        virtual ~Callable() = default;
        virtual R call(Args... args) = 0;
        virtual std::shared_ptr<Callable> clone() const = 0;
    };
    template <class F>
    struct Holder final : Callable
    {
        F function;
        explicit Holder(F f) : function(std::move(f)) {}
        R call(Args... args) override { return std::invoke(function, std::forward<Args>(args)...); }
        std::shared_ptr<Callable> clone() const override { return makeShared<Holder<F>>(function); }
    };
    std::shared_ptr<Callable> callable_;

public:
    Function() = default;
    Function(std::nullptr_t) {}
    Function(const Function &f) : callable_(f.callable_ ? f.callable_->clone() : nullptr) {}
    Function(Function &&) noexcept = default;
    Function &operator=(const Function &f)
    {
        Function copy(f);
        callable_.swap(copy.callable_);
        return *this;
    }
    Function &operator=(Function &&) noexcept = default;
    Function &operator=(std::nullptr_t)
    {
        callable_.reset();
        return *this;
    }
    template <class F, class = std::enable_if_t<!std::is_same_v<std::decay_t<F>, Function> && std::is_invocable_r_v<R, F &, Args...>>>
    Function(F f) : callable_(makeShared<Holder<std::decay_t<F>>>(std::move(f)))
    {
    }
    explicit operator bool() const noexcept { return bool(callable_); }
    friend bool operator==(const Function &f, std::nullptr_t) noexcept { return !f.callable_; }
    friend bool operator!=(const Function &f, std::nullptr_t) noexcept { return bool(f.callable_); }
    friend bool operator==(std::nullptr_t, const Function &f) noexcept { return !f.callable_; }
    friend bool operator!=(std::nullptr_t, const Function &f) noexcept { return bool(f.callable_); }
    R operator()(Args... args) const
    {
        if (!callable_) McuAllocationFailed("调用了空的函数值（板端无异常可抛）");
        return callable_->call(std::forward<Args>(args)...);
    }
};
#else
template <class T>
using Function = std::function<T>;
#endif
template <class T>
using Vector = std::vector<T, ContainerAllocator<T>>;
template <class T>
using Deque = std::deque<T, ContainerAllocator<T>>;
template <class T>
using List = std::list<T, ContainerAllocator<T>>;
template <class K, class V, class C = std::less<K>>
using Map = std::map<K, V, C, ContainerAllocator<std::pair<const K, V>>>;
template <class K, class C = std::less<K>>
using Set = std::set<K, C, ContainerAllocator<K>>;
template <class K, class V, class H = std::hash<K>, class E = std::equal_to<K>>
using UnorderedMap = std::unordered_map<K, V, H, E, ContainerAllocator<std::pair<const K, V>>>;
template <class K, class H = std::hash<K>, class E = std::equal_to<K>>
using UnorderedSet = std::unordered_set<K, H, E, ContainerAllocator<K>>;
#ifdef MAGEL_CUSTOM_ALLOCATOR
class String : public std::basic_string<char, std::char_traits<char>, Allocator<char>>
{
    using Base = std::basic_string<char, std::char_traits<char>, Allocator<char>>;

public:
    using Base::Base;
    using Base::operator=;
    String() = default;
    String(const String &) = default;
    String(String &&) noexcept = default;
    String &operator=(const String &) = default;
    String &operator=(String &&) noexcept = default;
    String(const Base &s) : Base(s) {}
    String(Base &&s) : Base(std::move(s)) {}
    String(const std::string &s) : Base(s.data(), s.size()) {}
    String(std::string_view s) : Base(s.data(), s.size()) {}
    operator std::string() const { return std::string(data(), size()); }
    operator std::filesystem::path() const { return std::filesystem::u8path(data(), data() + size()); }
    String substr(std::size_t pos = 0, std::size_t count = Base::npos) const { return String(Base::substr(pos, count)); }
    friend String operator+(String a, const String &b)
    {
        a.append(b);
        return a;
    }
    friend String operator+(String a, const char *b)
    {
        a.append(b);
        return a;
    }
    friend String operator+(const char *a, const String &b)
    {
        String r(a);
        r.append(b);
        return r;
    }
    friend String operator+(String a, const std::string &b)
    {
        a.append(b.data(), b.size());
        return a;
    }
    friend String operator+(const std::string &a, const String &b)
    {
        String r(a);
        r.append(b);
        return r;
    }
    friend String operator+(String a, char b)
    {
        a.push_back(b);
        return a;
    }
    friend String operator+(char a, const String &b)
    {
        String r(1, a);
        r.append(b);
        return r;
    }
};
#else
using String = std::string;
#endif
template <class T>
String toString(T value)
{ return String(std::to_string(value)); }
inline std::filesystem::path u8path(const String &text) { return std::filesystem::u8path(text.data(), text.data() + text.size()); }
#ifdef MAGEL_CUSTOM_ALLOCATOR
template <class Stream>
class TextStream : public Stream
{
public:
    using Stream::Stream;
    using Stream::str;
    String str() const { return String(Stream::str()); }
};
using OutputStringStream = TextStream<std::basic_ostringstream<char, std::char_traits<char>, Allocator<char>>>;
using InputStringStream = TextStream<std::basic_istringstream<char, std::char_traits<char>, Allocator<char>>>;
using StringStream = TextStream<std::basic_stringstream<char, std::char_traits<char>, Allocator<char>>>;
#else
using OutputStringStream = std::ostringstream;
using InputStringStream = std::istringstream;
using StringStream = std::stringstream;
#endif
template <class T>
ContainerAllocator<T> persistentAllocator()
{
#ifdef MAGEL_CUSTOM_ALLOCATOR
    return ContainerAllocator<T>(Memory::defaultRuntimeAllocator());
#else
    return ContainerAllocator<T>();
#endif
}
}  // namespace Memory
#ifdef MAGEL_CUSTOM_ALLOCATOR
namespace std
{
template <>
struct hash<Memory::String>
{
    size_t operator()(const Memory::String &s) const noexcept { return hash<string_view>()(string_view(s.data(), s.size())); }
};
}  // namespace std
#endif
#endif
