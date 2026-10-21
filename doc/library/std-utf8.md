# std::utf8 — UTF-8 与码点

[返回库目录](README.md) · [全部符号](symbols.md)

## 引入

```magel
import std::utf8;
```

## 公共约定与错误边界

RFC 3629 严格校验与码点解码，拒绝过长序列、代理项及超范围值。数量和截取按 Unicode 码点，不能代替字素簇处理。String 不适合保存内嵌零字节。

## 参考代码

完整程序：[example/samples/std_utf8.mgl](../../example/samples/std_utf8.mgl)。

```powershell
magel run example/samples/std_utf8.mgl
```

<details>
<summary>展开完整参考代码</summary>

```magel
import std::utf8;
int64 main(){
    String text="A中文😀";
    List<uint32> points=decodeUtf8(text).unwrap();
    assert(points.len()==4 && points[0]==(uint32)65 && points[3]==(uint32)128512);
    assert(encodeUtf8(points).unwrap()==text && codePointCount(text).unwrap()==4);
    assert(sliceCodePoints(text,1,2).unwrap()=="中文");
    assert(sliceCodePoints(text,4,0).unwrap()=="");
    assert(!sliceCodePoints(text,5,0).isOk() && !sliceCodePoints(text,0,-1).isOk());
    List<uint8> overlong;overlong.push((uint8)192);overlong.push((uint8)175);assert(!decodeUtf8Bytes(overlong).isOk());
    List<uint8> surrogateBytes;surrogateBytes.push((uint8)237);surrogateBytes.push((uint8)160);surrogateBytes.push((uint8)128);assert(!decodeUtf8Bytes(surrogateBytes).isOk());
    List<uint8> tooLarge;tooLarge.push((uint8)244);tooLarge.push((uint8)144);tooLarge.push((uint8)128);tooLarge.push((uint8)128);assert(!decodeUtf8Bytes(tooLarge).isOk());
    List<uint8> truncated;truncated.push((uint8)228);truncated.push((uint8)184);assert(!decodeUtf8Bytes(truncated).isOk());
    List<uint32> zero;zero.push((uint32)0);assert(encodeUtf8Bytes(zero).unwrap()[0]==(uint8)0 && !encodeUtf8(zero).isOk());
    List<uint32> surrogate;surrogate.push((uint32)55296);assert(!encodeUtf8(surrogate).isOk());
    assert(codePointCount("é").unwrap()==2);
    println("UTF-8 validation and code points OK");return 0;
}
```

</details>

<details>
<summary>示例输出</summary>

```text
UTF-8 validation and code points OK
```

</details>

聚合入口的具体操作参见对应子模块；本页类型/方法的调用形式列在下方。

## 类型、枚举、常量与字段

公开字段按源码原样列出；句柄和游标字段用于包装器状态，通常通过方法操作。枚举的括号表示变体载荷。

| 所属类型 | 声明 | 说明 |
| --- | --- | --- |
| 模块 | <a id="symbol-526"></a>`enum Utf8Error` | 非法 UTF-8 的首个字节位置，或非法码点的位置。 |
| Utf8Error | `InvalidSequence(int64 offset), InvalidCodePoint(int64 index), InvalidRange, ZeroByteText` | 非法 UTF-8 的首个字节位置，或非法码点的位置。 变体及载荷见声明。 |

## 函数索引

| 名称 | 声明 |
| --- | --- |
| [decodeUtf8Bytes](#api-0) | `Result<List<uint32>,Utf8Error> decodeUtf8Bytes(List<uint8> text)` |
| [decodeUtf8](#api-1) | `Result<List<uint32>,Utf8Error> decodeUtf8(String text)` |
| [isValidUtf8](#api-2) | `bool isValidUtf8(String text)` |
| [encodeUtf8Bytes](#api-3) | `Result<List<uint8>,Utf8Error> encodeUtf8Bytes(List<uint32> points)` |
| [encodeUtf8](#api-4) | `Result<String,Utf8Error> encodeUtf8(List<uint32> points)` |
| [codePointCount](#api-5) | `Result<int64,Utf8Error> codePointCount(String text)` |
| [sliceCodePoints](#api-6) | `Result<String,Utf8Error> sliceCodePoints(String text, int64 start, int64 count)` |

<a id="api-0"></a>

### decodeUtf8Bytes（1）

**声明**

```magel
Result<List<uint32>,Utf8Error> decodeUtf8Bytes(List<uint8> text);
```

**行为**

按 RFC 3629 严格解码；拒绝过长序列、代理项、截断和超范围码点。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `text` | `List<uint8> text` | 输入文本，位置与长度通常按字节 | 无 |

**返回值**

`Result<List<uint32>,Utf8Error>`。按 RFC 3629 严格解码；拒绝过长序列、代理项、截断和超范围码点。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::utf8::decodeUtf8Bytes(text);
```

**源码**：[source/std/utf8.mgl:7](../../source/std/utf8.mgl#L7)。

**完整示例**：[example/samples/std_utf8.mgl](../../example/samples/std_utf8.mgl)。

<a id="api-1"></a>

### decodeUtf8（2）

**声明**

```magel
Result<List<uint32>,Utf8Error> decodeUtf8(String text);
```

**行为**

将字符串按字节复制后严格解码。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `text` | `String text` | 输入文本，位置与长度通常按字节 | 无 |

**返回值**

`Result<List<uint32>,Utf8Error>`。将字符串按字节复制后严格解码。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::utf8::decodeUtf8(text);
```

**源码**：[source/std/utf8.mgl:40](../../source/std/utf8.mgl#L40)。

**完整示例**：[example/samples/std_utf8.mgl](../../example/samples/std_utf8.mgl)。

<a id="api-2"></a>

### isValidUtf8（3）

**声明**

```magel
bool isValidUtf8(String text);
```

**行为**

校验整个字符串是否为合法 UTF-8。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `text` | `String text` | 输入文本，位置与长度通常按字节 | 无 |

**返回值**

`bool`。校验整个字符串是否为合法 UTF-8。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::utf8::isValidUtf8(text);
```

**源码**：[source/std/utf8.mgl:45](../../source/std/utf8.mgl#L45)。

**完整示例**：[example/samples/std_utf8.mgl](../../example/samples/std_utf8.mgl)。

<a id="api-3"></a>

### encodeUtf8Bytes（4）

**声明**

```magel
Result<List<uint8>,Utf8Error> encodeUtf8Bytes(List<uint32> points);
```

**行为**

将 Unicode 标量值编码为 UTF-8；不接受 UTF-16 代理项。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `points` | `List<uint32> points` | Unicode 标量值列表。 | 无 |

**返回值**

`Result<List<uint8>,Utf8Error>`。将 Unicode 标量值编码为 UTF-8；不接受 UTF-16 代理项。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::utf8::encodeUtf8Bytes(points);
```

**源码**：[source/std/utf8.mgl:47](../../source/std/utf8.mgl#L47)。

**完整示例**：[example/samples/std_utf8.mgl](../../example/samples/std_utf8.mgl)。

<a id="api-4"></a>

### encodeUtf8（5）

**声明**

```magel
Result<String,Utf8Error> encodeUtf8(List<uint32> points);
```

**行为**

编码为 String；U+0000 应使用 encodeUtf8Bytes，避免文本被零字节截断。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `points` | `List<uint32> points` | Unicode 标量值列表。 | 无 |

**返回值**

`Result<String,Utf8Error>`。编码为 String；U+0000 应使用 encodeUtf8Bytes，避免文本被零字节截断。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::utf8::encodeUtf8(points);
```

**源码**：[source/std/utf8.mgl:66](../../source/std/utf8.mgl#L66)。

**完整示例**：[example/samples/std_utf8.mgl](../../example/samples/std_utf8.mgl)。

<a id="api-5"></a>

### codePointCount（6）

**声明**

```magel
Result<int64,Utf8Error> codePointCount(String text);
```

**行为**

返回码点数量；码点不是字素簇，组合字符可能占多个码点。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `text` | `String text` | 输入文本，位置与长度通常按字节 | 无 |

**返回值**

`Result<int64,Utf8Error>`。返回码点数量；码点不是字素簇，组合字符可能占多个码点。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::utf8::codePointCount(text);
```

**源码**：[source/std/utf8.mgl:75](../../source/std/utf8.mgl#L75)。

**完整示例**：[example/samples/std_utf8.mgl](../../example/samples/std_utf8.mgl)。

<a id="api-6"></a>

### sliceCodePoints（7）

**声明**

```magel
Result<String,Utf8Error> sliceCodePoints(String text, int64 start, int64 count);
```

**行为**

按零基码点下标截取 count 个码点；先校验完整输入。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `text` | `String text` | 输入文本，位置与长度通常按字节 | 无 |
| `start` | `int64 start` | 起始值 | 无 |
| `count` | `int64 count` | 元素或字节数量，具体单位见行为说明 | 无 |

**返回值**

`Result<String,Utf8Error>`。按零基码点下标截取 count 个码点；先校验完整输入。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::utf8::sliceCodePoints(text, start, count);
```

**源码**：[source/std/utf8.mgl:80](../../source/std/utf8.mgl#L80)。

**完整示例**：[example/samples/std_utf8.mgl](../../example/samples/std_utf8.mgl)。
