# std::event — 显式事件与定时源

[返回库目录](README.md) · [全部符号](symbols.md)

## 引入

```chali
import std::event;
```

## 公共约定与错误边界

事件只在 receive/receiveFor/tryReceive 显式接收时交付值，绝不自动执行用户函数。队列容量 256，post 在队列满时返回 FULL。定时源使用宿主单调纳秒时钟、创建后立即计时，最多 1024 个活动定时源；服务计算截止时间，不按定时源创建线程。周期通知合并，missed 表示错过的周期数。Timer.stop/drop 删除未接收通知；队列关闭后先排空，结束后的宿主弱引用投递器返回 CLOSED。事件操作不支持同步胶水重入。

## 参考代码

完整程序：[example/samples/event_timer.chl](../../example/samples/event_timer.chl)。

```powershell
chali run example/samples/event_timer.chl
```

<details>
<summary>展开完整参考代码</summary>

```chali
import std::event;
void main() {
    assert(std::event::post(7, 19) == 0);
    let posted = std::event::tryReceive();
    assert(posted.isReady() && posted.value.tag == 7 && posted.value.source == 19);
    Timer timer = Timer::afterMillis(2);
    assert(timer.isValid());
    let tick = std::event::receiveFor(1000);
    assert(tick.isReady() && tick.value.timer == timer.id());
    timer.stop();
    assert(std::event::tryReceive().status == 1);
    println("events ok");

}
```

</details>

<details>
<summary>示例输出</summary>

```text
events ok
```

</details>

聚合入口的具体操作参见对应子模块；本页类型/方法的调用形式列在下方。

## 类型、枚举、常量与字段

公开字段按源码原样列出；句柄和游标字段用于包装器状态，通常通过方法操作。枚举的括号表示变体载荷。

| 所属类型 | 声明 | 说明 |
| --- | --- | --- |
| 模块 | <a id="symbol-60"></a>`struct Event` | 显式接收的定时或宿主事件值。 |
| Event | <a id="symbol-61"></a>`int64 timer` | 定时事件的宿主资源。 |
| Event | <a id="symbol-62"></a>`int64 tag` | 宿主投递的事件标签。 |
| Event | <a id="symbol-63"></a>`int64 source` | 被适配的源游标或连续存储。 |
| Event | <a id="symbol-64"></a>`int64 missed` | 周期通知合并时错过的周期数量。 |
| 模块 | <a id="symbol-65"></a>`struct Timer` | 不可复制的定时源所有者，事件中的 timer 仅为来源标识。 |

## 函数索引

| 名称 | 声明 |
| --- | --- |
| [receiveFor](#api-0) | `std::concurrency::Receive<Event> receiveFor(int64 millis)` |
| [receive](#api-1) | `std::concurrency::Receive<Event> receive()` |
| [tryReceive](#api-2) | `std::concurrency::Receive<Event> tryReceive()` |
| [post](#api-3) | `int64 post(int64 tag, int64 source)` |
| [close](#api-4) | `void close()` |
| [Timer::everyNanos](#api-5) | `static Timer everyNanos(int64 nanos)` |
| [Timer::afterNanos](#api-6) | `static Timer afterNanos(int64 nanos)` |
| [Timer::everyMillis](#api-7) | `static Timer everyMillis(int64 millis)` |
| [Timer::afterMillis](#api-8) | `static Timer afterMillis(int64 millis)` |
| [Timer::isValid](#api-9) | `bool isValid()` |
| [Timer::id](#api-10) | `int64 id()` |
| [Timer::stop](#api-11) | `void stop()` |

<a id="api-0"></a>

### receiveFor（1）

**声明**

```chali
std::concurrency::Receive<Event> receiveFor(int64 millis);
```

**行为**

显式限时接收，只有 READY 时 value 有效；-1 无限、0 非阻塞。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `millis` | `int64 millis` | 等待毫秒数；-1 无限、0 非阻塞，waitFor 和 sleepFor 不接受负数 | 无 |

**返回值**

`std::concurrency::Receive<Event>`。显式限时接收，只有 READY 时 value 有效；-1 无限、0 非阻塞。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
std::event::receiveFor(millis);
```

**源码**：[source/std/event.chl:20](../../source/std/event.chl#L20)。

**完整示例**：[example/samples/event_timer.chl](../../example/samples/event_timer.chl)。

<a id="api-1"></a>

### receive（2）

**声明**

```chali
std::concurrency::Receive<Event> receive();
```

**行为**

显式等待并消费事件，不执行隐式用户回调。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`std::concurrency::Receive<Event>`。显式等待并消费事件，不执行隐式用户回调。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
std::event::receive();
```

**源码**：[source/std/event.chl:31](../../source/std/event.chl#L31)。

**完整示例**：[example/samples/event_timer.chl](../../example/samples/event_timer.chl)。

<a id="api-2"></a>

### tryReceive（3）

**声明**

```chali
std::concurrency::Receive<Event> tryReceive();
```

**行为**

立即尝试接收，未就绪返回 TIMEOUT，只有 READY 时读取 value。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`std::concurrency::Receive<Event>`。立即尝试接收，未就绪返回 TIMEOUT，只有 READY 时读取 value。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
std::event::tryReceive();
```

**源码**：[source/std/event.chl:32](../../source/std/event.chl#L32)。

**完整示例**：[example/samples/event_timer.chl](../../example/samples/event_timer.chl)。

<a id="api-3"></a>

### post（4）

**声明**

```chali
int64 post(int64 tag, int64 source);
```

**行为**

向当前运行的有界事件队列投递 tag/source，满或关闭返回对应状态。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `tag` | `int64 tag` | 宿主事件的整数标签。 | 无 |
| `source` | `int64 source` | 输入游标、存储或事件来源值 | 无 |

**返回值**

`int64`。向当前运行的有界事件队列投递 tag/source，满或关闭返回对应状态。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
std::event::post(tag, source);
```

**源码**：[source/std/event.chl:33](../../source/std/event.chl#L33)。

**完整示例**：[example/samples/event_timer.chl](../../example/samples/event_timer.chl)。

<a id="api-4"></a>

### close（5）

**声明**

```chali
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

```chali
std::event::close();
```

**源码**：[source/std/event.chl:34](../../source/std/event.chl#L34)。

**完整示例**：[example/samples/event_timer.chl](../../example/samples/event_timer.chl)。

<a id="api-5"></a>

### Timer::everyNanos（6）

**声明**

```chali
static Timer everyNanos(int64 nanos);
```

**行为**

创建立即开始计时的周期纳秒定时源，错过周期合并。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `nanos` | `int64 nanos` | 正纳秒间隔 | 无 |

**返回值**

`Timer`。创建立即开始计时的周期纳秒定时源，错过周期合并。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
std::event::Timer::everyNanos(nanos);
```

**源码**：[source/std/event.chl:45](../../source/std/event.chl#L45)。

**完整示例**：[example/samples/event_timer.chl](../../example/samples/event_timer.chl)。

<a id="api-6"></a>

### Timer::afterNanos（7）

**声明**

```chali
static Timer afterNanos(int64 nanos);
```

**行为**

创建立即开始计时的一次性纳秒定时源。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `nanos` | `int64 nanos` | 正纳秒间隔 | 无 |

**返回值**

`Timer`。创建立即开始计时的一次性纳秒定时源。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
std::event::Timer::afterNanos(nanos);
```

**源码**：[source/std/event.chl:49](../../source/std/event.chl#L49)。

**完整示例**：[example/samples/event_timer.chl](../../example/samples/event_timer.chl)。

<a id="api-7"></a>

### Timer::everyMillis（8）

**声明**

```chali
static Timer everyMillis(int64 millis);
```

**行为**

创建立即开始计时的周期毫秒定时源，错过周期合并。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `millis` | `int64 millis` | 等待毫秒数；-1 无限、0 非阻塞，waitFor 和 sleepFor 不接受负数 | 无 |

**返回值**

`Timer`。创建立即开始计时的周期毫秒定时源，错过周期合并。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
std::event::Timer::everyMillis(millis);
```

**源码**：[source/std/event.chl:53](../../source/std/event.chl#L53)。

**完整示例**：[example/samples/event_timer.chl](../../example/samples/event_timer.chl)。

<a id="api-8"></a>

### Timer::afterMillis（9）

**声明**

```chali
static Timer afterMillis(int64 millis);
```

**行为**

创建立即开始计时的一次性毫秒定时源；无效间隔返回无效定时源。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `millis` | `int64 millis` | 等待毫秒数；-1 无限、0 非阻塞，waitFor 和 sleepFor 不接受负数 | 无 |

**返回值**

`Timer`。创建立即开始计时的一次性毫秒定时源；无效间隔返回无效定时源。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
std::event::Timer::afterMillis(millis);
```

**源码**：[source/std/event.chl:58](../../source/std/event.chl#L58)。

**完整示例**：[example/samples/event_timer.chl](../../example/samples/event_timer.chl)。

<a id="api-9"></a>

### Timer::isValid（10）

**声明**

```chali
bool isValid();
```

**行为**

定时源句柄非零时返回 true。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`bool`。定时源句柄非零时返回 true。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.isValid();
```

**源码**：[source/std/event.chl:63](../../source/std/event.chl#L63)。

**完整示例**：[example/samples/event_timer.chl](../../example/samples/event_timer.chl)。

<a id="api-10"></a>

### Timer::id（11）

**声明**

```chali
int64 id();
```

**行为**

只返回来源标识，不转移释放责任。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`int64`。只返回来源标识，不转移释放责任。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.id();
```

**源码**：[source/std/event.chl:65](../../source/std/event.chl#L65)。

**完整示例**：[example/samples/event_timer.chl](../../example/samples/event_timer.chl)。

<a id="api-11"></a>

### Timer::stop（12）

**声明**

```chali
void stop();
```

**行为**

删除定时源及其未接收通知，重复停止安全。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

无返回值。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.stop();
```

**源码**：[source/std/event.chl:66](../../source/std/event.chl#L66)。

**完整示例**：[example/samples/event_timer.chl](../../example/samples/event_timer.chl)。
