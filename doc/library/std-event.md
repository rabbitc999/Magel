# std::event — 显式事件与定时源

[返回库目录](README.md) · [全部符号](symbols.md)

## 引入

```magel
import std::event;
```

## 公共约定与错误边界

事件只在 receive/receiveFor/tryReceive 显式接收时交付值，绝不自动执行用户函数。队列容量 256，post 在队列满时返回 FULL。定时源使用宿主单调纳秒时钟、创建后立即计时，最多 1024 个活动定时源；服务计算截止时间，不按定时源创建线程。周期通知合并，missed 表示错过的周期数。Timer.stop/drop 删除未接收通知；队列关闭后先排空，结束后的宿主弱引用投递器返回 CLOSED。事件操作不支持同步胶水重入。

## 参考代码

完整程序：[example/samples/event_timer.mgl](../../example/samples/event_timer.mgl)。

```powershell
magel run example/samples/event_timer.mgl
```

<details>
<summary>展开完整参考代码</summary>

```magel
import std::event;
int64 main() {
    assert(std::event::post(7, 19) == 0);
    auto posted = std::event::tryReceive();
    assert(posted.isReady() && posted.value.tag == 7 && posted.value.source == 19);
    Timer timer = Timer::afterMillis(2);
    assert(timer.isValid());
    auto tick = std::event::receiveFor(1000);
    assert(tick.isReady() && tick.value.timer == timer.handle);
    timer.stop();
    assert(std::event::tryReceive().status == 1);
    println("events ok");
    return 0;
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
| 模块 | <a id="symbol-59"></a>`struct Event` | 显式接收的定时或宿主事件值。 |
| Event | <a id="symbol-60"></a>`int64 timer` | 定时事件的宿主资源。 |
| Event | <a id="symbol-61"></a>`int64 tag` | 宿主投递的事件标签。 |
| Event | <a id="symbol-62"></a>`int64 source` | 被适配的源游标或连续存储。 |
| Event | <a id="symbol-63"></a>`int64 missed` | 周期通知合并时错过的周期数量。 |
| 模块 | <a id="symbol-64"></a>`struct Timer : Drop` | 宿主定时事件的资源包装；通过方法创建、停止与释放。 |
| Timer | <a id="symbol-65"></a>`int64 handle` | 宿主资源句柄；应通过所属类型的方法操作。 |

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
| [Timer::stop](#api-10) | `void stop()` |
| [Timer::drop](#api-11) | `void drop()` |

<a id="api-0"></a>

### receiveFor（1）

**声明**

```magel
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

```magel
std::event::receiveFor(millis);
```

**源码**：[source/std/event.mgl:19](../../source/std/event.mgl#L19)。

**完整示例**：[example/samples/event_timer.mgl](../../example/samples/event_timer.mgl)。

<a id="api-1"></a>

### receive（2）

**声明**

```magel
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

```magel
std::event::receive();
```

**源码**：[source/std/event.mgl:30](../../source/std/event.mgl#L30)。

**完整示例**：[example/samples/event_timer.mgl](../../example/samples/event_timer.mgl)。

<a id="api-2"></a>

### tryReceive（3）

**声明**

```magel
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

```magel
std::event::tryReceive();
```

**源码**：[source/std/event.mgl:31](../../source/std/event.mgl#L31)。

**完整示例**：[example/samples/event_timer.mgl](../../example/samples/event_timer.mgl)。

<a id="api-3"></a>

### post（4）

**声明**

```magel
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

```magel
std::event::post(tag, source);
```

**源码**：[source/std/event.mgl:32](../../source/std/event.mgl#L32)。

**完整示例**：[example/samples/event_timer.mgl](../../example/samples/event_timer.mgl)。

<a id="api-4"></a>

### close（5）

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
std::event::close();
```

**源码**：[source/std/event.mgl:33](../../source/std/event.mgl#L33)。

**完整示例**：[example/samples/event_timer.mgl](../../example/samples/event_timer.mgl)。

<a id="api-5"></a>

### Timer::everyNanos（6）

**声明**

```magel
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

```magel
std::event::Timer::everyNanos(nanos);
```

**源码**：[source/std/event.mgl:37](../../source/std/event.mgl#L37)。

**完整示例**：[example/samples/event_timer.mgl](../../example/samples/event_timer.mgl)。

<a id="api-6"></a>

### Timer::afterNanos（7）

**声明**

```magel
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

```magel
std::event::Timer::afterNanos(nanos);
```

**源码**：[source/std/event.mgl:38](../../source/std/event.mgl#L38)。

**完整示例**：[example/samples/event_timer.mgl](../../example/samples/event_timer.mgl)。

<a id="api-7"></a>

### Timer::everyMillis（8）

**声明**

```magel
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

```magel
std::event::Timer::everyMillis(millis);
```

**源码**：[source/std/event.mgl:39](../../source/std/event.mgl#L39)。

**完整示例**：[example/samples/event_timer.mgl](../../example/samples/event_timer.mgl)。

<a id="api-8"></a>

### Timer::afterMillis（9）

**声明**

```magel
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

```magel
std::event::Timer::afterMillis(millis);
```

**源码**：[source/std/event.mgl:44](../../source/std/event.mgl#L44)。

**完整示例**：[example/samples/event_timer.mgl](../../example/samples/event_timer.mgl)。

<a id="api-9"></a>

### Timer::isValid（10）

**声明**

```magel
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

```magel
obj.isValid();
```

**源码**：[source/std/event.mgl:49](../../source/std/event.mgl#L49)。

**完整示例**：[example/samples/event_timer.mgl](../../example/samples/event_timer.mgl)。

<a id="api-10"></a>

### Timer::stop（11）

**声明**

```magel
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

```magel
obj.stop();
```

**源码**：[source/std/event.mgl:50](../../source/std/event.mgl#L50)。

**完整示例**：[example/samples/event_timer.mgl](../../example/samples/event_timer.mgl)。

<a id="api-11"></a>

### Timer::drop（12）

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

**源码**：[source/std/event.mgl:51](../../source/std/event.mgl#L51)。

**完整示例**：[example/samples/event_timer.mgl](../../example/samples/event_timer.mgl)。
