# std::coroutine — 协作式协程

[返回库目录](README.md) · [全部符号](symbols.md)

## 引入

```magel
import std::coroutine;
```

## 公共约定与错误边界

create 创建共享执行上下文的协程，yield 让出执行，wait 推进调度直到目标完成。receive 在无消息时等待调度。yelid 是保留的拼写兼容入口，推荐 yield。跨线程隔离任务使用 std::thread。

## 参考代码

完整程序：[example/samples/coroutine_messages.mgl](../../example/samples/coroutine_messages.mgl)。

```powershell
magel run example/samples/coroutine_messages.mgl
```

<details>
<summary>展开完整参考代码</summary>

```magel

import std::coroutine;

void worker() {
    String message = std::coroutine::receive();
    String reply = "ack:" + message;
    std::coroutine::send(std::coroutine::parent(), reply);
}

int64 main() {
    SharedCoroutineHandle task = std::coroutine::create(worker);
    std::coroutine::send(task, "hello");
    String reply = std::coroutine::receive();
    std::coroutine::wait(task);
    println("{}", reply);
    return 0;
}
```

</details>

<details>
<summary>示例输出</summary>

```text
ack:hello
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
| [create](#api-0) | `SharedCoroutineHandle create(void fn() task)` |
| [wait](#api-1) | `void wait(SharedCoroutineHandle coroutine)` |
| [send](#api-2) | `void send(SharedCoroutineHandle coroutine, std::lang::String message)` |
| [receive](#api-3) | `std::lang::String receive()` |
| [current](#api-4) | `SharedCoroutineHandle current()` |
| [parent](#api-5) | `SharedCoroutineHandle parent()` |
| [yield](#api-6) | `void yield()` |
| [yelid](#api-7) | `void yelid()` |

<a id="api-0"></a>

### create（1）

**声明**

```magel
SharedCoroutineHandle create(void fn() task);
```

**行为**

创建共享协程，返回可供后续操作的值。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `task` | `void fn() task` | 无参数任务函数 | 无 |

**返回值**

`SharedCoroutineHandle`。创建共享协程，返回可供后续操作的值。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::coroutine::create(task);
```

**源码**：[source/std/coroutine.mgl:14](../../source/std/coroutine.mgl#L14)。

**完整示例**：[example/samples/coroutine_messages.mgl](../../example/samples/coroutine_messages.mgl)。

<a id="api-1"></a>

### wait（2）

**声明**

```magel
void wait(SharedCoroutineHandle coroutine);
```

**行为**

调度或等待目标协程/线程结束。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `coroutine` | `SharedCoroutineHandle coroutine` | 协程句柄 | 无 |

**返回值**

无返回值。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::coroutine::wait(coroutine);
```

**源码**：[source/std/coroutine.mgl:15](../../source/std/coroutine.mgl#L15)。

**完整示例**：[example/samples/coroutine_messages.mgl](../../example/samples/coroutine_messages.mgl)。

<a id="api-2"></a>

### send（3）

**声明**

```magel
void send(SharedCoroutineHandle coroutine, std::lang::String message);
```

**行为**

向目标任务发送字符串消息。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `coroutine` | `SharedCoroutineHandle coroutine` | 协程句柄 | 无 |
| `message` | `std::lang::String message` | 诊断文本或消息载荷 | 无 |

**返回值**

无返回值。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::coroutine::send(coroutine, message);
```

**源码**：[source/std/coroutine.mgl:16](../../source/std/coroutine.mgl#L16)。

**完整示例**：[example/samples/coroutine_messages.mgl](../../example/samples/coroutine_messages.mgl)。

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
std::coroutine::receive();
```

**源码**：[source/std/coroutine.mgl:17](../../source/std/coroutine.mgl#L17)。

**完整示例**：[example/samples/coroutine_messages.mgl](../../example/samples/coroutine_messages.mgl)。

<a id="api-4"></a>

### current（5）

**声明**

```magel
SharedCoroutineHandle current();
```

**行为**

返回当前协程/线程的句柄。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`SharedCoroutineHandle`。返回当前协程/线程的句柄。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::coroutine::current();
```

**源码**：[source/std/coroutine.mgl:18](../../source/std/coroutine.mgl#L18)。

**完整示例**：[example/samples/coroutine_messages.mgl](../../example/samples/coroutine_messages.mgl)。

<a id="api-5"></a>

### parent（6）

**声明**

```magel
SharedCoroutineHandle parent();
```

**行为**

返回创建当前任务的父协程/线程句柄。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`SharedCoroutineHandle`。返回创建当前任务的父协程/线程句柄。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::coroutine::parent();
```

**源码**：[source/std/coroutine.mgl:19](../../source/std/coroutine.mgl#L19)。

**完整示例**：[example/samples/coroutine_messages.mgl](../../example/samples/coroutine_messages.mgl)。

<a id="api-6"></a>

### yield（7）

**声明**

```magel
void yield();
```

**行为**

主动让出协程执行权。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

无返回值。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::coroutine::yield();
```

**源码**：[source/std/coroutine.mgl:20](../../source/std/coroutine.mgl#L20)。

**完整示例**：[example/samples/coroutine_messages.mgl](../../example/samples/coroutine_messages.mgl)。

<a id="api-7"></a>

### yelid（8）

**声明**

```magel
void yelid();
```

**行为**

yield 的拼写兼容入口。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

无返回值。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::coroutine::yelid();
```

**源码**：[source/std/coroutine.mgl:21](../../source/std/coroutine.mgl#L21)。

**完整示例**：[example/samples/coroutine_messages.mgl](../../example/samples/coroutine_messages.mgl)。
