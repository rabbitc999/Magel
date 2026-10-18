#ifndef PARSEENGINE_MAGEL_POOL_H
#define PARSEENGINE_MAGEL_POOL_H

#include <stddef.h>
#include <stdint.h>

/* 分配器类型宏：在**编译期**选择内存池的底层实现（用 -DMAGEL_ALLOCATOR_TYPE=… 覆盖；
 * 缺省链表式）。两种实现共用同一份 McPool 句柄、McPoolStats 与 magel_pool_* 接口，
 * 所以上层（magel_allocator.hpp 的 arena、容器、导出 exe）不需要任何改动。
 *
 *   MAGEL_ALLOCATOR_LINKED = 1（缺省）
 *       边界标记 + 显式空闲**双向链表**：分配用首次适配，切块后维护相邻块信息，
 *       释放时合并相邻空闲块。**释放过的地址可以再分配出去**，长循环里的
 *       “分配—释放—再分配”不会持续吃内存。
 *   MAGEL_ALLOCATOR_LINEAR = 2
 *       线性（bump）分配：只向后切块，释放只把块记成墓碑，地址到 reset 前**不复用**。
 *       适合“地址一旦给出就不再复用”的护栏场景，代价是长循环占用只增不减。 */
#define MAGEL_ALLOCATOR_LINKED 1
#define MAGEL_ALLOCATOR_LINEAR 2
#ifndef MAGEL_ALLOCATOR_TYPE
#define MAGEL_ALLOCATOR_TYPE MAGEL_ALLOCATOR_LINKED
#endif
#if (MAGEL_ALLOCATOR_TYPE != MAGEL_ALLOCATOR_LINKED) && (MAGEL_ALLOCATOR_TYPE != MAGEL_ALLOCATOR_LINEAR)
#error "MAGEL_ALLOCATOR_TYPE 只能是 MAGEL_ALLOCATOR_LINKED 或 MAGEL_ALLOCATOR_LINEAR"
#endif

/* C99, caller-owned storage; no dynamic metadata or fixed block-count limit.
 * A pool is single-threaded: it carries no lock. The caller must serialize concurrent access
 * (缺省 C++ 侧由 magel_allocator.hpp 的 arena 持 std::mutex 保证，见 BufferRuntimeArena /
 * StaticRuntimeArena)。
 * Functions have internal linkage so exported C++ programs remain one file. */
typedef struct McPoolStats
{
    size_t live_bytes, live_blocks, peak_bytes, total_blocks;
} McPoolStats;
typedef struct McPool
{
    void *raw_storage;
    size_t raw_capacity;
    unsigned char *storage;
    size_t capacity;
    size_t free_head;
    McPoolStats stats;
} McPool;

static inline int magel_pool_init(McPool *pool, void *storage, size_t capacity);
static inline void *magel_pool_allocate(McPool *pool, size_t bytes, size_t alignment);
static inline int magel_pool_release(McPool *pool, void *pointer);
static inline int magel_pool_owns(const McPool *pool, const void *pointer);
/* Fails while blocks remain live. Saves stats, then zeros the entire input buffer. */
static inline int magel_pool_finish(McPool *pool, McPoolStats *stats);

#endif
