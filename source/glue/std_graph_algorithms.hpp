#pragma once
#include "../chali_glue.hpp"
namespace std_graph_glue
{
using Context = chali_glue::Context;
using Ints = Memory::Vector<std::int64_t>;
/**
 * @brief 图算法回执的类型标识，槽布局由标准库 GraphReply 定义。
 */
struct GraphReplyTag
{
};
using GraphReply = chali_glue::Record<GraphReplyTag>;
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
              GraphReply reply);

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
              GraphReply reply);

/**
 * @brief 统计连通分量个数（无向图口径，对有向图按边无向处理）。
 * @param head 每个顶点的首条边下标。
 * @param edgeTo 边终点表。
 * @param edgeNext 同起点下一条边表。
 * @param vertexCount 顶点数。
 * @param reply 回执视图：value 为分量个数。
 */
void graphComponents(const Ints &head, const Ints &edgeTo, const Ints &edgeNext,
                     std::int64_t vertexCount, GraphReply reply);

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
               std::int64_t vertexCount, Context &ctx, GraphReply reply);

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
                   std::int64_t start, Context &ctx, GraphReply reply);

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
                   std::int64_t vertexCount, bool directed, GraphReply reply);

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
                    const Ints &edgeNext, std::int64_t vertexCount, GraphReply reply);
}  // namespace std_graph_glue
