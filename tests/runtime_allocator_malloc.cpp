#include "chali_runtime_allocator.hpp"
#include <cassert>
/**
 * @brief 运行本文件的回归测试并报告执行结果。
 * @retval 进程退出状态，0 表示成功，非零表示失败。
 */
int main()
{
    static_assert(!Memory::runtimeMemoryStatsAvailable);
    alignas(64) unsigned char buffer[256];
    for (auto &b : buffer) b = 0xa5;
    Memory::BufferRuntimeArena arena(buffer, sizeof buffer);
    {
        Memory::RuntimeSession session(arena);
        Memory::Vector<int> values(10000, 42);
        auto p = Memory::makeShared<int>(7);
        assert(values[9999] == 42 && *p == 7);
        assert(arena.stats().peakBytes == 0);
    }
    for (auto b : buffer) assert(b == 0xa5);
    assert(arena.peakBytes() == 0);
}
