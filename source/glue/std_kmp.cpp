#include "std_kmp.hpp"

#include "../chali_glue.hpp"

#include <cstdint>
#include <cstring>
#include <string>

/**
 * @brief CHALI KMP 字符串查找胶水：失配表、单次查找与全部匹配位置。
 *
 * KmpReply 槽布局与 std::text::KmpReply 一致：
 *   0 error(String)  1 value(下标或 -1)  2 flag  3 table(List<int64>)  4 positions(List<int64>)
 *
 * 输入按字节（UTF-8 串按字节匹配，合法 UTF-8 的针必落在字符边界上）。
 * find 未命中时 value = -1；空针按 std::string::find 口径返回 from。
 */
namespace std_kmp_glue
{
using Context = chali_glue::Context;
using Bytes = Memory::Vector<std::uint8_t>;
using Ints = Memory::Vector<std::int64_t>;

struct ReplyTag
{
};
using Reply = chali_glue::Record<ReplyTag>;

/**
 * @brief 读取传入的字节列表。
 * @param ctx 调用上下文。
 * @param handle 列表句柄。
 * @param out 输出缓冲。
 */
inline void readBytes(Context &ctx, std::int64_t handle, Bytes &out)
{
    Memory::Vector<chali_glue::Value> raw;
    if (!collection_glue::readList(ctx, handle, raw))
    {
        ctx.fail("KMP：字节列表参数无效");
        return;
    }
    out.resize(raw.size());
    for (std::size_t i = 0; i < raw.size(); ++i) out[i] = static_cast<std::uint8_t>(raw[i].asInt() & 0xFF);
}

/**
 * @brief 把 int64 序列写入回执的指定槽。
 * @param ctx 调用上下文。
 * @param reply 回执视图。
 * @param slot 目标槽下标。
 * @param values 源数据。
 */
inline void writeInts(Context &ctx, Reply reply, std::size_t slot, const Ints &values)
{
    const std::int64_t handle = reply.field<std::int64_t>(slot);
    for (std::int64_t value : values) collection_glue::listPush(ctx, handle, chali_glue::Value::fromInt(value));
}

/**
 * @brief 计算 KMP 失配表：table[i] 是 pattern[0..i] 的最长相等真前后缀长度。
 * @param pattern 模式串字节。
 * @param ctx 调用上下文。
 * @param reply 回执视图：table 槽输出失配表（长度与模式串相同）。
 */
inline void kmpTable(const Bytes &pattern, Context &ctx, Reply reply)
{
    Ints table;
    const std::int64_t length = static_cast<std::int64_t>(pattern.size());
    if (length > 0)
    {
        table.resize(static_cast<std::size_t>(length), 0);
        std::int64_t prefix = 0;
        for (std::int64_t i = 1; i < length; ++i)
        {
            while (prefix > 0 && pattern[static_cast<std::size_t>(i)] != pattern[static_cast<std::size_t>(prefix)])
            {
                prefix = table[static_cast<std::size_t>(prefix - 1)];
            }
            if (pattern[static_cast<std::size_t>(i)] == pattern[static_cast<std::size_t>(prefix)]) prefix += 1;
            table[static_cast<std::size_t>(i)] = prefix;
        }
    }
    reply.set<std::int64_t>(1, length);
    reply.set<std::int64_t>(2, 1);
    writeInts(ctx, reply, 3, table);
}

/**
 * @brief 从 from 起用 KMP 找针的首次出现。
 * @param hay 干草堆字节。
 * @param needle 针字节。
 * @param from 起始字节下标。
 * @param ctx 调用上下文。
 * @param reply 回执视图：value 为命中下标，未命中为 -1。
 */
inline void kmpFind(const Bytes &hay, const Bytes &needle, std::int64_t from, Context &ctx,
                    Reply reply)
{
    const std::int64_t hayLen = static_cast<std::int64_t>(hay.size());
    const std::int64_t needleLen = static_cast<std::int64_t>(needle.size());
    if (from < 0 || from > hayLen)
    {
        reply.set<std::int64_t>(1, -1);
        reply.set<std::int64_t>(2, 0);
        return;
    }
    if (needleLen == 0)
    {
        reply.set<std::int64_t>(1, from);
        reply.set<std::int64_t>(2, 1);
        return;
    }
    if (hayLen - from < needleLen)
    {
        reply.set<std::int64_t>(1, -1);
        reply.set<std::int64_t>(2, 0);
        return;
    }
    Ints table;
    table.resize(static_cast<std::size_t>(needleLen), 0);
    std::int64_t prefix = 0;
    for (std::int64_t i = 1; i < needleLen; ++i)
    {
        while (prefix > 0 && needle[static_cast<std::size_t>(i)] != needle[static_cast<std::size_t>(prefix)])
        {
            prefix = table[static_cast<std::size_t>(prefix - 1)];
        }
        if (needle[static_cast<std::size_t>(i)] == needle[static_cast<std::size_t>(prefix)]) prefix += 1;
        table[static_cast<std::size_t>(i)] = prefix;
    }
    std::int64_t matched = 0;
    for (std::int64_t i = from; i < hayLen; ++i)
    {
        while (matched > 0 && hay[static_cast<std::size_t>(i)] != needle[static_cast<std::size_t>(matched)])
        {
            matched = table[static_cast<std::size_t>(matched - 1)];
        }
        if (hay[static_cast<std::size_t>(i)] == needle[static_cast<std::size_t>(matched)]) matched += 1;
        if (matched == needleLen)
        {
            reply.set<std::int64_t>(1, i - needleLen + 1);
            reply.set<std::int64_t>(2, 1);
            return;
        }
    }
    reply.set<std::int64_t>(1, -1);
    reply.set<std::int64_t>(2, 0);
}

/**
 * @brief 找出全部（可重叠）匹配位置。
 * @param hay 干草堆字节。
 * @param needle 针字节。
 * @param ctx 调用上下文。
 * @param reply 回执视图：positions 槽输出按升序的命中下标。
 */
inline void kmpFindAll(const Bytes &hay, const Bytes &needle, Context &ctx, Reply reply)
{
    Ints positions;
    const std::int64_t hayLen = static_cast<std::int64_t>(hay.size());
    const std::int64_t needleLen = static_cast<std::int64_t>(needle.size());
    if (needleLen == 0 || hayLen >= needleLen)
    {
        if (needleLen > 0)
        {
            Ints table;
            table.resize(static_cast<std::size_t>(needleLen), 0);
            std::int64_t prefix = 0;
            for (std::int64_t i = 1; i < needleLen; ++i)
            {
                while (prefix > 0 && needle[static_cast<std::size_t>(i)] != needle[static_cast<std::size_t>(prefix)])
                {
                    prefix = table[static_cast<std::size_t>(prefix - 1)];
                }
                if (needle[static_cast<std::size_t>(i)] == needle[static_cast<std::size_t>(prefix)]) prefix += 1;
                table[static_cast<std::size_t>(i)] = prefix;
            }
            std::int64_t matched = 0;
            for (std::int64_t i = 0; i < hayLen; ++i)
            {
                while (matched > 0 && hay[static_cast<std::size_t>(i)] != needle[static_cast<std::size_t>(matched)])
                {
                    matched = table[static_cast<std::size_t>(matched - 1)];
                }
                if (hay[static_cast<std::size_t>(i)] == needle[static_cast<std::size_t>(matched)]) matched += 1;
                if (matched == needleLen)
                {
                    positions.push_back(i - needleLen + 1);
                    matched = table[static_cast<std::size_t>(matched - 1)];
                }
            }
        }
    }
    reply.set<std::int64_t>(1, static_cast<std::int64_t>(positions.size()));
    reply.set<std::int64_t>(2, 1);
    writeInts(ctx, reply, 4, positions);
}

/**
 * @brief 注册 std::text 的 KMP 胶水入口。
 * @param registry 函数注册表。
 * @param error 注册失败时的错误说明。
 * @return 全部注册成功返回 true。
 */
bool bind(chali_glue::Registry &registry, Memory::String *error)
{
    bool ok = true;
    ok = registry.bindTypedWithContext<void, Bytes, Reply>(
             "std::text::__kmpTable",
             [](Context &ctx, Bytes pattern, Reply reply) { kmpTable(pattern, ctx, reply); })
         && ok;
    ok = registry.bindTypedWithContext<void, Bytes, Bytes, std::int64_t, Reply>(
             "std::text::__kmpFind",
             [](Context &ctx, Bytes hay, Bytes needle, std::int64_t from, Reply reply) {
                 kmpFind(hay, needle, from, ctx, reply);
             })
         && ok;
    ok = registry.bindTypedWithContext<void, Bytes, Bytes, Reply>(
             "std::text::__kmpFindAll",
             [](Context &ctx, Bytes hay, Bytes needle, Reply reply) {
                 kmpFindAll(hay, needle, ctx, reply);
             })
         && ok;
    if (!ok && error) *error = Memory::String("std::text KMP 胶水注册失败", std::strlen("std::text KMP 胶水注册失败"));
    return ok;
}
}  // namespace std_kmp_glue
