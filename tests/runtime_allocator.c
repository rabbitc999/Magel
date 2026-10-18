#include "../source/allocator/magel_pool.c"
#include <assert.h>
#include <stdint.h>
#include <string.h>
int main(void)
{
    unsigned char buffer[8192];
    McPool pool;
    void *blocks[40];
    size_t i;
    McPoolStats saved;
    memset(buffer, 0xa5, sizeof(buffer));
    assert(magel_pool_init(&pool, buffer + 1, sizeof(buffer) - 1));
    assert(!magel_pool_allocate(&pool, SIZE_MAX, 16));
    assert(!magel_pool_allocate(&pool, 1, 3));
    for (i = 0; i < 40; ++i)
    {
        size_t alignment = (size_t)1 << (i % 8);
        blocks[i] = magel_pool_allocate(&pool, 40 + i, alignment);
        assert(blocks[i] && (uintptr_t)blocks[i] % alignment == 0);
        memset(blocks[i], (int)i, 40 + i);
    }
    for (i = 0; i < 40; ++i)
    {
        assert(((unsigned char *)blocks[i])[39 + i] == (unsigned char)i);
        assert(!magel_pool_release(&pool, (unsigned char *)blocks[i] + 1));
    }
    for (i = 0; i < 40; i += 2) assert(magel_pool_release(&pool, blocks[i]));
    for (i = 1; i < 40; i += 2) assert(magel_pool_release(&pool, blocks[i]));
    assert(pool.stats.live_blocks == 0 && pool.stats.live_bytes == 0);
    assert(pool.stats.peak_bytes > 40 * 40 + 39 * 40 / 2);
    assert(!magel_pool_release(&pool, blocks[0]));
    for (i = 0; i < 40; ++i) assert(!magel_pool_release(&pool, blocks[i]));
#if MAGEL_ALLOCATOR_TYPE == MAGEL_ALLOCATOR_LINKED
    blocks[0] = magel_pool_allocate(&pool, 7600, 64);
    assert(blocks[0] && !magel_pool_finish(&pool, &saved));
    assert(magel_pool_release(&pool, blocks[0]));
    assert(magel_pool_finish(&pool, &saved) && saved.peak_bytes > 0);
    assert(buffer[0] == 0xa5);
    for (i = 1; i < sizeof(buffer); ++i) assert(buffer[i] == 0);
#else /* MAGEL_ALLOCATOR_LINEAR：释放留墓碑，地址不复用，容量只减不增 */
    {
        unsigned char arena[4096];
        void *first, *second;
        size_t bump;
        assert(magel_pool_init(&pool, arena, sizeof(arena)));
        first = magel_pool_allocate(&pool, 1000, 16);
        assert(first && (uintptr_t)first % 16 == 0);
        assert(magel_pool_release(&pool, first));
        assert(!magel_pool_release(&pool, first)); /* 双重释放必须报错 */
        bump = pool.free_head;
        second = magel_pool_allocate(&pool, 1000, 16);
        assert(second && second != first); /* 线性模式地址不复用 */
        assert(pool.free_head > bump);
        while (magel_pool_allocate(&pool, 256, 16) != NULL) {}
        assert(pool.stats.live_blocks > 0);
        assert(magel_pool_finish(&pool, &saved) == 0); /* 还有活动块，不能清零 */
    }
#endif
    {
        unsigned char arena[65536];
        void *live[128] = { 0 };
        size_t lengths[128] = { 0 };
        uint32_t random = 12345;
        assert(magel_pool_init(&pool, arena, sizeof(arena)));
        for (i = 0; i < 50000; ++i)
        {
            size_t slot, j;
            random = random * 1664525u + 1013904223u;
            slot = (random >> 16) % 128;
            if (live[slot])
            {
                for (j = 0; j < lengths[slot]; ++j)
                    assert(((unsigned char *)live[slot])[j] == (unsigned char)slot);
                assert(magel_pool_release(&pool, live[slot]));
                live[slot] = NULL;
            }
            else
            {
                lengths[slot] = 1 + random % 511;
                live[slot] = magel_pool_allocate(&pool, lengths[slot], (size_t)1 << (random % 9));
                if (live[slot]) memset(live[slot], (int)slot, lengths[slot]);
            }
        }
        for (i = 0; i < 128; ++i)
            if (live[i]) assert(magel_pool_release(&pool, live[i]));
        assert(pool.stats.live_blocks == 0);
        assert(magel_pool_finish(&pool, &saved));
        for (i = 0; i < sizeof(arena); ++i) assert(arena[i] == 0);
    }
    return 0;
}
