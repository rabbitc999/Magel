# std::numtheory — 整数数论

[返回库目录](README.md) · [全部符号](symbols.md)

## 引入

```magel
import std::numtheory;
```

## 公共约定与错误边界

实现已全部迁到 .mgl，公开入口是模块级泛型自由函数（不再有 NumberTheory 静态方法包装，也没有给胶水用的宿主私有名）。gcd/lcm/addMod/mulMod/powMod/modInverse/isPrime 对任意整数类型 T 成立，含无符号与窄整型；gcd/lcm 接受任意数量实参，逐个折叠，结果非负。模运算要求 modulus>0；模乘通过逐位加法避免乘法中间溢出。isPrime 内部加宽到 uint64，使用七个底数的确定性 Miller-Rabin。modInverse 要求 modulus>1 且与 value 互素；不存在逆元时停机。lcm 溢出也停机。

## 参考代码

完整程序：[example/samples/std_numerics_number_theory.mgl](../../example/samples/std_numerics_number_theory.mgl)。

```powershell
magel run example/samples/std_numerics_number_theory.mgl
```

<details>
<summary>展开完整参考代码</summary>

```magel
import std::linalg;
import std::numerics;
import std::numtheory;

import std::math;

float64 square(float64 x) { return x*x; }
float64 rootFunction(float64 x) { return x*x-2.0; }
float64 exponentialRate(float64 time, float64 value) { return value; }

int64 main() {
    std::linalg::Vector<float32> left=std::linalg::Vector<float32>::zeros(2);
    left.set(0,(float32)3.0); left.set(1,(float32)4.0);
    std::linalg::Vector<float32> right=left+left;
    std::linalg::Vector<float32> difference=right-left;
    std::linalg::Vector<float32> scaled=left*(float32)2.0;
    assert(difference.at(0)==(float32)3.0 && scaled.at(1)==(float32)8.0);
    std::linalg::Matrix<float32> matrix=std::linalg::Matrix<float32>::identity(2);
    std::linalg::Matrix<float32> doubled=matrix+matrix;
    std::linalg::Matrix<float32> recovered=doubled-matrix;
    std::linalg::Matrix<float32> scaledMatrix=matrix*(float32)3.0;
    std::linalg::Vector<float32> mapped=doubled*left;
    assert(mapped.at(0)==(float32)6.0 && mapped.at(1)==(float32)8.0);
    assert(recovered.at(1,1)==(float32)1.0 && scaledMatrix.at(0,0)==(float32)3.0);
    std::linalg::Matrix<float32> product=matrix*matrix;
    assert(product.at(0,0)==(float32)1.0);

    float64 integral=std::numerics::Numerics::integrateSimpson(square,0.0,1.0,20);
    float64 slope=std::numerics::Numerics::derivative(square,2.0,0.0001);
    float64 root=std::numerics::Numerics::bisect(rootFunction,1.0,2.0,0.00000001,100);
    float64 newton=std::numerics::Numerics::newton(rootFunction,1.0,0.00000001,20);
    List<float64> euler=std::numerics::Numerics::solveOdeEuler(exponentialRate,0.0,1.0,0.1,10);
    List<float64> rk4=std::numerics::Numerics::solveOdeRK4(exponentialRate,0.0,1.0,0.1,10);
    assert(std::math::abs(integral-1.0/3.0)<0.00001);
    assert(std::math::abs(slope-4.0)<0.00001);
    assert(std::math::abs(root-std::math::sqrt(2.0))<0.000001);
    assert(std::math::abs(newton-std::math::sqrt(2.0))<0.000001);
    assert(euler.len()==11 && rk4.len()==11);
    assert(rk4.at(10)>euler.at(10) &&
    std::math::abs(rk4.at(10)-std::math::E)<0.00001);

    assert(std::numtheory::gcd((uint64)54,(uint64)24)==6);
    assert(std::numtheory::lcm((uint64)6,(uint64)8)==24);
    assert(std::numtheory::modInverse((int64)3,(int64)11)==4);
    assert(std::numtheory::modInverse((int64)-3,(int64)11)==7);
    assert(std::numtheory::powMod((uint64)7,(uint64)128,(uint64)97)==35);
    assert(std::numtheory::isPrime((uint64)97));
    assert(!std::numtheory::isPrime((uint64)561));
    uint64 largePrime=(uint64)2305843009213693951;
    assert(largePrime==(((uint64)1<<61)-1));
    assert(std::numtheory::mulMod(largePrime-1,largePrime-1,largePrime)==1);
    assert(std::numtheory::isPrime((uint64)65537));
    println("generic matrix numerics number theory OK");
    return 0;
}
```

</details>

<details>
<summary>示例输出</summary>

```text
generic matrix numerics number theory OK
```

</details>

聚合入口的具体操作参见对应子模块；本页类型/方法的调用形式列在下方。

## 函数索引

| 名称 | 声明 |
| --- | --- |
| [gcd](#api-0) | `T gcd<T>(T... values)` |
| [lcm](#api-1) | `T lcm<T>(T... values)` |
| [addMod](#api-2) | `T addMod<T>(T left, T right, T modulus)` |
| [mulMod](#api-3) | `T mulMod<T>(T left, T right, T modulus)` |
| [powMod](#api-4) | `T powMod<T>(T base, T exponent, T modulus)` |
| [modInverse](#api-5) | `T modInverse<T>(T value, T modulus)` |
| [isPrime](#api-6) | `bool isPrime<T>(T value)` |

<a id="api-0"></a>

### gcd（1）

**声明**

```magel
T gcd<T>(T... values);
```

**行为**

计算实参的最大公约数；泛型入口，接受任意数量实参，结果非负。

任意数量实参的最大公约数；单个实参给它的绝对值，零实参在编译期报无法推导类型。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `values` | `T... values` | 参与操作的元素序列 | 无 |

**返回值**

`T`。计算实参的最大公约数；泛型入口，接受任意数量实参，结果非负。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::numtheory::gcd<T>(values);
```

**源码**：[source/std/numtheory.mgl:96](../../source/std/numtheory.mgl#L96)。

**完整示例**：[example/samples/std_numerics_number_theory.mgl](../../example/samples/std_numerics_number_theory.mgl)。

<a id="api-1"></a>

### lcm（2）

**声明**

```magel
T lcm<T>(T... values);
```

**行为**

计算实参的最小公倍数；泛型入口，接受任意数量实参，任一实参为零给零，乘积回绕时报运行错误。

任意数量实参的最小公倍数；任一实参为零给零，乘积回绕时停机。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `values` | `T... values` | 参与操作的元素序列 | 无 |

**返回值**

`T`。计算实参的最小公倍数；泛型入口，接受任意数量实参，任一实参为零给零，乘积回绕时报运行错误。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::numtheory::lcm<T>(values);
```

**源码**：[source/std/numtheory.mgl:108](../../source/std/numtheory.mgl#L108)。

**完整示例**：[example/samples/std_numerics_number_theory.mgl](../../example/samples/std_numerics_number_theory.mgl)。

<a id="api-2"></a>

### addMod（3）

**声明**

```magel
T addMod<T>(T left, T right, T modulus);
```

**行为**

计算 (left+right) mod modulus；modulus 不为正时报运行错误。

模加；模数不为正时停机。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `left` | `T left` | 左操作数或区间左端 | 无 |
| `right` | `T right` | 右操作数或区间右端 | 无 |
| `modulus` | `T modulus` | 模数 | 无 |

**返回值**

`T`。计算 (left+right) mod modulus；modulus 不为正时报运行错误。

**错误与边界**

遵循本页“公共约定与错误边界”。
实现包含 panic 分支；调用前应处理空值或失败状态。

**调用形式**

```magel
std::numtheory::addMod<T>(left, right, modulus);
```

**源码**：[source/std/numtheory.mgl:120](../../source/std/numtheory.mgl#L120)。

**完整示例**：[example/samples/std_numerics_number_theory.mgl](../../example/samples/std_numerics_number_theory.mgl)。

<a id="api-3"></a>

### mulMod（4）

**声明**

```magel
T mulMod<T>(T left, T right, T modulus);
```

**行为**

计算 (left×right) mod modulus，避免直接乘法的中间溢出；modulus 不为正时报运行错误。

模乘，逐位加法避免乘法溢出；模数不为正时停机。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `left` | `T left` | 左操作数或区间左端 | 无 |
| `right` | `T right` | 右操作数或区间右端 | 无 |
| `modulus` | `T modulus` | 模数 | 无 |

**返回值**

`T`。计算 (left×right) mod modulus，避免直接乘法的中间溢出；modulus 不为正时报运行错误。

**错误与边界**

遵循本页“公共约定与错误边界”。
实现包含 panic 分支；调用前应处理空值或失败状态。

**调用形式**

```magel
std::numtheory::mulMod<T>(left, right, modulus);
```

**源码**：[source/std/numtheory.mgl:130](../../source/std/numtheory.mgl#L130)。

**完整示例**：[example/samples/std_numerics_number_theory.mgl](../../example/samples/std_numerics_number_theory.mgl)。

<a id="api-4"></a>

### powMod（5）

**声明**

```magel
T powMod<T>(T base, T exponent, T modulus);
```

**行为**

通过快速幂计算 base^exponent mod modulus；modulus 不为正时报运行错误。

模幂，平方-乘；模数不为正时停机。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `base` | `T base` | 幂底数 | 无 |
| `exponent` | `T exponent` | 指数 | 无 |
| `modulus` | `T modulus` | 模数 | 无 |

**返回值**

`T`。通过快速幂计算 base^exponent mod modulus；modulus 不为正时报运行错误。

**错误与边界**

遵循本页“公共约定与错误边界”。
实现包含 panic 分支；调用前应处理空值或失败状态。

**调用形式**

```magel
std::numtheory::powMod<T>(base, exponent, modulus);
```

**源码**：[source/std/numtheory.mgl:140](../../source/std/numtheory.mgl#L140)。

**完整示例**：[example/samples/std_numerics_number_theory.mgl](../../example/samples/std_numerics_number_theory.mgl)。

<a id="api-5"></a>

### modInverse（6）

**声明**

```magel
T modInverse<T>(T value, T modulus);
```

**行为**

计算模乘逆元；模数不大于 1 或不互素时报运行错误。

模逆元，扩展欧几里得；模数不大于 1 或不互素时停机。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `value` | `T value` | 输入值或写入的新值 | 无 |
| `modulus` | `T modulus` | 模数 | 无 |

**返回值**

`T`。计算模乘逆元；模数不大于 1 或不互素时报运行错误。

**错误与边界**

遵循本页“公共约定与错误边界”。
实现包含 panic 分支；调用前应处理空值或失败状态。

**调用形式**

```magel
std::numtheory::modInverse<T>(value, modulus);
```

**源码**：[source/std/numtheory.mgl:150](../../source/std/numtheory.mgl#L150)。

**完整示例**：[example/samples/std_numerics_number_theory.mgl](../../example/samples/std_numerics_number_theory.mgl)。

<a id="api-6"></a>

### isPrime（7）

**声明**

```magel
bool isPrime<T>(T value);
```

**行为**

判断整数是否为素数；内部加宽到 uint64 做七个底数的确定性 Miller-Rabin。

确定性 Miller-Rabin 素性判定；七个底数覆盖完整 uint64 范围，窄整型在 uint64 上加宽计算。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `value` | `T value` | 输入值或写入的新值 | 无 |

**返回值**

`bool`。判断整数是否为素数；内部加宽到 uint64 做七个底数的确定性 Miller-Rabin。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::numtheory::isPrime<T>(value);
```

**源码**：[source/std/numtheory.mgl:179](../../source/std/numtheory.mgl#L179)。

**完整示例**：[example/samples/std_numerics_number_theory.mgl](../../example/samples/std_numerics_number_theory.mgl)。
