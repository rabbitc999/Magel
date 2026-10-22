#include "std_red_black_tree.hpp"
#include "std_native_record.hpp"
#include "../chali_glue.hpp"
#include <algorithm>
#include <memory>
#include <stdexcept>
#include <vector>

namespace std_red_black_tree_glue
{
namespace
{
using namespace native_record_glue;
using Index = std::size_t;
/**
 * @brief 保存父子关系和颜色的节点；下标零是共享黑色空叶。
 */
struct Node
{
    std::int64_t key = 0;
    Index parent = 0, child[2] = { 0, 0 };
    bool red = false;
};
/**
 * @brief 独立拥有节点数组和回收槽的红黑集合。
 */
struct Tree
{
    std::vector<Node> nodes{ Node{} };
    std::vector<Index> free;
    Index root = 0, count = 0;

    /**
     * @brief 沿有序父子关系查找指定键。
     * @param key 待查找的键。
     * @return 节点下标；不存在时返回零。
     */
    Index find(std::int64_t key) const
    {
        auto x = root;
        while (x && nodes[x].key != key) x = nodes[x].child[key > nodes[x].key];
        return x;
    }
    /**
     * @brief 旋转子树并原地维护父节点和根。
     * @param x 旋转支点。
     * @param direction 提升的孩子方向，零为右旋、一为左旋。
     */
    void rotate(Index x, unsigned direction)
    {
        auto y = nodes[x].child[direction];
        auto middle = nodes[y].child[1 - direction];
        nodes[x].child[direction] = middle;
        if (middle) nodes[middle].parent = x;
        auto p = nodes[x].parent;
        nodes[y].parent = p;
        if (!p)
            root = y;
        else
            nodes[p].child[nodes[p].child[1] == x] = y;
        nodes[y].child[1 - direction] = x;
        nodes[x].parent = y;
    }
    /**
     * @brief 插入新键并通过变色和旋转消除连续红节点。
     * @param key 要插入的键。
     * @return 键原先不存在且插入成功时返回 true。
     */
    bool insert(std::int64_t key)
    {
        Index p = 0, x = root;
        while (x)
        {
            if (nodes[x].key == key) return false;
            p = x;
            x = nodes[x].child[key > nodes[x].key];
        }
        if (free.empty())
        {
            x = nodes.size();
            nodes.push_back(Node{});
        }
        else
        {
            x = free.back();
            free.pop_back();
            nodes[x] = Node{};
        }
        nodes[x].key = key;
        nodes[x].parent = p;
        nodes[x].red = true;
        if (!p)
            root = x;
        else
            nodes[p].child[key > nodes[p].key] = x;
        ++count;
        while (nodes[nodes[x].parent].red)
        {
            p = nodes[x].parent;
            auto g = nodes[p].parent;
            unsigned side = nodes[g].child[1] == p;
            auto uncle = nodes[g].child[1 - side];
            if (nodes[uncle].red)
            {
                nodes[p].red = nodes[uncle].red = false;
                nodes[g].red = true;
                x = g;
            }
            else
            {
                if (nodes[p].child[1 - side] == x)
                {
                    x = p;
                    rotate(x, 1 - side);
                    p = nodes[x].parent;
                }
                nodes[p].red = false;
                nodes[g].red = true;
                rotate(g, side);
            }
        }
        nodes[root].red = false;
        return true;
    }
    /**
     * @brief 修复删除黑节点后产生的黑高亏缺，空叶的父节点由删除入口保存。
     * @param x 替代被删除节点的孩子，可以是空叶。
     */
    void repairErase(Index x)
    {
        while (x != root && !nodes[x].red)
        {
            auto p = nodes[x].parent;
            unsigned side = nodes[p].child[1] == x;
            auto sibling = nodes[p].child[1 - side];
            if (nodes[sibling].red)
            {
                nodes[sibling].red = false;
                nodes[p].red = true;
                rotate(p, 1 - side);
                sibling = nodes[p].child[1 - side];
            }
            auto nearChild = nodes[sibling].child[side];
            auto farChild = nodes[sibling].child[1 - side];
            if (!nodes[nearChild].red && !nodes[farChild].red)
            {
                if (sibling) nodes[sibling].red = true;
                x = p;
            }
            else
            {
                if (!nodes[farChild].red)
                {
                    nodes[nearChild].red = false;
                    nodes[sibling].red = true;
                    rotate(sibling, side);
                    sibling = nodes[p].child[1 - side];
                    farChild = nodes[sibling].child[1 - side];
                }
                nodes[sibling].red = nodes[p].red;
                nodes[p].red = false;
                nodes[farChild].red = false;
                rotate(p, 1 - side);
                x = root;
            }
        }
        nodes[x].red = false;
        nodes[0].parent = 0;
    }
    /**
     * @brief 删除键并原地修复红黑性质，回收脱离树的节点槽。
     * @param key 要删除的键。
     * @return 键存在且已删除时返回 true。
     */
    bool erase(std::int64_t key)
    {
        auto z = find(key);
        if (!z) return false;
        auto y = z;
        if (nodes[z].child[0] && nodes[z].child[1])
        {
            y = nodes[z].child[1];
            while (nodes[y].child[0]) y = nodes[y].child[0];
            nodes[z].key = nodes[y].key;
        }
        auto x = nodes[y].child[nodes[y].child[0] == 0];
        auto p = nodes[y].parent;
        nodes[x].parent = p;
        if (!p)
            root = x;
        else
            nodes[p].child[nodes[p].child[1] == y] = x;
        if (!nodes[y].red) repairErase(x);
        nodes[0].parent = 0;
        nodes[y] = Node{};
        free.push_back(y);
        --count;
        return true;
    }
    /**
     * @brief 检查父子关系、严格排序、红色约束和每条路径黑高。
     * @param x 当前子树根。
     * @param parent 期望的父节点。
     * @param lower 严格下界节点，零表示无下界。
     * @param upper 严格上界节点，零表示无上界。
     * @param visited 累积已访问节点数，并限制损坏结构的遍历。
     * @return 合法子树的黑高；违反约束时返回负一。
     */
    int check(Index x, Index parent, Index lower, Index upper, Index &visited) const
    {
        if (!x) return 1;
        if (x >= nodes.size() || ++visited > count) return -1;
        const auto &n = nodes[x];
        if (n.parent != parent || (lower && n.key <= nodes[lower].key) || (upper && n.key >= nodes[upper].key)) return -1;
        if (n.child[0] >= nodes.size() || n.child[1] >= nodes.size()) return -1;
        if (n.red && (nodes[n.child[0]].red || nodes[n.child[1]].red)) return -1;
        auto left = check(n.child[0], x, lower, x, visited);
        auto right = check(n.child[1], x, x, upper, visited);
        return left < 0 || right != left ? -1 : left + !n.red;
    }
    /**
     * @brief 计算子树高度，空树高度为零。
     * @param x 子树根下标。
     * @return 从根到叶的最大节点数。
     */
    std::int64_t height(Index x) const
    {
        return x ? 1 + std::max(height(nodes[x].child[0]), height(nodes[x].child[1])) : 0;
    }
};
/**
 * @brief 深复制节点和回收槽，副本的后续操作互不影响。
 * @param source 原树状态。
 * @param error 本操作不产生可恢复诊断。
 * @return 新分配的独立树。
 */
void *cloneTree(const void *source, Memory::String &error)
{
    (void)error;
    return new Tree(*static_cast<const Tree *>(source));
}
/**
 * @brief 释放树及其全部节点。
 * @param value 待释放的树状态。
 */
void destroyTree(void *value) { delete static_cast<Tree *>(value); }
const stackvm::NativeRecordType treeType{ "std::tree::RedBlackTree", cloneTree, destroyTree, nullptr };
/**
 * @brief 创建空红黑树并把所有权交给 VM。
 * @param state 当前调用状态。
 * @param arguments 无实参。
 * @return 拥有树状态的原生记录句柄。
 */
Value create(State &state, const Memory::Vector<Value> &arguments)
{
    (void)arguments;
    auto value = std::make_unique<Tree>();
    auto handle = state.host.makeNativeRecord(treeType, value.get());
    if (handle) value.release();
    return Value::fromInt(handle);
}
/**
 * @brief 执行不导出内部数组的原地修改或标量查询。
 * @param state 当前调用状态。
 * @param arguments 树句柄、操作编号和键。
 * @return 操作对应的布尔值、键、数量或高度。
 */
Value operate(State &state, const Memory::Vector<Value> &arguments)
{
    auto *tree = record<Tree>(state, arguments[0], treeType, "红黑树");
    if (!tree) return Value::fromInt(0);
    const auto key = arguments[2].asInt();
    switch (arguments[1].asInt())
    {
    case 0: return Value::fromInt(tree->insert(key));
    case 1: return Value::fromInt(tree->erase(key));
    case 2: return Value::fromInt(tree->find(key) != 0);
    case 3: return Value::fromInt(tree->count);
    case 4: return Value::fromInt(tree->height(tree->root));
    case 5: *tree = Tree{}; return Value::fromInt(0);
    case 6:
    case 7:
    {
        auto x = tree->root;
        if (!x) throw std::out_of_range("空红黑树没有最小或最大键");
        unsigned direction = arguments[1].asInt() == 7;
        while (tree->nodes[x].child[direction]) x = tree->nodes[x].child[direction];
        return Value::fromInt(tree->nodes[x].key);
    }
    case 8:
    {
        Index visited = 0;
        return Value::fromInt(!tree->nodes[0].red && !tree->nodes[tree->root].red
                              && tree->check(tree->root, 0, 0, 0, visited) > 0 && visited == tree->count);
    }
    default: throw std::invalid_argument("红黑树操作编号无效");
    }
}
/**
 * @brief 导出有独立存储的升序键快照，不暴露节点槽。
 * @param state 当前调用状态。
 * @param arguments 树句柄。
 * @return 调用方拥有的升序列表。
 */
Value snapshot(State &state, const Memory::Vector<Value> &arguments)
{
    auto *tree = record<Tree>(state, arguments[0], treeType, "红黑树快照");
    if (!tree) return Value::fromInt(0);
    Memory::Vector<std::int64_t> keys;
    std::vector<Index> stack;
    auto x = tree->root;
    while (x || !stack.empty())
    {
        while (x)
        {
            stack.push_back(x);
            x = tree->nodes[x].child[0];
        }
        x = stack.back();
        stack.pop_back();
        keys.push_back(tree->nodes[x].key);
        x = tree->nodes[x].child[1];
    }
    return chali_glue::Codec<Memory::Vector<std::int64_t>>::encode(state.host, keys);
}
}  // namespace
/**
 * @brief 注册红黑树拥有关系、原地操作和显式快照入口。
 * @param registry 当前宿主的注册表。
 * @param error 接收注册失败的诊断。
 * @return 所有入口注册成功时返回 true。
 */
bool bind(chali_glue::Registry &registry, Memory::String *error)
{
    auto &native = registry.native();
    return native_record_glue::bind(native, "std::tree::__redBlackNew", 0, create, error)
           && native_record_glue::bind(native, "std::tree::__redBlackCopy", 1, copy, error)
           && native_record_glue::bind(native, "std::tree::__redBlackDrop", 1, drop, error)
           && native_record_glue::bind(native, "std::tree::__redBlackOp", 3, operate, error)
           && native_record_glue::bind(native, "std::tree::__redBlackSnapshot", 1, snapshot, error);
}
}  // namespace std_red_black_tree_glue
