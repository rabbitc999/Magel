# std::csv — CSV 读写

[返回库目录](README.md) · [全部符号](symbols.md)

## 引入

```magel
import std::csv;
```

## 公共约定与错误边界

支持自定义单字节分隔符、引号转义和字段内换行，保留空字段与空行。错误位置从一开始，列按字节计。输出记录以 LF 结束，空记录必须含至少一个空字段。

## 参考代码

完整程序：[example/samples/std_csv.mgl](../../example/samples/std_csv.mgl)。

```powershell
magel run example/samples/std_csv.mgl
```

<details>
<summary>展开完整参考代码</summary>

```magel
import std::csv;
int64 main(){
    String text="name,note,empty\r\n小兔,\"a,b\",\r\n糖糖,\"line1\nline2 \"\"quoted\"\"\",end\n";
    List<List<String>> rows=parseCsv(text).unwrap();
    assert(rows.len()==3 && rows[1][0]=="小兔" && rows[1][1]=="a,b" && rows[1][2]=="");
    assert(rows[2][1]=="line1\nline2 \"quoted\"");
    List<List<String>> copy=parseCsv(writeCsv(rows).unwrap()).unwrap();
    assert(copy[2][1]==rows[2][1] && copy[1].len()==3);
    assert(parseCsv("a;b\n\"c;d\";e",';').unwrap()[1][0]=="c;d");
    assert(parseCsv("").unwrap().len()==0 && parseCsv("\n").unwrap().len()==1);
    assert(parseCsv("a,").unwrap()[0].len()==2);
    auto failure=parseCsv("a,b\n\"unfinished");assert(!failure.isOk());
    CsvError error=failure.error();assert(error.line==2 && error.column==1);
    assert(!parseCsv("\"x\"tail").isOk() && !parseCsv("x\"y").isOk());
    assert(!parseCsv("a,b",'"').isOk());
    println("CSV quoting, delimiters and positions OK");return 0;
}
```

</details>

<details>
<summary>示例输出</summary>

```text
CSV quoting, delimiters and positions OK
```

</details>

聚合入口的具体操作参见对应子模块；本页类型/方法的调用形式列在下方。

## 类型、枚举、常量与字段

公开字段按源码原样列出；句柄和游标字段用于包装器状态，通常通过方法操作。枚举的括号表示变体载荷。

| 所属类型 | 声明 | 说明 |
| --- | --- | --- |
| 模块 | <a id="symbol-39"></a>`struct CsvError` | CSV 错误位置从一开始，column 是该行的字节列。 |
| CsvError | <a id="symbol-40"></a>`int64 line` | 错误所在行号，从 1 开始。 |
| CsvError | <a id="symbol-41"></a>`int64 column` | 错误所在列号，从 1 开始。 |
| CsvError | <a id="symbol-42"></a>`String message` | 供用户阅读的错误原因。 |

## 函数索引

| 名称 | 声明 |
| --- | --- |
| [csvError](#api-0) | `CsvError csvError(int64 line, int64 column, String message)` |
| [parseCsv](#api-1) | `Result<List<List<String>>,CsvError> parseCsv(String text, char delimiter=',')` |
| [writeCsv](#api-2) | `Result<String,CsvError> writeCsv(List<List<String>> rows, char delimiter=',')` |

<a id="api-0"></a>

### csvError（1）

**声明**

```magel
CsvError csvError(int64 line, int64 column, String message);
```

**行为**

构造包含位置的解析错误。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `line` | `int64 line` | 用于诊断的源码行号 | 无 |
| `column` | `int64 column` | 列下标或列名 | 无 |
| `message` | `String message` | 诊断文本或消息载荷 | 无 |

**返回值**

`CsvError`。构造包含位置的解析错误。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::csv::csvError(line, column, message);
```

**源码**：[source/std/csv.mgl:6](../../source/std/csv.mgl#L6)。

**完整示例**：[example/samples/std_csv.mgl](../../example/samples/std_csv.mgl)。

<a id="api-1"></a>

### parseCsv（2）

**声明**

```magel
Result<List<List<String>>,CsvError> parseCsv(String text, char delimiter=',');
```

**行为**

解析 CSV；支持字段内换行和双引号转义，保留空字段与空行。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `text` | `String text` | 输入文本，位置与长度通常按字节 | 无 |
| `delimiter` | `char delimiter` | 单字节字段分隔符。 | `','` |

**返回值**

`Result<List<List<String>>,CsvError>`。解析 CSV；支持字段内换行和双引号转义，保留空字段与空行。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::csv::parseCsv(text, delimiter);
```

**源码**：[source/std/csv.mgl:10](../../source/std/csv.mgl#L10)。

**完整示例**：[example/samples/std_csv.mgl](../../example/samples/std_csv.mgl)。

<a id="api-2"></a>

### writeCsv（3）

**声明**

```magel
Result<String,CsvError> writeCsv(List<List<String>> rows, char delimiter=',');
```

**行为**

输出 CSV，字段需要时加引号；每条记录以 LF 结束。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `rows` | `List<List<String>> rows` | 矩阵行数 | 无 |
| `delimiter` | `char delimiter` | 单字节字段分隔符。 | `','` |

**返回值**

`Result<String,CsvError>`。输出 CSV，字段需要时加引号；每条记录以 LF 结束。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::csv::writeCsv(rows, delimiter);
```

**源码**：[source/std/csv.mgl:50](../../source/std/csv.mgl#L50)。

**完整示例**：[example/samples/std_csv.mgl](../../example/samples/std_csv.mgl)。
