# std::math — 标量数学

[返回库目录](README.md) · [全部符号](symbols.md)

## 引入

```magel
import std::math;
```

## 公共约定与错误边界

三角函数使用弧度，浮点运算转发到宿主 C++ 数学函数，定义域错误可产生 NaN/无穷。round 的中点远离零，trunc 向零。abs/min/max 是泛型入口，min/max 接受任意数量实参；折叠按成对原始顺序做（min(a,b,c)=min(min(a,b),c)），不等同于 fmin/fmax 的 NaN 规则：NaN 与任何值比较都为假，min(NaN,2.0) 仍是 2.0。clamp 保留 int64/float64 两份重载，要求 low<=high，否则停机。整数 abs 的最小负数沿用整数回绕语义。常数使用 std::lang::Math::PI/E。

## 参考代码

完整程序：[example/samples/std_math_clock_random.mgl](../../example/samples/std_math_clock_random.mgl)。

```powershell
magel run example/samples/std_math_clock_random.mgl
```

<details>
<summary>展开完整参考代码</summary>

```magel
/* std_math_clock_random.mgl —— 第二档：Math 补齐、Clock、Random 扩展。
 *
 * 三条路线各不相同，改这块前先分清：
 *   * Math 的 float64 取整 / 判定走**宿主胶水**（glue/std_math.hpp 绑 <cmath>）；
 *   * Math 的 int64 版是**Magel 重载转发到宿主胶水**（写在同一模块里，实参完全匹配优先于 int64→float64 放宽）；
 *   * Clock 是宿主胶水（chrono 的 steady_clock）；
 *   * Random 的 nextRange / nextBool 是 宿主胶水的薄封装。
 *
 * `choice` / `shuffle` 不在 `Random::` 下：Magel 没有泛型方法，而 `std::random::Random` 这个
 * 名字已经被同名的 struct 占着（再开同名模块会让 `Random::withSeed` 找不到）。
 * 它们放在 `algorithms`，调用写 `std::algorithms::choiceValue<int64>(表, 种子)`。
 */

import std::random;

import std::math;
import std::algorithms;

import std::clock;

int64 main() {
    /* --- Math：float64 取整 / 取余 / 特殊值判定（宿主 <cmath>） --- */
    println("{} {} {} {}", std::math::floor(2.7), std::math::ceil(2.1),
    std::math::round(2.5), std::math::trunc(-2.7));
    println("{} {}", std::math::fmod(7.5, 2.0), std::math::fmod(-7.5, 2.0));

    /* Magel 里浮点除零是运行期错误，所以 NaN / Inf 从宿主函数拿：
     * sqrt(-1) = NaN、ln(0) = -Inf。 */
    float64 notNumber = std::math::sqrt(-1.0);
    float64 infinite = std::math::ln(0.0);
    println("{} {} {} {}", std::math::isNaN(notNumber), std::math::isNaN(1.0),
    std::math::isInf(infinite), std::math::isInf(1.0));

    /* --- Math：int64 版（Magel 重载转发到宿主胶水）+ float64 版 abs 仍然可用 --- */
    println("{} {} {} {}", std::math::abs(-5), std::math::min(3, 8),
    std::math::max(3, 8), std::math::clamp(12, 0, 10));
    println("{}", std::math::abs(-2.5));

    /* --- Clock：单调时钟。读数本身不可复现，只断言"拿到了、且不倒退"。 --- */
    int64 before = std::clock::nowNanos();
    int64 millis = std::clock::nowMs();
    int64 after = std::clock::nowNanos();
    println("{} {} {}", before > 0, millis > 0, after >= before);

    /* --- Random：分布 --- */
    std::random::Random rng = std::random::Random::withSeed((uint64)12345);
    float64 ranged = rng.nextRange(10.0, 20.0);
    int64 drawn = rng.nextInt(0, 10);
    bool coin = rng.nextBool();
    /* 固定种子 -> 固定序列（和 std_parse_random_path_memory 一个口径：钉住随机输出）。 */
    println("{} {} {}", ranged, drawn, coin);
    println("{} {}", ranged >= 10.0, ranged < 20.0);

    /* --- 取一个 / 洗一副（种子自己给，结果可复现） --- */
    List<int64> items;
    items.push(1);
    items.push(2);
    items.push(3);
    /* 种子 0：`0 % 3 == 0`，无论随机数怎么算都取到第 0 个。 */
    println("{}", std::algorithms::choiceValue<int64>(items, (uint64)0));

    List<int64> mixed = std::algorithms::shuffleValue<int64>(items, (uint64)4242);
    println("{} {} {}", mixed.len(),
    mixed[0] + mixed[1] + mixed[2],   /* 洗牌不改总和 */
    items[0]);                        /* 原表不动 */

    List<int64> again = std::algorithms::shuffleValue<int64>(items, (uint64)4242);
    println("{} {} {}", again[0] == mixed[0], again[1] == mixed[1],
    again[2] == mixed[2]);
    return 0;
}
```

</details>

<details>
<summary>示例输出</summary>

```text
2 3 3 -2
1.5 -1.5
1 0 1 0
5 3 8 10
2.5
1 1 1
11.1 3 1
1 1
1
3 6 1
1 1 1
```

</details>

聚合入口的具体操作参见对应子模块；本页类型/方法的调用形式列在下方。

## 函数索引

| 名称 | 声明 |
| --- | --- |
| [sin](#api-0) | `float64 sin(float64 value)` |
| [cos](#api-1) | `float64 cos(float64 value)` |
| [tan](#api-2) | `float64 tan(float64 value)` |
| [asin](#api-3) | `float64 asin(float64 value)` |
| [acos](#api-4) | `float64 acos(float64 value)` |
| [atan](#api-5) | `float64 atan(float64 value)` |
| [exp](#api-6) | `float64 exp(float64 value)` |
| [ln](#api-7) | `float64 ln(float64 value)` |
| [log10](#api-8) | `float64 log10(float64 value)` |
| [pow](#api-9) | `float64 pow(float64 value, float64 exponent)` |
| [sqrt](#api-10) | `float64 sqrt(float64 value)` |
| [floor](#api-11) | `float64 floor(float64 value)` |
| [ceil](#api-12) | `float64 ceil(float64 value)` |
| [round](#api-13) | `float64 round(float64 value)` |
| [trunc](#api-14) | `float64 trunc(float64 value)` |
| [fmod](#api-15) | `float64 fmod(float64 value, float64 divisor)` |
| [isNaN](#api-16) | `bool isNaN(float64 value)` |
| [isInf](#api-17) | `bool isInf(float64 value)` |
| [abs](#api-18) | `T abs<T>(T value)` |
| [min](#api-19) | `T min<T>(T... values)` |
| [max](#api-20) | `T max<T>(T... values)` |
| [clamp](#api-21) | `int64 clamp(int64 value, int64 low, int64 high)` |
| [clamp](#api-22) | `float64 clamp(float64 value, float64 low, float64 high)` |

<a id="api-0"></a>

### sin（1）

**声明**

```magel
float64 sin(float64 value);
```

**行为**

计算正弦。

编译器提供只读 float64 常数 std::lang::Math::PI 与 std::lang::Math::E。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `value` | `float64 value` | 输入值或写入的新值 | 无 |

**返回值**

`float64`。计算正弦。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::math::sin(value);
```

**源码**：[source/std/math.mgl:18](../../source/std/math.mgl#L18)。

**完整示例**：[example/samples/std_math_clock_random.mgl](../../example/samples/std_math_clock_random.mgl)。

<a id="api-1"></a>

### cos（2）

**声明**

```magel
float64 cos(float64 value);
```

**行为**

计算余弦。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `value` | `float64 value` | 输入值或写入的新值 | 无 |

**返回值**

`float64`。计算余弦。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::math::cos(value);
```

**源码**：[source/std/math.mgl:19](../../source/std/math.mgl#L19)。

**完整示例**：[example/samples/std_math_clock_random.mgl](../../example/samples/std_math_clock_random.mgl)。

<a id="api-2"></a>

### tan（3）

**声明**

```magel
float64 tan(float64 value);
```

**行为**

计算正切。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `value` | `float64 value` | 输入值或写入的新值 | 无 |

**返回值**

`float64`。计算正切。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::math::tan(value);
```

**源码**：[source/std/math.mgl:20](../../source/std/math.mgl#L20)。

**完整示例**：[example/samples/std_math_clock_random.mgl](../../example/samples/std_math_clock_random.mgl)。

<a id="api-3"></a>

### asin（4）

**声明**

```magel
float64 asin(float64 value);
```

**行为**

计算反正弦，结果为弧度。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `value` | `float64 value` | 输入值或写入的新值 | 无 |

**返回值**

`float64`。计算反正弦，结果为弧度。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::math::asin(value);
```

**源码**：[source/std/math.mgl:21](../../source/std/math.mgl#L21)。

**完整示例**：[example/samples/std_math_clock_random.mgl](../../example/samples/std_math_clock_random.mgl)。

<a id="api-4"></a>

### acos（5）

**声明**

```magel
float64 acos(float64 value);
```

**行为**

计算反余弦，结果为弧度。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `value` | `float64 value` | 输入值或写入的新值 | 无 |

**返回值**

`float64`。计算反余弦，结果为弧度。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::math::acos(value);
```

**源码**：[source/std/math.mgl:22](../../source/std/math.mgl#L22)。

**完整示例**：[example/samples/std_math_clock_random.mgl](../../example/samples/std_math_clock_random.mgl)。

<a id="api-5"></a>

### atan（6）

**声明**

```magel
float64 atan(float64 value);
```

**行为**

计算反正切，结果为弧度。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `value` | `float64 value` | 输入值或写入的新值 | 无 |

**返回值**

`float64`。计算反正切，结果为弧度。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::math::atan(value);
```

**源码**：[source/std/math.mgl:23](../../source/std/math.mgl#L23)。

**完整示例**：[example/samples/std_math_clock_random.mgl](../../example/samples/std_math_clock_random.mgl)。

<a id="api-6"></a>

### exp（7）

**声明**

```magel
float64 exp(float64 value);
```

**行为**

计算 e 的 value 次幂。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `value` | `float64 value` | 输入值或写入的新值 | 无 |

**返回值**

`float64`。计算 e 的 value 次幂。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::math::exp(value);
```

**源码**：[source/std/math.mgl:24](../../source/std/math.mgl#L24)。

**完整示例**：[example/samples/std_math_clock_random.mgl](../../example/samples/std_math_clock_random.mgl)。

<a id="api-7"></a>

### ln（8）

**声明**

```magel
float64 ln(float64 value);
```

**行为**

计算自然对数。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `value` | `float64 value` | 输入值或写入的新值 | 无 |

**返回值**

`float64`。计算自然对数。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::math::ln(value);
```

**源码**：[source/std/math.mgl:25](../../source/std/math.mgl#L25)。

**完整示例**：[example/samples/std_math_clock_random.mgl](../../example/samples/std_math_clock_random.mgl)。

<a id="api-8"></a>

### log10（9）

**声明**

```magel
float64 log10(float64 value);
```

**行为**

计算以 10 为底的对数。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `value` | `float64 value` | 输入值或写入的新值 | 无 |

**返回值**

`float64`。计算以 10 为底的对数。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::math::log10(value);
```

**源码**：[source/std/math.mgl:26](../../source/std/math.mgl#L26)。

**完整示例**：[example/samples/std_math_clock_random.mgl](../../example/samples/std_math_clock_random.mgl)。

<a id="api-9"></a>

### pow（10）

**声明**

```magel
float64 pow(float64 value, float64 exponent);
```

**行为**

计算 value 的 exponent 次幂。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `value` | `float64 value` | 输入值或写入的新值 | 无 |
| `exponent` | `float64 exponent` | 指数 | 无 |

**返回值**

`float64`。计算 value 的 exponent 次幂。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::math::pow(value, exponent);
```

**源码**：[source/std/math.mgl:27](../../source/std/math.mgl#L27)。

**完整示例**：[example/samples/std_math_clock_random.mgl](../../example/samples/std_math_clock_random.mgl)。

<a id="api-10"></a>

### sqrt（11）

**声明**

```magel
float64 sqrt(float64 value);
```

**行为**

计算平方根。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `value` | `float64 value` | 输入值或写入的新值 | 无 |

**返回值**

`float64`。计算平方根。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::math::sqrt(value);
```

**源码**：[source/std/math.mgl:28](../../source/std/math.mgl#L28)。

**完整示例**：[example/samples/std_math_clock_random.mgl](../../example/samples/std_math_clock_random.mgl)。

<a id="api-11"></a>

### floor（12）

**声明**

```magel
float64 floor(float64 value);
```

**行为**

向负无穷取整。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `value` | `float64 value` | 输入值或写入的新值 | 无 |

**返回值**

`float64`。向负无穷取整。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::math::floor(value);
```

**源码**：[source/std/math.mgl:29](../../source/std/math.mgl#L29)。

**完整示例**：[example/samples/std_math_clock_random.mgl](../../example/samples/std_math_clock_random.mgl)。

<a id="api-12"></a>

### ceil（13）

**声明**

```magel
float64 ceil(float64 value);
```

**行为**

向正无穷取整。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `value` | `float64 value` | 输入值或写入的新值 | 无 |

**返回值**

`float64`。向正无穷取整。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::math::ceil(value);
```

**源码**：[source/std/math.mgl:30](../../source/std/math.mgl#L30)。

**完整示例**：[example/samples/std_math_clock_random.mgl](../../example/samples/std_math_clock_random.mgl)。

<a id="api-13"></a>

### round（14）

**声明**

```magel
float64 round(float64 value);
```

**行为**

舍入到最近整数，中点远离零。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `value` | `float64 value` | 输入值或写入的新值 | 无 |

**返回值**

`float64`。舍入到最近整数，中点远离零。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::math::round(value);
```

**源码**：[source/std/math.mgl:31](../../source/std/math.mgl#L31)。

**完整示例**：[example/samples/std_math_clock_random.mgl](../../example/samples/std_math_clock_random.mgl)。

<a id="api-14"></a>

### trunc（15）

**声明**

```magel
float64 trunc(float64 value);
```

**行为**

向零截断小数部分。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `value` | `float64 value` | 输入值或写入的新值 | 无 |

**返回值**

`float64`。向零截断小数部分。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::math::trunc(value);
```

**源码**：[source/std/math.mgl:32](../../source/std/math.mgl#L32)。

**完整示例**：[example/samples/std_math_clock_random.mgl](../../example/samples/std_math_clock_random.mgl)。

<a id="api-15"></a>

### fmod（16）

**声明**

```magel
float64 fmod(float64 value, float64 divisor);
```

**行为**

计算浮点余数，结果符号与被除数一致。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `value` | `float64 value` | 输入值或写入的新值 | 无 |
| `divisor` | `float64 divisor` | 除数 | 无 |

**返回值**

`float64`。计算浮点余数，结果符号与被除数一致。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::math::fmod(value, divisor);
```

**源码**：[source/std/math.mgl:33](../../source/std/math.mgl#L33)。

**完整示例**：[example/samples/std_math_clock_random.mgl](../../example/samples/std_math_clock_random.mgl)。

<a id="api-16"></a>

### isNaN（17）

**声明**

```magel
bool isNaN(float64 value);
```

**行为**

判断是否为 NaN。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `value` | `float64 value` | 输入值或写入的新值 | 无 |

**返回值**

`bool`。判断是否为 NaN。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::math::isNaN(value);
```

**源码**：[source/std/math.mgl:34](../../source/std/math.mgl#L34)。

**完整示例**：[example/samples/std_math_clock_random.mgl](../../example/samples/std_math_clock_random.mgl)。

<a id="api-17"></a>

### isInf（18）

**声明**

```magel
bool isInf(float64 value);
```

**行为**

判断是否为正无穷或负无穷。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `value` | `float64 value` | 输入值或写入的新值 | 无 |

**返回值**

`bool`。判断是否为正无穷或负无穷。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::math::isInf(value);
```

**源码**：[source/std/math.mgl:35](../../source/std/math.mgl#L35)。

**完整示例**：[example/samples/std_math_clock_random.mgl](../../example/samples/std_math_clock_random.mgl)。

<a id="api-18"></a>

### abs（19）

**声明**

```magel
T abs<T>(T value);
```

**行为**

返回绝对值；泛型入口，整数最小值取负按位宽回绕，NaN 与负数零原样返回。

返回 `value` 的绝对值；整数最小值取负回绕后仍是自身，NaN 与负数零原样返回。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `value` | `T value` | 输入值或写入的新值 | 无 |

**返回值**

`T`。返回绝对值；泛型入口，整数最小值取负按位宽回绕，NaN 与负数零原样返回。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::math::abs<T>(value);
```

**源码**：[source/std/math.mgl:38](../../source/std/math.mgl#L38)。

**完整示例**：[example/samples/std_math_clock_random.mgl](../../example/samples/std_math_clock_random.mgl)。

<a id="api-19"></a>

### min（20）

**声明**

```magel
T min<T>(T... values);
```

**行为**

返回实参中的较小值；泛型入口，接受任意数量实参，相等时取后者。

任意数量实参中的最小值；单个实参给自身，相等时给后一个（与逐对比较的顺序一致）。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `values` | `T... values` | 参与操作的元素序列 | 无 |

**返回值**

`T`。返回实参中的较小值；泛型入口，接受任意数量实参，相等时取后者。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::math::min<T>(values);
```

**源码**：[source/std/math.mgl:45](../../source/std/math.mgl#L45)。

**完整示例**：[example/samples/std_math_clock_random.mgl](../../example/samples/std_math_clock_random.mgl)。

<a id="api-20"></a>

### max（21）

**声明**

```magel
T max<T>(T... values);
```

**行为**

返回实参中的较大值；泛型入口，接受任意数量实参，相等时取后者。

任意数量实参中的最大值；单个实参给自身，相等时给后一个（与逐对比较的顺序一致）。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `values` | `T... values` | 参与操作的元素序列 | 无 |

**返回值**

`T`。返回实参中的较大值；泛型入口，接受任意数量实参，相等时取后者。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::math::max<T>(values);
```

**源码**：[source/std/math.mgl:57](../../source/std/math.mgl#L57)。

**完整示例**：[example/samples/std_math_clock_random.mgl](../../example/samples/std_math_clock_random.mgl)。

<a id="api-21"></a>

### clamp（22）

**声明**

```magel
int64 clamp(int64 value, int64 low, int64 high);
```

**行为**

将 value 限制在闭区间 [low,high]；low>high 时报运行错误。

把 `value` 夹进 `[low, high]`；`low > high` 直接停机（多半是写反了）。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `value` | `int64 value` | 输入值或写入的新值 | 无 |
| `low` | `int64 low` | 闭区间下界 | 无 |
| `high` | `int64 high` | 闭区间上界 | 无 |

**返回值**

`int64`。将 value 限制在闭区间 [low,high]；low>high 时报运行错误。

**错误与边界**

遵循本页“公共约定与错误边界”。
实现包含 panic 分支；调用前应处理空值或失败状态。

**调用形式**

```magel
std::math::clamp(value, low, high);
```

**源码**：[source/std/math.mgl:69](../../source/std/math.mgl#L69)。

**完整示例**：[example/samples/std_math_clock_random.mgl](../../example/samples/std_math_clock_random.mgl)。

<a id="api-22"></a>

### clamp（23）

**声明**

```magel
float64 clamp(float64 value, float64 low, float64 high);
```

**行为**

将 value 限制在闭区间 [low,high]；low>high 时报运行错误。

把 `value` 夹进 `[low, high]`；`low > high` 直接停机（多半是写反了）。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `value` | `float64 value` | 输入值或写入的新值 | 无 |
| `low` | `float64 low` | 闭区间下界 | 无 |
| `high` | `float64 high` | 闭区间上界 | 无 |

**返回值**

`float64`。将 value 限制在闭区间 [low,high]；low>high 时报运行错误。

**错误与边界**

遵循本页“公共约定与错误边界”。
实现包含 panic 分支；调用前应处理空值或失败状态。

**调用形式**

```magel
std::math::clamp(value, low, high);
```

**源码**：[source/std/math.mgl:81](../../source/std/math.mgl#L81)。

**完整示例**：[example/samples/std_math_clock_random.mgl](../../example/samples/std_math_clock_random.mgl)。
