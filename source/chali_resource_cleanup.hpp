#pragma once
#include "chali_borrow_flow.hpp"

/**
 * @brief 根据借用控制流事实合并同一生命周期的资源终结操作。
 */
namespace chali_resource_cleanup
{
/**
 * @brief 一个可合并的显式释放；确定重复时删除，否则保留路径守卫。
 */
struct Merge
{
    int operation = -1;
    bool redundant = false;
};

/**
 * @brief 为可追踪的重复 drop 生成合并方案，不合并移动和普通读写。
 * @param graph 已绑定存储身份的控制流图。
 * @param states 每个节点执行前的生命周期状态。
 * @param reachable 各节点的可达状态。
 * @return 按源码操作编号定位的删除或条件执行方案。
 */
Memory::Vector<Merge> coalesce(const chali_borrow_checker::FlowGraph &graph,
                               const Memory::Vector<chali_borrow_checker::FlowState> &states,
                               const Memory::Vector<bool> &reachable);
}  // namespace chali_resource_cleanup
