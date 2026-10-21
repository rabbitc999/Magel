# std::linkedlist — 双向链表

[返回库目录](README.md) · [全部符号](symbols.md)

## 引入

```magel
import std::linkedlist;
```

## 公共约定与错误边界

LinkedList 以私有拥有型 opcode 保存持久双向节点及游标索引，保留 Iterator<int64>。普通复制深复制节点并在副本内保留对应游标。首尾及按游标插删为平均 O(1)；按值查询和按序迭代 O(n)。游标不重复使用，删除或 clear 后永久失效；toArray 显式导出独立顺序快照。裁剪宏 NO_MAGEL_STD_LINKEDLIST。

## 参考代码

聚合入口的具体操作参见对应子模块；本页类型/方法的调用形式列在下方。

## 类型、枚举、常量与字段

公开字段按源码原样列出；句柄和游标字段用于包装器状态，通常通过方法操作。枚举的括号表示变体载荷。

| 所属类型 | 声明 | 说明 |
| --- | --- | --- |
| 模块 | <a id="symbol-306"></a>`struct LinkedList : Iterator<int64>` | 持久双向链表，值复制独立，旧游标在删除或 clear 后永久失效。 |

## 函数索引

| 名称 | 声明 |
| --- | --- |
| [LinkedList::create](#api-0) | `static LinkedList create()` |
| [LinkedList::pushFront](#api-1) | `int64 pushFront(int64 value)` |
| [LinkedList::pushBack](#api-2) | `int64 pushBack(int64 value)` |
| [LinkedList::insertBefore](#api-3) | `int64 insertBefore(int64 cursor, int64 value)` |
| [LinkedList::insertAfter](#api-4) | `int64 insertAfter(int64 cursor, int64 value)` |
| [LinkedList::removeAt](#api-5) | `Option<int64> removeAt(int64 cursor)` |
| [LinkedList::popFront](#api-6) | `Option<int64> popFront()` |
| [LinkedList::popBack](#api-7) | `Option<int64> popBack()` |
| [LinkedList::reverse](#api-8) | `void reverse()` |
| [LinkedList::toArray](#api-9) | `List<int64> toArray()` |
| [LinkedList::fromArray](#api-10) | `static LinkedList fromArray(List<int64> items)` |
| [LinkedList::find](#api-11) | `int64 find(int64 value)` |
| [LinkedList::contains](#api-12) | `bool contains(int64 value)` |
| [LinkedList::front](#api-13) | `Option<int64> front()` |
| [LinkedList::back](#api-14) | `Option<int64> back()` |
| [LinkedList::len](#api-15) | `int64 len()` |
| [LinkedList::isEmpty](#api-16) | `bool isEmpty()` |
| [LinkedList::iterator](#api-17) | `Option<int64> iterator(int64 index)` |
| [LinkedList::clear](#api-18) | `void clear()` |

<a id="api-0"></a>

### LinkedList::create（1）

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

**源码**：[source/std/linkedlist.mgl:18](../../source/std/linkedlist.mgl#L18)。

<a id="api-1"></a>

### LinkedList::pushFront（2）

**声明**

```magel
int64 pushFront(int64 value);
```

**行为**

将 value 插入首部。

头插并返回新节点游标。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `value` | `int64 value` | 输入值或写入的新值 | 无 |

**返回值**

`int64`。将 value 插入首部。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.pushFront(value);
```

**源码**：[source/std/linkedlist.mgl:20](../../source/std/linkedlist.mgl#L20)。

<a id="api-2"></a>

### LinkedList::pushBack（3）

**声明**

```magel
int64 pushBack(int64 value);
```

**行为**

将 value 追加到尾部。

尾插并返回新节点游标。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `value` | `int64 value` | 输入值或写入的新值 | 无 |

**返回值**

`int64`。将 value 追加到尾部。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.pushBack(value);
```

**源码**：[source/std/linkedlist.mgl:22](../../source/std/linkedlist.mgl#L22)。

<a id="api-3"></a>

### LinkedList::insertBefore（4）

**声明**

```magel
int64 insertBefore(int64 cursor, int64 value);
```

**行为**

在游标前插入，无效时返回 -1。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `cursor` | `int64 cursor` | 仍有效的节点游标 | 无 |
| `value` | `int64 value` | 输入值或写入的新值 | 无 |

**返回值**

`int64`。在游标前插入，无效时返回 -1。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.insertBefore(cursor, value);
```

**源码**：[source/std/linkedlist.mgl:24](../../source/std/linkedlist.mgl#L24)。

<a id="api-4"></a>

### LinkedList::insertAfter（5）

**声明**

```magel
int64 insertAfter(int64 cursor, int64 value);
```

**行为**

在游标后插入，无效时返回 -1。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `cursor` | `int64 cursor` | 仍有效的节点游标 | 无 |
| `value` | `int64 value` | 输入值或写入的新值 | 无 |

**返回值**

`int64`。在游标后插入，无效时返回 -1。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.insertAfter(cursor, value);
```

**源码**：[source/std/linkedlist.mgl:26](../../source/std/linkedlist.mgl#L26)。

<a id="api-5"></a>

### LinkedList::removeAt（6）

**声明**

```magel
Option<int64> removeAt(int64 cursor);
```

**行为**

摘除游标节点并返回其值，无效时返回 None。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `cursor` | `int64 cursor` | 仍有效的节点游标 | 无 |

**返回值**

`Option<int64>`。摘除游标节点并返回其值，无效时返回 None。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.removeAt(cursor);
```

**源码**：[source/std/linkedlist.mgl:28](../../source/std/linkedlist.mgl#L28)。

<a id="api-6"></a>

### LinkedList::popFront（7）

**声明**

```magel
Option<int64> popFront();
```

**行为**

移除首部并返回 Option；空时 None。

弹出首节点，空表返回 None。

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

**源码**：[source/std/linkedlist.mgl:33](../../source/std/linkedlist.mgl#L33)。

<a id="api-7"></a>

### LinkedList::popBack（8）

**声明**

```magel
Option<int64> popBack();
```

**行为**

移除尾部并返回 Option；空时 None。

弹出尾节点，空表返回 None。

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

**源码**：[source/std/linkedlist.mgl:35](../../source/std/linkedlist.mgl#L35)。

<a id="api-8"></a>

### LinkedList::reverse（9）

**声明**

```magel
void reverse();
```

**行为**

原地反转，不使游标失效。

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

**源码**：[source/std/linkedlist.mgl:37](../../source/std/linkedlist.mgl#L37)。

<a id="api-9"></a>

### LinkedList::toArray（10）

**声明**

```magel
List<int64> toArray();
```

**行为**

复制成由调用者作用域管理的独立运行时数组。

显式导出独立顺序快照。

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

**源码**：[source/std/linkedlist.mgl:39](../../source/std/linkedlist.mgl#L39)。

<a id="api-10"></a>

### LinkedList::fromArray（11）

**声明**

```magel
static LinkedList fromArray(List<int64> items);
```

**行为**

从连续数组复制元素；带 count 的重载由调用者保证有效范围。

从顺序值列表建立独立链表。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `items` | `List<int64> items` | 参与处理的列表元素 | 无 |

**返回值**

`LinkedList`。从连续数组复制元素；带 count 的重载由调用者保证有效范围。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::linkedlist::LinkedList::fromArray(items);
```

**源码**：[source/std/linkedlist.mgl:41](../../source/std/linkedlist.mgl#L41)。

<a id="api-11"></a>

### LinkedList::find（12）

**声明**

```magel
int64 find(int64 value);
```

**行为**

查找首个匹配项，返回下标或可选载荷；具体未找到表示见返回类型及模块约定。

首个匹配节点游标，不存在时为 -1。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `value` | `int64 value` | 输入值或写入的新值 | 无 |

**返回值**

`int64`。查找首个匹配项，返回下标或可选载荷；具体未找到表示见返回类型及模块约定。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.find(value);
```

**源码**：[source/std/linkedlist.mgl:48](../../source/std/linkedlist.mgl#L48)。

<a id="api-12"></a>

### LinkedList::contains（13）

**声明**

```magel
bool contains(int64 value);
```

**行为**

判断是否包含目标值或匹配文本。

是否包含值。

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

**源码**：[source/std/linkedlist.mgl:50](../../source/std/linkedlist.mgl#L50)。

<a id="api-13"></a>

### LinkedList::front（14）

**声明**

```magel
Option<int64> front();
```

**行为**

首节点值，空表返回 None。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`Option<int64>`。首节点值，空表返回 None。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.front();
```

**源码**：[source/std/linkedlist.mgl:52](../../source/std/linkedlist.mgl#L52)。

<a id="api-14"></a>

### LinkedList::back（15）

**声明**

```magel
Option<int64> back();
```

**行为**

尾节点值，空表返回 None。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`Option<int64>`。尾节点值，空表返回 None。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.back();
```

**源码**：[source/std/linkedlist.mgl:54](../../source/std/linkedlist.mgl#L54)。

<a id="api-15"></a>

### LinkedList::len（16）

**声明**

```magel
int64 len();
```

**行为**

返回当前元素数量。

有效节点数。

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

**源码**：[source/std/linkedlist.mgl:60](../../source/std/linkedlist.mgl#L60)。

<a id="api-16"></a>

### LinkedList::isEmpty（17）

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

**源码**：[source/std/linkedlist.mgl:62](../../source/std/linkedlist.mgl#L62)。

<a id="api-17"></a>

### LinkedList::iterator（18）

**声明**

```magel
Option<int64> iterator(int64 index);
```

**行为**

读取指定迭代位置；无元素时返回 None。

第 index 个值，越界返回 None。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `index` | `int64 index` | 从 0 开始的位置 | 无 |

**返回值**

`Option<int64>`。读取指定迭代位置；无元素时返回 None。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.iterator(index);
```

**源码**：[source/std/linkedlist.mgl:64](../../source/std/linkedlist.mgl#L64)。

<a id="api-18"></a>

### LinkedList::clear（19）

**声明**

```magel
void clear();
```

**行为**

移除全部元素。

清空节点并使旧游标永久失效。

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

**源码**：[source/std/linkedlist.mgl:70](../../source/std/linkedlist.mgl#L70)。
