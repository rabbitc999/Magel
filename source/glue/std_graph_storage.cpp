#include "std_graph.hpp"
#include "std_graph_algorithms.hpp"
#include "std_union_find.hpp"
#include "std_native_record.hpp"
#include <algorithm>
#include <memory>
#include <stdexcept>
namespace std_graph_glue
{
/**
 * @brief 拥有邻接数组和图属性的独立持久状态。
 */
struct GraphState
{
    Ints head, to, next, weight;
    std::int64_t edges = 0;
    bool directed = false;
};
/**
 * @brief 深复制全部邻接数组。
 * @param source 原图状态。
 * @param error 本操作不产生可恢复诊断。
 * @return 独立的新图状态。
 */
void *cloneGraph(const void *source, Memory::String &error)
{
    (void)error;
    return new GraphState(*static_cast<const GraphState *>(source));
}
/**
 * @brief 释放图及其邻接数组。
 * @param value 待释放的图状态。
 */
void destroyGraph(void *value) { delete static_cast<GraphState *>(value); }
const stackvm::NativeRecordType graphType{ "std::graph::Graph", cloneGraph, destroyGraph, nullptr };
/**
 * @brief 建立空图或指定顶点数的图，并转移给 VM 管理。
 * @param state 当前调用状态。
 * @param arguments 可选的顶点数和有向标志。
 * @return 拥有图状态的原生记录。
 */
native_record_glue::Value createGraph(native_record_glue::State &state, const Memory::Vector<native_record_glue::Value> &arguments)
{
    auto count = arguments.empty() ? 0 : arguments[0].asInt();
    if (count < 0 || count > 16777216) throw std::out_of_range("图顶点数必须在 0..16777216 之间");
    auto value = std::make_unique<GraphState>();
    value->head.assign(static_cast<std::size_t>(count), -1);
    value->directed = !arguments.empty() && arguments[1].asInt();
    auto handle = state.host.makeNativeRecord(graphType, value.get());
    if (handle) value.release();
    return native_record_glue::Value::fromInt(handle);
}
/**
 * @brief 追加单条邻接记录，不导出或复制图数组。
 * @param graph 原地更新的图。
 * @param from 起点。
 * @param to 终点。
 * @param weight 边权。
 */
void appendEdge(GraphState &graph, std::int64_t from, std::int64_t to, std::int64_t weight)
{
    graph.to.push_back(to);
    graph.next.push_back(graph.head[static_cast<std::size_t>(from)]);
    graph.weight.push_back(weight);
    graph.head[static_cast<std::size_t>(from)] = static_cast<std::int64_t>(graph.to.size()) - 1;
}
/**
 * @brief 注册持久图、并查集与图算法入口。
 * @param registry 当前宿主注册表。
 * @param error 接收绑定失败的诊断。
 * @return 所有入口绑定成功时返回 true。
 */
bool bind(chali_glue::Registry &registry, Memory::String *error)
{
    using namespace native_record_glue;
    auto &native = registry.native();
    bool ok = std_union_find_glue::bind(registry, error)
              && native_record_glue::bind(native, "std::graph::__graphNew", 0, createGraph, error)
              && native_record_glue::bind(native, "std::graph::__graphCreate", 2, createGraph, error)
              && native_record_glue::bind(native, "std::graph::__graphCopy", 1, copy, error)
              && native_record_glue::bind(native, "std::graph::__graphDrop", 1, drop, error);
    ok = registry.bindTypedWithContext<std::int64_t, std::int64_t, std::int64_t>("std::graph::__graphState", [](Context &ctx, std::int64_t handle, std::int64_t operation) {
        auto *g = static_cast<GraphState *>(ctx.nativeRecord(handle, graphType));
        if (!g) return std::int64_t{0};
        return operation == 0 ? static_cast<std::int64_t>(g->head.size()) : operation == 1 ? static_cast<std::int64_t>(g->directed) : g->edges; }, error) && ok;
    ok = registry.bindTypedWithContext<void, std::int64_t, std::int64_t, std::int64_t, std::int64_t>("std::graph::__graphAdd", [](Context &ctx, std::int64_t handle, std::int64_t from, std::int64_t to, std::int64_t weight) {
        auto *g = static_cast<GraphState *>(ctx.nativeRecord(handle, graphType));
        if (!g) return;
        if (from < 0 || to < 0 || static_cast<std::uint64_t>(from) >= g->head.size() || static_cast<std::uint64_t>(to) >= g->head.size()) throw std::out_of_range("图顶点下标越界");
        const auto needed = g->to.size() + (!g->directed && from != to ? 2 : 1);
        if (needed > g->to.capacity())
        {
            const auto capacity = std::max(needed, g->to.capacity() * 2);
            g->to.reserve(capacity); g->next.reserve(capacity); g->weight.reserve(capacity);
        }
        appendEdge(*g, from, to, weight);
        if (!g->directed && from != to) appendEdge(*g, to, from, weight);
        ++g->edges; }, error) && ok;
    ok = registry.bindTypedWithContext<void, std::int64_t, std::int64_t, std::int64_t, GraphReply>("std::graph::__graphQuery", [](Context &ctx, std::int64_t handle, std::int64_t operation, std::int64_t start, GraphReply reply) {
        auto *g = static_cast<GraphState *>(ctx.nativeRecord(handle, graphType));
        if (!g) return;
        auto count = static_cast<std::int64_t>(g->head.size());
        switch (operation)
        {
        case 0: graphBfs(g->head, g->to, g->next, count, start, ctx, reply); break;
        case 1: graphDfs(g->head, g->to, g->next, count, start, ctx, reply); break;
        case 2: graphComponents(g->head, g->to, g->next, count, reply); break;
        case 3: graphTopo(g->head, g->to, g->next, count, ctx, reply); break;
        case 4: graphDijkstra(g->head, g->to, g->weight, g->next, count, start, ctx, reply); break;
        case 5: graphHasCycle(g->head, g->to, g->next, count, g->directed, reply); break;
        case 6: graphMstWeight(g->head, g->to, g->weight, g->next, count, reply); break;
        default: throw std::invalid_argument("图算法编号无效");
        } }, error) && ok;
    return registry.bindTypedWithContext<Ints, std::int64_t, std::int64_t>("std::graph::__graphSnapshot", [](Context &ctx, std::int64_t handle, std::int64_t part) {
        auto *g = static_cast<GraphState *>(ctx.nativeRecord(handle, graphType));
        if (!g) return Ints{};
        return part == 0 ? g->head : part == 1 ? g->to : part == 2 ? g->next : g->weight; }, error) && ok;
}
}  // namespace std_graph_glue
