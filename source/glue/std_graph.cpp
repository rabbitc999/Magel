#include "std_graph_algorithms.hpp"
#include <algorithm>
#include <cstdint>
namespace std_graph_glue
{
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
    for (std::int64_t value : values) collection_glue::listPush(ctx, handle, chali_glue::Value::fromInt(value));
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
void graphBfs(const Ints &head, const Ints &edgeTo, const Ints &edgeNext,
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
void graphDfs(const Ints &head, const Ints &edgeTo, const Ints &edgeNext,
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
void graphComponents(const Ints &head, const Ints &edgeTo, const Ints &edgeNext,
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
void graphTopo(const Ints &head, const Ints &edgeTo, const Ints &edgeNext,
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
void graphDijkstra(const Ints &head, const Ints &edgeTo, const Ints &edgeWeight,
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
void graphHasCycle(const Ints &head, const Ints &edgeTo, const Ints &edgeNext,
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
            if (other < node) continue;  // 无向边只处理一次。
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
            if (rootA == rootB)
                hasCycle = true;
            else
                parent[static_cast<std::size_t>(rootA)] = rootB;
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
void graphMstWeight(const Ints &head, const Ints &edgeTo, const Ints &edgeWeight,
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

}  // namespace std_graph_glue
