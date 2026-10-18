# std::thread — 隔离线程

[返回库目录](README.md) · [全部符号](symbols.md)

## 引入

```magel
import std::thread;
```

## 公共约定与错误边界

运行和导出时需 --threads。任务有独立全局变量视图；共享值用 Shared<T>，通信使用消息。wait 等待目标完成；receive 等待当前线程消息。线程句柄不是 OS 原生线程句柄，不得交给外部系统 API。

## 参考代码

完整程序：[example/samples/thread_messages.mgl](../../example/samples/thread_messages.mgl)。

```powershell
magel run example/samples/thread_messages.mgl --threads
```

<details>
<summary>展开完整参考代码</summary>

```magel

import std::thread;

int64 isolated;

void worker() {
    String message = std::thread::receive();
    isolated = 77;
    String reply = "ack:" + message;
    std::thread::send(std::thread::parent(), reply);
}

int64 main() {
    isolated = 5;
    IsolatedThreadHandle task = std::thread::create(worker);
    std::thread::send(task, "hello");
    String reply = std::thread::receive();
    std::thread::wait(task);
    println("{} {}", reply, isolated);
    return 0;
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

## 函数索引

| 名称 | 声明 |
| --- | --- |
| [create](#api-0) | `IsolatedThreadHandle create(void fn() task)` |
| [wait](#api-1) | `void wait(IsolatedThreadHandle thread)` |
| [send](#api-2) | `void send(IsolatedThreadHandle thread, std::lang::String message)` |
| [receive](#api-3) | `std::lang::String receive()` |
| [current](#api-4) | `IsolatedThreadHandle current()` |
| [parent](#api-5) | `IsolatedThreadHandle parent()` |

<a id="api-0"></a>

### create（1）

**声明**

```magel
IsolatedThreadHandle create(void fn() task);
```

**行为**

创建隔离线程，返回可供后续操作的值。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `task` | `void fn() task` | 无参数任务函数 | 无 |

**返回值**

`IsolatedThreadHandle`。创建隔离线程，返回可供后续操作的值。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::thread::create(task);
```

**源码**：[source/std/thread.mgl:14](../../source/std/thread.mgl#L14)。

**完整示例**：[example/samples/thread_messages.mgl](../../example/samples/thread_messages.mgl)。

<a id="api-1"></a>

### wait（2）

**声明**

```magel
void wait(IsolatedThreadHandle thread);
```

**行为**

调度或等待目标协程/线程结束。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `thread` | `IsolatedThreadHandle thread` | 线程句柄或归属线程 | 无 |

**返回值**

无返回值。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::thread::wait(thread);
```

**源码**：[source/std/thread.mgl:15](../../source/std/thread.mgl#L15)。

**完整示例**：[example/samples/thread_messages.mgl](../../example/samples/thread_messages.mgl)。

<a id="api-2"></a>

### send（3）

**声明**

```magel
void send(IsolatedThreadHandle thread, std::lang::String message);
```

**行为**

向目标任务发送字符串消息。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `thread` | `IsolatedThreadHandle thread` | 线程句柄或归属线程 | 无 |
| `message` | `std::lang::String message` | 诊断文本或消息载荷 | 无 |

**返回值**

无返回值。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::thread::send(thread, message);
```

**源码**：[source/std/thread.mgl:16](../../source/std/thread.mgl#L16)。

**完整示例**：[example/samples/thread_messages.mgl](../../example/samples/thread_messages.mgl)。

<a id="api-3"></a>

### receive（4）

**声明**

```magel
std::lang::String receive();
```

**行为**

等待并接收当前任务队列的一条字符串消息。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`std::lang::String`。等待并接收当前任务队列的一条字符串消息。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::thread::receive();
```

**源码**：[source/std/thread.mgl:17](../../source/std/thread.mgl#L17)。

**完整示例**：[example/samples/thread_messages.mgl](../../example/samples/thread_messages.mgl)。

<a id="api-4"></a>

### current（5）

**声明**

```magel
IsolatedThreadHandle current();
```

**行为**

返回当前协程/线程的句柄。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`IsolatedThreadHandle`。返回当前协程/线程的句柄。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::thread::current();
```

**源码**：[source/std/thread.mgl:18](../../source/std/thread.mgl#L18)。

**完整示例**：[example/samples/thread_messages.mgl](../../example/samples/thread_messages.mgl)。

<a id="api-5"></a>

### parent（6）

**声明**

```magel
IsolatedThreadHandle parent();
```

**行为**

返回创建当前任务的父协程/线程句柄。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`IsolatedThreadHandle`。返回创建当前任务的父协程/线程句柄。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::thread::parent();
```

**源码**：[source/std/thread.mgl:19](../../source/std/thread.mgl#L19)。

**完整示例**：[example/samples/thread_messages.mgl](../../example/samples/thread_messages.mgl)。
