#ifndef CHALI_CUSTOM_ALLOCATOR
#define CHALI_CUSTOM_ALLOCATOR 1
#endif
#ifndef PARSEENGINE_CHALI_ALLOCATOR_HPP
#define PARSEENGINE_CHALI_ALLOCATOR_HPP
#if defined(CHALI_USE_MALLOC) && CHALI_USE_MALLOC
#error "CHALI_USE_MALLOC has been removed; use the runtime memory pool"
#endif

/* 裁剪宏按是否定义生效；导出运行时和所有翻译单元必须使用相同配置。 */
#ifdef NO_CHALI_BIG_INT
#undef STACKVM_WITH_BIGINT
#define STACKVM_WITH_BIGINT 0
#undef STACKVM_WITH_BIGFLOAT
#define STACKVM_WITH_BIGFLOAT 0
#endif
#ifdef NO_CHALI_BIG_FLOAT
#undef STACKVM_WITH_BIGFLOAT
#define STACKVM_WITH_BIGFLOAT 0
#endif
#if defined(NO_CHALI_STD) || defined(NO_CHALI_STD_JSON)
#ifndef GLUE_NO_STD_JSON
#define GLUE_NO_STD_JSON 1
#endif
#endif
#if defined(NO_CHALI_STD) || defined(NO_CHALI_STD_TOML)
#ifndef GLUE_NO_STD_TOML
#define GLUE_NO_STD_TOML 1
#endif
#endif
#if defined(NO_CHALI_STD) || defined(NO_CHALI_STD_MATH)
#ifndef GLUE_NO_STD_MATH
#define GLUE_NO_STD_MATH 1
#endif
#endif
#if defined(NO_CHALI_STD) || defined(NO_CHALI_STD_RANDOM)
#ifndef GLUE_NO_STD_RANDOM
#define GLUE_NO_STD_RANDOM 1
#endif
#endif
#if defined(NO_CHALI_STD) || defined(NO_CHALI_STD_REGEX)
#ifndef GLUE_NO_STD_REGEX
#define GLUE_NO_STD_REGEX 1
#endif
#endif
#if defined(NO_CHALI_STD) || defined(NO_CHALI_STD_SIGNAL)
#ifndef GLUE_NO_STD_SIGNAL
#define GLUE_NO_STD_SIGNAL 1
#endif
#endif
#if defined(NO_CHALI_STD) || defined(NO_CHALI_STD_BINARY)
#ifndef GLUE_NO_STD_BINARY
#define GLUE_NO_STD_BINARY 1
#endif
#endif
#if defined(NO_CHALI_STD) || defined(NO_CHALI_STD_LINALG)
#ifndef GLUE_NO_STD_LINALG
#define GLUE_NO_STD_LINALG 1
#endif
#endif
#if defined(NO_CHALI_STD) || defined(NO_CHALI_STD_PARSE)
#ifndef GLUE_NO_STD_PARSE
#define GLUE_NO_STD_PARSE 1
#endif
#endif
#if defined(NO_CHALI_STD) || defined(NO_CHALI_STD_PROCESS)
#ifndef GLUE_NO_STD_PROCESS
#define GLUE_NO_STD_PROCESS 1
#endif
#endif
#if defined(NO_CHALI_STD) || defined(NO_CHALI_STD_IMAGE)
#ifndef GLUE_NO_STD_IMAGE
#define GLUE_NO_STD_IMAGE 1
#endif
#endif
#if defined(NO_CHALI_STD) || defined(NO_CHALI_STD_TREE)
#ifndef GLUE_NO_STD_TREE
#define GLUE_NO_STD_TREE 1
#endif
#endif
#if defined(NO_CHALI_STD) || defined(NO_CHALI_STD_GRAPH)
#ifndef GLUE_NO_STD_GRAPH
#define GLUE_NO_STD_GRAPH 1
#endif
#endif
#if defined(NO_CHALI_STD) || defined(NO_CHALI_STD_LINKEDLIST)
#ifndef GLUE_NO_STD_LINKEDLIST
#define GLUE_NO_STD_LINKEDLIST 1
#endif
#endif
#if defined(NO_CHALI_STD) || defined(NO_CHALI_STD_KMP)
#ifndef GLUE_NO_STD_KMP
#define GLUE_NO_STD_KMP 1
#endif
#endif

#include "chali_pool.h"
#include "chali_pool.c"
#include <cstddef>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
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
/* 当前编译进来的池实现名，由 chali_pool.h 的 CHALI_ALLOCATOR_TYPE 在编译期决定。 */
#if CHALI_ALLOCATOR_TYPE == CHALI_ALLOCATOR_LINKED
inline constexpr const char *allocatorTypeName = "linked";
#else
inline constexpr const char *allocatorTypeName = "linear";
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
};
inline RuntimeAllocator *&currentRuntimeAllocator()
{
    static thread_local RuntimeAllocator *allocator = nullptr;
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
 *  软重启信号（桌面口径）
 *
 *  分配失败记录原因并抛出 `std::bad_alloc`，宿主在执行边界决定是否恢复。
 *  显式重启请求由 VM 在安全点检查；信号不直接跳转或替换宿主控制流。
 *
 *  本对象不吃 ChaLi 池（纯 POD 成员），所以池耗尽之后仍然可读。
 * ------------------------------------------------------------------------- */

/**
 * @brief 一次软重启的触发原因。
 */
enum class WatchdogReason
{
    None = 0,               ///< 没有触发。
    PoolExhausted = 1,      ///< ChaLi 运行内存池（外部缓冲区 / 动态池）用尽。
    HostHeapExhausted = 2,  ///< 保留历史重启原因编号，内存池不再使用宿主后备分配。
    MemoryWatermark = 3,    ///< 跑完一轮后发现内存水位超限（宿主量，VM 只带原因号）。
    ExplicitRequest = 4,    ///< 程序显式 `std::watchdog::requestRestart()`。
};

/**
 * @brief 一次触发（分配失败 / 显式请求）的快照；只在触发点**记录**。
 */
struct AllocationFailure
{
    WatchdogReason reason = WatchdogReason::None;  ///< 触发原因。
    std::size_t requestedBytes = 0;                ///< 这次申请了多少字节（显式请求时为 0）。
    std::size_t liveBytes = 0;                     ///< 失败时池内还在用多少字节。
    std::size_t liveBlocks = 0;                    ///< 失败时池内还挂着几块。
    std::size_t capacity = 0;                      ///< 池总容量；宿主堆模式为 0。
    std::uint64_t sequence = 0;                    ///< 第几次触发（从 1 开始，跨重启不复位）。
};

/**
 * @brief 软重启的"信号位"：分配失败快照 + 显式重启请求 + 程序侧 `enable()` 的水位。
 *
 * 桌面可能多线程，所以字段用原子；`pending` 置位后保持到 `clear()`，宿主每次重启尝试前清一次。
 */
class RestartSignal
{
    std::atomic<unsigned> enabled_{ 0u };
    std::atomic<std::size_t> watermarkBytes_{ 0u };
    std::atomic<unsigned> reason_{ 0u };
    std::atomic<std::size_t> requestedBytes_{ 0 };
    std::atomic<std::size_t> liveBytes_{ 0 };
    std::atomic<std::size_t> liveBlocks_{ 0 };
    std::atomic<std::size_t> capacity_{ 0 };
    std::atomic<std::uint64_t> sequence_{ 0 };
    std::atomic<unsigned> pending_{ 0u };

    void note(WatchdogReason reason, std::size_t requestedBytes, std::size_t liveBytes,
              std::size_t liveBlocks, std::size_t capacity)
    {
        reason_.store(static_cast<unsigned>(reason), std::memory_order_relaxed);
        requestedBytes_.store(requestedBytes, std::memory_order_relaxed);
        liveBytes_.store(liveBytes, std::memory_order_relaxed);
        liveBlocks_.store(liveBlocks, std::memory_order_relaxed);
        capacity_.store(capacity, std::memory_order_relaxed);
        sequence_.fetch_add(1u, std::memory_order_relaxed);
        pending_.store(1u, std::memory_order_release); /* 最后一笔发布 */
    }

public:
    /**
     * @brief 程序侧 `std::watchdog::enable(watermarkBytes)`：打开看门狗并记下水位。
     */
    void enable(std::size_t watermarkBytes)
    {
        watermarkBytes_.store(watermarkBytes, std::memory_order_relaxed);
        enabled_.store(1u, std::memory_order_release);
    }
    /**
     * @brief 看门狗是否已经由程序打开。
     */
    [[nodiscard]] bool enabled() const { return enabled_.load(std::memory_order_acquire) != 0u; }
    /**
     * @brief 程序要的水位（字节）；0 = 不按水位判。
     */
    [[nodiscard]] std::size_t watermarkBytes() const
    {
        return watermarkBytes_.load(std::memory_order_relaxed);
    }

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
     * @return 记下了返回 true；没 `enable()` 过返回 false（调用方据此报 `NotEnabled`）。
     */
    bool requestRestart()
    {
        if (!enabled()) return false;
        note(WatchdogReason::ExplicitRequest, 0u, 0u, 0u, 0u);
        return true;
    }
    /**
     * @brief 有没有待处理的触发。
     */
    [[nodiscard]] bool pending() const { return pending_.load(std::memory_order_acquire) != 0u; }
    /**
     * @brief 待处理触发的原因。
     */
    [[nodiscard]] WatchdogReason reason() const
    {
        return static_cast<WatchdogReason>(reason_.load(std::memory_order_relaxed));
    }
    /**
     * @brief 读取触发快照（分配失败时才有意义）。
     */
    [[nodiscard]] AllocationFailure peek() const
    {
        AllocationFailure failure;
        failure.reason = reason();
        failure.requestedBytes = requestedBytes_.load(std::memory_order_relaxed);
        failure.liveBytes = liveBytes_.load(std::memory_order_relaxed);
        failure.liveBlocks = liveBlocks_.load(std::memory_order_relaxed);
        failure.capacity = capacity_.load(std::memory_order_relaxed);
        failure.sequence = sequence_.load(std::memory_order_relaxed);
        return failure;
    }
    /**
     * @brief 已累计的触发次数（跨重启不复位）。
     */
    [[nodiscard]] std::uint64_t sequence() const { return sequence_.load(std::memory_order_relaxed); }
    /**
     * @brief 清掉待处理位（**不动** `sequence` / `enabled` / 水位）。
     */
    void clear() { pending_.store(0u, std::memory_order_release); }
};

/**
 * @brief 进程级软重启信号位，由宿主在执行边界检查。
 */
inline RestartSignal &restartSignal()
{
    static RestartSignal signal;
    return signal;
}

/**
 * @brief 小对象分组分配：一次向池要一整块，块内按固定尺寸切片。
 *
 * 池（TLSF）每块带 16 字节块头并向上取整到 16 字节：一条 23 字节的名字串要占 48
 * 字节，一半多是头与舍入。指令流、符号表这类结构里这种小对象是十万到百万级的，
 * 逐条分配除了浪费字节，还会把空闲表切成海量小块，命中率与碎片都变差。
 *
 * 所以按组分配：一次向池要 `blockBytes`（64 KiB），组内切成固定尺寸的片，每片的
 * 头退化成 8 字节标签。TLSF 的块大小永远是 16 的倍数（低 2 位用作空闲标志），
 * 所以「载荷前一个字的最低位为 1」可以安全地标记成本层的片，正常块不会撞上。
 * 分配到的指针仍然落在池的地址范围内，`release` 也仍然只收一个指针，上层
 * （容器、字符串）不需要任何改动。
 */
class SlabPool
{
public:
    static constexpr std::size_t blockBytes = 64u * 1024u;          ///< 一次向池要的字节数。
    static constexpr std::size_t tagBytes = sizeof(std::size_t);    ///< 每片的前缀：指向所属组的标签。
    static constexpr std::size_t maxPayload = 120u;                 ///< 超过这个字节数就别进小对象组。
    /// 片尺寸（含 8 字节标签）；覆盖 8/16/24/40/56/72/88/120 字节的载荷。
    static constexpr std::size_t sizes[] = { 16u, 24u, 32u, 48u, 64u, 80u, 96u, 128u };
    static constexpr std::size_t noChunk = static_cast<std::size_t>(-1);
    struct Group
    {
        unsigned char *base = nullptr;   ///< 组内切片起点（池块首地址）。
        std::size_t chunk = 0;           ///< 片尺寸（含标签）。
        std::size_t count = 0;           ///< 片数。
        std::size_t freeHead = noChunk;  ///< 空闲片下标链表头。
        std::size_t live = 0;            ///< 已交出的片数。
        Group *next = nullptr;           ///< 下一组。
    };
    SlabPool() = default;
    SlabPool(const SlabPool &) = delete;
    SlabPool &operator=(const SlabPool &) = delete;
    ~SlabPool() { abandon(); }
    /**
     * @brief 试探一次分配：不归本层管时返回 nullptr，交回池处理。
     * @param pool 底层池。
     * @param bytes 载荷字节数。
     * @param alignment 对齐要求（超过指针宽度就交回池）。
     * @return 载荷指针或 nullptr。
     */
    void *allocate(McPool *pool, std::size_t bytes, std::size_t alignment)
    {
        if (!pool || bytes == 0u || bytes > maxPayload || alignment > tagBytes) return nullptr;
        const std::size_t chunk = chunkFor(bytes);
        if (chunk == 0u) return nullptr;
        Group *group = groups_;
        while (group && (group->chunk != chunk || group->freeHead == noChunk)) group = group->next;
        if (!group && !(group = carve(pool, chunk))) return nullptr;
        const std::size_t index = group->freeHead;
        unsigned char *chunkAt = group->base + index * chunk;
        std::size_t next = noChunk;
        std::memcpy(&next, chunkAt + tagBytes, sizeof(next));
        group->freeHead = next;
        ++group->live;
        const std::size_t tag = reinterpret_cast<std::size_t>(group) | 1u;
        std::memcpy(chunkAt, &tag, sizeof(tag));
        return chunkAt + tagBytes;
    }
    /**
     * @brief 判断一个指针是不是本层的片。
     * @param pointer 载荷指针。
     * @return 是则返回 true（此时才该调用 `release`）。
     */
    static bool holds(const void *pointer)
    {
        std::size_t tag = 0;
        std::memcpy(&tag, static_cast<const unsigned char *>(pointer) - tagBytes, sizeof(tag));
        return (tag & 1u) != 0u;
    }
    /**
     * @brief 归还一片；整组都空了出来就顺手把组还给池。
     * @param pool 底层池。
     * @param pointer 载荷指针。
     * @return 标签与片位置都对得上时返回 true。
     */
    bool release(McPool *pool, void *pointer)
    {
        unsigned char *chunkAt = static_cast<unsigned char *>(pointer) - tagBytes;
        std::size_t tag = 0;
        std::memcpy(&tag, chunkAt, sizeof(tag));
        Group *group = reinterpret_cast<Group *>(tag & ~static_cast<std::size_t>(1u));
        if (!group || !group->base || group->chunk == 0u) return false;
        const std::size_t offset = static_cast<std::size_t>(chunkAt - group->base);
        if (offset % group->chunk != 0u) return false;
        const std::size_t index = offset / group->chunk;
        if (index >= group->count || group->live == 0u) return false;
        std::memcpy(chunkAt + tagBytes, &group->freeHead, sizeof(group->freeHead));
        group->freeHead = index;
        if (--group->live == 0u)
        {
            detach(group);
            if (pool) chali_pool_release(pool, group->base);
            std::free(group);
        }
        return true;
    }
    /**
     * @brief 还有没有交出去没收回的片（泄漏检查用）。
     */
    bool hasLiveChunks() const
    {
        for (const Group *group = groups_; group; group = group->next)
            if (group->live != 0u) return true;
        return false;
    }
    /**
     * @brief 把整组还给池；调用前必须确认没有活动片。
     * @param pool 底层池。
     */
    void releaseAll(McPool *pool)
    {
        while (groups_)
        {
            Group *next = groups_->next;
            if (pool) chali_pool_release(pool, groups_->base);
            std::free(groups_);
            groups_ = next;
        }
    }
    /**
     * @brief 只丢描述符、不还池（池已销毁时的兜底）。
     */
    void abandon()
    {
        while (groups_)
        {
            Group *next = groups_->next;
            std::free(groups_);
            groups_ = next;
        }
    }

private:
    /**
     * @brief 把一组从链表里摘掉。
     */
    void detach(Group *group)
    {
        Group **slot = &groups_;
        while (*slot && *slot != group) slot = &(*slot)->next;
        if (*slot) *slot = group->next;
    }
    /**
     * @brief 选一个够用的片尺寸。
     * @param bytes 载荷字节数。
     * @return 片尺寸；超出上限返回 0。
     */
    static std::size_t chunkFor(std::size_t bytes)
    {
        for (std::size_t size : sizes)
            if (bytes + tagBytes <= size) return size;
        return 0u;
    }
    /**
     * @brief 向池要一整块，切成片并把它们串成空闲链表。
     * @param pool 底层池。
     * @param chunk 片尺寸。
     * @return 新组；池或描述符不够时返回 nullptr。
     */
    Group *carve(McPool *pool, std::size_t chunk)
    {
        void *raw = chali_pool_allocate(pool, blockBytes, tagBytes);
        if (!raw) return nullptr;
        Group *group = static_cast<Group *>(std::malloc(sizeof(Group)));
        if (!group) return nullptr; /* 池里那块先留着：容量代价一组 64 KiB，比丢掉分配好。 */
        group->base = static_cast<unsigned char *>(raw);
        group->chunk = chunk;
        group->count = blockBytes / chunk;
        group->live = 0;
        group->freeHead = 0;
        for (std::size_t index = 0; index < group->count; ++index)
        {
            const std::size_t following = index + 1u < group->count ? index + 1u : noChunk;
            std::memcpy(group->base + index * chunk + tagBytes, &following, sizeof(following));
        }
        group->next = groups_;
        groups_ = group;
        return group;
    }
    Group *groups_ = nullptr;  ///< 所有已切出的组。
};
class BufferRuntimeArena : public RuntimeAllocator
{
    mutable std::mutex mutex_;
    void *buffer_;
    std::size_t bytes_;
    McPool pool_{};
    SlabPool slabs_{};  ///< 小对象按组分配，剩下的才逐块进池。
    Stats last_{};

public:
    BufferRuntimeArena(void *buffer, std::size_t bytes) : buffer_(buffer), bytes_(bytes)
    {
        if (!chali_pool_init(&pool_, buffer, bytes)) throw std::bad_alloc();
    }
    BufferRuntimeArena(const BufferRuntimeArena &) = delete;
    BufferRuntimeArena &operator=(const BufferRuntimeArena &) = delete;
    std::size_t capacity() const override { return bytes_; }
    void *allocate(std::size_t bytes) override { return allocateAligned(bytes, alignof(std::max_align_t)); }
    void *allocateAligned(std::size_t bytes, std::size_t alignment) override
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (void *small = slabs_.allocate(&pool_, bytes, alignment)) return small;
        return chali_pool_allocate(&pool_, bytes, alignment);
    }
    bool release(void *p) override
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (SlabPool::holds(p)) return slabs_.release(&pool_, p);
        return chali_pool_release(&pool_, p) != 0;
    }
    bool owns(const void *p) const override { return chali_pool_owns(&pool_, p) != 0; }
    Stats stats() const override
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return { pool_.stats.live_bytes, pool_.stats.live_blocks, pool_.stats.peak_bytes, pool_.stats.total_blocks };
    }
    Stats lastRunStats() const { return last_; }
    std::size_t peakBytes() const { return last_.peakBytes; }
    void finish()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!pool_.storage) return;
        /* 组本身也要还，否则块数归不了零；有片没还就是真泄漏，直接留给下面的检查报错。 */
        if (!slabs_.hasLiveChunks()) slabs_.releaseAll(&pool_);
        if (pool_.stats.live_blocks)
        {
            std::fputs("运行时分配器：仍有活动块，不能清零\n", stderr);
            std::abort();
        }
        McPoolStats saved{};
        chali_pool_finish(&pool_, &saved);
        last_ = { saved.live_bytes, saved.live_blocks, saved.peak_bytes, saved.total_blocks };
    }
    void begin()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (pool_.stats.live_blocks) throw std::bad_alloc();
        if (!pool_.storage && !chali_pool_init(&pool_, buffer_, bytes_)) throw std::bad_alloc();
    }
};
class DynamicRuntimeArena final : public BufferRuntimeArena
{
    void *storage_;
    static void *reserve(std::size_t bytes)
    {
        if (bytes < 64) throw std::bad_alloc();
        void *p = std::malloc(bytes);
        if (!p) throw std::bad_alloc();
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
#ifdef NO_CHALI_DEFAULT_POOL
    throw std::bad_alloc();
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
    SlabPool slabs_{};     ///< 小对象按组分配，剩下的才逐块进池。
    mutable std::mutex mutex_; /* 静态 arena 可能被并发的 VM 线程共用，和 BufferRuntimeArena 一样加锁 */
public:
    StaticRuntimeArena() { reset(); }
    void reset()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (slabs_.hasLiveChunks())
        {
            std::fputs("运行时分配器：仍有活动块，不能重置\n", stderr);
            std::abort();
        }
        slabs_.releaseAll(&pool_); /* 组本身也占块，先还掉再判泄漏 */
        if (pool_.stats.live_blocks)
        {
            std::fputs("运行时分配器：仍有活动块，不能重置\n", stderr);
            std::abort();
        }
        chali_pool_init(&pool_, storage_, Capacity);
    }
    std::size_t capacity() const override { return pool_.capacity; }
    void *allocate(std::size_t bytes) override { return allocateAligned(bytes, alignof(std::max_align_t)); }
    void *allocateAligned(std::size_t bytes, std::size_t alignment) override
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (void *small = slabs_.allocate(&pool_, bytes, alignment)) return small;
        return chali_pool_allocate(&pool_, bytes, alignment);
    }
    bool release(void *p) override
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (SlabPool::holds(p)) return slabs_.release(&pool_, p);
        return chali_pool_release(&pool_, p) != 0;
    }
    bool owns(const void *p) const override
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return chali_pool_owns(&pool_, p) != 0;
    }
    Stats stats() const override
    {
        std::lock_guard<std::mutex> lock(mutex_);
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
        /* 软重启信号：**只记录**标志 + 原因（本对象不吃池，池耗尽后仍可读），
         * 失败点的行为一如既往（打印 + 抛 `std::bad_alloc`）；宿主读信号决定这是不是
         * "可恢复终止"。见本文件顶部的 `RestartSignal`。 */
        const RuntimeAllocator::Stats stats = owner->stats();
        restartSignal().recordFailure(WatchdogReason::PoolExhausted,
                                      bytes, stats.liveBytes, stats.liveBlocks, owner->capacity());
        if (owner) std::fprintf(stderr, "运行时分配器：arena 用尽（要 %zu 字节）\n", bytes);
        throw std::bad_alloc();
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
        if (count > std::numeric_limits<std::size_t>::max() / sizeof(T)) throw std::bad_array_new_length();
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
#ifdef CHALI_CUSTOM_ALLOCATOR
template <class T>
using ContainerAllocator = Allocator<T>;
template <class T>
using UniquePtr = std::unique_ptr<T, Deleter<T>>;
template <class T, class... Args>
UniquePtr<T> makeUnique(Args &&...args)
{
    auto *owner = Memory::runtimeAllocator();
    void *storage = Memory::allocate(owner, sizeof(T), alignof(T));
    try
    {
        T *object = new (storage) T(std::forward<Args>(args)...);
        return UniquePtr<T>(object, Deleter<T>(owner, storage, [](void *p) { static_cast<T *>(p)->~T(); }));
    } catch (...)
    {
        Memory::release(owner, storage);
        throw;
    }
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
#ifdef CHALI_CUSTOM_ALLOCATOR
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
        if (!callable_) throw std::bad_function_call();
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
#ifdef CHALI_CUSTOM_ALLOCATOR
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
#ifdef CHALI_CUSTOM_ALLOCATOR
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
#ifdef CHALI_CUSTOM_ALLOCATOR
    return ContainerAllocator<T>(Memory::defaultRuntimeAllocator());
#else
    return ContainerAllocator<T>();
#endif
}
}  // namespace Memory
#ifdef CHALI_CUSTOM_ALLOCATOR
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
