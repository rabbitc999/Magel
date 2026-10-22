/**
 * @file vm_trim.cpp
 * @brief 验证共享 VM 的功能裁剪、显式静态运行时池及缺省堆禁用行为。
 */
#include "stackvm.hpp"
#include <iostream>

/**
 * @brief 在裁剪 VM 中组装、完整优化并执行一段四元式。
 * @param source 四元式文本。
 * @param expectedSuccess 预期是否正常结束。
 * @param expectedOutput 成功时的数值输出，失败时为必须包含的诊断片段。
 * @return 执行状态和预期一致时返回 true。
 */
static bool runCase(const char *source, bool expectedSuccess, const char *expectedOutput = "")
{
    Memory::restartSignal().clear();
    auto quads = stackvm::parseQuads(source, "trim.quad");
    stackvm::optimize(quads);
    stackvm::Program program;
    if (!stackvm::assemble(quads, program)) return false;
    stackvm::DataArea data;
    data.binary.resize(16u, 0u);
    stackvm::Machine machine;
    machine.setData(&data);
    const auto result = machine.run(program);
    if (result.ok != expectedSuccess || (expectedSuccess ? result.output != expectedOutput : result.error.find(expectedOutput) == Memory::String::npos))
    {
        std::cerr << "unexpected trimmed VM result: " << result.error << '\n';
        return false;
    }
    return true;
}

/**
 * @brief 在没有默认池、默认字节堆和标准胶水的构建中验证实际 VM 执行。
 * @return 全部裁剪及外部内存池检查通过时返回零。
 */
int main()
{
    static_assert(STACKVM_WITH_REFLECTION == 1 && CHALI_WITH_REFLECTION == 1);
    static_assert(STACKVM_WITH_DEFAULT_HEAP == 0 && STACKVM_WITH_THREADS == 0);
    static Memory::StaticRuntimeArena<8u * 1024u * 1024u> arena;
    Memory::AllocationScope scope(&arena);
    bool rejectedDefaultPool = false;
    try
    {
        (void)Memory::defaultRuntimeAllocator();
    } catch (const std::bad_alloc &)
    {
        rejectedDefaultPool = true;
    }
    if (!rejectedDefaultPool) return 1;
    if (!runCase("0: (+, 19, 23, value)\n1: (write, value, _, _)\n2: (halt, _, _, _)\n", true, "42")) return 2;
    if (!runCase("0: (decl, i64, global, pointer)\n1: (alloc, 8, _, pointer)\n2: (halt, _, _, _)\n", false)) return 3;
    for (const char *name : { "__std_sin", "__std_bint_from_int", "__std_bfloat_from_int" })
    {
        const auto text = Memory::String("0: (call, ") + name + ", _, value)\n1: (halt, _, _, _)\n";
        if (!runCase(text.c_str(), false, name)) return 4;
    }
    if (arena.stats().totalBlocks == 0u) return 5;
    std::cout << "trimmed VM and explicit static runtime pool passed\n";
    return 0;
}
