#pragma once
#include "allocator/chali_allocator.hpp"
#include <algorithm>
#include <cstddef>

/**
 * @brief 独立的借用控制流与来源集合分析，不依赖前端类型、符号表或代码生成。
 * 存储路径、投影冲突与初始化数据流参考 rustc 的 move_paths / places_conflict / dataflow；
 * 本实现遵循 ChaLi 的别名契约，源码入口与语义边界见 doc/22-resource-lifetime.md。
 */
namespace chali_borrow_checker
{
/**
 * @brief 存储路径；星号表示经引用槽解引用，其他分段表示字段或下标。
 */
struct Place
{
    Memory::String root;
    Memory::Vector<Memory::String> path;
    /**
     * @brief 对存储路径作稳定字典序比较。
     * @param other 另一条路径。
     * @return 当前路径较小时为 true。
     */
    bool operator<(const Place &other) const { return root < other.root || (root == other.root && path < other.path); }
    /**
     * @brief 判断存储路径是否相同。
     * @param other 另一条路径。
     * @return 完全相同时为 true。
     */
    bool operator==(const Place &other) const { return root == other.root && path == other.path; }
};
/**
 * @brief 控制流上一项借用操作；后继使用图节点下标。
 */
struct FlowNode
{
    enum Kind
    {
        Nop,
        Read,
        Write,
        Initialize,
        Bind,
        Allocate,
        Destroy,
        Drop,
        Move,
        MoveScalar,
        Redeclare,
        CaptureBorrow,
        CopyCaptures,
        InheritCaptures,
        CheckCaptures
    } kind = Nop;
    Place target, source;
    Memory::Vector<std::size_t> next;
    std::size_t line = 0;
    Memory::String label;
    int operation = -1;  ///< 对应的源码操作编号，供资源合并阶段定位。
};
/**
 * @brief 一次函数调用的静态控制流，入口为零号节点。
 */
struct FlowGraph
{ Memory::Vector<FlowNode> nodes; };
/**
 * @brief 控制流检查产生的独立诊断。
 */
struct FlowDiagnostic
{
    Memory::String message;
    std::size_t line = 0;
};
/**
 * @brief 判断存储路径是否重叠，动态下标保守覆盖所有下标。
 * @param a 路径。
 * @param b 路径。
 * @return 可能重叠时为 true。
 */
inline bool overlaps(const Place &a, const Place &b)
{
    if (a.root.empty() || a.root != b.root) return false;
    for (std::size_t i = 0; i < std::min(a.path.size(), b.path.size()); ++i)
        if (a.path[i] != b.path[i] && a.path[i] != "[*]" && b.path[i] != "[*]" && a.path[i] != "$any" && b.path[i] != "$any") return false;
    return true;
}
/**
 * @brief 一条控制流路径上可能的引用来源及已失效存储。
 */
struct FlowState
{
    /**
     * @brief 区分可重新初始化的析构值、禁止读写的移动绑定及已释放存储。
     */
    enum InvalidKind
    {
        Dropped,
        Moved,
        Freed
    };
    Memory::Map<Place, Memory::Set<Place>> origins;
    Memory::Map<Place, Memory::Set<Place>> captures;  ///< 闭包及聚合字段持有的借用来源。
    Memory::Set<Place> definitelyMoved;               ///< 所有到达路径均已完整移动的存储。
    Memory::Map<Place, InvalidKind> invalid;
    Memory::Map<Place, InvalidKind> terminated;  ///< 所有到达路径上均已终结的精确存储。
};
/**
 * @brief 沿引用槽解析所有可能的来源，分支合并后保留来源并集。
 * @param state 当前状态。
 * @param place 未解析路径。
 * @return 具体存储集合。
 */
inline Memory::Set<Place> resolve(const FlowState &state, const Place &place)
{
    if (place.root.empty()) return {};
    Memory::Set<Place> current{ { place.root, {} } };
    for (const auto &part : place.path)
    {
        Memory::Set<Place> next;
        for (auto item : current)
        {
            if (part == "*")
            {
                const auto found = state.origins.find(item);
                if (found != state.origins.end())
                    next.insert(found->second.begin(), found->second.end());
                else if (item.path.empty())
                {
                    item.path.push_back("$external");
                    next.insert(std::move(item));
                }
            }
            else
            {
                item.path.push_back(part);
                next.insert(std::move(item));
            }
            // 循环中不断追踪 next 等递归字段时做路径加宽，保证来源域有限。
            if (next.size() > 0u)
            {
                Memory::Set<Place> bounded;
                for (auto value : next)
                {
                    if (value.path.size() > 16u)
                    {
                        value.path.resize(16u);
                        value.path.back() = "$any";
                    }
                    bounded.insert(std::move(value));
                }
                next = std::move(bounded);
            }
        }
        current = std::move(next);
    }
    return current;
}
/**
 * @brief 对后继节点合并可能来源的并集与确定移动的交集。
 * @param target 后继输入。
 * @param source 前驱输出。
 * @return 状态变化时为 true。
 */
inline bool merge(FlowState &target, const FlowState &source)
{
    bool changed = false;
    for (auto it = target.definitelyMoved.begin(); it != target.definitelyMoved.end();)
    {
        if (!source.definitelyMoved.count(*it))
        {
            it = target.definitelyMoved.erase(it);
            changed = true;
        }
        else
            ++it;
    }
    for (auto it = target.terminated.begin(); it != target.terminated.end();)
    {
        const auto found = source.terminated.find(it->first);
        if (found == source.terminated.end() || found->second != it->second)
        {
            it = target.terminated.erase(it);
            changed = true;
        }
        else
            ++it;
    }
    for (const auto &entry : source.captures)
    {
        auto &values = target.captures[entry.first];
        const auto size = values.size();
        values.insert(entry.second.begin(), entry.second.end());
        changed = changed || size != values.size();
    }
    for (const auto &entry : source.origins)
    {
        auto &values = target.origins[entry.first];
        const auto size = values.size();
        values.insert(entry.second.begin(), entry.second.end());
        changed = changed || size != values.size();
    }
    for (const auto &entry : source.invalid)
    {
        const auto found = target.invalid.find(entry.first);
        if (found == target.invalid.end())
        {
            target.invalid.insert(entry);
            changed = true;
        }
        else if (entry.second > found->second)
        {
            found->second = entry.second;
            changed = true;
        }
    }
    return changed;
}
/**
 * @brief 以工作队列迭代到控制流不动点，再检查每个可达节点，避免迭代过程中的临时误报。
 * @param graph 已绑定存储身份的函数控制流图。
 * @param entryStates 可选输出：每个操作之前的来源与生命周期状态。
 * @param reachable 可选输出：各节点是否可达。
 * @return 全部释放后访问、移动后访问及非法重新定义诊断；不修改输入。
 */
inline Memory::Vector<FlowDiagnostic> checkFlow(const FlowGraph &graph, Memory::Vector<FlowState> *entryStates = nullptr, Memory::Vector<bool> *reachable = nullptr)
{
    const auto count = graph.nodes.size();
    if (!count) return {};
    Memory::Vector<FlowState> states(count);
    Memory::Vector<bool> reached(count, false), queued(count, false);
    Memory::Deque<std::size_t> work{ 0 };
    reached[0] = queued[0] = true;
    const auto transfer = [](const FlowNode &node, FlowState state) {
        const auto targets = resolve(state, node.target);
        if (node.kind == FlowNode::Allocate) state.terminated.clear();
        if (node.kind == FlowNode::Initialize || node.kind == FlowNode::Write || node.kind == FlowNode::Bind || node.kind == FlowNode::Move || node.kind == FlowNode::MoveScalar)
            for (const auto &target : targets)
                for (auto it = state.terminated.begin(); it != state.terminated.end();)
                    if (overlaps(target, it->first))
                        it = state.terminated.erase(it);
                    else
                        ++it;
        if ((node.kind == FlowNode::Drop || node.kind == FlowNode::Destroy) && targets.size() == 1u)
            state.terminated[*targets.begin()] = std::max(state.terminated[*targets.begin()], node.kind == FlowNode::Destroy ? FlowState::Freed : FlowState::Dropped);
        if ((node.kind == FlowNode::Move || node.kind == FlowNode::MoveScalar) && targets.size() == 1u)
            state.definitelyMoved.insert(*targets.begin());
        else if (node.kind == FlowNode::Initialize || node.kind == FlowNode::Drop || node.kind == FlowNode::Destroy)
        {
            for (auto it = state.definitelyMoved.begin(); it != state.definitelyMoved.end();)
            {
                bool affected = false;
                for (const auto &target : targets) affected = affected || overlaps(target, *it);
                if (affected)
                    it = state.definitelyMoved.erase(it);
                else
                    ++it;
            }
        }
        if (node.kind == FlowNode::CaptureBorrow)
        {
            const auto sources = resolve(state, node.source);
            for (const auto &target : targets)
                state.captures[target].insert(sources.begin(), sources.end());
        }
        else if (node.kind == FlowNode::InheritCaptures)
        {
            const auto sources = resolve(state, node.source);
            Memory::Set<Place> borrowed;
            for (const auto &source : sources)
                for (const auto &entry : state.captures)
                    if (overlaps(source, entry.first)) borrowed.insert(entry.second.begin(), entry.second.end());
            for (const auto &target : targets)
                state.captures[target].insert(borrowed.begin(), borrowed.end());
        }
        else if (node.kind == FlowNode::CopyCaptures)
        {
            // 先快照来源再覆盖目标，支持自赋值及同一聚合内的字段复制。
            const auto sources = resolve(state, node.source);
            Memory::Map<Place, Memory::Set<Place>> copied;
            for (const auto &target : targets)
                for (const auto &source : sources)
                    for (const auto &entry : state.captures)
                        if (source.root == entry.first.root && source.path.size() <= entry.first.path.size() && std::equal(source.path.begin(), source.path.end(), entry.first.path.begin()))
                        {
                            auto slot = target;
                            slot.path.insert(slot.path.end(), entry.first.path.begin() + source.path.size(), entry.first.path.end());
                            copied[slot].insert(entry.second.begin(), entry.second.end());
                        }
            for (auto it = state.captures.begin(); it != state.captures.end();)
            {
                bool covered = false;
                for (const auto &target : targets)
                    covered = covered || (target.root == it->first.root && target.path.size() <= it->first.path.size() && std::equal(target.path.begin(), target.path.end(), it->first.path.begin()));
                if (covered && targets.size() == 1u)
                    it = state.captures.erase(it);
                else
                    ++it;
            }
            for (const auto &entry : copied)
                state.captures[entry.first].insert(entry.second.begin(), entry.second.end());
        }
        else if (node.kind == FlowNode::Allocate)
        {
            // 分配点采用最近一次 / 更早实例两个抽象身份；重新分配不能复活旧别名。
            const auto fresh = node.source.root;
            const auto older = fresh + "$older";
            FlowState renamed;
            renamed.definitelyMoved = state.definitelyMoved;
            const auto age = [&](Place place) { if (place.root == fresh) place.root = older; return place; };
            for (const auto &entry : state.origins)
            {
                auto &values = renamed.origins[age(entry.first)];
                for (const auto &value : entry.second) values.insert(age(value));
            }
            for (const auto &entry : state.captures)
            {
                auto &values = renamed.captures[age(entry.first)];
                for (const auto &value : entry.second) values.insert(age(value));
            }
            for (const auto &entry : state.invalid)
            {
                auto &freed = renamed.invalid[age(entry.first)];
                freed = std::max(freed, entry.second);
            }
            state = std::move(renamed);
            for (const auto &target : targets) state.origins[target] = { node.source };
        }
        else if (node.kind == FlowNode::Bind)
        {
            // 重绑只更新引用来源；不能复活已移动的旧绑定。
            if (targets.size() == 1u)
            {
                const auto found = state.invalid.find(*targets.begin());
                if (found != state.invalid.end() && found->second == FlowState::Dropped)
                    state.invalid.erase(found);
            }
            const auto sources = resolve(state, node.source);
            for (const auto &target : targets)
            {
                if (targets.size() == 1u)
                    state.origins[target] = sources;
                else
                    state.origins[target].insert(sources.begin(), sources.end());
            }
        }
        else if (node.kind == FlowNode::Destroy || node.kind == FlowNode::Drop || node.kind == FlowNode::Move || node.kind == FlowNode::MoveScalar)
        {
            for (const auto &target : targets)
                state.invalid[target] = std::max(state.invalid[target], node.kind == FlowNode::Destroy ? FlowState::Freed : node.kind == FlowNode::Drop ? FlowState::Dropped :
                                                                                                                                                          FlowState::Moved);
        }
        else if ((node.kind == FlowNode::Initialize || node.kind == FlowNode::Write) && targets.size() == 1u)
        {
            const auto &target = *targets.begin();
            for (auto it = state.invalid.begin(); it != state.invalid.end();)
            {
                const bool covers = target.root == it->first.root && target.path.size() <= it->first.path.size() && std::equal(target.path.begin(), target.path.end(), it->first.path.begin());
                if (covers && (node.kind == FlowNode::Initialize || it->second == FlowState::Dropped))
                    it = state.invalid.erase(it);
                else
                    ++it;
            }
        }
        return state;
    };
    while (!work.empty())
    {
        const auto index = work.front();
        work.pop_front();
        queued[index] = false;
        const auto output = transfer(graph.nodes[index], states[index]);
        for (const auto next : graph.nodes[index].next)
        {
            if (next >= count) continue;
            const bool changed = !reached[next] || merge(states[next], output);
            if (!reached[next]) states[next] = output;
            if ((!reached[next] || changed) && !queued[next])
            {
                work.push_back(next);
                queued[next] = true;
            }
            reached[next] = true;
        }
    }
    Memory::Vector<FlowDiagnostic> diagnostics;
    for (std::size_t index = 0; index < count; ++index)
    {
        if (!reached[index]) continue;
        const auto &node = graph.nodes[index];
        if (node.kind == FlowNode::Redeclare)
        {
            if (!states[index].definitelyMoved.count(node.target)) diagnostics.push_back({ "同一个块里不能重复声明：" + node.label + "（旧变量必须在所有到达路径上完整移动后才能重新定义）", node.line });
            continue;
        }
        if (node.kind != FlowNode::CheckCaptures && node.kind != FlowNode::Read && node.kind != FlowNode::Write && node.kind != FlowNode::Bind && node.kind != FlowNode::Allocate && node.kind != FlowNode::Move && node.kind != FlowNode::MoveScalar && node.kind != FlowNode::Destroy && node.kind != FlowNode::Drop) continue;
        bool freed = false, moved = false;
        auto accessed = resolve(states[index], node.target);
        if (node.kind == FlowNode::Read || node.kind == FlowNode::CheckCaptures)
        {
            Memory::Set<Place> borrowed;
            for (const auto &target : accessed)
                for (const auto &capture : states[index].captures)
                    if (overlaps(target, capture.first)) borrowed.insert(capture.second.begin(), capture.second.end());
            accessed.insert(borrowed.begin(), borrowed.end());
        }
        for (const auto &target : accessed)
            for (const auto &invalid : states[index].invalid)
                if (overlaps(target, invalid.first))
                {
                    freed = freed || invalid.second == FlowState::Freed;
                    const bool writing = node.kind == FlowNode::Write || node.kind == FlowNode::Bind || node.kind == FlowNode::Allocate;
                    moved = moved || (node.kind != FlowNode::Destroy && invalid.second == FlowState::Moved) || (!writing && node.kind != FlowNode::Destroy && node.kind != FlowNode::Drop && invalid.second == FlowState::Dropped);
                }
        if (((node.kind != FlowNode::Destroy && node.kind != FlowNode::Drop) || moved) && (freed || moved)) diagnostics.push_back({ freed ? "[CHL1205] 借用来源已释放，不能继续访问：" + node.label : "[CHL1205] 借用来源已移动或销毁，不能继续访问（源对象已经销毁）：" + node.label, node.line });
    }
    if (entryStates) *entryStates = std::move(states);
    if (reachable) *reachable = std::move(reached);
    return diagnostics;
}
}  // namespace chali_borrow_checker
