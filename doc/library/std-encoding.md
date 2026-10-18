# std::encoding — 文本编码

[返回库目录](README.md) · [全部符号](symbols.md)

## 引入

```magel
import std::encoding;
```

## 公共约定与错误边界

RFC 4648 的 Base64、Base64URL 与十六进制编解码。输入字节使用 List<uint8>，解码返回 Result；拒绝非法字符、填充与非零尾部位，不忽略空白。

## 参考代码

完整程序：[example/samples/std_encoding.mgl](../../example/samples/std_encoding.mgl)。

```powershell
magel run example/samples/std_encoding.mgl
```

<details>
<summary>展开完整参考代码</summary>

```magel
import std::encoding;
List<uint8> bytesOf(String text){List<uint8> bytes;for(int64 i=0;i<len(text);i=i+1){bytes.push((uint8)text[i]);}return bytes;}
int64 main(){
    List<String> texts;texts.push("");texts.push("f");texts.push("fo");texts.push("foo");texts.push("foob");texts.push("fooba");texts.push("foobar");
    List<String> encoded;encoded.push("");encoded.push("Zg==");encoded.push("Zm8=");encoded.push("Zm9v");encoded.push("Zm9vYg==");encoded.push("Zm9vYmE=");encoded.push("Zm9vYmFy");
    for(int64 i=0;i<texts.len();i=i+1){
        List<uint8> bytes=bytesOf(texts[i]);
        assert(encodeBase64(bytes)==encoded[i]);
        assert(encodeHex(decodeBase64(encoded[i]).unwrap())==encodeHex(bytes));
        assert(encodeHex(decodeBase64Url(encodeBase64Url(bytes)).unwrap())==encodeHex(bytes));
    }
    List<uint8> binary;binary.push((uint8)0);binary.push((uint8)251);binary.push((uint8)255);
    assert(encodeHex(binary)=="00fbff" && encodeHex(binary,true)=="00FBFF");
    assert(encodeBase64(binary)=="APv/" && encodeBase64Url(binary)=="APv_");
    assert(decodeHex("00FBff").unwrap()[2]==(uint8)255);
    assert(!decodeHex("0").isOk() && !decodeHex("xz").isOk());
    List<String> inputs1;inputs1.push("A");inputs1.push("A===");inputs1.push("Zg=");inputs1.push("Z=g=");inputs1.push("Zh==");inputs1.push("Zm9=");inputs1.push("Zg==\n");inputs1.push("====");for(String invalid in inputs1){assert(!decodeBase64(invalid).isOk());}
    assert(!decodeBase64Url("APv/").isOk() && !decodeBase64("APv_").isOk());
    println("encoding RFC vectors OK");return 0;
}
```

</details>

<details>
<summary>示例输出</summary>

```text
encoding RFC vectors OK
```

</details>

聚合入口的具体操作参见对应子模块；本页类型/方法的调用形式列在下方。

## 类型、枚举、常量与字段

公开字段按源码原样列出；句柄和游标字段用于包装器状态，通常通过方法操作。枚举的括号表示变体载荷。

| 所属类型 | 声明 | 说明 |
| --- | --- | --- |
| 模块 | <a id="symbol-51"></a>`enum EncodingError` | 编码错误包含首个非法输入的字节位置；位置从零开始。 |
| EncodingError | `InvalidLength, InvalidCharacter(int64 offset), InvalidPadding, NonCanonical` | 编码错误包含首个非法输入的字节位置；位置从零开始。 变体及载荷见声明。 |

## 函数索引

| 名称 | 声明 |
| --- | --- |
| [encodeHex](#api-0) | `String encodeHex(List<uint8> bytes, bool uppercase=false)` |
| [hexDigit](#api-1) | `int64 hexDigit(char value)` |
| [decodeHex](#api-2) | `Result<List<uint8>,EncodingError> decodeHex(String text)` |
| [encodeBase64](#api-3) | `String encodeBase64(List<uint8> bytes, bool url=false, bool padding=true)` |
| [encodeBase64Url](#api-4) | `String encodeBase64Url(List<uint8> bytes, bool padding=false)` |
| [decodeBase64](#api-5) | `Result<List<uint8>,EncodingError> decodeBase64(String text, bool url=false)` |
| [decodeBase64Url](#api-6) | `Result<List<uint8>,EncodingError> decodeBase64Url(String text)` |

<a id="api-0"></a>

### encodeHex（1）

**声明**

```magel
String encodeHex(List<uint8> bytes, bool uppercase=false);
```

**行为**

按 RFC 4648 输出十六进制文本。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `bytes` | `List<uint8> bytes` | 输入或输出字节存储 | 无 |
| `uppercase` | `bool uppercase` | 是否输出大写十六进制。 | `false` |

**返回值**

`String`。按 RFC 4648 输出十六进制文本。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::encoding::encodeHex(bytes, uppercase);
```

**源码**：[source/std/encoding.mgl:7](../../source/std/encoding.mgl#L7)。

**完整示例**：[example/samples/std_encoding.mgl](../../example/samples/std_encoding.mgl)。

<a id="api-1"></a>

### hexDigit（2）

**声明**

```magel
int64 hexDigit(char value);
```

**行为**

将一个十六进制字符转换为数值；非法字符返回 -1。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `value` | `char value` | 输入值或写入的新值 | 无 |

**返回值**

`int64`。将一个十六进制字符转换为数值；非法字符返回 -1。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::encoding::hexDigit(value);
```

**源码**：[source/std/encoding.mgl:17](../../source/std/encoding.mgl#L17)。

**完整示例**：[example/samples/std_encoding.mgl](../../example/samples/std_encoding.mgl)。

<a id="api-2"></a>

### decodeHex（3）

**声明**

```magel
Result<List<uint8>,EncodingError> decodeHex(String text);
```

**行为**

严格解码偶数长度的十六进制文本，不忽略空白。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `text` | `String text` | 输入文本，位置与长度通常按字节 | 无 |

**返回值**

`Result<List<uint8>,EncodingError>`。严格解码偶数长度的十六进制文本，不忽略空白。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::encoding::decodeHex(text);
```

**源码**：[source/std/encoding.mgl:24](../../source/std/encoding.mgl#L24)。

**完整示例**：[example/samples/std_encoding.mgl](../../example/samples/std_encoding.mgl)。

<a id="api-3"></a>

### encodeBase64（4）

**声明**

```magel
String encodeBase64(List<uint8> bytes, bool url=false, bool padding=true);
```

**行为**

Base64 编码；URL 字母表可选，padding 控制尾部等号。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `bytes` | `List<uint8> bytes` | 输入或输出字节存储 | 无 |
| `url` | `bool url` | HTTP URL，仅支持 http:// | `false` |
| `padding` | `bool padding` | 是否输出 Base64 填充。 | `true` |

**返回值**

`String`。Base64 编码；URL 字母表可选，padding 控制尾部等号。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::encoding::encodeBase64(bytes, url, padding);
```

**源码**：[source/std/encoding.mgl:37](../../source/std/encoding.mgl#L37)。

**完整示例**：[example/samples/std_encoding.mgl](../../example/samples/std_encoding.mgl)。

<a id="api-4"></a>

### encodeBase64Url（5）

**声明**

```magel
String encodeBase64Url(List<uint8> bytes, bool padding=false);
```

**行为**

URL 安全 Base64，默认省略填充。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `bytes` | `List<uint8> bytes` | 输入或输出字节存储 | 无 |
| `padding` | `bool padding` | 是否输出 Base64 填充。 | `false` |

**返回值**

`String`。URL 安全 Base64，默认省略填充。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::encoding::encodeBase64Url(bytes, padding);
```

**源码**：[source/std/encoding.mgl:54](../../source/std/encoding.mgl#L54)。

**完整示例**：[example/samples/std_encoding.mgl](../../example/samples/std_encoding.mgl)。

<a id="api-5"></a>

### decodeBase64（6）

**声明**

```magel
Result<List<uint8>,EncodingError> decodeBase64(String text, bool url=false);
```

**行为**

严格 Base64 解码；接受正确填充或无填充形式，拒绝非零尾部位。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `text` | `String text` | 输入文本，位置与长度通常按字节 | 无 |
| `url` | `bool url` | HTTP URL，仅支持 http:// | `false` |

**返回值**

`Result<List<uint8>,EncodingError>`。严格 Base64 解码；接受正确填充或无填充形式，拒绝非零尾部位。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::encoding::decodeBase64(text, url);
```

**源码**：[source/std/encoding.mgl:56](../../source/std/encoding.mgl#L56)。

**完整示例**：[example/samples/std_encoding.mgl](../../example/samples/std_encoding.mgl)。

<a id="api-6"></a>

### decodeBase64Url（7）

**声明**

```magel
Result<List<uint8>,EncodingError> decodeBase64Url(String text);
```

**行为**

解码 URL 字母表，不接受标准字母表的加号与斜杠。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `text` | `String text` | 输入文本，位置与长度通常按字节 | 无 |

**返回值**

`Result<List<uint8>,EncodingError>`。解码 URL 字母表，不接受标准字母表的加号与斜杠。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::encoding::decodeBase64Url(text);
```

**源码**：[source/std/encoding.mgl:79](../../source/std/encoding.mgl#L79)。

**完整示例**：[example/samples/std_encoding.mgl](../../example/samples/std_encoding.mgl)。
