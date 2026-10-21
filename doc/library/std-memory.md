# std::memory — 内存视图与 arena

[返回库目录](README.md) · [全部符号](symbols.md)

## 引入

```magel
import std::memory;
```

## 公共约定与错误边界

Span<T> 是引用加长度，不拥有缓冲区；iterator 返回元素 mut&。调用者保证底层连续存储有效。Arena.allocate 返回视图，reset/drop 后全部视图失效；不能继续解引用。Memory.stats 返回宿主分配器统计，不代表整个操作系统进程内存。

## 参考代码

完整程序：[example/samples/std_parse_random_path_memory.mgl](../../example/samples/std_parse_random_path_memory.mgl)。

```powershell
magel run example/samples/std_parse_random_path_memory.mgl
```

<details>
<summary>展开完整参考代码</summary>

```magel
import std::memory;
import std::parse;
import std::path;
import std::random;

int64 main() {
    Result<int64, std::parse::ParseError> integer = std::parse::Parse::parseInt64("-42");
    Result<uint64, std::parse::ParseError> natural = std::parse::Parse::parseUInt64("18446744073709551615");
    Result<float64, std::parse::ParseError> real = std::parse::Parse::parseFloat64("12.5");
    Result<bool, std::parse::ParseError> flag = std::parse::Parse::parseBool("true");
    match (integer) {
        Result<int64, std::parse::ParseError>::Ok(value) => { println("int64={}", value); }
        Result<int64, std::parse::ParseError>::Err(error) => { println("int64=error"); }
    }
    match (natural) {
        Result<uint64, std::parse::ParseError>::Ok(value) => { println("uint64={}", value); }
        Result<uint64, std::parse::ParseError>::Err(error) => { println("uint64=error"); }
    }
    match (real) {
        Result<float64, std::parse::ParseError>::Ok(value) => { println("float64={:1F}", value); }
        Result<float64, std::parse::ParseError>::Err(error) => { println("float64=error"); }
    }
    match (flag) {
        Result<bool, std::parse::ParseError>::Ok(value) => { println("bool={}", (int64)value); }
        Result<bool, std::parse::ParseError>::Err(error) => { println("bool=error"); }
    }

    Result<int64, std::parse::ParseError> empty = std::parse::Parse::parseInt64("");
    Result<int64, std::parse::ParseError> invalid = std::parse::Parse::parseInt64("4x");
    Result<int64, std::parse::ParseError> overflow =
    std::parse::Parse::parseInt64("999999999999999999999999");
    match (empty) {
        Result<int64, std::parse::ParseError>::Ok(value) => { println("empty=bad"); }
        Result<int64, std::parse::ParseError>::Err(error) => {
            match (error) {
                std::parse::ParseError::Empty => { println("empty=ok"); }
                _ => { println("empty=bad"); }
            }
        }
    }
    match (invalid) {
        Result<int64, std::parse::ParseError>::Ok(value) => { println("invalid=bad"); }
        Result<int64, std::parse::ParseError>::Err(error) => {
            match (error) {
                std::parse::ParseError::Invalid => { println("invalid=ok"); }
                _ => { println("invalid=bad"); }
            }
        }
    }
    match (overflow) {
        Result<int64, std::parse::ParseError>::Ok(value) => { println("overflow=bad"); }
        Result<int64, std::parse::ParseError>::Err(error) => {
            match (error) {
                std::parse::ParseError::Overflow => { println("overflow=ok"); }
                _ => { println("overflow=bad"); }
            }
        }
    }

    std::random::Random first = std::random::Random::withSeed((uint64)7);
    std::random::Random second = std::random::Random::withSeed((uint64)7);
    uint64 firstValue = first.nextUInt64();
    uint64 secondValue = second.nextUInt64();
    assert(firstValue == secondValue);
    println("random={},{}", firstValue, first.nextInt(-10, 10));
    std::random::Random entropy = std::random::Random::fromEntropy();
    entropy.nextUInt64();

    String joined = std::path::PathString::join("alpha/beta", "../gamma.txt");
    String name = std::path::PathString::fileName("alpha/gamma.txt");
    String extension = std::path::PathString::extension("alpha/gamma.txt");
    String normalized = std::path::PathString::normalize("a\\b/./c/../d");
    println("path={}", joined);
    println("name={} ext={}", name, extension);
    println("normalized={}", normalized);

    int64 before = std::memory::Memory::stats().liveBytes;
    std::memory::Arena arena = std::memory::Arena::create();
    std::memory::Span<char> bytes = arena.allocate(16);
    assert(bytes.len() == 16);
    int64 during = std::memory::Memory::stats().liveBytes;
    assert(during >= before + 16);
    arena.reset();
    int64 after = std::memory::Memory::stats().liveBytes;
    assert(after <= during - 16);
    println("memory=ok");
    return 0;
}
```

</details>

<details>
<summary>示例输出</summary>

```text
int64=-42
uint64=18446744073709551615
float64=12.5
bool=1
empty=ok
invalid=ok
overflow=ok
random=9098160460397411210,-5
path=alpha/gamma.txt
name=gamma.txt ext=.txt
normalized=a/b/d
memory=ok
```

</details>

聚合入口的具体操作参见对应子模块；本页类型/方法的调用形式列在下方。

## 类型、枚举、常量与字段

公开字段按源码原样列出；句柄和游标字段用于包装器状态，通常通过方法操作。枚举的括号表示变体载荷。

| 所属类型 | 声明 | 说明 |
| --- | --- | --- |
| 模块 | <a id="symbol-336"></a>`struct Span<T>: Iterator<T mut&>, Index<T, int64>` | `Span<T>` 是"指针 + 长度"的视图，所以它的迭代器**返回元素指针**：
`for (T mut& p in span) { *p = …; }` 拿到的是可写元素地址，不复制元素；
只要值就读 `span[i]` 或先 `T v = *p;`。 |
| Span | <a id="symbol-337"></a>`T mut& data` | 视图引用的连续存储；视图不延长来源的生命周期。 |
| Span | <a id="symbol-338"></a>`int64 length` | 连续存储中可访问的元素数量。 |
| 模块 | <a id="symbol-339"></a>`struct MemoryStats` | 当前活跃分配的字节数、块数及历史字节峰值快照。 |
| MemoryStats | <a id="symbol-340"></a>`int64 liveBytes` | 当前活跃分配的字节数。 |
| MemoryStats | <a id="symbol-341"></a>`int64 liveBlocks` | 当前活跃分配的块数。 |
| MemoryStats | <a id="symbol-342"></a>`int64 peakBytes` | 历史活跃分配字节数的峰值。 |
| 模块 | <a id="symbol-343"></a>`struct Memory` | 内存统计与宿主内存管理操作的静态入口。 |
| 模块 | <a id="symbol-344"></a>`struct Arena : Drop` | 拥有宿主 arena 句柄的资源包装，集中管理 arena 分配。 |
| Arena | <a id="symbol-345"></a>`uint64 handle` | 宿主资源句柄；应通过所属类型的方法操作。 |

## 函数索引

| 名称 | 声明 |
| --- | --- |
| [Span::from](#api-0) | `static Span<T> from(T mut& data, int64 length)` |
| [Span::index](#api-1) | `T index(int64 key)` |
| [Span::iterator](#api-2) | `Option<T mut&> iterator(int64 index)` |
| [Span::len](#api-3) | `int64 len()` |
| [Memory::stats](#api-4) | `static MemoryStats stats()` |
| [Arena::create](#api-5) | `static Arena create()` |
| [Arena::allocate](#api-6) | `Span<char> allocate(int64 bytes)` |
| [Arena::reset](#api-7) | `void reset()` |
| [Arena::drop](#api-8) | `void drop()` |

<a id="api-0"></a>

### Span::from（1）

**声明**

```magel
static Span<T> from(T mut& data, int64 length);
```

**行为**

用首元素引用和长度创建非拥有 Span 视图。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `data` | `T mut& data` | 连续存储的首元素引用，调用者保证范围有效 | 无 |
| `length` | `int64 length` | 有效长度 | 无 |

**返回值**

`Span<T>`。用首元素引用和长度创建非拥有 Span 视图。

**错误与边界**

遵循本页“公共约定与错误边界”。
显式前置检查：`length >= 0`。

**调用形式**

```magel
std::memory::Span<T>::from(data, length);
```

**源码**：[source/std/memory.mgl:11](../../source/std/memory.mgl#L11)。

**完整示例**：[example/samples/std_parse_random_path_memory.mgl](../../example/samples/std_parse_random_path_memory.mgl)。

<a id="api-1"></a>

### Span::index（2）

**声明**

```magel
T index(int64 key);
```

**行为**

读取 key 对应的元素，供下标语法调用。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `key` | `int64 key` | 查找键或下标 | 无 |

**返回值**

`T`。读取 key 对应的元素，供下标语法调用。

**错误与边界**

遵循本页“公共约定与错误边界”。
显式前置检查：`key >= 0 && key < self.length`。

**调用形式**

```magel
obj.index(key);
```

**源码**：[source/std/memory.mgl:19](../../source/std/memory.mgl#L19)。

**完整示例**：[example/samples/std_parse_random_path_memory.mgl](../../example/samples/std_parse_random_path_memory.mgl)。

<a id="api-2"></a>

### Span::iterator（3）

**声明**

```magel
Option<T mut&> iterator(int64 index);
```

**行为**

读取指定迭代位置；无元素时返回 None。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `index` | `int64 index` | 从 0 开始的位置 | 无 |

**返回值**

`Option<T mut&>`。读取指定迭代位置；无元素时返回 None。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.iterator(index);
```

**源码**：[source/std/memory.mgl:23](../../source/std/memory.mgl#L23)。

**完整示例**：[example/samples/std_parse_random_path_memory.mgl](../../example/samples/std_parse_random_path_memory.mgl)。

<a id="api-3"></a>

### Span::len（4）

**声明**

```magel
int64 len();
```

**行为**

返回当前元素数量。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`int64`。返回当前元素数量。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.len();
```

**源码**：[source/std/memory.mgl:27](../../source/std/memory.mgl#L27)。

**完整示例**：[example/samples/std_parse_random_path_memory.mgl](../../example/samples/std_parse_random_path_memory.mgl)。

<a id="api-4"></a>

### Memory::stats（5）

**声明**

```magel
static MemoryStats stats();
```

**行为**

返回当前分配器的活动字节、活动块及峰值统计。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`MemoryStats`。返回当前分配器的活动字节、活动块及峰值统计。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::memory::Memory::stats();
```

**源码**：[source/std/memory.mgl:45](../../source/std/memory.mgl#L45)。

**完整示例**：[example/samples/std_parse_random_path_memory.mgl](../../example/samples/std_parse_random_path_memory.mgl)。

<a id="api-5"></a>

### Arena::create（6）

**声明**

```magel
static Arena create();
```

**行为**

创建独立 arena。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`Arena`。创建独立 arena。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::memory::Arena::create();
```

**源码**：[source/std/memory.mgl:57](../../source/std/memory.mgl#L57)。

**完整示例**：[example/samples/std_parse_random_path_memory.mgl](../../example/samples/std_parse_random_path_memory.mgl)。

<a id="api-6"></a>

### Arena::allocate（7）

**声明**

```magel
Span<char> allocate(int64 bytes);
```

**行为**

在 arena 中分配 bytes 字节，返回 Span<char>。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `bytes` | `int64 bytes` | 输入或输出字节存储 | 无 |

**返回值**

`Span<char>`。在 arena 中分配 bytes 字节，返回 Span<char>。

**错误与边界**

遵循本页“公共约定与错误边界”。
显式前置检查：`bytes >= 0`。

**调用形式**

```magel
obj.allocate(bytes);
```

**源码**：[source/std/memory.mgl:61](../../source/std/memory.mgl#L61)。

**完整示例**：[example/samples/std_parse_random_path_memory.mgl](../../example/samples/std_parse_random_path_memory.mgl)。

<a id="api-7"></a>

### Arena::reset（8）

**声明**

```magel
void reset();
```

**行为**

清空对象的历史状态或分配状态，具体资源规则见模块约定。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

无返回值。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.reset();
```

**源码**：[source/std/memory.mgl:67](../../source/std/memory.mgl#L67)。

**完整示例**：[example/samples/std_parse_random_path_memory.mgl](../../example/samples/std_parse_random_path_memory.mgl)。

<a id="api-8"></a>

### Arena::drop（9）

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

**源码**：[source/std/memory.mgl:68](../../source/std/memory.mgl#L68)。

**完整示例**：[example/samples/std_parse_random_path_memory.mgl](../../example/samples/std_parse_random_path_memory.mgl)。
