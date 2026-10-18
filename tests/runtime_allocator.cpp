#include "../source/runtime_allocator.hpp"
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <limits>
#include <thread>
struct Base
{
    /**
     * @brief 执行分配器测试基类的虚析构。
     */
    virtual ~Base() = default;
};
struct Derived : Base
{
    int *count;
    /**
     * @brief 初始化分配器测试使用的派生对象。
     * @param p 输入字节位置或关联的测试计数器。
     */
    explicit Derived(int *p) : count(p) {}
    /**
     * @brief 释放测试派生对象并记录析构结果。
     */
    ~Derived() override { ++*count; }
};
struct alignas(256) Aligned
{ int value = 7; };
/**
 * @brief 运行本文件的回归测试并报告执行结果。
 * @param argc 命令行参数数量。
 * @retval 进程退出状态，0 表示成功，非零表示失败。
 */
int main(int argc, char **)
{
    Memory::StaticRuntimeArena<4096> small;
    assert(!small.allocate(std::numeric_limits<std::size_t>::max()));
    void *block = small.allocate(32);
    assert(block && reinterpret_cast<std::uintptr_t>(block) % alignof(std::max_align_t) == 0);
    assert(!small.release(static_cast<unsigned char *>(block) + 1));
    assert(small.release(block));
    assert(!small.release(block));
    if (argc > 1)
    {
        Memory::release(&small, block);
        return 1;
    }
    assert(Memory::defaultRuntimeAllocator()->stats().liveBlocks == 0);
    assert(Memory::defaultRuntimeCapacity == 512u * 1024u * 1024u);
    alignas(256) unsigned char storage[131072];
    std::fill(std::begin(storage), std::end(storage), 0xa5);
    Memory::BufferRuntimeArena arena(storage, sizeof(storage));
    for (int round = 0; round < 2; ++round)
    {
        {
            Memory::RuntimeSession session(arena);
            Memory::Vector<int> values(100, 42);
            Memory::Map<int, Memory::String> map;
            map.emplace(1, Memory::String(2048, 'x'));
            auto shared = Memory::makeShared<Aligned>();
            assert(reinterpret_cast<std::uintptr_t>(shared.get()) % 256 == 0);
            int destroyed = 0;
            {
                Memory::UniquePtr<Base> object = Memory::makeUnique<Derived>(&destroyed);
            }
            assert(destroyed == 1);
            Memory::Function<int()> callback = [n = 0]() mutable { return ++n; };
            auto copied = callback;
            assert(callback() == 1 && callback() == 2 && copied() == 1);
            Memory::OutputStringStream text;
            text << Memory::String(4096, 'z');
            assert(text.str().size() == 4096);
            assert(arena.stats().liveBytes >= 2448);
            // Ordinary host new is unaffected by the MAGEL allocator.
            const auto before = arena.stats().liveBlocks;
            int *host = new int(17);
            assert(*host == 17 && arena.stats().liveBlocks == before);
            delete host;
            std::thread worker([&] { Memory::AllocationScope scope(&arena); Memory::Vector<int> local(500, 8); assert(local[0] == 8); });
            worker.join();
            assert(arena.stats().liveBlocks > 0);
        }
        assert(arena.stats().liveBlocks == 0 && arena.peakBytes() >= 2448);
        assert(std::all_of(std::begin(storage), std::end(storage), [](unsigned char c) { return c == 0; }));
    }
    const auto peak = arena.peakBytes();
    try
    {
        Memory::RuntimeSession session(arena);
        Memory::Vector<unsigned char> tooLarge(sizeof(storage) * 2);
        assert(false);
    } catch (const std::bad_alloc &) {}
    assert(arena.peakBytes() == 0);
    assert(peak > 0);
    assert(std::all_of(std::begin(storage), std::end(storage), [](unsigned char c) { return c == 0; }));
    // An allocator captured before switching continues to release through its original owner.
    {
        Memory::setRuntimeAllocator(&small);
        Memory::Vector<int> values(12, 1);
        Memory::setRuntimeAllocator(nullptr);
        values.push_back(2);
        assert(small.stats().liveBlocks > 0);
    }
    assert(small.stats().liveBlocks == 0);
    Memory::DynamicRuntimeArena first(16384), second(16384);
    {
        Memory::AllocationScope scope(&first);
        Memory::Vector<Memory::String> original(2, Memory::String(200, 'a'));
        {
            Memory::AllocationScope target(&second);
            auto copied = original;
            assert(copied.get_allocator().owner == &second);
            assert(copied[0].get_allocator().owner == &second);
            Memory::Vector<Memory::String> assigned;
            assigned = original;
            assert(assigned.get_allocator().owner == &second);
            assert(assigned[0].get_allocator().owner == &second);
        }
        assert(second.stats().liveBlocks == 0);
    }
    first.finish();
    second.finish();
    Memory::DynamicRuntimeArena dynamic(8192);
    {
        Memory::RuntimeSession session(dynamic);
        auto p = Memory::makeUnique<Aligned>();
        assert(p->value == 7);
    }
    assert(dynamic.peakBytes() >= sizeof(Aligned));
}
