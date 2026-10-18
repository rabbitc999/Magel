/**
 * @file mcu_allocation_identity.cpp
 * @brief 板端静态字节池的逻辑身份、越界与缓存失效验证。
 */
#include "embed_pool_allocator.hpp"
#include <cstdio>
/**
 * @brief 在无异常编译口径下验证复用与幂等释放。
 * @return 失败为一。
 */
int main()
{
    embed_stackvm::EmbedPoolAllocator<1024> heap;
    heap.reset(8);
    const auto old = heap.allocate(16);
    char value = 42, read = 0;
    if (!old || !heap.writeBytes(old, &value, 1) || !heap.readBytes(old, &read, 1) || read != 42) return 1;
    if (!heap.release(old) || !heap.release(old)) return 2;
    const auto current = heap.allocate(16);
    if (!current || current == old || heap.readBytes(old, &read, 1) || !heap.release(old)) return 3;
    if (!heap.readBytes(current, &read, 1) || read != 0) return 4;
    std::size_t remaining = 0;
    if (!heap.capacityAt(current + 15, remaining) || remaining != 1 || heap.capacityAt(current + 16, remaining)) return 5;
    if (heap.release(current + 1) || !heap.writeBytes(current, &value, 1)) return 6;
    if (heap.sweep() != 1 || heap.release(old) || !heap.readBytes(current, &read, 1) || read != 42) return 9;
    if (!heap.release(current) || heap.liveBlocks() != 0 || heap.validate() != EMBED_POOL_ERR_OK) return 7;
    if (!heap.release(current) || heap.sweep() != 1 || heap.release(current)) return 10;
    heap.reset(8);
    if (heap.allocate(16) != old) return 8;
    std::puts("板端分配身份回归通过");
    return 0;
}
