# std::algorithms — 通用算法

[返回库目录](README.md) · [全部符号](symbols.md)

## 引入

```magel
import std::algorithms;
```

## 公共约定与错误边界

需 import std::algorithms; 并使用完整限定名。Value 族操作列表值；sortValue、shuffleValue 返回副本。Reference 族直接操作连续存储，调用者保证 count 对应有效元素数；返回的引用不能越过缓冲区生命周期。排序比较使用 >；默认稳定插入排序，构建时 SIMPLE_SORT 可切换到不稳定希尔排序。二分查找要求已升序排列。

## 参考代码

完整程序：[example/samples/std_algorithms_extra.mgl](../../example/samples/std_algorithms_extra.mgl)。

```powershell
magel run example/samples/std_algorithms_extra.mgl
```

<details>
<summary>展开完整参考代码</summary>

```magel
/* std_algorithms_extra.mgl —— `algorithms` 的 map / filter / reduce / sum / count。
 *
 * 这一批和 `fold` / `all` / `any` 是同一套"回调式内建"：源码里写
 * `std::algorithms::mapValue<T>(表, 函数名)`，泛型预处理器只负责吃掉 `<T>`，
 * 真正的降级在 `LowerAlgorithmCall`，运行期落到 VM 的 `__vm_vec_map` 等等。
 *
 * 两条既有约束（都不是这批新名字引入的，改样例时别当成 bug）：
 *   ① 回调要写成**裸函数名**，而 `module X;` 里传裸函数名当 fptr 实参现在解析不到
 *      （连既有的 `all` / `fold` 也一样）—— 所以这份样例不写 `module` 声明；
 *   ② 顶层函数的总数/排列踩到某个值会让编译器把结构体方法的形参类型判错
 *      （报 `__parseFloat64Error` 找不到 `(int64)` 的重载）。这是**先于本批改动
 *      就存在**的缺陷，最小复现：普通样例 + 11 个未被调用的 `int64 padN(int64,int64)`。
 *      这份样例的回调个数已经避开了那个点位。
 */

import std::algorithms;

import std::text;

int64 doubled(int64 value) { return value * 2; }
float64 halved(int64 value) { return (float64)value / 2.0; }
String stars(int64 value) { return std::text::Text::repeat("*", value); }
int64 lengthOf(String text) { return len(text); }
bool odd(int64 value) { return value % 2 != 0; }
int64 addInt(int64 total, int64 value) { return total + value; }
float64 addReal(float64 total, float64 value) { return total + value; }

int64 main() {
    List<int64> values;
    values.push(3);
    values.push(1);
    values.push(4);
    values.push(1);
    values.push(5);

    /* map：结果表的元素类型由**回调的返回类型**定，`map<int64>` 只给一个类型实参。 */
    List<int64> tripled = std::algorithms::mapValue<int64>(values, doubled);
    List<float64> halves = std::algorithms::mapValue<int64>(values, halved);
    List<String> marks = std::algorithms::mapValue<int64>(values, stars);
    List<int64> lengths = std::algorithms::mapValue<String>(marks, lengthOf);

    /* filter：留下判定为真的元素，元素类型不变。 */
    List<int64> odds = std::algorithms::filterValue<int64>(values, odd);

    println("map    {} {} {}", tripled.len(), tripled[0], tripled[4]);
    println("map    {} {} {}", halves[0], marks[2], lengths[4]);
    println("filter {} {} {}", odds.len(), odds[0], odds[3]);

    /* reduce 两种写法：两参拿第一个元素当初值，三参等同 fold。 */
    println("reduce {} {}", std::algorithms::reduceValue<int64>(values, addInt),
    std::algorithms::reduceValue<int64>(values, 100, addInt));
    /* sum / count：count 的第二参是 fptr 就按判定数，是值就按相等数。 */
    println("sum    {} {}", std::algorithms::sumValue<int64>(values),
    std::algorithms::sumValue<float64>(halves));
    println("count  {} {}", std::algorithms::countValue<int64>(values, 1),
    std::algorithms::countValue<int64>(values, odd));

    List<float64> reals;
    reals.push(1.5);
    reals.push(2.25);
    println("float64    {}", std::algorithms::reduceValue<float64>(reals, addReal));
    return 0;
}
```

</details>

<details>
<summary>示例输出</summary>

```text
map    5 6 10
map    1.5 **** 5
filter 4 3 5
reduce 14 114
sum    14 7
count  2 4
float64    3.75
```

</details>

聚合入口的具体操作参见对应子模块；本页类型/方法的调用形式列在下方。

## 函数索引

| 名称 | 声明 |
| --- | --- |
| [choiceValue](#api-0) | `T choiceValue<T>(std::lang::List<T> items, uint64 seed)` |
| [shuffleValue](#api-1) | `std::lang::List<T> shuffleValue<T>(std::lang::List<T> items, uint64 seed)` |
| [shuffleReference](#api-2) | `void shuffleReference<T>(T mut& data, int64 count, uint64 seed)` |
| [zipValue](#api-3) | `std::lang::List<(T, U)> zipValue<T, U>(std::lang::List<T> left, std::lang::List<U> right)` |
| [sortReference](#api-4) | `void sortReference<T>(T mut& data, int64 count)` |
| [reverseReference](#api-5) | `void reverseReference<T>(T mut& data, int64 count)` |
| [findReference](#api-6) | `int64 findReference<T>(T const& data, int64 count, T value)` |
| [binarySearchReference](#api-7) | `int64 binarySearchReference<T>(T const& data, int64 count, T value)` |
| [minReference](#api-8) | `Option<T mut&> minReference<T>(T mut& data, int64 count)` |
| [maxReference](#api-9) | `Option<T mut&> maxReference<T>(T mut& data, int64 count)` |
| [countReference](#api-10) | `int64 countReference<T>(T const& data, int64 count, T value)` |
| [sumReference](#api-11) | `T sumReference<T>(T mut& data, int64 count)` |
| [sortValue](#api-12) | `List<T> sortValue<T>(List<T> items)` |
| [findValue](#api-13) | `int64 findValue<T>(List<T> items,T value)` |
| [binarySearchValue](#api-14) | `int64 binarySearchValue<T>(List<T> items,T value)` |
| [minValue](#api-15) | `T minValue<T>(T first,T rest...)` |
| [maxValue](#api-16) | `T maxValue<T>(T first,T rest...)` |
| [clampValue](#api-17) | `T clampValue<T>(T value,T low,T high)` |
| [allValue](#api-18) | `bool allValue<T>(List<T> items,bool fn(T) predicate)` |
| [anyValue](#api-19) | `bool anyValue<T>(List<T> items,bool fn(T) predicate)` |
| [foldValue](#api-20) | `T foldValue<T>(List<T> items,T initial,T fn(T,T) folder)` |
| [mapValue](#api-21) | `List<R> mapValue<T>(List<T> items,R fn(T) mapper)` |
| [filterValue](#api-22) | `List<T> filterValue<T>(List<T> items,bool fn(T) predicate)` |
| [reduceValue](#api-23) | `T reduceValue<T>(List<T> items,T fn(T,T) folder)` |
| [reduceValue](#api-24) | `T reduceValue<T>(List<T> items,T initial,T fn(T,T) folder)` |
| [sumValue](#api-25) | `T sumValue<T>(List<T> items)` |
| [countValue](#api-26) | `int64 countValue<T>(List<T> items,T value)` |
| [countValue](#api-27) | `int64 countValue<T>(List<T> items,bool fn(T) predicate)` |

<a id="api-0"></a>

### choiceValue（1）

**声明**

```magel
T choiceValue<T>(std::lang::List<T> items, uint64 seed);
```

**行为**

从表里随机取一个元素；空表直接停机（和 `at` 越界一个口径）。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `items` | `std::lang::List<T> items` | 参与处理的列表元素 | 无 |
| `seed` | `uint64 seed` | 随机种子 | 无 |

**返回值**

`T`。从表里随机取一个元素；空表直接停机（和 `at` 越界一个口径）。

**错误与边界**

遵循本页“公共约定与错误边界”。
显式前置检查：`items.len() > 0, "std::algorithms::choiceValue 不能作用在空表上"`。

**调用形式**

```magel
std::algorithms::choiceValue<T>(items, seed);
```

**源码**：[source/magel_generic.hpp:1723](../../source/magel_generic.hpp#L1723)。

**完整示例**：[example/samples/std_algorithms_extra.mgl](../../example/samples/std_algorithms_extra.mgl)。

<a id="api-1"></a>

### shuffleValue（2）

**声明**

```magel
std::lang::List<T> shuffleValue<T>(std::lang::List<T> items, uint64 seed);
```

**行为**

Fisher-Yates 洗牌，返回**打乱后的副本**（原表不动）。 从后往前逐个和前面 `[0, i]` 里的一个随机位置交换；同一个 `seed` 结果可复现。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `items` | `std::lang::List<T> items` | 参与处理的列表元素 | 无 |
| `seed` | `uint64 seed` | 随机种子 | 无 |

**返回值**

`std::lang::List<T>`。Fisher-Yates 洗牌，返回**打乱后的副本**（原表不动）。 从后往前逐个和前面 `[0, i]` 里的一个随机位置交换；同一个 `seed` 结果可复现。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::algorithms::shuffleValue<T>(items, seed);
```

**源码**：[source/magel_generic.hpp:1732](../../source/magel_generic.hpp#L1732)。

**完整示例**：[example/samples/std_algorithms_extra.mgl](../../example/samples/std_algorithms_extra.mgl)。

<a id="api-2"></a>

### shuffleReference（3）

**声明**

```magel
void shuffleReference<T>(T mut& data, int64 count, uint64 seed);
```

**行为**

Fisher-Yates 洗牌的**引用形式**：直接在 `data` 里 `count` 个元素上原地打乱。 同一个 `seed` 结果可复现，和 `shuffleValue` 用同一条 LCG。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `data` | `T mut& data` | 连续存储的首元素引用，调用者保证范围有效 | 无 |
| `count` | `int64 count` | 元素或字节数量，具体单位见行为说明 | 无 |
| `seed` | `uint64 seed` | 随机种子 | 无 |

**返回值**

无返回值。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::algorithms::shuffleReference<T>(data, count, seed);
```

**源码**：[source/magel_generic.hpp:1752](../../source/magel_generic.hpp#L1752)。

**完整示例**：[example/samples/std_algorithms_extra.mgl](../../example/samples/std_algorithms_extra.mgl)。

<a id="api-3"></a>

### zipValue（4）

**声明**

```magel
std::lang::List<(T, U)> zipValue<T, U>(std::lang::List<T> left, std::lang::List<U> right);
```

**行为**

按较短列表长度配对元素并返回元组列表，较长列表的多余元素忽略。

Pair corresponding elements; excess elements in the longer list are ignored.

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `left` | `std::lang::List<T> left` | 左操作数或区间左端 | 无 |
| `right` | `std::lang::List<U> right` | 右操作数或区间右端 | 无 |

**返回值**

`std::lang::List<(T, U)>`。按较短列表长度配对元素并返回元组列表，较长列表的多余元素忽略。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::algorithms::zipValue<T, U>(left, right);
```

**源码**：[source/magel_generic.hpp:1766](../../source/magel_generic.hpp#L1766)。

**完整示例**：[example/samples/std_algorithms_extra.mgl](../../example/samples/std_algorithms_extra.mgl)。

<a id="api-4"></a>

### sortReference（5）

**声明**

```magel
void sortReference<T>(T mut& data, int64 count);
```

**行为**

原地排序，O(1) 额外空间：`data` 里 `count` 个元素直接排好。 默认是插入排序（稳定）；构建编译器时定义 `SIMPLE_SORT` 改用希尔排序（把间距逐步 缩到 1，同样就地、不额外分配，但**不稳定**）。VM 里的列表排序 `sortValue` 也吃同一个开关。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `data` | `T mut& data` | 连续存储的首元素引用，调用者保证范围有效 | 无 |
| `count` | `int64 count` | 元素或字节数量，具体单位见行为说明 | 无 |

**返回值**

无返回值。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::algorithms::sortReference<T>(data, count);
```

**源码**：[source/magel_generic.hpp:1794](../../source/magel_generic.hpp#L1794)。

**完整示例**：[example/samples/std_algorithms_extra.mgl](../../example/samples/std_algorithms_extra.mgl)。

<a id="api-5"></a>

### reverseReference（6）

**声明**

```magel
void reverseReference<T>(T mut& data, int64 count);
```

**行为**

原地反转 `[0, count)`。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `data` | `T mut& data` | 连续存储的首元素引用，调用者保证范围有效 | 无 |
| `count` | `int64 count` | 元素或字节数量，具体单位见行为说明 | 无 |

**返回值**

无返回值。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::algorithms::reverseReference<T>(data, count);
```

**源码**：[source/magel_generic.hpp:1799](../../source/magel_generic.hpp#L1799)。

**完整示例**：[example/samples/std_algorithms_extra.mgl](../../example/samples/std_algorithms_extra.mgl)。

<a id="api-6"></a>

### findReference（7）

**声明**

```magel
int64 findReference<T>(T const& data, int64 count, T value);
```

**行为**

线性查找，返回下标；找不到返回 -1。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `data` | `T const& data` | 连续存储的首元素引用，调用者保证范围有效 | 无 |
| `count` | `int64 count` | 元素或字节数量，具体单位见行为说明 | 无 |
| `value` | `T value` | 输入值或写入的新值 | 无 |

**返回值**

`int64`。线性查找，返回下标；找不到返回 -1。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::algorithms::findReference<T>(data, count, value);
```

**源码**：[source/magel_generic.hpp:1812](../../source/magel_generic.hpp#L1812)。

**完整示例**：[example/samples/std_algorithms_extra.mgl](../../example/samples/std_algorithms_extra.mgl)。

<a id="api-7"></a>

### binarySearchReference（8）

**声明**

```magel
int64 binarySearchReference<T>(T const& data, int64 count, T value);
```

**行为**

在按 `>` 升序排好的区间上二分，返回下标；找不到返回 -1。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `data` | `T const& data` | 连续存储的首元素引用，调用者保证范围有效 | 无 |
| `count` | `int64 count` | 元素或字节数量，具体单位见行为说明 | 无 |
| `value` | `T value` | 输入值或写入的新值 | 无 |

**返回值**

`int64`。在按 `>` 升序排好的区间上二分，返回下标；找不到返回 -1。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::algorithms::binarySearchReference<T>(data, count, value);
```

**源码**：[source/magel_generic.hpp:1822](../../source/magel_generic.hpp#L1822)。

**完整示例**：[example/samples/std_algorithms_extra.mgl](../../example/samples/std_algorithms_extra.mgl)。

<a id="api-8"></a>

### minReference（9）

**声明**

```magel
Option<T mut&> minReference<T>(T mut& data, int64 count);
```

**行为**

最小元素的地址；空区间返回 `None`。拿着它可以直接改元素。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `data` | `T mut& data` | 连续存储的首元素引用，调用者保证范围有效 | 无 |
| `count` | `int64 count` | 元素或字节数量，具体单位见行为说明 | 无 |

**返回值**

`Option<T mut&>`。最小元素的地址；空区间返回 `None`。拿着它可以直接改元素。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::algorithms::minReference<T>(data, count);
```

**源码**：[source/magel_generic.hpp:1834](../../source/magel_generic.hpp#L1834)。

**完整示例**：[example/samples/std_algorithms_extra.mgl](../../example/samples/std_algorithms_extra.mgl)。

<a id="api-9"></a>

### maxReference（10）

**声明**

```magel
Option<T mut&> maxReference<T>(T mut& data, int64 count);
```

**行为**

最大元素的地址；空区间返回 `None`。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `data` | `T mut& data` | 连续存储的首元素引用，调用者保证范围有效 | 无 |
| `count` | `int64 count` | 元素或字节数量，具体单位见行为说明 | 无 |

**返回值**

`Option<T mut&>`。最大元素的地址；空区间返回 `None`。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::algorithms::maxReference<T>(data, count);
```

**源码**：[source/magel_generic.hpp:1846](../../source/magel_generic.hpp#L1846)。

**完整示例**：[example/samples/std_algorithms_extra.mgl](../../example/samples/std_algorithms_extra.mgl)。

<a id="api-10"></a>

### countReference（11）

**声明**

```magel
int64 countReference<T>(T const& data, int64 count, T value);
```

**行为**

等于 `value` 的元素个数。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `data` | `T const& data` | 连续存储的首元素引用，调用者保证范围有效 | 无 |
| `count` | `int64 count` | 元素或字节数量，具体单位见行为说明 | 无 |
| `value` | `T value` | 输入值或写入的新值 | 无 |

**返回值**

`int64`。等于 `value` 的元素个数。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::algorithms::countReference<T>(data, count, value);
```

**源码**：[source/magel_generic.hpp:1858](../../source/magel_generic.hpp#L1858)。

**完整示例**：[example/samples/std_algorithms_extra.mgl](../../example/samples/std_algorithms_extra.mgl)。

<a id="api-11"></a>

### sumReference（12）

**声明**

```magel
T sumReference<T>(T mut& data, int64 count);
```

**行为**

求和；空区间直接停机（泛型里没有通用的"零值"可造）。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `data` | `T mut& data` | 连续存储的首元素引用，调用者保证范围有效 | 无 |
| `count` | `int64 count` | 元素或字节数量，具体单位见行为说明 | 无 |

**返回值**

`T`。求和；空区间直接停机（泛型里没有通用的"零值"可造）。

**错误与边界**

遵循本页“公共约定与错误边界”。
显式前置检查：`count > 0, "std::algorithms::sumReference 需要至少一个元素"`。

**调用形式**

```magel
std::algorithms::sumReference<T>(data, count);
```

**源码**：[source/magel_generic.hpp:1869](../../source/magel_generic.hpp#L1869)。

**完整示例**：[example/samples/std_algorithms_extra.mgl](../../example/samples/std_algorithms_extra.mgl)。

<a id="api-12"></a>

### sortValue（13）

**声明**

```magel
List<T> sortValue<T>(List<T> items);
```

**行为**

返回升序排序副本，原列表保持不变。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `items` | `List<T> items` | 参与处理的列表元素 | 无 |

**返回值**

`List<T>`。返回升序排序副本，原列表保持不变。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::algorithms::sortValue<T>(items);
```

**源码**：[source/epilogue/boxing.cpp:397](../../source/epilogue/boxing.cpp#L397)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_algorithms_extra.mgl](../../example/samples/std_algorithms_extra.mgl)。

<a id="api-13"></a>

### findValue（14）

**声明**

```magel
int64 findValue<T>(List<T> items,T value);
```

**行为**

线性查找，返回首个下标；未找到为 -1。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `items` | `List<T> items` | 参与处理的列表元素 | 无 |
| `value` | `T value` | 输入值或写入的新值 | 无 |

**返回值**

`int64`。线性查找，返回首个下标；未找到为 -1。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::algorithms::findValue<T>(items, value);
```

**源码**：[source/epilogue/boxing.cpp:425](../../source/epilogue/boxing.cpp#L425)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_algorithms_extra.mgl](../../example/samples/std_algorithms_extra.mgl)。

<a id="api-14"></a>

### binarySearchValue（15）

**声明**

```magel
int64 binarySearchValue<T>(List<T> items,T value);
```

**行为**

在已升序排序的列表二分查找，未找到为 -1。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `items` | `List<T> items` | 参与处理的列表元素 | 无 |
| `value` | `T value` | 输入值或写入的新值 | 无 |

**返回值**

`int64`。在已升序排序的列表二分查找，未找到为 -1。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::algorithms::binarySearchValue<T>(items, value);
```

**源码**：[source/epilogue/boxing.cpp:425](../../source/epilogue/boxing.cpp#L425)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_algorithms_extra.mgl](../../example/samples/std_algorithms_extra.mgl)。

<a id="api-15"></a>

### minValue（16）

**声明**

```magel
T minValue<T>(T first,T rest...);
```

**行为**

返回参数中的最小值，至少一个实参。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `first` | `T first` | 第一分量 | 无 |
| `rest` | `T rest...` | 后续变长实参 | 无 |

**返回值**

`T`。返回参数中的最小值，至少一个实参。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::algorithms::minValue<T>(first, rest);
```

**源码**：[source/epilogue/boxing.cpp:279](../../source/epilogue/boxing.cpp#L279)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_algorithms_extra.mgl](../../example/samples/std_algorithms_extra.mgl)。

<a id="api-16"></a>

### maxValue（17）

**声明**

```magel
T maxValue<T>(T first,T rest...);
```

**行为**

返回参数中的最大值，至少一个实参。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `first` | `T first` | 第一分量 | 无 |
| `rest` | `T rest...` | 后续变长实参 | 无 |

**返回值**

`T`。返回参数中的最大值，至少一个实参。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::algorithms::maxValue<T>(first, rest);
```

**源码**：[source/epilogue/boxing.cpp:279](../../source/epilogue/boxing.cpp#L279)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_algorithms_extra.mgl](../../example/samples/std_algorithms_extra.mgl)。

<a id="api-17"></a>

### clampValue（18）

**声明**

```magel
T clampValue<T>(T value,T low,T high);
```

**行为**

将 value 限制到闭区间 [low,high]。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `value` | `T value` | 输入值或写入的新值 | 无 |
| `low` | `T low` | 闭区间下界 | 无 |
| `high` | `T high` | 闭区间上界 | 无 |

**返回值**

`T`。将 value 限制到闭区间 [low,high]。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::algorithms::clampValue<T>(value, low, high);
```

**源码**：[source/epilogue/boxing.cpp:397](../../source/epilogue/boxing.cpp#L397)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_algorithms_extra.mgl](../../example/samples/std_algorithms_extra.mgl)。

<a id="api-18"></a>

### allValue（19）

**声明**

```magel
bool allValue<T>(List<T> items,bool fn(T) predicate);
```

**行为**

判断全部元素是否满足谓词，空列表为 true。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `items` | `List<T> items` | 参与处理的列表元素 | 无 |
| `predicate` | `bool fn(T) predicate` | 判断回调，返回 bool | 无 |

**返回值**

`bool`。判断全部元素是否满足谓词，空列表为 true。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::algorithms::allValue<T>(items, predicate);
```

**源码**：[source/epilogue/boxing.cpp:436](../../source/epilogue/boxing.cpp#L436)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_algorithms_extra.mgl](../../example/samples/std_algorithms_extra.mgl)。

<a id="api-19"></a>

### anyValue（20）

**声明**

```magel
bool anyValue<T>(List<T> items,bool fn(T) predicate);
```

**行为**

判断是否有元素满足谓词，空列表为 false。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `items` | `List<T> items` | 参与处理的列表元素 | 无 |
| `predicate` | `bool fn(T) predicate` | 判断回调，返回 bool | 无 |

**返回值**

`bool`。判断是否有元素满足谓词，空列表为 false。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::algorithms::anyValue<T>(items, predicate);
```

**源码**：[source/epilogue/boxing.cpp:436](../../source/epilogue/boxing.cpp#L436)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_algorithms_extra.mgl](../../example/samples/std_algorithms_extra.mgl)。

<a id="api-20"></a>

### foldValue（21）

**声明**

```magel
T foldValue<T>(List<T> items,T initial,T fn(T,T) folder);
```

**行为**

从 initial 开始顺序累积。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `items` | `List<T> items` | 参与处理的列表元素 | 无 |
| `initial` | `T initial` | 迭代/累积初值 | 无 |
| `folder` | `T fn(T,T) folder` | 按签名接收累积值与当前元素的折叠回调 | 无 |

**返回值**

`T`。从 initial 开始顺序累积。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::algorithms::foldValue<T>(items, initial, folder);
```

**源码**：[source/epilogue/boxing.cpp:397](../../source/epilogue/boxing.cpp#L397)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_algorithms_extra.mgl](../../example/samples/std_algorithms_extra.mgl)。

<a id="api-21"></a>

### mapValue（22）

**声明**

```magel
List<R> mapValue<T>(List<T> items,R fn(T) mapper);
```

**行为**

对各元素调用 mapper，返回新列表；R 由回调结果类型推导。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `items` | `List<T> items` | 参与处理的列表元素 | 无 |
| `mapper` | `R fn(T) mapper` | 转换回调，签名决定输入和输出类型 | 无 |

**返回值**

`List<R>`。对各元素调用 mapper，返回新列表；R 由回调结果类型推导。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::algorithms::mapValue<T>(items, mapper);
```

**源码**：[source/epilogue/boxing.cpp:314](../../source/epilogue/boxing.cpp#L314)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_algorithms_extra.mgl](../../example/samples/std_algorithms_extra.mgl)。

<a id="api-22"></a>

### filterValue（23）

**声明**

```magel
List<T> filterValue<T>(List<T> items,bool fn(T) predicate);
```

**行为**

返回满足 predicate 的元素列表副本。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `items` | `List<T> items` | 参与处理的列表元素 | 无 |
| `predicate` | `bool fn(T) predicate` | 判断回调，返回 bool | 无 |

**返回值**

`List<T>`。返回满足 predicate 的元素列表副本。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::algorithms::filterValue<T>(items, predicate);
```

**源码**：[source/epilogue/boxing.cpp:336](../../source/epilogue/boxing.cpp#L336)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_algorithms_extra.mgl](../../example/samples/std_algorithms_extra.mgl)。

<a id="api-23"></a>

### reduceValue（24）

**声明**

```magel
T reduceValue<T>(List<T> items,T fn(T,T) folder);
```

**行为**

用首元素作初值累积；空列表报错。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `items` | `List<T> items` | 参与处理的列表元素 | 无 |
| `folder` | `T fn(T,T) folder` | 按签名接收累积值与当前元素的折叠回调 | 无 |

**返回值**

`T`。用首元素作初值累积；空列表报错。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::algorithms::reduceValue<T>(items, folder);
```

**源码**：[source/epilogue/boxing.cpp:374](../../source/epilogue/boxing.cpp#L374)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_algorithms_extra.mgl](../../example/samples/std_algorithms_extra.mgl)。

<a id="api-24"></a>

### reduceValue（25）

**声明**

```magel
T reduceValue<T>(List<T> items,T initial,T fn(T,T) folder);
```

**行为**

从显式 initial 累积，等价 foldValue。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `items` | `List<T> items` | 参与处理的列表元素 | 无 |
| `initial` | `T initial` | 迭代/累积初值 | 无 |
| `folder` | `T fn(T,T) folder` | 按签名接收累积值与当前元素的折叠回调 | 无 |

**返回值**

`T`。从显式 initial 累积，等价 foldValue。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::algorithms::reduceValue<T>(items, initial, folder);
```

**源码**：[source/epilogue/boxing.cpp:374](../../source/epilogue/boxing.cpp#L374)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_algorithms_extra.mgl](../../example/samples/std_algorithms_extra.mgl)。

<a id="api-25"></a>

### sumValue（26）

**声明**

```magel
T sumValue<T>(List<T> items);
```

**行为**

对数值列表求和。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `items` | `List<T> items` | 参与处理的列表元素 | 无 |

**返回值**

`T`。对数值列表求和。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::algorithms::sumValue<T>(items);
```

**源码**：[source/epilogue/boxing.cpp:301](../../source/epilogue/boxing.cpp#L301)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_algorithms_extra.mgl](../../example/samples/std_algorithms_extra.mgl)。

<a id="api-26"></a>

### countValue（27）

**声明**

```magel
int64 countValue<T>(List<T> items,T value);
```

**行为**

统计等于 value 的元素数量。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `items` | `List<T> items` | 参与处理的列表元素 | 无 |
| `value` | `T value` | 输入值或写入的新值 | 无 |

**返回值**

`int64`。统计等于 value 的元素数量。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::algorithms::countValue<T>(items, value);
```

**源码**：[source/epilogue/boxing.cpp:351](../../source/epilogue/boxing.cpp#L351)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_algorithms_extra.mgl](../../example/samples/std_algorithms_extra.mgl)。

<a id="api-27"></a>

### countValue（28）

**声明**

```magel
int64 countValue<T>(List<T> items,bool fn(T) predicate);
```

**行为**

统计满足 predicate 的元素数量。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `items` | `List<T> items` | 参与处理的列表元素 | 无 |
| `predicate` | `bool fn(T) predicate` | 判断回调，返回 bool | 无 |

**返回值**

`int64`。统计满足 predicate 的元素数量。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::algorithms::countValue<T>(items, predicate);
```

**源码**：[source/epilogue/boxing.cpp:351](../../source/epilogue/boxing.cpp#L351)（编译器内建，签名为等价调用形式）。

**完整示例**：[example/samples/std_algorithms_extra.mgl](../../example/samples/std_algorithms_extra.mgl)。
