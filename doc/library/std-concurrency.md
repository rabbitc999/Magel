# std::concurrency — 线程通信与共享

[返回库目录](README.md) · [全部符号](symbols.md)

## 引入

```magel
import std::concurrency;
```

## 公共约定与错误边界

Channel<T> 是线程组内的有界多生产者多消费者通道，容量 1..1048576；队列按值复制，关闭后排空再返回 CLOSED。句柄可复制，显式 close 负责关闭，运行结束统一释放；最多 1024 个未排空通道。Receive<T> 只有 READY 时 value 有效。状态为 READY=0、TIMEOUT=1、CLOSED=2、CANCELLED=3、FULL=4、FAILED=5、INVALID=6。millis=-1 无限、0 非阻塞、正数限时；超时不取消任务。Shared<T> 单次 get/set/swap 原子；AtomicInt64.fetchAdd 原子返回旧值，按 int64 位宽回绕。需 --threads；普通全局变量继续隔离。

## 参考代码

完整程序：[example/samples/typed_messages_shared.mgl](../../example/samples/typed_messages_shared.mgl)。

```powershell
magel run example/samples/typed_messages_shared.mgl --threads
```

<details>
<summary>展开完整参考代码</summary>

```magel
import std::concurrency;


import std::message;
import std::thread;

struct Parcel { int64 id; String name; List<int64> readings; };

int64 isolatedCounter;

void typedWorker() {
    std::concurrency::Shared<int64> sharedCounter = std::message::receiveThread<std::concurrency::Shared<int64>>();
    Parcel received = std::message::receiveThread<Parcel>();
    int64 old = sharedCounter.swap(20);
    isolatedCounter = 99;
    std::message::sendThread<Parcel>(std::thread::parent(), received);
    std::message::sendThread<int64>(std::thread::parent(), old);
}

int64 main() {
    std::concurrency::Shared<int64> sharedCounter = std::concurrency::Shared<int64>::create(10);
    isolatedCounter = 3;
    Parcel original;
    original.id = 7;
    original.name = "before";
    List<int64> readings;
    readings.push(4);
    original.readings = readings;
    Thread task = std::thread::create(typedWorker);
    std::message::sendThread<std::concurrency::Shared<int64>>(task, sharedCounter);
    std::message::sendThread<Parcel>(task, original);
    original.id = 8;
    original.readings.push(6);
    Parcel reply = std::message::receiveThread<Parcel>();
    int64 previous = std::message::receiveThread<int64>();
    std::thread::wait(task);
    println("{} {} {} {} {} {}", reply.id, reply.name, reply.readings.len(),
    previous, sharedCounter.get(), isolatedCounter);
    return 0;
}
```

</details>

<details>
<summary>示例输出</summary>

```text
7 before 1 10 20 3
```

</details>

聚合入口的具体操作参见对应子模块；本页类型/方法的调用形式列在下方。

## 类型、枚举、常量与字段

公开字段按源码原样列出；句柄和游标字段用于包装器状态，通常通过方法操作。枚举的括号表示变体载荷。

| 所属类型 | 声明 | 说明 |
| --- | --- | --- |
| 模块 | <a id="symbol-39"></a>`struct Receive<T>` | 操作状态与载荷；仅在 READY 时读取 value。 |
| Receive | <a id="symbol-40"></a>`int64 status` | 统一操作状态码；载荷仅在 READY 时有效。 |
| Receive | <a id="symbol-41"></a>`T value` | 保存的单个值。 |
| 模块 | <a id="symbol-42"></a>`struct Shared<T>` | 每次 get/set/swap 原子；组合调用不构成原子事务。 |
| 模块 | <a id="symbol-43"></a>`struct AtomicInt64` | 共享 int64 原子槽，fetchAdd 返回旧值。 |
| AtomicInt64 | <a id="symbol-44"></a>`Shared<int64> slot` | 显式共享的 int64 原子槽。 |
| 模块 | <a id="symbol-45"></a>`struct Channel<T>` | 跨线程有界传值通道；可复制句柄，显式关闭。 |
| Channel | <a id="symbol-46"></a>`int64 handle` | 宿主资源句柄；应通过所属类型的方法操作。 |

## 函数索引

| 名称 | 声明 |
| --- | --- |
| [Receive::isReady](#api-0) | `bool isReady()` |
| [Shared::create](#api-1) | `static Shared<T> create(T value)` |
| [Shared::get](#api-2) | `T get()` |
| [Shared::set](#api-3) | `void set(T value)` |
| [Shared::swap](#api-4) | `T swap(T value)` |
| [Shared::id](#api-5) | `int64 id()` |
| [AtomicInt64::create](#api-6) | `static AtomicInt64 create(int64 value)` |
| [AtomicInt64::get](#api-7) | `int64 get()` |
| [AtomicInt64::set](#api-8) | `void set(int64 value)` |
| [AtomicInt64::fetchAdd](#api-9) | `int64 fetchAdd(int64 delta)` |
| [Channel::create](#api-10) | `static Channel<T> create(int64 capacity)` |
| [Channel::sendFor](#api-11) | `int64 sendFor(T value, int64 millis)` |
| [Channel::send](#api-12) | `int64 send(T value)` |
| [Channel::trySend](#api-13) | `int64 trySend(T value)` |
| [Channel::receiveFor](#api-14) | `Receive<T> receiveFor(int64 millis)` |
| [Channel::receive](#api-15) | `Receive<T> receive()` |
| [Channel::tryReceive](#api-16) | `Receive<T> tryReceive()` |
| [Channel::close](#api-17) | `void close()` |

<a id="api-0"></a>

### Receive::isReady（1）

**声明**

```magel
bool isReady();
```

**行为**

操作状态为 READY 时返回 true。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`bool`。操作状态为 READY 时返回 true。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.isReady();
```

**源码**：[source/std/concurrency.mgl:15](../../source/std/concurrency.mgl#L15)。

**完整示例**：[example/samples/typed_messages_shared.mgl](../../example/samples/typed_messages_shared.mgl)。

<a id="api-1"></a>

### Shared::create（2）

**声明**

```magel
static Shared<T> create(T value);
```

**行为**

创建值为 value 的原子共享槽。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `value` | `T value` | 输入值或写入的新值 | 无 |

**返回值**

`Shared<T>`。创建值为 value 的原子共享槽。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::concurrency::Shared<T>::create(value);
```

**源码**：[source/std/concurrency.mgl:41](../../source/std/concurrency.mgl#L41)。

**完整示例**：[example/samples/typed_messages_shared.mgl](../../example/samples/typed_messages_shared.mgl)。

<a id="api-2"></a>

### Shared::get（3）

**声明**

```magel
T get();
```

**行为**

读取共享槽当前值的副本。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`T`。读取共享槽当前值的副本。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.get();
```

**源码**：[source/std/concurrency.mgl:42](../../source/std/concurrency.mgl#L42)。

**完整示例**：[example/samples/typed_messages_shared.mgl](../../example/samples/typed_messages_shared.mgl)。

<a id="api-3"></a>

### Shared::set（4）

**声明**

```magel
void set(T value);
```

**行为**

将指定位置或共享槽替换为 value。

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
obj.set(value);
```

**源码**：[source/std/concurrency.mgl:43](../../source/std/concurrency.mgl#L43)。

**完整示例**：[example/samples/typed_messages_shared.mgl](../../example/samples/typed_messages_shared.mgl)。

<a id="api-4"></a>

### Shared::swap（5）

**声明**

```magel
T swap(T value);
```

**行为**

原子替换共享槽并返回旧值。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `value` | `T value` | 输入值或写入的新值 | 无 |

**返回值**

`T`。原子替换共享槽并返回旧值。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.swap(value);
```

**源码**：[source/std/concurrency.mgl:44](../../source/std/concurrency.mgl#L44)。

**完整示例**：[example/samples/typed_messages_shared.mgl](../../example/samples/typed_messages_shared.mgl)。

<a id="api-5"></a>

### Shared::id（6）

**声明**

```magel
int64 id();
```

**行为**

共享槽的引用标识，不拥有槽内值。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`int64`。共享槽的引用标识，不拥有槽内值。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.id();
```

**源码**：[source/std/concurrency.mgl:46](../../source/std/concurrency.mgl#L46)。

**完整示例**：[example/samples/typed_messages_shared.mgl](../../example/samples/typed_messages_shared.mgl)。

<a id="api-6"></a>

### AtomicInt64::create（7）

**声明**

```magel
static AtomicInt64 create(int64 value);
```

**行为**

创建 int64 原子槽。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `value` | `int64 value` | 输入值或写入的新值 | 无 |

**返回值**

`AtomicInt64`。创建 int64 原子槽。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::concurrency::AtomicInt64::create(value);
```

**源码**：[source/std/concurrency.mgl:52](../../source/std/concurrency.mgl#L52)。

**完整示例**：[example/samples/typed_messages_shared.mgl](../../example/samples/typed_messages_shared.mgl)。

<a id="api-7"></a>

### AtomicInt64::get（8）

**声明**

```magel
int64 get();
```

**行为**

读取共享槽当前值的副本。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`int64`。读取共享槽当前值的副本。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.get();
```

**源码**：[source/std/concurrency.mgl:53](../../source/std/concurrency.mgl#L53)。

**完整示例**：[example/samples/typed_messages_shared.mgl](../../example/samples/typed_messages_shared.mgl)。

<a id="api-8"></a>

### AtomicInt64::set（9）

**声明**

```magel
void set(int64 value);
```

**行为**

将指定位置或共享槽替换为 value。

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
obj.set(value);
```

**源码**：[source/std/concurrency.mgl:54](../../source/std/concurrency.mgl#L54)。

**完整示例**：[example/samples/typed_messages_shared.mgl](../../example/samples/typed_messages_shared.mgl)。

<a id="api-9"></a>

### AtomicInt64::fetchAdd（10）

**声明**

```magel
int64 fetchAdd(int64 delta);
```

**行为**

原子累加 int64 并返回旧值，溢出按 64 位回绕。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `delta` | `int64 delta` | 原子累加的 int64 增量 | 无 |

**返回值**

`int64`。原子累加 int64 并返回旧值，溢出按 64 位回绕。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.fetchAdd(delta);
```

**源码**：[source/std/concurrency.mgl:55](../../source/std/concurrency.mgl#L55)。

**完整示例**：[example/samples/typed_messages_shared.mgl](../../example/samples/typed_messages_shared.mgl)。

<a id="api-10"></a>

### Channel::create（11）

**声明**

```magel
static Channel<T> create(int64 capacity);
```

**行为**

创建指定容量的跨线程通道。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `capacity` | `int64 capacity` | 容器的最大元素数量 | 无 |

**返回值**

`Channel<T>`。创建指定容量的跨线程通道。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::concurrency::Channel<T>::create(capacity);
```

**源码**：[source/std/concurrency.mgl:61](../../source/std/concurrency.mgl#L61)。

**完整示例**：[example/samples/typed_messages_shared.mgl](../../example/samples/typed_messages_shared.mgl)。

<a id="api-11"></a>

### Channel::sendFor（12）

**声明**

```magel
int64 sendFor(T value, int64 millis);
```

**行为**

在截止时间内复制发送载荷，返回统一操作状态。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `value` | `T value` | 输入值或写入的新值 | 无 |
| `millis` | `int64 millis` | 等待毫秒数；-1 无限、0 非阻塞，waitFor 和 sleepFor 不接受负数 | 无 |

**返回值**

`int64`。在截止时间内复制发送载荷，返回统一操作状态。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.sendFor(value, millis);
```

**源码**：[source/std/concurrency.mgl:62](../../source/std/concurrency.mgl#L62)。

**完整示例**：[example/samples/typed_messages_shared.mgl](../../example/samples/typed_messages_shared.mgl)。

<a id="api-12"></a>

### Channel::send（13）

**声明**

```magel
int64 send(T value);
```

**行为**

按值复制并等待发送，队列满时施加背压。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `value` | `T value` | 输入值或写入的新值 | 无 |

**返回值**

`int64`。按值复制并等待发送，队列满时施加背压。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.send(value);
```

**源码**：[source/std/concurrency.mgl:63](../../source/std/concurrency.mgl#L63)。

**完整示例**：[example/samples/typed_messages_shared.mgl](../../example/samples/typed_messages_shared.mgl)。

<a id="api-13"></a>

### Channel::trySend（14）

**声明**

```magel
int64 trySend(T value);
```

**行为**

立即尝试复制发送，队列满返回 FULL。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `value` | `T value` | 输入值或写入的新值 | 无 |

**返回值**

`int64`。立即尝试复制发送，队列满返回 FULL。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.trySend(value);
```

**源码**：[source/std/concurrency.mgl:64](../../source/std/concurrency.mgl#L64)。

**完整示例**：[example/samples/typed_messages_shared.mgl](../../example/samples/typed_messages_shared.mgl)。

<a id="api-14"></a>

### Channel::receiveFor（15）

**声明**

```magel
Receive<T> receiveFor(int64 millis);
```

**行为**

显式限时接收，只有 READY 时 value 有效；-1 无限、0 非阻塞。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `millis` | `int64 millis` | 等待毫秒数；-1 无限、0 非阻塞，waitFor 和 sleepFor 不接受负数 | 无 |

**返回值**

`Receive<T>`。显式限时接收，只有 READY 时 value 有效；-1 无限、0 非阻塞。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.receiveFor(millis);
```

**源码**：[source/std/concurrency.mgl:65](../../source/std/concurrency.mgl#L65)。

**完整示例**：[example/samples/typed_messages_shared.mgl](../../example/samples/typed_messages_shared.mgl)。

<a id="api-15"></a>

### Channel::receive（16）

**声明**

```magel
Receive<T> receive();
```

**行为**

等待并按值接收下一项，关闭排空后返回 CLOSED。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`Receive<T>`。等待并按值接收下一项，关闭排空后返回 CLOSED。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.receive();
```

**源码**：[source/std/concurrency.mgl:72](../../source/std/concurrency.mgl#L72)。

**完整示例**：[example/samples/typed_messages_shared.mgl](../../example/samples/typed_messages_shared.mgl)。

<a id="api-16"></a>

### Channel::tryReceive（17）

**声明**

```magel
Receive<T> tryReceive();
```

**行为**

立即尝试接收，未就绪返回 TIMEOUT，只有 READY 时读取 value。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`Receive<T>`。立即尝试接收，未就绪返回 TIMEOUT，只有 READY 时读取 value。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.tryReceive();
```

**源码**：[source/std/concurrency.mgl:73](../../source/std/concurrency.mgl#L73)。

**完整示例**：[example/samples/typed_messages_shared.mgl](../../example/samples/typed_messages_shared.mgl)。

<a id="api-17"></a>

### Channel::close（18）

**声明**

```magel
void close();
```

**行为**

关闭队列并停止定时源；已排队的值先排空，再返回 CLOSED。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

无返回值。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.close();
```

**源码**：[source/std/concurrency.mgl:74](../../source/std/concurrency.mgl#L74)。

**完整示例**：[example/samples/typed_messages_shared.mgl](../../example/samples/typed_messages_shared.mgl)。

## 补充完整示例

[example/samples/thread_channel.mgl](../../example/samples/thread_channel.mgl)。

```magel
import std::thread;
import std::concurrency;
void worker(std::concurrency::Channel<int64> values) {
    values.send(42);
    values.close();
}
int64 main() {
    auto values = std::concurrency::Channel<int64>::create(2);
    Thread task = std::thread::createWith<std::concurrency::Channel<int64>>(worker, values);
    auto next = values.receive();
    assert(next.isReady() && next.value == 42);
    assert(values.receive().status == std::concurrency::CLOSED);
    auto completed = std::thread::waitResult(task);
    assert(completed.isReady());
    println("channel {}", next.value);
    return 0;
}
```
