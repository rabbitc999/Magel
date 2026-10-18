#include "mcu_metadata_heap.hpp"
#include "mcu_arenas.hpp"
#include <cassert>
#include <cstdlib>
#include <cstdint>

/**
 * @brief 验证启动分配跨 SDRAM 注入后可释放，以及外部运行时池可替代过小占位池。
 * @retval 断言全部通过时返回零。
 */
int main()
{
    auto *early = new std::uint64_t(123);
    assert(!mcuMetadataInUse());
    constexpr std::size_t capacity = 2u * 1024u * 1024u;
    void *storage = std::malloc(capacity);
    assert(storage && mcuMetadataSetArena(storage, capacity));
    auto *late = new std::uint64_t(456);
    assert(mcuMetadataInUse() && mcuMetadataLiveBytes() > 0);
    assert(reinterpret_cast<std::uintptr_t>(late) >= reinterpret_cast<std::uintptr_t>(storage));
    assert(reinterpret_cast<std::uintptr_t>(late) < reinterpret_cast<std::uintptr_t>(storage) + capacity);
    assert(*early == 123 && *late == 456);
    delete early;
    delete late;
    assert(mcuMetadataLiveBytes() == 0);
    void *runtimeStorage = std::malloc(capacity);
    assert(runtimeStorage && mcuArenas::SetRuntimeArena(runtimeStorage, capacity));
    mcuArenas::InstallRuntimeArena();
    void *byteStorage = std::malloc(capacity);
    assert(byteStorage && mcuArenas::SetBytePool(byteStorage, capacity));
    auto &bytePool = mcuArenas::CurrentBytePool();
    auto address = bytePool.allocate(64);
    assert(address != 0 && bytePool.liveBlocks() == 1);
    assert(bytePool.release(address));
    const auto indexBlocks = mcuArenas::CurrentRuntimeArena().stats().liveBlocks;
    assert(indexBlocks > 0);
    assert(mcuArenas::RuntimeArenaCapacityBytes() == capacity);
    assert(!mcuArenas::SetRuntimeArena(runtimeStorage, capacity));
    {
        Memory::String text(1024, 'x');
        assert(text.size() == 1024);
    }
    assert(mcuArenas::CurrentRuntimeArena().stats().liveBlocks == indexBlocks);
    Memory::setRuntimeAllocator(nullptr);
    // 静态字节池索引持有此运行时分配器，缓冲须存活到进程结束。
    assert(!mcuMetadataSetArena(storage, capacity));
    std::free(storage);
    return 0;
}
