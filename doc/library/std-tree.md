# std::tree — 树与堆

[返回库目录](README.md) · [全部符号](symbols.md)

## 引入

```magel
import std::tree;
```

## 公共约定与错误边界

BinarySearchTree 是集合语义的二叉搜索树，重复插入返回 false，中序即升序；删除双孩子节点取中序后继替换，被删节点的存储保留但不可达。RedBlackTree 只支持插入（含双红修复），颜色 0 红 1 黑，删除请用 BinarySearchTree::remove。MinHeap 是最小二叉堆，可当优先队列用，相等键的出堆顺序不保证插入序。GeneralTree 用"第一个孩子 / 下一个兄弟"存储，toBinary 与 fromBinary 按"左孩子 = 第一个孩子、右孩子 = 下一个兄弟"互转，转换本身不排序、不保证 BST 有序性。树高按节点数计（单节点 1，空树 0）；节点存放在平行列表里，`0 - 1` 表示空指针。全部算法由宿主胶水 glue/std_tree.hpp 提供，裁剪宏 NO_MAGEL_STD_TREE。

## 参考代码

聚合入口的具体操作参见对应子模块；本页类型/方法的调用形式列在下方。

## 类型、枚举、常量与字段

公开字段按源码原样列出；句柄和游标字段用于包装器状态，通常通过方法操作。枚举的括号表示变体载荷。

| 所属类型 | 声明 | 说明 |
| --- | --- | --- |
| 模块 | <a id="symbol-468"></a>`struct TreeReply` | 胶水操作的统一回执；槽布局必须与 glue/std_tree.hpp 的 Reply 一致。 |
| TreeReply | <a id="symbol-469"></a>`String error` | 本次胶水调用的错误说明；成功时为空串。 |
| TreeReply | <a id="symbol-470"></a>`int64 flag` | 布尔结果：1 = 成功 / 存在 / 非空。 |
| TreeReply | <a id="symbol-471"></a>`int64 value` | 标量结果：查到的键、弹出的堆顶或树高。 |
| TreeReply | <a id="symbol-472"></a>`int64 root` | 更新后的根节点下标，空树为 -1。 |
| TreeReply | <a id="symbol-473"></a>`List<int64> outKeys` | 更新后的键列表。 |
| TreeReply | <a id="symbol-474"></a>`List<int64> outLefts` | 更新后的左孩子列表。 |
| TreeReply | <a id="symbol-475"></a>`List<int64> outRights` | 更新后的右孩子列表。 |
| TreeReply | <a id="symbol-476"></a>`List<int64> values` | 遍历序列或更新后的堆数组。 |
| TreeReply | <a id="symbol-477"></a>`List<int64> colors` | 红黑树更新后的颜色列表，0 红 1 黑。 |
| 模块 | <a id="symbol-478"></a>`struct GeneralReply` | 通用树胶水回执；槽布局必须与 glue/std_tree.hpp 的 GeneralReply 一致。 |
| GeneralReply | <a id="symbol-479"></a>`String error` | 本次胶水调用的错误说明；成功时为空串。 |
| GeneralReply | <a id="symbol-480"></a>`int64 value` | 标量结果：新节点下标或树高。 |
| GeneralReply | <a id="symbol-481"></a>`int64 flag` | 布尔结果：1 = 成功。 |
| GeneralReply | <a id="symbol-482"></a>`List<int64> values` | 更新后的节点值列表。 |
| GeneralReply | <a id="symbol-483"></a>`List<int64> firstChild` | 更新后的第一个孩子列表。 |
| GeneralReply | <a id="symbol-484"></a>`List<int64> nextSibling` | 更新后的下一个兄弟列表。 |
| GeneralReply | <a id="symbol-485"></a>`List<int64> out` | 遍历序列或孩子下标序列。 |
| 模块 | <a id="symbol-486"></a>`struct BinarySearchTree : Drop` | 二叉搜索树：集合语义，重复插入返回 false；中序即升序。 |
| BinarySearchTree | <a id="symbol-487"></a>`List<int64> keys` | 节点键值，下标即节点号。 |
| BinarySearchTree | <a id="symbol-488"></a>`List<int64> lefts` | 左孩子下标，无孩子为 -1。 |
| BinarySearchTree | <a id="symbol-489"></a>`List<int64> rights` | 右孩子下标，无孩子为 -1。 |
| BinarySearchTree | <a id="symbol-490"></a>`int64 root` | 根节点下标，空树为 -1。 |
| BinarySearchTree | <a id="symbol-491"></a>`int64 count` | 当前有效节点数。 |
| 模块 | <a id="symbol-492"></a>`struct RedBlackTree : Drop` | 红黑树：平衡二叉搜索树，插入含双红修复；只支持插入不支持删除。 |
| RedBlackTree | <a id="symbol-493"></a>`List<int64> keys` | 节点键值，下标即节点号。 |
| RedBlackTree | <a id="symbol-494"></a>`List<int64> lefts` | 左孩子下标，无孩子为 -1。 |
| RedBlackTree | <a id="symbol-495"></a>`List<int64> rights` | 右孩子下标，无孩子为 -1。 |
| RedBlackTree | <a id="symbol-496"></a>`List<int64> colors` | 节点颜色，0 = 红、1 = 黑。 |
| RedBlackTree | <a id="symbol-497"></a>`int64 root` | 根节点下标，空树为 -1。 |
| RedBlackTree | <a id="symbol-498"></a>`int64 count` | 当前有效节点数。 |
| 模块 | <a id="symbol-499"></a>`struct MinHeap : Drop` | 二叉最小堆：可当优先队列用，pop 恒返回当前最小值；相等键出堆顺序不保证插入序。 |
| MinHeap | <a id="symbol-500"></a>`List<int64> values` | 堆数组，下标 0 是最小值。 |
| 模块 | <a id="symbol-501"></a>`struct GeneralTree : Drop` | 通用树（n 叉树）：第一个孩子 / 下一个兄弟存储，与二叉表示可按映射互转。 |
| GeneralTree | <a id="symbol-502"></a>`List<int64> values` | 节点值，下标即节点号。 |
| GeneralTree | <a id="symbol-503"></a>`List<int64> firstChild` | 第一个孩子下标，无孩子为 -1。 |
| GeneralTree | <a id="symbol-504"></a>`List<int64> nextSibling` | 下一个兄弟下标，无兄弟为 -1。 |
| GeneralTree | <a id="symbol-505"></a>`int64 count` | 当前节点数。 |

## 函数索引

| 名称 | 声明 |
| --- | --- |
| [BinarySearchTree::drop](#api-0) | `void drop()` |
| [BinarySearchTree::create](#api-1) | `static BinarySearchTree create()` |
| [BinarySearchTree::insert](#api-2) | `bool insert(int64 value)` |
| [BinarySearchTree::contains](#api-3) | `bool contains(int64 value)` |
| [BinarySearchTree::remove](#api-4) | `bool remove(int64 value)` |
| [BinarySearchTree::min](#api-5) | `Option<int64> min()` |
| [BinarySearchTree::max](#api-6) | `Option<int64> max()` |
| [BinarySearchTree::height](#api-7) | `int64 height()` |
| [BinarySearchTree::inorder](#api-8) | `List<int64> inorder()` |
| [BinarySearchTree::preorder](#api-9) | `List<int64> preorder()` |
| [BinarySearchTree::postorder](#api-10) | `List<int64> postorder()` |
| [BinarySearchTree::len](#api-11) | `int64 len()` |
| [BinarySearchTree::clear](#api-12) | `void clear()` |
| [RedBlackTree::drop](#api-13) | `void drop()` |
| [RedBlackTree::create](#api-14) | `static RedBlackTree create()` |
| [RedBlackTree::insert](#api-15) | `bool insert(int64 value)` |
| [RedBlackTree::contains](#api-16) | `bool contains(int64 value)` |
| [RedBlackTree::min](#api-17) | `Option<int64> min()` |
| [RedBlackTree::max](#api-18) | `Option<int64> max()` |
| [RedBlackTree::height](#api-19) | `int64 height()` |
| [RedBlackTree::inorder](#api-20) | `List<int64> inorder()` |
| [RedBlackTree::len](#api-21) | `int64 len()` |
| [RedBlackTree::clear](#api-22) | `void clear()` |
| [MinHeap::drop](#api-23) | `void drop()` |
| [MinHeap::create](#api-24) | `static MinHeap create()` |
| [MinHeap::push](#api-25) | `void push(int64 value)` |
| [MinHeap::pop](#api-26) | `Option<int64> pop()` |
| [MinHeap::peek](#api-27) | `Option<int64> peek()` |
| [MinHeap::len](#api-28) | `int64 len()` |
| [MinHeap::isEmpty](#api-29) | `bool isEmpty()` |
| [MinHeap::clear](#api-30) | `void clear()` |
| [GeneralTree::drop](#api-31) | `void drop()` |
| [GeneralTree::create](#api-32) | `static GeneralTree create()` |
| [GeneralTree::addRoot](#api-33) | `int64 addRoot(int64 value)` |
| [GeneralTree::addChild](#api-34) | `int64 addChild(int64 cursor, int64 value)` |
| [GeneralTree::valueAt](#api-35) | `int64 valueAt(int64 cursor)` |
| [GeneralTree::children](#api-36) | `List<int64> children(int64 cursor)` |
| [GeneralTree::preorder](#api-37) | `List<int64> preorder()` |
| [GeneralTree::levelOrder](#api-38) | `List<int64> levelOrder()` |
| [GeneralTree::height](#api-39) | `int64 height()` |
| [GeneralTree::len](#api-40) | `int64 len()` |
| [GeneralTree::toBinary](#api-41) | `BinarySearchTree toBinary()` |
| [GeneralTree::fromBinary](#api-42) | `static GeneralTree fromBinary(BinarySearchTree tree)` |
| [GeneralTree::clear](#api-43) | `void clear()` |
| [GeneralTree::rootNode](#api-44) | `int64 rootNode()` |

<a id="api-0"></a>

### BinarySearchTree::drop（1）

**声明**

```magel
void drop();
```

**行为**

释放对象拥有的资源，调用后遵守本模块失效规则。

///< 当前有效节点数。

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

**源码**：[source/std/tree.mgl:78](../../source/std/tree.mgl#L78)。

<a id="api-1"></a>

### BinarySearchTree::create（2）

**声明**

```magel
static BinarySearchTree create();
```

**行为**

创建BinarySearchTree，返回可供后续操作的值。

创建一棵空搜索树。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`BinarySearchTree`。创建BinarySearchTree，返回可供后续操作的值。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::tree::BinarySearchTree::create();
```

**源码**：[source/std/tree.mgl:83](../../source/std/tree.mgl#L83)。

<a id="api-2"></a>

### BinarySearchTree::insert（3）

**声明**

```magel
bool insert(int64 value);
```

**行为**

在 index 之前插入 value。

插入一个键。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `value` | `int64 value` | 要插入的键。 | 无 |

**返回值**

`bool`。在 index 之前插入 value。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.insert(value);
```

**源码**：[source/std/tree.mgl:99](../../source/std/tree.mgl#L99)。

<a id="api-3"></a>

### BinarySearchTree::contains（4）

**声明**

```magel
bool contains(int64 value);
```

**行为**

判断是否包含目标值或匹配文本。

判断树里有没有这个键。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `value` | `int64 value` | 要查的键。 | 无 |

**返回值**

`bool`。判断是否包含目标值或匹配文本。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.contains(value);
```

**源码**：[source/std/tree.mgl:115](../../source/std/tree.mgl#L115)。

<a id="api-4"></a>

### BinarySearchTree::remove（5）

**声明**

```magel
bool remove(int64 value);
```

**行为**

删除指定位置或键的元素。

删除一个键。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `value` | `int64 value` | 要删除的键。 | 无 |

**返回值**

`bool`。删除指定位置或键的元素。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.remove(value);
```

**源码**：[source/std/tree.mgl:127](../../source/std/tree.mgl#L127)。

<a id="api-5"></a>

### BinarySearchTree::min（6）

**声明**

```magel
Option<int64> min();
```

**行为**

返回实参中的较小值；泛型入口，接受任意数量实参，相等时取后者。

最小键。 @retval 空树返回 None；否则返回最小键的 Value。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`Option<int64>`。返回实参中的较小值；泛型入口，接受任意数量实参，相等时取后者。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.min();
```

**源码**：[source/std/tree.mgl:142](../../source/std/tree.mgl#L142)。

<a id="api-6"></a>

### BinarySearchTree::max（7）

**声明**

```magel
Option<int64> max();
```

**行为**

返回实参中的较大值；泛型入口，接受任意数量实参，相等时取后者。

最大键。 @retval 空树返回 None；否则返回最大键的 Value。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`Option<int64>`。返回实参中的较大值；泛型入口，接受任意数量实参，相等时取后者。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.max();
```

**源码**：[source/std/tree.mgl:153](../../source/std/tree.mgl#L153)。

<a id="api-7"></a>

### BinarySearchTree::height（8）

**声明**

```magel
int64 height();
```

**行为**

树高（节点数口径：单节点 1，空树 0）。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`int64`。树高（节点数口径：单节点 1，空树 0）。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.height();
```

**源码**：[source/std/tree.mgl:163](../../source/std/tree.mgl#L163)。

<a id="api-8"></a>

### BinarySearchTree::inorder（9）

**声明**

```magel
List<int64> inorder();
```

**行为**

中序遍历（升序序列）。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`List<int64>`。中序遍历（升序序列）。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.inorder();
```

**源码**：[source/std/tree.mgl:172](../../source/std/tree.mgl#L172)。

<a id="api-9"></a>

### BinarySearchTree::preorder（10）

**声明**

```magel
List<int64> preorder();
```

**行为**

前序遍历（根 - 左 - 右）。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`List<int64>`。前序遍历（根 - 左 - 右）。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.preorder();
```

**源码**：[source/std/tree.mgl:181](../../source/std/tree.mgl#L181)。

<a id="api-10"></a>

### BinarySearchTree::postorder（11）

**声明**

```magel
List<int64> postorder();
```

**行为**

后序遍历（左 - 右 - 根）。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`List<int64>`。后序遍历（左 - 右 - 根）。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.postorder();
```

**源码**：[source/std/tree.mgl:190](../../source/std/tree.mgl#L190)。

<a id="api-11"></a>

### BinarySearchTree::len（12）

**声明**

```magel
int64 len();
```

**行为**

返回当前元素数量。

当前有效节点数。

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

**源码**：[source/std/tree.mgl:199](../../source/std/tree.mgl#L199)。

<a id="api-12"></a>

### BinarySearchTree::clear（13）

**声明**

```magel
void clear();
```

**行为**

移除全部元素。

清空整棵树。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

无返回值。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.clear();
```

**源码**：[source/std/tree.mgl:204](../../source/std/tree.mgl#L204)。

<a id="api-13"></a>

### RedBlackTree::drop（14）

**声明**

```magel
void drop();
```

**行为**

释放对象拥有的资源，调用后遵守本模块失效规则。

///< 当前有效节点数。

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

**源码**：[source/std/tree.mgl:222](../../source/std/tree.mgl#L222)。

<a id="api-14"></a>

### RedBlackTree::create（15）

**声明**

```magel
static RedBlackTree create();
```

**行为**

创建RedBlackTree，返回可供后续操作的值。

创建一棵空红黑树。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`RedBlackTree`。创建RedBlackTree，返回可供后续操作的值。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::tree::RedBlackTree::create();
```

**源码**：[source/std/tree.mgl:227](../../source/std/tree.mgl#L227)。

<a id="api-15"></a>

### RedBlackTree::insert（16）

**声明**

```magel
bool insert(int64 value);
```

**行为**

在 index 之前插入 value。

插入一个键，插入后做双红修复保持平衡。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `value` | `int64 value` | 要插入的键。 | 无 |

**返回值**

`bool`。在 index 之前插入 value。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.insert(value);
```

**源码**：[source/std/tree.mgl:245](../../source/std/tree.mgl#L245)。

<a id="api-16"></a>

### RedBlackTree::contains（17）

**声明**

```magel
bool contains(int64 value);
```

**行为**

判断是否包含目标值或匹配文本。

判断树里有没有这个键。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `value` | `int64 value` | 要查的键。 | 无 |

**返回值**

`bool`。判断是否包含目标值或匹配文本。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.contains(value);
```

**源码**：[source/std/tree.mgl:262](../../source/std/tree.mgl#L262)。

<a id="api-17"></a>

### RedBlackTree::min（18）

**声明**

```magel
Option<int64> min();
```

**行为**

返回实参中的较小值；泛型入口，接受任意数量实参，相等时取后者。

最小键。 @retval 空树返回 None；否则返回最小键的 Value。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`Option<int64>`。返回实参中的较小值；泛型入口，接受任意数量实参，相等时取后者。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.min();
```

**源码**：[source/std/tree.mgl:272](../../source/std/tree.mgl#L272)。

<a id="api-18"></a>

### RedBlackTree::max（19）

**声明**

```magel
Option<int64> max();
```

**行为**

返回实参中的较大值；泛型入口，接受任意数量实参，相等时取后者。

最大键。 @retval 空树返回 None；否则返回最大键的 Value。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`Option<int64>`。返回实参中的较大值；泛型入口，接受任意数量实参，相等时取后者。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.max();
```

**源码**：[source/std/tree.mgl:283](../../source/std/tree.mgl#L283)。

<a id="api-19"></a>

### RedBlackTree::height（20）

**声明**

```magel
int64 height();
```

**行为**

树高（节点数口径；红黑性质保证不超过 2*log2(n+1)）。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`int64`。树高（节点数口径；红黑性质保证不超过 2*log2(n+1)）。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.height();
```

**源码**：[source/std/tree.mgl:293](../../source/std/tree.mgl#L293)。

<a id="api-20"></a>

### RedBlackTree::inorder（21）

**声明**

```magel
List<int64> inorder();
```

**行为**

中序遍历（升序序列）。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`List<int64>`。中序遍历（升序序列）。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.inorder();
```

**源码**：[source/std/tree.mgl:302](../../source/std/tree.mgl#L302)。

<a id="api-21"></a>

### RedBlackTree::len（22）

**声明**

```magel
int64 len();
```

**行为**

返回当前元素数量。

当前有效节点数。

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

**源码**：[source/std/tree.mgl:311](../../source/std/tree.mgl#L311)。

<a id="api-22"></a>

### RedBlackTree::clear（23）

**声明**

```magel
void clear();
```

**行为**

移除全部元素。

清空整棵树。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

无返回值。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.clear();
```

**源码**：[source/std/tree.mgl:316](../../source/std/tree.mgl#L316)。

<a id="api-23"></a>

### MinHeap::drop（24）

**声明**

```magel
void drop();
```

**行为**

释放对象拥有的资源，调用后遵守本模块失效规则。

///< 堆数组，下标 0 是最小值。

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

**源码**：[source/std/tree.mgl:330](../../source/std/tree.mgl#L330)。

<a id="api-24"></a>

### MinHeap::create（25）

**声明**

```magel
static MinHeap create();
```

**行为**

创建MinHeap，返回可供后续操作的值。

创建一个空最小堆。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`MinHeap`。创建MinHeap，返回可供后续操作的值。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::tree::MinHeap::create();
```

**源码**：[source/std/tree.mgl:335](../../source/std/tree.mgl#L335)。

<a id="api-25"></a>

### MinHeap::push（26）

**声明**

```magel
void push(int64 value);
```

**行为**

将 value 追加到列表尾部。

入堆一个值。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `value` | `int64 value` | 要入堆的值。 | 无 |

**返回值**

无返回值。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.push(value);
```

**源码**：[source/std/tree.mgl:344](../../source/std/tree.mgl#L344)。

<a id="api-26"></a>

### MinHeap::pop（27）

**声明**

```magel
Option<int64> pop();
```

**行为**

移除列表尾元素并返回值，空列表时报错。

出堆当前最小值。 @retval 空堆返回 None；否则弹出最小值的 Value。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`Option<int64>`。移除列表尾元素并返回值，空列表时报错。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.pop();
```

**源码**：[source/std/tree.mgl:354](../../source/std/tree.mgl#L354)。

<a id="api-27"></a>

### MinHeap::peek（28）

**声明**

```magel
Option<int64> peek();
```

**行为**

查看下一项并保留该项供后续消费。

查看当前最小值但不出堆。 @retval 空堆返回 None；否则返回最小值的 Value。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`Option<int64>`。查看下一项并保留该项供后续消费。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.peek();
```

**源码**：[source/std/tree.mgl:366](../../source/std/tree.mgl#L366)。

<a id="api-28"></a>

### MinHeap::len（29）

**声明**

```magel
int64 len();
```

**行为**

返回当前元素数量。

当前元素个数。

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

**源码**：[source/std/tree.mgl:376](../../source/std/tree.mgl#L376)。

<a id="api-29"></a>

### MinHeap::isEmpty（30）

**声明**

```magel
bool isEmpty();
```

**行为**

判断 Any 是否为空盒子，不公开内部执行期标签。

堆是否为空。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`bool`。判断 Any 是否为空盒子，不公开内部执行期标签。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.isEmpty();
```

**源码**：[source/std/tree.mgl:381](../../source/std/tree.mgl#L381)。

<a id="api-30"></a>

### MinHeap::clear（31）

**声明**

```magel
void clear();
```

**行为**

移除全部元素。

清空堆。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

无返回值。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.clear();
```

**源码**：[source/std/tree.mgl:386](../../source/std/tree.mgl#L386)。

<a id="api-31"></a>

### GeneralTree::drop（32）

**声明**

```magel
void drop();
```

**行为**

释放对象拥有的资源，调用后遵守本模块失效规则。

///< 当前节点数。

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

**源码**：[source/std/tree.mgl:396](../../source/std/tree.mgl#L396)。

<a id="api-32"></a>

### GeneralTree::create（33）

**声明**

```magel
static GeneralTree create();
```

**行为**

创建GeneralTree，返回可供后续操作的值。

创建一棵空通用树。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`GeneralTree`。创建GeneralTree，返回可供后续操作的值。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::tree::GeneralTree::create();
```

**源码**：[source/std/tree.mgl:401](../../source/std/tree.mgl#L401)。

<a id="api-33"></a>

### GeneralTree::addRoot（34）

**声明**

```magel
int64 addRoot(int64 value);
```

**行为**

建根（仅空树可用）。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `value` | `int64 value` | 根节点的值。 | 无 |

**返回值**

`int64`。建根（仅空树可用）。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.addRoot(value);
```

**源码**：[source/std/tree.mgl:416](../../source/std/tree.mgl#L416)。

<a id="api-34"></a>

### GeneralTree::addChild（35）

**声明**

```magel
int64 addChild(int64 cursor, int64 value);
```

**行为**

给指定节点追加一个孩子（排在已有孩子末尾）。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `cursor` | `int64 cursor` | 父节点下标。 | 无 |
| `value` | `int64 value` | 孩子的值。 | 无 |

**返回值**

`int64`。给指定节点追加一个孩子（排在已有孩子末尾）。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.addChild(cursor, value);
```

**源码**：[source/std/tree.mgl:433](../../source/std/tree.mgl#L433)。

<a id="api-35"></a>

### GeneralTree::valueAt（36）

**声明**

```magel
int64 valueAt(int64 cursor);
```

**行为**

读取节点值。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `cursor` | `int64 cursor` | 节点下标。 | 无 |

**返回值**

`int64`。读取节点值。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.valueAt(cursor);
```

**源码**：[source/std/tree.mgl:449](../../source/std/tree.mgl#L449)。

<a id="api-36"></a>

### GeneralTree::children（37）

**声明**

```magel
List<int64> children(int64 cursor);
```

**行为**

列出某节点的全部孩子下标（按插入序）。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `cursor` | `int64 cursor` | 节点下标。 | 无 |

**返回值**

`List<int64>`。列出某节点的全部孩子下标（按插入序）。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.children(cursor);
```

**源码**：[source/std/tree.mgl:456](../../source/std/tree.mgl#L456)。

<a id="api-37"></a>

### GeneralTree::preorder（38）

**声明**

```magel
List<int64> preorder();
```

**行为**

先序遍历（节点 - 各孩子依次）。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`List<int64>`。先序遍历（节点 - 各孩子依次）。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.preorder();
```

**源码**：[source/std/tree.mgl:465](../../source/std/tree.mgl#L465)。

<a id="api-38"></a>

### GeneralTree::levelOrder（39）

**声明**

```magel
List<int64> levelOrder();
```

**行为**

层序遍历。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`List<int64>`。层序遍历。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.levelOrder();
```

**源码**：[source/std/tree.mgl:474](../../source/std/tree.mgl#L474)。

<a id="api-39"></a>

### GeneralTree::height（40）

**声明**

```magel
int64 height();
```

**行为**

树高（节点数口径：单节点 1，空树 0）。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`int64`。树高（节点数口径：单节点 1，空树 0）。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.height();
```

**源码**：[source/std/tree.mgl:483](../../source/std/tree.mgl#L483)。

<a id="api-40"></a>

### GeneralTree::len（41）

**声明**

```magel
int64 len();
```

**行为**

返回当前元素数量。

当前节点数。

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

**源码**：[source/std/tree.mgl:492](../../source/std/tree.mgl#L492)。

<a id="api-41"></a>

### GeneralTree::toBinary（42）

**声明**

```magel
BinarySearchTree toBinary();
```

**行为**

转成二叉表示：左孩子 = 第一个孩子，右孩子 = 下一个兄弟。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`BinarySearchTree`。结构等价的二叉树；注意 BST 有序性不因此成立。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.toBinary();
```

**源码**：[source/std/tree.mgl:498](../../source/std/tree.mgl#L498)。

<a id="api-42"></a>

### GeneralTree::fromBinary（43）

**声明**

```magel
static GeneralTree fromBinary(BinarySearchTree tree);
```

**行为**

从二叉表示构造通用树：第一个孩子 = 左孩子，下一个兄弟 = 右孩子。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `tree` | `BinarySearchTree tree` | 源二叉树（如 BinarySearchTree / toBinary 的产物）。 | 无 |

**返回值**

`GeneralTree`。从二叉表示构造通用树：第一个孩子 = 左孩子，下一个兄弟 = 右孩子。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::tree::GeneralTree::fromBinary(tree);
```

**源码**：[source/std/tree.mgl:515](../../source/std/tree.mgl#L515)。

<a id="api-43"></a>

### GeneralTree::clear（44）

**声明**

```magel
void clear();
```

**行为**

移除全部元素。

清空整棵树。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

无返回值。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.clear();
```

**源码**：[source/std/tree.mgl:531](../../source/std/tree.mgl#L531)。

<a id="api-44"></a>

### GeneralTree::rootNode（45）

**声明**

```magel
int64 rootNode();
```

**行为**

根下标：非空树恒为 0（节点按分配顺序从 0 编号，首个节点是根）。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`int64`。根下标：非空树恒为 0（节点按分配顺序从 0 编号，首个节点是根）。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.rootNode();
```

**源码**：[source/std/tree.mgl:539](../../source/std/tree.mgl#L539)。
