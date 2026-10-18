#pragma once
#include "allocator/magel_allocator.hpp"
#include <cstddef>
#include "magel_borrow_flow.hpp"

/**
 * @brief 独立借用检查模块；仅接收类型分析后的事实，不访问语义状态或生成代码。
 */
namespace magel_borrow_checker
{
/**
 * @brief 一项借用操作及其来源和目标的词法作用域深度。
 */
struct Fact
{
    enum Kind
    {
        Store,
        Return,
        ObjectArgument
    } kind = Store;
    std::size_t sourceDepth = 0, targetDepth = 0, line = 0;
    Memory::String context;
};
/**
 * @brief 独立借用检查产生的源码诊断。
 */
struct Diagnostic
{
    Memory::String message;
    std::size_t line = 0;
};
/**
 * @brief 在语义分析结束后统一检查全部借用事实，保留所有独立违规。
 * @param facts 已完成类型和来源分析的借用操作。
 * @return 生命周期违规的诊断列表；不修改输入或语义状态。
 */
inline Memory::Vector<Diagnostic> check(const Memory::Vector<Fact> &facts)
{
    Memory::Vector<Diagnostic> result;
    for (const auto &fact : facts)
    {
        Memory::String message;
        if (fact.kind == Fact::Store && fact.sourceDepth > fact.targetDepth)
            message = "[MGL1204] 保存的引用不能超过来源的生命周期";
        else if (fact.kind == Fact::Return && fact.sourceDepth >= 2u)
            message = "[MGL1204] 返回值中的引用不能超过局部来源的生命周期" + fact.context;
        else if (fact.kind == Fact::ObjectArgument && fact.sourceDepth != 0u)
            message = "[MGL1204] 按值实参中的 Any 不能隐藏非全局引用来源，请使用显式引用类型传参" + fact.context;
        if (!message.empty()) result.push_back({ message, fact.line });
    }
    return result;
}
}  // namespace magel_borrow_checker
