# std::compress — 无损压缩

[返回库目录](README.md) · [全部符号](symbols.md)

## 引入

```magel
import std::compress;
```

## 公共约定与错误边界

compress 输入文本，输出二进制 List<uint8>；不能把压缩结果当 UTF-8 文本输出。decompress 对无效编码或损坏数据报告宿主运行错误，不返回 Result。

## 参考代码

完整程序：[example/samples/std_compress.mgl](../../example/samples/std_compress.mgl)。

```powershell
magel run example/samples/std_compress.mgl
```

<details>
<summary>展开完整参考代码</summary>

```magel
import std::compress;
import std::text;

/* std::compress —— 文本压成字节序列、再解回来（底层是仓库自带的 lztext）。
 * 压缩结果是二进制（含 NUL 字节），类型是 List<uint8>，不要当字符串拼接。 */
int64 main() {
    /* 高重复度文本：应该明显变小 */
    String text = std::text::Text::repeat("magel 文本压缩往返测试；", 40);
    List<uint8> packed = std::compress::compress(text);
    println("原文 {} 压缩后 {} 变小 {}", len(text), packed.len(),
    packed.len() < len(text));
    String back = std::compress::decompress(packed);
    assert(back == text);
    println("往返一致 {}", back == text);

    /* 空串与单字符：都要能原样解回来 */
    List<uint8> empty = std::compress::compress("");
    assert(std::compress::decompress(empty) == "");
    List<uint8> one = std::compress::compress("a");
    assert(std::compress::decompress(one) == "a");
    println("空串 {} 单字符 {}", empty.len(), one.len());

    /* 压不出收益时自动退回原样存储：体积不降，但解压照常正确 */
    String noise = "9f3a!Q7z@1mKp2Vb8Xc4Rt6Yh0Nj5Lw3Sg";
    List<uint8> packedNoise = std::compress::compress(noise);
    assert(std::compress::decompress(packedNoise) == noise);
    println("噪声 原文 {} 压缩后 {}", len(noise), packedNoise.len());
    return 0;
}
```

</details>

<details>
<summary>示例输出</summary>

```text
原文 1320 压缩后 179 变小 1
往返一致 1
空串 6 单字符 7
噪声 原文 34 压缩后 40
```

</details>

聚合入口的具体操作参见对应子模块；本页类型/方法的调用形式列在下方。

## 函数索引

| 名称 | 声明 |
| --- | --- |
| [compress](#api-0) | `std::lang::List<uint8> compress(std::lang::String text)` |
| [decompress](#api-1) | `std::lang::String decompress(std::lang::List<uint8> bytes)` |

<a id="api-0"></a>

### compress（1）

**声明**

```magel
std::lang::List<uint8> compress(std::lang::String text);
```

**行为**

文本压缩与解压：要 `import std::compress;` 才可见。 宿主绑定（lztext）的键名在 glue/std_compress.hpp 里，已改成 std::compress::*。 压缩结果是**二进制**（含 NUL 字节），所以类型是 std::lang::List<uint8>， 不要当字符串打印或拼接。解压出来的才是 std::lang::String。 注意：不保证一定比原文短——随机数据、极短文本这类压不出收益时， 底层会自动按原样存储，体积不变但解压照常正确。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `text` | `std::lang::String text` | 输入文本，位置与长度通常按字节 | 无 |

**返回值**

`std::lang::List<uint8>`。文本压缩与解压：要 `import std::compress;` 才可见。 宿主绑定（lztext）的键名在 glue/std_compress.hpp 里，已改成 std::compress::*。 压缩结果是**二进制**（含 NUL 字节），所以类型是 std::lang::List<uint8>， 不要当字符串打印或拼接。解压出来的才是 std::lang::String。 注意：不保证一定比原文短——随机数据、极短文本这类压不出收益时， 底层会自动按原样存储，体积不变但解压照常正确。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::compress::compress(text);
```

**源码**：[source/std/compress.mgl:12](../../source/std/compress.mgl#L12)。

**完整示例**：[example/samples/std_compress.mgl](../../example/samples/std_compress.mgl)。

<a id="api-1"></a>

### decompress（2）

**声明**

```magel
std::lang::String decompress(std::lang::List<uint8> bytes);
```

**行为**

将有效 lztext 二进制数据还原为文本；损坏时报运行错误。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `bytes` | `std::lang::List<uint8> bytes` | 输入或输出字节存储 | 无 |

**返回值**

`std::lang::String`。将有效 lztext 二进制数据还原为文本；损坏时报运行错误。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::compress::decompress(bytes);
```

**源码**：[source/std/compress.mgl:13](../../source/std/compress.mgl#L13)。

**完整示例**：[example/samples/std_compress.mgl](../../example/samples/std_compress.mgl)。
