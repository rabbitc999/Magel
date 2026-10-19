# std::io — 文件与流接口

[返回库目录](README.md) · [全部符号](symbols.md)

## 引入

```magel
import std::io;
```

## 公共约定与错误边界

FileStream 返回 Result，失败载荷是 IoError；read/write/seek 之前检查打开结果。read 按底层文件读取一行。close 显式关闭句柄。Reader/Writer/Seek 是用户可实现的接口，接口本身不创建文件。路径与模式由宿主文件系统解释。

## 参考代码

完整程序：[doc/examples/library/io.mgl](../../doc/examples/library/io.mgl)。

```powershell
magel run doc/examples/library/io.mgl -- build/library-example.txt
```

先创建 `build` 目录；示例会覆盖给定路径，请使用专用测试文件。

<details>
<summary>展开完整参考代码</summary>

```magel
import std::io;

/** @brief 在调用者指定的测试文件中演示 FileStream 的 Result 错误处理。
 * @param args 仅一个文件路径，该文件会被覆盖。 @return 成功为 0。 */
int64 main(List<String> args) {
    assert(args.len() == 1, "请提供允许覆盖的示例文件路径");
    Result<FileStream,IoError> opened = FileStream::open(args[0],"w+");
    assert(opened.isOk(), "打开失败");
    FileStream stream = opened.take();
    Result<int64,IoError> written = stream.write("alpha\nbeta\n");
    assert(written.isOk());
    Result<int64,IoError> flushed = stream.flush();
    assert(flushed.isOk());
    Result<int64,IoError> located = stream.seek(0,SeekOrigin::Begin);
    assert(located.isOk());
    Result<String,IoError> first = stream.read();
    assert(first.isOk() && first.unwrap() == "alpha");
    stream.close();
    println("io API ok");
    return 0;
}
```

</details>

聚合入口的具体操作参见对应子模块；本页类型/方法的调用形式列在下方。

## 类型、枚举、常量与字段

公开字段按源码原样列出；句柄和游标字段用于包装器状态，通常通过方法操作。枚举的括号表示变体载荷。

| 所属类型 | 声明 | 说明 |
| --- | --- | --- |
| 模块 | <a id="symbol-108"></a>`enum IoError` | 从 lang.mgl 原样迁出：符号名不变，只换了模块归属（现在必须 `import std::io;` 才能用）。 |
| IoError | `OpenFailed(std::lang::String message), ReadFailed(std::lang::String message), WriteFailed(std::lang::String message), SeekFailed(std::lang::String message), Closed` | 从 lang.mgl 原样迁出：符号名不变，只换了模块归属（现在必须 `import std::io;` 才能用）。 变体及载荷见声明。 |
| 模块 | <a id="symbol-109"></a>`enum SeekOrigin` | 文件定位的基准：文件开始、当前位置或文件末尾。 |
| SeekOrigin | `Begin, Current, End` | 文件定位的基准：文件开始、当前位置或文件末尾。 变体及载荷见声明。 |
| 模块 | <a id="symbol-110"></a>`interface Reader` | 按字节读取数据的接口。 |
| 模块 | <a id="symbol-111"></a>`interface Writer` | 按字节写入数据的接口。 |
| 模块 | <a id="symbol-112"></a>`interface Seek` | 改变资源读写位置的接口。 |
| 模块 | <a id="symbol-113"></a>`interface ToString` | 将值转换成拥有字符串的接口。 |
| 模块 | <a id="symbol-114"></a>`interface Formattable` | 按格式说明生成文本的接口。 |
| 模块 | <a id="symbol-115"></a>`struct FileStream : Reader, Writer, Seek` | 拥有文件句柄的字节流；通过 Result 报告读写与定位错误。 |
| FileStream | <a id="symbol-116"></a>`std::lang::File handle` | 宿主资源句柄；应通过所属类型的方法操作。 |
| 模块 | <a id="symbol-117"></a>`struct FileInfo` | 文件信息，时间为 Unix 秒；目录的 size 为零。 |
| FileInfo | <a id="symbol-118"></a>`int64 size` | 文件或响应报告的字节长度。 |
| FileInfo | <a id="symbol-119"></a>`bool isFile` | 目标是否为普通文件。 |
| FileInfo | <a id="symbol-120"></a>`bool isDirectory` | 目标是否为目录。 |
| FileInfo | <a id="symbol-121"></a>`int64 modifiedUnixSeconds` | 最后修改时间，以 Unix 秒表示。 |
| 模块 | <a id="symbol-122"></a>`struct IoReply` | 宿主回调的拥有型结果；字段顺序是胶水 ABI 的一部分。 |
| IoReply | <a id="symbol-123"></a>`String error` | 最近操作的错误说明。 |
| IoReply | <a id="symbol-124"></a>`List<uint8> bytes` | 字节数据，保留零字节。 |
| IoReply | <a id="symbol-125"></a>`List<String> paths` | 目录枚举返回的路径。 |
| IoReply | <a id="symbol-126"></a>`int64 count` | 本次操作实际处理的数量。 |
| IoReply | <a id="symbol-127"></a>`int64 size` | 文件或响应报告的字节长度。 |
| IoReply | <a id="symbol-128"></a>`bool isFile` | 目标是否为普通文件。 |
| IoReply | <a id="symbol-129"></a>`bool isDirectory` | 目标是否为目录。 |
| IoReply | <a id="symbol-130"></a>`int64 modifiedUnixSeconds` | 最后修改时间，以 Unix 秒表示。 |

## 函数索引

| 名称 | 声明 |
| --- | --- |
| [Reader::read](#api-0) | `Result<std::lang::String, IoError> read()` |
| [Writer::write](#api-1) | `Result<int64, IoError> write(std::lang::String text)` |
| [Writer::flush](#api-2) | `Result<int64, IoError> flush()` |
| [Seek::seek](#api-3) | `Result<int64, IoError> seek(int64 offset, SeekOrigin origin)` |
| [ToString::toString](#api-4) | `std::lang::String toString()` |
| [Formattable::toString](#api-5) | `std::lang::String toString(std::lang::String style)` |
| [FileStream::open](#api-6) | `static Result<FileStream, IoError> open(std::lang::String path, std::lang::String mode)` |
| [FileStream::read](#api-7) | `Result<std::lang::String, IoError> read()` |
| [FileStream::readLine](#api-8) | `Result<String,IoError> readLine()` |
| [FileStream::readString](#api-9) | `Result<String,IoError> readString(int64 count)` |
| [FileStream::readAllString](#api-10) | `Result<String,IoError> readAllString(int64 maxBytes=16777216)` |
| [FileStream::write](#api-11) | `Result<int64, IoError> write(std::lang::String text)` |
| [FileStream::readBytes](#api-12) | `Result<List<uint8>,IoError> readBytes(int64 count)` |
| [FileStream::writeBytes](#api-13) | `Result<int64,IoError> writeBytes(List<uint8> bytes)` |
| [FileStream::flush](#api-14) | `Result<int64, IoError> flush()` |
| [FileStream::seek](#api-15) | `Result<int64, IoError> seek(int64 offset, SeekOrigin origin)` |
| [FileStream::close](#api-16) | `int64 close()` |
| [fileOpen](#api-17) | `Result<std::lang::File, std::lang::String> fileOpen(std::lang::String path, std::lang::String mode)` |
| [fileRead](#api-18) | `std::lang::String fileRead(file const& handle)` |
| [fileReadLine](#api-19) | `String fileReadLine(File const& handle)` |
| [fileReadString](#api-20) | `String fileReadString(File const& handle, int64 count)` |
| [fileReadAllString](#api-21) | `String fileReadAllString(File const& handle, int64 maxBytes = 16777216)` |
| [fileWrite](#api-22) | `int64 fileWrite(file const& handle, std::lang::String text)` |
| [fileEof](#api-23) | `int64 fileEof(file const& handle)` |
| [fileClose](#api-24) | `int64 fileClose(file const& handle)` |
| [readAllBytes](#api-25) | `Result<List<uint8>,IoError> readAllBytes(String path, int64 maxBytes=16777216)` |
| [writeAllBytes](#api-26) | `Result<int64,IoError> writeAllBytes(String path, List<uint8> bytes)` |
| [readAllText](#api-27) | `Result<String,IoError> readAllText(String path, int64 maxBytes=16777216)` |
| [writeAllText](#api-28) | `Result<int64,IoError> writeAllText(String path, String text)` |
| [listDirectory](#api-29) | `Result<List<String>,IoError> listDirectory(String path)` |
| [fileInfo](#api-30) | `Result<FileInfo,IoError> fileInfo(String path)` |
| [createDirectories](#api-31) | `Result<int64,IoError> createDirectories(String path)` |
| [renamePath](#api-32) | `Result<int64,IoError> renamePath(String from, String to)` |

<a id="api-0"></a>

### Reader::read（1）

**声明**

```magel
Result<std::lang::String, IoError> read();
```

**行为**

读取一行文本，失败返回 ReadFailed。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`Result<std::lang::String, IoError>`。读取一行文本，失败返回 ReadFailed。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.read();
```

**源码**：[source/std/io.mgl:11](../../source/std/io.mgl#L11)。

**完整示例**：[doc/examples/library/io.mgl](../../doc/examples/library/io.mgl)。

<a id="api-1"></a>

### Writer::write（2）

**声明**

```magel
Result<int64, IoError> write(std::lang::String text);
```

**行为**

写入文本，返回写入字节数或 WriteFailed。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `text` | `std::lang::String text` | 输入文本，位置与长度通常按字节 | 无 |

**返回值**

`Result<int64, IoError>`。写入文本，返回写入字节数或 WriteFailed。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.write(text);
```

**源码**：[source/std/io.mgl:13](../../source/std/io.mgl#L13)。

**完整示例**：[doc/examples/library/io.mgl](../../doc/examples/library/io.mgl)。

<a id="api-2"></a>

### Writer::flush（3）

**声明**

```magel
Result<int64, IoError> flush();
```

**行为**

刷新输出缓冲，失败返回 WriteFailed。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`Result<int64, IoError>`。刷新输出缓冲，失败返回 WriteFailed。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.flush();
```

**源码**：[source/std/io.mgl:14](../../source/std/io.mgl#L14)。

**完整示例**：[doc/examples/library/io.mgl](../../doc/examples/library/io.mgl)。

<a id="api-3"></a>

### Seek::seek（4）

**声明**

```magel
Result<int64, IoError> seek(int64 offset, SeekOrigin origin);
```

**行为**

按 origin 与 offset 定位，返回新位置或 SeekFailed。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `offset` | `int64 offset` | 字节偏移或 seek 相对位移 | 无 |
| `origin` | `SeekOrigin origin` | 定位基准或坐标原点 | 无 |

**返回值**

`Result<int64, IoError>`。按 origin 与 offset 定位，返回新位置或 SeekFailed。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.seek(offset, origin);
```

**源码**：[source/std/io.mgl:16](../../source/std/io.mgl#L16)。

**完整示例**：[doc/examples/library/io.mgl](../../doc/examples/library/io.mgl)。

<a id="api-4"></a>

### ToString::toString（5）

**声明**

```magel
std::lang::String toString();
```

**行为**

由实现者将当前值转换为文本，Formattable 重载接收 style。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`std::lang::String`。由实现者将当前值转换为文本，Formattable 重载接收 style。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.toString();
```

**源码**：[source/std/io.mgl:17](../../source/std/io.mgl#L17)。

**完整示例**：[doc/examples/library/io.mgl](../../doc/examples/library/io.mgl)。

<a id="api-5"></a>

### Formattable::toString（6）

**声明**

```magel
std::lang::String toString(std::lang::String style);
```

**行为**

由实现者将当前值转换为文本，Formattable 重载接收 style。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `style` | `std::lang::String style` | 格式化风格 | 无 |

**返回值**

`std::lang::String`。由实现者将当前值转换为文本，Formattable 重载接收 style。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.toString(style);
```

**源码**：[source/std/io.mgl:18](../../source/std/io.mgl#L18)。

**完整示例**：[doc/examples/library/io.mgl](../../doc/examples/library/io.mgl)。

<a id="api-6"></a>

### FileStream::open（7）

**声明**

```magel
static Result<FileStream, IoError> open(std::lang::String path, std::lang::String mode);
```

**行为**

按 path/mode 打开文件，返回流或 OpenFailed。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `path` | `std::lang::String path` | 文件/数据库路径 | 无 |
| `mode` | `std::lang::String mode` | 宿主文件打开模式 | 无 |

**返回值**

`Result<FileStream, IoError>`。按 path/mode 打开文件，返回流或 OpenFailed。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::io::FileStream::open(path, mode);
```

**源码**：[source/std/io.mgl:34](../../source/std/io.mgl#L34)。

**完整示例**：[doc/examples/library/io.mgl](../../doc/examples/library/io.mgl)。

<a id="api-7"></a>

### FileStream::read（8）

**声明**

```magel
Result<std::lang::String, IoError> read();
```

**行为**

读取一行文本，失败返回 ReadFailed。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`Result<std::lang::String, IoError>`。读取一行文本，失败返回 ReadFailed。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.read();
```

**源码**：[source/std/io.mgl:46](../../source/std/io.mgl#L46)。

**完整示例**：[doc/examples/library/io.mgl](../../doc/examples/library/io.mgl)。

<a id="api-8"></a>

### FileStream::readLine（9）

**声明**

```magel
Result<String,IoError> readLine();
```

**行为**

读取一行文本，保留空格，去掉 LF/CRLF；EOF 返回 Ok(空串)。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`Result<String,IoError>`。行文本或 ReadFailed，不隐式关闭文件。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.readLine();
```

**源码**：[source/std/io.mgl:57](../../source/std/io.mgl#L57)。

**完整示例**：[doc/examples/library/io.mgl](../../doc/examples/library/io.mgl)。

<a id="api-9"></a>

### FileStream::readString（10）

**声明**

```magel
Result<String,IoError> readString(int64 count);
```

**行为**

从当前游标读取至多 count 个字节为文本；EOF 返回较短字符串。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `count` | `int64 count` | 非负字节数；不按字符或词语切分。 | 无 |

**返回值**

`Result<String,IoError>`。文本或 ReadFailed；含零字节时返回错误，应改用 readBytes。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.readString(count);
```

**源码**：[source/std/io.mgl:65](../../source/std/io.mgl#L65)。

**完整示例**：[doc/examples/library/io.mgl](../../doc/examples/library/io.mgl)。

<a id="api-10"></a>

### FileStream::readAllString（11）

**声明**

```magel
Result<String,IoError> readAllString(int64 maxBytes=16777216);
```

**行为**

从当前游标读到 EOF，保留文件换行字节；默认最多 16 MiB。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `maxBytes` | `int64 maxBytes` | 非负字节预算；超过预算或含零字节返回错误。 | `16777216` |

**返回值**

`Result<String,IoError>`。剩余完整文本或 ReadFailed；预算失败时可能已消费一个额外字节。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.readAllString(maxBytes);
```

**源码**：[source/std/io.mgl:77](../../source/std/io.mgl#L77)。

**完整示例**：[doc/examples/library/io.mgl](../../doc/examples/library/io.mgl)。

<a id="api-11"></a>

### FileStream::write（12）

**声明**

```magel
Result<int64, IoError> write(std::lang::String text);
```

**行为**

写入文本，返回写入字节数或 WriteFailed。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `text` | `std::lang::String text` | 输入文本，位置与长度通常按字节 | 无 |

**返回值**

`Result<int64, IoError>`。写入文本，返回写入字节数或 WriteFailed。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.write(text);
```

**源码**：[source/std/io.mgl:91](../../source/std/io.mgl#L91)。

**完整示例**：[doc/examples/library/io.mgl](../../doc/examples/library/io.mgl)。

<a id="api-12"></a>

### FileStream::readBytes（13）

**声明**

```magel
Result<List<uint8>,IoError> readBytes(int64 count);
```

**行为**

读取至多 count 个字节；EOF 返回较短列表。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `count` | `int64 count` | 元素或字节数量，具体单位见行为说明 | 无 |

**返回值**

`Result<List<uint8>,IoError>`。读取至多 count 个字节；EOF 返回较短列表。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.readBytes(count);
```

**源码**：[source/std/io.mgl:101](../../source/std/io.mgl#L101)。

**完整示例**：[doc/examples/library/io.mgl](../../doc/examples/library/io.mgl)。

<a id="api-13"></a>

### FileStream::writeBytes（14）

**声明**

```magel
Result<int64,IoError> writeBytes(List<uint8> bytes);
```

**行为**

写入所有字节，保留零字节。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `bytes` | `List<uint8> bytes` | 输入或输出字节存储 | 无 |

**返回值**

`Result<int64,IoError>`。写入所有字节，保留零字节。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.writeBytes(bytes);
```

**源码**：[source/std/io.mgl:107](../../source/std/io.mgl#L107)。

**完整示例**：[doc/examples/library/io.mgl](../../doc/examples/library/io.mgl)。

<a id="api-14"></a>

### FileStream::flush（15）

**声明**

```magel
Result<int64, IoError> flush();
```

**行为**

刷新输出缓冲，失败返回 WriteFailed。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`Result<int64, IoError>`。刷新输出缓冲，失败返回 WriteFailed。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.flush();
```

**源码**：[source/std/io.mgl:113](../../source/std/io.mgl#L113)。

**完整示例**：[doc/examples/library/io.mgl](../../doc/examples/library/io.mgl)。

<a id="api-15"></a>

### FileStream::seek（16）

**声明**

```magel
Result<int64, IoError> seek(int64 offset, SeekOrigin origin);
```

**行为**

按 origin 与 offset 定位，返回新位置或 SeekFailed。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `offset` | `int64 offset` | 字节偏移或 seek 相对位移 | 无 |
| `origin` | `SeekOrigin origin` | 定位基准或坐标原点 | 无 |

**返回值**

`Result<int64, IoError>`。按 origin 与 offset 定位，返回新位置或 SeekFailed。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.seek(offset, origin);
```

**源码**：[source/std/io.mgl:122](../../source/std/io.mgl#L122)。

**完整示例**：[doc/examples/library/io.mgl](../../doc/examples/library/io.mgl)。

<a id="api-16"></a>

### FileStream::close（17）

**声明**

```magel
int64 close();
```

**行为**

关闭底层资源；文档节点会使同文档所有视图失效。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`int64`。关闭底层资源；文档节点会使同文档所有视图失效。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.close();
```

**源码**：[source/std/io.mgl:137](../../source/std/io.mgl#L137)。

**完整示例**：[doc/examples/library/io.mgl](../../doc/examples/library/io.mgl)。

<a id="api-17"></a>

### fileOpen（18）

**声明**

```magel
Result<std::lang::File, std::lang::String> fileOpen(std::lang::String path, std::lang::String mode);
```

**行为**

按路径和模式打开文件，返回 Result<File,String>，失败为 Err。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `path` | `std::lang::String path` | 文件/数据库路径 | 无 |
| `mode` | `std::lang::String mode` | 宿主文件打开模式 | 无 |

**返回值**

`Result<std::lang::File, std::lang::String>`。按路径和模式打开文件，返回 Result<File,String>，失败为 Err。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::io::fileOpen(path, mode);
```

**源码**：[source/std/io.mgl:142](../../source/std/io.mgl#L142)。

**完整示例**：[doc/examples/library/io.mgl](../../doc/examples/library/io.mgl)。

<a id="api-18"></a>

### fileRead（19）

**声明**

```magel
std::lang::String fileRead(file const& handle);
```

**行为**

从已打开文件读取文本行。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `handle` | `file const& handle` | 本模块创建且仍有效的宿主句柄 | 无 |

**返回值**

`std::lang::String`。从已打开文件读取文本行。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::io::fileRead(handle);
```

**源码**：[source/std/io.mgl:150](../../source/std/io.mgl#L150)。

**完整示例**：[doc/examples/library/io.mgl](../../doc/examples/library/io.mgl)。

<a id="api-19"></a>

### fileReadLine（20）

**声明**

```magel
String fileReadLine(File const& handle);
```

**行为**

读取文件句柄的一行文本，不含 LF/CRLF；保留既有 fileRead 语义。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `handle` | `File const& handle` | 已打开的文件。 | 无 |

**返回值**

`String`。行文本，EOF 为空串，读取错误触发 panic。

**错误与边界**

遵循本页“公共约定与错误边界”。
实现包含 panic 分支；调用前应处理空值或失败状态。

**调用形式**

```magel
std::io::fileReadLine(handle);
```

**源码**：[source/std/io.mgl:153](../../source/std/io.mgl#L153)。

**完整示例**：[doc/examples/library/io.mgl](../../doc/examples/library/io.mgl)。

<a id="api-20"></a>

### fileReadString（21）

**声明**

```magel
String fileReadString(File const& handle, int64 count);
```

**行为**

从文件借用中读取至多 count 个字节的文本。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `handle` | `File const& handle` | 本模块创建且仍有效的宿主句柄 | 无 |
| `count` | `int64 count` | 元素或字节数量，具体单位见行为说明 | 无 |

**返回值**

`String`。从文件借用中读取至多 count 个字节的文本。

**错误与边界**

遵循本页“公共约定与错误边界”。
实现包含 panic 分支；调用前应处理空值或失败状态。

**调用形式**

```magel
std::io::fileReadString(handle, count);
```

**源码**：[source/std/io.mgl:159](../../source/std/io.mgl#L159)。

**完整示例**：[doc/examples/library/io.mgl](../../doc/examples/library/io.mgl)。

<a id="api-21"></a>

### fileReadAllString（22）

**声明**

```magel
String fileReadAllString(File const& handle, int64 maxBytes = 16777216);
```

**行为**

从文件借用中读取剩余文本，并检查字节预算。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `handle` | `File const& handle` | 本模块创建且仍有效的宿主句柄 | 无 |
| `maxBytes` | `int64 maxBytes` | 最多接收字节数 | `16777216` |

**返回值**

`String`。从文件借用中读取剩余文本，并检查字节预算。

**错误与边界**

遵循本页“公共约定与错误边界”。
实现包含 panic 分支；调用前应处理空值或失败状态。

**调用形式**

```magel
std::io::fileReadAllString(handle, maxBytes);
```

**源码**：[source/std/io.mgl:171](../../source/std/io.mgl#L171)。

**完整示例**：[doc/examples/library/io.mgl](../../doc/examples/library/io.mgl)。

<a id="api-22"></a>

### fileWrite（23）

**声明**

```magel
int64 fileWrite(file const& handle, std::lang::String text);
```

**行为**

向文件写入文本并返回宿主状态/计数。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `handle` | `file const& handle` | 本模块创建且仍有效的宿主句柄 | 无 |
| `text` | `std::lang::String text` | 输入文本，位置与长度通常按字节 | 无 |

**返回值**

`int64`。向文件写入文本并返回宿主状态/计数。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::io::fileWrite(handle, text);
```

**源码**：[source/std/io.mgl:185](../../source/std/io.mgl#L185)。

**完整示例**：[doc/examples/library/io.mgl](../../doc/examples/library/io.mgl)。

<a id="api-23"></a>

### fileEof（24）

**声明**

```magel
int64 fileEof(file const& handle);
```

**行为**

检查文件是否到达 EOF。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `handle` | `file const& handle` | 本模块创建且仍有效的宿主句柄 | 无 |

**返回值**

`int64`。检查文件是否到达 EOF。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::io::fileEof(handle);
```

**源码**：[source/std/io.mgl:186](../../source/std/io.mgl#L186)。

**完整示例**：[doc/examples/library/io.mgl](../../doc/examples/library/io.mgl)。

<a id="api-24"></a>

### fileClose（25）

**声明**

```magel
int64 fileClose(file const& handle);
```

**行为**

关闭原生文件句柄并返回宿主状态码。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `handle` | `file const& handle` | 本模块创建且仍有效的宿主句柄 | 无 |

**返回值**

`int64`。关闭原生文件句柄并返回宿主状态码。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::io::fileClose(handle);
```

**源码**：[source/std/io.mgl:187](../../source/std/io.mgl#L187)。

**完整示例**：[doc/examples/library/io.mgl](../../doc/examples/library/io.mgl)。

<a id="api-25"></a>

### readAllBytes（26）

**声明**

```magel
Result<List<uint8>,IoError> readAllBytes(String path, int64 maxBytes=16777216);
```

**行为**

读完整文件，保留零字节；默认最多 16 MiB，调用者可以指定预算。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `path` | `String path` | 文件/数据库路径 | 无 |
| `maxBytes` | `int64 maxBytes` | 最多接收字节数 | `16777216` |

**返回值**

`Result<List<uint8>,IoError>`。读完整文件，保留零字节；默认最多 16 MiB，调用者可以指定预算。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::io::readAllBytes(path, maxBytes);
```

**源码**：[source/std/io.mgl:203](../../source/std/io.mgl#L203)。

**完整示例**：[doc/examples/library/io.mgl](../../doc/examples/library/io.mgl)。

<a id="api-26"></a>

### writeAllBytes（27）

**声明**

```magel
Result<int64,IoError> writeAllBytes(String path, List<uint8> bytes);
```

**行为**

覆盖写完整文件，返回字节数量；不隐式创建父目录。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `path` | `String path` | 文件/数据库路径 | 无 |
| `bytes` | `List<uint8> bytes` | 输入或输出字节存储 | 无 |

**返回值**

`Result<int64,IoError>`。覆盖写完整文件，返回字节数量；不隐式创建父目录。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::io::writeAllBytes(path, bytes);
```

**源码**：[source/std/io.mgl:209](../../source/std/io.mgl#L209)。

**完整示例**：[doc/examples/library/io.mgl](../../doc/examples/library/io.mgl)。

<a id="api-27"></a>

### readAllText（28）

**声明**

```magel
Result<String,IoError> readAllText(String path, int64 maxBytes=16777216);
```

**行为**

读取完整文本；String 不支持内嵌零字节，此类文件应使用 readAllBytes。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `path` | `String path` | 文件/数据库路径 | 无 |
| `maxBytes` | `int64 maxBytes` | 最多接收字节数 | `16777216` |

**返回值**

`Result<String,IoError>`。读取完整文本；String 不支持内嵌零字节，此类文件应使用 readAllBytes。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::io::readAllText(path, maxBytes);
```

**源码**：[source/std/io.mgl:215](../../source/std/io.mgl#L215)。

**完整示例**：[doc/examples/library/io.mgl](../../doc/examples/library/io.mgl)。

<a id="api-28"></a>

### writeAllText（29）

**声明**

```magel
Result<int64,IoError> writeAllText(String path, String text);
```

**行为**

覆盖写完整文本，不自动追加换行。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `path` | `String path` | 文件/数据库路径 | 无 |
| `text` | `String text` | 输入文本，位置与长度通常按字节 | 无 |

**返回值**

`Result<int64,IoError>`。覆盖写完整文本，不自动追加换行。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::io::writeAllText(path, text);
```

**源码**：[source/std/io.mgl:224](../../source/std/io.mgl#L224)。

**完整示例**：[doc/examples/library/io.mgl](../../doc/examples/library/io.mgl)。

<a id="api-29"></a>

### listDirectory（30）

**声明**

```magel
Result<List<String>,IoError> listDirectory(String path);
```

**行为**

列出一层目录的完整路径，按字节字典序排序，不递归。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `path` | `String path` | 文件/数据库路径 | 无 |

**返回值**

`Result<List<String>,IoError>`。列出一层目录的完整路径，按字节字典序排序，不递归。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::io::listDirectory(path);
```

**源码**：[source/std/io.mgl:229](../../source/std/io.mgl#L229)。

**完整示例**：[doc/examples/library/io.mgl](../../doc/examples/library/io.mgl)。

<a id="api-30"></a>

### fileInfo（31）

**声明**

```magel
Result<FileInfo,IoError> fileInfo(String path);
```

**行为**

查询跟随符号链接后的文件信息；不存在时返回错误。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `path` | `String path` | 文件/数据库路径 | 无 |

**返回值**

`Result<FileInfo,IoError>`。查询跟随符号链接后的文件信息；不存在时返回错误。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::io::fileInfo(path);
```

**源码**：[source/std/io.mgl:235](../../source/std/io.mgl#L235)。

**完整示例**：[doc/examples/library/io.mgl](../../doc/examples/library/io.mgl)。

<a id="api-31"></a>

### createDirectories（32）

**声明**

```magel
Result<int64,IoError> createDirectories(String path);
```

**行为**

创建目录及缺失的父目录，已存在的目录视为成功。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `path` | `String path` | 文件/数据库路径 | 无 |

**返回值**

`Result<int64,IoError>`。创建目录及缺失的父目录，已存在的目录视为成功。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::io::createDirectories(path);
```

**源码**：[source/std/io.mgl:243](../../source/std/io.mgl#L243)。

**完整示例**：[doc/examples/library/io.mgl](../../doc/examples/library/io.mgl)。

<a id="api-32"></a>

### renamePath（33）

**声明**

```magel
Result<int64,IoError> renamePath(String from, String to);
```

**行为**

按宿主文件系统规则重命名；目标覆盖规则由平台决定。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `from` | `String from` | 被替换的子串 | 无 |
| `to` | `String to` | 替换文本 | 无 |

**返回值**

`Result<int64,IoError>`。按宿主文件系统规则重命名；目标覆盖规则由平台决定。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::io::renamePath(from, to);
```

**源码**：[source/std/io.mgl:249](../../source/std/io.mgl#L249)。

**完整示例**：[doc/examples/library/io.mgl](../../doc/examples/library/io.mgl)。

## 补充完整示例

[example/samples/std_file_strings.mgl](../../example/samples/std_file_strings.mgl)。

```magel
import std::io;

int64 main(List<String> args) {
    String path=args[0];
    FileStream output=FileStream::open(path,"wb").take();
    output.write("abc\r\nline two\nlast").unwrap();
    output.close();
    FileStream input=FileStream::open(path,"rb").take();
    assert(input.readString(2).unwrap()=="ab");
    assert(input.readLine().unwrap()=="c");
    assert(input.readLine().unwrap()=="line two");
    assert(input.readAllString().unwrap()=="last");
    assert(input.readLine().unwrap()=="");
    input.close();
    File handle=fileOpen(path,"rb").take();
    assert(fileReadString(&handle,3)=="abc");
    assert(fileReadLine(&handle)=="");
    assert(fileReadAllString(&handle)=="line two\nlast");
    fileClose(&handle);
    println("file strings ok");return 0;
}
```
