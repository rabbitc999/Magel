# std::event — 中断事件与定时器

[返回库目录](README.md) · [全部符号](symbols.md)

## 引入

```magel
import std::event;
```

## 公共约定与错误边界

事件在所属线程的 VM 指令安全点交付，不重入正在运行的宿主回调。定时器需 start 后运行；过期周期跳过，不累计补发。宿主无时钟注入时采用指令虚拟时钟。销毁后的触发为空操作。Timer 实现 Drop；Interrupt.of 接收句柄而非按值复制 Timer。

## 参考代码

完整程序：[example/samples/interrupt_arguments.mgl](../../example/samples/interrupt_arguments.mgl)。

```powershell
magel run example/samples/interrupt_arguments.mgl
```

<details>
<summary>展开完整参考代码</summary>

```magel
/*
 * interrupt_arguments.mgl —— 事件回调的**自定义参数列表**。
 *
 * `std::event::InterruptHandler` 只带一个 `int64`；参数更多时用
 * `std::event::EventCallback(int64 event, List<Any> arguments)`，
 * 事件值 + 自定义参数列表一起交付：
 *
 *   * 现成列表：`std::event::bindEventArguments(handle, callback, args)`；
 *   * 变长实参：`bindEventArguments(handle, callback, a, b, c)` —— 每个实参必须是
 *     `Any`（编译期逐项校验），空变长实参表示"没有自定义参数"；
 *   * 登记前校验：句柄已 `drop`（`__timerAlive` 为假）或回调是空函数指针时返回 0，
 *     不登记、不打断程序。
 *
 * 参数不止 `List<Any>`：结构体、元组这类"其他自定义参数列表"用
 * **用户蹦床**接 —— 参数放全局，写一个 `InterruptHandler` 转给真正的用户回调；
 * 末尾的 `NavPose` 就是这一模式（自定义结构体参数）。
 *
 * 交付仍在**下一条指令边界**：`raise` 之后往前走几条指令回调才会跑。
 */

import std::event;

/* ---- 自定义参数列表：事件值 + 任意长列表 ---- */
int64 seenEvent = 0;
int64 seenCount = 0;
int64 seenDigest = 0;   /* 每个元素按类型折算成整数再累加，输出才稳定 */

/* 一个参数折算成整数：通过类型判断接口分支。 */
int64 argDigest(Any value) {
    if (isType<int64>(&value)) { return (int64)value.getValue(); }
    if (isType<float64>(&value)) { return (int64)((float64)value.getValue() * 100.0); }
    if (isType<String>(&value)) {
        String text = value.getValue();
        return (int64)len(text);
    }
    return 0;
}

void onArguments(int64 event, List<Any> arguments) {
    seenEvent = event;
    seenCount = arguments.len();
    seenDigest = 0;
    int64 i = 0;
    while (i < arguments.len()) {
        Any value = arguments[i];
        seenDigest = seenDigest + argDigest(value);
        i = i + 1;
    }
}

/* ---- 其他自定义参数（结构体）：用户蹦床转给真正的回调 ---- */
struct NavPose { float64 x; float64 y; float64 yaw; };
NavPose navPose;
int64 navFires = 0;

void onNav(int64 event) {
    navFires = navFires + 1;
    println("trampoline x={} y={} yaw={} fired={}",
    navPose.x, navPose.y, navPose.yaw, navFires);
}

int64 main() {
    Timer events = Timer::everyNanos(0);   /* 周期 0：只接显式 raise */

    /* 现成列表：int64 + float64 + String，取的时候按类型还原。 */
    List<Any> args;
    Any number = 7;
    Any real = 2.5;
    Any word = "hello";
    args.push(number);
    args.push(real);
    args.push(word);
    int32 ok = std::event::bindEventArguments(events.handle, onArguments, args);
    events.raise(42);
    int64 i = 0;
    while (i < 8) { i = i + 1; }
    println("list ok={} event={} count={} digest={}", ok, seenEvent, seenCount, seenDigest);

    /* 变长实参：直接列出来，编译器逐项校验每个实参是 Any。 */
    Any nine = 9;
    Any quarter = 1.25;
    ok = std::event::bindEventArguments(events.handle, onArguments, nine, quarter);
    events.raise(1);
    i = 0;
    while (i < 8) { i = i + 1; }
    println("variadic ok={} event={} count={} digest={}", ok, seenEvent, seenCount, seenDigest);

    /* 空变长实参：没有自定义参数。 */
    ok = std::event::bindEventArguments(events.handle, onArguments);
    events.raise(2);
    i = 0;
    while (i < 8) { i = i + 1; }
    println("empty ok={} event={} count={}", ok, seenEvent, seenCount);

    /* 登记校验：句柄已释放 / 回调是空函数指针，都返回 0。 */
    Timer dead = Timer::everyNanos(0);
    EventHandle stale = dead.handle;
    dead.drop();
    void fn(int64, List<Any>) none;
    int32 deadOk = std::event::bindEventArguments(stale, onArguments, args);
    int32 noneOk = std::event::bindEventArguments(events.handle, none, args);
    println("invalid handle={} null={}", deadOk, noneOk);

    /* 其他自定义参数：结构体走用户蹦床。 */
    Timer pose = Timer::everyNanos(0);
    pose.onInterrupt(onNav);
    navPose.x = 1.5;
    navPose.y = -2.25;
    navPose.yaw = 0.75;
    pose.raise(7);
    i = 0;
    while (i < 8) { i = i + 1; }

    /* 取消登记后不再交付。 */
    std::event::unbindEventArguments();
    events.raise(3);
    i = 0;
    while (i < 8) { i = i + 1; }
    println("unbound event={} count={}", seenEvent, seenCount);

    delete args;
    return 0;
}
```

</details>

<details>
<summary>示例输出</summary>

```text
list ok=1 event=42 count=3 digest=262
variadic ok=1 event=1 count=2 digest=134
empty ok=1 event=2 count=0
invalid handle=0 null=0
trampoline x=1.5 y=-2.25 yaw=0.75 fired=1
unbound event=2 count=0
```

</details>

聚合入口的具体操作参见对应子模块；本页类型/方法的调用形式列在下方。

## 类型、枚举、常量与字段

公开字段按源码原样列出；句柄和游标字段用于包装器状态，通常通过方法操作。枚举的括号表示变体载荷。

| 所属类型 | 声明 | 说明 |
| --- | --- | --- |
| 模块 | <a id="symbol-52"></a>`struct Interrupt` | 中断句柄：一个中断事件函数 + 它归属的线程（+ 可选的定时器句柄）。
每个线程有**自己的**虚拟机求值循环与中断队列；中断只在**创建它的那个线程**里交付，
别的线程不受影响。`Timer` 创建时就把创建者的线程 id 记进定时器槽，`Timer.thread()`
可以读出来，`Interrupt.of(timer, handler)` 默认就用它当归属线程（也就是创建定时器的
那个线程）；宿主胶水拿这个句柄就能跨线程投递，投错线程是空操作
（见 `Machine::raiseInterrupt(InterruptHandle)`）。
句柄自带函数指针，所以定时器**没绑过**回调也能交付；`timer` 只负责"释放过就丢弃"
和线程归属。`raise()` 从 Magel 侧投递，等价于宿主调 `Machine::raiseInterrupt(句柄, source)`。 |
| Interrupt | <a id="symbol-53"></a>`void fn(int64) callback` | 宿主事件发生时执行的回调。 |
| Interrupt | <a id="symbol-54"></a>`EventHandle timer` | 定时事件的宿主资源。 |
| Interrupt | <a id="symbol-55"></a>`int64 thread` | 执行回调的宿主任務标识。 |
| 模块 | <a id="symbol-56"></a>`struct Timer : Drop` | 宿主定时事件的资源包装；通过方法创建、停止与释放。 |
| Timer | <a id="symbol-57"></a>`EventHandle handle` | 宿主资源句柄；应通过所属类型的方法操作。 |
| 模块 | <a id="symbol-58"></a>`struct EventBinding` | 当前登记的回调与参数。放进**模块级结构体**而不是裸容器全局：裸 `std::lang::List` 全局
一 `import` 就分配一个运行期对象，程序不登记也会在停机时报"没释放"；结构体的
容器字段只在真正登记时才分配。 |
| EventBinding | <a id="symbol-59"></a>`void fn(int64, std::lang::List<std::lang::Any>) callback` | 宿主事件发生时执行的回调。 |
| EventBinding | <a id="symbol-60"></a>`std::lang::List<std::lang::Any> arguments` | 绑定到事件回调的实际参数。 |
| EventBinding | <a id="symbol-61"></a>`bool bound` | 是否已建立事件绑定。 |

## 函数索引

| 名称 | 声明 |
| --- | --- |
| [Interrupt::of](#api-0) | `static Interrupt of(EventHandle timer, int64 thread, void fn(int64) handler)` |
| [Interrupt::of](#api-1) | `static Interrupt of(EventHandle timer, void fn(int64) handler)` |
| [Interrupt::raise](#api-2) | `void raise(int64 source)` |
| [Interrupt::raise](#api-3) | `void raise()` |
| [Timer::Timer](#api-4) | `Timer(int64 period)` |
| [Timer::everyNanos](#api-5) | `static Timer everyNanos(int64 period)` |
| [Timer::everyMillis](#api-6) | `static Timer everyMillis(int64 millis)` |
| [Timer::onInterrupt](#api-7) | `void onInterrupt(void fn(int64) callback)` |
| [Timer::onTick](#api-8) | `void onTick(void fn(int64) callback)` |
| [Timer::start](#api-9) | `void start()` |
| [Timer::stop](#api-10) | `void stop()` |
| [Timer::ticks](#api-11) | `int64 ticks()` |
| [Timer::running](#api-12) | `bool running()` |
| [Timer::thread](#api-13) | `int64 thread()` |
| [Timer::raise](#api-14) | `void raise(int64 source)` |
| [Timer::raise](#api-15) | `void raise()` |
| [Timer::drop](#api-16) | `void drop()` |
| [bindEventArguments](#api-17) | `int32 bindEventArguments(EventHandle handle, void fn(int64, std::lang::List<std::lang::Any>) callback, std::lang::List<std::lang::Any> arguments)` |
| [bindEventArguments](#api-18) | `int32 bindEventArguments(EventHandle handle, void fn(int64, std::lang::List<std::lang::Any>) callback, std::lang::Any... arguments)` |
| [unbindEventArguments](#api-19) | `void unbindEventArguments()` |

<a id="api-0"></a>

### Interrupt::of（1）

**声明**

```magel
static Interrupt of(EventHandle timer, int64 thread, void fn(int64) handler);
```

**行为**

组合事件句柄、所属线程和回调为中断描述值。

按值传 `Timer` 会触发一次 Drop（把定时器销毁），所以这里只收句柄。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `timer` | `EventHandle timer` | 事件/定时器句柄 | 无 |
| `thread` | `int64 thread` | 线程句柄或归属线程 | 无 |
| `handler` | `void fn(int64) handler` | 中断回调 | 无 |

**返回值**

`Interrupt`。组合事件句柄、所属线程和回调为中断描述值。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::event::Interrupt::of(timer, thread, handler);
```

**源码**：[source/std/event.mgl:66](../../source/std/event.mgl#L66)。

**完整示例**：[example/samples/interrupt_arguments.mgl](../../example/samples/interrupt_arguments.mgl)。

<a id="api-1"></a>

### Interrupt::of（2）

**声明**

```magel
static Interrupt of(EventHandle timer, void fn(int64) handler);
```

**行为**

组合事件句柄、所属线程和回调为中断描述值。

不给线程就默认归属创建该定时器的线程（某一线程创建的中断在它自己的 VM 里跑）。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `timer` | `EventHandle timer` | 事件/定时器句柄 | 无 |
| `handler` | `void fn(int64) handler` | 中断回调 | 无 |

**返回值**

`Interrupt`。组合事件句柄、所属线程和回调为中断描述值。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::event::Interrupt::of(timer, handler);
```

**源码**：[source/std/event.mgl:75](../../source/std/event.mgl#L75)。

**完整示例**：[example/samples/interrupt_arguments.mgl](../../example/samples/interrupt_arguments.mgl)。

<a id="api-2"></a>

### Interrupt::raise（3）

**声明**

```magel
void raise(int64 source);
```

**行为**

排入一次中断事件，等待安全点交付。

主动投递这个句柄：归属别的线程时是空操作，交付仍在指令边界做。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `source` | `int64 source` | 输入游标、存储或事件来源值 | 无 |

**返回值**

无返回值。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.raise(source);
```

**源码**：[source/std/event.mgl:84](../../source/std/event.mgl#L84)。

**完整示例**：[example/samples/interrupt_arguments.mgl](../../example/samples/interrupt_arguments.mgl)。

<a id="api-3"></a>

### Interrupt::raise（4）

**声明**

```magel
void raise();
```

**行为**

排入一次中断事件，等待安全点交付。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

无返回值。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.raise();
```

**源码**：[source/std/event.mgl:85](../../source/std/event.mgl#L85)。

**完整示例**：[example/samples/interrupt_arguments.mgl](../../example/samples/interrupt_arguments.mgl)。

<a id="api-4"></a>

### Timer::Timer（5）

**声明**

```magel
Timer(int64 period);
```

**行为**

以纳秒周期创建停止态定时器。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `period` | `int64 period` | 定时器周期 | 无 |

**返回值**

`Timer`。构造并初始化当前实例。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
// 构造：Timer obj = new(period);
```

**源码**：[source/std/event.mgl:91](../../source/std/event.mgl#L91)。

**完整示例**：[example/samples/interrupt_arguments.mgl](../../example/samples/interrupt_arguments.mgl)。

<a id="api-5"></a>

### Timer::everyNanos（6）

**声明**

```magel
static Timer everyNanos(int64 period);
```

**行为**

创建纳秒周期的停止态定时器。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `period` | `int64 period` | 定时器周期 | 无 |

**返回值**

`Timer`。创建纳秒周期的停止态定时器。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::event::Timer::everyNanos(period);
```

**源码**：[source/std/event.mgl:95](../../source/std/event.mgl#L95)。

**完整示例**：[example/samples/interrupt_arguments.mgl](../../example/samples/interrupt_arguments.mgl)。

<a id="api-6"></a>

### Timer::everyMillis（7）

**声明**

```magel
static Timer everyMillis(int64 millis);
```

**行为**

创建毫秒周期的停止态定时器。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `millis` | `int64 millis` | 毫秒数 | 无 |

**返回值**

`Timer`。创建毫秒周期的停止态定时器。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::event::Timer::everyMillis(millis);
```

**源码**：[source/std/event.mgl:101](../../source/std/event.mgl#L101)。

**完整示例**：[example/samples/interrupt_arguments.mgl](../../example/samples/interrupt_arguments.mgl)。

<a id="api-7"></a>

### Timer::onInterrupt（8）

**声明**

```magel
void onInterrupt(void fn(int64) callback);
```

**行为**

生成或绑定中断回调，见具体重载。

绑定中断事件函数；再绑一次就换掉上一个。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `callback` | `void fn(int64) callback` | 事件或任务回调 | 无 |

**返回值**

无返回值。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.onInterrupt(callback);
```

**源码**：[source/std/event.mgl:108](../../source/std/event.mgl#L108)。

**完整示例**：[example/samples/interrupt_arguments.mgl](../../example/samples/interrupt_arguments.mgl)。

<a id="api-8"></a>

### Timer::onTick（9）

**声明**

```magel
void onTick(void fn(int64) callback);
```

**行为**

为定时器绑定接收 source 的回调。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `callback` | `void fn(int64) callback` | 事件或任务回调 | 无 |

**返回值**

无返回值。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.onTick(callback);
```

**源码**：[source/std/event.mgl:109](../../source/std/event.mgl#L109)。

**完整示例**：[example/samples/interrupt_arguments.mgl](../../example/samples/interrupt_arguments.mgl)。

<a id="api-9"></a>

### Timer::start（10）

**声明**

```magel
void start();
```

**行为**

启动定时器。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

无返回值。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.start();
```

**源码**：[source/std/event.mgl:111](../../source/std/event.mgl#L111)。

**完整示例**：[example/samples/interrupt_arguments.mgl](../../example/samples/interrupt_arguments.mgl)。

<a id="api-10"></a>

### Timer::stop（11）

**声明**

```magel
void stop();
```

**行为**

停止定时器，保留句柄。

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

**源码**：[source/std/event.mgl:112](../../source/std/event.mgl#L112)。

**完整示例**：[example/samples/interrupt_arguments.mgl](../../example/samples/interrupt_arguments.mgl)。

<a id="api-11"></a>

### Timer::ticks（12）

**声明**

```magel
int64 ticks();
```

**行为**

返回累计触发计数。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`int64`。返回累计触发计数。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.ticks();
```

**源码**：[source/std/event.mgl:113](../../source/std/event.mgl#L113)。

**完整示例**：[example/samples/interrupt_arguments.mgl](../../example/samples/interrupt_arguments.mgl)。

<a id="api-12"></a>

### Timer::running（13）

**声明**

```magel
bool running();
```

**行为**

判断定时器是否在运行。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`bool`。判断定时器是否在运行。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.running();
```

**源码**：[source/std/event.mgl:114](../../source/std/event.mgl#L114)。

**完整示例**：[example/samples/interrupt_arguments.mgl](../../example/samples/interrupt_arguments.mgl)。

<a id="api-13"></a>

### Timer::thread（14）

**声明**

```magel
int64 thread();
```

**行为**

返回事件归属线程。

这个中断归属的线程 id（创建它的线程）；中断只在该线程的 VM 里交付。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`int64`。返回事件归属线程。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.thread();
```

**源码**：[source/std/event.mgl:117](../../source/std/event.mgl#L117)。

**完整示例**：[example/samples/interrupt_arguments.mgl](../../example/samples/interrupt_arguments.mgl)。

<a id="api-14"></a>

### Timer::raise（15）

**声明**

```magel
void raise(int64 source);
```

**行为**

排入一次中断事件，等待安全点交付。

主动触发一次（程序里也能用；外部胶水走 Machine::raiseInterrupt）。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `source` | `int64 source` | 输入游标、存储或事件来源值 | 无 |

**返回值**

无返回值。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.raise(source);
```

**源码**：[source/std/event.mgl:120](../../source/std/event.mgl#L120)。

**完整示例**：[example/samples/interrupt_arguments.mgl](../../example/samples/interrupt_arguments.mgl)。

<a id="api-15"></a>

### Timer::raise（16）

**声明**

```magel
void raise();
```

**行为**

排入一次中断事件，等待安全点交付。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

无返回值。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.raise();
```

**源码**：[source/std/event.mgl:121](../../source/std/event.mgl#L121)。

**完整示例**：[example/samples/interrupt_arguments.mgl](../../example/samples/interrupt_arguments.mgl)。

<a id="api-16"></a>

### Timer::drop（17）

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

**源码**：[source/std/event.mgl:123](../../source/std/event.mgl#L123)。

**完整示例**：[example/samples/interrupt_arguments.mgl](../../example/samples/interrupt_arguments.mgl)。

<a id="api-17"></a>

### bindEventArguments（18）

**声明**

```magel
int32 bindEventArguments(EventHandle handle, void fn(int64, std::lang::List<std::lang::Any>) callback, std::lang::List<std::lang::Any> arguments);
```

**行为**

将自定义参数绑定到事件回调。

现成列表形式：`bindEventArguments(handle, callback, args)`。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `handle` | `EventHandle handle` | 本模块创建且仍有效的宿主句柄 | 无 |
| `callback` | `void fn(int64, std::lang::List<std::lang::Any>) callback` | 事件或任务回调 | 无 |
| `arguments` | `std::lang::List<std::lang::Any> arguments` | 回调参数值 | 无 |

**返回值**

`int32`。将自定义参数绑定到事件回调。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::event::bindEventArguments(handle, callback, arguments);
```

**源码**：[source/std/event.mgl:180](../../source/std/event.mgl#L180)。

**完整示例**：[example/samples/interrupt_arguments.mgl](../../example/samples/interrupt_arguments.mgl)。

<a id="api-18"></a>

### bindEventArguments（19）

**声明**

```magel
int32 bindEventArguments(EventHandle handle, void fn(int64, std::lang::List<std::lang::Any>) callback, std::lang::Any... arguments);
```

**行为**

将自定义参数绑定到事件回调。

变长形式：`bindEventArguments(handle, callback, a, b, c)` 或者不带参数。 每个实参**必须**是 `std::lang::Any`（编译期逐项校验），空变长实参表示「没有自定义参数」。 元素个数 / 类型由调用点写死，与回调里的取值一一对应。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `handle` | `EventHandle handle` | 本模块创建且仍有效的宿主句柄 | 无 |
| `callback` | `void fn(int64, std::lang::List<std::lang::Any>) callback` | 事件或任务回调 | 无 |
| `arguments` | `std::lang::Any... arguments` | 回调参数值 | 无 |

**返回值**

`int32`。将自定义参数绑定到事件回调。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::event::bindEventArguments(handle, callback, arguments);
```

**源码**：[source/std/event.mgl:190](../../source/std/event.mgl#L190)。

**完整示例**：[example/samples/interrupt_arguments.mgl](../../example/samples/interrupt_arguments.mgl)。

<a id="api-19"></a>

### unbindEventArguments（20）

**声明**

```magel
void unbindEventArguments();
```

**行为**

解除事件的自定义参数绑定。

取消登记；之后事件不再交付带参数回调。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

无返回值。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::event::unbindEventArguments();
```

**源码**：[source/std/event.mgl:197](../../source/std/event.mgl#L197)。

**完整示例**：[example/samples/interrupt_arguments.mgl](../../example/samples/interrupt_arguments.mgl)。
