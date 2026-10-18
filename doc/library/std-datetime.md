# std::datetime — UTC 日期时间

[返回库目录](README.md) · [全部符号](symbols.md)

## 引入

```magel
import std::datetime;
```

## 公共约定与错误边界

Unix 秒与公历 UTC 日期换算，年份 1..9999，秒 0..59，不表示闰秒。ISO 8601 首版只接受 YYYY-MM-DDTHH:MM:SSZ。纯换算适用于板端；nowUtc 要求宿主时钟。

## 参考代码

完整程序：[example/samples/std_datetime.mgl](../../example/samples/std_datetime.mgl)。

```powershell
magel run example/samples/std_datetime.mgl
```

<details>
<summary>展开完整参考代码</summary>

```magel
import std::datetime;
int64 main(){
    assert(formatUtc(fromUnixSeconds(0).unwrap()).unwrap()=="1970-01-01T00:00:00Z");
    assert(formatUtc(fromUnixSeconds(-1).unwrap()).unwrap()=="1969-12-31T23:59:59Z");
    DateTime leap=parseUtc("2024-02-29T12:34:56Z").unwrap();
    assert(toUnixSeconds(leap).unwrap()==1709210096);
    assert(formatUtc(addDays(leap,1).unwrap()).unwrap()=="2024-03-01T12:34:56Z");
    assert(formatUtc(addSeconds(leap,-1).unwrap()).unwrap()=="2024-02-29T12:34:55Z");
    assert(!parseUtc("1900-02-29T00:00:00Z").isOk() && parseUtc("2000-02-29T00:00:00Z").isOk());
    assert(!parseUtc("2024-01-01T24:00:00Z").isOk() && !parseUtc("2024-01-01T00:00:60Z").isOk());
    assert(!parseUtc("2024-01-01T00:00:00+08:00").isOk());
    assert(toUnixSeconds(parseUtc("0001-01-01T00:00:00Z").unwrap()).unwrap()== -62135596800);
    assert(toUnixSeconds(parseUtc("9999-12-31T23:59:59Z").unwrap()).unwrap()==253402300799);
    assert(!fromUnixSeconds(253402300800).isOk());
    List<int64> inputs1;inputs1.push(-62135596800);inputs1.push(-2208988800);inputs1.push(-86401);inputs1.push(-1);inputs1.push(0);inputs1.push(86400);inputs1.push(1709210096);inputs1.push(253402300799);for(int64 seconds in inputs1){
        assert(toUnixSeconds(fromUnixSeconds(seconds).unwrap()).unwrap()==seconds);
    }
    println("UTC dates, epochs and arithmetic OK");return 0;
}
```

</details>

<details>
<summary>示例输出</summary>

```text
UTC dates, epochs and arithmetic OK
```

</details>

聚合入口的具体操作参见对应子模块；本页类型/方法的调用形式列在下方。

## 类型、枚举、常量与字段

公开字段按源码原样列出；句柄和游标字段用于包装器状态，通常通过方法操作。枚举的括号表示变体载荷。

| 所属类型 | 声明 | 说明 |
| --- | --- | --- |
| 模块 | <a id="symbol-43"></a>`struct DateTime` | UTC 民用时间，使用公历；有效年份为 1 到 9999，不表示闰秒。 |
| DateTime | <a id="symbol-44"></a>`int64 year` | 公历年份。 |
| DateTime | <a id="symbol-45"></a>`int64 month` | 月份，1–12。 |
| DateTime | <a id="symbol-46"></a>`int64 day` | 当月日期，从 1 开始。 |
| DateTime | <a id="symbol-47"></a>`int64 hour` | 小时，0–23。 |
| DateTime | <a id="symbol-48"></a>`int64 minute` | 分钟，0–59。 |
| DateTime | <a id="symbol-49"></a>`int64 second` | 秒，0–59，不表示闰秒。 |
| 模块 | <a id="symbol-50"></a>`enum DateTimeError` | 日期、时间、格式、可表示范围或宿主时钟不可用的错误。 |
| DateTimeError | `InvalidDate, InvalidTime, InvalidFormat, OutOfRange, ClockUnavailable` | 日期、时间、格式、可表示范围或宿主时钟不可用的错误。 变体及载荷见声明。 |

## 函数索引

| 名称 | 声明 |
| --- | --- |
| [isLeapYear](#api-0) | `bool isLeapYear(int64 year)` |
| [daysInMonth](#api-1) | `int64 daysInMonth(int64 year, int64 month)` |
| [toUnixSeconds](#api-2) | `Result<int64,DateTimeError> toUnixSeconds(DateTime value)` |
| [fromUnixSeconds](#api-3) | `Result<DateTime,DateTimeError> fromUnixSeconds(int64 seconds)` |
| [dateDigits](#api-4) | `String dateDigits(int64 value, int64 width)` |
| [formatUtc](#api-5) | `Result<String,DateTimeError> formatUtc(DateTime value)` |
| [parseUtc](#api-6) | `Result<DateTime,DateTimeError> parseUtc(String text)` |
| [addSeconds](#api-7) | `Result<DateTime,DateTimeError> addSeconds(DateTime value, int64 delta)` |
| [addDays](#api-8) | `Result<DateTime,DateTimeError> addDays(DateTime value, int64 days)` |
| [nowUtc](#api-9) | `Result<DateTime,DateTimeError> nowUtc()` |

<a id="api-0"></a>

### isLeapYear（1）

**声明**

```magel
bool isLeapYear(int64 year);
```

**行为**

公历闰年规则。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `year` | `int64 year` | 公历年份。 | 无 |

**返回值**

`bool`。公历闰年规则。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::datetime::isLeapYear(year);
```

**源码**：[source/std/datetime.mgl:7](../../source/std/datetime.mgl#L7)。

**完整示例**：[example/samples/std_datetime.mgl](../../example/samples/std_datetime.mgl)。

<a id="api-1"></a>

### daysInMonth（2）

**声明**

```magel
int64 daysInMonth(int64 year, int64 month);
```

**行为**

返回月份天数；非法月份返回零。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `year` | `int64 year` | 公历年份。 | 无 |
| `month` | `int64 month` | 月份，1 到 12。 | 无 |

**返回值**

`int64`。返回月份天数；非法月份返回零。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::datetime::daysInMonth(year, month);
```

**源码**：[source/std/datetime.mgl:9](../../source/std/datetime.mgl#L9)。

**完整示例**：[example/samples/std_datetime.mgl](../../example/samples/std_datetime.mgl)。

<a id="api-2"></a>

### toUnixSeconds（3）

**声明**

```magel
Result<int64,DateTimeError> toUnixSeconds(DateTime value);
```

**行为**

将 UTC 日期转换为 Unix 秒；不依赖宿主时区。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `value` | `DateTime value` | 输入值或写入的新值 | 无 |

**返回值**

`Result<int64,DateTimeError>`。将 UTC 日期转换为 Unix 秒；不依赖宿主时区。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::datetime::toUnixSeconds(value);
```

**源码**：[source/std/datetime.mgl:16](../../source/std/datetime.mgl#L16)。

**完整示例**：[example/samples/std_datetime.mgl](../../example/samples/std_datetime.mgl)。

<a id="api-3"></a>

### fromUnixSeconds（4）

**声明**

```magel
Result<DateTime,DateTimeError> fromUnixSeconds(int64 seconds);
```

**行为**

将 Unix 秒转换为 UTC 日期，负数表示 1970 年之前。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `seconds` | `int64 seconds` | Unix 秒数。 | 无 |

**返回值**

`Result<DateTime,DateTimeError>`。将 Unix 秒转换为 UTC 日期，负数表示 1970 年之前。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::datetime::fromUnixSeconds(seconds);
```

**源码**：[source/std/datetime.mgl:31](../../source/std/datetime.mgl#L31)。

**完整示例**：[example/samples/std_datetime.mgl](../../example/samples/std_datetime.mgl)。

<a id="api-4"></a>

### dateDigits（5）

**声明**

```magel
String dateDigits(int64 value, int64 width);
```

**行为**

输出固定位数的非负十进制字段。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `value` | `int64 value` | 输入值或写入的新值 | 无 |
| `width` | `int64 width` | 目标字节宽度或定宽字段大小 | 无 |

**返回值**

`String`。输出固定位数的非负十进制字段。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::datetime::dateDigits(value, width);
```

**源码**：[source/std/datetime.mgl:45](../../source/std/datetime.mgl#L45)。

**完整示例**：[example/samples/std_datetime.mgl](../../example/samples/std_datetime.mgl)。

<a id="api-5"></a>

### formatUtc（6）

**声明**

```magel
Result<String,DateTimeError> formatUtc(DateTime value);
```

**行为**

输出严格的 YYYY-MM-DDTHH:MM:SSZ 文本。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `value` | `DateTime value` | 输入值或写入的新值 | 无 |

**返回值**

`Result<String,DateTimeError>`。输出严格的 YYYY-MM-DDTHH:MM:SSZ 文本。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::datetime::formatUtc(value);
```

**源码**：[source/std/datetime.mgl:51](../../source/std/datetime.mgl#L51)。

**完整示例**：[example/samples/std_datetime.mgl](../../example/samples/std_datetime.mgl)。

<a id="api-6"></a>

### parseUtc（7）

**声明**

```magel
Result<DateTime,DateTimeError> parseUtc(String text);
```

**行为**

解析 UTC ISO 8601 秒精度文本，不接受时区偏移或隐式本地时区。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `text` | `String text` | 输入文本，位置与长度通常按字节 | 无 |

**返回值**

`Result<DateTime,DateTimeError>`。解析 UTC ISO 8601 秒精度文本，不接受时区偏移或隐式本地时区。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::datetime::parseUtc(text);
```

**源码**：[source/std/datetime.mgl:57](../../source/std/datetime.mgl#L57)。

**完整示例**：[example/samples/std_datetime.mgl](../../example/samples/std_datetime.mgl)。

<a id="api-7"></a>

### addSeconds（8）

**声明**

```magel
Result<DateTime,DateTimeError> addSeconds(DateTime value, int64 delta);
```

**行为**

在 UTC 时间上增加秒数，越界返回错误，不发生整数回绕。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `value` | `DateTime value` | 输入值或写入的新值 | 无 |
| `delta` | `int64 delta` | 时间偏移秒数。 | 无 |

**返回值**

`Result<DateTime,DateTimeError>`。在 UTC 时间上增加秒数，越界返回错误，不发生整数回绕。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::datetime::addSeconds(value, delta);
```

**源码**：[source/std/datetime.mgl:77](../../source/std/datetime.mgl#L77)。

**完整示例**：[example/samples/std_datetime.mgl](../../example/samples/std_datetime.mgl)。

<a id="api-8"></a>

### addDays（9）

**声明**

```magel
Result<DateTime,DateTimeError> addDays(DateTime value, int64 days);
```

**行为**

增加固定 86400 秒的一天，适用于 UTC。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `value` | `DateTime value` | 输入值或写入的新值 | 无 |
| `days` | `int64 days` | UTC 日期偏移天数。 | 无 |

**返回值**

`Result<DateTime,DateTimeError>`。增加固定 86400 秒的一天，适用于 UTC。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::datetime::addDays(value, days);
```

**源码**：[source/std/datetime.mgl:83](../../source/std/datetime.mgl#L83)。

**完整示例**：[example/samples/std_datetime.mgl](../../example/samples/std_datetime.mgl)。

<a id="api-9"></a>

### nowUtc（10）

**声明**

```magel
Result<DateTime,DateTimeError> nowUtc();
```

**行为**

读取宿主当前 UTC 时间；无时钟宿主应报告不可用。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`Result<DateTime,DateTimeError>`。读取宿主当前 UTC 时间；无时钟宿主应报告不可用。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::datetime::nowUtc();
```

**源码**：[source/std/datetime.mgl:90](../../source/std/datetime.mgl#L90)。

**完整示例**：[example/samples/std_datetime.mgl](../../example/samples/std_datetime.mgl)。
