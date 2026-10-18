# std::binary — 二进制读取

[返回库目录](README.md) · [全部符号](symbols.md)

## 引入

```magel
import std::binary;
```

## 公共约定与错误边界

BinaryReader 拥有输入的副本，默认小端。位置、长度均按字节；读取成功才推进游标。越界、负长度、元素数量乘法越界由宿主检查并报错。seek/skip 失败返回 false 且保留游标；文本字段在首个零字节停止显示但消费整个字段。

## 参考代码

完整程序：[example/samples/std_binary.mgl](../../example/samples/std_binary.mgl)。

```powershell
magel run example/samples/std_binary.mgl
```

<details>
<summary>展开完整参考代码</summary>

```magel
import std::binary;

/* 固定测试字节由 Python struct.pack 的大小端编码得到。 */
int64 main() {
    uint8 data[97];
    data[0] = (uint8)128;
    data[1] = (uint8)255;
    data[2] = (uint8)0;
    data[3] = (uint8)128;
    data[4] = (uint8)255;
    data[5] = (uint8)255;
    data[6] = (uint8)0;
    data[7] = (uint8)0;
    data[8] = (uint8)0;
    data[9] = (uint8)128;
    data[10] = (uint8)255;
    data[11] = (uint8)255;
    data[12] = (uint8)255;
    data[13] = (uint8)255;
    data[14] = (uint8)0;
    data[15] = (uint8)0;
    data[16] = (uint8)0;
    data[17] = (uint8)0;
    data[18] = (uint8)0;
    data[19] = (uint8)0;
    data[20] = (uint8)0;
    data[21] = (uint8)128;
    data[22] = (uint8)255;
    data[23] = (uint8)255;
    data[24] = (uint8)255;
    data[25] = (uint8)255;
    data[26] = (uint8)255;
    data[27] = (uint8)255;
    data[28] = (uint8)255;
    data[29] = (uint8)255;
    data[30] = (uint8)0;
    data[31] = (uint8)0;
    data[32] = (uint8)192;
    data[33] = (uint8)63;
    data[34] = (uint8)0;
    data[35] = (uint8)0;
    data[36] = (uint8)0;
    data[37] = (uint8)0;
    data[38] = (uint8)0;
    data[39] = (uint8)0;
    data[40] = (uint8)2;
    data[41] = (uint8)192;
    data[42] = (uint8)1;
    data[43] = (uint8)128;
    data[44] = (uint8)255;
    data[45] = (uint8)128;
    data[46] = (uint8)0;
    data[47] = (uint8)255;
    data[48] = (uint8)255;
    data[49] = (uint8)128;
    data[50] = (uint8)0;
    data[51] = (uint8)0;
    data[52] = (uint8)0;
    data[53] = (uint8)255;
    data[54] = (uint8)255;
    data[55] = (uint8)255;
    data[56] = (uint8)255;
    data[57] = (uint8)128;
    data[58] = (uint8)0;
    data[59] = (uint8)0;
    data[60] = (uint8)0;
    data[61] = (uint8)0;
    data[62] = (uint8)0;
    data[63] = (uint8)0;
    data[64] = (uint8)0;
    data[65] = (uint8)255;
    data[66] = (uint8)255;
    data[67] = (uint8)255;
    data[68] = (uint8)255;
    data[69] = (uint8)255;
    data[70] = (uint8)255;
    data[71] = (uint8)255;
    data[72] = (uint8)255;
    data[73] = (uint8)63;
    data[74] = (uint8)192;
    data[75] = (uint8)0;
    data[76] = (uint8)0;
    data[77] = (uint8)192;
    data[78] = (uint8)2;
    data[79] = (uint8)0;
    data[80] = (uint8)0;
    data[81] = (uint8)0;
    data[82] = (uint8)0;
    data[83] = (uint8)0;
    data[84] = (uint8)0;
    data[85] = (uint8)1;
    data[86] = (uint8)97;
    data[87] = (uint8)98;
    data[88] = (uint8)0;
    data[89] = (uint8)88;
    data[90] = (uint8)99;
    data[91] = (uint8)100;
    data[92] = (uint8)69;
    data[93] = (uint8)70;
    data[94] = (uint8)228;
    data[95] = (uint8)184;
    data[96] = (uint8)173;
    std::binary::BinaryReader reader = std::binary::BinaryReader::fromArray(data, 97);
    assert(reader.size() == 97);
    reader.setBigEndian(false);
    assert(reader.readInt8() == (int8)-128);
    assert(reader.readUint8() == (uint8)255);
    assert(reader.readInt16() == (int16)-32768);
    assert(reader.readUint16() == (uint16)65535);
    assert(reader.readInt32() == (int32)-2147483648);
    assert(reader.readUint32() == (uint32)4294967295);
    assert(format("{}", reader.readInt64()) == "-9223372036854775808");
    assert(reader.readUint64() == (uint64)((uint64)0 - (uint64)1));
    assert(reader.readFloat32() == (float32)1.5);
    assert(reader.readFloat64() == (float64)-2.25);
    assert(reader.readBool() == (bool)true);
    reader.setBigEndian(true);
    assert(reader.readInt8() == (int8)-128);
    assert(reader.readUint8() == (uint8)255);
    assert(reader.readInt16() == (int16)-32768);
    assert(reader.readUint16() == (uint16)65535);
    assert(reader.readInt32() == (int32)-2147483648);
    assert(reader.readUint32() == (uint32)4294967295);
    assert(format("{}", reader.readInt64()) == "-9223372036854775808");
    assert(reader.readUint64() == (uint64)((uint64)0 - (uint64)1));
    assert(reader.readFloat32() == (float32)1.5);
    assert(reader.readFloat64() == (float64)-2.25);
    assert(reader.readBool() == (bool)true);
    List<String> names = reader.readStringArray(2, 4);
    assert(names[0] == "ab" && names[1] == "cdEF");
    assert(reader.readString(3) == "中");
    assert(reader.remaining() == 0);
    assert(reader.readString(0) == "");
    assert(!reader.skip(1) && !reader.seek(-1));
    assert(reader.position() == 97);
    assert(reader.seek(0));
    reader.setBigEndian(false);
    List<int8> valuesInt8 = reader.readInt8Array(1);
    assert(valuesInt8[0] == (int8)-128);
    List<uint8> valuesUint8 = reader.readUint8Array(1);
    assert(valuesUint8[0] == (uint8)255);
    List<int16> valuesInt16 = reader.readInt16Array(1);
    assert(valuesInt16[0] == (int16)-32768);
    List<uint16> valuesUint16 = reader.readUint16Array(1);
    assert(valuesUint16[0] == (uint16)65535);
    List<int32> valuesInt32 = reader.readInt32Array(1);
    assert(valuesInt32[0] == (int32)-2147483648);
    List<uint32> valuesUint32 = reader.readUint32Array(1);
    assert(valuesUint32[0] == (uint32)4294967295);
    List<int64> valuesInt64 = reader.readInt64Array(1);
    assert(format("{}", valuesInt64[0]) == "-9223372036854775808");
    List<uint64> valuesUint64 = reader.readUint64Array(1);
    assert(valuesUint64[0] == (uint64)((uint64)0 - (uint64)1));
    List<float32> valuesFloat32 = reader.readFloat32Array(1);
    assert(valuesFloat32[0] == (float32)1.5);
    List<float64> valuesFloat64 = reader.readFloat64Array(1);
    assert(valuesFloat64[0] == (float64)-2.25);
    List<bool> valuesBool = reader.readBoolArray(1);
    assert(valuesBool[0] == (bool)true);
    List<uint8> bytes = reader.readBytes(3);
    std::binary::BinaryReader copied = std::binary::BinaryReader::fromBytes(bytes);
    bytes.clear();
    assert(copied.remaining() == 3 && copied.readUint8() == (uint8)128);
    assert(copied.skip(2));
    List<uint8> empty = copied.readBytes(0);
    assert(empty.len() == 0);
    uint8 dynamic[] = new[2];
    dynamic[0] = (uint8)52; dynamic[1] = (uint8)18;
    std::binary::BinaryReader dynamicReader = std::binary::BinaryReader::fromArray(dynamic);
    assert(dynamicReader.readUint16() == (uint16)4660);
    assert(reader.seek(0));
    List<int16> pair = reader.readInt16Array(2);
    assert(pair[0] == (int16)-128 && pair[1] == (int16)-32768);
    println("binary reader OK");
    return 0;
}
```

</details>

<details>
<summary>示例输出</summary>

```text
binary reader OK
```

</details>

聚合入口的具体操作参见对应子模块；本页类型/方法的调用形式列在下方。

## 类型、枚举、常量与字段

公开字段按源码原样列出；句柄和游标字段用于包装器状态，通常通过方法操作。枚举的括号表示变体载荷。

| 所属类型 | 声明 | 说明 |
| --- | --- | --- |
| 模块 | <a id="symbol-0"></a>`struct BinaryReader : Drop` | 二进制流读取器：拥有输入字节的副本，默认小端。 |
| BinaryReader | <a id="symbol-1"></a>`std::lang::List<uint8> bytes` | 字节数据，保留零字节。 |
| BinaryReader | <a id="symbol-2"></a>`int64 offset` | 下一次读取在 bytes 中的字节位置。 |
| BinaryReader | <a id="symbol-3"></a>`bool bigEndian` | 是否按大端序解析多字节数值。 |

## 函数索引

| 名称 | 声明 |
| --- | --- |
| [BinaryReader::drop](#api-0) | `void drop()` |
| [BinaryReader::fromBytes](#api-1) | `static BinaryReader fromBytes(std::lang::List<uint8> bytes)` |
| [BinaryReader::fromArray](#api-2) | `static BinaryReader fromArray(uint8 const& bytes, int64 count)` |
| [BinaryReader::fromArray](#api-3) | `static BinaryReader fromArray(uint8 bytes[])` |
| [BinaryReader::position](#api-4) | `int64 position()` |
| [BinaryReader::remaining](#api-5) | `int64 remaining()` |
| [BinaryReader::size](#api-6) | `int64 size()` |
| [BinaryReader::setBigEndian](#api-7) | `void setBigEndian(bool enabled)` |
| [BinaryReader::seek](#api-8) | `bool seek(int64 position)` |
| [BinaryReader::skip](#api-9) | `bool skip(int64 count)` |
| [BinaryReader::readInt8](#api-10) | `int8 readInt8()` |
| [BinaryReader::readInt8Array](#api-11) | `std::lang::List<int8> readInt8Array(int64 count)` |
| [BinaryReader::readUint8](#api-12) | `uint8 readUint8()` |
| [BinaryReader::readUint8Array](#api-13) | `std::lang::List<uint8> readUint8Array(int64 count)` |
| [BinaryReader::readInt16](#api-14) | `int16 readInt16()` |
| [BinaryReader::readInt16Array](#api-15) | `std::lang::List<int16> readInt16Array(int64 count)` |
| [BinaryReader::readUint16](#api-16) | `uint16 readUint16()` |
| [BinaryReader::readUint16Array](#api-17) | `std::lang::List<uint16> readUint16Array(int64 count)` |
| [BinaryReader::readInt32](#api-18) | `int32 readInt32()` |
| [BinaryReader::readInt32Array](#api-19) | `std::lang::List<int32> readInt32Array(int64 count)` |
| [BinaryReader::readUint32](#api-20) | `uint32 readUint32()` |
| [BinaryReader::readUint32Array](#api-21) | `std::lang::List<uint32> readUint32Array(int64 count)` |
| [BinaryReader::readInt64](#api-22) | `int64 readInt64()` |
| [BinaryReader::readInt64Array](#api-23) | `std::lang::List<int64> readInt64Array(int64 count)` |
| [BinaryReader::readUint64](#api-24) | `uint64 readUint64()` |
| [BinaryReader::readUint64Array](#api-25) | `std::lang::List<uint64> readUint64Array(int64 count)` |
| [BinaryReader::readFloat32](#api-26) | `float32 readFloat32()` |
| [BinaryReader::readFloat32Array](#api-27) | `std::lang::List<float32> readFloat32Array(int64 count)` |
| [BinaryReader::readFloat64](#api-28) | `float64 readFloat64()` |
| [BinaryReader::readFloat64Array](#api-29) | `std::lang::List<float64> readFloat64Array(int64 count)` |
| [BinaryReader::readBool](#api-30) | `bool readBool()` |
| [BinaryReader::readBoolArray](#api-31) | `std::lang::List<bool> readBoolArray(int64 count)` |
| [BinaryReader::readBytes](#api-32) | `std::lang::List<uint8> readBytes(int64 count)` |
| [BinaryReader::readString](#api-33) | `std::lang::String readString(int64 length)` |
| [BinaryReader::readStringArray](#api-34) | `std::lang::List<std::lang::String> readStringArray(int64 count, int64 width)` |

<a id="api-0"></a>

### BinaryReader::drop（1）

**声明**

```magel
void drop();
```

**行为**

释放对象拥有的资源，调用后遵守本模块失效规则。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

无返回值。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.drop();
```

**源码**：[source/std/binary.mgl:16](../../source/std/binary.mgl#L16)。

**完整示例**：[example/samples/std_binary.mgl](../../example/samples/std_binary.mgl)。

<a id="api-1"></a>

### BinaryReader::fromBytes（2）

**声明**

```magel
static BinaryReader fromBytes(std::lang::List<uint8> bytes);
```

**行为**

复制 bytes 为读取器自有存储。

从字节列表复制；不依赖原列表的生命周期。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `bytes` | `std::lang::List<uint8> bytes` | 输入或输出字节存储 | 无 |

**返回值**

`BinaryReader`。复制 bytes 为读取器自有存储。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::binary::BinaryReader::fromBytes(bytes);
```

**源码**：[source/std/binary.mgl:18](../../source/std/binary.mgl#L18)。

**完整示例**：[example/samples/std_binary.mgl](../../example/samples/std_binary.mgl)。

<a id="api-2"></a>

### BinaryReader::fromArray（3）

**声明**

```magel
static BinaryReader fromArray(uint8 const& bytes, int64 count);
```

**行为**

从连续数组复制元素；带 count 的重载由调用者保证有效范围。

从固定数组或指针复制 count 个字节。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `bytes` | `uint8 const& bytes` | 输入或输出字节存储 | 无 |
| `count` | `int64 count` | 元素或字节数量，具体单位见行为说明 | 无 |

**返回值**

`BinaryReader`。从连续数组复制元素；带 count 的重载由调用者保证有效范围。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::binary::BinaryReader::fromArray(bytes, count);
```

**源码**：[source/std/binary.mgl:26](../../source/std/binary.mgl#L26)。

**完整示例**：[example/samples/std_binary.mgl](../../example/samples/std_binary.mgl)。

<a id="api-3"></a>

### BinaryReader::fromArray（4）

**声明**

```magel
static BinaryReader fromArray(uint8 bytes[]);
```

**行为**

从连续数组复制元素；带 count 的重载由调用者保证有效范围。

从运行时数组复制全部字节。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `bytes` | `uint8 bytes[]` | 输入或输出字节存储 | 无 |

**返回值**

`BinaryReader`。从连续数组复制元素；带 count 的重载由调用者保证有效范围。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::binary::BinaryReader::fromArray(bytes);
```

**源码**：[source/std/binary.mgl:34](../../source/std/binary.mgl#L34)。

**完整示例**：[example/samples/std_binary.mgl](../../example/samples/std_binary.mgl)。

<a id="api-4"></a>

### BinaryReader::position（5）

**声明**

```magel
int64 position();
```

**行为**

返回当前字节游标。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`int64`。返回当前字节游标。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.position();
```

**源码**：[source/std/binary.mgl:35](../../source/std/binary.mgl#L35)。

**完整示例**：[example/samples/std_binary.mgl](../../example/samples/std_binary.mgl)。

<a id="api-5"></a>

### BinaryReader::remaining（6）

**声明**

```magel
int64 remaining();
```

**行为**

返回游标后尚未消费的字节数。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`int64`。返回游标后尚未消费的字节数。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.remaining();
```

**源码**：[source/std/binary.mgl:36](../../source/std/binary.mgl#L36)。

**完整示例**：[example/samples/std_binary.mgl](../../example/samples/std_binary.mgl)。

<a id="api-6"></a>

### BinaryReader::size（7）

**声明**

```magel
int64 size();
```

**行为**

返回存储的总长度。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`int64`。返回存储的总长度。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.size();
```

**源码**：[source/std/binary.mgl:37](../../source/std/binary.mgl#L37)。

**完整示例**：[example/samples/std_binary.mgl](../../example/samples/std_binary.mgl)。

<a id="api-7"></a>

### BinaryReader::setBigEndian（8）

**声明**

```magel
void setBigEndian(bool enabled);
```

**行为**

设置后续读取的大端/小端顺序。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `enabled` | `bool enabled` | 是否启用该选项 | 无 |

**返回值**

无返回值。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.setBigEndian(enabled);
```

**源码**：[source/std/binary.mgl:38](../../source/std/binary.mgl#L38)。

**完整示例**：[example/samples/std_binary.mgl](../../example/samples/std_binary.mgl)。

<a id="api-8"></a>

### BinaryReader::seek（9）

**声明**

```magel
bool seek(int64 position);
```

**行为**

绝对定位，失败返回 false 并保留游标。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `position` | `int64 position` | 目标绝对位置 | 无 |

**返回值**

`bool`。绝对定位，失败返回 false 并保留游标。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.seek(position);
```

**源码**：[source/std/binary.mgl:40](../../source/std/binary.mgl#L40)。

**完整示例**：[example/samples/std_binary.mgl](../../example/samples/std_binary.mgl)。

<a id="api-9"></a>

### BinaryReader::skip（10）

**声明**

```magel
bool skip(int64 count);
```

**行为**

前进 count 字节；失败返回 false 且游标不变。

跳过字节，失败返回 false 并保留游标。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `count` | `int64 count` | 元素或字节数量，具体单位见行为说明 | 无 |

**返回值**

`bool`。前进 count 字节；失败返回 false 且游标不变。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.skip(count);
```

**源码**：[source/std/binary.mgl:46](../../source/std/binary.mgl#L46)。

**完整示例**：[example/samples/std_binary.mgl](../../example/samples/std_binary.mgl)。

<a id="api-10"></a>

### BinaryReader::readInt8（11）

**声明**

```magel
int8 readInt8();
```

**行为**

读取一个 int8，成功后前进 1 字节。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`int8`。读取一个 int8，成功后前进 1 字节。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.readInt8();
```

**源码**：[source/std/binary.mgl:52](../../source/std/binary.mgl#L52)。

**完整示例**：[example/samples/std_binary.mgl](../../example/samples/std_binary.mgl)。

<a id="api-11"></a>

### BinaryReader::readInt8Array（12）

**声明**

```magel
std::lang::List<int8> readInt8Array(int64 count);
```

**行为**

读取 count 个 int8，返回独立列表。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `count` | `int64 count` | 元素或字节数量，具体单位见行为说明 | 无 |

**返回值**

`std::lang::List<int8>`。读取 count 个 int8，返回独立列表。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.readInt8Array(count);
```

**源码**：[source/std/binary.mgl:58](../../source/std/binary.mgl#L58)。

**完整示例**：[example/samples/std_binary.mgl](../../example/samples/std_binary.mgl)。

<a id="api-12"></a>

### BinaryReader::readUint8（13）

**声明**

```magel
uint8 readUint8();
```

**行为**

读取一个 uint8，成功后前进 1 字节。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`uint8`。读取一个 uint8，成功后前进 1 字节。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.readUint8();
```

**源码**：[source/std/binary.mgl:64](../../source/std/binary.mgl#L64)。

**完整示例**：[example/samples/std_binary.mgl](../../example/samples/std_binary.mgl)。

<a id="api-13"></a>

### BinaryReader::readUint8Array（14）

**声明**

```magel
std::lang::List<uint8> readUint8Array(int64 count);
```

**行为**

读取 count 个 uint8，返回独立列表。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `count` | `int64 count` | 元素或字节数量，具体单位见行为说明 | 无 |

**返回值**

`std::lang::List<uint8>`。读取 count 个 uint8，返回独立列表。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.readUint8Array(count);
```

**源码**：[source/std/binary.mgl:70](../../source/std/binary.mgl#L70)。

**完整示例**：[example/samples/std_binary.mgl](../../example/samples/std_binary.mgl)。

<a id="api-14"></a>

### BinaryReader::readInt16（15）

**声明**

```magel
int16 readInt16();
```

**行为**

读取一个 int16，成功后前进 2 字节。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`int16`。读取一个 int16，成功后前进 2 字节。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.readInt16();
```

**源码**：[source/std/binary.mgl:76](../../source/std/binary.mgl#L76)。

**完整示例**：[example/samples/std_binary.mgl](../../example/samples/std_binary.mgl)。

<a id="api-15"></a>

### BinaryReader::readInt16Array（16）

**声明**

```magel
std::lang::List<int16> readInt16Array(int64 count);
```

**行为**

读取 count 个 int16，返回独立列表。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `count` | `int64 count` | 元素或字节数量，具体单位见行为说明 | 无 |

**返回值**

`std::lang::List<int16>`。读取 count 个 int16，返回独立列表。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.readInt16Array(count);
```

**源码**：[source/std/binary.mgl:82](../../source/std/binary.mgl#L82)。

**完整示例**：[example/samples/std_binary.mgl](../../example/samples/std_binary.mgl)。

<a id="api-16"></a>

### BinaryReader::readUint16（17）

**声明**

```magel
uint16 readUint16();
```

**行为**

读取一个 uint16，成功后前进 2 字节。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`uint16`。读取一个 uint16，成功后前进 2 字节。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.readUint16();
```

**源码**：[source/std/binary.mgl:88](../../source/std/binary.mgl#L88)。

**完整示例**：[example/samples/std_binary.mgl](../../example/samples/std_binary.mgl)。

<a id="api-17"></a>

### BinaryReader::readUint16Array（18）

**声明**

```magel
std::lang::List<uint16> readUint16Array(int64 count);
```

**行为**

读取 count 个 uint16，返回独立列表。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `count` | `int64 count` | 元素或字节数量，具体单位见行为说明 | 无 |

**返回值**

`std::lang::List<uint16>`。读取 count 个 uint16，返回独立列表。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.readUint16Array(count);
```

**源码**：[source/std/binary.mgl:94](../../source/std/binary.mgl#L94)。

**完整示例**：[example/samples/std_binary.mgl](../../example/samples/std_binary.mgl)。

<a id="api-18"></a>

### BinaryReader::readInt32（19）

**声明**

```magel
int32 readInt32();
```

**行为**

读取一个 int32，成功后前进 4 字节。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`int32`。读取一个 int32，成功后前进 4 字节。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.readInt32();
```

**源码**：[source/std/binary.mgl:100](../../source/std/binary.mgl#L100)。

**完整示例**：[example/samples/std_binary.mgl](../../example/samples/std_binary.mgl)。

<a id="api-19"></a>

### BinaryReader::readInt32Array（20）

**声明**

```magel
std::lang::List<int32> readInt32Array(int64 count);
```

**行为**

读取 count 个 int32，返回独立列表。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `count` | `int64 count` | 元素或字节数量，具体单位见行为说明 | 无 |

**返回值**

`std::lang::List<int32>`。读取 count 个 int32，返回独立列表。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.readInt32Array(count);
```

**源码**：[source/std/binary.mgl:106](../../source/std/binary.mgl#L106)。

**完整示例**：[example/samples/std_binary.mgl](../../example/samples/std_binary.mgl)。

<a id="api-20"></a>

### BinaryReader::readUint32（21）

**声明**

```magel
uint32 readUint32();
```

**行为**

读取一个 uint32，成功后前进 4 字节。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`uint32`。读取一个 uint32，成功后前进 4 字节。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.readUint32();
```

**源码**：[source/std/binary.mgl:112](../../source/std/binary.mgl#L112)。

**完整示例**：[example/samples/std_binary.mgl](../../example/samples/std_binary.mgl)。

<a id="api-21"></a>

### BinaryReader::readUint32Array（22）

**声明**

```magel
std::lang::List<uint32> readUint32Array(int64 count);
```

**行为**

读取 count 个 uint32，返回独立列表。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `count` | `int64 count` | 元素或字节数量，具体单位见行为说明 | 无 |

**返回值**

`std::lang::List<uint32>`。读取 count 个 uint32，返回独立列表。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.readUint32Array(count);
```

**源码**：[source/std/binary.mgl:118](../../source/std/binary.mgl#L118)。

**完整示例**：[example/samples/std_binary.mgl](../../example/samples/std_binary.mgl)。

<a id="api-22"></a>

### BinaryReader::readInt64（23）

**声明**

```magel
int64 readInt64();
```

**行为**

读取一个 int64，成功后前进 8 字节。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`int64`。读取一个 int64，成功后前进 8 字节。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.readInt64();
```

**源码**：[source/std/binary.mgl:124](../../source/std/binary.mgl#L124)。

**完整示例**：[example/samples/std_binary.mgl](../../example/samples/std_binary.mgl)。

<a id="api-23"></a>

### BinaryReader::readInt64Array（24）

**声明**

```magel
std::lang::List<int64> readInt64Array(int64 count);
```

**行为**

读取 count 个 int64，返回独立列表。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `count` | `int64 count` | 元素或字节数量，具体单位见行为说明 | 无 |

**返回值**

`std::lang::List<int64>`。读取 count 个 int64，返回独立列表。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.readInt64Array(count);
```

**源码**：[source/std/binary.mgl:130](../../source/std/binary.mgl#L130)。

**完整示例**：[example/samples/std_binary.mgl](../../example/samples/std_binary.mgl)。

<a id="api-24"></a>

### BinaryReader::readUint64（25）

**声明**

```magel
uint64 readUint64();
```

**行为**

读取一个 uint64，成功后前进 8 字节。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`uint64`。读取一个 uint64，成功后前进 8 字节。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.readUint64();
```

**源码**：[source/std/binary.mgl:136](../../source/std/binary.mgl#L136)。

**完整示例**：[example/samples/std_binary.mgl](../../example/samples/std_binary.mgl)。

<a id="api-25"></a>

### BinaryReader::readUint64Array（26）

**声明**

```magel
std::lang::List<uint64> readUint64Array(int64 count);
```

**行为**

读取 count 个 uint64，返回独立列表。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `count` | `int64 count` | 元素或字节数量，具体单位见行为说明 | 无 |

**返回值**

`std::lang::List<uint64>`。读取 count 个 uint64，返回独立列表。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.readUint64Array(count);
```

**源码**：[source/std/binary.mgl:142](../../source/std/binary.mgl#L142)。

**完整示例**：[example/samples/std_binary.mgl](../../example/samples/std_binary.mgl)。

<a id="api-26"></a>

### BinaryReader::readFloat32（27）

**声明**

```magel
float32 readFloat32();
```

**行为**

读取一个 float32，成功后前进 4 字节。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`float32`。读取一个 float32，成功后前进 4 字节。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.readFloat32();
```

**源码**：[source/std/binary.mgl:148](../../source/std/binary.mgl#L148)。

**完整示例**：[example/samples/std_binary.mgl](../../example/samples/std_binary.mgl)。

<a id="api-27"></a>

### BinaryReader::readFloat32Array（28）

**声明**

```magel
std::lang::List<float32> readFloat32Array(int64 count);
```

**行为**

读取 count 个 float32，返回独立列表。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `count` | `int64 count` | 元素或字节数量，具体单位见行为说明 | 无 |

**返回值**

`std::lang::List<float32>`。读取 count 个 float32，返回独立列表。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.readFloat32Array(count);
```

**源码**：[source/std/binary.mgl:154](../../source/std/binary.mgl#L154)。

**完整示例**：[example/samples/std_binary.mgl](../../example/samples/std_binary.mgl)。

<a id="api-28"></a>

### BinaryReader::readFloat64（29）

**声明**

```magel
float64 readFloat64();
```

**行为**

读取一个 float64，成功后前进 8 字节。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`float64`。读取一个 float64，成功后前进 8 字节。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.readFloat64();
```

**源码**：[source/std/binary.mgl:160](../../source/std/binary.mgl#L160)。

**完整示例**：[example/samples/std_binary.mgl](../../example/samples/std_binary.mgl)。

<a id="api-29"></a>

### BinaryReader::readFloat64Array（30）

**声明**

```magel
std::lang::List<float64> readFloat64Array(int64 count);
```

**行为**

读取 count 个 float64，返回独立列表。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `count` | `int64 count` | 元素或字节数量，具体单位见行为说明 | 无 |

**返回值**

`std::lang::List<float64>`。读取 count 个 float64，返回独立列表。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.readFloat64Array(count);
```

**源码**：[source/std/binary.mgl:166](../../source/std/binary.mgl#L166)。

**完整示例**：[example/samples/std_binary.mgl](../../example/samples/std_binary.mgl)。

<a id="api-30"></a>

### BinaryReader::readBool（31）

**声明**

```magel
bool readBool();
```

**行为**

读取一个 bool，成功后前进 1 字节。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`bool`。读取一个 bool，成功后前进 1 字节。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.readBool();
```

**源码**：[source/std/binary.mgl:172](../../source/std/binary.mgl#L172)。

**完整示例**：[example/samples/std_binary.mgl](../../example/samples/std_binary.mgl)。

<a id="api-31"></a>

### BinaryReader::readBoolArray（32）

**声明**

```magel
std::lang::List<bool> readBoolArray(int64 count);
```

**行为**

读取 count 个 bool，返回独立列表。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `count` | `int64 count` | 元素或字节数量，具体单位见行为说明 | 无 |

**返回值**

`std::lang::List<bool>`。读取 count 个 bool，返回独立列表。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.readBoolArray(count);
```

**源码**：[source/std/binary.mgl:178](../../source/std/binary.mgl#L178)。

**完整示例**：[example/samples/std_binary.mgl](../../example/samples/std_binary.mgl)。

<a id="api-32"></a>

### BinaryReader::readBytes（33）

**声明**

```magel
std::lang::List<uint8> readBytes(int64 count);
```

**行为**

读取原始字节，保留零字节。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `count` | `int64 count` | 元素或字节数量，具体单位见行为说明 | 无 |

**返回值**

`std::lang::List<uint8>`。读取原始字节，保留零字节。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.readBytes(count);
```

**源码**：[source/std/binary.mgl:184](../../source/std/binary.mgl#L184)。

**完整示例**：[example/samples/std_binary.mgl](../../example/samples/std_binary.mgl)。

<a id="api-33"></a>

### BinaryReader::readString（34）

**声明**

```magel
std::lang::String readString(int64 length);
```

**行为**

读取 length 字节文本字段，首个零字节终止文本，仍消费整个字段。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `length` | `int64 length` | 有效长度 | 无 |

**返回值**

`std::lang::String`。读取 length 字节文本字段，首个零字节终止文本，仍消费整个字段。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.readString(length);
```

**源码**：[source/std/binary.mgl:186](../../source/std/binary.mgl#L186)。

**完整示例**：[example/samples/std_binary.mgl](../../example/samples/std_binary.mgl)。

<a id="api-34"></a>

### BinaryReader::readStringArray（35）

**声明**

```magel
std::lang::List<std::lang::String> readStringArray(int64 count, int64 width);
```

**行为**

读取 count 个定宽字符串，每个字段占 width 字节。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `count` | `int64 count` | 元素或字节数量，具体单位见行为说明 | 无 |
| `width` | `int64 width` | 目标字节宽度或定宽字段大小 | 无 |

**返回值**

`std::lang::List<std::lang::String>`。读取 count 个定宽字符串，每个字段占 width 字节。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.readStringArray(count, width);
```

**源码**：[source/std/binary.mgl:192](../../source/std/binary.mgl#L192)。

**完整示例**：[example/samples/std_binary.mgl](../../example/samples/std_binary.mgl)。
