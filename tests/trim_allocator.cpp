/**
 * @file trim_allocator.cpp
 * @brief 验证裁剪默认池后的显式区域与大数开关联动。
 */
#include "../source/stackvm/stackvm.hpp"
#include <cassert>
/**
 * @brief 执行裁剪配置检查。
 * @return 成功返回零。
 */
int main()
{
#ifdef NO_CHALI_BIG_INT
    static_assert(!STACKVM_WITH_BIGINT && !STACKVM_WITH_BIGFLOAT);
#endif
#ifdef NO_CHALI_BIG_FLOAT
    static_assert(!STACKVM_WITH_BIGFLOAT);
#endif
#ifdef NO_CHALI_DEFAULT_POOL
    bool rejected = false;
    try
    {
        Memory::defaultRuntimeAllocator();
    } catch (const std::bad_alloc &) { rejected = true; }
    assert(rejected && !Memory::defaultRuntimeAllocatorIfCreated());
#endif
    alignas(std::max_align_t) unsigned char storage[16384];
    Memory::BufferRuntimeArena arena(storage, sizeof(storage));
    {
        Memory::RuntimeSession session(arena);
        Memory::Vector<int> numbers(128, 42);
        assert(numbers.back() == 42 && arena.stats().liveBlocks > 0);
    }
    assert(arena.stats().liveBlocks == 0);
}
