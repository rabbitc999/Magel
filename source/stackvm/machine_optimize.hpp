/**
 * @file machine_optimize.hpp
 * @brief 机器级优化：小函数内联、常量折叠 / 传播、死代码消除、公共子表达式、
 *        强度削减与循环不变量外提。
 *
 * 分工：`quad_optimizer` 负责**语义级**化简（它先跑），这一层只做"接近目标机"的
 * 改写——把能在编译期算出来的算掉、把重复计算合并、把乘法换成移位、把循环里
 * 不变的式子提到前置头、把循环里的小调用就地展开。所有改写都必须保住两件事：
 * **值与陷阱时机**。会失败的操作（除零、除法溢出、移位越界）一律不折叠、不外提；
 * 内联只展开"非递归、函数体很小"的被调函数，递归与找不到函数体的一律原样调用。
 */
#pragma once
#include "quad_model.hpp"

namespace stackvm
{
namespace machine
{
/**
 * @brief 机器级优化的统计。
 */
struct Stats
{
    std::size_t before = 0;        ///< 优化前的四元式条数。
    std::size_t after = 0;         ///< 优化后的四元式条数。
    std::size_t folded = 0;        ///< 常量折叠掉的条数。
    std::size_t propagated = 0;    ///< 常量传播替换的操作数个数。
    std::size_t removed = 0;       ///< 死代码消除掉的条数。
    std::size_t merged = 0;        ///< 公共子表达式合并掉的条数。
    std::size_t reduced = 0;       ///< 强度削减掉的条数。
    std::size_t hoisted = 0;       ///< 循环不变量外提的条数。
    std::size_t inlined = 0;       ///< 展开的调用点数。
    std::size_t inlinedQuads = 0;  ///< 展开带来的净增四元式条数。
};

/**
 * @brief 机器级优化开关（`CHALI_NO_MACHINE_OPT=1` 时整层关闭）。
 * @return 开启返回 true。
 */
[[nodiscard]] bool enabled();

/**
 * @brief 对四元式跑一轮机器级优化。
 * @param file 已由语义层化简过的四元式。
 * @param stats 可选统计。
 * @return 优化后的四元式；不改变输入。
 * @note 保留每条四元式的源码位置（新插入的继承插入点位置）。
 */
[[nodiscard]] QuadFile optimize(const QuadFile &file, Stats *stats = nullptr);

/**
 * @brief 最近一次运行的累计统计。
 * @return 统计。
 */
Stats lastStats();

/**
 * @brief 清零累计统计。
 */
void resetStats();

/**
 * @brief 把统计打印成一行文本。
 * @param stats 统计。
 * @return 文本，不含换行。
 */
[[nodiscard]] Memory::String describe(const Stats &stats);
}  // namespace machine
}  // namespace stackvm
