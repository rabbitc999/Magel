# std::path — 路径字符串

[返回库目录](README.md) · [全部符号](symbols.md)

## 引入

```magel
import std::path;
```

## 公共约定与错误边界

只进行路径字符串处理，不创建文件、不验证目标存在；分隔符与规范化遵循宿主实现。不要把字符串规范化当作文件权限或符号链接安全校验。

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
| 模块 | <a id="symbol-367"></a>`struct PathString` | 路径文本组合与拆分的静态入口。 |

## 函数索引

| 名称 | 声明 |
| --- | --- |
| [PathString::join](#api-0) | `static std::lang::String join(std::lang::String left, std::lang::String right)` |
| [PathString::fileName](#api-1) | `static std::lang::String fileName(std::lang::String path)` |
| [PathString::extension](#api-2) | `static std::lang::String extension(std::lang::String path)` |
| [PathString::normalize](#api-3) | `static std::lang::String normalize(std::lang::String path)` |

<a id="api-0"></a>

### PathString::join（1）

**声明**

```magel
static std::lang::String join(std::lang::String left, std::lang::String right);
```

**行为**

将 left 与 right 组合为路径字符串。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `left` | `std::lang::String left` | 左操作数或区间左端 | 无 |
| `right` | `std::lang::String right` | 右操作数或区间右端 | 无 |

**返回值**

`std::lang::String`。将 left 与 right 组合为路径字符串。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::path::PathString::join(left, right);
```

**源码**：[source/std/path.mgl:137](../../source/std/path.mgl#L137)。

**完整示例**：[example/samples/std_parse_random_path_memory.mgl](../../example/samples/std_parse_random_path_memory.mgl)。

<a id="api-1"></a>

### PathString::fileName（2）

**声明**

```magel
static std::lang::String fileName(std::lang::String path);
```

**行为**

返回路径末尾文件名。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `path` | `std::lang::String path` | 文件/数据库路径 | 无 |

**返回值**

`std::lang::String`。返回路径末尾文件名。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::path::PathString::fileName(path);
```

**源码**：[source/std/path.mgl:138](../../source/std/path.mgl#L138)。

**完整示例**：[example/samples/std_parse_random_path_memory.mgl](../../example/samples/std_parse_random_path_memory.mgl)。

<a id="api-2"></a>

### PathString::extension（3）

**声明**

```magel
static std::lang::String extension(std::lang::String path);
```

**行为**

返回路径的扩展名。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `path` | `std::lang::String path` | 文件/数据库路径 | 无 |

**返回值**

`std::lang::String`。返回路径的扩展名。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::path::PathString::extension(path);
```

**源码**：[source/std/path.mgl:139](../../source/std/path.mgl#L139)。

**完整示例**：[example/samples/std_parse_random_path_memory.mgl](../../example/samples/std_parse_random_path_memory.mgl)。

<a id="api-3"></a>

### PathString::normalize（4）

**声明**

```magel
static std::lang::String normalize(std::lang::String path);
```

**行为**

返回宿主规则下的规范化路径字符串。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `path` | `std::lang::String path` | 文件/数据库路径 | 无 |

**返回值**

`std::lang::String`。返回宿主规则下的规范化路径字符串。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::path::PathString::normalize(path);
```

**源码**：[source/std/path.mgl:140](../../source/std/path.mgl#L140)。

**完整示例**：[example/samples/std_parse_random_path_memory.mgl](../../example/samples/std_parse_random_path_memory.mgl)。
