# std::json — JSON 文档与反射编解码

[返回库目录](README.md) · [全部符号](symbols.md)

## 引入

```chali
import std::json;
```

## 公共约定与错误边界

parse 返回 Result<JsonValue,String>。节点由拥有型 opcode 保存独立子树；普通复制深复制，close 只关闭当前值，已取得的子值和副本继续有效，重复关闭返回 false。asXxx 返回 Option，不符合类型时为 None；asFloat64 可读取整数。含零字节的文本/键使用 asBytes/findBytes/keyBytesAt。kind：0 null、1 bool、2 int、3 float、4 string、5 array、6 object、7 date-time。

## 参考代码

完整程序：[example/samples/std_structured_data.chl](../../example/samples/std_structured_data.chl)。

```powershell
chali run example/samples/std_structured_data.chl
```

<details>
<summary>展开完整参考代码</summary>

```chali
/*
 * std_structured_data.chl —— `std::json` 与 `std::toml` 的读写样例。
 *
 * 本例将 find / keyAt / asString 的结果分别保存，方便逐层检查 Result/Option。
 * 当前语言也支持 root.find("x").unwrap() 等持资源结果的链式访问；编译器在完整
 * 表达式结束后清理中间值。节点拥有独立子树，close 不影响副本及已提取的子值。
 */
import std::json;
import std::toml;

void main() {
    Result<std::json::JsonValue, String> parsed = std::json::parse(
    "{\"name\":\"中文\",\"items\":[1,2,3],\"active\":true,\"nothing\":null,"
    "\"ratio\":1.5,\"min\":-9223372036854775808}");
    std::json::JsonValue root = parsed.unwrap();
    assert(root.isObject() && root.len() == 6);
    Option<String> firstKey = root.keyAt(0);
    assert(firstKey.unwrap() == "name");
    Option<std::json::JsonValue> nameField = root.find("name");
    std::json::JsonValue nameValue = nameField.unwrap();
    Option<String> nameText = nameValue.asString();
    assert(nameText.unwrap() == "中文");

    Option<std::json::JsonValue> itemsField = root.find("items");
    std::json::JsonValue items = itemsField.unwrap();
    int64 sum = 0;
    for (std::json::JsonValue item in items) {
        Option<int64> itemNumber = item.asInt();
        sum = sum + itemNumber.unwrap();
    }
    assert(sum == 6 && items.at(-1).isNone() && items.at(3).isNone());
    Option<std::json::JsonValue> missingField = root.find("missing");
    assert(missingField.isNone());
    Option<std::json::JsonValue> activeField = root.find("active");
    std::json::JsonValue activeValue = activeField.unwrap();
    Option<bool> activeBool = activeValue.asBool();
    assert(activeBool.unwrap());
    Option<std::json::JsonValue> nothingField = root.find("nothing");
    std::json::JsonValue nothingValue = nothingField.unwrap();
    assert(nothingValue.isNull());
    Option<int64> nothingInt = nothingValue.asInt();
    assert(nothingInt.isNone());
    Option<std::json::JsonValue> ratioField = root.find("ratio");
    std::json::JsonValue ratioValue = ratioField.unwrap();
    Option<float64> ratioNumber = ratioValue.asFloat64();
    assert(ratioNumber.unwrap() == 1.5);
    Option<int64> ratioInt = ratioValue.asInt();
    assert(ratioInt.isNone());
    Option<std::json::JsonValue> minField = root.find("min");
    std::json::JsonValue minValue = minField.unwrap();
    Option<int64> minNumber = minValue.asInt();
    assert(format("{}", minNumber.unwrap()) == "-9223372036854775808");

    String rootText = root.dump();
    Result<std::json::JsonValue, String> reparsed = std::json::parse(rootText);
    std::json::JsonValue reloaded = reparsed.unwrap();
    Option<std::json::JsonValue> reloadedName = reloaded.find("name");
    std::json::JsonValue reloadedNameValue = reloadedName.unwrap();
    Option<String> reloadedText = reloadedNameValue.asString();
    assert(reloadedText.unwrap() == "中文");
    assert(reloaded.close());
    assert(root.close() && items.isValid() && !root.close());

    Result<std::json::JsonValue, String> parsedZero =
    std::json::parse("{\"a\\u0000b\":\"x\\u0000y\"}");
    std::json::JsonValue zero = parsedZero.unwrap();
    Option<String> zeroKey = zero.keyAt(0);
    assert(zeroKey.isNone());
    Option<List<uint8>> keyResult = zero.keyBytesAt(0);
    List<uint8> key = keyResult.unwrap();
    assert(key.len() == 3 && key[1] == (uint8)0);
    Option<std::json::JsonValue> zeroTextField = zero.findBytes(key);
    std::json::JsonValue zeroText = zeroTextField.unwrap();
    Option<String> zeroTextString = zeroText.asString();
    assert(zeroTextString.isNone());
    Option<List<uint8>> rawResult = zeroText.asBytes();
    List<uint8> raw = rawResult.unwrap();
    assert(raw.len() == 3 && raw[0] == (uint8)120 && raw[1] == (uint8)0 &&
    raw[2] == (uint8)121);
    assert(zero.close());
    Result<std::json::JsonValue, String> badJson = std::json::parse("{\"x\":}");
    assert(badJson.isErr() && len(badJson.error()) > 0);
    Result<std::json::JsonValue, String> trailing = std::json::parse("[1,]");
    assert(trailing.isErr());

    Result<std::toml::TomlValue, String> parsedConfig = std::toml::parse(
    "title = \"ChaLi\"\nreleased = 1979-05-27T07:32:00Z\n[server]\nport = 8080\n"
    "enabled = true\nvalues = [1, 2, 3]\n[[products]]\nname = \"one\"\n"
    "[[products]]\nname = \"two\"\n");
    std::toml::TomlValue config = parsedConfig.unwrap();
    assert(config.isTable());
    Option<String> titleKey = config.keyAt(0);
    assert(titleKey.unwrap() == "title");
    Option<std::toml::TomlValue> titleField = config.find("title");
    std::toml::TomlValue titleValue = titleField.unwrap();
    Option<String> titleText = titleValue.asString();
    assert(titleText.unwrap() == "ChaLi");
    Option<std::toml::TomlValue> serverField = config.find("server");
    std::toml::TomlValue server = serverField.unwrap();
    Option<std::toml::TomlValue> portField = server.find("port");
    std::toml::TomlValue portValue = portField.unwrap();
    Option<int64> portNumber = portValue.asInt();
    assert(portNumber.unwrap() == 8080);
    Option<std::toml::TomlValue> enabledField = server.find("enabled");
    std::toml::TomlValue enabledValue = enabledField.unwrap();
    Option<bool> enabledBool = enabledValue.asBool();
    assert(enabledBool.unwrap());
    Option<std::toml::TomlValue> releasedField = config.find("released");
    std::toml::TomlValue releasedValue = releasedField.unwrap();
    Option<String> releasedText = releasedValue.asDateTime();
    assert(releasedText.unwrap() == "1979-05-27T07:32:00Z");

    Option<std::toml::TomlValue> productsField = config.find("products");
    std::toml::TomlValue products = productsField.unwrap();
    assert(products.len() == 2);
    Option<std::toml::TomlValue> secondProduct = products.at(1);
    std::toml::TomlValue second = secondProduct.unwrap();
    Option<std::toml::TomlValue> secondName = second.find("name");
    std::toml::TomlValue secondNameValue = secondName.unwrap();
    Option<String> secondNameText = secondNameValue.asString();
    assert(secondNameText.unwrap() == "two");
    int64 members = 0;
    for (std::toml::TomlValue item in server) { members = members + 1; }
    assert(members == 3);
    Option<String> missingMember = server.keyAt(3);
    assert(missingMember.isNone());

    String configText = config.dump();
    Result<std::toml::TomlValue, String> reparsedConfig = std::toml::parse(configText);
    std::toml::TomlValue copy = reparsedConfig.unwrap();
    Option<std::toml::TomlValue> copyServer = copy.find("server");
    std::toml::TomlValue copyServerValue = copyServer.unwrap();
    Option<std::toml::TomlValue> copyPort = copyServerValue.find("port");
    std::toml::TomlValue copyPortValue = copyPort.unwrap();
    Option<int64> copyPortNumber = copyPortValue.asInt();
    assert(copyPortNumber.unwrap() == 8080);
    assert(copy.close() && config.close() && server.isValid());
    Result<std::toml::TomlValue, String> duplicate = std::toml::parse("x = 1\nx = 2\n");
    assert(duplicate.isErr() && len(duplicate.error()) > 0);
    {
        int64 i = 0;
        while (i < 50) {

        Result<std::json::JsonValue, String> parsedSmall = std::json::parse("[]");
        std::json::JsonValue small = parsedSmall.unwrap();
        assert(small.close());
        Result<std::toml::TomlValue, String> invalid = std::toml::parse("x = [");
        assert(invalid.isErr());
    
            i = i + 1;
        }
    }
    Result<std::json::JsonValue, String> trailingOk = std::json::parse("true");
    assert(trailingOk.isOk()); // 注册表结束时清理未显式关闭的文档
    println("JSON and TOML parsing OK");

}
```

</details>

<details>
<summary>示例输出</summary>

```text
JSON and TOML parsing OK
```

</details>

聚合入口的具体操作参见对应子模块；本页类型/方法的调用形式列在下方。

## 类型、枚举、常量与字段

公开字段按源码原样列出；句柄和游标字段用于包装器状态，通常通过方法操作。枚举的括号表示变体载荷。

| 所属类型 | 声明 | 说明 |
| --- | --- | --- |
| 模块 | <a id="symbol-215"></a>`struct JsonValue : Iterator<JsonValue>` | 独立拥有 JSON 子树的值，关闭不影响副本或已提取子值。 |

## 函数索引

| 名称 | 声明 |
| --- | --- |
| [serialize](#api-0) | `Result<String,String> serialize<T>(T const& value)` |
| [deserialize](#api-1) | `Result<T,String> deserialize<T>(String text)` |
| [JsonValue::isValid](#api-2) | `bool isValid()` |
| [JsonValue::close](#api-3) | `bool close()` |
| [JsonValue::kind](#api-4) | `int64 kind()` |
| [JsonValue::isNull](#api-5) | `bool isNull()` |
| [JsonValue::isBool](#api-6) | `bool isBool()` |
| [JsonValue::isInt](#api-7) | `bool isInt()` |
| [JsonValue::isFloat](#api-8) | `bool isFloat()` |
| [JsonValue::isString](#api-9) | `bool isString()` |
| [JsonValue::isArray](#api-10) | `bool isArray()` |
| [JsonValue::isObject](#api-11) | `bool isObject()` |
| [JsonValue::isTable](#api-12) | `bool isTable()` |
| [JsonValue::isDateTime](#api-13) | `bool isDateTime()` |
| [JsonValue::len](#api-14) | `int64 len()` |
| [JsonValue::asInt](#api-15) | `Option<int64> asInt()` |
| [JsonValue::asFloat64](#api-16) | `Option<float64> asFloat64()` |
| [JsonValue::asBool](#api-17) | `Option<bool> asBool()` |
| [JsonValue::asString](#api-18) | `Option<std::lang::String> asString()` |
| [JsonValue::asBytes](#api-19) | `Option<std::lang::List<uint8>> asBytes()` |
| [JsonValue::asDateTime](#api-20) | `Option<std::lang::String> asDateTime()` |
| [JsonValue::find](#api-21) | `Option<JsonValue> find(std::lang::String key)` |
| [JsonValue::findBytes](#api-22) | `Option<JsonValue> findBytes(std::lang::List<uint8> key)` |
| [JsonValue::at](#api-23) | `Option<JsonValue> at(int64 index)` |
| [JsonValue::iterator](#api-24) | `Option<JsonValue> iterator(int64 index)` |
| [JsonValue::keyAt](#api-25) | `Option<std::lang::String> keyAt(int64 index)` |
| [JsonValue::keyBytesAt](#api-26) | `Option<std::lang::List<uint8>> keyBytesAt(int64 index)` |
| [JsonValue::dump](#api-27) | `std::lang::String dump()` |
| [parse](#api-28) | `Result<JsonValue, std::lang::String> parse(std::lang::String text)` |

<a id="api-0"></a>

### serialize（1）

**声明**

```chali
Result<String,String> serialize<T>(T const& value);
```

**行为**

按具体反射类型编码独立值，失败返回诊断。

将已暴露给反射的值编码为 JSON，数字和字段类型保持精确。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `value` | `T const& value` | 输入值或写入的新值 | 无 |

**返回值**

`Result<String,String>`。按具体反射类型编码独立值，失败返回诊断。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
std::json::serialize<T>(value);
```

**源码**：[source/std/json.chl:7](../../source/std/json.chl#L7)。

**完整示例**：[example/samples/std_structured_data.chl](../../example/samples/std_structured_data.chl)。

<a id="api-1"></a>

### deserialize（2）

**声明**

```chali
Result<T,String> deserialize<T>(String text);
```

**行为**

按具体反射类型严格校验并恢复独立拥有的值。

按具体类型验证并恢复拥有独立存储的值。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `text` | `String text` | 输入文本，位置与长度通常按字节 | 无 |

**返回值**

`Result<T,String>`。按具体反射类型严格校验并恢复独立拥有的值。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
std::json::deserialize<T>(text);
```

**源码**：[source/std/json.chl:9](../../source/std/json.chl#L9)。

**完整示例**：[example/samples/std_structured_data.chl](../../example/samples/std_structured_data.chl)。

<a id="api-2"></a>

### JsonValue::isValid（3）

**声明**

```chali
bool isValid();
```

**行为**

判断共享文档是否仍有效。

检查当前值是否有效。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`bool`。判断共享文档是否仍有效。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.isValid();
```

**源码**：[source/std/json.chl:45](../../source/std/json.chl#L45)。

**完整示例**：[example/samples/std_structured_data.chl](../../example/samples/std_structured_data.chl)。

<a id="api-3"></a>

### JsonValue::close（4）

**声明**

```chali
bool close();
```

**行为**

关闭底层资源；文档节点会使同文档所有视图失效。

关闭当前值；副本与已提取的子值继续有效，重复关闭返回 false。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`bool`。关闭底层资源；文档节点会使同文档所有视图失效。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.close();
```

**源码**：[source/std/json.chl:47](../../source/std/json.chl#L47)。

**完整示例**：[example/samples/std_structured_data.chl](../../example/samples/std_structured_data.chl)。

<a id="api-4"></a>

### JsonValue::kind（5）

**声明**

```chali
int64 kind();
```

**行为**

返回节点类型编号。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`int64`。返回节点类型编号。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.kind();
```

**源码**：[source/std/json.chl:48](../../source/std/json.chl#L48)。

**完整示例**：[example/samples/std_structured_data.chl](../../example/samples/std_structured_data.chl)。

<a id="api-5"></a>

### JsonValue::isNull（6）

**声明**

```chali
bool isNull();
```

**行为**

判断是否为 null/NULL。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`bool`。判断是否为 null/NULL。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.isNull();
```

**源码**：[source/std/json.chl:49](../../source/std/json.chl#L49)。

**完整示例**：[example/samples/std_structured_data.chl](../../example/samples/std_structured_data.chl)。

<a id="api-6"></a>

### JsonValue::isBool（7）

**声明**

```chali
bool isBool();
```

**行为**

判断是否为布尔节点。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`bool`。判断是否为布尔节点。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.isBool();
```

**源码**：[source/std/json.chl:50](../../source/std/json.chl#L50)。

**完整示例**：[example/samples/std_structured_data.chl](../../example/samples/std_structured_data.chl)。

<a id="api-7"></a>

### JsonValue::isInt（8）

**声明**

```chali
bool isInt();
```

**行为**

判断是否为整数节点。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`bool`。判断是否为整数节点。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.isInt();
```

**源码**：[source/std/json.chl:51](../../source/std/json.chl#L51)。

**完整示例**：[example/samples/std_structured_data.chl](../../example/samples/std_structured_data.chl)。

<a id="api-8"></a>

### JsonValue::isFloat（9）

**声明**

```chali
bool isFloat();
```

**行为**

判断是否为浮点节点。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`bool`。判断是否为浮点节点。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.isFloat();
```

**源码**：[source/std/json.chl:52](../../source/std/json.chl#L52)。

**完整示例**：[example/samples/std_structured_data.chl](../../example/samples/std_structured_data.chl)。

<a id="api-9"></a>

### JsonValue::isString（10）

**声明**

```chali
bool isString();
```

**行为**

判断是否为字符串节点。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`bool`。判断是否为字符串节点。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.isString();
```

**源码**：[source/std/json.chl:53](../../source/std/json.chl#L53)。

**完整示例**：[example/samples/std_structured_data.chl](../../example/samples/std_structured_data.chl)。

<a id="api-10"></a>

### JsonValue::isArray（11）

**声明**

```chali
bool isArray();
```

**行为**

判断是否为数组节点。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`bool`。判断是否为数组节点。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.isArray();
```

**源码**：[source/std/json.chl:54](../../source/std/json.chl#L54)。

**完整示例**：[example/samples/std_structured_data.chl](../../example/samples/std_structured_data.chl)。

<a id="api-11"></a>

### JsonValue::isObject（12）

**声明**

```chali
bool isObject();
```

**行为**

判断是否为对象/表节点。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`bool`。判断是否为对象/表节点。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.isObject();
```

**源码**：[source/std/json.chl:55](../../source/std/json.chl#L55)。

**完整示例**：[example/samples/std_structured_data.chl](../../example/samples/std_structured_data.chl)。

<a id="api-12"></a>

### JsonValue::isTable（13）

**声明**

```chali
bool isTable();
```

**行为**

判断是否为表/对象节点。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`bool`。判断是否为表/对象节点。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.isTable();
```

**源码**：[source/std/json.chl:56](../../source/std/json.chl#L56)。

**完整示例**：[example/samples/std_structured_data.chl](../../example/samples/std_structured_data.chl)。

<a id="api-13"></a>

### JsonValue::isDateTime（14）

**声明**

```chali
bool isDateTime();
```

**行为**

判断是否为日期时间节点。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`bool`。判断是否为日期时间节点。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.isDateTime();
```

**源码**：[source/std/json.chl:57](../../source/std/json.chl#L57)。

**完整示例**：[example/samples/std_structured_data.chl](../../example/samples/std_structured_data.chl)。

<a id="api-14"></a>

### JsonValue::len（15）

**声明**

```chali
int64 len();
```

**行为**

返回当前元素数量。

数组元素数或对象成员数，标量为零。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`int64`。返回当前元素数量。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.len();
```

**源码**：[source/std/json.chl:59](../../source/std/json.chl#L59)。

**完整示例**：[example/samples/std_structured_data.chl](../../example/samples/std_structured_data.chl)。

<a id="api-15"></a>

### JsonValue::asInt（16）

**声明**

```chali
Option<int64> asInt();
```

**行为**

提取整数值；具体转换规则见所属类型。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`Option<int64>`。提取整数值；具体转换规则见所属类型。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.asInt();
```

**源码**：[source/std/json.chl:60](../../source/std/json.chl#L60)。

**完整示例**：[example/samples/std_structured_data.chl](../../example/samples/std_structured_data.chl)。

<a id="api-16"></a>

### JsonValue::asFloat64（17）

**声明**

```chali
Option<float64> asFloat64();
```

**行为**

提取浮点值，也接受整数节点。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`Option<float64>`。提取浮点值，也接受整数节点。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.asFloat64();
```

**源码**：[source/std/json.chl:64](../../source/std/json.chl#L64)。

**完整示例**：[example/samples/std_structured_data.chl](../../example/samples/std_structured_data.chl)。

<a id="api-17"></a>

### JsonValue::asBool（18）

**声明**

```chali
Option<bool> asBool();
```

**行为**

以 Option/Result 提取布尔值，类型不符走失败分支。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`Option<bool>`。以 Option/Result 提取布尔值，类型不符走失败分支。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.asBool();
```

**源码**：[source/std/json.chl:68](../../source/std/json.chl#L68)。

**完整示例**：[example/samples/std_structured_data.chl](../../example/samples/std_structured_data.chl)。

<a id="api-18"></a>

### JsonValue::asString（19）

**声明**

```chali
Option<std::lang::String> asString();
```

**行为**

提取不含零字节的字符串；否则 None。

非字符串或含零字节时返回 None；含零字节的文本使用 asBytes。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`Option<std::lang::String>`。提取不含零字节的字符串；否则 None。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.asString();
```

**源码**：[source/std/json.chl:73](../../source/std/json.chl#L73)。

**完整示例**：[example/samples/std_structured_data.chl](../../example/samples/std_structured_data.chl)。

<a id="api-19"></a>

### JsonValue::asBytes（20）

**声明**

```chali
Option<std::lang::List<uint8>> asBytes();
```

**行为**

提取字符串的原始字节，包括零字节。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`Option<std::lang::List<uint8>>`。提取字符串的原始字节，包括零字节。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.asBytes();
```

**源码**：[source/std/json.chl:78](../../source/std/json.chl#L78)。

**完整示例**：[example/samples/std_structured_data.chl](../../example/samples/std_structured_data.chl)。

<a id="api-20"></a>

### JsonValue::asDateTime（21）

**声明**

```chali
Option<std::lang::String> asDateTime();
```

**行为**

提取日期时间的文本表示。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`Option<std::lang::String>`。提取日期时间的文本表示。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.asDateTime();
```

**源码**：[source/std/json.chl:83](../../source/std/json.chl#L83)。

**完整示例**：[example/samples/std_structured_data.chl](../../example/samples/std_structured_data.chl)。

<a id="api-21"></a>

### JsonValue::find（22）

**声明**

```chali
Option<JsonValue> find(std::lang::String key);
```

**行为**

查找首个匹配项，返回下标或可选载荷；具体未找到表示见返回类型及模块约定。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `key` | `std::lang::String key` | 查找键或下标 | 无 |

**返回值**

`Option<JsonValue>`。查找首个匹配项，返回下标或可选载荷；具体未找到表示见返回类型及模块约定。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.find(key);
```

**源码**：[source/std/json.chl:88](../../source/std/json.chl#L88)。

**完整示例**：[example/samples/std_structured_data.chl](../../example/samples/std_structured_data.chl)。

<a id="api-22"></a>

### JsonValue::findBytes（23）

**声明**

```chali
Option<JsonValue> findBytes(std::lang::List<uint8> key);
```

**行为**

按原始字节键查找成员，失败 None。

允许包含零字节的 UTF-8 键名。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `key` | `std::lang::List<uint8> key` | 查找键或下标 | 无 |

**返回值**

`Option<JsonValue>`。按原始字节键查找成员，失败 None。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.findBytes(key);
```

**源码**：[source/std/json.chl:95](../../source/std/json.chl#L95)。

**完整示例**：[example/samples/std_structured_data.chl](../../example/samples/std_structured_data.chl)。

<a id="api-23"></a>

### JsonValue::at（24）

**声明**

```chali
Option<JsonValue> at(int64 index);
```

**行为**

读取从 0 开始的指定位置。

数组按索引读取；对象按插入顺序读取成员值。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `index` | `int64 index` | 从 0 开始的位置 | 无 |

**返回值**

`Option<JsonValue>`。读取从 0 开始的指定位置。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.at(index);
```

**源码**：[source/std/json.chl:102](../../source/std/json.chl#L102)。

**完整示例**：[example/samples/std_structured_data.chl](../../example/samples/std_structured_data.chl)。

<a id="api-24"></a>

### JsonValue::iterator（25）

**声明**

```chali
Option<JsonValue> iterator(int64 index);
```

**行为**

读取指定迭代位置；无元素时返回 None。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `index` | `int64 index` | 从 0 开始的位置 | 无 |

**返回值**

`Option<JsonValue>`。读取指定迭代位置；无元素时返回 None。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.iterator(index);
```

**源码**：[source/std/json.chl:108](../../source/std/json.chl#L108)。

**完整示例**：[example/samples/std_structured_data.chl](../../example/samples/std_structured_data.chl)。

<a id="api-25"></a>

### JsonValue::keyAt（26）

**声明**

```chali
Option<std::lang::String> keyAt(int64 index);
```

**行为**

按遍历位置读取键。

对象成员的键名；越界或键中含零字节时返回 None。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `index` | `int64 index` | 从 0 开始的位置 | 无 |

**返回值**

`Option<std::lang::String>`。按遍历位置读取键。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.keyAt(index);
```

**源码**：[source/std/json.chl:110](../../source/std/json.chl#L110)。

**完整示例**：[example/samples/std_structured_data.chl](../../example/samples/std_structured_data.chl)。

<a id="api-26"></a>

### JsonValue::keyBytesAt（27）

**声明**

```chali
Option<std::lang::List<uint8>> keyBytesAt(int64 index);
```

**行为**

按成员位置读取原始字节键。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `index` | `int64 index` | 从 0 开始的位置 | 无 |

**返回值**

`Option<std::lang::List<uint8>>`。按成员位置读取原始字节键。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.keyBytesAt(index);
```

**源码**：[source/std/json.chl:115](../../source/std/json.chl#L115)。

**完整示例**：[example/samples/std_structured_data.chl](../../example/samples/std_structured_data.chl)。

<a id="api-27"></a>

### JsonValue::dump（28）

**声明**

```chali
std::lang::String dump();
```

**行为**

将节点序列化为文本。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`std::lang::String`。将节点序列化为文本。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
obj.dump();
```

**源码**：[source/std/json.chl:120](../../source/std/json.chl#L120)。

**完整示例**：[example/samples/std_structured_data.chl](../../example/samples/std_structured_data.chl)。

<a id="api-28"></a>

### parse（29）

**声明**

```chali
Result<JsonValue, std::lang::String> parse(std::lang::String text);
```

**行为**

解析JSON 文本；成功返回根节点，失败返回诊断字符串。

解析文本；失败返回包含行列信息的诊断。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `text` | `std::lang::String text` | 输入文本，位置与长度通常按字节 | 无 |

**返回值**

`Result<JsonValue, std::lang::String>`。解析JSON 文本；成功返回根节点，失败返回诊断字符串。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```chali
std::json::parse(text);
```

**源码**：[source/std/json.chl:124](../../source/std/json.chl#L124)。

**完整示例**：[example/samples/std_structured_data.chl](../../example/samples/std_structured_data.chl)。
