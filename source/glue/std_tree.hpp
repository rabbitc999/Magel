#ifndef PARSE_ENGINE_STD_TREE_HPP
#define PARSE_ENGINE_STD_TREE_HPP

#include "../magel_glue.hpp"

#include <cstdint>
#include <cstring>
#include <string>

/**
 * @brief MAGEL 二叉树胶水：二叉搜索树与二叉最小堆的全部算法实现。
 *
 * 槽布局与 std::tree::TreeReply 一致（字段顺序即槽序）：
 *   0 error(String)  1 flag  2 value  3 root
 *   4 outKeys(List<int64>)  5 outLefts(List<int64>)  6 outRights(List<int64>)
 *   7 values(List<int64>)  8 colors(List<int64>)
 *
 * BST / 红黑树节点存放在平行列表（keys / lefts / rights，红黑树另有 colors）里，
 * `0 - 1` 表示空指针；颜色约定 0 = 红、1 = 黑。
 * 有状态操作把更新后的列表写回 4/5/6（二叉树）或 7/8 号槽，Magel 侧同步回结构体字段。
 * 堆操作输入的 values 列表在槽 7 输出更新后的副本。
 *
 * 通用树（GeneralReply，与 std::tree::GeneralReply 一致）：
 *   0 error(String)  1 value  2 flag
 *   3 values(List<int64>)  4 firstChild(List<int64>)  5 nextSibling(List<int64>)
 *   6 out(List<int64>)
 * 存储为"第一个孩子 / 下一个兄弟"链：与二叉表示（左孩子 = 第一个孩子，
 * 右孩子 = 下一个兄弟）一一对应，二叉树与通用树可按该映射互转。
 */
namespace std_tree_glue
{
using Context = magel_glue::Context;
using Ints = Memory::Vector<std::int64_t>;

struct ReplyTag
{
};
using Reply = magel_glue::Record<ReplyTag>;

struct GeneralReplyTag
{
};
using GeneralReply = magel_glue::Record<GeneralReplyTag>;

/**
 * @brief 写入结果标量与错误占位（成功路径统一出口）。
 * @param reply 回执视图。
 * @param flag 布尔结果（1/0）。
 * @param value 标量结果。
 */
inline void setResult(Reply reply, std::int64_t flag, std::int64_t value)
{
    reply.set<std::int64_t>(1, flag);
    reply.set<std::int64_t>(2, value);
}

/**
 * @brief 读取传入的 int64 列表。
 * @param ctx 调用上下文。
 * @param handle 列表句柄。
 * @param out 输出缓冲。
 */
inline void readInts(Context &ctx, std::int64_t handle, Ints &out)
{
    Memory::Vector<magel_glue::Value> raw;
    if (!collection_glue::readList(ctx, handle, raw))
    {
        ctx.fail("二叉树：列表参数无效");
        return;
    }
    out.resize(raw.size());
    for (std::size_t i = 0; i < raw.size(); ++i) out[i] = raw[i].asInt();
}

/**
 * @brief 把列表写入回执的指定槽。
 * @param ctx 调用上下文。
 * @param reply 回执视图。
 * @param slot 目标槽下标。
 * @param values 源数据。
 */
inline void writeInts(Context &ctx, Reply reply, std::size_t slot, const Ints &values)
{
    const std::int64_t handle = reply.field<std::int64_t>(slot);
    for (std::int64_t value : values) collection_glue::listPush(ctx, handle, magel_glue::Value::fromInt(value));
}

/**
 * @brief 在树里新分配一个节点（追加到三个平行列表末尾）。
 * @param keys 键列表。
 * @param lefts 左孩子列表。
 * @param rights 右孩子列表。
 * @param value 节点键值。
 * @return 新节点下标。
 */
inline std::int64_t makeNode(Ints &keys, Ints &lefts, Ints &rights, std::int64_t value)
{
    keys.push_back(value);
    lefts.push_back(-1);
    rights.push_back(-1);
    return static_cast<std::int64_t>(keys.size()) - 1;
}

/**
 * @brief 插入一个键（集合语义，重复键忽略）。
 * @param keys 键列表。
 * @param lefts 左孩子列表。
 * @param rights 右孩子列表。
 * @param root 根下标，空树为 -1。
 * @param value 要插入的键。
 * @param ctx 调用上下文。
 * @param reply 回执视图：更新 root / flag（1=已插入）与 4/5/6 号列表槽。
 */
inline void bstInsert(Ints &keys, Ints &lefts, Ints &rights, std::int64_t root,
                      std::int64_t value, Context &ctx, Reply reply)
{
    if (root < 0)
    {
        root = makeNode(keys, lefts, rights, value);
        reply.set<std::int64_t>(1, 1);
        reply.set<std::int64_t>(3, root);
        writeInts(ctx, reply, 4, keys);
        writeInts(ctx, reply, 5, lefts);
        writeInts(ctx, reply, 6, rights);
        return;
    }
    std::int64_t node = root;
    while (true)
    {
        if (value == keys[static_cast<std::size_t>(node)])
        {
            setResult(reply, 0, 0);
            reply.set<std::int64_t>(3, root);
            writeInts(ctx, reply, 4, keys);
            writeInts(ctx, reply, 5, lefts);
            writeInts(ctx, reply, 6, rights);
            return;
        }
        bool goLeft = value < keys[static_cast<std::size_t>(node)];
        std::int64_t child = goLeft ? lefts[static_cast<std::size_t>(node)] : rights[static_cast<std::size_t>(node)];
        if (child < 0)
        {
            const std::int64_t fresh = makeNode(keys, lefts, rights, value);
            if (goLeft)
                lefts[static_cast<std::size_t>(node)] = fresh;
            else
                rights[static_cast<std::size_t>(node)] = fresh;
            reply.set<std::int64_t>(1, 1);
            reply.set<std::int64_t>(3, root);
            writeInts(ctx, reply, 4, keys);
            writeInts(ctx, reply, 5, lefts);
            writeInts(ctx, reply, 6, rights);
            return;
        }
        node = child;
    }
}

/**
 * @brief 查找键是否存在。
 * @param keys 键列表。
 * @param lefts 左孩子列表。
 * @param rights 右孩子列表。
 * @param root 根下标。
 * @param value 要查的键。
 * @param reply 回执视图：flag = 1 表示存在。
 */
inline void bstContains(const Ints &keys, const Ints &lefts, const Ints &rights,
                        std::int64_t root, std::int64_t value, Reply reply)
{
    std::int64_t node = root;
    while (node >= 0)
    {
        const std::int64_t key = keys[static_cast<std::size_t>(node)];
        if (value == key)
        {
            setResult(reply, 1, key);
            return;
        }
        node = value < key ? lefts[static_cast<std::size_t>(node)] : rights[static_cast<std::size_t>(node)];
    }
    setResult(reply, 0, 0);
}

/**
 * @brief 删除一个键（三情况拼接，双孩子取中序后继）。
 * @param keys 键列表。
 * @param lefts 左孩子列表。
 * @param rights 右孩子列表。
 * @param root 根下标。
 * @param value 要删除的键。
 * @param ctx 调用上下文。
 * @param reply 回执视图：flag = 1 表示删除成功，4/5/6 号槽为更新后的列表。
 */
inline void bstRemove(Ints &keys, Ints &lefts, Ints &rights, std::int64_t root,
                      std::int64_t value, Context &ctx, Reply reply)
{
    std::int64_t node = root;
    std::int64_t parent = -1;
    while (node >= 0 && value != keys[static_cast<std::size_t>(node)])
    {
        parent = node;
        node = value < keys[static_cast<std::size_t>(node)] ? lefts[static_cast<std::size_t>(node)] : rights[static_cast<std::size_t>(node)];
    }
    if (node < 0)
    {
        setResult(reply, 0, 0);
        reply.set<std::int64_t>(3, root);
        writeInts(ctx, reply, 4, keys);
        writeInts(ctx, reply, 5, lefts);
        writeInts(ctx, reply, 6, rights);
        return;
    }
    if (lefts[static_cast<std::size_t>(node)] >= 0 && rights[static_cast<std::size_t>(node)] >= 0)
    {
        std::int64_t successor = rights[static_cast<std::size_t>(node)];
        std::int64_t successorParent = node;
        while (lefts[static_cast<std::size_t>(successor)] >= 0)
        {
            successorParent = successor;
            successor = lefts[static_cast<std::size_t>(successor)];
        }
        keys[static_cast<std::size_t>(node)] = keys[static_cast<std::size_t>(successor)];
        node = successor;
        parent = successorParent;
    }
    std::int64_t child = lefts[static_cast<std::size_t>(node)];
    if (child < 0) child = rights[static_cast<std::size_t>(node)];
    if (parent < 0)
    {
        root = child;
    }
    else if (lefts[static_cast<std::size_t>(parent)] == node)
    {
        lefts[static_cast<std::size_t>(parent)] = child;
    }
    else
    {
        rights[static_cast<std::size_t>(parent)] = child;
    }
    setResult(reply, 1, 0);
    reply.set<std::int64_t>(3, root);
    writeInts(ctx, reply, 4, keys);
    writeInts(ctx, reply, 5, lefts);
    writeInts(ctx, reply, 6, rights);
}

/**
 * @brief 求最小键。
 * @param keys 键列表。
 * @param lefts 左孩子列表。
 * @param root 根下标。
 * @param reply 回执视图：flag = 1 表示非空，value 为最小键。
 */
inline void bstMin(const Ints &keys, const Ints &lefts, std::int64_t root, Reply reply)
{
    if (root < 0)
    {
        setResult(reply, 0, 0);
        return;
    }
    std::int64_t node = root;
    while (lefts[static_cast<std::size_t>(node)] >= 0) node = lefts[static_cast<std::size_t>(node)];
    setResult(reply, 1, keys[static_cast<std::size_t>(node)]);
}

/**
 * @brief 求最大键。
 * @param keys 键列表。
 * @param rights 右孩子列表。
 * @param root 根下标。
 * @param reply 回执视图：flag = 1 表示非空，value 为最大键。
 */
inline void bstMax(const Ints &keys, const Ints &rights, std::int64_t root, Reply reply)
{
    if (root < 0)
    {
        setResult(reply, 0, 0);
        return;
    }
    std::int64_t node = root;
    while (rights[static_cast<std::size_t>(node)] >= 0) node = rights[static_cast<std::size_t>(node)];
    setResult(reply, 1, keys[static_cast<std::size_t>(node)]);
}

/**
 * @brief 求树高（节点数口径：单节点 1，空树 0）。
 * @param lefts 左孩子列表。
 * @param rights 右孩子列表。
 * @param root 根下标。
 * @param reply 回执视图：value 为树高。
 */
inline void bstHeight(const Ints &lefts, const Ints &rights, std::int64_t root, Reply reply)
{
    if (root < 0)
    {
        setResult(reply, 1, 0);
        return;
    }
    Ints stack;
    Ints depth;
    stack.push_back(root);
    depth.push_back(1);
    std::int64_t best = 0;
    while (!stack.empty())
    {
        const std::int64_t node = stack.back();
        const std::int64_t level = depth.back();
        stack.pop_back();
        depth.pop_back();
        if (level > best) best = level;
        const std::int64_t l = lefts[static_cast<std::size_t>(node)];
        const std::int64_t r = rights[static_cast<std::size_t>(node)];
        if (l >= 0)
        {
            stack.push_back(l);
            depth.push_back(level + 1);
        }
        if (r >= 0)
        {
            stack.push_back(r);
            depth.push_back(level + 1);
        }
    }
    setResult(reply, 1, best);
}

/**
 * @brief 三种深度的遍历。
 * @param keys 键列表。
 * @param lefts 左孩子列表。
 * @param rights 右孩子列表。
 * @param root 根下标。
 * @param mode 0 中序 / 1 前序 / 2 后序。
 * @param ctx 调用上下文。
 * @param reply 回执视图：7 号槽输出遍历序列。
 */
inline void bstTraverse(const Ints &keys, const Ints &lefts, const Ints &rights,
                        std::int64_t root, std::int64_t mode, Context &ctx, Reply reply)
{
    Ints result;
    if (mode == 0)
    {
        Ints stack;
        std::int64_t node = root;
        while (node >= 0 || !stack.empty())
        {
            while (node >= 0)
            {
                stack.push_back(node);
                node = lefts[static_cast<std::size_t>(node)];
            }
            node = stack.back();
            stack.pop_back();
            result.push_back(keys[static_cast<std::size_t>(node)]);
            node = rights[static_cast<std::size_t>(node)];
        }
    }
    else if (mode == 1)
    {
        Ints stack;
        if (root >= 0) stack.push_back(root);
        while (!stack.empty())
        {
            const std::int64_t node = stack.back();
            stack.pop_back();
            result.push_back(keys[static_cast<std::size_t>(node)]);
            if (rights[static_cast<std::size_t>(node)] >= 0) stack.push_back(rights[static_cast<std::size_t>(node)]);
            if (lefts[static_cast<std::size_t>(node)] >= 0) stack.push_back(lefts[static_cast<std::size_t>(node)]);
        }
    }
    else
    {
        Ints stack;
        if (root >= 0) stack.push_back(root);
        while (!stack.empty())
        {
            const std::int64_t node = stack.back();
            stack.pop_back();
            result.push_back(keys[static_cast<std::size_t>(node)]);
            if (lefts[static_cast<std::size_t>(node)] >= 0) stack.push_back(lefts[static_cast<std::size_t>(node)]);
            if (rights[static_cast<std::size_t>(node)] >= 0) stack.push_back(rights[static_cast<std::size_t>(node)]);
        }
        for (std::size_t lo = 0, hi = result.size(); lo < hi; ++lo)
        {
            --hi;
            const std::int64_t temporary = result[lo];
            result[lo] = result[hi];
            result[hi] = temporary;
        }
    }
    writeInts(ctx, reply, 7, result);
}

/**
 * @brief 堆上浮（push 的后半段）。
 * @param values 堆数组，就地调整。
 */
inline void heapSiftUp(Ints &values)
{
    std::int64_t child = static_cast<std::int64_t>(values.size()) - 1;
    while (child > 0)
    {
        const std::int64_t parent = (child - 1) / 2;
        if (values[static_cast<std::size_t>(child)] < values[static_cast<std::size_t>(parent)])
        {
            const std::int64_t temporary = values[static_cast<std::size_t>(child)];
            values[static_cast<std::size_t>(child)] = values[static_cast<std::size_t>(parent)];
            values[static_cast<std::size_t>(parent)] = temporary;
            child = parent;
        }
        else
        {
            break;
        }
    }
}

/**
 * @brief 堆下沉（pop 的后半段）。
 * @param values 堆数组，就地调整。
 */
inline void heapSiftDown(Ints &values)
{
    const std::int64_t total = static_cast<std::int64_t>(values.size());
    std::int64_t parent = 0;
    while (true)
    {
        const std::int64_t left = parent * 2 + 1;
        const std::int64_t right = left + 1;
        std::int64_t best = parent;
        if (left < total && values[static_cast<std::size_t>(left)] < values[static_cast<std::size_t>(best)]) best = left;
        if (right < total && values[static_cast<std::size_t>(right)] < values[static_cast<std::size_t>(best)]) best = right;
        if (best == parent) break;
        const std::int64_t temporary = values[static_cast<std::size_t>(parent)];
        values[static_cast<std::size_t>(parent)] = values[static_cast<std::size_t>(best)];
        values[static_cast<std::size_t>(best)] = temporary;
        parent = best;
    }
}

/**
 * @brief 入堆一个值。
 * @param values 堆数组。
 * @param value 要入堆的值。
 * @param ctx 调用上下文。
 * @param reply 回执视图：7 号槽输出更新后的堆。
 */
inline void heapPush(Ints &values, std::int64_t value, Context &ctx, Reply reply)
{
    values.push_back(value);
    heapSiftUp(values);
    writeInts(ctx, reply, 7, values);
}

/**
 * @brief 出堆当前最小值。
 * @param values 堆数组。
 * @param reply 回执视图：flag = 1 表示非空，value 为弹出的最小值，7 号槽为更新后的堆。
 */
inline void heapPop(Ints &values, Context &ctx, Reply reply)
{
    if (values.empty())
    {
        setResult(reply, 0, 0);
        return;
    }
    const std::int64_t smallest = values[0];
    const std::int64_t last = values.back();
    values.pop_back();
    if (!values.empty())
    {
        values[0] = last;
        heapSiftDown(values);
    }
    setResult(reply, 1, smallest);
    writeInts(ctx, reply, 7, values);
}

/**
 * @brief 查看堆顶但不弹出。
 * @param values 堆数组。
 * @param reply 回执视图：flag = 1 表示非空，value 为堆顶。
 */
inline void heapPeek(const Ints &values, Reply reply)
{
    if (values.empty())
    {
        setResult(reply, 0, 0);
        return;
    }
    setResult(reply, 1, values[0]);
}

/* ---- 红黑树 ---- */

/**
 * @brief 读取传入的 int64 列表（通用树 / 红黑树共用）。
 * @param ctx 调用上下文。
 * @param handle 列表句柄。
 * @param out 输出缓冲。
 */
inline void readIntsReply(Context &ctx, std::int64_t handle, Ints &out)
{
    Memory::Vector<magel_glue::Value> raw;
    if (!collection_glue::readList(ctx, handle, raw))
    {
        ctx.fail("树：列表参数无效");
        return;
    }
    out.resize(raw.size());
    for (std::size_t i = 0; i < raw.size(); ++i) out[i] = raw[i].asInt();
}

/**
 * @brief 把更新后的红黑树状态写回回执（root / 4/5/6 号列表槽 / 8 号颜色槽）。
 * @param ctx 调用上下文。
 * @param reply 回执视图。
 * @param root 根下标。
 * @param keys 键列表。
 * @param lefts 左孩子列表。
 * @param rights 右孩子列表。
 * @param colors 颜色列表。
 */
inline void writeRbt(Context &ctx, Reply reply, std::int64_t root, const Ints &keys,
                     const Ints &lefts, const Ints &rights, const Ints &colors)
{
    reply.set<std::int64_t>(3, root);
    writeInts(ctx, reply, 4, keys);
    writeInts(ctx, reply, 5, lefts);
    writeInts(ctx, reply, 6, rights);
    writeInts(ctx, reply, 8, colors);
}

/**
 * @brief 红黑树左旋，就地维护父指针表与各孩子下标。
 * @param keys 键列表。
 * @param lefts 左孩子列表。
 * @param rights 右孩子列表。
 * @param parent 父指针表（就地更新）。
 * @param root 根下标（就地更新）。
 * @param x 旋转支点。
 */
inline void rbtRotateLeft(Ints &keys, Ints &lefts, Ints &rights, Ints &parent,
                          std::int64_t &root, std::int64_t x)
{
    std::int64_t y = rights[static_cast<std::size_t>(x)];
    rights[static_cast<std::size_t>(x)] = lefts[static_cast<std::size_t>(y)];
    if (lefts[static_cast<std::size_t>(y)] >= 0) parent[static_cast<std::size_t>(lefts[static_cast<std::size_t>(y)])] = x;
    parent[static_cast<std::size_t>(y)] = parent[static_cast<std::size_t>(x)];
    if (parent[static_cast<std::size_t>(x)] < 0)
    {
        root = y;
    }
    else if (lefts[static_cast<std::size_t>(parent[static_cast<std::size_t>(x)])] == x)
    {
        lefts[static_cast<std::size_t>(parent[static_cast<std::size_t>(x)])] = y;
    }
    else
    {
        rights[static_cast<std::size_t>(parent[static_cast<std::size_t>(x)])] = y;
    }
    lefts[static_cast<std::size_t>(y)] = x;
    parent[static_cast<std::size_t>(x)] = y;
}

/**
 * @brief 红黑树右旋，就地维护父指针表与各孩子下标。
 * @param keys 键列表。
 * @param lefts 左孩子列表。
 * @param rights 右孩子列表。
 * @param parent 父指针表（就地更新）。
 * @param root 根下标（就地更新）。
 * @param x 旋转支点。
 */
inline void rbtRotateRight(Ints &keys, Ints &lefts, Ints &rights, Ints &parent,
                           std::int64_t &root, std::int64_t x)
{
    std::int64_t y = lefts[static_cast<std::size_t>(x)];
    lefts[static_cast<std::size_t>(x)] = rights[static_cast<std::size_t>(y)];
    if (rights[static_cast<std::size_t>(y)] >= 0) parent[static_cast<std::size_t>(rights[static_cast<std::size_t>(y)])] = x;
    parent[static_cast<std::size_t>(y)] = parent[static_cast<std::size_t>(x)];
    if (parent[static_cast<std::size_t>(x)] < 0)
    {
        root = y;
    }
    else if (rights[static_cast<std::size_t>(parent[static_cast<std::size_t>(x)])] == x)
    {
        rights[static_cast<std::size_t>(parent[static_cast<std::size_t>(x)])] = y;
    }
    else
    {
        lefts[static_cast<std::size_t>(parent[static_cast<std::size_t>(x)])] = y;
    }
    rights[static_cast<std::size_t>(y)] = x;
    parent[static_cast<std::size_t>(x)] = y;
}

/**
 * @brief 红黑树插入（含双红修复：叔红变色上溯，叔黑旋转）。
 * @param keys 键列表。
 * @param lefts 左孩子列表。
 * @param rights 右孩子列表。
 * @param colors 颜色列表，0 = 红、1 = 黑。
 * @param root 根下标，空树为 -1。
 * @param value 要插入的键。
 * @param ctx 调用上下文。
 * @param reply 回执视图：flag = 1 表示插入成功（重复键忽略），
 *               4/5/6/8 号槽为更新后的列表，root 为新根。
 * @note 本实现只支持插入不支持删除；删除请用 BinarySearchTree::remove。
 */
inline void rbtInsert(Ints &keys, Ints &lefts, Ints &rights, Ints &colors,
                      std::int64_t &root, std::int64_t value, Context &ctx, Reply reply)
{
    const std::int64_t total = static_cast<std::int64_t>(keys.size());
    if (root < 0)
    {
        keys.push_back(value);
        lefts.push_back(-1);
        rights.push_back(-1);
        colors.push_back(1);  // 根恒黑。
        writeRbt(ctx, reply, total, keys, lefts, rights, colors);
        setResult(reply, 1, 0);
        return;
    }
    // 从 root 重建父指针表。
    Ints parent;
    parent.resize(static_cast<std::size_t>(total), -1);
    {
        Ints stack;
        stack.push_back(root);
        while (!stack.empty())
        {
            const std::int64_t node = stack.back();
            stack.pop_back();
            const std::int64_t l = lefts[static_cast<std::size_t>(node)];
            const std::int64_t r = rights[static_cast<std::size_t>(node)];
            if (l >= 0)
            {
                parent[static_cast<std::size_t>(l)] = node;
                stack.push_back(l);
            }
            if (r >= 0)
            {
                parent[static_cast<std::size_t>(r)] = node;
                stack.push_back(r);
            }
        }
    }
    // 普通 BST 插入。
    std::int64_t node = root;
    std::int64_t child = -1;
    while (true)
    {
        if (value == keys[static_cast<std::size_t>(node)])
        {
            setResult(reply, 0, 0);
            writeRbt(ctx, reply, root, keys, lefts, rights, colors);
            return;
        }
        const bool goLeft = value < keys[static_cast<std::size_t>(node)];
        child = goLeft ? lefts[static_cast<std::size_t>(node)] : rights[static_cast<std::size_t>(node)];
        if (child < 0)
        {
            keys.push_back(value);
            lefts.push_back(-1);
            rights.push_back(-1);
            colors.push_back(0);  // 新节点为红。
            child = static_cast<std::int64_t>(keys.size()) - 1;
            parent.push_back(node);
            if (goLeft)
                lefts[static_cast<std::size_t>(node)] = child;
            else
                rights[static_cast<std::size_t>(node)] = child;
            break;
        }
        node = child;
    }
    // 双红修复。
    std::int64_t z = child;
    while (z != root && colors[static_cast<std::size_t>(parent[static_cast<std::size_t>(z)])] == 0)
    {
        const std::int64_t p = parent[static_cast<std::size_t>(z)];
        const std::int64_t g = parent[static_cast<std::size_t>(p)];
        if (lefts[static_cast<std::size_t>(g)] == p)
        {
            const std::int64_t uncle = rights[static_cast<std::size_t>(g)];
            if (uncle >= 0 && colors[static_cast<std::size_t>(uncle)] == 0)
            {
                colors[static_cast<std::size_t>(p)] = 1;
                colors[static_cast<std::size_t>(uncle)] = 1;
                colors[static_cast<std::size_t>(g)] = 0;
                z = g;
            }
            else
            {
                if (rights[static_cast<std::size_t>(p)] == z)
                {
                    z = p;
                    rbtRotateLeft(keys, lefts, rights, parent, root, z);
                }
                const std::int64_t p2 = parent[static_cast<std::size_t>(z)];
                colors[static_cast<std::size_t>(p2)] = 1;
                colors[static_cast<std::size_t>(g)] = 0;
                rbtRotateRight(keys, lefts, rights, parent, root, g);
            }
        }
        else
        {
            const std::int64_t uncle = lefts[static_cast<std::size_t>(g)];
            if (uncle >= 0 && colors[static_cast<std::size_t>(uncle)] == 0)
            {
                colors[static_cast<std::size_t>(p)] = 1;
                colors[static_cast<std::size_t>(uncle)] = 1;
                colors[static_cast<std::size_t>(g)] = 0;
                z = g;
            }
            else
            {
                if (lefts[static_cast<std::size_t>(p)] == z)
                {
                    z = p;
                    rbtRotateRight(keys, lefts, rights, parent, root, z);
                }
                const std::int64_t p2 = parent[static_cast<std::size_t>(z)];
                colors[static_cast<std::size_t>(p2)] = 1;
                colors[static_cast<std::size_t>(g)] = 0;
                rbtRotateLeft(keys, lefts, rights, parent, root, g);
            }
        }
    }
    colors[static_cast<std::size_t>(root)] = 1;
    setResult(reply, 1, 0);
    writeRbt(ctx, reply, root, keys, lefts, rights, colors);
}

/* ---- 通用树（第一个孩子 / 下一个兄弟存储） ---- */

/**
 * @brief 把列表写入通用树回执的指定槽。
 * @param ctx 调用上下文。
 * @param reply 回执视图。
 * @param slot 目标槽下标。
 * @param values 源数据。
 */
inline void writeIntsGeneral(Context &ctx, GeneralReply reply, std::size_t slot, const Ints &values)
{
    const std::int64_t handle = reply.field<std::int64_t>(slot);
    for (std::int64_t value : values) collection_glue::listPush(ctx, handle, magel_glue::Value::fromInt(value));
}

/**
 * @brief 把通用树状态写回回执。
 * @param ctx 调用上下文。
 * @param reply 回执视图。
 * @param values 值列表。
 * @param firstChild 首孩子列表。
 * @param nextSibling 下一兄弟列表。
 */
inline void gtWriteState(Context &ctx, GeneralReply reply, const Ints &values,
                         const Ints &firstChild, const Ints &nextSibling)
{
    writeIntsGeneral(ctx, reply, 3, values);
    writeIntsGeneral(ctx, reply, 4, firstChild);
    writeIntsGeneral(ctx, reply, 5, nextSibling);
}

/**
 * @brief 读取通用树回执里的列表参数。
 * @param ctx 调用上下文。
 * @param reply 回执视图。
 */
inline void gtReadState(Context &ctx, GeneralReply reply, Ints &values, Ints &firstChild,
                        Ints &nextSibling)
{
    readIntsReply(ctx, reply.field<std::int64_t>(3), values);
    readIntsReply(ctx, reply.field<std::int64_t>(4), firstChild);
    readIntsReply(ctx, reply.field<std::int64_t>(5), nextSibling);
}

/**
 * @brief 通用树追加节点：cursor < 0 建根（仅空树），否则成为 cursor 的最后一个孩子。
 * @param values 值列表。
 * @param firstChild 首孩子列表。
 * @param nextSibling 下一兄弟列表。
 * @param count 当前节点数。
 * @param cursor 父节点下标，建根传 -1。
 * @param value 新节点的值。
 * @param ctx 调用上下文。
 * @param reply 回执视图：value 为新节点下标，flag = 1 表示成功，
 *               3/4/5 号槽为更新后的列表。
 */
inline void gtAddChild(Ints &values, Ints &firstChild, Ints &nextSibling,
                       std::int64_t count, std::int64_t cursor, std::int64_t value,
                       Context &ctx, GeneralReply reply)
{
    if (cursor < 0)
    {
        if (count != 0)
        {
            reply.set<std::int64_t>(2, 0);
            return;
        }
        values.push_back(value);
        firstChild.push_back(-1);
        nextSibling.push_back(-1);
        reply.set<std::int64_t>(1, 0);
        reply.set<std::int64_t>(2, 1);
        gtWriteState(ctx, reply, values, firstChild, nextSibling);
        return;
    }
    if (cursor >= static_cast<std::int64_t>(values.size()))
    {
        reply.set<std::int64_t>(2, 0);
        return;
    }
    values.push_back(value);
    firstChild.push_back(-1);
    nextSibling.push_back(-1);
    const std::int64_t fresh = static_cast<std::int64_t>(values.size()) - 1;
    const std::int64_t child = firstChild[static_cast<std::size_t>(cursor)];
    if (child < 0)
    {
        firstChild[static_cast<std::size_t>(cursor)] = fresh;
    }
    else
    {
        std::int64_t last = child;
        while (nextSibling[static_cast<std::size_t>(last)] >= 0)
        {
            last = nextSibling[static_cast<std::size_t>(last)];
        }
        nextSibling[static_cast<std::size_t>(last)] = fresh;
    }
    reply.set<std::int64_t>(1, fresh);
    reply.set<std::int64_t>(2, 1);
    gtWriteState(ctx, reply, values, firstChild, nextSibling);
}

/**
 * @brief 列出某节点的全部孩子下标（按插入序）。
 * @param firstChild 首孩子列表。
 * @param nextSibling 下一兄弟列表。
 * @param cursor 目标节点下标。
 * @param ctx 调用上下文。
 * @param reply 回执视图：out 槽输出孩子下标列表。
 */
inline void gtChildren(const Ints &firstChild, const Ints &nextSibling, std::int64_t cursor,
                       Context &ctx, GeneralReply reply)
{
    Ints result;
    std::int64_t child = cursor >= 0 ? firstChild[static_cast<std::size_t>(cursor)] : -1;
    while (child >= 0)
    {
        result.push_back(child);
        child = nextSibling[static_cast<std::size_t>(child)];
    }
    writeIntsGeneral(ctx, reply, 6, result);
}

/**
 * @brief 先序遍历（节点 - 各孩子依次）。
 * @param firstChild 首孩子列表。
 * @param nextSibling 下一兄弟列表。
 * @param root 根下标。
 * @param ctx 调用上下文。
 * @param reply 回执视图：out 槽输出访问序。
 */
inline void gtPreorder(const Ints &firstChild, const Ints &nextSibling, std::int64_t root,
                       Context &ctx, GeneralReply reply)
{
    Ints result;
    Ints stack;
    if (root >= 0) stack.push_back(root);
    while (!stack.empty())
    {
        const std::int64_t node = stack.back();
        stack.pop_back();
        result.push_back(node);
        Ints children;
        for (std::int64_t child = firstChild[static_cast<std::size_t>(node)]; child >= 0;
             child = nextSibling[static_cast<std::size_t>(child)])
        {
            children.push_back(child);
        }
        for (std::size_t i = children.size(); i > 0; --i) stack.push_back(children[i - 1]);
    }
    writeIntsGeneral(ctx, reply, 6, result);
}

/**
 * @brief 层序遍历。
 * @param firstChild 首孩子列表。
 * @param nextSibling 下一兄弟列表。
 * @param root 根下标。
 * @param ctx 调用上下文。
 * @param reply 回执视图：out 槽输出访问序。
 */
inline void gtLevelOrder(const Ints &firstChild, const Ints &nextSibling, std::int64_t root,
                         Context &ctx, GeneralReply reply)
{
    Ints result;
    Ints queue;
    if (root >= 0) queue.push_back(root);
    for (std::size_t head = 0; head < queue.size(); ++head)
    {
        const std::int64_t node = queue[head];
        result.push_back(node);
        for (std::int64_t child = firstChild[static_cast<std::size_t>(node)]; child >= 0;
             child = nextSibling[static_cast<std::size_t>(child)])
        {
            queue.push_back(child);
        }
    }
    writeIntsGeneral(ctx, reply, 6, result);
}

/**
 * @brief 树高（节点数口径：单节点 1，空树 0）。
 * @param firstChild 首孩子列表。
 * @param nextSibling 下一兄弟列表。
 * @param root 根下标。
 * @param reply 回执视图：value 为树高。
 */
inline void gtHeight(const Ints &firstChild, const Ints &nextSibling, std::int64_t root,
                     GeneralReply reply)
{
    if (root < 0)
    {
        reply.set<std::int64_t>(1, 0);
        return;
    }
    Ints stack;
    Ints depth;
    stack.push_back(root);
    depth.push_back(1);
    std::int64_t best = 0;
    while (!stack.empty())
    {
        const std::int64_t node = stack.back();
        const std::int64_t level = depth.back();
        stack.pop_back();
        depth.pop_back();
        if (level > best) best = level;
        for (std::int64_t child = firstChild[static_cast<std::size_t>(node)]; child >= 0;
             child = nextSibling[static_cast<std::size_t>(child)])
        {
            stack.push_back(child);
            depth.push_back(level + 1);
        }
    }
    reply.set<std::int64_t>(1, best);
}

/**
 * @brief 注册 std::tree 的全部胶水入口。
 * @param registry 函数注册表。
 * @param error 注册失败时的错误说明。
 * @return 全部注册成功返回 true。
 */
inline bool bind(magel_glue::Registry &registry, Memory::String *error)
{
    bool ok = true;
    auto fail = [&](const char *message) {
        if (error) *error = Memory::String(message, std::strlen(message));
        ok = false;
    };
    ok = registry.bindTypedWithContext<void, Ints, Ints, Ints, std::int64_t, std::int64_t, Reply>(
             "std::tree::__bstInsert",
             [](Context &ctx, Ints keys, Ints lefts, Ints rights, std::int64_t root,
                std::int64_t value, Reply reply) {
                 bstInsert(keys, lefts, rights, root, value, ctx, reply);
             })
         && ok;
    ok = registry.bindTypedWithContext<void, Ints, Ints, Ints, std::int64_t, std::int64_t, Reply>(
             "std::tree::__bstContains",
             [](Context &, Ints keys, Ints lefts, Ints rights, std::int64_t root,
                std::int64_t value, Reply reply) {
                 bstContains(keys, lefts, rights, root, value, reply);
             })
         && ok;
    ok = registry.bindTypedWithContext<void, Ints, Ints, Ints, std::int64_t, std::int64_t, Reply>(
             "std::tree::__bstRemove",
             [](Context &ctx, Ints keys, Ints lefts, Ints rights, std::int64_t root,
                std::int64_t value, Reply reply) {
                 bstRemove(keys, lefts, rights, root, value, ctx, reply);
             })
         && ok;
    ok = registry.bindTypedWithContext<void, Ints, Ints, std::int64_t, Reply>(
             "std::tree::__bstMin",
             [](Context &, Ints keys, Ints lefts, std::int64_t root, Reply reply) {
                 bstMin(keys, lefts, root, reply);
             })
         && ok;
    ok = registry.bindTypedWithContext<void, Ints, Ints, std::int64_t, Reply>(
             "std::tree::__bstMax",
             [](Context &, Ints keys, Ints rights, std::int64_t root, Reply reply) {
                 bstMax(keys, rights, root, reply);
             })
         && ok;
    ok = registry.bindTypedWithContext<void, Ints, Ints, std::int64_t, Reply>(
             "std::tree::__bstHeight",
             [](Context &, Ints lefts, Ints rights, std::int64_t root, Reply reply) {
                 bstHeight(lefts, rights, root, reply);
             })
         && ok;
    ok = registry.bindTypedWithContext<void, Ints, Ints, Ints, std::int64_t, std::int64_t, Reply>(
             "std::tree::__bstTraverse",
             [](Context &ctx, Ints keys, Ints lefts, Ints rights, std::int64_t root,
                std::int64_t mode, Reply reply) {
                 bstTraverse(keys, lefts, rights, root, mode, ctx, reply);
             })
         && ok;
    ok = registry.bindTypedWithContext<void, Ints, std::int64_t, Reply>(
             "std::tree::__heapPush",
             [](Context &ctx, Ints values, std::int64_t value, Reply reply) {
                 heapPush(values, value, ctx, reply);
             })
         && ok;
    ok = registry.bindTypedWithContext<void, Ints, Reply>(
             "std::tree::__heapPop",
             [](Context &ctx, Ints values, Reply reply) { heapPop(values, ctx, reply); })
         && ok;
    ok = registry.bindTypedWithContext<void, Ints, Reply>(
             "std::tree::__heapPeek",
             [](Context &, Ints values, Reply reply) { heapPeek(values, reply); })
         && ok;
    ok = registry.bindTypedWithContext<void, Ints, Ints, Ints, Ints, std::int64_t, std::int64_t, Reply>(
             "std::tree::__rbtInsert",
             [](Context &ctx, Ints keys, Ints lefts, Ints rights, Ints colors, std::int64_t root,
                std::int64_t value, Reply reply) {
                 rbtInsert(keys, lefts, rights, colors, root, value, ctx, reply);
             })
         && ok;
    ok = registry.bindTypedWithContext<void, Ints, Ints, Ints, std::int64_t, std::int64_t, Reply>(
             "std::tree::__rbtContains",
             [](Context &, Ints keys, Ints lefts, Ints rights, std::int64_t root, std::int64_t value,
                Reply reply) { bstContains(keys, lefts, rights, root, value, reply); })
         && ok;
    ok = registry.bindTypedWithContext<void, Ints, Ints, std::int64_t, Reply>(
             "std::tree::__rbtMin",
             [](Context &, Ints keys, Ints lefts, std::int64_t root, Reply reply) {
                 bstMin(keys, lefts, root, reply);
             })
         && ok;
    ok = registry.bindTypedWithContext<void, Ints, Ints, std::int64_t, Reply>(
             "std::tree::__rbtMax",
             [](Context &, Ints keys, Ints rights, std::int64_t root, Reply reply) {
                 bstMax(keys, rights, root, reply);
             })
         && ok;
    ok = registry.bindTypedWithContext<void, Ints, Ints, std::int64_t, Reply>(
             "std::tree::__rbtHeight",
             [](Context &, Ints lefts, Ints rights, std::int64_t root, Reply reply) {
                 bstHeight(lefts, rights, root, reply);
             })
         && ok;
    ok = registry.bindTypedWithContext<void, Ints, Ints, Ints, std::int64_t, Reply>(
             "std::tree::__rbtInorder",
             [](Context &ctx, Ints keys, Ints lefts, Ints rights, std::int64_t root, Reply reply) {
                 bstTraverse(keys, lefts, rights, root, 0, ctx, reply);
             })
         && ok;
    ok = registry.bindTypedWithContext<
             void, Ints, Ints, Ints, std::int64_t, std::int64_t, std::int64_t, GeneralReply>(
             "std::tree::__gtAddChild",
             [](Context &ctx, Ints values, Ints firstChild, Ints nextSibling, std::int64_t count,
                std::int64_t cursor, std::int64_t value, GeneralReply reply) {
                 gtAddChild(values, firstChild, nextSibling, count, cursor, value, ctx, reply);
             })
         && ok;
    ok = registry.bindTypedWithContext<void, Ints, Ints, std::int64_t, GeneralReply>(
             "std::tree::__gtChildren",
             [](Context &ctx, Ints firstChild, Ints nextSibling, std::int64_t cursor,
                GeneralReply reply) { gtChildren(firstChild, nextSibling, cursor, ctx, reply); })
         && ok;
    ok = registry.bindTypedWithContext<void, Ints, Ints, std::int64_t, GeneralReply>(
             "std::tree::__gtPreorder",
             [](Context &ctx, Ints firstChild, Ints nextSibling, std::int64_t root,
                GeneralReply reply) { gtPreorder(firstChild, nextSibling, root, ctx, reply); })
         && ok;
    ok = registry.bindTypedWithContext<void, Ints, Ints, std::int64_t, GeneralReply>(
             "std::tree::__gtLevelOrder",
             [](Context &ctx, Ints firstChild, Ints nextSibling, std::int64_t root,
                GeneralReply reply) { gtLevelOrder(firstChild, nextSibling, root, ctx, reply); })
         && ok;
    ok = registry.bindTypedWithContext<void, Ints, Ints, std::int64_t, GeneralReply>(
             "std::tree::__gtHeight",
             [](Context &, Ints firstChild, Ints nextSibling, std::int64_t root, GeneralReply reply) {
                 gtHeight(firstChild, nextSibling, root, reply);
             })
         && ok;
    if (!ok) fail("std::tree 胶水注册失败");
    return ok;
}
}  // namespace std_tree_glue

#endif
