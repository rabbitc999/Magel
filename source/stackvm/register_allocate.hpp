/**
 * @file register_allocate.hpp
 * @brief 寄存器分配：活跃性分析 + 线性扫描 + 槽位复用。
 *
 * 这里的"寄存器"是调用帧里的槽位。原来的做法是**按声明顺序一人一个槽**——帧大小等于
 * 变量个数，函数里写得越长、临时量越多，帧就越大。这一层先做活跃性分析，再按存活区间
 * 线性扫描分配：两个变量只要不同时活着，就共用一个槽。
 *
 * 形参固定占 0..n-1（固定 ABI）：调用方按位置绑定实参，被调方不能挪动它们的槽号。
 *
 * 只有**形参**和**本函数里声明过（`Op::Declare`）的局部**才配方槽；其余符号（`__temp_*`、
 * `__chali_ret_v$*` 这类跨帧共享的暂存量）继续走全程序暂存表。这条边界是语义要求，不是
 * 优化取舍：暂存量在不同函数之间按同一份存储互相读写，给它们配帧槽就等于把它们拆成
 * 每帧一份，跨帧读到的会是空/陈旧值（拥有型句柄还会被释放两次）。
 */
#pragma once
#include "program_model.hpp"

namespace stackvm
{
namespace allocation
{
/**
 * @brief 一个函数体的活跃性视图，只描述**本函数**可达的指令。
 *
 * 所有数组都按**位置**（`order` 里的下标）索引，不按全局 PC——按 PC 建表会让每张表都
 * 有"整个程序的指令数"那么大，而这份内存来自运行期 arena（帧布局是执行期惰性算的），
 * 会和语言堆抢同一块地。
 */
struct Body
{
    Memory::Vector<std::size_t> order;                               ///< 本函数可达的 PC，按遍历顺序；数据流只扫这些。
    Memory::Vector<std::pair<std::size_t, std::size_t>> successors;  ///< 每个位置的两个后继（位置下标），`absent` 表示没有。
    Memory::Vector<std::size_t> defines;                             ///< 每个位置定义的符号，`absent` 表示没有。
    Memory::Vector<std::size_t> uses;                                ///< 每个位置使用的符号，`absent` 表示没有。
    Memory::Vector<std::size_t> parameters;                          ///< 形参符号，按实参顺序固定占用前面的槽。
    Memory::Vector<std::size_t> declared;                            ///< 本函数里有 `Op::Declare` 的符号（只有它们配帧槽）。
};

/**
 * @brief 分配结果。
 */
struct Layout
{
    Memory::Vector<std::pair<std::size_t, std::size_t>> slots;  ///< 符号 → 槽位，按符号升序。
    std::size_t count = 0;                                      ///< 帧槽数。
};

/**
 * @brief 累计的分配统计，供 `--stats` 比较复用前后的槽数。
 */
struct Stats
{
    std::size_t functions = 0;  ///< 参与分配的函数个数。
    std::size_t symbols = 0;    ///< 这些函数的局部符号总数（复用前的槽数）。
    std::size_t slots = 0;      ///< 复用后的槽位数。
};

/**
 * @brief 为一个函数分配槽位。
 * @param body 活跃性视图。
 * @param stats 可选的累计统计。
 * @return 符号到槽位的映射。
 * @note 纯函数：同样的输入永远得到同样的分配。
 */
[[nodiscard]] Layout assign(const Body &body, Stats *stats = nullptr);

/**
 * @brief 最近一次运行的累计统计。
 * @return 统计。
 */
Stats lastStats();

/**
 * @brief 清零累计统计。
 */
void resetStats();
}  // namespace allocation
}  // namespace stackvm
