#ifndef PARSE_ENGINE_STD_GRAPH_HPP
#define PARSE_ENGINE_STD_GRAPH_HPP

#include "../magel_glue.hpp"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <string>

/**
 * @brief MAGEL 图论胶水：BFS / DFS / 连通分量 / 拓扑排序 / Dijkstra /
 *        环检测 / Kruskal 最小生成树与并查集的全部算法实现。
 *
 * 图采用"链式前向星"邻接存储：head[v] 是顶点 v 的第一条边下标（-1 表示没有），
 * edgeTo[e] / edgeNext[e] / edgeWeight[e] 分别是边 e 的终点、同起点下一条边与边权。
 * 顶点编号 0..n-1，全部为 int64。
 *
 * GraphReply 槽布局与 std::graph::GraphReply 一致：
 *   0 error(String)  1 value  2 flag  3 order(List<int64>)  4 dist(List<int64>)
 * DsuReply 槽布局与 std::graph::DsuReply 一致：
 *   0 parent(List<int64>)  1 sizes(List<int64>)  2 root  3 flag
 *
 * Dijkstra 输出距离表，不可达顶点为 -1；要求边权非负。
 * 拓扑排序输出一个合法序列，有环时 order 为空、flag = 0。
 * 最小生成树只对无向图有意义，图不连通时返回 -1。
 */
namespace std_graph_glue
{
using Context = magel_glue::Context;
using Ints = Memory::Vector<std::int64_t>;

struct GraphReplyTag
{
};
using GraphReply = magel_glue::Record<GraphReplyTag>;

struct DsuReplyTag
{
};
using DsuReply = magel_glue::Record<DsuReplyTag>;

/**
 * @brief 读取传入的 int64 列表。
 * @param ctx 调用上下文。
 * @param handle 列表句柄。
 * @param out 输出缓冲。
 */
inline void readInts(Context &ctx, std::int64_t handle, Ints &out)
{
    Memory::Vector<magel_glue::Value> raw;
    if (!ctx.readList(handle, raw))
    {
        ctx.fail("图论：列表参数无效");
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
inline void writeInts(Context &ctx, GraphReply reply, std::size_t slot, const Ints &values)
{
    const std::int64_t handle = reply.field<std::int64_t>(slot);
    for (std::int64_t value : values) ctx.listPush(handle, magel_glue::Value::fromInt(value));
}

/**
 * @brief 广度优先搜索，输出从 start 可达顶点的访问序。
 * @param head 每个顶点的首条边下标。
 * @param edgeTo 边终点表。
 * @param edgeNext 同起点下一条边表。
 * @param vertexCount 顶点数。
 * @param start 起点下标。
 * @param ctx 调用上下文。
 * @param reply 回执视图：order 槽输出访问序。
 */
inline void graphBfs(const Ints &head, const Ints &edgeTo, const Ints &edgeNext,
                     std::int64_t vertexCount, std::int64_t start, Context &ctx,
                     GraphReply reply)
{
    Ints order;
    Ints visited(static_cast<std::size_t>(vertexCount), 0);
    Ints queue;
    if (start >= 0 && start < vertexCount)
    {
        queue.push_back(start);
        visited[static_cast<std::size_t>(start)] = 1;
    }
    for (std::size_t cursor = 0; cursor < queue.size(); ++cursor)
    {
        const std::int64_t node = queue[cursor];
        order.push_back(node);
        for (std::int64_t edge = head[static_cast<std::size_t>(node)]; edge >= 0;
             edge = edgeNext[static_cast<std::size_t>(edge)])
        {
            const std::int64_t next = edgeTo[static_cast<std::size_t>(edge)];
            if (!visited[static_cast<std::size_t>(next)])
            {
                visited[static_cast<std::size_t>(next)] = 1;
                queue.push_back(next);
            }
        }
    }
    writeInts(ctx, reply, 3, order);
}

/**
 * @brief 深度优先搜索（显式栈，邻接按插入序访问），输出访问序。
 * @param head 每个顶点的首条边下标。
 * @param edgeTo 边终点表。
 * @param edgeNext 同起点下一条边表。
 * @param vertexCount 顶点数。
 * @param start 起点下标。
 * @param ctx 调用上下文。
 * @param reply 回执视图：order 槽输出访问序。
 */
inline void graphDfs(const Ints &head, const Ints &edgeTo, const Ints &edgeNext,
                     std::int64_t vertexCount, std::int64_t start, Context &ctx,
                     GraphReply reply)
{
    Ints order;
    Ints visited(static_cast<std::size_t>(vertexCount), 0);
    Ints stack;
    if (start >= 0 && start < vertexCount) stack.push_back(start);
    while (!stack.empty())
    {
        const std::int64_t node = stack.back();
        stack.pop_back();
        if (visited[static_cast<std::size_t>(node)]) continue;
        visited[static_cast<std::size_t>(node)] = 1;
        order.push_back(node);
        // 逆序压栈，保证先访问先插入的邻居。
        Ints neighbors;
        for (std::int64_t edge = head[static_cast<std::size_t>(node)]; edge >= 0;
             edge = edgeNext[static_cast<std::size_t>(edge)])
        {
            neighbors.push_back(edgeTo[static_cast<std::size_t>(edge)]);
        }
        for (std::size_t i = neighbors.size(); i > 0; --i)
        {
            const std::int64_t next = neighbors[i - 1];
            if (!visited[static_cast<std::size_t>(next)]) stack.push_back(next);
        }
    }
    writeInts(ctx, reply, 3, order);
}

/**
 * @brief 统计连通分量个数（无向图口径，对有向图按边无向处理）。
 * @param head 每个顶点的首条边下标。
 * @param edgeTo 边终点表。
 * @param edgeNext 同起点下一条边表。
 * @param vertexCount 顶点数。
 * @param reply 回执视图：value 为分量个数。
 */
inline void graphComponents(const Ints &head, const Ints &edgeTo, const Ints &edgeNext,
                            std::int64_t vertexCount, GraphReply reply)
{
    Ints visited(static_cast<std::size_t>(vertexCount), 0);
    Ints queue;
    std::int64_t components = 0;
    for (std::int64_t seed = 0; seed < vertexCount; ++seed)
    {
        if (visited[static_cast<std::size_t>(seed)]) continue;
        components = components + 1;
        queue.push_back(seed);
        visited[static_cast<std::size_t>(seed)] = 1;
        for (std::size_t cursor = 0; cursor < queue.size(); ++cursor)
        {
            const std::int64_t node = queue[cursor];
            for (std::int64_t edge = head[static_cast<std::size_t>(node)]; edge >= 0;
                 edge = edgeNext[static_cast<std::size_t>(edge)])
            {
                const std::int64_t next = edgeTo[static_cast<std::size_t>(edge)];
                if (!visited[static_cast<std::size_t>(next)])
                {
                    visited[static_cast<std::size_t>(next)] = 1;
                    queue.push_back(next);
                }
            }
        }
        queue.clear();
    }
    reply.set<std::int64_t>(1, components);
}

/**
 * @brief Kahn 拓扑排序。
 * @param head 每个顶点的首条边下标。
 * @param edgeTo 边终点表。
 * @param edgeNext 同起点下一条边表。
 * @param vertexCount 顶点数。
 * @param ctx 调用上下文。
 * @param reply 回执视图：flag = 1 表示无环，order 槽输出序列（有环时为空）。
 */
inline void graphTopo(const Ints &head, const Ints &edgeTo, const Ints &edgeNext,
                      std::int64_t vertexCount, Context &ctx, GraphReply reply)
{
    Ints indegree(static_cast<std::size_t>(vertexCount), 0);
    for (std::int64_t edge = 0; edge < static_cast<std::int64_t>(edgeTo.size()); ++edge)
    {
        indegree[static_cast<std::size_t>(edgeTo[static_cast<std::size_t>(edge)])] += 1;
    }
    Ints order;
    Ints queue;
    for (std::int64_t node = 0; node < vertexCount; ++node)
    {
        if (indegree[static_cast<std::size_t>(node)] == 0) queue.push_back(node);
    }
    for (std::size_t cursor = 0; cursor < queue.size(); ++cursor)
    {
        const std::int64_t node = queue[cursor];
        order.push_back(node);
        for (std::int64_t edge = head[static_cast<std::size_t>(node)]; edge >= 0;
             edge = edgeNext[static_cast<std::size_t>(edge)])
        {
            const std::int64_t next = edgeTo[static_cast<std::size_t>(edge)];
            indegree[static_cast<std::size_t>(next)] -= 1;
            if (indegree[static_cast<std::size_t>(next)] == 0) queue.push_back(next);
        }
    }
    const bool acyclic = static_cast<std::int64_t>(order.size()) == vertexCount;
    reply.set<std::int64_t>(2, acyclic ? 1 : 0);
    writeInts(ctx, reply, 3, order);
}

/**
 * @brief Dijkstra 单源最短路（O(n²) 选择版，要求边权非负）。
 * @param head 每个顶点的首条边下标。
 * @param edgeTo 边终点表。
 * @param edgeWeight 边权表。
 * @param edgeNext 同起点下一条边表。
 * @param vertexCount 顶点数。
 * @param start 源点下标。
 * @param ctx 调用上下文。
 * @param reply 回执视图：dist 槽输出距离表，不可达为 -1。
 */
inline void graphDijkstra(const Ints &head, const Ints &edgeTo, const Ints &edgeWeight,
                          const Ints &edgeNext, std::int64_t vertexCount,
                          std::int64_t start, Context &ctx, GraphReply reply)
{
    for (std::int64_t edge = 0; edge < static_cast<std::int64_t>(edgeWeight.size()); ++edge)
    {
        if (edgeWeight[static_cast<std::size_t>(edge)] < 0)
        {
            ctx.fail("图论：Dijkstra 不支持负边权");
            return;
        }
    }
    Ints dist(static_cast<std::size_t>(vertexCount), -1);
    Ints done(static_cast<std::size_t>(vertexCount), 0);
    if (start >= 0 && start < vertexCount) dist[static_cast<std::size_t>(start)] = 0;
    for (std::int64_t round = 0; round < vertexCount; ++round)
    {
        std::int64_t best = -1;
        std::int64_t bestDist = 0;
        for (std::int64_t node = 0; node < vertexCount; ++node)
        {
            if (done[static_cast<std::size_t>(node)]) continue;
            const std::int64_t value = dist[static_cast<std::size_t>(node)];
            if (value < 0) continue;
            if (best < 0 || value < bestDist)
            {
                best = node;
                bestDist = value;
            }
        }
        if (best < 0) break;
        done[static_cast<std::size_t>(best)] = 1;
        for (std::int64_t edge = head[static_cast<std::size_t>(best)]; edge >= 0;
             edge = edgeNext[static_cast<std::size_t>(edge)])
        {
            const std::int64_t next = edgeTo[static_cast<std::size_t>(edge)];
            if (done[static_cast<std::size_t>(next)]) continue;
            const std::int64_t candidate = bestDist + edgeWeight[static_cast<std::size_t>(edge)];
            const std::int64_t current = dist[static_cast<std::size_t>(next)];
            if (current < 0 || candidate < current) dist[static_cast<std::size_t>(next)] = candidate;
        }
    }
    writeInts(ctx, reply, 4, dist);
}

/**
 * @brief 环检测：有向图用 Kahn 计数，无向图用并查集判自环/双边。
 * @param head 每个顶点的首条边下标。
 * @param edgeTo 边终点表。
 * @param edgeNext 同起点下一条边表。
 * @param vertexCount 顶点数。
 * @param directed 是否有向。
 * @param reply 回执视图：flag = 1 表示有环。
 */
inline void graphHasCycle(const Ints &head, const Ints &edgeTo, const Ints &edgeNext,
                          std::int64_t vertexCount, bool directed, GraphReply reply)
{
    if (directed)
    {
        Ints indegree(static_cast<std::size_t>(vertexCount), 0);
        for (std::int64_t edge = 0; edge < static_cast<std::int64_t>(edgeTo.size()); ++edge)
        {
            indegree[static_cast<std::size_t>(edgeTo[static_cast<std::size_t>(edge)])] += 1;
        }
        Ints queue;
        for (std::int64_t node = 0; node < vertexCount; ++node)
        {
            if (indegree[static_cast<std::size_t>(node)] == 0) queue.push_back(node);
        }
        std::int64_t seen = 0;
        for (std::size_t cursor = 0; cursor < queue.size(); ++cursor)
        {
            const std::int64_t node = queue[cursor];
            seen += 1;
            for (std::int64_t edge = head[static_cast<std::size_t>(node)]; edge >= 0;
                 edge = edgeNext[static_cast<std::size_t>(edge)])
            {
                const std::int64_t next = edgeTo[static_cast<std::size_t>(edge)];
                indegree[static_cast<std::size_t>(next)] -= 1;
                if (indegree[static_cast<std::size_t>(next)] == 0) queue.push_back(next);
            }
        }
        reply.set<std::int64_t>(2, seen < vertexCount ? 1 : 0);
        return;
    }
    // 无向图：并查集，出现两端已连通的边即有环。
    Ints parent(static_cast<std::size_t>(vertexCount), 0);
    for (std::int64_t node = 0; node < vertexCount; ++node) parent[static_cast<std::size_t>(node)] = node;
    std::int64_t findRoot = 0;
    Ints path;
    bool hasCycle = false;
    for (std::int64_t node = 0; node < vertexCount && !hasCycle; ++node)
    {
        for (std::int64_t edge = head[static_cast<std::size_t>(node)]; edge >= 0 && !hasCycle;
             edge = edgeNext[static_cast<std::size_t>(edge)])
        {
            const std::int64_t other = edgeTo[static_cast<std::size_t>(edge)];
            if (other < node) continue; // 无向边只处理一次。
            // find(node)
            findRoot = node;
            path.clear();
            while (parent[static_cast<std::size_t>(findRoot)] != findRoot)
            {
                path.push_back(findRoot);
                findRoot = parent[static_cast<std::size_t>(findRoot)];
            }
            for (std::int64_t hop : path) parent[static_cast<std::size_t>(hop)] = findRoot;
            std::int64_t rootA = findRoot;
            // find(other)
            findRoot = other;
            path.clear();
            while (parent[static_cast<std::size_t>(findRoot)] != findRoot)
            {
                path.push_back(findRoot);
                findRoot = parent[static_cast<std::size_t>(findRoot)];
            }
            for (std::int64_t hop : path) parent[static_cast<std::size_t>(hop)] = findRoot;
            const std::int64_t rootB = findRoot;
            if (rootA == rootB) hasCycle = true;
            else parent[static_cast<std::size_t>(rootA)] = rootB;
        }
    }
    reply.set<std::int64_t>(2, hasCycle ? 1 : 0);
}

/**
 * @brief Kruskal 最小生成树总权值（无向图；边权排序用插入排序）。
 * @param head 每个顶点的首条边下标。
 * @param edgeTo 边终点表。
 * @param edgeWeight 边权表。
 * @param edgeNext 同起点下一条边表。
 * @param vertexCount 顶点数。
 * @param reply 回执视图：value 为生成树总权值，不连通时为 -1。
 */
inline void graphMstWeight(const Ints &head, const Ints &edgeTo, const Ints &edgeWeight,
                           const Ints &edgeNext, std::int64_t vertexCount, GraphReply reply)
{
    // 收集边（无向图每条只收一次：u < v），三元组平行数组。
    Ints edgeU;
    Ints edgeV;
    Ints edgeW;
    for (std::int64_t node = 0; node < vertexCount; ++node)
    {
        for (std::int64_t edge = head[static_cast<std::size_t>(node)]; edge >= 0;
             edge = edgeNext[static_cast<std::size_t>(edge)])
        {
            const std::int64_t other = edgeTo[static_cast<std::size_t>(edge)];
            if (other == node || other < node) continue;
            edgeU.push_back(node);
            edgeV.push_back(other);
            edgeW.push_back(edgeWeight[static_cast<std::size_t>(edge)]);
        }
    }
    // 插入排序按下标重排（按权升序）。
    const std::int64_t total = static_cast<std::int64_t>(edgeU.size());
    for (std::int64_t i = 1; i < total; ++i)
    {
        const std::int64_t u = edgeU[static_cast<std::size_t>(i)];
        const std::int64_t v = edgeV[static_cast<std::size_t>(i)];
        const std::int64_t w = edgeW[static_cast<std::size_t>(i)];
        std::int64_t j = i - 1;
        while (j >= 0 && edgeW[static_cast<std::size_t>(j)] > w)
        {
            edgeU[static_cast<std::size_t>(j + 1)] = edgeU[static_cast<std::size_t>(j)];
            edgeV[static_cast<std::size_t>(j + 1)] = edgeV[static_cast<std::size_t>(j)];
            edgeW[static_cast<std::size_t>(j + 1)] = edgeW[static_cast<std::size_t>(j)];
            j = j - 1;
        }
        edgeU[static_cast<std::size_t>(j + 1)] = u;
        edgeV[static_cast<std::size_t>(j + 1)] = v;
        edgeW[static_cast<std::size_t>(j + 1)] = w;
    }
    // 并查集选边。
    Ints parent(static_cast<std::size_t>(vertexCount), 0);
    Ints sizes(static_cast<std::size_t>(vertexCount), 1);
    for (std::int64_t node = 0; node < vertexCount; ++node) parent[static_cast<std::size_t>(node)] = node;
    Ints path;
    std::int64_t totalWeight = 0;
    std::int64_t used = 0;
    for (std::int64_t i = 0; i < total && used + 1 < vertexCount; ++i)
    {
        std::int64_t rootA = edgeU[static_cast<std::size_t>(i)];
        path.clear();
        while (parent[static_cast<std::size_t>(rootA)] != rootA)
        {
            path.push_back(rootA);
            rootA = parent[static_cast<std::size_t>(rootA)];
        }
        for (std::int64_t hop : path) parent[static_cast<std::size_t>(hop)] = rootA;
        std::int64_t rootB = edgeV[static_cast<std::size_t>(i)];
        path.clear();
        while (parent[static_cast<std::size_t>(rootB)] != rootB)
        {
            path.push_back(rootB);
            rootB = parent[static_cast<std::size_t>(rootB)];
        }
        for (std::int64_t hop : path) parent[static_cast<std::size_t>(hop)] = rootB;
        if (rootA == rootB) continue;
        if (sizes[static_cast<std::size_t>(rootA)] < sizes[static_cast<std::size_t>(rootB)])
        {
            const std::int64_t swap = rootA;
            rootA = rootB;
            rootB = swap;
        }
        parent[static_cast<std::size_t>(rootB)] = rootA;
        sizes[static_cast<std::size_t>(rootA)] += sizes[static_cast<std::size_t>(rootB)];
        totalWeight += edgeW[static_cast<std::size_t>(i)];
        used += 1;
    }
    reply.set<std::int64_t>(1, used + 1 == vertexCount ? totalWeight : -1);
}

/**
 * @brief 并查集查找（带路径压缩）。
 * @param parent 父指针数组。
 * @param x 目标元素。
 * @param ctx 调用上下文。
 * @param reply 回执视图：root 为根，parent 槽输出压缩后的父指针数组。
 */
inline void dsuFind(Ints &parent, std::int64_t x, Context &ctx, DsuReply reply)
{
    Ints compressed = parent;
    if (x < 0 || x >= static_cast<std::int64_t>(compressed.size()))
    {
        ctx.fail("图论：并查集下标越界");
        return;
    }
    std::int64_t root = x;
    Ints path;
    while (compressed[static_cast<std::size_t>(root)] != root)
    {
        path.push_back(root);
        root = compressed[static_cast<std::size_t>(root)];
    }
    for (std::int64_t hop : path) compressed[static_cast<std::size_t>(hop)] = root;
    reply.set<std::int64_t>(2, root);
    reply.set<std::int64_t>(3, 1);
    const std::int64_t parentHandle = reply.field<std::int64_t>(0);
    for (std::int64_t value : compressed) ctx.listPush(parentHandle, magel_glue::Value::fromInt(value));
}

/**
 * @brief 并查集按大小合并。
 * @param parent 父指针数组。
 * @param sizes 大小数组。
 * @param a 元素一。
 * @param b 元素二。
 * @param ctx 调用上下文。
 * @param reply 回执视图：flag = 1 表示发生了合并，parent / sizes 槽输出更新后的数组，
 *               root 为合并后的根。
 */
inline void dsuUnion(Ints &parent, Ints &sizes, std::int64_t a, std::int64_t b,
                     Context &ctx, DsuReply reply)
{
    const std::int64_t count = static_cast<std::int64_t>(parent.size());
    if (a < 0 || a >= count || b < 0 || b >= count)
    {
        ctx.fail("图论：并查集下标越界");
        return;
    }
    Ints compressed = parent;
    std::int64_t rootA = a;
    Ints path;
    while (compressed[static_cast<std::size_t>(rootA)] != rootA)
    {
        path.push_back(rootA);
        rootA = compressed[static_cast<std::size_t>(rootA)];
    }
    for (std::int64_t hop : path) compressed[static_cast<std::size_t>(hop)] = rootA;
    std::int64_t rootB = b;
    path.clear();
    while (compressed[static_cast<std::size_t>(rootB)] != rootB)
    {
        path.push_back(rootB);
        rootB = compressed[static_cast<std::size_t>(rootB)];
    }
    for (std::int64_t hop : path) compressed[static_cast<std::size_t>(hop)] = rootB;
    if (rootA == rootB)
    {
        reply.set<std::int64_t>(2, rootA);
        reply.set<std::int64_t>(3, 0);
        const std::int64_t parentHandle = reply.field<std::int64_t>(0);
        for (std::int64_t value : compressed) ctx.listPush(parentHandle, magel_glue::Value::fromInt(value));
        const std::int64_t sizesHandle = reply.field<std::int64_t>(1);
        for (std::int64_t value : sizes) ctx.listPush(sizesHandle, magel_glue::Value::fromInt(value));
        return;
    }
    if (sizes[static_cast<std::size_t>(rootA)] < sizes[static_cast<std::size_t>(rootB)])
    {
        const std::int64_t swap = rootA;
        rootA = rootB;
        rootB = swap;
    }
    compressed[static_cast<std::size_t>(rootB)] = rootA;
    sizes[static_cast<std::size_t>(rootA)] += sizes[static_cast<std::size_t>(rootB)];
    reply.set<std::int64_t>(2, rootA);
    reply.set<std::int64_t>(3, 1);
    const std::int64_t parentHandle = reply.field<std::int64_t>(0);
    for (std::int64_t value : compressed) ctx.listPush(parentHandle, magel_glue::Value::fromInt(value));
    const std::int64_t sizesHandle = reply.field<std::int64_t>(1);
    for (std::int64_t value : sizes) ctx.listPush(sizesHandle, magel_glue::Value::fromInt(value));
}

/**
 * @brief 注册 std::graph 的全部胶水入口。
 * @param registry 函数注册表。
 * @param error 注册失败时的错误说明。
 * @return 全部注册成功返回 true。
 */
inline bool bind(magel_glue::Registry &registry, Memory::String *error)
{
    bool ok = true;
    ok = registry.bindTypedWithContext<void, Ints, Ints, Ints, std::int64_t, std::int64_t, GraphReply>(
        "std::graph::__bfs",
        [](Context &ctx, Ints head, Ints to, Ints next, std::int64_t n, std::int64_t start,
           GraphReply reply) { graphBfs(head, to, next, n, start, ctx, reply); }) && ok;
    ok = registry.bindTypedWithContext<void, Ints, Ints, Ints, std::int64_t, std::int64_t, GraphReply>(
        "std::graph::__dfs",
        [](Context &ctx, Ints head, Ints to, Ints next, std::int64_t n, std::int64_t start,
           GraphReply reply) { graphDfs(head, to, next, n, start, ctx, reply); }) && ok;
    ok = registry.bindTypedWithContext<void, Ints, Ints, Ints, std::int64_t, GraphReply>(
        "std::graph::__components",
        [](Context &, Ints head, Ints to, Ints next, std::int64_t n, GraphReply reply) {
            graphComponents(head, to, next, n, reply);
        }) && ok;
    ok = registry.bindTypedWithContext<void, Ints, Ints, Ints, std::int64_t, GraphReply>(
        "std::graph::__topo",
        [](Context &ctx, Ints head, Ints to, Ints next, std::int64_t n, GraphReply reply) {
            graphTopo(head, to, next, n, ctx, reply);
        }) && ok;
    ok = registry.bindTypedWithContext<void, Ints, Ints, Ints, Ints, std::int64_t, std::int64_t, GraphReply>(
        "std::graph::__dijkstra",
        [](Context &ctx, Ints head, Ints to, Ints weight, Ints next, std::int64_t n,
           std::int64_t start, GraphReply reply) {
            graphDijkstra(head, to, weight, next, n, start, ctx, reply);
        }) && ok;
    ok = registry.bindTypedWithContext<void, Ints, Ints, Ints, std::int64_t, std::int64_t, GraphReply>(
        "std::graph::__hasCycle",
        [](Context &, Ints head, Ints to, Ints next, std::int64_t n, std::int64_t directed,
           GraphReply reply) { graphHasCycle(head, to, next, n, directed != 0, reply); }) && ok;
    ok = registry.bindTypedWithContext<void, Ints, Ints, Ints, Ints, std::int64_t, GraphReply>(
        "std::graph::__mstWeight",
        [](Context &, Ints head, Ints to, Ints weight, Ints next, std::int64_t n,
           GraphReply reply) { graphMstWeight(head, to, weight, next, n, reply); }) && ok;
    ok = registry.bindTypedWithContext<void, Ints, std::int64_t, DsuReply>(
        "std::graph::__dsuFind",
        [](Context &ctx, Ints parent, std::int64_t x, DsuReply reply) {
            dsuFind(parent, x, ctx, reply);
        }) && ok;
    ok = registry.bindTypedWithContext<void, Ints, Ints, std::int64_t, std::int64_t, DsuReply>(
        "std::graph::__dsuUnion",
        [](Context &ctx, Ints parent, Ints sizes, std::int64_t a, std::int64_t b,
           DsuReply reply) { dsuUnion(parent, sizes, a, b, ctx, reply); }) && ok;
    if (!ok && error) *error = Memory::String("std::graph 胶水注册失败", std::strlen("std::graph 胶水注册失败"));
    return ok;
}
} // namespace std_graph_glue

#endif
