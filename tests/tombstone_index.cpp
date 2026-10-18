/**
 * @file tombstone_index.cpp
 * @brief 墓碑压缩、拆分复用及清扫边界回归。
 */
#include "../source/runtime_allocator.hpp"
#include "../library/stackvm/allocation_index.hpp"
#include <cassert>
#include <cstdio>
/**
 * @brief 检查长期复用的存储上界及旧引用释放隔离。
 * @return 成功为零。
 */
int main()
{
    stackvm::execution_detail::AllocationIndex index;
    index.reset(8);
    const auto first = index.add(0, 32);
    assert(first && index.release(first));
    for (int i = 0; i < 1000000; ++i)
    {
        const auto current = index.add(0, 16);
        assert(current && index.release(first));
        assert(index.find(current) && !index.find(first));
        assert(!index.release(current + 1) && index.release(current));
    }
    assert(index.slotCount() == 1 && index.retiredCount() == 1000001);
    const auto live = index.add(0, 64);
    assert(live);
    const auto other = index.add(128, 8);
    assert(other && index.release(other));
    assert(index.slotCount() == 2 && index.sweep() == 1000002);
    assert(!index.release(first) && !index.release(other) && index.find(live));
    assert(index.release(live) && index.release(live));
    assert(index.sweep() == 1 && index.sweep() == 0 && !index.release(live));
    const auto after = index.add(0, 8);
    assert(after && after != live && index.release(after));
    index.reset(8);
    assert(index.slotCount() == 0 && !index.retired(after));
    const auto small = index.add(0, 8);
    assert(small && index.release(small));
    const auto large = index.add(0, 2u * 1024u * 1024u);
    assert(large && index.release(small));
    assert(!index.find(small + 8) && index.find(large + 1024u * 1024u));
    assert(index.slotCount() == 1 && index.release(large));
    index.reset(8);
    index.reset(8);
    const auto left = index.add(0, 8);
    assert(left && index.release(left));
    auto right = index.add(8, 8);
    assert(right && index.release(right));
    right = index.add(8, 8);
    assert(right && index.release(right));
    const auto merged = index.add(0, 16);
    assert(merged && !index.find(right));
    assert(index.release(right) && index.find(merged + 8) && !index.release(right + 1));
    assert(index.release(merged));
    stackvm::execution_detail::AllocationIndex limited(3);
    limited.reset(8);
    std::size_t exhausted = 0;
    for (int i = 0; i < 3; ++i)
    {
        exhausted = limited.add(0, 16);
        assert(exhausted && limited.release(exhausted));
    }
    assert(!limited.add(0, 16) && limited.retired(exhausted));
    std::size_t placement = 0;
    assert(limited.placement(0, 32, 16, placement) && placement == 1);
    const auto fallback = limited.add(placement, 16);
    assert(fallback && limited.release(fallback));
    assert(limited.placement(0, 32, 16, placement) && placement == 1);
    assert(!index.add(std::numeric_limits<std::size_t>::max() - 7, 16));
    std::puts("墓碑索引：百万次分配一个槽，清扫边界与活跃身份保留通过");
}
