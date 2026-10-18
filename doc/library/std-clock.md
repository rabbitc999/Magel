# std::clock — 单调时钟

[返回库目录](README.md) · [全部符号](symbols.md)

## 引入

```magel
import std::clock;
```

## 公共约定与错误边界

采用宿主 steady_clock。读数的起点由平台决定，只能用于差值与计时，不代表日期时间；单位分别为毫秒和纳秒，实际分辨率由平台决定。

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
| [nowMs](#api-0) | `int64 nowMs()` |
| [nowNanos](#api-1) | `int64 nowNanos()` |

<a id="api-0"></a>

### nowMs（1）

**声明**

```magel
int64 nowMs();
```

**行为**

读取单调时钟毫秒计数。

单调时钟：`steady_clock`，不受系统时间调整影响，测速 / 基准用。 基准点由平台定，只能拿两次读数做差（不要当成挂钟时间）。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`int64`。读取单调时钟毫秒计数。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::clock::nowMs();
```

**源码**：[source/std/clock.mgl:10](../../source/std/clock.mgl#L10)。

**完整示例**：[example/samples/std_math_clock_random.mgl](../../example/samples/std_math_clock_random.mgl)。

<a id="api-1"></a>

### nowNanos（2）

**声明**

```magel
int64 nowNanos();
```

**行为**

读取单调时钟纳秒计数。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`int64`。读取单调时钟纳秒计数。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::clock::nowNanos();
```

**源码**：[source/std/clock.mgl:11](../../source/std/clock.mgl#L11)。

**完整示例**：[example/samples/std_math_clock_random.mgl](../../example/samples/std_math_clock_random.mgl)。
