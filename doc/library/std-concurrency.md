# std::concurrency — 显式共享值

[返回库目录](README.md) · [全部符号](symbols.md)

## 引入

```magel
import std::concurrency;
```

## 公共约定与错误边界

Shared<T> 提供跨隔离线程的显式共享槽，get/set/swap 单次操作原子。get 后再 set 的组合不是原子事务。普通全局变量在线程间隔离；启动线程需 --threads。

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
    IsolatedThreadHandle task = std::thread::create(typedWorker);
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
| 模块 | <a id="symbol-37"></a>`struct Shared<T>` | 显式共享槽：普通全局变量在线程之间仍然隔离。每次 get/set/swap 都是原子操作。 |
| Shared | <a id="symbol-38"></a>`int64 handle` | 宿主资源句柄；应通过所属类型的方法操作。 |

## 函数索引

| 名称 | 声明 |
| --- | --- |
| [Shared::create](#api-0) | `static Shared<T> create(T value)` |
| [Shared::get](#api-1) | `T get()` |
| [Shared::set](#api-2) | `void set(T value)` |
| [Shared::swap](#api-3) | `T swap(T value)` |

<a id="api-0"></a>

### Shared::create（1）

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

**源码**：[source/std/concurrency.mgl:8](../../source/std/concurrency.mgl#L8)。

**完整示例**：[example/samples/typed_messages_shared.mgl](../../example/samples/typed_messages_shared.mgl)。

<a id="api-1"></a>

### Shared::get（2）

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

**源码**：[source/std/concurrency.mgl:11](../../source/std/concurrency.mgl#L11)。

**完整示例**：[example/samples/typed_messages_shared.mgl](../../example/samples/typed_messages_shared.mgl)。

<a id="api-2"></a>

### Shared::set（3）

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

**源码**：[source/std/concurrency.mgl:12](../../source/std/concurrency.mgl#L12)。

**完整示例**：[example/samples/typed_messages_shared.mgl](../../example/samples/typed_messages_shared.mgl)。

<a id="api-3"></a>

### Shared::swap（4）

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

**源码**：[source/std/concurrency.mgl:13](../../source/std/concurrency.mgl#L13)。

**完整示例**：[example/samples/typed_messages_shared.mgl](../../example/samples/typed_messages_shared.mgl)。
