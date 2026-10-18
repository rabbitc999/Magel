# std::collections — 集合扩展

[返回库目录](README.md) · [全部符号](symbols.md)

## 引入

```magel
import std::collections;
```

## 公共约定与错误边界

Deque、Queue、Channel 基于 List；Channel 是当前执行上下文的值队列，不是跨线程消息系统。FixedVector 与 RingBuffer 的容量固定，满时 push 返回 false；空时 pop/receive 返回 None。索引从 0 开始。

## 参考代码

完整程序：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

```powershell
magel run example/samples/std_collections_algorithms.mgl
```

<details>
<summary>展开完整参考代码</summary>

```magel
import std::collections;

import std::memory;

import std::algorithms;

bool positive(int64 value) { return value > 0; }
int64 addValues(int64 left, int64 right) { return left + right; }

int64 main() {
    List<int64> values;
    values.push(4);
    values.push(1);
    values.push(3);
    List<int64> sorted = std::algorithms::sortValue<int64>(values);
    println("{} {} {} {} {}", sorted[0], sorted[1], sorted[2],
    std::algorithms::binarySearchValue<int64>(sorted, 3),
    std::algorithms::foldValue<int64>(sorted, 0, addValues));
    println("{} {} {}", values[0], values[1], values[2]);

    List<float64> stableValues;
    stableValues.push(-0.0);
    stableValues.push(2.0);
    stableValues.push(0.0);
    stableValues.push(1.0);
    List<float64> stableSorted = std::algorithms::sortValue<float64>(stableValues);
    println("{1:1F} {2:1F} {3:1F} {4:1F}", stableSorted[0], stableSorted[1],
    stableSorted[2], stableSorted[3]);
    println("{} {} {} {} {}", std::algorithms::findValue<int64>(sorted, 4),
    std::algorithms::minValue<int64>(9, 2), std::algorithms::maxValue<int64>(9, 2),
    std::algorithms::clampValue<int64>(12, 0, 10),
    std::algorithms::allValue<int64>(sorted, positive));
    println("{} {} {} {}", std::algorithms::minValue<int64>(9, 2, 7, -1),
    std::algorithms::maxValue<int64>(9, 2, 12, 7),
    std::algorithms::minValue<int64>(5), std::algorithms::maxValue<int64>(6));

    std::collections::Deque<int64> deque = std::collections::Deque<int64>::create();
    deque.pushBack(2);
    deque.pushFront(1);
    println("{} {} {}", deque.len(), deque.popFront().unwrap(),
    deque.popBack().unwrap());

    std::collections::Queue<int64> queue = std::collections::Queue<int64>::create();
    queue.push(7);
    queue.push(8);
    println("{} {}", queue.peek().unwrap(), queue.pop().unwrap());

    std::collections::RingBuffer<int64> ring = std::collections::RingBuffer<int64>::withCapacity(2);
    println("{} {} {}", ring.push(5), ring.push(6), ring.push(7));
    println("{} {}", ring.pop().unwrap(), ring.available());

    std::collections::FixedVector<int64> fixed = std::collections::FixedVector<int64>::withCapacity(1);
    println("{} {} {}", fixed.push(9), fixed.push(10), fixed[0]);

    int64 raw[] = new[2];
    raw[0] = 11;
    raw[1] = 12;
    std::memory::Span<int64> span = std::memory::Span<int64>::from(&raw[0], len(raw));
    println("{} {} {}", span.len(), span[0], span[1]);

    std::collections::Channel<int64> channel = std::collections::Channel<int64>::create();
    channel.send(21);
    println("{} {}", channel.len(), channel.receive().unwrap());

    Pair<int64, String> pair = Pair<int64, String>::of(7, "seven");
    println("{} {}", pair.first, pair.second);
    delete raw;
    return 0;
}
```

</details>

<details>
<summary>示例输出</summary>

```text
1 3 4 1 8
4 1 3
-0.0 0.0 1.0 2.0
2 2 9 10 1
-1 12 5 6
2 1 2
7 7
1 1 0
5 1
1 0 9
2 11 12
1 21
7 seven
```

</details>

聚合入口的具体操作参见对应子模块；本页类型/方法的调用形式列在下方。

## 类型、枚举、常量与字段

公开字段按源码原样列出；句柄和游标字段用于包装器状态，通常通过方法操作。枚举的括号表示变体载荷。

| 所属类型 | 声明 | 说明 |
| --- | --- | --- |
| 模块 | <a id="symbol-22"></a>`struct Deque<T>: Iterator<T>, Index<T, int64>, Drop` | 标准集合模块：import std::collections; 后使用 std::collections::Deque 等类型。 |
| Deque | <a id="symbol-23"></a>`std::lang::List<T> values` | 按声明顺序保存的元素存储。 |
| 模块 | <a id="symbol-24"></a>`struct Queue<T>: Iterator<T>, Drop` | 先进先出的值队列；出队返回 Option，空队列返回 None。 |
| Queue | <a id="symbol-25"></a>`std::lang::List<T> values` | 按声明顺序保存的元素存储。 |
| 模块 | <a id="symbol-26"></a>`struct RingBuffer<T>: Iterator<T>, Drop` | 容量固定的环形值缓冲区；满时写入返回 false。 |
| RingBuffer | <a id="symbol-27"></a>`std::lang::List<T> values` | 按声明顺序保存的元素存储。 |
| RingBuffer | <a id="symbol-28"></a>`int64 capacity` | 最多可保存的元素数量。 |
| 模块 | <a id="symbol-29"></a>`struct FixedVector<T>: Iterator<T>, Index<T, int64>, Drop` | 容量固定的顺序容器；保存的元素数量不得超过 capacity。 |
| FixedVector | <a id="symbol-30"></a>`std::lang::List<T> values` | 按声明顺序保存的元素存储。 |
| FixedVector | <a id="symbol-31"></a>`int64 capacity` | 最多可保存的元素数量。 |
| 模块 | <a id="symbol-32"></a>`struct Channel<T>: Iterator<T>, Drop` | 当前执行上下文中的值消息队列，不提供跨线程同步。 |
| Channel | <a id="symbol-33"></a>`std::lang::List<T> pending` | 等待消费的元素或队列。 |

## 函数索引

| 名称 | 声明 |
| --- | --- |
| [Deque::create](#api-0) | `static Deque<T> create()` |
| [Deque::pushBack](#api-1) | `void pushBack(T value)` |
| [Deque::pushFront](#api-2) | `void pushFront(T value)` |
| [Deque::popBack](#api-3) | `Option<T> popBack()` |
| [Deque::popFront](#api-4) | `Option<T> popFront()` |
| [Deque::index](#api-5) | `T index(int64 key)` |
| [Deque::iterator](#api-6) | `Option<T> iterator(int64 index)` |
| [Deque::len](#api-7) | `int64 len()` |
| [Deque::isEmpty](#api-8) | `bool isEmpty()` |
| [Deque::clear](#api-9) | `void clear()` |
| [Deque::drop](#api-10) | `void drop()` |
| [Queue::create](#api-11) | `static Queue<T> create()` |
| [Queue::push](#api-12) | `void push(T value)` |
| [Queue::pop](#api-13) | `Option<T> pop()` |
| [Queue::peek](#api-14) | `Option<T> peek()` |
| [Queue::iterator](#api-15) | `Option<T> iterator(int64 index)` |
| [Queue::len](#api-16) | `int64 len()` |
| [Queue::isEmpty](#api-17) | `bool isEmpty()` |
| [Queue::drop](#api-18) | `void drop()` |
| [RingBuffer::withCapacity](#api-19) | `static RingBuffer<T> withCapacity(int64 capacity)` |
| [RingBuffer::push](#api-20) | `bool push(T value)` |
| [RingBuffer::pop](#api-21) | `Option<T> pop()` |
| [RingBuffer::iterator](#api-22) | `Option<T> iterator(int64 index)` |
| [RingBuffer::len](#api-23) | `int64 len()` |
| [RingBuffer::available](#api-24) | `int64 available()` |
| [RingBuffer::drop](#api-25) | `void drop()` |
| [FixedVector::withCapacity](#api-26) | `static FixedVector<T> withCapacity(int64 capacity)` |
| [FixedVector::push](#api-27) | `bool push(T value)` |
| [FixedVector::pop](#api-28) | `Option<T> pop()` |
| [FixedVector::index](#api-29) | `T index(int64 key)` |
| [FixedVector::iterator](#api-30) | `Option<T> iterator(int64 index)` |
| [FixedVector::len](#api-31) | `int64 len()` |
| [FixedVector::capacityValue](#api-32) | `int64 capacityValue()` |
| [FixedVector::drop](#api-33) | `void drop()` |
| [Channel::create](#api-34) | `static Channel<T> create()` |
| [Channel::send](#api-35) | `void send(T value)` |
| [Channel::receive](#api-36) | `Option<T> receive()` |
| [Channel::iterator](#api-37) | `Option<T> iterator(int64 index)` |
| [Channel::len](#api-38) | `int64 len()` |
| [Channel::drop](#api-39) | `void drop()` |

<a id="api-0"></a>

### Deque::create（1）

**声明**

```magel
static Deque<T> create();
```

**行为**

创建Deque，返回可供后续操作的值。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`Deque<T>`。创建Deque，返回可供后续操作的值。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::collections::Deque<T>::create();
```

**源码**：[source/std/collections.mgl:8](../../source/std/collections.mgl#L8)。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-1"></a>

### Deque::pushBack（2）

**声明**

```magel
void pushBack(T value);
```

**行为**

将 value 追加到尾部。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `value` | `T value` | 输入值或写入的新值 | 无 |

**返回值**

无返回值。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.pushBack(value);
```

**源码**：[source/std/collections.mgl:13](../../source/std/collections.mgl#L13)。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-2"></a>

### Deque::pushFront（3）

**声明**

```magel
void pushFront(T value);
```

**行为**

将 value 插入首部。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `value` | `T value` | 输入值或写入的新值 | 无 |

**返回值**

无返回值。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.pushFront(value);
```

**源码**：[source/std/collections.mgl:14](../../source/std/collections.mgl#L14)。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-3"></a>

### Deque::popBack（4）

**声明**

```magel
Option<T> popBack();
```

**行为**

移除尾部并返回 Option；空时 None。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`Option<T>`。移除尾部并返回 Option；空时 None。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.popBack();
```

**源码**：[source/std/collections.mgl:15](../../source/std/collections.mgl#L15)。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-4"></a>

### Deque::popFront（5）

**声明**

```magel
Option<T> popFront();
```

**行为**

移除首部并返回 Option；空时 None。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`Option<T>`。移除首部并返回 Option；空时 None。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.popFront();
```

**源码**：[source/std/collections.mgl:19](../../source/std/collections.mgl#L19)。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-5"></a>

### Deque::index（6）

**声明**

```magel
T index(int64 key);
```

**行为**

读取 key 对应的元素，供下标语法调用。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `key` | `int64 key` | 查找键或下标 | 无 |

**返回值**

`T`。读取 key 对应的元素，供下标语法调用。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.index(key);
```

**源码**：[source/std/collections.mgl:25](../../source/std/collections.mgl#L25)。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-6"></a>

### Deque::iterator（7）

**声明**

```magel
Option<T> iterator(int64 index);
```

**行为**

读取指定迭代位置；无元素时返回 None。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `index` | `int64 index` | 从 0 开始的位置 | 无 |

**返回值**

`Option<T>`。读取指定迭代位置；无元素时返回 None。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.iterator(index);
```

**源码**：[source/std/collections.mgl:26](../../source/std/collections.mgl#L26)。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-7"></a>

### Deque::len（8）

**声明**

```magel
int64 len();
```

**行为**

返回当前元素数量。

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

**源码**：[source/std/collections.mgl:30](../../source/std/collections.mgl#L30)。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-8"></a>

### Deque::isEmpty（9）

**声明**

```magel
bool isEmpty();
```

**行为**

判断 Any 是否为空盒子，不公开内部执行期标签。

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

**源码**：[source/std/collections.mgl:31](../../source/std/collections.mgl#L31)。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-9"></a>

### Deque::clear（10）

**声明**

```magel
void clear();
```

**行为**

移除全部元素。

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

**源码**：[source/std/collections.mgl:32](../../source/std/collections.mgl#L32)。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-10"></a>

### Deque::drop（11）

**声明**

```magel
void drop();
```

**行为**

释放对象拥有的资源，调用后遵守本模块失效规则。

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

**源码**：[source/std/collections.mgl:33](../../source/std/collections.mgl#L33)。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-11"></a>

### Queue::create（12）

**声明**

```magel
static Queue<T> create();
```

**行为**

创建Queue，返回可供后续操作的值。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`Queue<T>`。创建Queue，返回可供后续操作的值。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::collections::Queue<T>::create();
```

**源码**：[source/std/collections.mgl:39](../../source/std/collections.mgl#L39)。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-12"></a>

### Queue::push（13）

**声明**

```magel
void push(T value);
```

**行为**

追加元素。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `value` | `T value` | 输入值或写入的新值 | 无 |

**返回值**

无返回值。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.push(value);
```

**源码**：[source/std/collections.mgl:44](../../source/std/collections.mgl#L44)。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-13"></a>

### Queue::pop（14）

**声明**

```magel
Option<T> pop();
```

**行为**

移除并返回下一元素；空容器返回 None。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`Option<T>`。移除并返回下一元素；空容器返回 None。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.pop();
```

**源码**：[source/std/collections.mgl:45](../../source/std/collections.mgl#L45)。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-14"></a>

### Queue::peek（15）

**声明**

```magel
Option<T> peek();
```

**行为**

查看下一项并保留该项供后续消费。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`Option<T>`。查看下一项并保留该项供后续消费。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.peek();
```

**源码**：[source/std/collections.mgl:51](../../source/std/collections.mgl#L51)。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-15"></a>

### Queue::iterator（16）

**声明**

```magel
Option<T> iterator(int64 index);
```

**行为**

读取指定迭代位置；无元素时返回 None。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `index` | `int64 index` | 从 0 开始的位置 | 无 |

**返回值**

`Option<T>`。读取指定迭代位置；无元素时返回 None。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.iterator(index);
```

**源码**：[source/std/collections.mgl:55](../../source/std/collections.mgl#L55)。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-16"></a>

### Queue::len（17）

**声明**

```magel
int64 len();
```

**行为**

返回当前元素数量。

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

**源码**：[source/std/collections.mgl:59](../../source/std/collections.mgl#L59)。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-17"></a>

### Queue::isEmpty（18）

**声明**

```magel
bool isEmpty();
```

**行为**

判断 Any 是否为空盒子，不公开内部执行期标签。

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

**源码**：[source/std/collections.mgl:60](../../source/std/collections.mgl#L60)。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-18"></a>

### Queue::drop（19）

**声明**

```magel
void drop();
```

**行为**

释放对象拥有的资源，调用后遵守本模块失效规则。

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

**源码**：[source/std/collections.mgl:61](../../source/std/collections.mgl#L61)。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-19"></a>

### RingBuffer::withCapacity（20）

**声明**

```magel
static RingBuffer<T> withCapacity(int64 capacity);
```

**行为**

用指定 capacity 创建有界容器。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `capacity` | `int64 capacity` | 容器的最大元素数量 | 无 |

**返回值**

`RingBuffer<T>`。用指定 capacity 创建有界容器。

**错误与边界**

遵循本页“公共约定与错误边界”。
显式前置检查：`capacity > 0`。

**调用形式**

```magel
std::collections::RingBuffer<T>::withCapacity(capacity);
```

**源码**：[source/std/collections.mgl:68](../../source/std/collections.mgl#L68)。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-20"></a>

### RingBuffer::push（21）

**声明**

```magel
bool push(T value);
```

**行为**

追加 value；容量已满时返回 false，成功为 true。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `value` | `T value` | 输入值或写入的新值 | 无 |

**返回值**

`bool`。追加 value；容量已满时返回 false，成功为 true。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.push(value);
```

**源码**：[source/std/collections.mgl:77](../../source/std/collections.mgl#L77)。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-21"></a>

### RingBuffer::pop（22）

**声明**

```magel
Option<T> pop();
```

**行为**

移除并返回下一元素；空容器返回 None。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`Option<T>`。移除并返回下一元素；空容器返回 None。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.pop();
```

**源码**：[source/std/collections.mgl:82](../../source/std/collections.mgl#L82)。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-22"></a>

### RingBuffer::iterator（23）

**声明**

```magel
Option<T> iterator(int64 index);
```

**行为**

读取指定迭代位置；无元素时返回 None。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `index` | `int64 index` | 从 0 开始的位置 | 无 |

**返回值**

`Option<T>`。读取指定迭代位置；无元素时返回 None。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.iterator(index);
```

**源码**：[source/std/collections.mgl:88](../../source/std/collections.mgl#L88)。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-23"></a>

### RingBuffer::len（24）

**声明**

```magel
int64 len();
```

**行为**

返回当前元素数量。

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

**源码**：[source/std/collections.mgl:92](../../source/std/collections.mgl#L92)。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-24"></a>

### RingBuffer::available（25）

**声明**

```magel
int64 available();
```

**行为**

返回剩余可写容量。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`int64`。返回剩余可写容量。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.available();
```

**源码**：[source/std/collections.mgl:93](../../source/std/collections.mgl#L93)。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-25"></a>

### RingBuffer::drop（26）

**声明**

```magel
void drop();
```

**行为**

释放对象拥有的资源，调用后遵守本模块失效规则。

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

**源码**：[source/std/collections.mgl:94](../../source/std/collections.mgl#L94)。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-26"></a>

### FixedVector::withCapacity（27）

**声明**

```magel
static FixedVector<T> withCapacity(int64 capacity);
```

**行为**

用指定 capacity 创建有界容器。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `capacity` | `int64 capacity` | 容器的最大元素数量 | 无 |

**返回值**

`FixedVector<T>`。用指定 capacity 创建有界容器。

**错误与边界**

遵循本页“公共约定与错误边界”。
显式前置检查：`capacity >= 0`。

**调用形式**

```magel
std::collections::FixedVector<T>::withCapacity(capacity);
```

**源码**：[source/std/collections.mgl:101](../../source/std/collections.mgl#L101)。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-27"></a>

### FixedVector::push（28）

**声明**

```magel
bool push(T value);
```

**行为**

追加 value；容量已满时返回 false，成功为 true。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `value` | `T value` | 输入值或写入的新值 | 无 |

**返回值**

`bool`。追加 value；容量已满时返回 false，成功为 true。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.push(value);
```

**源码**：[source/std/collections.mgl:110](../../source/std/collections.mgl#L110)。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-28"></a>

### FixedVector::pop（29）

**声明**

```magel
Option<T> pop();
```

**行为**

移除并返回下一元素；空容器返回 None。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`Option<T>`。移除并返回下一元素；空容器返回 None。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.pop();
```

**源码**：[source/std/collections.mgl:115](../../source/std/collections.mgl#L115)。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-29"></a>

### FixedVector::index（30）

**声明**

```magel
T index(int64 key);
```

**行为**

读取 key 对应的元素，供下标语法调用。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `key` | `int64 key` | 查找键或下标 | 无 |

**返回值**

`T`。读取 key 对应的元素，供下标语法调用。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.index(key);
```

**源码**：[source/std/collections.mgl:119](../../source/std/collections.mgl#L119)。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-30"></a>

### FixedVector::iterator（31）

**声明**

```magel
Option<T> iterator(int64 index);
```

**行为**

读取指定迭代位置；无元素时返回 None。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `index` | `int64 index` | 从 0 开始的位置 | 无 |

**返回值**

`Option<T>`。读取指定迭代位置；无元素时返回 None。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.iterator(index);
```

**源码**：[source/std/collections.mgl:120](../../source/std/collections.mgl#L120)。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-31"></a>

### FixedVector::len（32）

**声明**

```magel
int64 len();
```

**行为**

返回当前元素数量。

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

**源码**：[source/std/collections.mgl:124](../../source/std/collections.mgl#L124)。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-32"></a>

### FixedVector::capacityValue（33）

**声明**

```magel
int64 capacityValue();
```

**行为**

返回固定容量。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`int64`。返回固定容量。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.capacityValue();
```

**源码**：[source/std/collections.mgl:125](../../source/std/collections.mgl#L125)。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-33"></a>

### FixedVector::drop（34）

**声明**

```magel
void drop();
```

**行为**

释放对象拥有的资源，调用后遵守本模块失效规则。

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

**源码**：[source/std/collections.mgl:126](../../source/std/collections.mgl#L126)。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-34"></a>

### Channel::create（35）

**声明**

```magel
static Channel<T> create();
```

**行为**

创建Channel，返回可供后续操作的值。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`Channel<T>`。创建Channel，返回可供后续操作的值。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::collections::Channel<T>::create();
```

**源码**：[source/std/collections.mgl:132](../../source/std/collections.mgl#L132)。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-35"></a>

### Channel::send（36）

**声明**

```magel
void send(T value);
```

**行为**

将 value 加入本地值队列。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `value` | `T value` | 输入值或写入的新值 | 无 |

**返回值**

无返回值。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.send(value);
```

**源码**：[source/std/collections.mgl:137](../../source/std/collections.mgl#L137)。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-36"></a>

### Channel::receive（37）

**声明**

```magel
Option<T> receive();
```

**行为**

取出队首值；空队列返回 None。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`Option<T>`。取出队首值；空队列返回 None。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.receive();
```

**源码**：[source/std/collections.mgl:138](../../source/std/collections.mgl#L138)。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-37"></a>

### Channel::iterator（38）

**声明**

```magel
Option<T> iterator(int64 index);
```

**行为**

读取指定迭代位置；无元素时返回 None。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `index` | `int64 index` | 从 0 开始的位置 | 无 |

**返回值**

`Option<T>`。读取指定迭代位置；无元素时返回 None。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.iterator(index);
```

**源码**：[source/std/collections.mgl:144](../../source/std/collections.mgl#L144)。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-38"></a>

### Channel::len（39）

**声明**

```magel
int64 len();
```

**行为**

返回当前元素数量。

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

**源码**：[source/std/collections.mgl:148](../../source/std/collections.mgl#L148)。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。

<a id="api-39"></a>

### Channel::drop（40）

**声明**

```magel
void drop();
```

**行为**

释放对象拥有的资源，调用后遵守本模块失效规则。

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

**源码**：[source/std/collections.mgl:149](../../source/std/collections.mgl#L149)。

**完整示例**：[example/samples/std_collections_algorithms.mgl](../../example/samples/std_collections_algorithms.mgl)。
