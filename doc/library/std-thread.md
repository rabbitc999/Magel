# std::thread — 隔离线程

[返回库目录](README.md) · [全部符号](symbols.md)

## 引入

```chali
import std::thread;
```

## 公共约定与错误边界

公开线程类型为 Thread，模块 std::thread；运行时需 --threads。create 使用无参静态入口，createWith 在创建时复制初始参数。wait/waitFor 传播普通失败原始诊断；waitResult/For 收集状态、error 与 trace，普通失败可被调用方处理；panic 始终终止本次 VM 运行及全部隔离线程。成功收集只允许一次，超时不消费句柄。requestStop 在 VM 安全点及可取消等待处终止任务，释放 VM 持有的宿主记录和堆；不在取消途中调用用户 Drop 代码，业务收尾应在请求取消前通过消息完成。阻塞宿主调用须由宿主自行支持超时/取消，VM 无法抢占正在执行的 C++ 函数。主程序结束请求停止并回收全部子任务。ThreadGroup 等待时监测失败并取消兄弟任务，离开作用域取消并回收。ThreadPool 使用固定线程和有界静态任务队列，closeAndWait 排空后回收，cancelAll 取消工作线程并丢弃未开始任务；长期阻塞 I/O 使用专用线程。ThreadGroup、ThreadPool 和 Timer 不克隆底层任务或定时源，使用引用或 move 维持单一收尾责任，避免复制拥有者。

## 参考代码

完整程序：[example/samples/thread_messages.chl](../../example/samples/thread_messages.chl)。

```powershell
chali run example/samples/thread_messages.chl --threads
```

<details>
<summary>展开完整参考代码</summary>

```chali

import std::thread;

int64 isolated;

void worker() {
    String message = std::thread::receive();
    isolated = 77;
    String reply = "ack:" + message;
    std::thread::send(std::thread::parent(), reply);
}

void main() {
    isolated = 5;
    Thread task = std::thread::create(worker);
    std::thread::send(task, "hello");
    String reply = std::thread::receive();
    std::thread::wait(task);
    println("{} {}", reply, isolated);

}
```

</details>

<details>
<summary>示例输出</summary>

```text
ack:hello 5
```

</details>

聚合入口的具体操作参见对应子模块；本页类型/方法的调用形式列在下方。

## 类型、枚举、常量与字段

公开字段按源码原样列出；句柄和游标字段用于包装器状态，通常通过方法操作。枚举的括号表示变体载荷。

| 所属类型 | 声明 | 说明 |
| --- | --- | --- |
| 模块 | <a id="symbol-443"></a>`struct ThreadResult` | 任务完成状态、原始错误与调用栈。 |
| ThreadResult | <a id="symbol-444"></a>`int64 status` | 线程等待状态，区分完成、超时、取消与失败。 |
| ThreadResult | <a id="symbol-445"></a>`String error` | 最近操作的错误说明。 |
| ThreadResult | <a id="symbol-446"></a>`String trace` | 失败任务保留的原始源码调用栈文本。 |
| 模块 | <a id="symbol-447"></a>`struct ThreadGroup : Drop` | 拥有任务回收责任的线程组。 |
| ThreadGroup | <a id="symbol-448"></a>`List<Thread> tasks` | 线程组登记并负责回收的 Thread 列表。 |
| ThreadGroup | <a id="symbol-449"></a>`bool completed` | 全部任务是否已经回收。 |
| ThreadGroup | <a id="symbol-450"></a>`bool successful` | 已完成线程组是否全部成功。 |
| ThreadGroup | <a id="symbol-451"></a>`String error` | 最近操作的错误说明。 |
| 模块 | <a id="symbol-452"></a>`struct ThreadPool : Drop` | 固定工作线程和有界静态任务队列。 |
| ThreadPool | <a id="symbol-453"></a>`std::concurrency::Channel<void fn()> jobs` | 跨线程的有界静态任务队列。 |
| ThreadPool | <a id="symbol-454"></a>`ThreadGroup group` | 统一负责工作线程生命周期的线程组。 |
| ThreadPool | <a id="symbol-455"></a>`bool closed` | 是否已停止接受任务或事件。 |

## 函数索引

| 名称 | 声明 |
| --- | --- |
| [create](#api-0) | `Thread create(void fn() task)` |
| [createWith](#api-1) | `Thread createWith<T>(void fn(T) task, T value)` |
| [wait](#api-2) | `void wait(Thread thread)` |
| [waitFor](#api-3) | `bool waitFor(Thread thread, int64 millis)` |
| [tryWait](#api-4) | `bool tryWait(Thread thread)` |
| [requestStop](#api-5) | `void requestStop(Thread thread)` |
| [stopRequested](#api-6) | `bool stopRequested()` |
| [state](#api-7) | `int64 state(Thread thread)` |
| [sleepFor](#api-8) | `int64 sleepFor(int64 millis)` |
| [ThreadResult::isReady](#api-9) | `bool isReady()` |
| [waitResultFor](#api-10) | `ThreadResult waitResultFor(Thread thread, int64 millis)` |
| [waitResult](#api-11) | `ThreadResult waitResult(Thread thread)` |
| [send](#api-12) | `void send(Thread thread, String message)` |
| [receive](#api-13) | `String receive()` |
| [current](#api-14) | `Thread current()` |
| [parent](#api-15) | `Thread parent()` |
| [receiveFor](#api-16) | `std::concurrency::Receive<String> receiveFor(int64 millis)` |
| [tryReceive](#api-17) | `std::concurrency::Receive<String> tryReceive()` |
| [closeInbox](#api-18) | `void closeInbox()` |
| [ThreadGroup::create](#api-19) | `static ThreadGroup create()` |
| [ThreadGroup::start](#api-20) | `Thread start(void fn() task)` |
| [ThreadGroup::add](#api-21) | `void add(Thread task)` |
| [ThreadGroup::cancelAll](#api-22) | `void cancelAll()` |
| [ThreadGroup::waitAll](#api-23) | `bool waitAll()` |
| [ThreadGroup::drop](#api-24) | `void drop()` |
| [ThreadPool::create](#api-25) | `static ThreadPool create(int64 workers, int64 capacity)` |
| [ThreadPool::submitFor](#api-26) | `int64 submitFor(void fn() task, int64 millis)` |
| [ThreadPool::submit](#api-27) | `int64 submit(void fn() task)` |
| [ThreadPool::closeAndWait](#api-28) | `bool closeAndWait()` |
| [ThreadPool::cancelAll](#api-29) | `void cancelAll()` |
| [ThreadPool::drop](#api-30) | `void drop()` |

<a id="api-0"></a>

### create（1）

**声明**

```chali
Thread create(void fn() task);
```

**行为**

创建隔离线程，返回可供后续操作的值。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `task` | `void fn() task` | 静态任务函数，参数由签名决定 | 无 |

**返回值**

`Thread`。创建隔离线程，返回可供后续操作的值。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
std::thread::create(task);
```

**源码**：[source/std/thread.chl:11](../../source/std/thread.chl#L11)。

**完整示例**：[example/samples/thread_messages.chl](../../example/samples/thread_messages.chl)。

<a id="api-1"></a>

### createWith（2）

**声明**

```chali
Thread createWith<T>(void fn(T) task, T value);
```

**行为**

创建隔离线程并在创建时复制初始参数，拒绝借用闭包和不可传递载荷。

静态入口和初始参数在创建时复制；引用、资源和借用闭包不可传递。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `task` | `void fn(T) task` | 静态任务函数，参数由签名决定 | 无 |
| `value` | `T value` | 输入值或写入的新值 | 无 |

**返回值**

`Thread`。创建隔离线程并在创建时复制初始参数，拒绝借用闭包和不可传递载荷。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
std::thread::createWith<T>(task, value);
```

**源码**：[source/std/thread.chl:13](../../source/std/thread.chl#L13)。

**完整示例**：[example/samples/thread_messages.chl](../../example/samples/thread_messages.chl)。

<a id="api-2"></a>

### wait（3）

**声明**

```chali
void wait(Thread thread);
```

**行为**

等待并回收目标线程，失败时保留原始诊断。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `thread` | `Thread thread` | 线程句柄或归属线程 | 无 |

**返回值**

无返回值。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
std::thread::wait(thread);
```

**源码**：[source/std/thread.chl:14](../../source/std/thread.chl#L14)。

**完整示例**：[example/samples/thread_messages.chl](../../example/samples/thread_messages.chl)。

<a id="api-3"></a>

### waitFor（4）

**声明**

```chali
bool waitFor(Thread thread, int64 millis);
```

**行为**

最多等待 millis 毫秒，完成并回收目标返回 true，超时返回 false；超时不取消目标，目标失败传播原始诊断。

非负毫秒；超时不会取消线程或消费句柄。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `thread` | `Thread thread` | 线程句柄或归属线程 | 无 |
| `millis` | `int64 millis` | 等待毫秒数；-1 无限、0 非阻塞，waitFor 和 sleepFor 不接受负数 | 无 |

**返回值**

`bool`。最多等待 millis 毫秒，完成并回收目标返回 true，超时返回 false；超时不取消目标，目标失败传播原始诊断。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
std::thread::waitFor(thread, millis);
```

**源码**：[source/std/thread.chl:16](../../source/std/thread.chl#L16)。

**完整示例**：[example/samples/thread_messages.chl](../../example/samples/thread_messages.chl)。

<a id="api-4"></a>

### tryWait（5）

**声明**

```chali
bool tryWait(Thread thread);
```

**行为**

不阻塞地尝试等待并回收目标，尚未完成返回 false；目标失败传播原始诊断。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `thread` | `Thread thread` | 线程句柄或归属线程 | 无 |

**返回值**

`bool`。不阻塞地尝试等待并回收目标，尚未完成返回 false；目标失败传播原始诊断。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
std::thread::tryWait(thread);
```

**源码**：[source/std/thread.chl:17](../../source/std/thread.chl#L17)。

**完整示例**：[example/samples/thread_messages.chl](../../example/samples/thread_messages.chl)。

<a id="api-5"></a>

### requestStop（6）

**声明**

```chali
void requestStop(Thread thread);
```

**行为**

请求指定任务在 VM 执行或等待安全点停止，不抢占阻塞宿主函数。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `thread` | `Thread thread` | 线程句柄或归属线程 | 无 |

**返回值**

无返回值。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
std::thread::requestStop(thread);
```

**源码**：[source/std/thread.chl:18](../../source/std/thread.chl#L18)。

**完整示例**：[example/samples/thread_messages.chl](../../example/samples/thread_messages.chl)。

<a id="api-6"></a>

### stopRequested（7）

**声明**

```chali
bool stopRequested();
```

**行为**

查询当前任务是否已收到协作停止请求。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`bool`。查询当前任务是否已收到协作停止请求。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
std::thread::stopRequested();
```

**源码**：[source/std/thread.chl:19](../../source/std/thread.chl#L19)。

**完整示例**：[example/samples/thread_messages.chl](../../example/samples/thread_messages.chl)。

<a id="api-7"></a>

### state（8）

**声明**

```chali
int64 state(Thread thread);
```

**行为**

查询任务状态：0 运行、1 成功、2 失败、3 已取消；不消费句柄。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `thread` | `Thread thread` | 线程句柄或归属线程 | 无 |

**返回值**

`int64`。查询任务状态：0 运行、1 成功、2 失败、3 已取消；不消费句柄。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
std::thread::state(thread);
```

**源码**：[source/std/thread.chl:20](../../source/std/thread.chl#L20)。

**完整示例**：[example/samples/thread_messages.chl](../../example/samples/thread_messages.chl)。

<a id="api-8"></a>

### sleepFor（9）

**声明**

```chali
int64 sleepFor(int64 millis);
```

**行为**

可取消地睡眠非负毫秒数，成功返回 READY。

可取消睡眠，负毫秒报错，成功返回 READY。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `millis` | `int64 millis` | 等待毫秒数；-1 无限、0 非阻塞，waitFor 和 sleepFor 不接受负数 | 无 |

**返回值**

`int64`。可取消地睡眠非负毫秒数，成功返回 READY。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
std::thread::sleepFor(millis);
```

**源码**：[source/std/thread.chl:22](../../source/std/thread.chl#L22)。

**完整示例**：[example/samples/thread_messages.chl](../../example/samples/thread_messages.chl)。

<a id="api-9"></a>

### ThreadResult::isReady（10）

**声明**

```chali
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

```chali
obj.isReady();
```

**源码**：[source/std/thread.chl:30](../../source/std/thread.chl#L30)。

**完整示例**：[example/samples/thread_messages.chl](../../example/samples/thread_messages.chl)。

<a id="api-10"></a>

### waitResultFor（11）

**声明**

```chali
ThreadResult waitResultFor(Thread thread, int64 millis);
```

**行为**

限时收集完成结果；超时不取消目标或消费句柄，-1 表示无限。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `thread` | `Thread thread` | 线程句柄或归属线程 | 无 |
| `millis` | `int64 millis` | 等待毫秒数；-1 无限、0 非阻塞，waitFor 和 sleepFor 不接受负数 | 无 |

**返回值**

`ThreadResult`。限时收集完成结果；超时不取消目标或消费句柄，-1 表示无限。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
std::thread::waitResultFor(thread, millis);
```

**源码**：[source/std/thread.chl:32](../../source/std/thread.chl#L32)。

**完整示例**：[example/samples/thread_messages.chl](../../example/samples/thread_messages.chl)。

<a id="api-11"></a>

### waitResult（12）

**声明**

```chali
ThreadResult waitResult(Thread thread);
```

**行为**

收集完成结果和原始 error/trace，普通失败返回 FAILED，panic 继续传播。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `thread` | `Thread thread` | 线程句柄或归属线程 | 无 |

**返回值**

`ThreadResult`。收集完成结果和原始 error/trace，普通失败返回 FAILED，panic 继续传播。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
std::thread::waitResult(thread);
```

**源码**：[source/std/thread.chl:41](../../source/std/thread.chl#L41)。

**完整示例**：[example/samples/thread_messages.chl](../../example/samples/thread_messages.chl)。

<a id="api-12"></a>

### send（13）

**声明**

```chali
void send(Thread thread, String message);
```

**行为**

向目标任务发送字符串消息。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `thread` | `Thread thread` | 线程句柄或归属线程 | 无 |
| `message` | `String message` | 诊断文本或消息载荷 | 无 |

**返回值**

无返回值。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
std::thread::send(thread, message);
```

**源码**：[source/std/thread.chl:42](../../source/std/thread.chl#L42)。

**完整示例**：[example/samples/thread_messages.chl](../../example/samples/thread_messages.chl)。

<a id="api-13"></a>

### receive（14）

**声明**

```chali
String receive();
```

**行为**

等待并接收当前任务队列的一条字符串消息。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`String`。等待并接收当前任务队列的一条字符串消息。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
std::thread::receive();
```

**源码**：[source/std/thread.chl:43](../../source/std/thread.chl#L43)。

**完整示例**：[example/samples/thread_messages.chl](../../example/samples/thread_messages.chl)。

<a id="api-14"></a>

### current（15）

**声明**

```chali
Thread current();
```

**行为**

返回当前线程的 Thread 句柄。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`Thread`。返回当前线程的 Thread 句柄。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
std::thread::current();
```

**源码**：[source/std/thread.chl:44](../../source/std/thread.chl#L44)。

**完整示例**：[example/samples/thread_messages.chl](../../example/samples/thread_messages.chl)。

<a id="api-15"></a>

### parent（16）

**声明**

```chali
Thread parent();
```

**行为**

返回创建当前任务的父线程 Thread 句柄。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`Thread`。返回创建当前任务的父线程 Thread 句柄。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
std::thread::parent();
```

**源码**：[source/std/thread.chl:45](../../source/std/thread.chl#L45)。

**完整示例**：[example/samples/thread_messages.chl](../../example/samples/thread_messages.chl)。

<a id="api-16"></a>

### receiveFor（17）

**声明**

```chali
std::concurrency::Receive<String> receiveFor(int64 millis);
```

**行为**

显式限时接收，只有 READY 时 value 有效；-1 无限、0 非阻塞。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `millis` | `int64 millis` | 等待毫秒数；-1 无限、0 非阻塞，waitFor 和 sleepFor 不接受负数 | 无 |

**返回值**

`std::concurrency::Receive<String>`。显式限时接收，只有 READY 时 value 有效；-1 无限、0 非阻塞。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
std::thread::receiveFor(millis);
```

**源码**：[source/std/thread.chl:46](../../source/std/thread.chl#L46)。

**完整示例**：[example/samples/thread_messages.chl](../../example/samples/thread_messages.chl)。

<a id="api-17"></a>

### tryReceive（18）

**声明**

```chali
std::concurrency::Receive<String> tryReceive();
```

**行为**

立即尝试接收，未就绪返回 TIMEOUT，只有 READY 时读取 value。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`std::concurrency::Receive<String>`。立即尝试接收，未就绪返回 TIMEOUT，只有 READY 时读取 value。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
std::thread::tryReceive();
```

**源码**：[source/std/thread.chl:52](../../source/std/thread.chl#L52)。

**完整示例**：[example/samples/thread_messages.chl](../../example/samples/thread_messages.chl)。

<a id="api-18"></a>

### closeInbox（19）

**声明**

```chali
void closeInbox();
```

**行为**

关闭当前任务邮箱并唤醒等待者；已排队消息仍可排空。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

无返回值。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
std::thread::closeInbox();
```

**源码**：[source/std/thread.chl:53](../../source/std/thread.chl#L53)。

**完整示例**：[example/samples/thread_messages.chl](../../example/samples/thread_messages.chl)。

<a id="api-19"></a>

### ThreadGroup::create（20）

**声明**

```chali
static ThreadGroup create();
```

**行为**

创建负责回收子任务的线程组。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`ThreadGroup`。创建负责回收子任务的线程组。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
std::thread::ThreadGroup::create();
```

**源码**：[source/std/thread.chl:61](../../source/std/thread.chl#L61)。

**完整示例**：[example/samples/thread_messages.chl](../../example/samples/thread_messages.chl)。

<a id="api-20"></a>

### ThreadGroup::start（21）

**声明**

```chali
Thread start(void fn() task);
```

**行为**

在当前线程组中创建任务并登记统一回收责任。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `task` | `void fn() task` | 静态任务函数，参数由签名决定 | 无 |

**返回值**

`Thread`。在当前线程组中创建任务并登记统一回收责任。

**错误与边界**

遵循本页“公共约定与错误边界”。
实现包含 panic 分支；调用前应处理空值或失败状态。

**调用形式**

```chali
obj.start(task);
```

**源码**：[source/std/thread.chl:62](../../source/std/thread.chl#L62)。

**完整示例**：[example/samples/thread_messages.chl](../../example/samples/thread_messages.chl)。

<a id="api-21"></a>

### ThreadGroup::add（22）

**声明**

```chali
void add(Thread task);
```

**行为**

登记已创建线程的统一等待责任，结束后的线程组拒绝新任务。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `task` | `Thread task` | 静态任务函数，参数由签名决定 | 无 |

**返回值**

无返回值。

**错误与边界**

遵循本页“公共约定与错误边界”。
实现包含 panic 分支；调用前应处理空值或失败状态。

**调用形式**

```chali
obj.add(task);
```

**源码**：[source/std/thread.chl:68](../../source/std/thread.chl#L68)。

**完整示例**：[example/samples/thread_messages.chl](../../example/samples/thread_messages.chl)。

<a id="api-22"></a>

### ThreadGroup::cancelAll（23）

**声明**

```chali
void cancelAll();
```

**行为**

请求全部工作任务停止；线程池同时关闭并丢弃未开始的任务。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

无返回值。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.cancelAll();
```

**源码**：[source/std/thread.chl:77](../../source/std/thread.chl#L77)。

**完整示例**：[example/samples/thread_messages.chl](../../example/samples/thread_messages.chl)。

<a id="api-23"></a>

### ThreadGroup::waitAll（24）

**声明**

```chali
bool waitAll();
```

**行为**

监测全部任务，发现普通失败就取消兄弟任务，全部回收后返回是否均成功。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`bool`。监测全部任务，发现普通失败就取消兄弟任务，全部回收后返回是否均成功。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.waitAll();
```

**源码**：[source/std/thread.chl:80](../../source/std/thread.chl#L80)。

**完整示例**：[example/samples/thread_messages.chl](../../example/samples/thread_messages.chl)。

<a id="api-24"></a>

### ThreadGroup::drop（25）

**声明**

```chali
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

```chali
obj.drop();
```

**源码**：[source/std/thread.chl:102](../../source/std/thread.chl#L102)。

**完整示例**：[example/samples/thread_messages.chl](../../example/samples/thread_messages.chl)。

<a id="api-25"></a>

### ThreadPool::create（26）

**声明**

```chali
static ThreadPool create(int64 workers, int64 capacity);
```

**行为**

创建固定数量工作线程和有界任务队列。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `workers` | `int64 workers` | 固定工作线程数量，1..1024 | 无 |
| `capacity` | `int64 capacity` | 容器的最大元素数量 | 无 |

**返回值**

`ThreadPool`。创建固定数量工作线程和有界任务队列。

**错误与边界**

遵循本页“公共约定与错误边界”。
实现包含 panic 分支；调用前应处理空值或失败状态。

**调用形式**

```chali
std::thread::ThreadPool::create(workers, capacity);
```

**源码**：[source/std/thread.chl:121](../../source/std/thread.chl#L121)。

**完整示例**：[example/samples/thread_messages.chl](../../example/samples/thread_messages.chl)。

<a id="api-26"></a>

### ThreadPool::submitFor（27）

**声明**

```chali
int64 submitFor(void fn() task, int64 millis);
```

**行为**

限时提交静态任务，超时不取消已接收任务；拒绝已关闭或失败的线程池。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `task` | `void fn() task` | 静态任务函数，参数由签名决定 | 无 |
| `millis` | `int64 millis` | 等待毫秒数；-1 无限、0 非阻塞，waitFor 和 sleepFor 不接受负数 | 无 |

**返回值**

`int64`。限时提交静态任务，超时不取消已接收任务；拒绝已关闭或失败的线程池。

**错误与边界**

遵循本页“公共约定与错误边界”。
实现包含 panic 分支；调用前应处理空值或失败状态。

**调用形式**

```chali
obj.submitFor(task, millis);
```

**源码**：[source/std/thread.chl:126](../../source/std/thread.chl#L126)。

**完整示例**：[example/samples/thread_messages.chl](../../example/samples/thread_messages.chl)。

<a id="api-27"></a>

### ThreadPool::submit（28）

**声明**

```chali
int64 submit(void fn() task);
```

**行为**

向线程池提交静态任务；有界队列满时等待并监测工作线程失败。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `task` | `void fn() task` | 静态任务函数，参数由签名决定 | 无 |

**返回值**

`int64`。向线程池提交静态任务；有界队列满时等待并监测工作线程失败。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.submit(task);
```

**源码**：[source/std/thread.chl:143](../../source/std/thread.chl#L143)。

**完整示例**：[example/samples/thread_messages.chl](../../example/samples/thread_messages.chl)。

<a id="api-28"></a>

### ThreadPool::closeAndWait（29）

**声明**

```chali
bool closeAndWait();
```

**行为**

关闭任务队列，排空已提交任务并回收固定工作线程。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`bool`。关闭任务队列，排空已提交任务并回收固定工作线程。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.closeAndWait();
```

**源码**：[source/std/thread.chl:144](../../source/std/thread.chl#L144)。

**完整示例**：[example/samples/thread_messages.chl](../../example/samples/thread_messages.chl)。

<a id="api-29"></a>

### ThreadPool::cancelAll（30）

**声明**

```chali
void cancelAll();
```

**行为**

请求全部工作任务停止；线程池同时关闭并丢弃未开始的任务。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

无返回值。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.cancelAll();
```

**源码**：[source/std/thread.chl:148](../../source/std/thread.chl#L148)。

**完整示例**：[example/samples/thread_messages.chl](../../example/samples/thread_messages.chl)。

<a id="api-30"></a>

### ThreadPool::drop（31）

**声明**

```chali
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

```chali
obj.drop();
```

**源码**：[source/std/thread.chl:154](../../source/std/thread.chl#L154)。

**完整示例**：[example/samples/thread_messages.chl](../../example/samples/thread_messages.chl)。

## 补充完整示例

[example/samples/thread_pool.chl](../../example/samples/thread_pool.chl)。

```chali
import std::thread;
void main() {
    ThreadPool pool = ThreadPool::create(2, 4);
    assert(pool.closeAndWait());
    println("pool ok");

}
```
