/**
 * @file quad_optimizer.hpp
 * @brief 四元式优化接口与统计。
 */
#pragma once
#include "quad_model.hpp"

namespace stackvm
{
/**
 * @brief 化简统计。
 */
struct OptimizationReport
{
    std::size_t rounds = 0;                    ///< 实际迭代了几轮。
    std::size_t constantsFolded = 0;           ///< 常量折叠了几处。
    std::size_t algebraicSimplified = 0;       ///< 保留类型语义的整数恒等式化简处数。
    std::size_t deadBranches = 0;              ///< 常量条件的分支折掉了几处。
    std::size_t deadCode = 0;                  ///< 删掉了几条不可达 / 无用四元式。
    std::size_t copiesPropagated = 0;          ///< 基本块内传播或合并的复制。
    std::size_t conversionsRemoved = 0;        ///< 删除的同类型转换。
    std::size_t crossBlockConstants = 0;       ///< 控制流分析传播的常量操作数，包含循环回边。
    std::size_t commonExpressionsRemoved = 0;  ///< 删除的局部重复表达式。
    Memory::Vector<Memory::String> trace;      ///< 常量、代数与控制流化简说明。

    /**
     * @brief 一共改了多少处。
     * @return 处数。
     */
    [[nodiscard]] std::size_t total() const
    {
        return constantsFolded + algebraicSimplified + deadBranches + deadCode + copiesPropagated + conversionsRemoved + crossBlockConstants + commonExpressionsRemoved;
    }
};

/**
 * @brief 化简四元式（**就地改写** `file.quads`）。
 *
 * 在基本块内传播常量与同类型复制、折叠常量运算、消除同类型转换，
 * 合并只使用一次的临时结果与紧随其后的复制，化简保留类型语义的整数恒等式；
 * 保留动态算术的实际数值类型、窄类型存储和可能失败的读取。
 * 折叠常量条件与跳转链，删除不可达代码和安全的无用临时复制。
 * 控制流分析在合流点与循环回边求整数常量不动点，并消除块内重复表达式。
 * 调用、内存操作及未知副作用构成传播边界，不改变声明元数据及隐式输出格式。
 *
 * @param file 四元式文件。
 * @param maxRounds 最多迭代几轮（缺省 4）。
 * @param collectTrace 是否收集逐项优化说明，默认关闭以减少字符串分配。
 * @return 化简统计。
 */
[[nodiscard]] OptimizationReport optimize(QuadFile &file, std::size_t maxRounds = 4u, bool collectTrace = false);

// ---------------------------------------------------------------------------
//  执行
// ---------------------------------------------------------------------------
}  // namespace stackvm
