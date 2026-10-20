# std::parse — 严格文本解析

[返回库目录](README.md) · [全部符号](symbols.md)

## 引入

```magel
import std::parse;
```

## 公共约定与错误边界

返回 Result<T,ParseError>，区分 Empty、Invalid、Overflow。不会静默返回零代替失败。整数的位宽与符号由具体函数确定；调用者处理 Err 后再取值。

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
| 模块 | <a id="symbol-361"></a>`enum ParseError` | 从 lang.mgl 原样迁出：符号名不变，只换了模块归属（现在必须 `import std::parse;` 才能用）。 |
| ParseError | `Empty, Invalid, Overflow` | 从 lang.mgl 原样迁出：符号名不变，只换了模块归属（现在必须 `import std::parse;` 才能用）。 变体及载荷见声明。 |
| 模块 | <a id="symbol-362"></a>`struct Parse` | 文本到数值和布尔值的解析入口。 |

## 函数索引

| 名称 | 声明 |
| --- | --- |
| [Parse::parseInt64](#api-0) | `static Result<int64, ParseError> parseInt64(std::lang::String text)` |
| [Parse::parseUInt64](#api-1) | `static Result<uint64, ParseError> parseUInt64(std::lang::String text)` |
| [Parse::parseFloat64](#api-2) | `static Result<float64, ParseError> parseFloat64(std::lang::String text)` |
| [Parse::parseBool](#api-3) | `static Result<bool, ParseError> parseBool(std::lang::String text)` |

<a id="api-0"></a>

### Parse::parseInt64（1）

**声明**

```magel
static Result<int64, ParseError> parseInt64(std::lang::String text);
```

**行为**

严格解析有符号 64 位十进制整数。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `text` | `std::lang::String text` | 输入文本，位置与长度通常按字节 | 无 |

**返回值**

`Result<int64, ParseError>`。严格解析有符号 64 位十进制整数。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::parse::Parse::parseInt64(text);
```

**源码**：[source/std/parse.mgl:16](../../source/std/parse.mgl#L16)。

**完整示例**：[example/samples/std_parse_random_path_memory.mgl](../../example/samples/std_parse_random_path_memory.mgl)。

<a id="api-1"></a>

### Parse::parseUInt64（2）

**声明**

```magel
static Result<uint64, ParseError> parseUInt64(std::lang::String text);
```

**行为**

严格解析无符号 64 位十进制整数。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `text` | `std::lang::String text` | 输入文本，位置与长度通常按字节 | 无 |

**返回值**

`Result<uint64, ParseError>`。严格解析无符号 64 位十进制整数。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::parse::Parse::parseUInt64(text);
```

**源码**：[source/std/parse.mgl:26](../../source/std/parse.mgl#L26)。

**完整示例**：[example/samples/std_parse_random_path_memory.mgl](../../example/samples/std_parse_random_path_memory.mgl)。

<a id="api-2"></a>

### Parse::parseFloat64（3）

**声明**

```magel
static Result<float64, ParseError> parseFloat64(std::lang::String text);
```

**行为**

解析 64 位浮点数并报告非法输入/溢出。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `text` | `std::lang::String text` | 输入文本，位置与长度通常按字节 | 无 |

**返回值**

`Result<float64, ParseError>`。解析 64 位浮点数并报告非法输入/溢出。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::parse::Parse::parseFloat64(text);
```

**源码**：[source/std/parse.mgl:36](../../source/std/parse.mgl#L36)。

**完整示例**：[example/samples/std_parse_random_path_memory.mgl](../../example/samples/std_parse_random_path_memory.mgl)。

<a id="api-3"></a>

### Parse::parseBool（4）

**声明**

```magel
static Result<bool, ParseError> parseBool(std::lang::String text);
```

**行为**

解析布尔文本并报告非法输入。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `text` | `std::lang::String text` | 输入文本，位置与长度通常按字节 | 无 |

**返回值**

`Result<bool, ParseError>`。解析布尔文本并报告非法输入。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::parse::Parse::parseBool(text);
```

**源码**：[source/std/parse.mgl:46](../../source/std/parse.mgl#L46)。

**完整示例**：[example/samples/std_parse_random_path_memory.mgl](../../example/samples/std_parse_random_path_memory.mgl)。
