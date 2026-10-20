# std::toml — TOML 只读文档

[返回库目录](README.md) · [全部符号](symbols.md)

## 引入

```magel
import std::toml;
```

## 公共约定与错误边界

parse 返回 Result<TomlValue,String>。节点共享文档生命周期，close 会使所有节点失效。表对应 isTable/isObject；时间值使用 isDateTime/asDateTime。asXxx 类型不符返回 None；含零字节的文本使用字节列表接口。kind 编码与 JSON 节点一致。

## 参考代码

完整程序：[example/samples/std_structured_data.mgl](../../example/samples/std_structured_data.mgl)。

```powershell
magel run example/samples/std_structured_data.mgl
```

<details>
<summary>展开完整参考代码</summary>

```magel
/*
 * std_structured_data.mgl —— `std::json` 与 `std::toml` 的读写样例。
 *
 * 本例将 find / keyAt / asString 的结果分别保存，方便逐层检查 Result/Option。
 * 当前语言也支持 root.find("x").unwrap() 等持资源结果的链式访问；编译器在完整
 * 表达式结束后清理中间值。文档节点是共享视图，close 后同文档的节点一起失效。
 */
import std::json;
import std::toml;

int64 main() {
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
    assert(root.close() && !items.isValid() && !root.close());

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
    "title = \"Magel\"\nreleased = 1979-05-27T07:32:00Z\n[server]\nport = 8080\n"
    "enabled = true\nvalues = [1, 2, 3]\n[[products]]\nname = \"one\"\n"
    "[[products]]\nname = \"two\"\n");
    std::toml::TomlValue config = parsedConfig.unwrap();
    assert(config.isTable());
    Option<String> titleKey = config.keyAt(0);
    assert(titleKey.unwrap() == "title");
    Option<std::toml::TomlValue> titleField = config.find("title");
    std::toml::TomlValue titleValue = titleField.unwrap();
    Option<String> titleText = titleValue.asString();
    assert(titleText.unwrap() == "Magel");
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
    assert(copy.close() && config.close() && !server.isValid());
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
    return 0;
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
| 模块 | <a id="symbol-466"></a>`struct TomlValue : Iterator<TomlValue>` | 只读节点视图；同一文档的节点共享生命周期，close 关闭整个文档。 |
| TomlValue | <a id="symbol-467"></a>`int64 document` | 拥有底层解析文档的宿主句柄。 |
| TomlValue | <a id="symbol-468"></a>`int64 node` | 当前值在解析文档中的节点标识。 |

## 函数索引

| 名称 | 声明 |
| --- | --- |
| [TomlValue::isValid](#api-0) | `bool isValid()` |
| [TomlValue::close](#api-1) | `bool close()` |
| [TomlValue::kind](#api-2) | `int64 kind()` |
| [TomlValue::isNull](#api-3) | `bool isNull()` |
| [TomlValue::isBool](#api-4) | `bool isBool()` |
| [TomlValue::isInt](#api-5) | `bool isInt()` |
| [TomlValue::isFloat](#api-6) | `bool isFloat()` |
| [TomlValue::isString](#api-7) | `bool isString()` |
| [TomlValue::isArray](#api-8) | `bool isArray()` |
| [TomlValue::isObject](#api-9) | `bool isObject()` |
| [TomlValue::isTable](#api-10) | `bool isTable()` |
| [TomlValue::isDateTime](#api-11) | `bool isDateTime()` |
| [TomlValue::len](#api-12) | `int64 len()` |
| [TomlValue::asInt](#api-13) | `Option<int64> asInt()` |
| [TomlValue::asFloat64](#api-14) | `Option<float64> asFloat64()` |
| [TomlValue::asBool](#api-15) | `Option<bool> asBool()` |
| [TomlValue::asString](#api-16) | `Option<std::lang::String> asString()` |
| [TomlValue::asBytes](#api-17) | `Option<std::lang::List<uint8>> asBytes()` |
| [TomlValue::asDateTime](#api-18) | `Option<std::lang::String> asDateTime()` |
| [TomlValue::find](#api-19) | `Option<TomlValue> find(std::lang::String key)` |
| [TomlValue::findBytes](#api-20) | `Option<TomlValue> findBytes(std::lang::List<uint8> key)` |
| [TomlValue::at](#api-21) | `Option<TomlValue> at(int64 index)` |
| [TomlValue::iterator](#api-22) | `Option<TomlValue> iterator(int64 index)` |
| [TomlValue::keyAt](#api-23) | `Option<std::lang::String> keyAt(int64 index)` |
| [TomlValue::keyBytesAt](#api-24) | `Option<std::lang::List<uint8>> keyBytesAt(int64 index)` |
| [TomlValue::dump](#api-25) | `std::lang::String dump()` |
| [parse](#api-26) | `Result<TomlValue, std::lang::String> parse(std::lang::String text)` |

<a id="api-0"></a>

### TomlValue::isValid（1）

**声明**

```magel
bool isValid();
```

**行为**

判断共享文档是否仍有效。

检查文档是否仍打开。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`bool`。判断共享文档是否仍有效。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.isValid();
```

**源码**：[source/std/toml.mgl:29](../../source/std/toml.mgl#L29)。

**完整示例**：[example/samples/std_structured_data.mgl](../../example/samples/std_structured_data.mgl)。

<a id="api-1"></a>

### TomlValue::close（2）

**声明**

```magel
bool close();
```

**行为**

关闭底层资源；文档节点会使同文档所有视图失效。

显式释放文档及其所有节点；重复关闭返回 false。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`bool`。关闭底层资源；文档节点会使同文档所有视图失效。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.close();
```

**源码**：[source/std/toml.mgl:31](../../source/std/toml.mgl#L31)。

**完整示例**：[example/samples/std_structured_data.mgl](../../example/samples/std_structured_data.mgl)。

<a id="api-2"></a>

### TomlValue::kind（3）

**声明**

```magel
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

```magel
obj.kind();
```

**源码**：[source/std/toml.mgl:32](../../source/std/toml.mgl#L32)。

**完整示例**：[example/samples/std_structured_data.mgl](../../example/samples/std_structured_data.mgl)。

<a id="api-3"></a>

### TomlValue::isNull（4）

**声明**

```magel
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

```magel
obj.isNull();
```

**源码**：[source/std/toml.mgl:33](../../source/std/toml.mgl#L33)。

**完整示例**：[example/samples/std_structured_data.mgl](../../example/samples/std_structured_data.mgl)。

<a id="api-4"></a>

### TomlValue::isBool（5）

**声明**

```magel
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

```magel
obj.isBool();
```

**源码**：[source/std/toml.mgl:34](../../source/std/toml.mgl#L34)。

**完整示例**：[example/samples/std_structured_data.mgl](../../example/samples/std_structured_data.mgl)。

<a id="api-5"></a>

### TomlValue::isInt（6）

**声明**

```magel
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

```magel
obj.isInt();
```

**源码**：[source/std/toml.mgl:35](../../source/std/toml.mgl#L35)。

**完整示例**：[example/samples/std_structured_data.mgl](../../example/samples/std_structured_data.mgl)。

<a id="api-6"></a>

### TomlValue::isFloat（7）

**声明**

```magel
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

```magel
obj.isFloat();
```

**源码**：[source/std/toml.mgl:36](../../source/std/toml.mgl#L36)。

**完整示例**：[example/samples/std_structured_data.mgl](../../example/samples/std_structured_data.mgl)。

<a id="api-7"></a>

### TomlValue::isString（8）

**声明**

```magel
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

```magel
obj.isString();
```

**源码**：[source/std/toml.mgl:37](../../source/std/toml.mgl#L37)。

**完整示例**：[example/samples/std_structured_data.mgl](../../example/samples/std_structured_data.mgl)。

<a id="api-8"></a>

### TomlValue::isArray（9）

**声明**

```magel
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

```magel
obj.isArray();
```

**源码**：[source/std/toml.mgl:38](../../source/std/toml.mgl#L38)。

**完整示例**：[example/samples/std_structured_data.mgl](../../example/samples/std_structured_data.mgl)。

<a id="api-9"></a>

### TomlValue::isObject（10）

**声明**

```magel
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

```magel
obj.isObject();
```

**源码**：[source/std/toml.mgl:39](../../source/std/toml.mgl#L39)。

**完整示例**：[example/samples/std_structured_data.mgl](../../example/samples/std_structured_data.mgl)。

<a id="api-10"></a>

### TomlValue::isTable（11）

**声明**

```magel
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

```magel
obj.isTable();
```

**源码**：[source/std/toml.mgl:40](../../source/std/toml.mgl#L40)。

**完整示例**：[example/samples/std_structured_data.mgl](../../example/samples/std_structured_data.mgl)。

<a id="api-11"></a>

### TomlValue::isDateTime（12）

**声明**

```magel
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

```magel
obj.isDateTime();
```

**源码**：[source/std/toml.mgl:41](../../source/std/toml.mgl#L41)。

**完整示例**：[example/samples/std_structured_data.mgl](../../example/samples/std_structured_data.mgl)。

<a id="api-12"></a>

### TomlValue::len（13）

**声明**

```magel
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

```magel
obj.len();
```

**源码**：[source/std/toml.mgl:43](../../source/std/toml.mgl#L43)。

**完整示例**：[example/samples/std_structured_data.mgl](../../example/samples/std_structured_data.mgl)。

<a id="api-13"></a>

### TomlValue::asInt（14）

**声明**

```magel
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

```magel
obj.asInt();
```

**源码**：[source/std/toml.mgl:44](../../source/std/toml.mgl#L44)。

**完整示例**：[example/samples/std_structured_data.mgl](../../example/samples/std_structured_data.mgl)。

<a id="api-14"></a>

### TomlValue::asFloat64（15）

**声明**

```magel
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

```magel
obj.asFloat64();
```

**源码**：[source/std/toml.mgl:48](../../source/std/toml.mgl#L48)。

**完整示例**：[example/samples/std_structured_data.mgl](../../example/samples/std_structured_data.mgl)。

<a id="api-15"></a>

### TomlValue::asBool（16）

**声明**

```magel
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

```magel
obj.asBool();
```

**源码**：[source/std/toml.mgl:52](../../source/std/toml.mgl#L52)。

**完整示例**：[example/samples/std_structured_data.mgl](../../example/samples/std_structured_data.mgl)。

<a id="api-16"></a>

### TomlValue::asString（17）

**声明**

```magel
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

```magel
obj.asString();
```

**源码**：[source/std/toml.mgl:57](../../source/std/toml.mgl#L57)。

**完整示例**：[example/samples/std_structured_data.mgl](../../example/samples/std_structured_data.mgl)。

<a id="api-17"></a>

### TomlValue::asBytes（18）

**声明**

```magel
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

```magel
obj.asBytes();
```

**源码**：[source/std/toml.mgl:62](../../source/std/toml.mgl#L62)。

**完整示例**：[example/samples/std_structured_data.mgl](../../example/samples/std_structured_data.mgl)。

<a id="api-18"></a>

### TomlValue::asDateTime（19）

**声明**

```magel
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

```magel
obj.asDateTime();
```

**源码**：[source/std/toml.mgl:67](../../source/std/toml.mgl#L67)。

**完整示例**：[example/samples/std_structured_data.mgl](../../example/samples/std_structured_data.mgl)。

<a id="api-19"></a>

### TomlValue::find（20）

**声明**

```magel
Option<TomlValue> find(std::lang::String key);
```

**行为**

查找首个匹配项，返回下标或可选载荷；具体未找到表示见返回类型及模块约定。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `key` | `std::lang::String key` | 查找键或下标 | 无 |

**返回值**

`Option<TomlValue>`。查找首个匹配项，返回下标或可选载荷；具体未找到表示见返回类型及模块约定。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.find(key);
```

**源码**：[source/std/toml.mgl:72](../../source/std/toml.mgl#L72)。

**完整示例**：[example/samples/std_structured_data.mgl](../../example/samples/std_structured_data.mgl)。

<a id="api-20"></a>

### TomlValue::findBytes（21）

**声明**

```magel
Option<TomlValue> findBytes(std::lang::List<uint8> key);
```

**行为**

按原始字节键查找成员，失败 None。

允许包含零字节的 UTF-8 键名。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `key` | `std::lang::List<uint8> key` | 查找键或下标 | 无 |

**返回值**

`Option<TomlValue>`。按原始字节键查找成员，失败 None。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.findBytes(key);
```

**源码**：[source/std/toml.mgl:79](../../source/std/toml.mgl#L79)。

**完整示例**：[example/samples/std_structured_data.mgl](../../example/samples/std_structured_data.mgl)。

<a id="api-21"></a>

### TomlValue::at（22）

**声明**

```magel
Option<TomlValue> at(int64 index);
```

**行为**

读取从 0 开始的指定位置。

数组按索引读取；对象按插入顺序读取成员值。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `index` | `int64 index` | 从 0 开始的位置 | 无 |

**返回值**

`Option<TomlValue>`。读取从 0 开始的指定位置。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.at(index);
```

**源码**：[source/std/toml.mgl:86](../../source/std/toml.mgl#L86)。

**完整示例**：[example/samples/std_structured_data.mgl](../../example/samples/std_structured_data.mgl)。

<a id="api-22"></a>

### TomlValue::iterator（23）

**声明**

```magel
Option<TomlValue> iterator(int64 index);
```

**行为**

读取指定迭代位置；无元素时返回 None。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `index` | `int64 index` | 从 0 开始的位置 | 无 |

**返回值**

`Option<TomlValue>`。读取指定迭代位置；无元素时返回 None。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.iterator(index);
```

**源码**：[source/std/toml.mgl:92](../../source/std/toml.mgl#L92)。

**完整示例**：[example/samples/std_structured_data.mgl](../../example/samples/std_structured_data.mgl)。

<a id="api-23"></a>

### TomlValue::keyAt（24）

**声明**

```magel
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

```magel
obj.keyAt(index);
```

**源码**：[source/std/toml.mgl:94](../../source/std/toml.mgl#L94)。

**完整示例**：[example/samples/std_structured_data.mgl](../../example/samples/std_structured_data.mgl)。

<a id="api-24"></a>

### TomlValue::keyBytesAt（25）

**声明**

```magel
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

```magel
obj.keyBytesAt(index);
```

**源码**：[source/std/toml.mgl:99](../../source/std/toml.mgl#L99)。

**完整示例**：[example/samples/std_structured_data.mgl](../../example/samples/std_structured_data.mgl)。

<a id="api-25"></a>

### TomlValue::dump（26）

**声明**

```magel
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

```magel
obj.dump();
```

**源码**：[source/std/toml.mgl:104](../../source/std/toml.mgl#L104)。

**完整示例**：[example/samples/std_structured_data.mgl](../../example/samples/std_structured_data.mgl)。

<a id="api-26"></a>

### parse（27）

**声明**

```magel
Result<TomlValue, std::lang::String> parse(std::lang::String text);
```

**行为**

解析TOML 文本；成功返回根节点，失败返回诊断字符串。

解析文本；失败返回包含行列信息的诊断。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `text` | `std::lang::String text` | 输入文本，位置与长度通常按字节 | 无 |

**返回值**

`Result<TomlValue, std::lang::String>`。解析TOML 文本；成功返回根节点，失败返回诊断字符串。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::toml::parse(text);
```

**源码**：[source/std/toml.mgl:108](../../source/std/toml.mgl#L108)。

**完整示例**：[example/samples/std_structured_data.mgl](../../example/samples/std_structured_data.mgl)。
