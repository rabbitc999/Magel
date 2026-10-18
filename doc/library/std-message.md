# std::message — 消息路由

[返回库目录](README.md) · [全部符号](symbols.md)

## 引入

```magel
import std::message;
```

## 公共约定与错误边界

泛型 send/receive 按 T 在协程或线程间传递值副本；接收类型必须与发送的类型匹配。协程与线程句柄不可混用。线程功能需 --threads；带类型消息语法及宿主交互见语言参考与正式样例。

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

## 函数索引

| 名称 | 声明 |
| --- | --- |
| [sendCoroutine](#api-0) | `void sendCoroutine<T>(SharedCoroutineHandle coroutine, T value)` |
| [receiveCoroutine](#api-1) | `T receiveCoroutine<T>()` |
| [sendThread](#api-2) | `void sendThread<T>(IsolatedThreadHandle thread, T value)` |
| [receiveThread](#api-3) | `T receiveThread<T>()` |

<a id="api-0"></a>

### sendCoroutine（1）

**声明**

```magel
void sendCoroutine<T>(SharedCoroutineHandle coroutine, T value);
```

**行为**

向指定协程投递 T 类型的值副本。

跨协程 / 跨线程的消息：`import std::message;`。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `coroutine` | `SharedCoroutineHandle coroutine` | 协程句柄 | 无 |
| `value` | `T value` | 输入值或写入的新值 | 无 |

**返回值**

无返回值。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::message::sendCoroutine<T>(coroutine, value);
```

**源码**：[source/std/message.mgl:9](../../source/std/message.mgl#L9)。

**完整示例**：[example/samples/coroutine_messages.mgl](../../example/samples/coroutine_messages.mgl)。

<a id="api-1"></a>

### receiveCoroutine（2）

**声明**

```magel
T receiveCoroutine<T>();
```

**行为**

等待当前协程的一条 T 类型消息。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`T`。等待当前协程的一条 T 类型消息。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::message::receiveCoroutine<T>();
```

**源码**：[source/std/message.mgl:10](../../source/std/message.mgl#L10)。

**完整示例**：[example/samples/coroutine_messages.mgl](../../example/samples/coroutine_messages.mgl)。

<a id="api-2"></a>

### sendThread（3）

**声明**

```magel
void sendThread<T>(IsolatedThreadHandle thread, T value);
```

**行为**

向指定线程投递 T 类型的值副本。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `thread` | `IsolatedThreadHandle thread` | 线程句柄或归属线程 | 无 |
| `value` | `T value` | 输入值或写入的新值 | 无 |

**返回值**

无返回值。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::message::sendThread<T>(thread, value);
```

**源码**：[source/std/message.mgl:11](../../source/std/message.mgl#L11)。

**完整示例**：[example/samples/coroutine_messages.mgl](../../example/samples/coroutine_messages.mgl)。

<a id="api-3"></a>

### receiveThread（4）

**声明**

```magel
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

```magel
std::message::receiveThread<T>();
```

**源码**：[source/std/message.mgl:12](../../source/std/message.mgl#L12)。

**完整示例**：[example/samples/coroutine_messages.mgl](../../example/samples/coroutine_messages.mgl)。
