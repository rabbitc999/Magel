/**
 * @file benchmark_mc.cpp
 * @brief 加载同一份 MAGEL .svmb，在两种宿主位数下测量 VM 执行时间。
 */

#include "stackvm.hpp"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

namespace
{
/**
 * @brief 读取单调时钟的纳秒计时值。
 * @retval 单调时钟的当前纳秒计数。
 */
std::uint64_t nowNanoseconds()
{
    return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(
                                          std::chrono::steady_clock::now().time_since_epoch())
                                          .count());
}
}  // namespace

/**
 * @brief 运行虚拟机性能基准并输出计时结果。
 * @param argc 命令行参数数量。
 * @param argv 命令行参数指针数组。
 * @retval 进程退出状态，0 表示成功，非零表示失败。
 */
int main(int argc, char **argv)
{
    if (argc < 4 || argc > 5)
    {
        std::cerr << "用法：magel-stackvm-magel-benchmark <程序.svmb> <数据.bin> <布局> [重复次数]\n";
        return 2;
    }
    const int repeats = argc == 5 ? std::atoi(argv[4]) : 7;
    if (repeats < 1)
    {
        std::cerr << "重复次数必须大于零\n";
        return 2;
    }

    stackvm::Program program;
    Memory::Vector<Memory::String> diagnostics;
    if (!stackvm::loadProgramBinary(argv[1], program, &diagnostics))
    {
        for (const Memory::String &message : diagnostics) std::cerr << message << '\n';
        return 1;
    }

    stackvm::DataArea data;
    if (!data.load(argv[2], argv[3], &diagnostics))
    {
        for (const Memory::String &message : diagnostics) std::cerr << message << '\n';
        return 1;
    }

    stackvm::Machine machine;
    machine.setData(&data);
    machine.setTimeCallback(nowNanoseconds);
    constexpr std::size_t kStepLimit = 100000000u;
    const stackvm::RunResult warmup = machine.run(program, {}, kStepLimit);
    if (!warmup.ok)
    {
        std::cerr << "预热失败：" << warmup.error << '\n';
        return 1;
    }

    Memory::Vector<std::uint64_t> samples;
    samples.reserve(static_cast<std::size_t>(repeats));
    std::size_t steps = 0;
    std::size_t maxStackDepth = 0;
    for (int run = 0; run < repeats; ++run)
    {
        const stackvm::RunResult result = machine.run(program, {}, kStepLimit);
        if (!result.ok || !result.timingAvailable)
        {
            std::cerr << "测量失败：" << result.error << '\n';
            return 1;
        }
        samples.push_back(result.elapsedNanoseconds);
        steps = result.steps;
        maxStackDepth = std::max(maxStackDepth, result.maxStackDepth);
    }

    std::sort(samples.begin(), samples.end());
    const std::uint64_t median = samples[samples.size() / 2u];
    const double seconds = static_cast<double>(median) / 1.0e9;
    const double mips = seconds == 0.0 ? 0.0 : static_cast<double>(steps) / seconds / 1.0e6;
    std::cout << "Magel .svmb benchmark\n"
              << "repeats=" << repeats << " steps=" << steps << " median_ns=" << median << '\n'
              << std::fixed << std::setprecision(3)
              << "median_ms=" << seconds * 1000.0 << " throughput_mips=" << mips << '\n'
              << "max_stack_slots=" << maxStackDepth << " checksum=asserted\n";
    if (!Memory::runtimeMemoryStatsAvailable)
        std::cout << "runtime_pool_peak_available=0\n";
    else
        std::cout << "runtime_pool_capacity=" << Memory::defaultRuntimeCapacity
                  << " runtime_pool_peak_bytes=" << Memory::runtimeAllocator()->stats().peakBytes << '\n';
    return 0;
}
