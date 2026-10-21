# std::tree — 树与堆

[返回库目录](README.md) · [全部符号](symbols.md)

## 引入

```magel
import std::tree;
```

## 公共约定与错误边界

BinarySearchTree 是集合语义的二叉搜索树，重复插入返回 false，中序即升序；删除双孩子节点取中序后继替换，被删节点槽回收复用。RedBlackTree 使用宿主持久红黑节点，insert/remove 原地旋转及变色修复；重复插入或删除不存在的键返回 false，validate 校验排序、父指针、根颜色、红节点约束及黑高，删除节点槽回收复用。MinHeap 是最小二叉堆，可当优先队列用，相等键的出堆顺序不保证插入序。GeneralTree 用"第一个孩子 / 下一个兄弟"存储，toBinary 与 fromBinary 按"左孩子 = 第一个孩子、右孩子 = 下一个兄弟"互转，转换本身不排序、不保证 BST 有序性。树高按节点数计（单节点 1，空树 0）；树与堆由 opcode 独立拥有持久状态，普通复制深复制；snapshot 显式导出独立节点或堆数组，遍历只导出结果。裁剪宏 NO_MAGEL_STD_TREE。

## 参考代码

聚合入口的具体操作参见对应子模块；本页类型/方法的调用形式列在下方。

## 类型、枚举、常量与字段

公开字段按源码原样列出；句柄和游标字段用于包装器状态，通常通过方法操作。枚举的括号表示变体载荷。

| 所属类型 | 声明 | 说明 |
| --- | --- | --- |
| 模块 | <a id="symbol-457"></a>`struct TreeSnapshot` | 独立节点快照，live 标识有效槽；空孩子及空根使用 -1。 |
| TreeSnapshot | <a id="symbol-458"></a>`List<int64> keys` | 节点槽键值。 |
| TreeSnapshot | <a id="symbol-459"></a>`List<int64> lefts` | 左孩子下标，空孩子为 -1。 |
| TreeSnapshot | <a id="symbol-460"></a>`List<int64> rights` | 右孩子下标，空孩子为 -1。 |
| TreeSnapshot | <a id="symbol-461"></a>`List<int64> live` | 有效槽标记，1 为有效。 |
| TreeSnapshot | <a id="symbol-462"></a>`int64 root` | 根节点下标，空树为 -1。 |
| TreeSnapshot | <a id="symbol-463"></a>`int64 count` | 有效节点数。 |
| 模块 | <a id="symbol-464"></a>`struct GeneralTreeSnapshot` | 独立通用树快照，首孩子与次兄弟保留原节点号。 |
| GeneralTreeSnapshot | <a id="symbol-465"></a>`List<int64> values` | 节点槽值。 |
| GeneralTreeSnapshot | <a id="symbol-466"></a>`List<int64> firstChild` | 首孩子下标，空孩子为 -1。 |
| GeneralTreeSnapshot | <a id="symbol-467"></a>`List<int64> nextSibling` | 次兄弟下标，空兄弟为 -1。 |
| GeneralTreeSnapshot | <a id="symbol-468"></a>`List<int64> live` | 有效槽标记，1 为有效。 |
| GeneralTreeSnapshot | <a id="symbol-469"></a>`int64 root` | 根节点号，空树为 -1。 |
| GeneralTreeSnapshot | <a id="symbol-470"></a>`int64 count` | 有效节点数。 |
| 模块 | <a id="symbol-471"></a>`opcode BinarySearchTree` | 宿主持久二叉搜索集合，普通复制深复制，snapshot 显式导出节点。 |
| 模块 | <a id="symbol-472"></a>`opcode RedBlackTree` | 宿主持久红黑集合，插入与删除原地修复，普通复制深复制。 |
| 模块 | <a id="symbol-473"></a>`opcode MinHeap` | 宿主持久最小堆，普通复制深复制，snapshot 显式导出堆数组。 |
| 模块 | <a id="symbol-474"></a>`opcode GeneralTree` | 宿主持久首孩子、次兄弟树，支持独立二叉表示转换。 |

## 函数索引

| 名称 | 声明 |
| --- | --- |
| [BinarySearchTree::create](#api-0) | `static BinarySearchTree create()` |
| [BinarySearchTree::len](#api-1) | `int64 len()` |
| [BinarySearchTree::clear](#api-2) | `void clear()` |
| [BinarySearchTree::insert](#api-3) | `bool insert(int64 value)` |
| [BinarySearchTree::remove](#api-4) | `bool remove(int64 value)` |
| [BinarySearchTree::contains](#api-5) | `bool contains(int64 value)` |
| [BinarySearchTree::min](#api-6) | `Option<int64> min()` |
| [BinarySearchTree::max](#api-7) | `Option<int64> max()` |
| [BinarySearchTree::height](#api-8) | `int64 height()` |
| [BinarySearchTree::inorder](#api-9) | `List<int64> inorder()` |
| [BinarySearchTree::preorder](#api-10) | `List<int64> preorder()` |
| [BinarySearchTree::postorder](#api-11) | `List<int64> postorder()` |
| [BinarySearchTree::snapshot](#api-12) | `TreeSnapshot snapshot()` |
| [RedBlackTree::create](#api-13) | `static RedBlackTree create()` |
| [RedBlackTree::insert](#api-14) | `bool insert(int64 value)` |
| [RedBlackTree::remove](#api-15) | `bool remove(int64 value)` |
| [RedBlackTree::contains](#api-16) | `bool contains(int64 value)` |
| [RedBlackTree::len](#api-17) | `int64 len()` |
| [RedBlackTree::height](#api-18) | `int64 height()` |
| [RedBlackTree::clear](#api-19) | `void clear()` |
| [RedBlackTree::min](#api-20) | `Option<int64> min()` |
| [RedBlackTree::max](#api-21) | `Option<int64> max()` |
| [RedBlackTree::validate](#api-22) | `bool validate()` |
| [RedBlackTree::inorder](#api-23) | `List<int64> inorder()` |
| [MinHeap::create](#api-24) | `static MinHeap create()` |
| [MinHeap::len](#api-25) | `int64 len()` |
| [MinHeap::clear](#api-26) | `void clear()` |
| [MinHeap::push](#api-27) | `void push(int64 value)` |
| [MinHeap::pop](#api-28) | `Option<int64> pop()` |
| [MinHeap::peek](#api-29) | `Option<int64> peek()` |
| [MinHeap::isEmpty](#api-30) | `bool isEmpty()` |
| [MinHeap::snapshot](#api-31) | `List<int64> snapshot()` |
| [GeneralTree::create](#api-32) | `static GeneralTree create()` |
| [GeneralTree::len](#api-33) | `int64 len()` |
| [GeneralTree::clear](#api-34) | `void clear()` |
| [GeneralTree::addRoot](#api-35) | `int64 addRoot(int64 value)` |
| [GeneralTree::addChild](#api-36) | `int64 addChild(int64 cursor, int64 value)` |
| [GeneralTree::valueAt](#api-37) | `int64 valueAt(int64 cursor)` |
| [GeneralTree::rootNode](#api-38) | `int64 rootNode()` |
| [GeneralTree::children](#api-39) | `List<int64> children(int64 cursor)` |
| [GeneralTree::preorder](#api-40) | `List<int64> preorder()` |
| [GeneralTree::levelOrder](#api-41) | `List<int64> levelOrder()` |
| [GeneralTree::height](#api-42) | `int64 height()` |
| [GeneralTree::toBinary](#api-43) | `BinarySearchTree toBinary()` |
| [GeneralTree::fromBinary](#api-44) | `static GeneralTree fromBinary(BinarySearchTree tree)` |
| [GeneralTree::snapshot](#api-45) | `GeneralTreeSnapshot snapshot()` |

<a id="api-0"></a>

### BinarySearchTree::create（1）

**声明**

```magel
static BinarySearchTree create();
```

**行为**

创建BinarySearchTree，返回可供后续操作的值。

创建空容器。

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

**源码**：[source/std/tree.mgl:36](../../source/std/tree.mgl#L36)。

<a id="api-1"></a>

### BinarySearchTree::len（2）

**声明**

```magel
int64 len();
```

**行为**

返回当前元素数量。

有效元素数。

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

**源码**：[source/std/tree.mgl:38](../../source/std/tree.mgl#L38)。

<a id="api-2"></a>

### BinarySearchTree::clear（3）

**声明**

```magel
void clear();
```

**行为**

移除全部元素。

清空所有节点和回收槽。

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

**源码**：[source/std/tree.mgl:40](../../source/std/tree.mgl#L40)。

<a id="api-3"></a>

### BinarySearchTree::insert（4）

**声明**

```magel
bool insert(int64 value);
```

**行为**

在 index 之前插入 value。

插入键，重复键返回 false。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `value` | `int64 value` | 输入值或写入的新值 | 无 |

**返回值**

`bool`。在 index 之前插入 value。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.insert(value);
```

**源码**：[source/std/tree.mgl:42](../../source/std/tree.mgl#L42)。

<a id="api-4"></a>

### BinarySearchTree::remove（5）

**声明**

```magel
bool remove(int64 value);
```

**行为**

删除指定位置或键的元素。

删除键，不存在返回 false。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `value` | `int64 value` | 输入值或写入的新值 | 无 |

**返回值**

`bool`。删除指定位置或键的元素。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.remove(value);
```

**源码**：[source/std/tree.mgl:44](../../source/std/tree.mgl#L44)。

<a id="api-5"></a>

### BinarySearchTree::contains（6）

**声明**

```magel
bool contains(int64 value);
```

**行为**

判断是否包含目标值或匹配文本。

查询键是否存在。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `value` | `int64 value` | 输入值或写入的新值 | 无 |

**返回值**

`bool`。判断是否包含目标值或匹配文本。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.contains(value);
```

**源码**：[source/std/tree.mgl:46](../../source/std/tree.mgl#L46)。

<a id="api-6"></a>

### BinarySearchTree::min（7）

**声明**

```magel
Option<int64> min();
```

**行为**

返回实参中的较小值；泛型入口，接受任意数量实参，相等时取后者。

最小键，空树返回 None。

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

**源码**：[source/std/tree.mgl:48](../../source/std/tree.mgl#L48)。

<a id="api-7"></a>

### BinarySearchTree::max（8）

**声明**

```magel
Option<int64> max();
```

**行为**

返回实参中的较大值；泛型入口，接受任意数量实参，相等时取后者。

最大键，空树返回 None。

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

**源码**：[source/std/tree.mgl:50](../../source/std/tree.mgl#L50)。

<a id="api-8"></a>

### BinarySearchTree::height（9）

**声明**

```magel
int64 height();
```

**行为**

最大路径节点数，空树为零。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`int64`。最大路径节点数，空树为零。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.height();
```

**源码**：[source/std/tree.mgl:52](../../source/std/tree.mgl#L52)。

<a id="api-9"></a>

### BinarySearchTree::inorder（10）

**声明**

```magel
List<int64> inorder();
```

**行为**

独立中序键快照。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`List<int64>`。独立中序键快照。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.inorder();
```

**源码**：[source/std/tree.mgl:54](../../source/std/tree.mgl#L54)。

<a id="api-10"></a>

### BinarySearchTree::preorder（11）

**声明**

```magel
List<int64> preorder();
```

**行为**

独立先序键快照。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`List<int64>`。独立先序键快照。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.preorder();
```

**源码**：[source/std/tree.mgl:56](../../source/std/tree.mgl#L56)。

<a id="api-11"></a>

### BinarySearchTree::postorder（12）

**声明**

```magel
List<int64> postorder();
```

**行为**

独立后序键快照。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`List<int64>`。独立后序键快照。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.postorder();
```

**源码**：[source/std/tree.mgl:58](../../source/std/tree.mgl#L58)。

<a id="api-12"></a>

### BinarySearchTree::snapshot（13）

**声明**

```magel
TreeSnapshot snapshot();
```

**行为**

显式导出独立节点结构。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`TreeSnapshot`。显式导出独立节点结构。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.snapshot();
```

**源码**：[source/std/tree.mgl:60](../../source/std/tree.mgl#L60)。

<a id="api-13"></a>

### RedBlackTree::create（14）

**声明**

```magel
static RedBlackTree create();
```

**行为**

创建RedBlackTree，返回可供后续操作的值。

创建空红黑树。

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

**源码**：[source/std/tree.mgl:81](../../source/std/tree.mgl#L81)。

<a id="api-14"></a>

### RedBlackTree::insert（15）

**声明**

```magel
bool insert(int64 value);
```

**行为**

在 index 之前插入 value。

插入不存在的键；重复键返回 false。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `value` | `int64 value` | 输入值或写入的新值 | 无 |

**返回值**

`bool`。在 index 之前插入 value。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.insert(value);
```

**源码**：[source/std/tree.mgl:83](../../source/std/tree.mgl#L83)。

<a id="api-15"></a>

### RedBlackTree::remove（16）

**声明**

```magel
bool remove(int64 value);
```

**行为**

删除指定位置或键的元素。

删除键并修复黑高与颜色；不存在时返回 false。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `value` | `int64 value` | 输入值或写入的新值 | 无 |

**返回值**

`bool`。删除指定位置或键的元素。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.remove(value);
```

**源码**：[source/std/tree.mgl:85](../../source/std/tree.mgl#L85)。

<a id="api-16"></a>

### RedBlackTree::contains（17）

**声明**

```magel
bool contains(int64 value);
```

**行为**

判断是否包含目标值或匹配文本。

查询键是否存在。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `value` | `int64 value` | 输入值或写入的新值 | 无 |

**返回值**

`bool`。判断是否包含目标值或匹配文本。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.contains(value);
```

**源码**：[source/std/tree.mgl:87](../../source/std/tree.mgl#L87)。

<a id="api-17"></a>

### RedBlackTree::len（18）

**声明**

```magel
int64 len();
```

**行为**

返回当前元素数量。

当前有效键数。

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

**源码**：[source/std/tree.mgl:89](../../source/std/tree.mgl#L89)。

<a id="api-18"></a>

### RedBlackTree::height（19）

**声明**

```magel
int64 height();
```

**行为**

树高，空树为零。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`int64`。树高，空树为零。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.height();
```

**源码**：[source/std/tree.mgl:91](../../source/std/tree.mgl#L91)。

<a id="api-19"></a>

### RedBlackTree::clear（20）

**声明**

```magel
void clear();
```

**行为**

移除全部元素。

清空节点及回收槽。

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

**源码**：[source/std/tree.mgl:93](../../source/std/tree.mgl#L93)。

<a id="api-20"></a>

### RedBlackTree::min（21）

**声明**

```magel
Option<int64> min();
```

**行为**

返回实参中的较小值；泛型入口，接受任意数量实参，相等时取后者。

最小键；空树返回 None。

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

**源码**：[source/std/tree.mgl:95](../../source/std/tree.mgl#L95)。

<a id="api-21"></a>

### RedBlackTree::max（22）

**声明**

```magel
Option<int64> max();
```

**行为**

返回实参中的较大值；泛型入口，接受任意数量实参，相等时取后者。

最大键；空树返回 None。

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

**源码**：[source/std/tree.mgl:100](../../source/std/tree.mgl#L100)。

<a id="api-22"></a>

### RedBlackTree::validate（23）

**声明**

```magel
bool validate();
```

**行为**

检查排序、父指针、根颜色、连续红节点和黑高约束。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`bool`。检查排序、父指针、根颜色、连续红节点和黑高约束。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.validate();
```

**源码**：[source/std/tree.mgl:105](../../source/std/tree.mgl#L105)。

<a id="api-23"></a>

### RedBlackTree::inorder（24）

**声明**

```magel
List<int64> inorder();
```

**行为**

显式导出独立升序键快照。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`List<int64>`。显式导出独立升序键快照。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.inorder();
```

**源码**：[source/std/tree.mgl:107](../../source/std/tree.mgl#L107)。

<a id="api-24"></a>

### MinHeap::create（25）

**声明**

```magel
static MinHeap create();
```

**行为**

创建MinHeap，返回可供后续操作的值。

创建空容器。

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

**源码**：[source/std/tree.mgl:116](../../source/std/tree.mgl#L116)。

<a id="api-25"></a>

### MinHeap::len（26）

**声明**

```magel
int64 len();
```

**行为**

返回当前元素数量。

有效元素数。

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

**源码**：[source/std/tree.mgl:118](../../source/std/tree.mgl#L118)。

<a id="api-26"></a>

### MinHeap::clear（27）

**声明**

```magel
void clear();
```

**行为**

移除全部元素。

清空所有节点和回收槽。

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

**源码**：[source/std/tree.mgl:120](../../source/std/tree.mgl#L120)。

<a id="api-27"></a>

### MinHeap::push（28）

**声明**

```magel
void push(int64 value);
```

**行为**

将 value 追加到列表尾部。

原地插入一个值。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `value` | `int64 value` | 输入值或写入的新值 | 无 |

**返回值**

无返回值。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.push(value);
```

**源码**：[source/std/tree.mgl:122](../../source/std/tree.mgl#L122)。

<a id="api-28"></a>

### MinHeap::pop（29）

**声明**

```magel
Option<int64> pop();
```

**行为**

移除列表尾元素并返回值，空列表时报错。

弹出最小值，空堆返回 None。

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

**源码**：[source/std/tree.mgl:124](../../source/std/tree.mgl#L124)。

<a id="api-29"></a>

### MinHeap::peek（30）

**声明**

```magel
Option<int64> peek();
```

**行为**

查看下一项并保留该项供后续消费。

读取最小值，空堆返回 None。

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

**源码**：[source/std/tree.mgl:126](../../source/std/tree.mgl#L126)。

<a id="api-30"></a>

### MinHeap::isEmpty（31）

**声明**

```magel
bool isEmpty();
```

**行为**

判断 Any 是否为空盒子，不公开内部执行期标签。

是否为空。

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

**源码**：[source/std/tree.mgl:128](../../source/std/tree.mgl#L128)。

<a id="api-31"></a>

### MinHeap::snapshot（32）

**声明**

```magel
List<int64> snapshot();
```

**行为**

显式导出独立堆数组。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`List<int64>`。显式导出独立堆数组。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.snapshot();
```

**源码**：[source/std/tree.mgl:130](../../source/std/tree.mgl#L130)。

<a id="api-32"></a>

### GeneralTree::create（33）

**声明**

```magel
static GeneralTree create();
```

**行为**

创建GeneralTree，返回可供后续操作的值。

创建空容器。

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

**源码**：[source/std/tree.mgl:139](../../source/std/tree.mgl#L139)。

<a id="api-33"></a>

### GeneralTree::len（34）

**声明**

```magel
int64 len();
```

**行为**

返回当前元素数量。

有效元素数。

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

**源码**：[source/std/tree.mgl:141](../../source/std/tree.mgl#L141)。

<a id="api-34"></a>

### GeneralTree::clear（35）

**声明**

```magel
void clear();
```

**行为**

移除全部元素。

清空所有节点和回收槽。

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

**源码**：[source/std/tree.mgl:143](../../source/std/tree.mgl#L143)。

<a id="api-35"></a>

### GeneralTree::addRoot（36）

**声明**

```magel
int64 addRoot(int64 value);
```

**行为**

仅空树可建立根，否则返回 -1。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `value` | `int64 value` | 输入值或写入的新值 | 无 |

**返回值**

`int64`。仅空树可建立根，否则返回 -1。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.addRoot(value);
```

**源码**：[source/std/tree.mgl:145](../../source/std/tree.mgl#L145)。

<a id="api-36"></a>

### GeneralTree::addChild（37）

**声明**

```magel
int64 addChild(int64 cursor, int64 value);
```

**行为**

在父节点末尾追加孩子，无效父节点返回 -1。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `cursor` | `int64 cursor` | 仍有效的节点游标 | 无 |
| `value` | `int64 value` | 输入值或写入的新值 | 无 |

**返回值**

`int64`。在父节点末尾追加孩子，无效父节点返回 -1。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.addChild(cursor, value);
```

**源码**：[source/std/tree.mgl:147](../../source/std/tree.mgl#L147)。

<a id="api-37"></a>

### GeneralTree::valueAt（38）

**声明**

```magel
int64 valueAt(int64 cursor);
```

**行为**

读取有效节点值，越界报错。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `cursor` | `int64 cursor` | 仍有效的节点游标 | 无 |

**返回值**

`int64`。读取有效节点值，越界报错。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.valueAt(cursor);
```

**源码**：[source/std/tree.mgl:149](../../source/std/tree.mgl#L149)。

<a id="api-38"></a>

### GeneralTree::rootNode（39）

**声明**

```magel
int64 rootNode();
```

**行为**

当前根节点号，空树为 -1。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`int64`。当前根节点号，空树为 -1。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.rootNode();
```

**源码**：[source/std/tree.mgl:151](../../source/std/tree.mgl#L151)。

<a id="api-39"></a>

### GeneralTree::children（40）

**声明**

```magel
List<int64> children(int64 cursor);
```

**行为**

独立孩子节点号列表，无效节点返回空列表。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `cursor` | `int64 cursor` | 仍有效的节点游标 | 无 |

**返回值**

`List<int64>`。独立孩子节点号列表，无效节点返回空列表。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.children(cursor);
```

**源码**：[source/std/tree.mgl:153](../../source/std/tree.mgl#L153)。

<a id="api-40"></a>

### GeneralTree::preorder（41）

**声明**

```magel
List<int64> preorder();
```

**行为**

独立先序节点号列表。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`List<int64>`。独立先序节点号列表。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.preorder();
```

**源码**：[source/std/tree.mgl:155](../../source/std/tree.mgl#L155)。

<a id="api-41"></a>

### GeneralTree::levelOrder（42）

**声明**

```magel
List<int64> levelOrder();
```

**行为**

独立层序节点号列表。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`List<int64>`。独立层序节点号列表。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.levelOrder();
```

**源码**：[source/std/tree.mgl:157](../../source/std/tree.mgl#L157)。

<a id="api-42"></a>

### GeneralTree::height（43）

**声明**

```magel
int64 height();
```

**行为**

根到叶的最大节点数，空树为零。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`int64`。根到叶的最大节点数，空树为零。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.height();
```

**源码**：[source/std/tree.mgl:159](../../source/std/tree.mgl#L159)。

<a id="api-43"></a>

### GeneralTree::toBinary（44）

**声明**

```magel
BinarySearchTree toBinary();
```

**行为**

独立转换首孩子为左孩子、次兄弟为右孩子；不保证键有序。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`BinarySearchTree`。独立转换首孩子为左孩子、次兄弟为右孩子；不保证键有序。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.toBinary();
```

**源码**：[source/std/tree.mgl:161](../../source/std/tree.mgl#L161)。

<a id="api-44"></a>

### GeneralTree::fromBinary（45）

**声明**

```magel
static GeneralTree fromBinary(BinarySearchTree tree);
```

**行为**

独立复制二叉表示，保持源根节点号及有效槽。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `tree` | `BinarySearchTree tree` | 独立转换的源二叉树 | 无 |

**返回值**

`GeneralTree`。独立复制二叉表示，保持源根节点号及有效槽。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::tree::GeneralTree::fromBinary(tree);
```

**源码**：[source/std/tree.mgl:163](../../source/std/tree.mgl#L163)。

<a id="api-45"></a>

### GeneralTree::snapshot（46）

**声明**

```magel
GeneralTreeSnapshot snapshot();
```

**行为**

显式导出独立首孩子、次兄弟结构。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`GeneralTreeSnapshot`。显式导出独立首孩子、次兄弟结构。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.snapshot();
```

**源码**：[source/std/tree.mgl:165](../../source/std/tree.mgl#L165)。
