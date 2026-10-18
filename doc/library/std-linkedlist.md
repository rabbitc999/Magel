# std::linkedlist — 双向链表

[返回库目录](README.md) · [全部符号](symbols.md)

## 引入

```magel
import std::linkedlist;
```

## 公共约定与错误边界

LinkedList 以游标存储：values/prevs/nexts 三个平行列表，下标即节点号，`0 - 1` 表示空指针；被删节点回收进 free 列表复用，删除后对应槽的值标记为 -2。pushFront/pushBack 返回新节点游标，insertBefore/insertAfter 在游标前后插入，removeAt 按游标摘除；游标在未发生指向它的删除操作前一直有效。首尾两端插删是 O(1)，按游标插删也是 O(1)，按值查找与按序迭代是 O(n)。fromArray 逐项尾插，toArray 返回拥有独立存储的值序列。容器实现 Drop，同时满足 Iterator<int64>。全部结构操作由宿主胶水 glue/std_linkedlist.hpp 提供，裁剪宏 NO_MAGEL_STD_LINKEDLIST。

## 参考代码

聚合入口的具体操作参见对应子模块；本页类型/方法的调用形式列在下方。

## 类型、枚举、常量与字段

公开字段按源码原样列出；句柄和游标字段用于包装器状态，通常通过方法操作。枚举的括号表示变体载荷。

| 所属类型 | 声明 | 说明 |
| --- | --- | --- |
| 模块 | <a id="symbol-304"></a>`struct ListReply` | 链表胶水回执；槽布局必须与 glue/std_linkedlist.hpp 的 Reply 一致。 |
| ListReply | <a id="symbol-305"></a>`String error` | 本次胶水调用的错误说明；成功时为空串。 |
| ListReply | <a id="symbol-306"></a>`int64 cursor` | 新分配节点的游标。 |
| ListReply | <a id="symbol-307"></a>`int64 flag` | 布尔结果：1 = 成功 / 非空 / 存在。 |
| ListReply | <a id="symbol-308"></a>`int64 value` | 弹出或查到的值。 |
| ListReply | <a id="symbol-309"></a>`int64 head` | 更新后的首节点下标。 |
| ListReply | <a id="symbol-310"></a>`int64 tail` | 更新后的尾节点下标。 |
| ListReply | <a id="symbol-311"></a>`int64 count` | 更新后的有效节点数。 |
| ListReply | <a id="symbol-312"></a>`List<int64> outValues` | 更新后的值列表。 |
| ListReply | <a id="symbol-313"></a>`List<int64> outPrevs` | 更新后的前驱列表。 |
| ListReply | <a id="symbol-314"></a>`List<int64> outNexts` | 更新后的后继列表。 |
| ListReply | <a id="symbol-315"></a>`List<int64> outFree` | 更新后的空闲槽列表。 |
| ListReply | <a id="symbol-316"></a>`List<int64> out` | 按链表顺序导出的值序列。 |
| 模块 | <a id="symbol-317"></a>`struct LinkedList : Iterator<int64>, Drop` | 双向链表：首尾两端 O(1) 插删，游标位置 O(1) 插删与摘除。 |
| LinkedList | <a id="symbol-318"></a>`List<int64> values` | 各节点值，下标即槽号。 |
| LinkedList | <a id="symbol-319"></a>`List<int64> prevs` | 前驱下标，无前驱为 -1。 |
| LinkedList | <a id="symbol-320"></a>`List<int64> nexts` | 后继下标，无后继为 -1。 |
| LinkedList | <a id="symbol-321"></a>`List<int64> freeSlots` | 已删除可复用的槽号。 |
| LinkedList | <a id="symbol-322"></a>`int64 head` | 首节点下标，空表为 -1。 |
| LinkedList | <a id="symbol-323"></a>`int64 tail` | 尾节点下标，空表为 -1。 |
| LinkedList | <a id="symbol-324"></a>`int64 count` | 当前有效节点数。 |

## 函数索引

| 名称 | 声明 |
| --- | --- |
| [LinkedList::drop](#api-0) | `void drop()` |
| [LinkedList::create](#api-1) | `static LinkedList create()` |
| [LinkedList::pushFront](#api-2) | `int64 pushFront(int64 value)` |
| [LinkedList::pushBack](#api-3) | `int64 pushBack(int64 value)` |
| [LinkedList::popFront](#api-4) | `Option<int64> popFront()` |
| [LinkedList::popBack](#api-5) | `Option<int64> popBack()` |
| [LinkedList::insertBefore](#api-6) | `int64 insertBefore(int64 cursor, int64 value)` |
| [LinkedList::insertAfter](#api-7) | `int64 insertAfter(int64 cursor, int64 value)` |
| [LinkedList::removeAt](#api-8) | `Option<int64> removeAt(int64 cursor)` |
| [LinkedList::reverse](#api-9) | `void reverse()` |
| [LinkedList::toArray](#api-10) | `List<int64> toArray()` |
| [LinkedList::fromArray](#api-11) | `static LinkedList fromArray(List<int64> items)` |
| [LinkedList::find](#api-12) | `int64 find(int64 value)` |
| [LinkedList::contains](#api-13) | `bool contains(int64 value)` |
| [LinkedList::front](#api-14) | `Option<int64> front()` |
| [LinkedList::back](#api-15) | `Option<int64> back()` |
| [LinkedList::len](#api-16) | `int64 len()` |
| [LinkedList::isEmpty](#api-17) | `bool isEmpty()` |
| [LinkedList::iterator](#api-18) | `Option<int64> iterator(int64 index)` |
| [LinkedList::clear](#api-19) | `void clear()` |
| [LinkedList::syncFrom](#api-20) | `void syncFrom(ListReply reply)` |

<a id="api-0"></a>

### LinkedList::drop（1）

**声明**

```magel
void drop();
```

**行为**

释放对象拥有的资源，调用后遵守本模块失效规则。

///< 有效节点数。

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

**源码**：[source/std/linkedlist.mgl:61](../../source/std/linkedlist.mgl#L61)。

<a id="api-1"></a>

### LinkedList::create（2）

**声明**

```magel
static LinkedList create();
```

**行为**

创建LinkedList，返回可供后续操作的值。

创建空链表。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`LinkedList`。创建LinkedList，返回可供后续操作的值。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::linkedlist::LinkedList::create();
```

**源码**：[source/std/linkedlist.mgl:66](../../source/std/linkedlist.mgl#L66)。

<a id="api-2"></a>

### LinkedList::pushFront（3）

**声明**

```magel
int64 pushFront(int64 value);
```

**行为**

将 value 插入首部。

头插一个值。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `value` | `int64 value` | 要插入的值。 | 无 |

**返回值**

`int64`。将 value 插入首部。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.pushFront(value);
```

**源码**：[source/std/linkedlist.mgl:85](../../source/std/linkedlist.mgl#L85)。

<a id="api-3"></a>

### LinkedList::pushBack（4）

**声明**

```magel
int64 pushBack(int64 value);
```

**行为**

将 value 追加到尾部。

尾插一个值。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `value` | `int64 value` | 要插入的值。 | 无 |

**返回值**

`int64`。将 value 追加到尾部。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.pushBack(value);
```

**源码**：[source/std/linkedlist.mgl:98](../../source/std/linkedlist.mgl#L98)。

<a id="api-4"></a>

### LinkedList::popFront（5）

**声明**

```magel
Option<int64> popFront();
```

**行为**

移除首部并返回 Option；空时 None。

弹出首节点。 @retval 空表返回 None；否则返回首节点的 Value。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`Option<int64>`。移除首部并返回 Option；空时 None。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.popFront();
```

**源码**：[source/std/linkedlist.mgl:110](../../source/std/linkedlist.mgl#L110)。

<a id="api-5"></a>

### LinkedList::popBack（6）

**声明**

```magel
Option<int64> popBack();
```

**行为**

移除尾部并返回 Option；空时 None。

弹出尾节点。 @retval 空表返回 None；否则返回尾节点的 Value。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`Option<int64>`。移除尾部并返回 Option；空时 None。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.popBack();
```

**源码**：[source/std/linkedlist.mgl:123](../../source/std/linkedlist.mgl#L123)。

<a id="api-6"></a>

### LinkedList::insertBefore（7）

**声明**

```magel
int64 insertBefore(int64 cursor, int64 value);
```

**行为**

在指定游标前插入。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `cursor` | `int64 cursor` | 目标节点游标。 | 无 |
| `value` | `int64 value` | 要插入的值。 | 无 |

**返回值**

`int64`。在指定游标前插入。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.insertBefore(cursor, value);
```

**源码**：[source/std/linkedlist.mgl:138](../../source/std/linkedlist.mgl#L138)。

<a id="api-7"></a>

### LinkedList::insertAfter（8）

**声明**

```magel
int64 insertAfter(int64 cursor, int64 value);
```

**行为**

在指定游标后插入。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `cursor` | `int64 cursor` | 目标节点游标。 | 无 |
| `value` | `int64 value` | 要插入的值。 | 无 |

**返回值**

`int64`。在指定游标后插入。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.insertAfter(cursor, value);
```

**源码**：[source/std/linkedlist.mgl:153](../../source/std/linkedlist.mgl#L153)。

<a id="api-8"></a>

### LinkedList::removeAt（9）

**声明**

```magel
Option<int64> removeAt(int64 cursor);
```

**行为**

摘除指定游标的节点。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `cursor` | `int64 cursor` | 节点游标。 | 无 |

**返回值**

`Option<int64>`。摘除指定游标的节点。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.removeAt(cursor);
```

**源码**：[source/std/linkedlist.mgl:167](../../source/std/linkedlist.mgl#L167)。

<a id="api-9"></a>

### LinkedList::reverse（10）

**声明**

```magel
void reverse();
```

**行为**

原地反转链表。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

无返回值。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.reverse();
```

**源码**：[source/std/linkedlist.mgl:179](../../source/std/linkedlist.mgl#L179)。

<a id="api-10"></a>

### LinkedList::toArray（11）

**声明**

```magel
List<int64> toArray();
```

**行为**

复制成由调用者作用域管理的独立运行时数组。

按链表顺序导出值序列。 @retval 值列表，拥有独立存储。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`List<int64>`。复制成由调用者作用域管理的独立运行时数组。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.toArray();
```

**源码**：[source/std/linkedlist.mgl:189](../../source/std/linkedlist.mgl#L189)。

<a id="api-11"></a>

### LinkedList::fromArray（12）

**声明**

```magel
static LinkedList fromArray(List<int64> items);
```

**行为**

从连续数组复制元素；带 count 的重载由调用者保证有效范围。

从值序列构建链表（按序尾插）。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `items` | `List<int64> items` | 源值列表。 | 无 |

**返回值**

`LinkedList`。从连续数组复制元素；带 count 的重载由调用者保证有效范围。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::linkedlist::LinkedList::fromArray(items);
```

**源码**：[source/std/linkedlist.mgl:200](../../source/std/linkedlist.mgl#L200)。

<a id="api-12"></a>

### LinkedList::find（13）

**声明**

```magel
int64 find(int64 value);
```

**行为**

查找首个匹配项，返回下标或可选载荷；具体未找到表示见返回类型及模块约定。

按链表顺序查找值。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `value` | `int64 value` | 目标值。 | 无 |

**返回值**

`int64`。查找首个匹配项，返回下标或可选载荷；具体未找到表示见返回类型及模块约定。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.find(value);
```

**源码**：[source/std/linkedlist.mgl:213](../../source/std/linkedlist.mgl#L213)。

<a id="api-13"></a>

### LinkedList::contains（14）

**声明**

```magel
bool contains(int64 value);
```

**行为**

判断是否包含目标值或匹配文本。

含不含某值。

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

**源码**：[source/std/linkedlist.mgl:223](../../source/std/linkedlist.mgl#L223)。

<a id="api-14"></a>

### LinkedList::front（15）

**声明**

```magel
Option<int64> front();
```

**行为**

首节点的值。 @retval 空表返回 None。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`Option<int64>`。首节点的值。 @retval 空表返回 None。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.front();
```

**源码**：[source/std/linkedlist.mgl:229](../../source/std/linkedlist.mgl#L229)。

<a id="api-15"></a>

### LinkedList::back（16）

**声明**

```magel
Option<int64> back();
```

**行为**

尾节点的值。 @retval 空表返回 None。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`Option<int64>`。尾节点的值。 @retval 空表返回 None。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.back();
```

**源码**：[source/std/linkedlist.mgl:238](../../source/std/linkedlist.mgl#L238)。

<a id="api-16"></a>

### LinkedList::len（17）

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

**源码**：[source/std/linkedlist.mgl:246](../../source/std/linkedlist.mgl#L246)。

<a id="api-17"></a>

### LinkedList::isEmpty（18）

**声明**

```magel
bool isEmpty();
```

**行为**

判断 Any 是否为空盒子，不公开内部执行期标签。

是否为空表。

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

**源码**：[source/std/linkedlist.mgl:251](../../source/std/linkedlist.mgl#L251)。

<a id="api-18"></a>

### LinkedList::iterator（19）

**声明**

```magel
Option<int64> iterator(int64 index);
```

**行为**

读取指定迭代位置；无元素时返回 None。

按链表顺序迭代（Iterator 接口）。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `index` | `int64 index` | 迭代序号。 | 无 |

**返回值**

`Option<int64>`。读取指定迭代位置；无元素时返回 None。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.iterator(index);
```

**源码**：[source/std/linkedlist.mgl:258](../../source/std/linkedlist.mgl#L258)。

<a id="api-19"></a>

### LinkedList::clear（20）

**声明**

```magel
void clear();
```

**行为**

移除全部元素。

清空链表。

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

**源码**：[source/std/linkedlist.mgl:272](../../source/std/linkedlist.mgl#L272)。

<a id="api-20"></a>

### LinkedList::syncFrom（21）

**声明**

```magel
void syncFrom(ListReply reply);
```

**行为**

把胶水回执里的更新状态同步回自身字段。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `reply` | `ListReply reply` | 胶水回执结构，调用后槽位被写回 | 无 |

**返回值**

无返回值。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.syncFrom(reply);
```

**源码**：[source/std/linkedlist.mgl:283](../../source/std/linkedlist.mgl#L283)。
