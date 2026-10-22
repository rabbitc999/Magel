# std::math — 泛型数学

[返回库目录](README.md) · [全部符号](symbols.md)

## 引入

```magel
import std::math;
```

## 公共约定与错误边界

三角函数使用弧度；普通整数返回 float64，float32 保持类型，float64 转发宿主数学库。BigInteger 转为 BigDecimal，BigDecimal 使用十进制高精度算法并沿用有效位数，Expr 组合符号 AST。pi(digits) 返回 1..100000 位圆周率。大十进制无 NaN/无穷，非法定义域报告错误；普通浮点沿用宿主特殊值。round 中点远离零，trunc 向零，fmod 余数与被除数同号。abs/min/max 保留既有泛型比较语义；clamp 检查 low<=high。

## 参考代码

完整程序：[example/samples/math_generic.mgl](../../example/samples/math_generic.mgl)。

```powershell
magel run example/samples/math_generic.mgl
```

<details>
<summary>展开完整参考代码</summary>

```magel
import std::math;
import std::cas;

int64 main()
{
    float32 single = sin((float32)0);
    float64 integer = cos(0);
    assert(single == 0.0f);
    assert(integer == 1.0);
    BigDecimal value = BigDecimal(1);
    value.setPrecision(100);
    println("sin(1) = {}", sin(value).str());
    println("exp(1) = {}", exp(value).str());
    println("pi = {}", pi(100).str());
    assert(sin(BigDecimal(0)) == BigDecimal(0));
    assert(cos(BigDecimal(0)) == BigDecimal(1));
    assert(ln(BigDecimal(1)) == BigDecimal(0));
    assert(pow(BigDecimal(-2), 3) == BigDecimal(-8));
    assert(fmod(BigDecimal("-5.5"), BigDecimal(2)) == BigDecimal("-1.5"));
    assert(floor(BigDecimal("-1.5")) == BigDecimal(-2));
    assert(ceil(BigDecimal("-1.5")) == BigDecimal(-1));
    assert(round(BigDecimal("-1.5")) == BigDecimal(-2));
    Expr x = Expr::symbol("x");
    println("symbolic = {}", sin(x).differentiate().str());
    assert(sin(x).str() == "sin(x)");
    assert(sin(Expr::parse("pi/6")) == Expr::parse("1/2"));
    assert(sqrt(Expr::parse("4")) == Expr::parse("2"));
    assert(floor(Expr::parse("-3/2")) == Expr::parse("-2"));
    assert(round(Expr::parse("-3/2")) == Expr::parse("-2"));
    assert(fmod(Expr::parse("-11/2"), Expr::parse("2")) == Expr::parse("-3/2"));
    return 0;
}
```

</details>

<details>
<summary>示例输出</summary>

```text
sin(1) = 0.8414709848078965066525023216302989996225630607983710656727517099919104043912396689486397435430526959
exp(1) = 2.718281828459045235360287471352662497757247093699959574966967627724076630353547594571382178525166427
pi = 3.141592653589793238462643383279502884197169399375105820974944592307816406286208998628034825342117068
symbolic = cos(x)
```

</details>

聚合入口的具体操作参见对应子模块；本页类型/方法的调用形式列在下方。

## 函数索引

| 名称 | 声明 |
| --- | --- |
| [pi](#api-0) | `BigDecimal pi(int64 digits = 50)` |
| [sin](#api-1) | `let sin<T>(T value)` |
| [cos](#api-2) | `let cos<T>(T value)` |
| [tan](#api-3) | `let tan<T>(T value)` |
| [asin](#api-4) | `let asin<T>(T value)` |
| [acos](#api-5) | `let acos<T>(T value)` |
| [atan](#api-6) | `let atan<T>(T value)` |
| [exp](#api-7) | `let exp<T>(T value)` |
| [ln](#api-8) | `let ln<T>(T value)` |
| [log10](#api-9) | `let log10<T>(T value)` |
| [sqrt](#api-10) | `let sqrt<T>(T value)` |
| [floor](#api-11) | `let floor<T>(T value)` |
| [ceil](#api-12) | `let ceil<T>(T value)` |
| [round](#api-13) | `let round<T>(T value)` |
| [trunc](#api-14) | `let trunc<T>(T value)` |
| [pow](#api-15) | `let pow<T, U>(T value, U other)` |
| [fmod](#api-16) | `let fmod<T, U>(T value, U other)` |
| [isNaN](#api-17) | `bool isNaN<T>(T value)` |
| [isInf](#api-18) | `bool isInf<T>(T value)` |
| [abs](#api-19) | `T abs<T>(T value)` |
| [min](#api-20) | `T min<T>(T... values)` |
| [max](#api-21) | `T max<T>(T... values)` |
| [clamp](#api-22) | `int64 clamp(int64 value, int64 low, int64 high)` |
| [clamp](#api-23) | `float64 clamp(float64 value, float64 low, float64 high)` |

<a id="api-0"></a>

### pi（1）

**声明**

```magel
BigDecimal pi(int64 digits = 50);
```

**行为**

按指定有效十进制位数计算圆周率。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `digits` | `int64 digits` | 十进制有效数字精度 | `50` |

**返回值**

`BigDecimal`。按指定有效十进制位数计算圆周率。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::math::pi(digits);
```

**源码**：[source/std/math.mgl:7](../../source/std/math.mgl#L7)。

**完整示例**：[example/samples/math_generic.mgl](../../example/samples/math_generic.mgl)。

<a id="api-1"></a>

### sin（2）

**声明**

```magel
let sin<T>(T value);
```

**行为**

计算正弦。

对普通数值、大十进制或符号表达式计算 sin。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `value` | `T value` | 输入值或写入的新值 | 无 |

**返回值**

`let`。计算正弦。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::math::sin<T>(value);
```

**源码**：[source/std/math.mgl:10](../../source/std/math.mgl#L10)。

**完整示例**：[example/samples/math_generic.mgl](../../example/samples/math_generic.mgl)。

<a id="api-2"></a>

### cos（3）

**声明**

```magel
let cos<T>(T value);
```

**行为**

计算余弦。

对普通数值、大十进制或符号表达式计算 cos。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `value` | `T value` | 输入值或写入的新值 | 无 |

**返回值**

`let`。计算余弦。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::math::cos<T>(value);
```

**源码**：[source/std/math.mgl:26](../../source/std/math.mgl#L26)。

**完整示例**：[example/samples/math_generic.mgl](../../example/samples/math_generic.mgl)。

<a id="api-3"></a>

### tan（4）

**声明**

```magel
let tan<T>(T value);
```

**行为**

计算正切。

对普通数值、大十进制或符号表达式计算 tan。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `value` | `T value` | 输入值或写入的新值 | 无 |

**返回值**

`let`。计算正切。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::math::tan<T>(value);
```

**源码**：[source/std/math.mgl:42](../../source/std/math.mgl#L42)。

**完整示例**：[example/samples/math_generic.mgl](../../example/samples/math_generic.mgl)。

<a id="api-4"></a>

### asin（5）

**声明**

```magel
let asin<T>(T value);
```

**行为**

计算反正弦，结果为弧度。

对普通数值、大十进制或符号表达式计算 asin。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `value` | `T value` | 输入值或写入的新值 | 无 |

**返回值**

`let`。计算反正弦，结果为弧度。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::math::asin<T>(value);
```

**源码**：[source/std/math.mgl:58](../../source/std/math.mgl#L58)。

**完整示例**：[example/samples/math_generic.mgl](../../example/samples/math_generic.mgl)。

<a id="api-5"></a>

### acos（6）

**声明**

```magel
let acos<T>(T value);
```

**行为**

计算反余弦，结果为弧度。

对普通数值、大十进制或符号表达式计算 acos。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `value` | `T value` | 输入值或写入的新值 | 无 |

**返回值**

`let`。计算反余弦，结果为弧度。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::math::acos<T>(value);
```

**源码**：[source/std/math.mgl:74](../../source/std/math.mgl#L74)。

**完整示例**：[example/samples/math_generic.mgl](../../example/samples/math_generic.mgl)。

<a id="api-6"></a>

### atan（7）

**声明**

```magel
let atan<T>(T value);
```

**行为**

计算反正切，结果为弧度。

对普通数值、大十进制或符号表达式计算 atan。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `value` | `T value` | 输入值或写入的新值 | 无 |

**返回值**

`let`。计算反正切，结果为弧度。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::math::atan<T>(value);
```

**源码**：[source/std/math.mgl:90](../../source/std/math.mgl#L90)。

**完整示例**：[example/samples/math_generic.mgl](../../example/samples/math_generic.mgl)。

<a id="api-7"></a>

### exp（8）

**声明**

```magel
let exp<T>(T value);
```

**行为**

计算 e 的 value 次幂。

对普通数值、大十进制或符号表达式计算 exp。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `value` | `T value` | 输入值或写入的新值 | 无 |

**返回值**

`let`。计算 e 的 value 次幂。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::math::exp<T>(value);
```

**源码**：[source/std/math.mgl:106](../../source/std/math.mgl#L106)。

**完整示例**：[example/samples/math_generic.mgl](../../example/samples/math_generic.mgl)。

<a id="api-8"></a>

### ln（9）

**声明**

```magel
let ln<T>(T value);
```

**行为**

计算自然对数。

对普通数值、大十进制或符号表达式计算 ln。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `value` | `T value` | 输入值或写入的新值 | 无 |

**返回值**

`let`。计算自然对数。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::math::ln<T>(value);
```

**源码**：[source/std/math.mgl:122](../../source/std/math.mgl#L122)。

**完整示例**：[example/samples/math_generic.mgl](../../example/samples/math_generic.mgl)。

<a id="api-9"></a>

### log10（10）

**声明**

```magel
let log10<T>(T value);
```

**行为**

计算以 10 为底的对数。

对普通数值、大十进制或符号表达式计算 log10。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `value` | `T value` | 输入值或写入的新值 | 无 |

**返回值**

`let`。计算以 10 为底的对数。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::math::log10<T>(value);
```

**源码**：[source/std/math.mgl:138](../../source/std/math.mgl#L138)。

**完整示例**：[example/samples/math_generic.mgl](../../example/samples/math_generic.mgl)。

<a id="api-10"></a>

### sqrt（11）

**声明**

```magel
let sqrt<T>(T value);
```

**行为**

计算平方根。

对普通数值、大十进制或符号表达式计算 sqrt。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `value` | `T value` | 输入值或写入的新值 | 无 |

**返回值**

`let`。计算平方根。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::math::sqrt<T>(value);
```

**源码**：[source/std/math.mgl:154](../../source/std/math.mgl#L154)。

**完整示例**：[example/samples/math_generic.mgl](../../example/samples/math_generic.mgl)。

<a id="api-11"></a>

### floor（12）

**声明**

```magel
let floor<T>(T value);
```

**行为**

向负无穷取整。

对普通数值、大十进制或符号表达式计算 floor。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `value` | `T value` | 输入值或写入的新值 | 无 |

**返回值**

`let`。向负无穷取整。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::math::floor<T>(value);
```

**源码**：[source/std/math.mgl:170](../../source/std/math.mgl#L170)。

**完整示例**：[example/samples/math_generic.mgl](../../example/samples/math_generic.mgl)。

<a id="api-12"></a>

### ceil（13）

**声明**

```magel
let ceil<T>(T value);
```

**行为**

向正无穷取整。

对普通数值、大十进制或符号表达式计算 ceil。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `value` | `T value` | 输入值或写入的新值 | 无 |

**返回值**

`let`。向正无穷取整。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::math::ceil<T>(value);
```

**源码**：[source/std/math.mgl:186](../../source/std/math.mgl#L186)。

**完整示例**：[example/samples/math_generic.mgl](../../example/samples/math_generic.mgl)。

<a id="api-13"></a>

### round（14）

**声明**

```magel
let round<T>(T value);
```

**行为**

舍入到最近整数，中点远离零。

对普通数值、大十进制或符号表达式计算 round。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `value` | `T value` | 输入值或写入的新值 | 无 |

**返回值**

`let`。舍入到最近整数，中点远离零。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::math::round<T>(value);
```

**源码**：[source/std/math.mgl:202](../../source/std/math.mgl#L202)。

**完整示例**：[example/samples/math_generic.mgl](../../example/samples/math_generic.mgl)。

<a id="api-14"></a>

### trunc（15）

**声明**

```magel
let trunc<T>(T value);
```

**行为**

向零截断小数部分。

对普通数值、大十进制或符号表达式计算 trunc。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `value` | `T value` | 输入值或写入的新值 | 无 |

**返回值**

`let`。向零截断小数部分。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::math::trunc<T>(value);
```

**源码**：[source/std/math.mgl:218](../../source/std/math.mgl#L218)。

**完整示例**：[example/samples/math_generic.mgl](../../example/samples/math_generic.mgl)。

<a id="api-15"></a>

### pow（16）

**声明**

```magel
let pow<T, U>(T value, U other);
```

**行为**

计算 value 的 exponent 次幂。

根据实参类型计算 pow，大十进制保持有效位精度。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `value` | `T value` | 输入值或写入的新值 | 无 |
| `other` | `U other` | 另一操作数 | 无 |

**返回值**

`let`。计算 value 的 exponent 次幂。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::math::pow<T, U>(value, other);
```

**源码**：[source/std/math.mgl:234](../../source/std/math.mgl#L234)。

**完整示例**：[example/samples/math_generic.mgl](../../example/samples/math_generic.mgl)。

<a id="api-16"></a>

### fmod（17）

**声明**

```magel
let fmod<T, U>(T value, U other);
```

**行为**

计算浮点余数，结果符号与被除数一致。

根据实参类型计算 fmod，大十进制保持有效位精度。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `value` | `T value` | 输入值或写入的新值 | 无 |
| `other` | `U other` | 另一操作数 | 无 |

**返回值**

`let`。计算浮点余数，结果符号与被除数一致。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::math::fmod<T, U>(value, other);
```

**源码**：[source/std/math.mgl:383](../../source/std/math.mgl#L383)。

**完整示例**：[example/samples/math_generic.mgl](../../example/samples/math_generic.mgl)。

<a id="api-17"></a>

### isNaN（18）

**声明**

```magel
bool isNaN<T>(T value);
```

**行为**

判断是否为 NaN。

判断数值是否为 isNaN 对应的特殊浮点值。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `value` | `T value` | 输入值或写入的新值 | 无 |

**返回值**

`bool`。判断是否为 NaN。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::math::isNaN<T>(value);
```

**源码**：[source/std/math.mgl:531](../../source/std/math.mgl#L531)。

**完整示例**：[example/samples/math_generic.mgl](../../example/samples/math_generic.mgl)。

<a id="api-18"></a>

### isInf（19）

**声明**

```magel
bool isInf<T>(T value);
```

**行为**

判断是否为正无穷或负无穷。

判断数值是否为 isInf 对应的特殊浮点值。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `value` | `T value` | 输入值或写入的新值 | 无 |

**返回值**

`bool`。判断是否为正无穷或负无穷。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::math::isInf<T>(value);
```

**源码**：[source/std/math.mgl:546](../../source/std/math.mgl#L546)。

**完整示例**：[example/samples/math_generic.mgl](../../example/samples/math_generic.mgl)。

<a id="api-19"></a>

### abs（20）

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

**源码**：[source/std/math.mgl:560](../../source/std/math.mgl#L560)。

**完整示例**：[example/samples/math_generic.mgl](../../example/samples/math_generic.mgl)。

<a id="api-20"></a>

### min（21）

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

**源码**：[source/std/math.mgl:567](../../source/std/math.mgl#L567)。

**完整示例**：[example/samples/math_generic.mgl](../../example/samples/math_generic.mgl)。

<a id="api-21"></a>

### max（22）

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

**源码**：[source/std/math.mgl:579](../../source/std/math.mgl#L579)。

**完整示例**：[example/samples/math_generic.mgl](../../example/samples/math_generic.mgl)。

<a id="api-22"></a>

### clamp（23）

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

**源码**：[source/std/math.mgl:591](../../source/std/math.mgl#L591)。

**完整示例**：[example/samples/math_generic.mgl](../../example/samples/math_generic.mgl)。

<a id="api-23"></a>

### clamp（24）

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

**源码**：[source/std/math.mgl:603](../../source/std/math.mgl#L603)。

**完整示例**：[example/samples/math_generic.mgl](../../example/samples/math_generic.mgl)。
