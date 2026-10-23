/**
 * @file bigint_benchmark.cpp
 * @brief 使用固定输入和五轮中位数测量大整数乘除法及算法交叉区。
 */
#include "../source/bignum/bigint.hpp"
#include <algorithm>
#include <chrono>
#include <iostream>
#include <limits>
#include <vector>
using bigint::BigInt;
volatile std::size_t sink = 0;
/**
 * @brief 构造最高块非零的固定种子稠密幅值。
 * @param size 32 位分块数量。
 * @param salt xorshift 的非零种子。
 * @return 指定宽度的大整数。
 */
BigInt makeOperand(std::size_t size, unsigned salt)
{
    Memory::String text;
    std::uint32_t state = salt;
    for (std::size_t i = 0; i < size * 8; ++i)
    {
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        text.push_back("89abcdef"[state % 8]);
    }
    return BigInt::fromString(text, 16);
}
/**
 * @brief 返回五轮重复计算的单次耗时中位数。
 * @param operation 产生大整数结果的操作。
 * @param repeats 每轮重复次数。
 * @return 单次操作耗时，单位为微秒。
 */
template <class F>
double measureMedian(F operation, int repeats)
{
    std::vector<double> times;
    for (int round = 0; round < 5; ++round)
    {
        auto start = std::chrono::steady_clock::now();
        for (int i = 0; i < repeats; ++i) sink = operation().limbCount();
        times.push_back(std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - start).count() / repeats);
    }
    std::sort(times.begin(), times.end());
    return times[2];
}
/**
 * @brief 输出默认乘除法、强制乘法内核或除法阈值的 CSV 性能数据。
 * @param argc 命令行参数数量。
 * @param argv 可选 --tune 或 --division 参数。
 * @return 正常完成时返回 0。
 */
int main(int argc, char **argv)
{
    const std::string mode = argc > 1 ? argv[1] : "";
    const bool tune = mode == "--tune";
    if (mode == "--division")
    {
        std::cout << "divisor_limbs,threshold,microseconds\n";
        for (auto size : { 32u, 56u, 64u, 128u, 256u, 512u, 1024u, 2048u, 4096u, 8192u })
        {
            auto a = makeOperand(size * 2, 123), b = makeOperand(size, 456);
            for (auto threshold : { 56u, 128u, 256u, 512u, 1024u, 1048576u })
            {
                BigInt::setDivisionThreshold(threshold);
                std::cout << size << ',' << threshold << ',' << measureMedian([&] { return a / b; }, std::max(1, 1000 / static_cast<int>(size))) << '\n';
            }
        }
        return 0;
    }
    std::cout << "operation,left_limbs,right_limbs,microseconds\n";
    for (std::size_t size : { 1u, 2u, 8u, 16u, 32u, 48u, 64u, 96u, 128u, 192u, 256u, 384u, 512u, 768u, 1024u, 1536u, 2048u, 3072u, 4096u, 6144u, 8192u, 12288u, 16384u, 24576u })
    {
        auto a = makeOperand(size, 123), b = makeOperand(size, 456);
        int repeats = std::max(1, static_cast<int>(2000 / size));
        if (tune)
        {
            BigInt::setMultiplicationThreshold(std::numeric_limits<std::size_t>::max());
            std::cout << "karatsuba," << size << ',' << size << ',' << measureMedian([&] { return a * b; }, repeats) << '\n';
            BigInt::setMultiplicationThreshold(1);
            std::cout << "ntt," << size << ',' << size << ',' << measureMedian([&] { return a * b; }, repeats) << '\n';
        }
        else
        {
            std::cout << "multiply," << size << ',' << size << ',' << measureMedian([&] { return a * b; }, repeats) << '\n';
            std::cout << "square," << size << ',' << size << ',' << measureMedian([&] { return a * a; }, repeats) << '\n';
            a.shiftLeft(size * 32);
            std::cout << "divide," << size * 2 << ',' << size << ',' << measureMedian([&] { return a / b; }, repeats) << '\n';
        }
        std::cout.flush();
    }
    if (!tune)
    {
        auto a = makeOperand(48000, 123);
        for (auto size : { 1u, 8u, 96u, 1000u })
        {
            auto b = makeOperand(size, 456);
            std::cout << "unbalanced," << 48000 << ',' << size << ',' << measureMedian([&] { return a * b; }, 1) << '\n';
        }
    }
    return 0;
}
