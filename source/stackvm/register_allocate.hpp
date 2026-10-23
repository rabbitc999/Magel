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
 * **形参**、本函数里声明过（`Op::Declare`）的局部、以及编译器临时量（`__temp_*`、
 * `__chali_ret_v$*`，由执行计划单独收集）都配帧槽。临时量必须每次激活一份：它们只在本函数
 * 一次激活里写了再读，跨激活共享一份存储会让递归的内层调用覆写外层挂起的结果
 * （`return f(n-1) + f(n-1)` 的第一趟返回值会被第二趟递归覆盖）。其余未声明名字才是
 * 真正的全局变量，继续走全程序全局表——那条边界仍然保留。
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
    /* 按浮点形态用槽的名字：寄存器核是按「槽里现在是整数还是双精度」直接读写的，
     * 而一个槽只有一份 `Slot::isFloat`。把浮点槽复用给整数（或反过来）会让这块存储
     * 在两种形态之间来回跳，区域就永远绑不上（浮点循环因此退回逐条执行）。
     * 所以复用只在同形态之间进行：这份名单里的名字只和名单里的名字共用槽。 */
    Memory::Vector<std::size_t> reals;
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
