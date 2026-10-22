#include "std_tree_storage.hpp"
#include "std_native_record.hpp"
#include "../chali_glue.hpp"
#include <algorithm>
#include <functional>
#include <memory>
#include <stdexcept>
#include <vector>

namespace std_tree_storage_glue
{
namespace
{
using namespace native_record_glue;
using Ints = Memory::Vector<std::int64_t>;
/**
 * @brief 保存二叉孩子或首孩子、次兄弟的节点。
 */
struct Node
{
    std::int64_t value, left = -1, right = -1, last = -1;
    bool live = true;
};
/**
 * @brief 独立拥有节点或堆数组的持久容器。
 */
struct Tree
{
    std::vector<Node> nodes;
    std::vector<std::int64_t> free, heap;
    std::int64_t root = -1, count = 0, kind = 0;
    /**
     * @brief 分配或复用节点槽。
     * @param value 新节点值。
     * @return 新节点槽号。
     */
    std::int64_t allocate(std::int64_t value)
    {
        auto index = static_cast<std::int64_t>(nodes.size());
        if (free.empty())
            nodes.push_back(Node{ value });
        else
        {
            index = free.back();
            free.pop_back();
            nodes[index] = Node{ value };
        }
        ++count;
        return index;
    }
    /**
     * @brief 查询二叉搜索树节点，不复制节点数组。
     * @param key 待查找的键。
     * @return 匹配节点槽号，不存在时为负一。
     */
    std::int64_t find(std::int64_t key) const
    {
        auto x = root;
        while (x >= 0 && nodes[x].value != key) x = key < nodes[x].value ? nodes[x].left : nodes[x].right;
        return x;
    }
    /**
     * @brief 原地插入二叉搜索树键。
     * @param key 待插入的键。
     * @return 新增键时返回 true，重复键时返回 false。
     */
    bool insert(std::int64_t key)
    {
        if (root < 0)
        {
            root = allocate(key);
            return true;
        }
        auto x = root;
        while (true)
        {
            if (nodes[x].value == key) return false;
            bool left = key < nodes[x].value;
            auto next = left ? nodes[x].left : nodes[x].right;
            if (next >= 0)
            {
                x = next;
                continue;
            }
            auto index = allocate(key);
            if (left)
                nodes[x].left = index;
            else
                nodes[x].right = index;
            return true;
        }
    }
    /**
     * @brief 原地删除键，双孩子节点使用中序后继替换。
     * @param key 要删除的键。
     * @return 存在且删除成功时返回 true。
     */
    bool erase(std::int64_t key)
    {
        std::int64_t parent = -1, x = root;
        while (x >= 0 && nodes[x].value != key)
        {
            parent = x;
            x = key < nodes[x].value ? nodes[x].left : nodes[x].right;
        }
        if (x < 0) return false;
        if (nodes[x].left >= 0 && nodes[x].right >= 0)
        {
            auto target = x;
            parent = x;
            x = nodes[x].right;
            while (nodes[x].left >= 0)
            {
                parent = x;
                x = nodes[x].left;
            }
            nodes[target].value = nodes[x].value;
        }
        auto child = nodes[x].left >= 0 ? nodes[x].left : nodes[x].right;
        if (parent < 0)
            root = child;
        else if (nodes[parent].left == x)
            nodes[parent].left = child;
        else
            nodes[parent].right = child;
        nodes[x].live = false;
        free.push_back(x);
        --count;
        return true;
    }
};
/**
 * @brief 深复制容器的节点、堆和回收槽。
 * @param source 原容器。
 * @param error 本操作不产生可恢复诊断。
 * @return 独立状态副本。
 */
void *cloneTree(const void *source, Memory::String &error)
{
    (void)error;
    return new Tree(*static_cast<const Tree *>(source));
}
/**
 * @brief 释放容器及其所有数组。
 * @param value 待释放的容器。
 */
void destroyTree(void *value) { delete static_cast<Tree *>(value); }
const stackvm::NativeRecordType treeType{ "std::tree::TreeStorage", cloneTree, destroyTree, nullptr };
/**
 * @brief 将容器所有权交给 VM。
 * @param ctx 当前调用上下文。
 * @param value 待接管的容器。
 * @return 原生记录句柄。
 */
std::int64_t own(chali_glue::Context &ctx, std::unique_ptr<Tree> value)
{
    auto handle = ctx.makeNativeRecord(treeType, value.get());
    if (handle) value.release();
    return handle;
}
/**
 * @brief 创建指定种类的空容器。
 * @param ctx 当前调用上下文。
 * @param kind 零为二叉搜索树、一为通用树、二为最小堆。
 * @return 拥有空容器的句柄。
 */
std::int64_t create(chali_glue::Context &ctx, std::int64_t kind)
{
    auto tree = std::make_unique<Tree>();
    tree->kind = kind;
    return own(ctx, std::move(tree));
}
/**
 * @brief 执行原地修改或标量查询，不导出内部数组。
 * @param state 当前调用状态。
 * @param arguments 容器句柄、操作编号和两个整数参数。
 * @return 操作对应的布尔值、节点号、键、数量或高度。
 */
Value operate(State &state, const Memory::Vector<Value> &arguments)
{
    auto *tree = record<Tree>(state, arguments[0], treeType, "树或堆");
    if (!tree) return Value::fromInt(0);
    auto op = arguments[1].asInt(), value = arguments[2].asInt(), extra = arguments[3].asInt();
    switch (op)
    {
    case 0: return Value::fromInt(tree->kind == 2 ? tree->heap.size() : tree->count);
    case 1:
    {
        auto kind = tree->kind;
        *tree = Tree{};
        tree->kind = kind;
        return Value::fromInt(0);
    }
    case 2: return Value::fromInt(tree->insert(value));
    case 3: return Value::fromInt(tree->erase(value));
    case 4: return Value::fromInt(tree->find(value) >= 0);
    case 5:
    case 6:
    {
        auto x = tree->root;
        if (x < 0) throw std::out_of_range("空树没有最小或最大键");
        while ((op == 5 ? tree->nodes[x].left : tree->nodes[x].right) >= 0) x = op == 5 ? tree->nodes[x].left : tree->nodes[x].right;
        return Value::fromInt(tree->nodes[x].value);
    }
    case 7:
    {
        std::int64_t best = 0;
        std::vector<std::pair<std::int64_t, std::int64_t>> stack;
        if (tree->root >= 0) stack.emplace_back(tree->root, 1);
        while (!stack.empty())
        {
            auto [x, depth] = stack.back();
            stack.pop_back();
            best = std::max(best, depth);
            if (tree->nodes[x].left >= 0) stack.emplace_back(tree->nodes[x].left, depth + 1);
            if (tree->nodes[x].right >= 0) stack.emplace_back(tree->nodes[x].right, depth + (tree->kind == 1 ? 0 : 1));
        }
        return Value::fromInt(best);
    }
    case 8:
        tree->heap.push_back(value);
        std::push_heap(tree->heap.begin(), tree->heap.end(), std::greater<std::int64_t>{});
        return Value::fromInt(0);
    case 9:
    case 10:
    {
        if (tree->heap.empty()) throw std::out_of_range("空堆没有最小值");
        auto result = tree->heap.front();
        if (op == 9)
        {
            std::pop_heap(tree->heap.begin(), tree->heap.end(), std::greater<std::int64_t>{});
            tree->heap.pop_back();
        }
        return Value::fromInt(result);
    }
    case 11:
    {
        if (tree->count) return Value::fromInt(-1);
        tree->root = tree->allocate(value);
        return Value::fromInt(tree->root);
    }
    case 12:
    {
        if (value < 0 || static_cast<std::uint64_t>(value) >= tree->nodes.size() || !tree->nodes[value].live) return Value::fromInt(-1);
        auto index = tree->allocate(extra);
        if (tree->nodes[value].left < 0)
            tree->nodes[value].left = index;
        else
            tree->nodes[tree->nodes[value].last].right = index;
        tree->nodes[value].last = index;
        return Value::fromInt(index);
    }
    case 13:
        if (value < 0 || static_cast<std::uint64_t>(value) >= tree->nodes.size() || !tree->nodes[value].live) throw std::out_of_range("树节点下标越界");
        return Value::fromInt(tree->nodes[value].value);
    case 14: return Value::fromInt(tree->root);
    default: throw std::invalid_argument("树操作编号无效");
    }
}
/**
 * @brief 生成遍历结果或显式结构快照。
 * @param state 当前调用状态。
 * @param arguments 容器句柄、模式和起始节点。
 * @return 独立整数列表。
 */
Value values(State &state, const Memory::Vector<Value> &arguments)
{
    const auto *tree = record<Tree>(state, arguments[0], treeType, "树快照");
    if (!tree) return Value::fromInt(0);
    auto mode = arguments[1].asInt(), start = arguments[2].asInt();
    Ints result;
    if (mode >= 10)
    {
        if (mode == 14)
            result.assign(tree->heap.begin(), tree->heap.end());
        else
            for (const auto &node : tree->nodes) result.push_back(mode == 10 ? node.value : mode == 11 ? node.left :
                                                                                        mode == 12     ? node.right :
                                                                                                         node.live);
    }
    else if (mode == 5)
    {
        if (start >= 0 && static_cast<std::uint64_t>(start) < tree->nodes.size() && tree->nodes[start].live)
            for (auto x = tree->nodes[start].left; x >= 0; x = tree->nodes[x].right) result.push_back(x);
    }
    else if (mode == 4)
    {
        std::vector<std::int64_t> queue;
        if (tree->root >= 0) queue.push_back(tree->root);
        for (std::size_t at = 0; at < queue.size(); ++at)
        {
            auto x = queue[at];
            result.push_back(x);
            for (auto child = tree->nodes[x].left; child >= 0; child = tree->nodes[child].right) queue.push_back(child);
        }
    }
    else
    {
        std::vector<std::pair<std::int64_t, bool>> stack;
        if (tree->root >= 0) stack.emplace_back(tree->root, false);
        while (!stack.empty())
        {
            auto [x, emit] = stack.back();
            stack.pop_back();
            if (emit)
            {
                result.push_back(tree->kind == 1 ? x : tree->nodes[x].value);
                continue;
            }
            const auto &node = tree->nodes[x];
            if (mode == 2) stack.emplace_back(x, true);
            if (node.right >= 0) stack.emplace_back(node.right, false);
            if (mode == 0) stack.emplace_back(x, true);
            if (node.left >= 0) stack.emplace_back(node.left, false);
            if (mode == 1 || mode == 3) stack.emplace_back(x, true);
        }
    }
    return chali_glue::Codec<Ints>::encode(state.host, result);
}
/**
 * @brief 独立复制二叉和首孩子次兄弟表示，重建通用树的末孩子索引。
 * @param ctx 当前调用上下文。
 * @param handle 源树句柄。
 * @param kind 目标树种类。
 * @return 拥有独立转换结果的句柄。
 */
std::int64_t convert(chali_glue::Context &ctx, std::int64_t handle, std::int64_t kind)
{
    const auto *source = static_cast<const Tree *>(ctx.nativeRecord(handle, treeType));
    if (!source) return 0;
    auto tree = std::make_unique<Tree>(*source);
    tree->kind = kind;
    if (kind == 1)
        for (auto &node : tree->nodes)
        {
            node.last = node.left;
            while (node.last >= 0 && tree->nodes[node.last].right >= 0) node.last = tree->nodes[node.last].right;
        }
    return own(ctx, std::move(tree));
}
}  // namespace
/**
 * @brief 注册持久树和堆的生命周期、原地操作与显式快照。
 * @param registry 当前宿主注册表。
 * @param error 接收绑定失败的诊断。
 * @return 所有入口注册成功时返回 true。
 */
bool bind(chali_glue::Registry &registry, Memory::String *error)
{
    auto &native = registry.native();
    return registry.bindTypedWithContext<std::int64_t>("std::tree::__bstNew", [](chali_glue::Context &ctx) { return create(ctx, 0); }, error) && registry.bindTypedWithContext<std::int64_t>("std::tree::__generalNew", [](chali_glue::Context &ctx) { return create(ctx, 1); }, error) && registry.bindTypedWithContext<std::int64_t>("std::tree::__heapNew", [](chali_glue::Context &ctx) { return create(ctx, 2); }, error) && native_record_glue::bind(native, "std::tree::__treeCopy", 1, copy, error) && native_record_glue::bind(native, "std::tree::__treeDrop", 1, drop, error) && native_record_glue::bind(native, "std::tree::__treeOp", 4, operate, error) && native_record_glue::bind(native, "std::tree::__treeValues", 3, values, error) && registry.bindTypedWithContext<std::int64_t, std::int64_t>("std::tree::__treeAsBinary", [](chali_glue::Context &ctx, std::int64_t handle) { return convert(ctx, handle, 0); }, error) && registry.bindTypedWithContext<std::int64_t, std::int64_t>("std::tree::__treeAsGeneral", [](chali_glue::Context &ctx, std::int64_t handle) { return convert(ctx, handle, 1); }, error);
}
}  // namespace std_tree_storage_glue
