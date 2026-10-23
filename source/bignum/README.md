# big_number：任意精度大整数 + 十进制浮点（C++17）

> **谁在用**：`source/stackvm` 的 `bint` 类型（虚拟机的任意精度整数）后端就是这个库的
> `bigint::BigInt` —— 宿主语言（如 `chali` 的 ChaLi 语言）里的
> `bint a = bint("…"); a + b;` 走到虚拟机内部就是这边的运算符重载。
> 那一步是**可选的**：`make -C source/stackvm STACKVM_WITH_BIGINT=0` 能编出不带大整数的
> 轻量虚拟机。ChaLi 语言层的 `BigInteger` / `BigDecimal` 方法（`.pow()`、`.strRadix()`、
> `.sqrt()` 等）也直接落在这些 C++ 成员上。

这是一个**只依赖标准库**的大数库：加 / 减 / 乘 / 除 / 取余 /
幂 / 最大公约数 / 最小公倍数 / 同余幂（含大指数）/ 数论（扩展欧几里得、模逆元、
非负取模）/ 概率素性测试 / 位运算 / 2..36 进制互转 / 整数 k 次方根，
以及十进制字符串互转。实现参考
`eigenmath-plus/eigenmath_bigint.c` 的整数部分（同一套"符号 + 小端 limb + 有效长度"的
结构），把手工内存管理换成 `std::vector`、把 C 接口换成 C++ 的运算符重载。

同一个库里还有 `bigfloat::BigFloat`（`bigfloat.hpp` / `bigfloat.cpp`）：**十进制浮点**，
`值 = 尾数 × 10^指数`、尾数就是上面的 `BigInt` —— 对应参考实现的 `BigFloat_T`。
所以 `0.1 + 0.2` 精确等于 `0.3`（二进制浮点那套表示误差这里不存在），
大整数乘除法的优化也会用于浮点尾数运算。

目录名是 `bignum`（一个库装下整数与浮点），C++ 里的名字仍按类型走：
整数是 `bigint::BigInt`（`bigint.hpp`）、十进制浮点是 `bigfloat::BigFloat`
（`bigfloat.hpp`）—— 头文件名与命名空间说的都是"类型"，不是库目录。

## 表示与约定

```text
value = sign × Σ limbs[i] × 2^(32 i)      limbs 是小端 uint32_t 数组
```

- 0 只有一种表示：`sign() == 0`、limb 个数为 0；
- 高位不存 0 limb（每次运算后规范化）；
- `a / b`、`a % b` 与 C / C++ 的整数除法一致：**向零截断**，
  `a == (a / b) * b + a % b`，余数符号跟被除数；
- 除以零：`divmod()` 返回 `false`；`operator/`、`operator%` 抛 `std::domain_error`；
- `int64_t` 构造只吃有符号范围，大于 `INT64_MAX` 的无符号值用
  **`BigInt::fromU64()`**（与参考实现的 `EigenMath_BigIntFromU64` 对应）。

## API

```cpp
using bigint::BigInt;

BigInt a("123456789012345678901234567890");   // 十进制串（可带 +/- 号）
BigInt b = BigInt::fromU64(18446744073709551615ull);
BigInt c(42), d(-7);

a + b;  a - b;  a * b;  a / b;  a % b;   -a;      // 运算符重载（值语义）
a += b; a -= b; a *= b; a /= b; a %= b; ++a; a--; // 就地运算
a == b; a != b; a <  b; a <= b; a > b; a >= b;    // 比较（含负数）

a.toString();          // 十进制字符串
a.toString(16);        // 2..36 进制字符串（数字用 0-9a-z，负数带 -）
a.decimalDigits();     // 十进制位数
a.limbCount();         // 32 位 limb 个数
a.isZero(); a.sign();  // 0 / ±1
a.isEven(); a.isOdd(); a.isNegative(); a.isPositive();
a.abs();
a.toU64(); a.toI64(); a.toDouble();   // 截断（toDouble 会丢精度，符号跟着走）
static_cast<std::int64_t>(a);         // 与 toI64 一致（显式收窄）
static_cast<double>(a);               // 与 toDouble 一致（显式收窄）

a.pow(300);                          // 快速幂（二进制）
BigInt::gcd(a, b); BigInt::lcm(a, b); // 欧几里得 / 最小公倍数
a.powmod(300, modulus);              // 同余幂（64 位指数，逐步取模）
a.powmod(bigExponent, modulus);      // 同余幂（**指数也是大整数**，素性测试用）
a.modNonNegative(modulus);           // 非负取模：结果落在 [0, |modulus|)
BigInt::ExtendedGcd eg = BigInt::extendedGcd(a, b);  // a*eg.x + b*eg.y = eg.gcd
BigInt::modInverse(a, modulus);      // 模逆元（不互素抛 std::domain_error）
a.isProbablePrime(25);               // Miller-Rabin（固定种子，可复现）
a.nextProbablePrime();               // 严格大于 a 的下一个（概率）素数

a.bitLength(); a.bitAt(5);           // 位长 / 取位
a.setBit(3); a.clearBit(3); a.flipBit(3);
a.shiftLeft(10); a.shiftRight(10);   // 就地位移
a & b; a | b; a ^ b;                 // 按位与 / 或 / 异或（仅非负数，负数抛异常）
~a;                                  // 补口语义 ~x = -x - 1
BigInt::min(a, b); BigInt::max(a, b);
bigint::integerSqrt(a);              // 整数平方根（向下取整）
bigint::integerRoot(a, 3);           // 整数 k 次方根（向下取整）

BigInt q, r;
if (a.divmod(b, &q, &r)) { /* a == q*b + r */ }   // 除零返回 false，不抛异常

bool ok = false;
BigInt parsed = BigInt::fromString("12a", &ok);   // 不抛异常的解析；ok == false
BigInt hex = BigInt::fromString("-ff", 16);        // 2..36 进制解析（字母不分大小写）
```

## BigFloat API

```cpp
using bigfloat::BigFloat;

BigFloat x("123.456");            // 十进制文本：123.456 / -0.5 / .5 / 1e100 / 2.5E-3
BigFloat y = BigFloat::fromInt(BigInt("123456789012345678901234567890"));  // 整数 → 浮点（精确）
BigFloat z = BigFloat::fromDouble(0.1);   // double → 最短往返十进制：z.toString() == "0.1"

x + y; x - y; x * y; x / y;  -x;          // 运算符重载（operator/ 结果保留 34 位）
x += y; x -= y; x *= y; x /= y;
x == y; x != y; x < y; x <= y; x > y; x >= y;

x.mantissa(); x.exponent10();  // 值 = 尾数 × 10^指数（尾数带符号）
x.sign(); x.isZero();

x.round(10);            // 舍入到 10 位有效数字（四舍五入、远离零）
x.toString();           // 十进制文本，缺省 17 位（%.17g 口径，指数大/小切科学记数法）
x.toString(30);         // 30 位有效数字
x.toString(0);          // ≤ 0 = 不舍入（按值精确输出，显示阈值仍按 17 位）
x.toPlainString();      // 恒定普通记法：1e20 → "100000000000000000000"
x.divide(y, 50);        // 除法并指定有效位数
x.sqrt(30);             // 平方根，保留 30 位（负数抛 std::domain_error）
x.root(3, 30);          // k 次方根，牛顿迭代（k=2 即 sqrt）
x.pow(5);               // 精确整数次幂（尾数走大整数幂，不丢精度）
x.abs();                // 绝对值
x.isInteger();          // 5.0 / 1e20 是整数，0.5 / 1e-3 不是
x.floor(); x.ceil(); x.trunc();   // 取整，返回 bigint::BigInt

BigFloat implicitInt = 42;        // int64 → BigFloat 隐式构造
BigFloat implicitDouble = 0.5;    // double → BigFloat 隐式构造
static_cast<std::int64_t>(x);     // 向零截断成 int64（显式收窄）
static_cast<double>(x);           // 转 double（显式收窄，可能丢精度）
```

精度口径与参考实现一致：**所有运算都在整数上做，多算几位再舍入**到目标有效位数。

输出记法与 `%g` 同一口径：设 P 为显示位数（`digits > 0` 时就是 `digits`，否则 17）、
X 为科学记数法指数（`值 = d.ddd × 10^X`），则 `X < -4 || X >= P` 用科学记数法，
否则用普通记法。所以 `BigFloat("1e20").toString()` 是 `"1e+20"`（20 ≥ 17），
要看全 21 位就用 `toPlainString()`。

## 算法

| 操作 | 做法 | 复杂度 |
| --- | --- | --- |
| 加 / 减 | limb 级带进位 / 借位（符号交叉在 `operator±` 里分派） | O(n) |
| 乘（默认） | **Karatsuba**：`z0 + ((z1-z0-z2) << m) + (z2 << 2m)`，一次分配工作区，递归终点为原生整数乘法 | O(n^log2(3)) |
| 乘（超大规模） | **双素数 NTT + CRT 重建**（15 位数字，`p1 = 998244353`、`p2 = 1004535809`，原根 3） | **O(n log n)** |
| 除 / 取余（默认） | **Burnikel-Ziegler**：递归求解 `2n/1n` 与 `3n/2n` 子问题 | 分治复杂度，取决于乘法内核 |
| 除 / 取余（小规模） | **Knuth D 分块长除法**，默认递归基线不超过 256 limb；单块除数线性扫描 | O((n−m+1)×m) |
| 幂 / 同余幂 | 二进制快速幂 | O(log e) 次乘法 |
| gcd | 欧几里得（相除取余） | O(log min(a,b)) 次除法 |
| 十进制互转 | 每 9 位一段，反复乘 / 除 10^9 | O(段数 × n) |
| 浮点加 / 减 | 对齐到较小的十进制指数、整数加减、再规范化 | O(位数) |
| 浮点乘 | 尾数相乘（默认 Karatsuba，超大规模走 NTT）+ 指数相加 | O(n^log2(3)) / O(n log n) |
| 浮点除 | 放大被除数到 `digits + 2` 位有效数字，整数除 + 舍入 | 一次大整数除法 |
| 浮点平方根 | 指数凑成偶数、尾数放大到 `2×digits + 2` 位，`integerSqrt` + 舍入 | 一次整数开方 |

乘法仅保留 Karatsuba 与 NTT。Karatsuba 使用差值形式，工作区一次分配，递归分支复用；
长短操作数按较短一侧的补齐宽度分块，避免将短数扩展到长数宽度。
递归终点是 32 位原生乘法；支持 `unsigned __int128` 时，两块使用一次 64 位原生乘法，
四块使用三个 64 位乘积展开的 Karatsuba，不使用学校乘法双循环。

NTT 将 32 位块以滑动窗口转换为 15 位数字，每级预计算并复用旋转因子；
平方在每个素数域只做一次正变换，CRT 只重建实际乘积系数。
共同支持的变换长度上限是 **2^21**，超过时使用 Karatsuba，避免执行无效变换。

NTT 的候选下限与实际分派区分如下：

- 支持原生 128 位乘积时，两侧块数合计至少 **3,072**；可移植递归路径为 **768**。
- 较短操作数不超过 **96** 块时保留 Karatsuba，避免为长短悬殊的乘法启动大变换。
- 设 `w` 为较短操作数补齐到 2 的幂后的宽度，`k = log2(w)`，长侧为 `L`，
  Karatsuba 工作量取 `3^k × ceil(L/w)`；NTT 工作量取按块数上界推算的补齐变换长度 `t × log2(t)`。
  支持原生 128 位乘积时比较 `2 × Karatsuba工作量` 与 `4 × NTT工作量`，
  平方将后者权重改为 3；可移植路径的 Karatsuba 权重为 6。估计 NTT 较快时才切换。

这些规则保留变换长度的阶梯效应：当前原生宽度配置下，等长的 1,536 块普通乘法走
Karatsuba、平方走 NTT；3,072 块普通乘法走 NTT，4,096 块又优先 Karatsuba，
6,144 块走 NTT。它们不是按十进制位数划出一个永不回退的界限。
`setMultiplicationThreshold()` 调整候选下限，设为 `1` 强制支持范围内的多块输入走 NTT；
`setKaratsubaThreshold()` 保留接口，现表示短侧的 Karatsuba 保护区，不再控制学校算法基线。

除法将 Knuth D 作为 Burnikel–Ziegler 的递归基线：归一化后，每次估计一个 32 位商块，
根据次高块校正估商，减乘结果为负时回加除数。单块除数直接扫描，最多两块商的输入也直接
使用 Knuth D。大规模输入仍递归处理 `2n/1n`、`3n/2n` 子问题。
`setDivisionThreshold()` 默认 **256**、最小 **32**；设很大可以强制 Knuth 路径。
向零截断、余数符号及除零行为保持不变。

当前测量使用 AMD Ryzen 5 5600X、MinGW g++ 11.2 `-O2`、固定种子稠密输入和五轮中位数；
优化前基线为提交 `9388cf0`。完整前后数据、强制算法交叉区和除法阈值测量见
[原始 JSON](../../doc/benchmarks/bigint-arithmetic.json)。删除学校算法后，部分小规模乘法可能
慢于基线；数据保留这些退化点，不以超大输入的加速代表所有输入。

可复现命令（从仓库根目录执行）：

```sh
g++ -std=c++17 -O2 tests/bigint_benchmark.cpp source/bignum/bigint*.cpp -o tmp/bigint-benchmark
tmp/bigint-benchmark
tmp/bigint-benchmark --tune
tmp/bigint-benchmark --division
python tests/bigint_arithmetic.py
python tests/bigint_arithmetic.py --portable --binary tmp/bigint-portable/arithmetic.exe
```

差分测试使用 Python 整数独立验证默认、强制 NTT、强制 Karatsuba 三种配置，覆盖估商回加、
归一化边界、正负商和余数、平方、输入输出别名、乘法及除法切换边界。
可移植路径通过仅供回归的 `BIGINT_FORCE_PORTABLE_MULTIPLY` 验证，日常构建不定义此宏。

### 历史测量

下列数据来自优化前的三级内核，保留其实际算法和配置，用于历史对照，不代表当前界限。

当时的三个默认值在测量机器上用 MinGW g++ 11.2 `-O2`、稠密平衡操作数、5 轮中位数重新标定。
学校算法与 Karatsuba 在约 **96–120 limb** 进入交叉区（下表单位为毫秒）：

| 每边 limb | 学校算法 | Karatsuba（基线 96） |
| ---: | ---: | ---: |
| 96 | 0.0050 | 0.0050 |
| 104 | 0.0059 | **0.0051** |
| 120 | 0.0065 | **0.0063** |
| 129 | 0.0091 | **0.0070** |
| 161 | 0.0137 | **0.0104** |

Karatsuba 与 NTT 的首个明确交叉落在每边约 **24K limb**、两边合计约 **48K limb**。
NTT 长度取 2 的幂，因此耗时会阶梯式跳变，不存在对所有形状都完美的单一阈值：

| 每边 limb | Karatsuba（基线 96） | NTT |
| ---: | ---: | ---: |
| 20,481 | **26.6** | 29.8 |
| 24,576 | 35.4 | **30.4** |
| 32,768 | **55.4** | 58.8 |
| 40,961 | 79.3 | **60.4** |

历史移位相减除法的平衡 `2n / n` 交叉区在除数 **48–56 limb**；默认取 56，避免短商场景过早进入
分治。除数为 41 / 49 / 57 limb 时，长除法与 Burnikel-Ziegler 分别为
`0.242/0.252`、`0.321/0.296`、`0.411/0.330` ms。不同 CPU、编译器与操作数比例会移动
交叉点，所以三个阈值都保留公开调节接口。

NTT 内部还有一处关键写法：`ntt` / `convolution` 把模数写成
**模板参数**（编译期常量），蝶形里的 `% modulus` 才能被编成"乘高位 + 移位"而不是
很慢的 64 位除法指令 —— 同一台机器上这一处就让 400,000 位的乘法从 43 ms 掉到 23 ms。

NTT 的细节（与参考实现的思路一致）：

```text
32 位 limb  →  15 位数字（小端）
    →  两个素数各做一次长度 2^k 的 NTT 卷积
    →  CRT：x = r1 + p1 × ((r2 − r1) × p1^{-1} mod p2)
    →  15 位数字内部进位  →  线性打包回 32 位 limb
```

15 位数字保证"系数真值 < p1×p2 ≈ 2^60"，两个素数都支持到 2^21 的长度，
所以几个兆比特以内的乘法都不会溢出。

## 构建与自检

```sh
cmake -S source/bignum -B build/bignum -DCMAKE_BUILD_TYPE=Release
cmake --build build/bignum --config Release
ctest --test-dir build/bignum -C Release --output-on-failure
```

演示（`big_number-demo`）打印固定内容，`check` 会顺带跑它：

```text
2^100      = 1267650600228229401496703205376
30!        = 265252859812191058636308480000000
F(200)     = 280571172992510140037611932413038677189525
a           = 123456789012345678901234567890
b           = 98765432109876543210
a / b      = 1249999988
a % b      = 60185185207253086410
gcd(a, b)  = 90
lcm(12, 18) = 36
7^300 mod 1000000007 = 678144625
位十进制位数 = 30（limb 4 个）

BigFloat（尾数 × 10^指数，十进制浮点）：
0.1 + 0.2    = 0.3（同样的 double 在二进制浮点里是 0.30000000000000004）
1 / 3 (30 位)= 0.333333333333333333333333333333
sqrt(2) (30) = 1.41421356237309504880168872421
1e20 + 1     = 100000000000000000001（精确，没有丢低位）
2^200 精确   = 1606938044258990275541962092341162602522202993782792835301376
2^200 17 位  = 1.6069380442589903e+60
```

大整数与大浮点单元测试分别覆盖 273 与 136 项断言。

## 高精度超越函数

`bigfloat_math.hpp` 提供 `pi`、`exp`、`ln`、`log10`、`sin`、`cos`、`tan`、`atan`、`asin`、`acos`、实数幂和截断余数。圆周率使用 AGM，指数使用级数与缩放平方，对数使用平方根约化，三角函数使用高精度参数约化与级数；全部复用十进制 `BigFloat`，不转二进制浮点。工作精度增加保护位并检查资源预算，非法实数定义域抛出异常。

ChaLi 的 `std::math` 使用这些函数计算 `BigDecimal` 分支，有效位数沿用实参设置，未设置时采用 34 位；显式 `pi(digits)` 支持 1..100000 位。独立参考回归为 `python tests/bigfloat_math.py`；参考值由 mpmath 生成并固化，日常测试只需 Python 标准库。更新参考值使用 `python tests/bigfloat_math.py --refresh-reference`，需要 mpmath。

## 源码组织

公开接口保留在 `bigint.hpp`、`bigfloat.hpp` 与 `bigfloat_math.hpp`；实现按职责独立编译：

- `bigint.cpp`：构造、规范化、基础幅值与有符号算术、整数方根。
- `bigint_multiplication.cpp`：Karatsuba/NTT 内核与算法阈值。
- `bigint_division.cpp`：Knuth 与 Burnikel-Ziegler 幅值除法。
- `bigint_bits.cpp`、`bigint_number_theory.cpp`、`bigint_modular.cpp`、`bigint_conversion.cpp`：位操作、幂与公约数、模运算与素性、文本与机器数值转换。
- `bigfloat.cpp`：解析、格式化、舍入、比较和四则运算。
- `bigfloat_roots.cpp`：幂、方根和取整。
- `bigfloat_transcendental.cpp`、`bigfloat_trigonometry.cpp`：高精度数学。

使用 CMake 静态库目标链接全部实现；不要只编译 `bigint.cpp` 或 `bigfloat.cpp`。ChaLi 的 Makefile、内嵌资产与导出编译器模板包含相同单元，保留大数裁剪宏。当前修改限于本仓库副本。
