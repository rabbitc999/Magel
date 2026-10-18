# std::cli — 命令行参数

[返回库目录](README.md) · [全部符号](symbols.md)

## 引入

```magel
import std::cli;
```

## 公共约定与错误边界

长短选项、短选项组合、附着值、位置参数与 -- 分隔。重复选项采用最后一次值，required 必须显式出现。--help/-h 自动识别但不退出或打印，由应用处理。

## 参考代码

完整程序：[example/samples/std_cli.mgl](../../example/samples/std_cli.mgl)。

```powershell
magel run example/samples/std_cli.mgl
```

<details>
<summary>展开完整参考代码</summary>

```magel
import std::cli;
int64 main(){
    CliParser parser;parser.program="demo";parser.description="sample tool";
    CliOption count;count.name="count";count.shortName='n';count.takesValue=true;count.defaultValue="3";count.description="number of items";
    CliOption verbose;verbose.name="verbose";verbose.shortName='v';
    CliOption output;output.name="output";output.shortName='o';output.takesValue=true;output.required=true;
    assert(parser.add(count) && parser.add(verbose) && parser.add(output) && !parser.add(count));
    List<String> args;args.push("-vn12");args.push("--output=result.txt");args.push("input.txt");args.push("--");args.push("-literal");
    CliArguments result=parser.parse(args).unwrap();
    assert(result.getInt("count").unwrap()==12 && result.get("verbose").unwrap()=="true");
    assert(result.get("output").unwrap()=="result.txt" && result.positional.len()==2 && result.positional[1]=="-literal");
    List<String> defaults;defaults.push("-o");defaults.push("out");assert(parser.parse(defaults).unwrap().getInt("count").unwrap()==3);
    List<String> bad;bad.push("--unknown");assert(!parser.parse(bad).isOk());
    List<String> missing;missing.push("-n");assert(!parser.parse(missing).isOk());
    List<String> help;help.push("--help");assert(parser.parse(help).unwrap().helpRequested);
    List<String> required;assert(!parser.parse(required).isOk());
    List<String> invalid;invalid.push("-oout");invalid.push("-nabc");assert(!parser.parse(invalid).unwrap().getInt("count").isOk());
    assert(len(parser.help())>40);
    println("CLI options, defaults and validation OK");return 0;
}
```

</details>

<details>
<summary>示例输出</summary>

```text
CLI options, defaults and validation OK
```

</details>

聚合入口的具体操作参见对应子模块；本页类型/方法的调用形式列在下方。

## 类型、枚举、常量与字段

公开字段按源码原样列出；句柄和游标字段用于包装器状态，通常通过方法操作。枚举的括号表示变体载荷。

| 所属类型 | 声明 | 说明 |
| --- | --- | --- |
| 模块 | <a id="symbol-4"></a>`struct CliError` | 参数错误保留原始参数及可供用户阅读的原因。 |
| CliError | <a id="symbol-5"></a>`String argument` | 导致错误的命令行实参。 |
| CliError | <a id="symbol-6"></a>`String message` | 供用户阅读的错误原因。 |
| 模块 | <a id="symbol-7"></a>`struct CliOption` | 一个选项；shortName 为零时没有短名称，takesValue=false 表示布尔开关。 |
| CliOption | <a id="symbol-8"></a>`String name` | 声明或选项的名称。 |
| CliOption | <a id="symbol-9"></a>`char shortName` | 单字符短选项名称。 |
| CliOption | <a id="symbol-10"></a>`bool takesValue` | 选项是否需要附加值。 |
| CliOption | <a id="symbol-11"></a>`bool required` | 是否要求调用方显式提供该选项。 |
| CliOption | <a id="symbol-12"></a>`String defaultValue` | 没有显式提供值时使用的默认值。 |
| CliOption | <a id="symbol-13"></a>`String description` | 用于帮助或展示的说明文本。 |
| 模块 | <a id="symbol-14"></a>`struct CliArguments` | 解析结果；选项使用长名称作为键，位置参数保持输入顺序。 |
| CliArguments | <a id="symbol-15"></a>`Map<String,String> options` | 已解析的选项名称与值。 |
| CliArguments | <a id="symbol-16"></a>`List<String> positional` | 按输入顺序保存的位置实参。 |
| CliArguments | <a id="symbol-17"></a>`bool helpRequested` | 输入是否请求显示帮助。 |
| 模块 | <a id="symbol-18"></a>`struct CliParser` | 命令行选项定义与解析器；不调用 exit、不打印帮助，由应用决定交互行为。 |
| CliParser | <a id="symbol-19"></a>`String program` | 帮助文本中显示的程序名称。 |
| CliParser | <a id="symbol-20"></a>`String description` | 用于帮助或展示的说明文本。 |
| CliParser | <a id="symbol-21"></a>`List<CliOption> definitions` | 解析器登记的选项定义。 |

## 函数索引

| 名称 | 声明 |
| --- | --- |
| [CliArguments::get](#api-0) | `Option<String> get(String name)` |
| [CliArguments::getInt](#api-1) | `Result<int64,CliError> getInt(String name)` |
| [cliError](#api-2) | `CliError cliError(String argument, String message)` |
| [cliTail](#api-3) | `String cliTail(String text,int64 start)` |
| [CliParser::add](#api-4) | `bool add(CliOption option)` |
| [CliParser::help](#api-5) | `String help()` |
| [CliParser::parse](#api-6) | `Result<CliArguments,CliError> parse(List<String> arguments)` |

<a id="api-0"></a>

### CliArguments::get（1）

**声明**

```magel
Option<String> get(String name);
```

**行为**

读取共享槽当前值的副本。

查询选项值；未定义的名称返回 None。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `name` | `String name` | 名称 | 无 |

**返回值**

`Option<String>`。读取共享槽当前值的副本。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.get(name);
```

**源码**：[source/std/cli.mgl:12](../../source/std/cli.mgl#L12)。

**完整示例**：[example/samples/std_cli.mgl](../../example/samples/std_cli.mgl)。

<a id="api-1"></a>

### CliArguments::getInt（2）

**声明**

```magel
Result<int64,CliError> getInt(String name);
```

**行为**

严格转换整数参数，失败返回带参数名的错误。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `name` | `String name` | 名称 | 无 |

**返回值**

`Result<int64,CliError>`。严格转换整数参数，失败返回带参数名的错误。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.getInt(name);
```

**源码**：[source/std/cli.mgl:17](../../source/std/cli.mgl#L17)。

**完整示例**：[example/samples/std_cli.mgl](../../example/samples/std_cli.mgl)。

<a id="api-2"></a>

### cliError（3）

**声明**

```magel
CliError cliError(String argument, String message);
```

**行为**

生成参数诊断。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `argument` | `String argument` | 原始命令行参数。 | 无 |
| `message` | `String message` | 诊断文本或消息载荷 | 无 |

**返回值**

`CliError`。生成参数诊断。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::cli::cliError(argument, message);
```

**源码**：[source/std/cli.mgl:26](../../source/std/cli.mgl#L26)。

**完整示例**：[example/samples/std_cli.mgl](../../example/samples/std_cli.mgl)。

<a id="api-3"></a>

### cliTail（4）

**声明**

```magel
String cliTail(String text,int64 start);
```

**行为**

从字节位置 start 复制命令行参数后缀。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `text` | `String text` | 输入文本，位置与长度通常按字节 | 无 |
| `start` | `int64 start` | 起始值 | 无 |

**返回值**

`String`。从字节位置 start 复制命令行参数后缀。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::cli::cliTail(text, start);
```

**源码**：[source/std/cli.mgl:28](../../source/std/cli.mgl#L28)。

**完整示例**：[example/samples/std_cli.mgl](../../example/samples/std_cli.mgl)。

<a id="api-4"></a>

### CliParser::add（5）

**声明**

```magel
bool add(CliOption option);
```

**行为**

计算逐项相加或追加元素，具体操作取决于所属类型。

登记选项；拒绝重复名称、保留的 help/h、非法名称及带值开关的歧义。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `option` | `CliOption option` | 待登记的命令行选项。 | 无 |

**返回值**

`bool`。计算逐项相加或追加元素，具体操作取决于所属类型。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.add(option);
```

**源码**：[source/std/cli.mgl:33](../../source/std/cli.mgl#L33)。

**完整示例**：[example/samples/std_cli.mgl](../../example/samples/std_cli.mgl)。

<a id="api-5"></a>

### CliParser::help（6）

**声明**

```magel
String help();
```

**行为**

自动生成使用说明和长短选项帮助。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`String`。自动生成使用说明和长短选项帮助。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.help();
```

**源码**：[source/std/cli.mgl:48](../../source/std/cli.mgl#L48)。

**完整示例**：[example/samples/std_cli.mgl](../../example/samples/std_cli.mgl)。

<a id="api-6"></a>

### CliParser::parse（7）

**声明**

```magel
Result<CliArguments,CliError> parse(List<String> arguments);
```

**行为**

解析长选项、短选项组合、附着值与 --；重复选项采用最后一次的值。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `arguments` | `List<String> arguments` | 回调参数值 | 无 |

**返回值**

`Result<CliArguments,CliError>`。解析长选项、短选项组合、附着值与 --；重复选项采用最后一次的值。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.parse(arguments);
```

**源码**：[source/std/cli.mgl:62](../../source/std/cli.mgl#L62)。

**完整示例**：[example/samples/std_cli.mgl](../../example/samples/std_cli.mgl)。
