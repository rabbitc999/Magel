#ifndef PARSEENGINE_CHALI_POOL_H
#define PARSEENGINE_CHALI_POOL_H

#include <stddef.h>
#include <stdint.h>

/* 分配器类型宏：在**编译期**选择内存池的底层实现（用 -DCHALI_ALLOCATOR_TYPE=… 覆盖；
 * 缺省 TLSF）。两种实现共用同一份 McPool 句柄、McPoolStats 与 chali_pool_* 接口，
 * 所以上层（chali_allocator.hpp 的 arena、容器、导出 exe）不需要任何改动。
 *
 *   CHALI_ALLOCATOR_LINKED = 1（缺省）
 *       TLSF（两级隔离适配）：块头 16 字节，两级位图 + 空闲双向链表定位，
 *       分配与释放都是 O(1)，释放时立即合并相邻空闲块。**释放过的地址可以再分配
 *       出去**，长循环里的“分配—释放—再分配”不会持续吃内存。
 *   CHALI_ALLOCATOR_LINEAR = 2
 *       线性（bump）分配：只向后切块，释放只把块记成墓碑，地址到 reset 前**不复用**。
 *       适合“地址一旦给出就不再复用”的护栏场景，代价是长循环占用只增不减。 */
#define CHALI_ALLOCATOR_LINKED 1
#define CHALI_ALLOCATOR_LINEAR 2
#ifndef CHALI_ALLOCATOR_TYPE
#define CHALI_ALLOCATOR_TYPE CHALI_ALLOCATOR_LINKED
#endif
#if (CHALI_ALLOCATOR_TYPE != CHALI_ALLOCATOR_LINKED) && (CHALI_ALLOCATOR_TYPE != CHALI_ALLOCATOR_LINEAR)
#error "CHALI_ALLOCATOR_TYPE 只能是 CHALI_ALLOCATOR_LINKED 或 CHALI_ALLOCATOR_LINEAR"
#endif
/* TLSF 的两级索引规模：32 个一级 × 32 个二级，覆盖到 512 GiB 以内的请求。 */
#define CHALI_FL_COUNT 32
#define CHALI_SL_COUNT 32

/* C99, caller-owned storage; no dynamic metadata or fixed block-count limit.
 * A pool is single-threaded: it carries no lock. The caller must serialize concurrent access
 * (缺省 C++ 侧由 chali_allocator.hpp 的 arena 持 std::mutex 保证，见 BufferRuntimeArena /
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
#if CHALI_ALLOCATOR_TYPE == CHALI_ALLOCATOR_LINKED
    /* TLSF 的两级空闲表与位图；放在句柄里，于是句柄可以随 arena 一起静态分配。 */
    void *free_lists[CHALI_FL_COUNT][CHALI_SL_COUNT];
    uint32_t fl_bitmap;
    uint32_t sl_bitmap[CHALI_FL_COUNT];
#endif
} McPool;

static inline int chali_pool_init(McPool *pool, void *storage, size_t capacity);
static inline void *chali_pool_allocate(McPool *pool, size_t bytes, size_t alignment);
static inline int chali_pool_release(McPool *pool, void *pointer);
static inline int chali_pool_owns(const McPool *pool, const void *pointer);
/* Fails while blocks remain live. Saves stats, then zeros the entire input buffer. */
static inline int chali_pool_finish(McPool *pool, McPoolStats *stats);

#endif
