# std::message — 线程邮箱

[返回库目录](README.md) · [全部符号](symbols.md)

## 引入

```chali
import std::message;
```

## 公共约定与错误边界

泛型线程消息按值复制，接收类型必须匹配；静态函数可传递，引用、独占资源、嵌套函数值和带捕获闭包不可传递。邮箱容量 256，sendThread 在满时等待；sendFor/trySend 和 receiveFor/tryReceive 返回统一操作状态。receiveThread 沿用阻塞和原始诊断；Receive<T> 仅在 READY 时读取 value。需 --threads。

## 参考代码

完整程序：[example/samples/typed_messages_shared.chl](../../example/samples/typed_messages_shared.chl)。

```powershell
chali run example/samples/typed_messages_shared.chl --threads
```

<details>
<summary>展开完整参考代码</summary>

```chali
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

void main() {
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

## 函数索引

| 名称 | 声明 |
| --- | --- |
| [sendThread](#api-0) | `void sendThread<T>(Thread thread, T value)` |
| [receiveThread](#api-1) | `T receiveThread<T>()` |
| [sendFor](#api-2) | `int64 sendFor<T>(Thread thread, T value, int64 millis)` |
| [trySend](#api-3) | `int64 trySend<T>(Thread thread, T value)` |
| [receiveFor](#api-4) | `std::concurrency::Receive<T> receiveFor<T>(int64 millis)` |
| [tryReceive](#api-5) | `std::concurrency::Receive<T> tryReceive<T>()` |

<a id="api-0"></a>

### sendThread（1）

**声明**

```chali
void sendThread<T>(Thread thread, T value);
```

**行为**

向指定线程投递 T 类型的值副本。

邮箱传值复制，静态函数可传递，借用闭包和宿主独占资源不可传递。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `thread` | `Thread thread` | 线程句柄或归属线程 | 无 |
| `value` | `T value` | 输入值或写入的新值 | 无 |

**返回值**

无返回值。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
std::message::sendThread<T>(thread, value);
```

**源码**：[source/std/message.chl:6](../../source/std/message.chl#L6)。

**完整示例**：[example/samples/typed_messages_shared.chl](../../example/samples/typed_messages_shared.chl)。

<a id="api-1"></a>

### receiveThread（2）

**声明**

```chali
T receiveThread<T>();
```

**行为**

等待当前线程的一条 T 类型消息。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`T`。等待当前线程的一条 T 类型消息。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
std::message::receiveThread<T>();
```

**源码**：[source/std/message.chl:7](../../source/std/message.chl#L7)。

**完整示例**：[example/samples/typed_messages_shared.chl](../../example/samples/typed_messages_shared.chl)。

<a id="api-2"></a>

### sendFor（3）

**声明**

```chali
int64 sendFor<T>(Thread thread, T value, int64 millis);
```

**行为**

在截止时间内复制发送载荷，返回统一操作状态。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `thread` | `Thread thread` | 线程句柄或归属线程 | 无 |
| `value` | `T value` | 输入值或写入的新值 | 无 |
| `millis` | `int64 millis` | 等待毫秒数；-1 无限、0 非阻塞，waitFor 和 sleepFor 不接受负数 | 无 |

**返回值**

`int64`。在截止时间内复制发送载荷，返回统一操作状态。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
std::message::sendFor<T>(thread, value, millis);
```

**源码**：[source/std/message.chl:9](../../source/std/message.chl#L9)。

**完整示例**：[example/samples/typed_messages_shared.chl](../../example/samples/typed_messages_shared.chl)。

<a id="api-3"></a>

### trySend（4）

**声明**

```chali
int64 trySend<T>(Thread thread, T value);
```

**行为**

立即尝试复制发送，队列满返回 FULL。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `thread` | `Thread thread` | 线程句柄或归属线程 | 无 |
| `value` | `T value` | 输入值或写入的新值 | 无 |

**返回值**

`int64`。立即尝试复制发送，队列满返回 FULL。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
std::message::trySend<T>(thread, value);
```

**源码**：[source/std/message.chl:12](../../source/std/message.chl#L12)。

**完整示例**：[example/samples/typed_messages_shared.chl](../../example/samples/typed_messages_shared.chl)。

<a id="api-4"></a>

### receiveFor（5）

**声明**

```chali
std::concurrency::Receive<T> receiveFor<T>(int64 millis);
```

**行为**

显式限时接收，只有 READY 时 value 有效；-1 无限、0 非阻塞。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `millis` | `int64 millis` | 等待毫秒数；-1 无限、0 非阻塞，waitFor 和 sleepFor 不接受负数 | 无 |

**返回值**

`std::concurrency::Receive<T>`。显式限时接收，只有 READY 时 value 有效；-1 无限、0 非阻塞。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
std::message::receiveFor<T>(millis);
```

**源码**：[source/std/message.chl:13](../../source/std/message.chl#L13)。

**完整示例**：[example/samples/typed_messages_shared.chl](../../example/samples/typed_messages_shared.chl)。

<a id="api-5"></a>

### tryReceive（6）

**声明**

```chali
std::concurrency::Receive<T> tryReceive<T>();
```

**行为**

立即尝试接收，未就绪返回 TIMEOUT，只有 READY 时读取 value。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`std::concurrency::Receive<T>`。立即尝试接收，未就绪返回 TIMEOUT，只有 READY 时读取 value。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
std::message::tryReceive<T>();
```

**源码**：[source/std/message.chl:19](../../source/std/message.chl#L19)。

**完整示例**：[example/samples/typed_messages_shared.chl](../../example/samples/typed_messages_shared.chl)。
