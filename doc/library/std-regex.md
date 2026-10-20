# std::regex — 正则表达式

[返回库目录](README.md) · [全部符号](symbols.md)

## 引入

```magel
import std::regex;
```

## 公共约定与错误边界

模式在宿主注册表按文本缓存，compile 失败返回 RegexError::Invalid。find 返回首个匹配的字节下标或 None。capture 的 group=0 表示完整匹配。语法和 replacement 捕获组替换规则采用宿主 std::regex 的默认 ECMAScript 规则。

## 参考代码

完整程序：[example/samples/std_glue_edges.mgl](../../example/samples/std_glue_edges.mgl)。

```powershell
magel run example/samples/std_glue_edges.mgl
```

<details>
<summary>展开完整参考代码</summary>

```magel
import std::text;
import std::math;
int64 main() {
    List<String> parts = std::text::Text::split("::甲::::乙::", "::");
    println("{} [{}]", parts.len(), std::text::Text::join(parts, "|"));
    List<String> empty = std::text::Text::split("", ",");
    List<String> whole = std::text::Text::split("甲乙", "");
    List<String> none;
    println("{} [{}] [{}]", empty.len(), whole[0], std::text::Text::join(none, ","));
    parts[1] = "changed";
    println("[{}]", std::text::Text::join(parts, "|"));
    float64 notNumber = std::math::sqrt(-1.0);
    println("{} {} {}", std::math::min(notNumber, 2.0), std::math::isNaN(std::math::max(2.0, notNumber)), std::math::isNaN(std::math::clamp(notNumber, 0.0, 1.0)));
    int64 smallest = (int64)((uint64)1 << 63);
    println("{} {} {} {}", std::math::abs(smallest) == smallest, std::math::min(4, 3), std::math::max(-4.0, -3.0), std::math::clamp(-4, -3, 2));
    return 0;
}
```

</details>

<details>
<summary>示例输出</summary>

```text
5 [|甲||乙|]
1 [甲乙] []
[|changed||乙|]
2 1 1
1 3 -3 -3
```

</details>

聚合入口的具体操作参见对应子模块；本页类型/方法的调用形式列在下方。

## 类型、枚举、常量与字段

公开字段按源码原样列出；句柄和游标字段用于包装器状态，通常通过方法操作。枚举的括号表示变体载荷。

| 所属类型 | 声明 | 说明 |
| --- | --- | --- |
| 模块 | <a id="symbol-433"></a>`enum RegexError` | 正则表达式在宿主注册表里按模式缓存；Magel 值只保存整数句柄，Result 的载荷不持有堆对象。 |
| RegexError | `Invalid` | 正则表达式在宿主注册表里按模式缓存；Magel 值只保存整数句柄，Result 的载荷不持有堆对象。 变体及载荷见声明。 |
| 模块 | <a id="symbol-434"></a>`struct Regex` | 拥有已编译正则表达式句柄的资源包装。 |
| Regex | <a id="symbol-435"></a>`int64 handle` | 宿主资源句柄；应通过所属类型的方法操作。 |

## 函数索引

| 名称 | 声明 |
| --- | --- |
| [Regex::compile](#api-0) | `static Result<Regex, RegexError> compile(std::lang::String pattern)` |
| [Regex::fullMatch](#api-1) | `bool fullMatch(std::lang::String text)` |
| [Regex::contains](#api-2) | `bool contains(std::lang::String text)` |
| [Regex::find](#api-3) | `Option<int64> find(std::lang::String text)` |
| [Regex::capture](#api-4) | `std::lang::String capture(std::lang::String text, int64 group)` |
| [Regex::replaceFirst](#api-5) | `std::lang::String replaceFirst(std::lang::String text, std::lang::String replacement)` |
| [Regex::replaceAll](#api-6) | `std::lang::String replaceAll(std::lang::String text, std::lang::String replacement)` |

<a id="api-0"></a>

### Regex::compile（1）

**声明**

```magel
static Result<Regex, RegexError> compile(std::lang::String pattern);
```

**行为**

编译正则模式；失败返回 Err(Invalid)。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `pattern` | `std::lang::String pattern` | 正则模式 | 无 |

**返回值**

`Result<Regex, RegexError>`。编译正则模式；失败返回 Err(Invalid)。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::regex::Regex::compile(pattern);
```

**源码**：[source/std/regex.mgl:18](../../source/std/regex.mgl#L18)。

**完整示例**：[example/samples/std_glue_edges.mgl](../../example/samples/std_glue_edges.mgl)。

<a id="api-1"></a>

### Regex::fullMatch（2）

**声明**

```magel
bool fullMatch(std::lang::String text);
```

**行为**

判断整个 text 是否匹配模式。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `text` | `std::lang::String text` | 输入文本，位置与长度通常按字节 | 无 |

**返回值**

`bool`。判断整个 text 是否匹配模式。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.fullMatch(text);
```

**源码**：[source/std/regex.mgl:26](../../source/std/regex.mgl#L26)。

**完整示例**：[example/samples/std_glue_edges.mgl](../../example/samples/std_glue_edges.mgl)。

<a id="api-2"></a>

### Regex::contains（3）

**声明**

```magel
bool contains(std::lang::String text);
```

**行为**

判断是否包含目标值或匹配文本。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `text` | `std::lang::String text` | 输入文本，位置与长度通常按字节 | 无 |

**返回值**

`bool`。判断是否包含目标值或匹配文本。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.contains(text);
```

**源码**：[source/std/regex.mgl:27](../../source/std/regex.mgl#L27)。

**完整示例**：[example/samples/std_glue_edges.mgl](../../example/samples/std_glue_edges.mgl)。

<a id="api-3"></a>

### Regex::find（4）

**声明**

```magel
Option<int64> find(std::lang::String text);
```

**行为**

查找首个匹配项，返回下标或可选载荷；具体未找到表示见返回类型及模块约定。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `text` | `std::lang::String text` | 输入文本，位置与长度通常按字节 | 无 |

**返回值**

`Option<int64>`。查找首个匹配项，返回下标或可选载荷；具体未找到表示见返回类型及模块约定。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.find(text);
```

**源码**：[source/std/regex.mgl:28](../../source/std/regex.mgl#L28)。

**完整示例**：[example/samples/std_glue_edges.mgl](../../example/samples/std_glue_edges.mgl)。

<a id="api-4"></a>

### Regex::capture（5）

**声明**

```magel
std::lang::String capture(std::lang::String text, int64 group);
```

**行为**

提取首次匹配的 group 捕获组。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `text` | `std::lang::String text` | 输入文本，位置与长度通常按字节 | 无 |
| `group` | `int64 group` | 捕获组编号，0 为完整匹配 | 无 |

**返回值**

`std::lang::String`。提取首次匹配的 group 捕获组。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.capture(text, group);
```

**源码**：[source/std/regex.mgl:33](../../source/std/regex.mgl#L33)。

**完整示例**：[example/samples/std_glue_edges.mgl](../../example/samples/std_glue_edges.mgl)。

<a id="api-5"></a>

### Regex::replaceFirst（6）

**声明**

```magel
std::lang::String replaceFirst(std::lang::String text, std::lang::String replacement);
```

**行为**

替换首次正则匹配。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `text` | `std::lang::String text` | 输入文本，位置与长度通常按字节 | 无 |
| `replacement` | `std::lang::String replacement` | 正则替换文本 | 无 |

**返回值**

`std::lang::String`。替换首次正则匹配。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.replaceFirst(text, replacement);
```

**源码**：[source/std/regex.mgl:36](../../source/std/regex.mgl#L36)。

**完整示例**：[example/samples/std_glue_edges.mgl](../../example/samples/std_glue_edges.mgl)。

<a id="api-6"></a>

### Regex::replaceAll（7）

**声明**

```magel
std::lang::String replaceAll(std::lang::String text, std::lang::String replacement);
```

**行为**

替换全部正则匹配。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `text` | `std::lang::String text` | 输入文本，位置与长度通常按字节 | 无 |
| `replacement` | `std::lang::String replacement` | 正则替换文本 | 无 |

**返回值**

`std::lang::String`。替换全部正则匹配。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.replaceAll(text, replacement);
```

**源码**：[source/std/regex.mgl:39](../../source/std/regex.mgl#L39)。

**完整示例**：[example/samples/std_glue_edges.mgl](../../example/samples/std_glue_edges.mgl)。
