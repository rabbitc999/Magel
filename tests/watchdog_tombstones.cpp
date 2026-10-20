/**
 * @file watchdog_tombstones.cpp
 * @brief 在宿主收尾边界验证 VM清空墓碑。
 */
#include "stackvm.hpp"
#include "../library/stackvm_mcu/embed_pool_allocator.hpp"
#include <iostream>

/**
 * @brief 验证重启清扫发生在下一轮初始化之前，避免把新初始化产生的墓碑误认为残留。
 * @return 成功返回零。
 */
int main()
{
    using namespace stackvm;
    embed_stackvm::EmbedPoolAllocator<4096> heap;
    DataArea data;
    data.binary.resize(8u, 0u);
    Program program;
    const auto quads = parseQuads(
        "0: (alloc, 16, _, p)\n1: (free, p, _, _)\n"
        "2: (param, 0, _, _)\n3: (call, __watchdogEnable, _, enabled)\n"
        "4: (call, __watchdogRequestRestart, _, asked)\n5: (halt, _, _, _)\n",
        "<watchdog-sweep>");
    if (!assemble(quads, program)) return 1;
    Machine machine;
    machine.setData(&data);
    machine.setHeapAllocator(&heap);
    const auto result = machine.run(program);
    if (!result.recoverable || heap.sweep() != 0u) return 2;
    std::cout << "VM watchdog tombstones cleared before reset\n";
}
