# std::random — 随机数

[返回库目录](README.md) · [全部符号](symbols.md)

## 引入

```magel
import std::random;
```

## 公共约定与错误边界

固定种子采用确定性 LCG，可复现；不用于密码学。nextInt/nextRange 区间为 [minimum,maximum)，要求 maximum>minimum；nextFloat64 为 [0,1)。Random 实现 Drop，裸 handle 接口需要配对 randomDrop。

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

## 类型、枚举、常量与字段

公开字段按源码原样列出；句柄和游标字段用于包装器状态，通常通过方法操作。枚举的括号表示变体载荷。

| 所属类型 | 声明 | 说明 |
| --- | --- | --- |
| 模块 | <a id="symbol-369"></a>`struct Random : Drop` | 拥有伪随机生成器状态的资源包装，可指定种子。 |
| Random | <a id="symbol-370"></a>`uint64 handle` | 宿主资源句柄；应通过所属类型的方法操作。 |

## 函数索引

| 名称 | 声明 |
| --- | --- |
| [randomWithSeed](#api-0) | `uint64 randomWithSeed(uint64 seed)` |
| [randomFromEntropy](#api-1) | `uint64 randomFromEntropy()` |
| [randomNextUInt64](#api-2) | `uint64 randomNextUInt64(uint64 handle)` |
| [randomNextInt](#api-3) | `int64 randomNextInt(uint64 handle, int64 minimum, int64 maximum)` |
| [randomNextFloat64](#api-4) | `float64 randomNextFloat64(uint64 handle)` |
| [randomNextRange](#api-5) | `float64 randomNextRange(uint64 handle, float64 minimum, float64 maximum)` |
| [randomNextBool](#api-6) | `bool randomNextBool(uint64 handle)` |
| [randomDrop](#api-7) | `void randomDrop(uint64 handle)` |
| [Random::withSeed](#api-8) | `static Random withSeed(uint64 seed)` |
| [Random::fromEntropy](#api-9) | `static Random fromEntropy()` |
| [Random::nextUInt64](#api-10) | `uint64 nextUInt64()` |
| [Random::nextInt](#api-11) | `int64 nextInt(int64 minimum, int64 maximum)` |
| [Random::nextFloat64](#api-12) | `float64 nextFloat64()` |
| [Random::nextRange](#api-13) | `float64 nextRange(float64 minimum, float64 maximum)` |
| [Random::nextBool](#api-14) | `bool nextBool()` |
| [Random::drop](#api-15) | `void drop()` |

<a id="api-0"></a>

### randomWithSeed（1）

**声明**

```magel
uint64 randomWithSeed(uint64 seed);
```

**行为**

宿主入口：用固定 seed 创建随机数状态。

随机数生成器。实现下沉到宿主 C++（glue/std_random.hpp，确定性 LCG 与拆分前逐位一致）， 本文件只留宿主签名 + 极薄 wrapper。句柄型状态，实现 Drop。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `seed` | `uint64 seed` | 随机种子 | 无 |

**返回值**

`uint64`。宿主入口：用固定 seed 创建随机数状态。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::random::randomWithSeed(seed);
```

**源码**：[source/std/random.mgl:5](../../source/std/random.mgl#L5)。

**完整示例**：[example/samples/std_math_clock_random.mgl](../../example/samples/std_math_clock_random.mgl)。

<a id="api-1"></a>

### randomFromEntropy（2）

**声明**

```magel
uint64 randomFromEntropy();
```

**行为**

宿主入口：用宿主熵源播种随机数状态。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`uint64`。宿主入口：用宿主熵源播种随机数状态。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::random::randomFromEntropy();
```

**源码**：[source/std/random.mgl:6](../../source/std/random.mgl#L6)。

**完整示例**：[example/samples/std_math_clock_random.mgl](../../example/samples/std_math_clock_random.mgl)。

<a id="api-2"></a>

### randomNextUInt64（3）

**声明**

```magel
uint64 randomNextUInt64(uint64 handle);
```

**行为**

宿主入口：推进生成器并返回 64 位随机值。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `handle` | `uint64 handle` | 本模块创建且仍有效的宿主句柄 | 无 |

**返回值**

`uint64`。宿主入口：推进生成器并返回 64 位随机值。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::random::randomNextUInt64(handle);
```

**源码**：[source/std/random.mgl:7](../../source/std/random.mgl#L7)。

**完整示例**：[example/samples/std_math_clock_random.mgl](../../example/samples/std_math_clock_random.mgl)。

<a id="api-3"></a>

### randomNextInt（4）

**声明**

```magel
int64 randomNextInt(uint64 handle, int64 minimum, int64 maximum);
```

**行为**

宿主入口：返回 [minimum,maximum) 内的随机整数。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `handle` | `uint64 handle` | 本模块创建且仍有效的宿主句柄 | 无 |
| `minimum` | `int64 minimum` | 包含的下界 | 无 |
| `maximum` | `int64 maximum` | 不包含的上界 | 无 |

**返回值**

`int64`。宿主入口：返回 [minimum,maximum) 内的随机整数。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::random::randomNextInt(handle, minimum, maximum);
```

**源码**：[source/std/random.mgl:8](../../source/std/random.mgl#L8)。

**完整示例**：[example/samples/std_math_clock_random.mgl](../../example/samples/std_math_clock_random.mgl)。

<a id="api-4"></a>

### randomNextFloat64（5）

**声明**

```magel
float64 randomNextFloat64(uint64 handle);
```

**行为**

宿主入口：返回 [0,1) 内的随机浮点数。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `handle` | `uint64 handle` | 本模块创建且仍有效的宿主句柄 | 无 |

**返回值**

`float64`。宿主入口：返回 [0,1) 内的随机浮点数。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::random::randomNextFloat64(handle);
```

**源码**：[source/std/random.mgl:9](../../source/std/random.mgl#L9)。

**完整示例**：[example/samples/std_math_clock_random.mgl](../../example/samples/std_math_clock_random.mgl)。

<a id="api-5"></a>

### randomNextRange（6）

**声明**

```magel
float64 randomNextRange(uint64 handle, float64 minimum, float64 maximum);
```

**行为**

宿主入口：返回 [minimum,maximum) 内的随机浮点数。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `handle` | `uint64 handle` | 本模块创建且仍有效的宿主句柄 | 无 |
| `minimum` | `float64 minimum` | 包含的下界 | 无 |
| `maximum` | `float64 maximum` | 不包含的上界 | 无 |

**返回值**

`float64`。宿主入口：返回 [minimum,maximum) 内的随机浮点数。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::random::randomNextRange(handle, minimum, maximum);
```

**源码**：[source/std/random.mgl:10](../../source/std/random.mgl#L10)。

**完整示例**：[example/samples/std_math_clock_random.mgl](../../example/samples/std_math_clock_random.mgl)。

<a id="api-6"></a>

### randomNextBool（7）

**声明**

```magel
bool randomNextBool(uint64 handle);
```

**行为**

宿主入口：推进生成器并返回随机布尔值。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `handle` | `uint64 handle` | 本模块创建且仍有效的宿主句柄 | 无 |

**返回值**

`bool`。宿主入口：推进生成器并返回随机布尔值。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::random::randomNextBool(handle);
```

**源码**：[source/std/random.mgl:11](../../source/std/random.mgl#L11)。

**完整示例**：[example/samples/std_math_clock_random.mgl](../../example/samples/std_math_clock_random.mgl)。

<a id="api-7"></a>

### randomDrop（8）

**声明**

```magel
void randomDrop(uint64 handle);
```

**行为**

宿主入口：释放对象拥有的资源，调用后遵守本模块失效规则。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `handle` | `uint64 handle` | 本模块创建且仍有效的宿主句柄 | 无 |

**返回值**

无返回值。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::random::randomDrop(handle);
```

**源码**：[source/std/random.mgl:12](../../source/std/random.mgl#L12)。

**完整示例**：[example/samples/std_math_clock_random.mgl](../../example/samples/std_math_clock_random.mgl)。

<a id="api-8"></a>

### Random::withSeed（9）

**声明**

```magel
static Random withSeed(uint64 seed);
```

**行为**

用固定 seed 创建随机数状态。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `seed` | `uint64 seed` | 随机种子 | 无 |

**返回值**

`Random`。用固定 seed 创建随机数状态。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::random::Random::withSeed(seed);
```

**源码**：[source/std/random.mgl:17](../../source/std/random.mgl#L17)。

**完整示例**：[example/samples/std_math_clock_random.mgl](../../example/samples/std_math_clock_random.mgl)。

<a id="api-9"></a>

### Random::fromEntropy（10）

**声明**

```magel
static Random fromEntropy();
```

**行为**

用宿主熵源播种随机数状态。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`Random`。用宿主熵源播种随机数状态。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::random::Random::fromEntropy();
```

**源码**：[source/std/random.mgl:18](../../source/std/random.mgl#L18)。

**完整示例**：[example/samples/std_math_clock_random.mgl](../../example/samples/std_math_clock_random.mgl)。

<a id="api-10"></a>

### Random::nextUInt64（11）

**声明**

```magel
uint64 nextUInt64();
```

**行为**

推进生成器并返回 64 位随机值。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`uint64`。推进生成器并返回 64 位随机值。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.nextUInt64();
```

**源码**：[source/std/random.mgl:20](../../source/std/random.mgl#L20)。

**完整示例**：[example/samples/std_math_clock_random.mgl](../../example/samples/std_math_clock_random.mgl)。

<a id="api-11"></a>

### Random::nextInt（12）

**声明**

```magel
int64 nextInt(int64 minimum, int64 maximum);
```

**行为**

返回 [minimum,maximum) 内的随机整数。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `minimum` | `int64 minimum` | 包含的下界 | 无 |
| `maximum` | `int64 maximum` | 不包含的上界 | 无 |

**返回值**

`int64`。返回 [minimum,maximum) 内的随机整数。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.nextInt(minimum, maximum);
```

**源码**：[source/std/random.mgl:21](../../source/std/random.mgl#L21)。

**完整示例**：[example/samples/std_math_clock_random.mgl](../../example/samples/std_math_clock_random.mgl)。

<a id="api-12"></a>

### Random::nextFloat64（13）

**声明**

```magel
float64 nextFloat64();
```

**行为**

返回 [0,1) 内的随机浮点数。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`float64`。返回 [0,1) 内的随机浮点数。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.nextFloat64();
```

**源码**：[source/std/random.mgl:22](../../source/std/random.mgl#L22)。

**完整示例**：[example/samples/std_math_clock_random.mgl](../../example/samples/std_math_clock_random.mgl)。

<a id="api-13"></a>

### Random::nextRange（14）

**声明**

```magel
float64 nextRange(float64 minimum, float64 maximum);
```

**行为**

返回 [minimum,maximum) 内的随机浮点数。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `minimum` | `float64 minimum` | 包含的下界 | 无 |
| `maximum` | `float64 maximum` | 不包含的上界 | 无 |

**返回值**

`float64`。返回 [minimum,maximum) 内的随机浮点数。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.nextRange(minimum, maximum);
```

**源码**：[source/std/random.mgl:23](../../source/std/random.mgl#L23)。

**完整示例**：[example/samples/std_math_clock_random.mgl](../../example/samples/std_math_clock_random.mgl)。

<a id="api-14"></a>

### Random::nextBool（15）

**声明**

```magel
bool nextBool();
```

**行为**

推进生成器并返回随机布尔值。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`bool`。推进生成器并返回随机布尔值。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.nextBool();
```

**源码**：[source/std/random.mgl:24](../../source/std/random.mgl#L24)。

**完整示例**：[example/samples/std_math_clock_random.mgl](../../example/samples/std_math_clock_random.mgl)。

<a id="api-15"></a>

### Random::drop（16）

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

**源码**：[source/std/random.mgl:25](../../source/std/random.mgl#L25)。

**完整示例**：[example/samples/std_math_clock_random.mgl](../../example/samples/std_math_clock_random.mgl)。
