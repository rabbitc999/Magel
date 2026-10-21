# std::image — 图片处理

[返回库目录](README.md) · [全部符号](symbols.md)

## 引入

```magel
import std::image;
```

## 公共约定与错误边界

Image 是可深复制的 opcode，尺寸与连续 RGBA8888 像素存储由宿主记录拥有；width()/height() 返回尺寸，pixels() 返回独立列表快照。decode/open 支持无压缩 24/32 位 BMP 与二进制 P5/P6，按魔数自动识别；encodeBmp/encodePpm/encodePgm 与 saveBmp/savePpm/savePgm 生成对应文件字节或写盘。getPixel/setPixel 越界或分量超 0..255 返回错误，不 panic。卷积要求核边长为不超过 15 的奇数，边缘复制填充；池化窗口 0=最大 1=平均，输出尺寸向下取整。灰度采用 BT.601 亮度；ARGB8888 每像素一个 uint32，RGB565 每像素一个 uint16，还原时按高位复制扩展。大津法返回 0..255 阈值，二值化大于阈值给 255。重活由宿主胶水 glue/std_image.hpp 提供，裁剪宏 NO_MAGEL_STD_IMAGE。

## 参考代码

聚合入口的具体操作参见对应子模块；本页类型/方法的调用形式列在下方。

## 类型、枚举、常量与字段

公开字段按源码原样列出；句柄和游标字段用于包装器状态，通常通过方法操作。枚举的括号表示变体载荷。

| 所属类型 | 声明 | 说明 |
| --- | --- | --- |
| 模块 | <a id="symbol-89"></a>`struct Pixel` | 单个像素的颜色分量。 |
| Pixel | <a id="symbol-90"></a>`int64 r` | 红色分量，0..255。 |
| Pixel | <a id="symbol-91"></a>`int64 g` | 绿色分量，0..255。 |
| Pixel | <a id="symbol-92"></a>`int64 b` | 蓝色分量，0..255。 |
| Pixel | <a id="symbol-93"></a>`int64 a` | 透明度分量，0..255，255 为不透明。 |
| 模块 | <a id="symbol-94"></a>`struct ImageReply` | 胶水操作的统一回执；槽布局必须与 glue/std_image.hpp 的 Reply 一致。 |
| ImageReply | <a id="symbol-95"></a>`String error` | 本次胶水调用的错误说明；成功时为空串。 |
| ImageReply | <a id="symbol-96"></a>`int64 width` | 结果图片宽。 |
| ImageReply | <a id="symbol-97"></a>`int64 height` | 结果图片高。 |
| ImageReply | <a id="symbol-98"></a>`List<uint8> pixels` | 结果 RGBA 像素缓冲。 |
| ImageReply | <a id="symbol-99"></a>`List<uint8> bytes` | 编码得到的文件字节。 |
| ImageReply | <a id="symbol-100"></a>`List<uint8> gray` | 灰度缓冲，长度 width*height。 |
| ImageReply | <a id="symbol-101"></a>`List<uint32> argb` | ARGB8888 结果，每像素一个值。 |
| ImageReply | <a id="symbol-102"></a>`List<uint16> rgb565` | RGB565 结果，每像素一个值。 |
| ImageReply | <a id="symbol-103"></a>`int64 threshold` | 大津法阈值，0..255。 |
| ImageReply | <a id="symbol-104"></a>`int64 r` | getPixel 请求的 x 坐标（入参槽）。 |
| ImageReply | <a id="symbol-105"></a>`int64 g` | 保留槽，恒为 0。 |
| ImageReply | <a id="symbol-106"></a>`int64 b` | 保留槽，恒为 0。 |
| ImageReply | <a id="symbol-107"></a>`int64 a` | 保留槽，恒为 0。 |
| 模块 | <a id="symbol-108"></a>`opcode Image` | RGBA 图片：width × height，像素缓冲长度恒为 width*height*4。 |

## 函数索引

| 名称 | 声明 |
| --- | --- |
| [Image::width](#api-0) | `int64 width()` |
| [Image::height](#api-1) | `int64 height()` |
| [Image::pixels](#api-2) | `List<uint8> pixels()` |
| [Image::make](#api-3) | `static Result<Image, String> make(int64 width, int64 height)` |
| [Image::decode](#api-4) | `static Result<Image, String> decode(List<uint8> bytes)` |
| [Image::open](#api-5) | `static Result<Image, String> open(String path)` |
| [Image::encodeBmp](#api-6) | `Result<List<uint8>, String> encodeBmp()` |
| [Image::encodePpm](#api-7) | `Result<List<uint8>, String> encodePpm()` |
| [Image::encodePgm](#api-8) | `Result<List<uint8>, String> encodePgm()` |
| [Image::saveBmp](#api-9) | `Result<int64, String> saveBmp(String path)` |
| [Image::savePpm](#api-10) | `Result<int64, String> savePpm(String path)` |
| [Image::savePgm](#api-11) | `Result<int64, String> savePgm(String path)` |
| [Image::getPixel](#api-12) | `Result<Pixel, String> getPixel(int64 x, int64 y)` |
| [Image::setPixel](#api-13) | `Result<int64, String> setPixel(int64 x, int64 y, int64 r, int64 g, int64 b, int64 a)` |
| [Image::convolve](#api-14) | `Result<Image, String> convolve(List<float64> kernel, int64 kw, int64 kh)` |
| [Image::pool](#api-15) | `Result<Image, String> pool(int64 mode, int64 kw, int64 kh)` |
| [Image::toGray](#api-16) | `Result<List<uint8>, String> toGray()` |
| [Image::fromGray](#api-17) | `static Result<Image, String> fromGray(int64 width, int64 height, List<uint8> gray)` |
| [Image::toARGB](#api-18) | `Result<List<uint32>, String> toARGB()` |
| [Image::fromARGB](#api-19) | `static Result<Image, String> fromARGB(int64 width, int64 height, List<uint32> argb)` |
| [Image::toRGB565](#api-20) | `Result<List<uint16>, String> toRGB565()` |
| [Image::fromRGB565](#api-21) | `static Result<Image, String> fromRGB565(int64 width, int64 height, List<uint16> rgb565)` |
| [Image::otsuThreshold](#api-22) | `Result<int64, String> otsuThreshold()` |
| [Image::binarize](#api-23) | `static Result<List<uint8>, String> binarize(int64 width, int64 height, List<uint8> gray, int64 threshold)` |
| [Image::otsuBinarize](#api-24) | `Result<List<uint8>, String> otsuBinarize()` |

<a id="api-0"></a>

### Image::width（1）

**声明**

```magel
int64 width();
```

**行为**

图片宽度，单位像素。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`int64`。图片宽度，单位像素。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.width();
```

**源码**：[source/std/image.mgl:78](../../source/std/image.mgl#L78)。

<a id="api-1"></a>

### Image::height（2）

**声明**

```magel
int64 height();
```

**行为**

图片高度，单位像素。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`int64`。图片高度，单位像素。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.height();
```

**源码**：[source/std/image.mgl:80](../../source/std/image.mgl#L80)。

<a id="api-2"></a>

### Image::pixels（3）

**声明**

```magel
List<uint8> pixels();
```

**行为**

导出独立 RGBA 像素列表，修改返回值不会修改原图。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`List<uint8>`。导出独立 RGBA 像素列表，修改返回值不会修改原图。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.pixels();
```

**源码**：[source/std/image.mgl:82](../../source/std/image.mgl#L82)。

<a id="api-3"></a>

### Image::make（4）

**声明**

```magel
static Result<Image, String> make(int64 width, int64 height);
```

**行为**

新建全透明黑图。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `width` | `int64 width` | 图片宽，单位像素，必须为正。 | 无 |
| `height` | `int64 height` | 图片高，单位像素，必须为正。 | 无 |

**返回值**

`Result<Image, String>`。新图片；宽高不合法时返回错误。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::image::Image::make(width, height);
```

**源码**：[source/std/image.mgl:90](../../source/std/image.mgl#L90)。

<a id="api-4"></a>

### Image::decode（5）

**声明**

```magel
static Result<Image, String> decode(List<uint8> bytes);
```

**行为**

从图片文件字节解码，按魔数自动识别格式。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `bytes` | `List<uint8> bytes` | 文件字节，支持无压缩 24/32 位 BMP 与二进制 P5/P6。 | 无 |

**返回值**

`Result<Image, String>`。解码后的图片；格式不识别或数据不完整时返回错误。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::image::Image::decode(bytes);
```

**源码**：[source/std/image.mgl:102](../../source/std/image.mgl#L102)。

<a id="api-5"></a>

### Image::open（6）

**声明**

```magel
static Result<Image, String> open(String path);
```

**行为**

打开图片文件并解码。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `path` | `String path` | 图片文件路径。 | 无 |

**返回值**

`Result<Image, String>`。解码后的图片；读取失败或格式不识别时返回错误。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::image::Image::open(path);
```

**源码**：[source/std/image.mgl:116](../../source/std/image.mgl#L116)。

<a id="api-6"></a>

### Image::encodeBmp（7）

**声明**

```magel
Result<List<uint8>, String> encodeBmp();
```

**行为**

编码为 24 位 BMP 文件字节。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`Result<List<uint8>, String>`。BMP 文件字节；缓冲长度不符时返回错误。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.encodeBmp();
```

**源码**：[source/std/image.mgl:127](../../source/std/image.mgl#L127)。

<a id="api-7"></a>

### Image::encodePpm（8）

**声明**

```magel
Result<List<uint8>, String> encodePpm();
```

**行为**

编码为二进制 PPM（P6）文件字节。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`Result<List<uint8>, String>`。PPM 文件字节；缓冲长度不符时返回错误。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.encodePpm();
```

**源码**：[source/std/image.mgl:138](../../source/std/image.mgl#L138)。

<a id="api-8"></a>

### Image::encodePgm（9）

**声明**

```magel
Result<List<uint8>, String> encodePgm();
```

**行为**

编码为二进制 PGM（P5）文件字节。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`Result<List<uint8>, String>`。PGM 文件字节；缓冲长度不符时返回错误。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.encodePgm();
```

**源码**：[source/std/image.mgl:150](../../source/std/image.mgl#L150)。

<a id="api-9"></a>

### Image::saveBmp（10）

**声明**

```magel
Result<int64, String> saveBmp(String path);
```

**行为**

保存为 BMP 文件。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `path` | `String path` | 目标文件路径，覆盖写入，不创建父目录。 | 无 |

**返回值**

`Result<int64, String>`。写入的字节数；编码或写盘失败时返回错误。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.saveBmp(path);
```

**源码**：[source/std/image.mgl:162](../../source/std/image.mgl#L162)。

<a id="api-10"></a>

### Image::savePpm（11）

**声明**

```magel
Result<int64, String> savePpm(String path);
```

**行为**

保存为 PPM（P6）文件。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `path` | `String path` | 目标文件路径，覆盖写入，不创建父目录。 | 无 |

**返回值**

`Result<int64, String>`。写入的字节数；编码或写盘失败时返回错误。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.savePpm(path);
```

**源码**：[source/std/image.mgl:175](../../source/std/image.mgl#L175)。

<a id="api-11"></a>

### Image::savePgm（12）

**声明**

```magel
Result<int64, String> savePgm(String path);
```

**行为**

保存为 PGM（P5）文件。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `path` | `String path` | 目标文件路径，覆盖写入，不创建父目录。 | 无 |

**返回值**

`Result<int64, String>`。写入的字节数；编码或写盘失败时返回错误。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.savePgm(path);
```

**源码**：[source/std/image.mgl:188](../../source/std/image.mgl#L188)。

<a id="api-12"></a>

### Image::getPixel（13）

**声明**

```magel
Result<Pixel, String> getPixel(int64 x, int64 y);
```

**行为**

读取一个像素。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `x` | `int64 x` | 像素横坐标，从 0 开始。 | 无 |
| `y` | `int64 y` | 像素纵坐标，从 0 开始。 | 无 |

**返回值**

`Result<Pixel, String>`。像素颜色分量；坐标越界时返回错误。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.getPixel(x, y);
```

**源码**：[source/std/image.mgl:202](../../source/std/image.mgl#L202)。

<a id="api-13"></a>

### Image::setPixel（14）

**声明**

```magel
Result<int64, String> setPixel(int64 x, int64 y, int64 r, int64 g, int64 b, int64 a);
```

**行为**

修改一个像素。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `x` | `int64 x` | 像素横坐标，从 0 开始。 | 无 |
| `y` | `int64 y` | 像素纵坐标，从 0 开始。 | 无 |
| `r` | `int64 r` | 红色分量，0..255。 | 无 |
| `g` | `int64 g` | 绿色分量，0..255。 | 无 |
| `b` | `int64 b` | 蓝色分量，0..255。 | 无 |
| `a` | `int64 a` | 透明度分量，0..255。 | 无 |

**返回值**

`Result<int64, String>`。成功时为 0；坐标越界或分量超范围时返回错误，不修改缓冲。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.setPixel(x, y, r, g, b, a);
```

**源码**：[source/std/image.mgl:225](../../source/std/image.mgl#L225)。

<a id="api-14"></a>

### Image::convolve（15）

**声明**

```magel
Result<Image, String> convolve(List<float64> kernel, int64 kw, int64 kh);
```

**行为**

用浮点核对图片做卷积。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `kernel` | `List<float64> kernel` | 卷积核元素，按行优先排列，长度必须为 kw kh。 | 无 |
| `kw` | `int64 kw` | 卷积核宽，不超过 15 的奇数。 | 无 |
| `kh` | `int64 kh` | 卷积核高，不超过 15 的奇数。 | 无 |

**返回值**

`Result<Image, String>`。卷积后的新图片；核形状或缓冲长度不符时返回错误。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.convolve(kernel, kw, kh);
```

**源码**：[source/std/image.mgl:248](../../source/std/image.mgl#L248)。

<a id="api-15"></a>

### Image::pool（16）

**声明**

```magel
Result<Image, String> pool(int64 mode, int64 kw, int64 kh);
```

**行为**

对图片做不重叠窗口池化。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `mode` | `int64 mode` | 池化模式：0 = 窗口最大值，1 = 窗口平均值（四舍五入）。 | 无 |
| `kw` | `int64 kw` | 窗口宽，必须为正。 | 无 |
| `kh` | `int64 kh` | 窗口高，必须为正。 | 无 |

**返回值**

`Result<Image, String>`。池化后的新图片，尺寸为 (width/kw, height/kh) 向下取整；参数不合法时返回错误。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.pool(mode, kw, kh);
```

**源码**：[source/std/image.mgl:263](../../source/std/image.mgl#L263)。

<a id="api-16"></a>

### Image::toGray（17）

**声明**

```magel
Result<List<uint8>, String> toGray();
```

**行为**

转为 8 位灰度。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`Result<List<uint8>, String>`。长度 width height 的灰度缓冲，每像素按 BT.601 亮度取值；缓冲长度不符时返回错误。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.toGray();
```

**源码**：[source/std/image.mgl:275](../../source/std/image.mgl#L275)。

<a id="api-17"></a>

### Image::fromGray（18）

**声明**

```magel
static Result<Image, String> fromGray(int64 width, int64 height, List<uint8> gray);
```

**行为**

由灰度缓冲还原 RGB 图片。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `width` | `int64 width` | 图片宽，单位像素，必须为正。 | 无 |
| `height` | `int64 height` | 图片高，单位像素，必须为正。 | 无 |
| `gray` | `List<uint8> gray` | 灰度缓冲，长度必须为 width height。 | 无 |

**返回值**

`Result<Image, String>`。三通道同值、透明度 255 的图片；缓冲长度不符时返回错误。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::image::Image::fromGray(width, height, gray);
```

**源码**：[source/std/image.mgl:289](../../source/std/image.mgl#L289)。

<a id="api-18"></a>

### Image::toARGB（19）

**声明**

```magel
Result<List<uint32>, String> toARGB();
```

**行为**

转为 ARGB8888。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`Result<List<uint32>, String>`。每像素一个 uint32 的列表，布局 (A<<24)|(R<<16)|(G<<8)|B；缓冲长度不符时返回错误。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.toARGB();
```

**源码**：[source/std/image.mgl:301](../../source/std/image.mgl#L301)。

<a id="api-19"></a>

### Image::fromARGB（20）

**声明**

```magel
static Result<Image, String> fromARGB(int64 width, int64 height, List<uint32> argb);
```

**行为**

由 ARGB8888 缓冲还原图片。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `width` | `int64 width` | 图片宽，单位像素，必须为正。 | 无 |
| `height` | `int64 height` | 图片高，单位像素，必须为正。 | 无 |
| `argb` | `List<uint32> argb` | 像素值列表，长度必须为 width height。 | 无 |

**返回值**

`Result<Image, String>`。还原后的图片；缓冲长度不符时返回错误。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::image::Image::fromARGB(width, height, argb);
```

**源码**：[source/std/image.mgl:315](../../source/std/image.mgl#L315)。

<a id="api-20"></a>

### Image::toRGB565（21）

**声明**

```magel
Result<List<uint16>, String> toRGB565();
```

**行为**

转为 RGB565。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`Result<List<uint16>, String>`。每像素一个 uint16 的列表，R 高 5 位、G 中 6 位、B 低 5 位；缓冲长度不符时返回错误。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.toRGB565();
```

**源码**：[source/std/image.mgl:327](../../source/std/image.mgl#L327)。

<a id="api-21"></a>

### Image::fromRGB565（22）

**声明**

```magel
static Result<Image, String> fromRGB565(int64 width, int64 height, List<uint16> rgb565);
```

**行为**

由 RGB565 缓冲还原图片。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `width` | `int64 width` | 图片宽，单位像素，必须为正。 | 无 |
| `height` | `int64 height` | 图片高，单位像素，必须为正。 | 无 |
| `rgb565` | `List<uint16> rgb565` | 像素值列表，长度必须为 width height。 | 无 |

**返回值**

`Result<Image, String>`。还原后的图片，5/6 位分量按高位复制扩展回 8 位；缓冲长度不符时返回错误。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::image::Image::fromRGB565(width, height, rgb565);
```

**源码**：[source/std/image.mgl:341](../../source/std/image.mgl#L341)。

<a id="api-22"></a>

### Image::otsuThreshold（23）

**声明**

```magel
Result<int64, String> otsuThreshold();
```

**行为**

用大津法求二值化阈值。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`Result<int64, String>`。使类间方差最大的灰度阈值，0..255；图片无效时返回错误。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.otsuThreshold();
```

**源码**：[source/std/image.mgl:353](../../source/std/image.mgl#L353)。

<a id="api-23"></a>

### Image::binarize（24）

**声明**

```magel
static Result<List<uint8>, String> binarize(int64 width, int64 height, List<uint8> gray, int64 threshold);
```

**行为**

按阈值二值化灰度缓冲。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `width` | `int64 width` | 图片宽，单位像素，必须为正。 | 无 |
| `height` | `int64 height` | 图片高，单位像素，必须为正。 | 无 |
| `gray` | `List<uint8> gray` | 灰度缓冲，长度必须为 width height。 | 无 |
| `threshold` | `int64 threshold` | 阈值，0..255；大于阈值给 255，否则给 0。 | 无 |

**返回值**

`Result<List<uint8>, String>`。二值化后的灰度缓冲；阈值越界或缓冲长度不符时返回错误。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::image::Image::binarize(width, height, gray, threshold);
```

**源码**：[source/std/image.mgl:369](../../source/std/image.mgl#L369)。

<a id="api-24"></a>

### Image::otsuBinarize（25）

**声明**

```magel
Result<List<uint8>, String> otsuBinarize();
```

**行为**

转灰度、大津法求阈值并二值化。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`Result<List<uint8>, String>`。0/255 灰度缓冲；任何一步失败时返回错误。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.otsuBinarize();
```

**源码**：[source/std/image.mgl:380](../../source/std/image.mgl#L380)。
