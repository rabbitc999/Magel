# std::graph — 图论与并查集

[返回库目录](README.md) · [全部符号](symbols.md)

## 引入

```chali
import std::graph;
```

## 公共约定与错误边界

Graph 使用链式前向星存储：head 是每顶点首条边，edgeTo/edgeNext/edgeWeight 是边终点、同起点下一条边与边权，顶点编号 0..vertexCount-1，重边与自环都允许。有向 / 无向由 create 的 directed 决定，无向图加边时自动镜像登记。bfs/dfs 输出可达顶点的访问序；topologicalSort 用 Kahn 算法，有环时输出空表；dijkstra 要求边权非负，不可达顶点距离为 -1；hasCycle 有向图用 Kahn 计数、无向图用并查集；minimumSpanningTreeWeight 用 Kruskal，图不连通返回 -1。Graph 与 UnionFind 使用独立拥有型 opcode，普通复制深复制；Graph.snapshot 和 UnionFind.parents/sizes 显式导出独立快照。UnionFind 路径压缩与按大小合并均原地执行，不随每次查询复制容量数组。全部算法由宿主胶水 glue/std_graph.hpp 提供，裁剪宏 NO_CHALI_STD_GRAPH。

## 参考代码

聚合入口的具体操作参见对应子模块；本页类型/方法的调用形式列在下方。

## 类型、枚举、常量与字段

公开字段按源码原样列出；句柄和游标字段用于包装器状态，通常通过方法操作。枚举的括号表示变体载荷。

| 所属类型 | 声明 | 说明 |
| --- | --- | --- |
| 模块 | <a id="symbol-66"></a>`struct GraphReply` | 图算法胶水回执；槽布局必须与 glue/std_graph.hpp 的 GraphReply 一致。 |
| GraphReply | <a id="symbol-67"></a>`String error` | 本次胶水调用的错误说明；成功时为空串。 |
| GraphReply | <a id="symbol-68"></a>`int64 value` | 标量结果：连通分量个数或生成树总权值。 |
| GraphReply | <a id="symbol-69"></a>`int64 flag` | 布尔结果：按入口语义表示无环 / 有环。 |
| GraphReply | <a id="symbol-70"></a>`List<int64> order` | 访问序或拓扑序。 |
| GraphReply | <a id="symbol-71"></a>`List<int64> dist` | 单源最短路距离表，不可达顶点为 -1。 |
| 模块 | <a id="symbol-72"></a>`struct GraphSnapshot` | 带权图：有向 / 无向由构造时的 directed 决定；重边与自环都允许。 |
| GraphSnapshot | <a id="symbol-73"></a>`int64 vertexCount` | 顶点数量。 |
| GraphSnapshot | <a id="symbol-74"></a>`bool directed` | 是否为有向图。 |
| GraphSnapshot | <a id="symbol-75"></a>`int64 edgeCount` | 逻辑边数，不计无向镜像。 |
| GraphSnapshot | <a id="symbol-76"></a>`List<int64> head` | 各顶点第一条邻接记录。 |
| GraphSnapshot | <a id="symbol-77"></a>`List<int64> edgeTo` | 邻接终点数组。 |
| GraphSnapshot | <a id="symbol-78"></a>`List<int64> edgeNext` | 同起点下一条邻接记录。 |
| GraphSnapshot | <a id="symbol-79"></a>`List<int64> edgeWeight` | 邻接记录权重。 |
| 模块 | <a id="symbol-80"></a>`opcode Graph` | 持久邻接图，算法原地读取，普通复制深复制，snapshot 显式导出。 |
| 模块 | <a id="symbol-81"></a>`opcode UnionFind` | 持久父指针并查集，压缩和合并原地执行，普通复制深复制。 |

## 函数索引

| 名称 | 声明 |
| --- | --- |
| [Graph::create](#api-0) | `static Graph create(int64 vertexCount, bool directed)` |
| [Graph::addEdge](#api-1) | `void addEdge(int64 from, int64 to, int64 weight)` |
| [Graph::vertexCount](#api-2) | `int64 vertexCount()` |
| [Graph::isDirected](#api-3) | `bool isDirected()` |
| [Graph::len](#api-4) | `int64 len()` |
| [Graph::bfs](#api-5) | `List<int64> bfs(int64 start)` |
| [Graph::dfs](#api-6) | `List<int64> dfs(int64 start)` |
| [Graph::connectedComponents](#api-7) | `int64 connectedComponents()` |
| [Graph::topologicalSort](#api-8) | `List<int64> topologicalSort()` |
| [Graph::dijkstra](#api-9) | `List<int64> dijkstra(int64 start)` |
| [Graph::hasCycle](#api-10) | `bool hasCycle()` |
| [Graph::minimumSpanningTreeWeight](#api-11) | `int64 minimumSpanningTreeWeight()` |
| [Graph::snapshot](#api-12) | `GraphSnapshot snapshot()` |
| [UnionFind::create](#api-13) | `static UnionFind create(int64 n)` |
| [UnionFind::find](#api-14) | `int64 find(int64 x)` |
| [UnionFind::unionSets](#api-15) | `bool unionSets(int64 a, int64 b)` |
| [UnionFind::connected](#api-16) | `bool connected(int64 a, int64 b)` |
| [UnionFind::sizeOf](#api-17) | `int64 sizeOf(int64 x)` |
| [UnionFind::count](#api-18) | `int64 count()` |
| [UnionFind::len](#api-19) | `int64 len()` |
| [UnionFind::parents](#api-20) | `List<int64> parents()` |
| [UnionFind::sizes](#api-21) | `List<int64> sizes()` |

<a id="api-0"></a>

### Graph::create（1）

**声明**

```chali
static Graph create(int64 vertexCount, bool directed);
```

**行为**

创建Graph，返回可供后续操作的值。

创建指定顶点数及方向的图。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `vertexCount` | `int64 vertexCount` | 非负顶点数量 | 无 |
| `directed` | `bool directed` | 是否创建有向图 | 无 |

**返回值**

`Graph`。创建Graph，返回可供后续操作的值。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
std::graph::Graph::create(vertexCount, directed);
```

**源码**：[source/std/graph.chl:49](../../source/std/graph.chl#L49)。

<a id="api-1"></a>

### Graph::addEdge（2）

**声明**

```chali
void addEdge(int64 from, int64 to, int64 weight);
```

**行为**

原地添加边；无向图同时登记反向邻接。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `from` | `int64 from` | 被替换的子串 | 无 |
| `to` | `int64 to` | 替换文本 | 无 |
| `weight` | `int64 weight` | 边权值 | 无 |

**返回值**

无返回值。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.addEdge(from, to, weight);
```

**源码**：[source/std/graph.chl:51](../../source/std/graph.chl#L51)。

<a id="api-2"></a>

### Graph::vertexCount（3）

**声明**

```chali
int64 vertexCount();
```

**行为**

顶点数。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`int64`。顶点数。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.vertexCount();
```

**源码**：[source/std/graph.chl:53](../../source/std/graph.chl#L53)。

<a id="api-3"></a>

### Graph::isDirected（4）

**声明**

```chali
bool isDirected();
```

**行为**

是否有向。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`bool`。是否有向。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.isDirected();
```

**源码**：[source/std/graph.chl:55](../../source/std/graph.chl#L55)。

<a id="api-4"></a>

### Graph::len（5）

**声明**

```chali
int64 len();
```

**行为**

返回当前元素数量。

添加的逻辑边数。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`int64`。返回当前元素数量。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.len();
```

**源码**：[source/std/graph.chl:57](../../source/std/graph.chl#L57)。

<a id="api-5"></a>

### Graph::bfs（6）

**声明**

```chali
List<int64> bfs(int64 start);
```

**行为**

广度优先访问序列。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `start` | `int64 start` | 起始值 | 无 |

**返回值**

`List<int64>`。广度优先访问序列。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.bfs(start);
```

**源码**：[source/std/graph.chl:59](../../source/std/graph.chl#L59)。

<a id="api-6"></a>

### Graph::dfs（7）

**声明**

```chali
List<int64> dfs(int64 start);
```

**行为**

深度优先访问序列。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `start` | `int64 start` | 起始值 | 无 |

**返回值**

`List<int64>`。深度优先访问序列。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.dfs(start);
```

**源码**：[source/std/graph.chl:61](../../source/std/graph.chl#L61)。

<a id="api-7"></a>

### Graph::connectedComponents（8）

**声明**

```chali
int64 connectedComponents();
```

**行为**

无向口径的连通分量数。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`int64`。无向口径的连通分量数。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.connectedComponents();
```

**源码**：[source/std/graph.chl:63](../../source/std/graph.chl#L63)。

<a id="api-8"></a>

### Graph::topologicalSort（9）

**声明**

```chali
List<int64> topologicalSort();
```

**行为**

拓扑顺序，有环时为空。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`List<int64>`。拓扑顺序，有环时为空。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.topologicalSort();
```

**源码**：[source/std/graph.chl:65](../../source/std/graph.chl#L65)。

<a id="api-9"></a>

### Graph::dijkstra（10）

**声明**

```chali
List<int64> dijkstra(int64 start);
```

**行为**

单源非负权最短路，不可达距离为 -1。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `start` | `int64 start` | 起始值 | 无 |

**返回值**

`List<int64>`。单源非负权最短路，不可达距离为 -1。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.dijkstra(start);
```

**源码**：[source/std/graph.chl:67](../../source/std/graph.chl#L67)。

<a id="api-10"></a>

### Graph::hasCycle（11）

**声明**

```chali
bool hasCycle();
```

**行为**

按图方向检测环。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`bool`。按图方向检测环。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.hasCycle();
```

**源码**：[source/std/graph.chl:69](../../source/std/graph.chl#L69)。

<a id="api-11"></a>

### Graph::minimumSpanningTreeWeight（12）

**声明**

```chali
int64 minimumSpanningTreeWeight();
```

**行为**

无向最小生成树权重，不连通时为 -1。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`int64`。无向最小生成树权重，不连通时为 -1。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.minimumSpanningTreeWeight();
```

**源码**：[source/std/graph.chl:71](../../source/std/graph.chl#L71)。

<a id="api-12"></a>

### Graph::snapshot（13）

**声明**

```chali
GraphSnapshot snapshot();
```

**行为**

显式导出完整独立邻接快照。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`GraphSnapshot`。显式导出完整独立邻接快照。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.snapshot();
```

**源码**：[source/std/graph.chl:73](../../source/std/graph.chl#L73)。

<a id="api-13"></a>

### UnionFind::create（14）

**声明**

```chali
static UnionFind create(int64 n);
```

**行为**

创建UnionFind，返回可供后续操作的值。

创建 n 个单元素集合。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `n` | `int64 n` | 向量长度或矩阵阶数 | 无 |

**返回值**

`UnionFind`。创建UnionFind，返回可供后续操作的值。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
std::graph::UnionFind::create(n);
```

**源码**：[source/std/graph.chl:96](../../source/std/graph.chl#L96)。

<a id="api-14"></a>

### UnionFind::find（15）

**声明**

```chali
int64 find(int64 x);
```

**行为**

查找首个匹配项，返回下标或可选载荷；具体未找到表示见返回类型及模块约定。

带路径压缩查询根；不复制或导出内部数组。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `x` | `int64 x` | 输入横坐标/标量 | 无 |

**返回值**

`int64`。查找首个匹配项，返回下标或可选载荷；具体未找到表示见返回类型及模块约定。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.find(x);
```

**源码**：[source/std/graph.chl:98](../../source/std/graph.chl#L98)。

<a id="api-15"></a>

### UnionFind::unionSets（16）

**声明**

```chali
bool unionSets(int64 a, int64 b);
```

**行为**

按大小原地合并；发生合并时返回 true。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `a` | `int64 a` | 第一个元素或顶点下标 | 无 |
| `b` | `int64 b` | 第二个元素或顶点下标 | 无 |

**返回值**

`bool`。按大小原地合并；发生合并时返回 true。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.unionSets(a, b);
```

**源码**：[source/std/graph.chl:100](../../source/std/graph.chl#L100)。

<a id="api-16"></a>

### UnionFind::connected（17）

**声明**

```chali
bool connected(int64 a, int64 b);
```

**行为**

判断两个元素是否属于同一集合。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `a` | `int64 a` | 第一个元素或顶点下标 | 无 |
| `b` | `int64 b` | 第二个元素或顶点下标 | 无 |

**返回值**

`bool`。判断两个元素是否属于同一集合。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.connected(a, b);
```

**源码**：[source/std/graph.chl:102](../../source/std/graph.chl#L102)。

<a id="api-17"></a>

### UnionFind::sizeOf（18）

**声明**

```chali
int64 sizeOf(int64 x);
```

**行为**

查询元素所在集合的大小。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `x` | `int64 x` | 输入横坐标/标量 | 无 |

**返回值**

`int64`。查询元素所在集合的大小。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.sizeOf(x);
```

**源码**：[source/std/graph.chl:104](../../source/std/graph.chl#L104)。

<a id="api-18"></a>

### UnionFind::count（19）

**声明**

```chali
int64 count();
```

**行为**

读取结果首行首列的整数表示。

当前连通分量个数。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`int64`。读取结果首行首列的整数表示。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.count();
```

**源码**：[source/std/graph.chl:106](../../source/std/graph.chl#L106)。

<a id="api-19"></a>

### UnionFind::len（20）

**声明**

```chali
int64 len();
```

**行为**

返回当前元素数量。

总元素个数。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`int64`。返回当前元素数量。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.len();
```

**源码**：[source/std/graph.chl:108](../../source/std/graph.chl#L108)。

<a id="api-20"></a>

### UnionFind::parents（21）

**声明**

```chali
List<int64> parents();
```

**行为**

显式导出独立父指针快照。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`List<int64>`。显式导出独立父指针快照。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.parents();
```

**源码**：[source/std/graph.chl:110](../../source/std/graph.chl#L110)。

<a id="api-21"></a>

### UnionFind::sizes（22）

**声明**

```chali
List<int64> sizes();
```

**行为**

显式导出独立集合大小快照；仅根对应的值有集合大小含义。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`List<int64>`。显式导出独立集合大小快照；仅根对应的值有集合大小含义。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.sizes();
```

**源码**：[source/std/graph.chl:112](../../source/std/graph.chl#L112)。
