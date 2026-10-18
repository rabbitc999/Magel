#ifndef PARSEENGINE_MAGEL_POOL_IMPLEMENTATION
#define PARSEENGINE_MAGEL_POOL_IMPLEMENTATION
#include "magel_pool.h"
#include <stdlib.h>
#include <string.h>

typedef struct McPoolBlock
{
    size_t size, previous_size, payload, requested, next_free, previous_free;
} McPoolBlock;
#define MAGEL_POOL_ALIGNMENT ((size_t)sizeof(void *))
#define MAGEL_POOL_NONE      SIZE_MAX
static inline int magel_pool_valid_alignment(size_t a) { return a && !(a & (a - 1)); }
static inline McPoolBlock magel_pool_block(const McPool *p, size_t at)
{
    McPoolBlock b;
    memcpy(&b, p->storage + at, sizeof(b));
    return b;
}
static inline void magel_pool_store(McPool *p, size_t at, McPoolBlock b) { memcpy(p->storage + at, &b, sizeof(b)); }
#if MAGEL_ALLOCATOR_TYPE == MAGEL_ALLOCATOR_LINKED
static inline void magel_pool_remove_free(McPool *p, size_t at, McPoolBlock b)
{
    if (b.previous_free == MAGEL_POOL_NONE)
        p->free_head = b.next_free;
    else
    {
        McPoolBlock prev = magel_pool_block(p, b.previous_free);
        prev.next_free = b.next_free;
        magel_pool_store(p, b.previous_free, prev);
    }
    if (b.next_free != MAGEL_POOL_NONE)
    {
        McPoolBlock next = magel_pool_block(p, b.next_free);
        next.previous_free = b.previous_free;
        magel_pool_store(p, b.next_free, next);
    }
    (void)at;
}
static inline void magel_pool_add_free(McPool *p, size_t at, McPoolBlock b)
{
    b.previous_free = MAGEL_POOL_NONE;
    b.next_free = p->free_head;
    if (p->free_head != MAGEL_POOL_NONE)
    {
        McPoolBlock next = magel_pool_block(p, p->free_head);
        next.previous_free = at;
        magel_pool_store(p, p->free_head, next);
    }
    p->free_head = at;
    magel_pool_store(p, at, b);
}
#endif
static inline int magel_pool_init(McPool *p, void *storage, size_t capacity)
{
    uintptr_t start, aligned;
    size_t skipped, raw_capacity = capacity;
    McPoolBlock b;
    if (!p) return 0;
    memset(p, 0, sizeof(*p));
    p->free_head = MAGEL_POOL_NONE;
    if (!storage) return 0;
    start = (uintptr_t)storage;
    if (start > UINTPTR_MAX - (MAGEL_POOL_ALIGNMENT - 1)) return 0;
    aligned = (start + MAGEL_POOL_ALIGNMENT - 1) & ~(uintptr_t)(MAGEL_POOL_ALIGNMENT - 1);
    skipped = (size_t)(aligned - start);
    if (capacity < skipped) return 0;
    capacity = (capacity - skipped) & ~(MAGEL_POOL_ALIGNMENT - 1);
    if (capacity < sizeof(b) + sizeof(size_t) + MAGEL_POOL_ALIGNMENT) return 0;
    p->storage = (unsigned char *)storage + skipped;
    p->capacity = capacity;
    p->raw_storage = storage;
    p->raw_capacity = raw_capacity;
#if MAGEL_ALLOCATOR_TYPE == MAGEL_ALLOCATOR_LINKED
    memset(&b, 0, sizeof(b));
    b.size = capacity;
    magel_pool_add_free(p, 0, b);
#else
    (void)b;
    p->free_head = 0; /* 线性实现：free_head 当 bump 游标 */
#endif
    return 1;
}
static inline int magel_pool_owns(const McPool *p, const void *pointer)
{
    uintptr_t address = (uintptr_t)pointer, start;
    if (!p || !p->storage || !pointer) return 0;
    start = (uintptr_t)p->storage;
    return address >= start && address - start < p->capacity;
}
#if MAGEL_ALLOCATOR_TYPE == MAGEL_ALLOCATOR_LINKED
static inline void *magel_pool_allocate(McPool *p, size_t bytes, size_t alignment)
{
    size_t at;
    if (!p || !p->storage || !magel_pool_valid_alignment(alignment)) return NULL;
    if (!bytes) bytes = 1;
    for (at = p->free_head; at != MAGEL_POOL_NONE;)
    {
        McPoolBlock b = magel_pool_block(p, at);
        uintptr_t base = (uintptr_t)(p->storage + at), address;
        size_t payload, wanted;
        if (base > UINTPTR_MAX - sizeof(b) - sizeof(size_t) || base + sizeof(b) + sizeof(size_t) > UINTPTR_MAX - (alignment - 1)) return NULL;
        address = (base + sizeof(b) + sizeof(size_t) + alignment - 1) & ~(uintptr_t)(alignment - 1);
        payload = (size_t)(address - base);
        if (payload > SIZE_MAX - bytes || payload + bytes > SIZE_MAX - (MAGEL_POOL_ALIGNMENT - 1)) return NULL;
        wanted = (payload + bytes + MAGEL_POOL_ALIGNMENT - 1) & ~(MAGEL_POOL_ALIGNMENT - 1);
        if (wanted <= b.size)
        {
            magel_pool_remove_free(p, at, b);
            if (b.size - wanted >= sizeof(b) + sizeof(size_t) + MAGEL_POOL_ALIGNMENT)
            {
                McPoolBlock rest;
                size_t next;
                memset(&rest, 0, sizeof(rest));
                rest.size = b.size - wanted;
                rest.previous_size = wanted;
                b.size = wanted;
                magel_pool_add_free(p, at + wanted, rest);
                next = at + wanted + rest.size;
                if (next < p->capacity)
                {
                    McPoolBlock n = magel_pool_block(p, next);
                    n.previous_size = rest.size;
                    magel_pool_store(p, next, n);
                }
            }
            b.payload = payload;
            b.requested = bytes;
            magel_pool_store(p, at, b);
            memcpy((void *)(address - sizeof(size_t)), &at, sizeof(at));
            p->stats.live_bytes += b.size;
            ++p->stats.live_blocks;
            ++p->stats.total_blocks;
            if (p->stats.live_bytes > p->stats.peak_bytes) p->stats.peak_bytes = p->stats.live_bytes;
            return (void *)address;
        }
        at = b.next_free;
    }
    return NULL;
}
static inline int magel_pool_release(McPool *p, void *pointer)
{
    size_t at, next;
    McPoolBlock b;
    if (!magel_pool_owns(p, pointer) || (uintptr_t)pointer - (uintptr_t)p->storage < sizeof(b) + sizeof(size_t)) return 0;
    memcpy(&at, (unsigned char *)pointer - sizeof(size_t), sizeof(at));
    if (at % MAGEL_POOL_ALIGNMENT || at > p->capacity - sizeof(b)) return 0;
    b = magel_pool_block(p, at);
    if (!b.payload || b.size < sizeof(b) + sizeof(size_t) || b.size > p->capacity - at || b.payload >= b.size || p->storage + at + b.payload != pointer) return 0;
    p->stats.live_bytes -= b.size;
    --p->stats.live_blocks;
    b.payload = b.requested = 0;
    magel_pool_store(p, at, b);
    next = at + b.size;
    if (next < p->capacity)
    {
        McPoolBlock n = magel_pool_block(p, next);
        if (!n.payload)
        {
            magel_pool_remove_free(p, next, n);
            b.size += n.size;
        }
    }
    if (b.previous_size && b.previous_size <= at)
    {
        size_t prev_at = at - b.previous_size;
        McPoolBlock prev = magel_pool_block(p, prev_at);
        if (!prev.payload)
        {
            magel_pool_remove_free(p, prev_at, prev);
            prev.size += b.size;
            b = prev;
            at = prev_at;
        }
    }
    next = at + b.size;
    if (next < p->capacity)
    {
        McPoolBlock n = magel_pool_block(p, next);
        n.previous_size = b.size;
        magel_pool_store(p, next, n);
    }
    magel_pool_add_free(p, at, b);
    return 1;
}
#else /* MAGEL_ALLOCATOR_TYPE == MAGEL_ALLOCATOR_LINEAR */
static inline void *magel_pool_allocate(McPool *p, size_t bytes, size_t alignment)
{
    size_t at, avail, wanted;
    uintptr_t base, address;
    size_t payload;
    McPoolBlock b;
    if (!p || !p->storage || !magel_pool_valid_alignment(alignment)) return NULL;
    if (!bytes) bytes = 1;
    at = p->free_head; /* 线性实现：free_head 当 bump 游标（已对齐到 MAGEL_POOL_ALIGNMENT） */
    if (at > p->capacity) return NULL;
    avail = p->capacity - at;
    if (sizeof(b) + sizeof(size_t) > avail) return NULL;
    base = (uintptr_t)(p->storage + at);
    if (base > UINTPTR_MAX - sizeof(b) - sizeof(size_t) - (alignment - 1)) return NULL;
    address = (base + sizeof(b) + sizeof(size_t) + alignment - 1) & ~(uintptr_t)(alignment - 1);
    payload = (size_t)(address - base);
    if (payload > avail || bytes > avail - payload) return NULL;
    if (payload + bytes > SIZE_MAX - (MAGEL_POOL_ALIGNMENT - 1)) return NULL;
    wanted = (payload + bytes + MAGEL_POOL_ALIGNMENT - 1) & ~(MAGEL_POOL_ALIGNMENT - 1);
    if (wanted > avail) return NULL;
    memset(&b, 0, sizeof(b));
    b.size = wanted;
    b.previous_size = at;
    b.payload = payload;
    b.requested = bytes;
    magel_pool_store(p, at, b);
    memcpy((void *)(address - sizeof(size_t)), &at, sizeof(at));
    p->free_head = at + wanted;
    p->stats.live_bytes += b.size;
    ++p->stats.live_blocks;
    ++p->stats.total_blocks;
    if (p->stats.live_bytes > p->stats.peak_bytes) p->stats.peak_bytes = p->stats.live_bytes;
    return (void *)address;
}
static inline int magel_pool_release(McPool *p, void *pointer)
{
    size_t at;
    McPoolBlock b;
    if (!magel_pool_owns(p, pointer) || (uintptr_t)pointer - (uintptr_t)p->storage < sizeof(b) + sizeof(size_t)) return 0;
    memcpy(&at, (unsigned char *)pointer - sizeof(size_t), sizeof(at));
    if (at % MAGEL_POOL_ALIGNMENT || at > p->capacity - sizeof(b)) return 0;
    b = magel_pool_block(p, at);
    if (!b.payload || b.size < sizeof(b) + sizeof(size_t) || b.size > p->capacity - at || b.payload >= b.size || p->storage + at + b.payload != pointer) return 0;
    p->stats.live_bytes -= b.size;
    --p->stats.live_blocks;
    /* 墓碑：地址保留记录但不再复用，直到 reset / 整池清零。 */
    b.payload = b.requested = 0;
    magel_pool_store(p, at, b);
    return 1;
}
#endif
static inline int magel_pool_finish(McPool *p, McPoolStats *stats)
{
    size_t i;
    volatile unsigned char *buffer;
    if (!p || !p->storage || p->stats.live_blocks) return 0;
    if (stats) *stats = p->stats;
    buffer = (volatile unsigned char *)p->raw_storage;
    for (i = 0; i < p->raw_capacity; ++i) buffer[i] = 0;
    memset(p, 0, sizeof(*p));
    p->free_head = MAGEL_POOL_NONE;
    return 1;
}
#endif
