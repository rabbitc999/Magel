# std::graph — 图论与并查集

[返回库目录](README.md) · [全部符号](symbols.md)

## 引入

```magel
import std::graph;
```

## 公共约定与错误边界

Graph 使用链式前向星存储：head 是每顶点首条边，edgeTo/edgeNext/edgeWeight 是边终点、同起点下一条边与边权，顶点编号 0..vertexCount-1，重边与自环都允许。有向 / 无向由 create 的 directed 决定，无向图加边时自动镜像登记。bfs/dfs 输出可达顶点的访问序；topologicalSort 用 Kahn 算法，有环时输出空表；dijkstra 要求边权非负，不可达顶点距离为 -1；hasCycle 有向图用 Kahn 计数、无向图用并查集；minimumSpanningTreeWeight 用 Kruskal，图不连通返回 -1。UnionFind 采用路径压缩与按大小合并，find 返回后父指针已压缩。全部算法由宿主胶水 glue/std_graph.hpp 提供，裁剪宏 NO_MAGEL_STD_GRAPH。

## 参考代码

聚合入口的具体操作参见对应子模块；本页类型/方法的调用形式列在下方。

## 类型、枚举、常量与字段

公开字段按源码原样列出；句柄和游标字段用于包装器状态，通常通过方法操作。枚举的括号表示变体载荷。

| 所属类型 | 声明 | 说明 |
| --- | --- | --- |
| 模块 | <a id="symbol-62"></a>`struct GraphReply` | 图算法胶水回执；槽布局必须与 glue/std_graph.hpp 的 GraphReply 一致。 |
| GraphReply | <a id="symbol-63"></a>`String error` | 本次胶水调用的错误说明；成功时为空串。 |
| GraphReply | <a id="symbol-64"></a>`int64 value` | 标量结果：连通分量个数或生成树总权值。 |
| GraphReply | <a id="symbol-65"></a>`int64 flag` | 布尔结果：按入口语义表示无环 / 有环。 |
| GraphReply | <a id="symbol-66"></a>`List<int64> order` | 访问序或拓扑序。 |
| GraphReply | <a id="symbol-67"></a>`List<int64> dist` | 单源最短路距离表，不可达顶点为 -1。 |
| 模块 | <a id="symbol-68"></a>`struct DsuReply` | 并查集胶水回执；槽布局必须与 glue/std_graph.hpp 的 DsuReply 一致。 |
| DsuReply | <a id="symbol-69"></a>`List<int64> parent` | 更新后的父指针数组。 |
| DsuReply | <a id="symbol-70"></a>`List<int64> sizes` | 更新后的集合大小数组。 |
| DsuReply | <a id="symbol-71"></a>`int64 root` | 本次查找或合并后的根下标。 |
| DsuReply | <a id="symbol-72"></a>`int64 flag` | 布尔结果：合并 1 = 发生了合并。 |
| 模块 | <a id="symbol-73"></a>`struct Graph : Drop` | 带权图：有向 / 无向由构造时的 directed 决定；重边与自环都允许。 |
| Graph | <a id="symbol-74"></a>`int64 vertexCount` | 顶点数，编号 0..vertexCount-1。 |
| Graph | <a id="symbol-75"></a>`bool directed` | 是否为有向图。 |
| Graph | <a id="symbol-76"></a>`List<int64> head` | 每个顶点的首条边下标，无出边为 -1。 |
| Graph | <a id="symbol-77"></a>`List<int64> edgeTo` | 边终点表。 |
| Graph | <a id="symbol-78"></a>`List<int64> edgeNext` | 同起点下一条边表。 |
| Graph | <a id="symbol-79"></a>`List<int64> edgeWeight` | 边权表。 |
| Graph | <a id="symbol-80"></a>`int64 edgeCount` | 当前边数，按 addEdge 调用次数计。 |
| 模块 | <a id="symbol-81"></a>`struct UnionFind : Drop` | 并查集：路径压缩 + 按大小合并；find / union 返回后内部数组已更新。 |
| UnionFind | <a id="symbol-82"></a>`List<int64> parent` | 父指针数组。 |
| UnionFind | <a id="symbol-83"></a>`List<int64> sizes` | 各根对应的集合大小。 |
| UnionFind | <a id="symbol-84"></a>`int64 components` | 当前集合（连通分量）个数。 |

## 函数索引

| 名称 | 声明 |
| --- | --- |
| [Graph::drop](#api-0) | `void drop()` |
| [Graph::create](#api-1) | `static Graph create(int64 vertexCount, bool directed)` |
| [Graph::addEdge](#api-2) | `void addEdge(int64 u, int64 v, int64 weight)` |
| [Graph::bfs](#api-3) | `List<int64> bfs(int64 start)` |
| [Graph::dfs](#api-4) | `List<int64> dfs(int64 start)` |
| [Graph::connectedComponents](#api-5) | `int64 connectedComponents()` |
| [Graph::topologicalSort](#api-6) | `List<int64> topologicalSort()` |
| [Graph::dijkstra](#api-7) | `List<int64> dijkstra(int64 start)` |
| [Graph::hasCycle](#api-8) | `bool hasCycle()` |
| [Graph::minimumSpanningTreeWeight](#api-9) | `int64 minimumSpanningTreeWeight()` |
| [Graph::len](#api-10) | `int64 len()` |
| [UnionFind::drop](#api-11) | `void drop()` |
| [UnionFind::create](#api-12) | `static UnionFind create(int64 n)` |
| [UnionFind::find](#api-13) | `int64 find(int64 x)` |
| [UnionFind::unionSets](#api-14) | `bool unionSets(int64 a, int64 b)` |
| [UnionFind::connected](#api-15) | `bool connected(int64 a, int64 b)` |
| [UnionFind::sizeOf](#api-16) | `int64 sizeOf(int64 x)` |
| [UnionFind::count](#api-17) | `int64 count()` |

<a id="api-0"></a>

### Graph::drop（1）

**声明**

```magel
void drop();
```

**行为**

释放对象拥有的资源，调用后遵守本模块失效规则。

///< 当前边数（按 addEdge 调用次数计）。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

无返回值。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.drop();
```

**源码**：[source/std/graph.mgl:59](../../source/std/graph.mgl#L59)。

<a id="api-1"></a>

### Graph::create（2）

**声明**

```magel
static Graph create(int64 vertexCount, bool directed);
```

**行为**

创建Graph，返回可供后续操作的值。

创建 n 个顶点的空图。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `vertexCount` | `int64 vertexCount` | 顶点数，必须非负。 | 无 |
| `directed` | `bool directed` | true 为有向图，false 为无向图。 | 无 |

**返回值**

`Graph`。创建Graph，返回可供后续操作的值。

**错误与边界**

遵循本页“公共约定与错误边界”。
显式前置检查：`vertexCount >= 0`。

**调用形式**

```magel
std::graph::Graph::create(vertexCount, directed);
```

**源码**：[source/std/graph.mgl:66](../../source/std/graph.mgl#L66)。

<a id="api-2"></a>

### Graph::addEdge（3）

**声明**

```magel
void addEdge(int64 u, int64 v, int64 weight);
```

**行为**

加一条边。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `u` | `int64 u` | 起点下标。 | 无 |
| `v` | `int64 v` | 终点下标。 | 无 |
| `weight` | `int64 weight` | 边权，要求非负（Dijkstra / MST 的前提）。 | 无 |

**返回值**

无返回值。

**错误与边界**

遵循本页“公共约定与错误边界”。
显式前置检查：`u >= 0 && u < self.vertexCount && v >= 0 && v < self.vertexCount`。

**调用形式**

```magel
obj.addEdge(u, v, weight);
```

**源码**：[source/std/graph.mgl:95](../../source/std/graph.mgl#L95)。

<a id="api-3"></a>

### Graph::bfs（4）

**声明**

```magel
List<int64> bfs(int64 start);
```

**行为**

广度优先搜索，输出从 start 可达顶点的访问序。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `start` | `int64 start` | 起点下标。 | 无 |

**返回值**

`List<int64>`。广度优先搜索，输出从 start 可达顶点的访问序。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.bfs(start);
```

**源码**：[source/std/graph.mgl:115](../../source/std/graph.mgl#L115)。

<a id="api-4"></a>

### Graph::dfs（5）

**声明**

```magel
List<int64> dfs(int64 start);
```

**行为**

深度优先搜索（显式栈，邻居按插入序访问）。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `start` | `int64 start` | 起点下标。 | 无 |

**返回值**

`List<int64>`。深度优先搜索（显式栈，邻居按插入序访问）。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.dfs(start);
```

**源码**：[source/std/graph.mgl:126](../../source/std/graph.mgl#L126)。

<a id="api-5"></a>

### Graph::connectedComponents（6）

**声明**

```magel
int64 connectedComponents();
```

**行为**

连通分量个数（无向口径）。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`int64`。连通分量个数（无向口径）。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.connectedComponents();
```

**源码**：[source/std/graph.mgl:135](../../source/std/graph.mgl#L135)。

<a id="api-6"></a>

### Graph::topologicalSort（7）

**声明**

```magel
List<int64> topologicalSort();
```

**行为**

Kahn 拓扑排序。 @retval 无环时输出一个合法序列；有环时输出空列表。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`List<int64>`。Kahn 拓扑排序。 @retval 无环时输出一个合法序列；有环时输出空列表。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.topologicalSort();
```

**源码**：[source/std/graph.mgl:145](../../source/std/graph.mgl#L145)。

<a id="api-7"></a>

### Graph::dijkstra（8）

**声明**

```magel
List<int64> dijkstra(int64 start);
```

**行为**

Dijkstra 单源最短路。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `start` | `int64 start` | 源点下标。 | 无 |

**返回值**

`List<int64>`。Dijkstra 单源最短路。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.dijkstra(start);
```

**源码**：[source/std/graph.mgl:157](../../source/std/graph.mgl#L157)。

<a id="api-8"></a>

### Graph::hasCycle（9）

**声明**

```magel
bool hasCycle();
```

**行为**

环检测：有向图用 Kahn 计数，无向图用并查集。 @retval 有环返回 true。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`bool`。环检测：有向图用 Kahn 计数，无向图用并查集。 @retval 有环返回 true。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.hasCycle();
```

**源码**：[source/std/graph.mgl:167](../../source/std/graph.mgl#L167)。

<a id="api-9"></a>

### Graph::minimumSpanningTreeWeight（10）

**声明**

```magel
int64 minimumSpanningTreeWeight();
```

**行为**

Kruskal 最小生成树总权值。 @retval 无向连通图的总权值；图不连通时返回 -1。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`int64`。Kruskal 最小生成树总权值。 @retval 无向连通图的总权值；图不连通时返回 -1。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.minimumSpanningTreeWeight();
```

**源码**：[source/std/graph.mgl:181](../../source/std/graph.mgl#L181)。

<a id="api-10"></a>

### Graph::len（11）

**声明**

```magel
int64 len();
```

**行为**

返回当前元素数量。

当前边数（按 addEdge 调用次数计，不含无向图的镜像登记）。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`int64`。返回当前元素数量。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.len();
```

**源码**：[source/std/graph.mgl:190](../../source/std/graph.mgl#L190)。

<a id="api-11"></a>

### UnionFind::drop（12）

**声明**

```magel
void drop();
```

**行为**

释放对象拥有的资源，调用后遵守本模块失效规则。

///< 当前集合（连通分量）个数。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

无返回值。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.drop();
```

**源码**：[source/std/graph.mgl:199](../../source/std/graph.mgl#L199)。

<a id="api-12"></a>

### UnionFind::create（13）

**声明**

```magel
static UnionFind create(int64 n);
```

**行为**

创建UnionFind，返回可供后续操作的值。

创建 n 个单元素集合。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `n` | `int64 n` | 元素个数，必须非负。 | 无 |

**返回值**

`UnionFind`。创建UnionFind，返回可供后续操作的值。

**错误与边界**

遵循本页“公共约定与错误边界”。
显式前置检查：`n >= 0`。

**调用形式**

```magel
std::graph::UnionFind::create(n);
```

**源码**：[source/std/graph.mgl:205](../../source/std/graph.mgl#L205)。

<a id="api-13"></a>

### UnionFind::find（14）

**声明**

```magel
int64 find(int64 x);
```

**行为**

查找首个匹配项，返回下标或可选载荷；具体未找到表示见返回类型及模块约定。

查找元素所在集合的根（带路径压缩）。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `x` | `int64 x` | 元素下标。 | 无 |

**返回值**

`int64`。查找首个匹配项，返回下标或可选载荷；具体未找到表示见返回类型及模块约定。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.find(x);
```

**源码**：[source/std/graph.mgl:229](../../source/std/graph.mgl#L229)。

<a id="api-14"></a>

### UnionFind::unionSets（15）

**声明**

```magel
bool unionSets(int64 a, int64 b);
```

**行为**

合并两个元素所在集合（按大小）。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `a` | `int64 a` | 元素一下标。 | 无 |
| `b` | `int64 b` | 元素二下标。 | 无 |

**返回值**

`bool`。合并两个元素所在集合（按大小）。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.unionSets(a, b);
```

**源码**：[source/std/graph.mgl:242](../../source/std/graph.mgl#L242)。

<a id="api-15"></a>

### UnionFind::connected（16）

**声明**

```magel
bool connected(int64 a, int64 b);
```

**行为**

两个元素是否同集。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `a` | `int64 a` | 第一个元素或顶点下标 | 无 |
| `b` | `int64 b` | 第二个元素或顶点下标 | 无 |

**返回值**

`bool`。两个元素是否同集。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.connected(a, b);
```

**源码**：[source/std/graph.mgl:254](../../source/std/graph.mgl#L254)。

<a id="api-16"></a>

### UnionFind::sizeOf（17）

**声明**

```magel
int64 sizeOf(int64 x);
```

**行为**

x 所在集合的大小。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `x` | `int64 x` | 输入横坐标/标量 | 无 |

**返回值**

`int64`。x 所在集合的大小。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.sizeOf(x);
```

**源码**：[source/std/graph.mgl:259](../../source/std/graph.mgl#L259)。

<a id="api-17"></a>

### UnionFind::count（18）

**声明**

```magel
int64 count();
```

**行为**

读取结果首行首列的整数表示。

当前集合个数。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`int64`。读取结果首行首列的整数表示。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.count();
```

**源码**：[source/std/graph.mgl:264](../../source/std/graph.mgl#L264)。
