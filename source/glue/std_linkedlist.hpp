#ifndef PARSE_ENGINE_STD_LINKEDLIST_HPP
#define PARSE_ENGINE_STD_LINKEDLIST_HPP

#include "../magel_glue.hpp"

#include <cstdint>
#include <cstring>
#include <string>

/**
 * @brief MAGEL 双向链表胶水：LinkedList 的全部结构操作实现。
 *
 * 链表采用游标存储：values / prevs / nexts 三个平行列表，`0 - 1` 表示空指针；
 * 被删节点回收进 free 列表复用。head / tail 是首尾节点下标，count 是有效节点数。
 *
 * ListReply 槽布局与 std::linkedlist::ListReply 一致：
 *   0 error(String)  1 cursor  2 flag  3 value  4 head  5 tail  6 count
 *   7 values(List<int64>)  8 prevs(List<int64>)  9 nexts(List<int64>)
 *   10 free(List<int64>)  11 out(List<int64>)
 *
 * 有状态操作把更新后的列表写回 7/8/9/10 号槽，Magel 侧同步回结构体字段。
 */
namespace std_linkedlist_glue
{
using Context = magel_glue::Context;
using Ints = Memory::Vector<std::int64_t>;

struct ReplyTag
{
};
using Reply = magel_glue::Record<ReplyTag>;

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
        ctx.fail("链表：列表参数无效");
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
 * @brief 把全部更新写回回执（4/5/6 号标量槽 + 7/8/9/10 号列表槽）。
 * @param ctx 调用上下文。
 * @param reply 回执视图。
 * @param head 首节点下标。
 * @param tail 尾节点下标。
 * @param count 有效节点数。
 * @param values 值列表。
 * @param prevs 前驱列表。
 * @param nexts 后继列表。
 * @param freeSlots 空闲槽列表。
 */
inline void writeState(Context &ctx, Reply reply, std::int64_t head, std::int64_t tail,
                       std::int64_t count, const Ints &values, const Ints &prevs,
                       const Ints &nexts, const Ints &freeSlots)
{
    reply.set<std::int64_t>(4, head);
    reply.set<std::int64_t>(5, tail);
    reply.set<std::int64_t>(6, count);
    writeInts(ctx, reply, 7, values);
    writeInts(ctx, reply, 8, prevs);
    writeInts(ctx, reply, 9, nexts);
    writeInts(ctx, reply, 10, freeSlots);
}

/**
 * @brief 从空闲列表或尾部新分配一个节点槽。
 * @param values 值列表。
 * @param prevs 前驱列表。
 * @param nexts 后继列表。
 * @param freeSlots 空闲槽列表。
 * @return 分配到的槽下标。
 */
inline std::int64_t allocSlot(Ints &values, Ints &prevs, Ints &nexts, Ints &freeSlots)
{
    if (!freeSlots.empty())
    {
        const std::int64_t slot = freeSlots.back();
        freeSlots.pop_back();
        return slot;
    }
    values.push_back(0);
    prevs.push_back(-1);
    nexts.push_back(-1);
    return static_cast<std::int64_t>(values.size()) - 1;
}

/**
 * @brief 头插一个值。
 * @param values 值列表。
 * @param prevs 前驱列表。
 * @param nexts 后继列表。
 * @param freeSlots 空闲槽列表。
 * @param head 首节点下标。
 * @param tail 尾节点下标。
 * @param count 有效节点数。
 * @param value 要插入的值。
 * @param ctx 调用上下文。
 * @param reply 回执视图：cursor 为新节点下标，其余槽为更新后的链表状态。
 */
inline void llPushFront(Ints &values, Ints &prevs, Ints &nexts, Ints &freeSlots,
                        std::int64_t head, std::int64_t tail, std::int64_t count,
                        std::int64_t value, Context &ctx, Reply reply)
{
    const std::int64_t slot = allocSlot(values, prevs, nexts, freeSlots);
    values[static_cast<std::size_t>(slot)] = value;
    prevs[static_cast<std::size_t>(slot)] = -1;
    nexts[static_cast<std::size_t>(slot)] = head;
    if (head >= 0) prevs[static_cast<std::size_t>(head)] = slot;
    head = slot;
    if (tail < 0) tail = slot;
    count += 1;
    reply.set<std::int64_t>(1, slot);
    writeState(ctx, reply, head, tail, count, values, prevs, nexts, freeSlots);
}

/**
 * @brief 尾插一个值。
 * @param values 值列表。
 * @param prevs 前驱列表。
 * @param nexts 后继列表。
 * @param freeSlots 空闲槽列表。
 * @param head 首节点下标。
 * @param tail 尾节点下标。
 * @param count 有效节点数。
 * @param value 要插入的值。
 * @param ctx 调用上下文。
 * @param reply 回执视图：cursor 为新节点下标，其余槽为更新后的链表状态。
 */
inline void llPushBack(Ints &values, Ints &prevs, Ints &nexts, Ints &freeSlots,
                       std::int64_t head, std::int64_t tail, std::int64_t count,
                       std::int64_t value, Context &ctx, Reply reply)
{
    const std::int64_t slot = allocSlot(values, prevs, nexts, freeSlots);
    values[static_cast<std::size_t>(slot)] = value;
    nexts[static_cast<std::size_t>(slot)] = -1;
    prevs[static_cast<std::size_t>(slot)] = tail;
    if (tail >= 0) nexts[static_cast<std::size_t>(tail)] = slot;
    tail = slot;
    if (head < 0) head = slot;
    count += 1;
    reply.set<std::int64_t>(1, slot);
    writeState(ctx, reply, head, tail, count, values, prevs, nexts, freeSlots);
}

/**
 * @brief 在指定节点前插入。
 * @param values 值列表。
 * @param prevs 前驱列表。
 * @param nexts 后继列表。
 * @param freeSlots 空闲槽列表。
 * @param head 首节点下标。
 * @param tail 尾节点下标。
 * @param count 有效节点数。
 * @param cursor 目标节点下标。
 * @param value 要插入的值。
 * @param ctx 调用上下文。
 * @param reply 回执视图：cursor 为新节点下标（cursor 无效时 flag = 0）。
 */
inline void llInsertBefore(Ints &values, Ints &prevs, Ints &nexts, Ints &freeSlots,
                           std::int64_t head, std::int64_t tail, std::int64_t count,
                           std::int64_t cursor, std::int64_t value, Context &ctx, Reply reply)
{
    if (cursor < 0 || cursor >= static_cast<std::int64_t>(values.size())
        || prevs[static_cast<std::size_t>(cursor)] == -2)
    {
        reply.set<std::int64_t>(2, 0);
        return;
    }
    const std::int64_t slot = allocSlot(values, prevs, nexts, freeSlots);
    values[static_cast<std::size_t>(slot)] = value;
    prevs[static_cast<std::size_t>(slot)] = prevs[static_cast<std::size_t>(cursor)];
    nexts[static_cast<std::size_t>(slot)] = cursor;
    if (prevs[static_cast<std::size_t>(cursor)] >= 0)
    {
        nexts[static_cast<std::size_t>(prevs[static_cast<std::size_t>(cursor)])] = slot;
    }
    else
    {
        head = slot;
    }
    prevs[static_cast<std::size_t>(cursor)] = slot;
    count += 1;
    reply.set<std::int64_t>(1, slot);
    reply.set<std::int64_t>(2, 1);
    writeState(ctx, reply, head, tail, count, values, prevs, nexts, freeSlots);
}

/**
 * @brief 在指定节点后插入。
 * @param values 值列表。
 * @param prevs 前驱列表。
 * @param nexts 后继列表。
 * @param freeSlots 空闲槽列表。
 * @param head 首节点下标。
 * @param tail 尾节点下标。
 * @param count 有效节点数。
 * @param cursor 目标节点下标。
 * @param value 要插入的值。
 * @param ctx 调用上下文。
 * @param reply 回执视图：cursor 为新节点下标（cursor 无效时 flag = 0）。
 */
inline void llInsertAfter(Ints &values, Ints &prevs, Ints &nexts, Ints &freeSlots,
                          std::int64_t head, std::int64_t tail, std::int64_t count,
                          std::int64_t cursor, std::int64_t value, Context &ctx, Reply reply)
{
    if (cursor < 0 || cursor >= static_cast<std::int64_t>(values.size())
        || prevs[static_cast<std::size_t>(cursor)] == -2)
    {
        reply.set<std::int64_t>(2, 0);
        return;
    }
    const std::int64_t slot = allocSlot(values, prevs, nexts, freeSlots);
    values[static_cast<std::size_t>(slot)] = value;
    nexts[static_cast<std::size_t>(slot)] = nexts[static_cast<std::size_t>(cursor)];
    prevs[static_cast<std::size_t>(slot)] = cursor;
    if (nexts[static_cast<std::size_t>(cursor)] >= 0)
    {
        prevs[static_cast<std::size_t>(nexts[static_cast<std::size_t>(cursor)])] = slot;
    }
    else
    {
        tail = slot;
    }
    nexts[static_cast<std::size_t>(cursor)] = slot;
    count += 1;
    reply.set<std::int64_t>(1, slot);
    reply.set<std::int64_t>(2, 1);
    writeState(ctx, reply, head, tail, count, values, prevs, nexts, freeSlots);
}

/**
 * @brief 摘除一个节点（不动首尾语义，供 pop / removeAt 复用）。
 * @param values 值列表。
 * @param prevs 前驱列表。
 * @param nexts 后继列表。
 * @param freeSlots 空闲槽列表。
 * @param head 首节点下标（就地更新）。
 * @param tail 尾节点下标（就地更新）。
 * @param count 有效节点数（就地更新）。
 * @param node 要摘除的节点下标。
 * @return 摘除节点的原值。
 */
inline std::int64_t unlinkNode(Ints &values, Ints &prevs, Ints &nexts, Ints &freeSlots,
                               std::int64_t &head, std::int64_t &tail, std::int64_t &count,
                               std::int64_t node)
{
    const std::int64_t previous = prevs[static_cast<std::size_t>(node)];
    const std::int64_t following = nexts[static_cast<std::size_t>(node)];
    if (previous >= 0)
        nexts[static_cast<std::size_t>(previous)] = following;
    else
        head = following;
    if (following >= 0)
        prevs[static_cast<std::size_t>(following)] = previous;
    else
        tail = previous;
    prevs[static_cast<std::size_t>(node)] = -2;  // 标记已删除（-2 不是合法前驱）。
    nexts[static_cast<std::size_t>(node)] = -2;
    freeSlots.push_back(node);
    count -= 1;
    return values[static_cast<std::size_t>(node)];
}

/**
 * @brief 弹出首节点。
 * @param values 值列表。
 * @param prevs 前驱列表。
 * @param nexts 后继列表。
 * @param freeSlots 空闲槽列表。
 * @param head 首节点下标。
 * @param tail 尾节点下标。
 * @param count 有效节点数。
 * @param ctx 调用上下文。
 * @param reply 回执视图：flag = 1 表示非空，value 为弹出的值。
 */
inline void llPopFront(Ints &values, Ints &prevs, Ints &nexts, Ints &freeSlots,
                       std::int64_t head, std::int64_t tail, std::int64_t count,
                       Context &ctx, Reply reply)
{
    if (head < 0)
    {
        reply.set<std::int64_t>(2, 0);
        writeState(ctx, reply, head, tail, count, values, prevs, nexts, freeSlots);
        return;
    }
    const std::int64_t popped = unlinkNode(values, prevs, nexts, freeSlots, head, tail, count, head);
    reply.set<std::int64_t>(2, 1);
    reply.set<std::int64_t>(3, popped);
    writeState(ctx, reply, head, tail, count, values, prevs, nexts, freeSlots);
}

/**
 * @brief 弹出尾节点。
 * @param values 值列表。
 * @param prevs 前驱列表。
 * @param nexts 后继列表。
 * @param freeSlots 空闲槽列表。
 * @param head 首节点下标。
 * @param tail 尾节点下标。
 * @param count 有效节点数。
 * @param ctx 调用上下文。
 * @param reply 回执视图：flag = 1 表示非空，value 为弹出的值。
 */
inline void llPopBack(Ints &values, Ints &prevs, Ints &nexts, Ints &freeSlots,
                      std::int64_t head, std::int64_t tail, std::int64_t count,
                      Context &ctx, Reply reply)
{
    if (tail < 0)
    {
        reply.set<std::int64_t>(2, 0);
        writeState(ctx, reply, head, tail, count, values, prevs, nexts, freeSlots);
        return;
    }
    const std::int64_t popped = unlinkNode(values, prevs, nexts, freeSlots, head, tail, count, tail);
    reply.set<std::int64_t>(2, 1);
    reply.set<std::int64_t>(3, popped);
    writeState(ctx, reply, head, tail, count, values, prevs, nexts, freeSlots);
}

/**
 * @brief 摘除指定节点。
 * @param values 值列表。
 * @param prevs 前驱列表。
 * @param nexts 后继列表。
 * @param freeSlots 空闲槽列表。
 * @param head 首节点下标。
 * @param tail 尾节点下标。
 * @param count 有效节点数。
 * @param cursor 要摘除的节点下标。
 * @param ctx 调用上下文。
 * @param reply 回执视图：flag = 1 表示成功，value 为摘除的值。
 */
inline void llRemoveAt(Ints &values, Ints &prevs, Ints &nexts, Ints &freeSlots,
                       std::int64_t head, std::int64_t tail, std::int64_t count,
                       std::int64_t cursor, Context &ctx, Reply reply)
{
    const bool valid = cursor >= 0 && cursor < static_cast<std::int64_t>(values.size())
                       && prevs[static_cast<std::size_t>(cursor)] != -2;
    if (!valid)
    {
        reply.set<std::int64_t>(2, 0);
        writeState(ctx, reply, head, tail, count, values, prevs, nexts, freeSlots);
        return;
    }
    const std::int64_t removed = unlinkNode(values, prevs, nexts, freeSlots, head, tail, count, cursor);
    reply.set<std::int64_t>(2, 1);
    reply.set<std::int64_t>(3, removed);
    writeState(ctx, reply, head, tail, count, values, prevs, nexts, freeSlots);
}

/**
 * @brief 原地反转链表（nexts 与 prevs 逐槽交换，首尾对调）。
 * @param values 值列表。
 * @param prevs 前驱列表。
 * @param nexts 后继列表。
 * @param head 首节点下标。
 * @param tail 尾节点下标。
 * @param count 有效节点数。
 * @param ctx 调用上下文。
 * @param reply 回执视图：4/5/6 号槽与 7/8/9/10 号列表槽为反转后的状态。
 */
inline void llReverse(const Ints &values, const Ints &freeSlots, Ints &prevs, Ints &nexts, std::int64_t head,
                      std::int64_t tail, std::int64_t count, Context &ctx, Reply reply)
{
    Ints swappedNexts = prevs;
    Ints swappedPrevs = nexts;
    const std::int64_t swappedHead = tail;
    const std::int64_t swappedTail = head;
    writeState(ctx, reply, swappedHead, swappedTail, count, values, swappedPrevs, swappedNexts, freeSlots);
}

/**
 * @brief 按链表顺序导出值序列。
 * @param values 值列表。
 * @param nexts 后继列表。
 * @param head 首节点下标。
 * @param count 有效节点数。
 * @param ctx 调用上下文。
 * @param reply 回执视图：out 槽输出值序列。
 */
inline void llToArray(const Ints &values, const Ints &nexts, std::int64_t head,
                      std::int64_t count, Context &ctx, Reply reply)
{
    Ints result;
    std::int64_t node = head;
    while (node >= 0)
    {
        result.push_back(values[static_cast<std::size_t>(node)]);
        node = nexts[static_cast<std::size_t>(node)];
    }
    (void)count;
    writeInts(ctx, reply, 11, result);
}

/**
 * @brief 按链表顺序查找值。
 * @param values 值列表。
 * @param nexts 后继列表。
 * @param head 首节点下标。
 * @param value 目标值。
 * @param reply 回执视图：flag = 1 表示存在。
 */
inline void llContains(const Ints &values, const Ints &nexts, std::int64_t head,
                       std::int64_t value, Reply reply)
{
    std::int64_t node = head;
    while (node >= 0)
    {
        if (values[static_cast<std::size_t>(node)] == value)
        {
            reply.set<std::int64_t>(2, 1);
            reply.set<std::int64_t>(3, node);
            return;
        }
        node = nexts[static_cast<std::size_t>(node)];
    }
    reply.set<std::int64_t>(2, 0);
}

/**
 * @brief 注册 std::linkedlist 的全部胶水入口。
 * @param registry 函数注册表。
 * @param error 注册失败时的错误说明。
 * @return 全部注册成功返回 true。
 */
inline bool bind(magel_glue::Registry &registry, Memory::String *error)
{
    bool ok = true;
    auto push = [&](const char *name, auto &&lambda) {
        ok = registry.bindTyped(name, std::forward<decltype(lambda)>(lambda)) && ok;
    };
    (void)push;
    ok = registry.bindTypedWithContext<
             void, Ints, Ints, Ints, Ints, std::int64_t, std::int64_t, std::int64_t, std::int64_t, Reply>(
             "std::linkedlist::__pushFront",
             [](Context &ctx, Ints values, Ints prevs, Ints nexts, Ints freeSlots,
                std::int64_t head, std::int64_t tail, std::int64_t count, std::int64_t value,
                Reply reply) {
                 llPushFront(values, prevs, nexts, freeSlots, head, tail, count, value, ctx, reply);
             })
         && ok;
    ok = registry.bindTypedWithContext<
             void, Ints, Ints, Ints, Ints, std::int64_t, std::int64_t, std::int64_t, std::int64_t, Reply>(
             "std::linkedlist::__pushBack",
             [](Context &ctx, Ints values, Ints prevs, Ints nexts, Ints freeSlots,
                std::int64_t head, std::int64_t tail, std::int64_t count, std::int64_t value,
                Reply reply) {
                 llPushBack(values, prevs, nexts, freeSlots, head, tail, count, value, ctx, reply);
             })
         && ok;
    ok = registry.bindTypedWithContext<
             void, Ints, Ints, Ints, Ints, std::int64_t, std::int64_t, std::int64_t, Reply>(
             "std::linkedlist::__popFront",
             [](Context &ctx, Ints values, Ints prevs, Ints nexts, Ints freeSlots,
                std::int64_t head, std::int64_t tail, std::int64_t count, Reply reply) {
                 llPopFront(values, prevs, nexts, freeSlots, head, tail, count, ctx, reply);
             })
         && ok;
    ok = registry.bindTypedWithContext<
             void, Ints, Ints, Ints, Ints, std::int64_t, std::int64_t, std::int64_t, Reply>(
             "std::linkedlist::__popBack",
             [](Context &ctx, Ints values, Ints prevs, Ints nexts, Ints freeSlots,
                std::int64_t head, std::int64_t tail, std::int64_t count, Reply reply) {
                 llPopBack(values, prevs, nexts, freeSlots, head, tail, count, ctx, reply);
             })
         && ok;
    ok = registry.bindTypedWithContext<
             void, Ints, Ints, Ints, Ints, std::int64_t, std::int64_t, std::int64_t, std::int64_t, std::int64_t, Reply>(
             "std::linkedlist::__insertBefore",
             [](Context &ctx, Ints values, Ints prevs, Ints nexts, Ints freeSlots,
                std::int64_t head, std::int64_t tail, std::int64_t count, std::int64_t cursor,
                std::int64_t value, Reply reply) {
                 llInsertBefore(values, prevs, nexts, freeSlots, head, tail, count, cursor, value,
                                ctx, reply);
             })
         && ok;
    ok = registry.bindTypedWithContext<
             void, Ints, Ints, Ints, Ints, std::int64_t, std::int64_t, std::int64_t, std::int64_t, std::int64_t, Reply>(
             "std::linkedlist::__insertAfter",
             [](Context &ctx, Ints values, Ints prevs, Ints nexts, Ints freeSlots,
                std::int64_t head, std::int64_t tail, std::int64_t count, std::int64_t cursor,
                std::int64_t value, Reply reply) {
                 llInsertAfter(values, prevs, nexts, freeSlots, head, tail, count, cursor, value,
                               ctx, reply);
             })
         && ok;
    ok = registry.bindTypedWithContext<
             void, Ints, Ints, Ints, Ints, std::int64_t, std::int64_t, std::int64_t, std::int64_t, Reply>(
             "std::linkedlist::__removeAt",
             [](Context &ctx, Ints values, Ints prevs, Ints nexts, Ints freeSlots,
                std::int64_t head, std::int64_t tail, std::int64_t count, std::int64_t cursor,
                Reply reply) {
                 llRemoveAt(values, prevs, nexts, freeSlots, head, tail, count, cursor, ctx, reply);
             })
         && ok;
    ok = registry.bindTypedWithContext<
             void, Ints, Ints, Ints, Ints, std::int64_t, std::int64_t, std::int64_t, Reply>(
             "std::linkedlist::__reverse",
             [](Context &ctx, Ints values, Ints freeSlots, Ints prevs, Ints nexts, std::int64_t head,
                std::int64_t tail, std::int64_t count, Reply reply) {
                 llReverse(values, freeSlots, prevs, nexts, head, tail, count, ctx, reply);
             })
         && ok;
    ok = registry.bindTypedWithContext<void, Ints, Ints, std::int64_t, std::int64_t, Reply>(
             "std::linkedlist::__toArray",
             [](Context &ctx, Ints values, Ints nexts, std::int64_t head, std::int64_t count,
                Reply reply) { llToArray(values, nexts, head, count, ctx, reply); })
         && ok;
    ok = registry.bindTypedWithContext<void, Ints, Ints, std::int64_t, std::int64_t, Reply>(
             "std::linkedlist::__contains",
             [](Context &, Ints values, Ints nexts, std::int64_t head, std::int64_t value,
                Reply reply) { llContains(values, nexts, head, value, reply); })
         && ok;
    if (!ok && error) *error = Memory::String("std::linkedlist 胶水注册失败", std::strlen("std::linkedlist 胶水注册失败"));
    return ok;
}
}  // namespace std_linkedlist_glue

#endif
