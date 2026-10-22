# std::cas — 精确符号计算

[返回库目录](README.md) · [全部符号](symbols.md)

## 引入

```magel
import std::cas;
```

## 公共约定与错误边界

Expr 从字符串解析为不可变 AST，使用共享大整数后端保存精确整数和有理数。支持四则运算、节点遍历、化简、求导、积分、替换、极限与 Taylor 展开；不支持的积分保留符号节点。调用串行保护 CAS 内部资源，AST 可以独立复制。

## 参考代码

完整程序：[example/samples/cas.mgl](../../example/samples/cas.mgl)。

```powershell
magel run example/samples/cas.mgl
```

<details>
<summary>展开完整参考代码</summary>

```magel
import std::cas;

int64 main()
{
    Expr polynomial = Expr::parse("x^3 + 2*x");
    println("derivative = {}", polynomial.differentiate("x").str());
    println("integral = {}", Expr::parse("x^2").integrate("x").str());
    println("definite = {}", Expr::parse("x^2").integrateDefinite("x", Expr::parse("0"), Expr::parse("3")).str());

    Expr third = Expr::parse("1/3");
    Expr sixth = Expr::parse("1/6");
    Expr half = third + sixth;
    assert(half.numerator() == BigInteger(1));
    assert(half.denominator() == BigInteger(2));
    assert((third / sixth) == Expr::parse("2"));
    assert((-third + third) == Expr::parse("0"));
    println("exact = {}", half);
    println("decimal = {}", half.decimal(50).str());

    Expr huge = Expr::fromInteger(BigInteger("10000000000000000000000000000000000000001"));
    assert((huge * huge / huge) == huge);
    assert(Expr::fromDecimal(BigDecimal("0.1")) + Expr::fromDecimal(BigDecimal("0.2")) == Expr::parse("0.3"));

    Expr tree;
    {
        Expr parent = Expr::parse("sin(x)");
        tree = parent.ast();
    }
    assert(tree.kind() == NodeKind::Function);
    assert(tree.name() == "sin");
    assert(tree.childCount() == 1);
    let children = tree.children();
    assert(children[0].kind() == NodeKind::Symbol);
    println("ast = {}({})", tree.name(), children[0].name());
    return 0;
}
```

</details>

<details>
<summary>示例输出</summary>

```text
derivative = 3*x^2 + 2
integral = 1/3*x^3
definite = 9
exact = 1/2
decimal = 0.5
ast = sin(x)
```

</details>

聚合入口的具体操作参见对应子模块；本页类型/方法的调用形式列在下方。

## 类型、枚举、常量与字段

公开字段按源码原样列出；句柄和游标字段用于包装器状态，通常通过方法操作。枚举的括号表示变体载荷。

| 所属类型 | 声明 | 说明 |
| --- | --- | --- |
| 模块 | <a id="symbol-4"></a>`#derive(Equal, Hash, Clone) enum NodeKind` | 规范化 AST 的节点种类；减法和除法规范化为加法、乘法与幂。 |
| NodeKind | `Integer, Rational, Symbol, Add, Multiply, Power, Function, Constant` | 规范化 AST 的节点种类；减法和除法规范化为加法、乘法与幂。 变体及载荷见声明。 |
| 模块 | <a id="symbol-5"></a>`struct Expr : Add<Expr, Expr>, Subtract<Expr, Expr>, Multiply<Expr, Expr>, Divide<Expr, Expr>, Negate<Expr, Expr>, Equal<bool, Expr>` | 精确符号表达式，同时也是可遍历的 AST 根节点。 |

## 函数索引

| 名称 | 声明 |
| --- | --- |
| [Expr::parse](#api-0) | `static Expr parse(String text)` |
| [Expr::fromInteger](#api-1) | `static Expr fromInteger(BigInteger number)` |
| [Expr::fromDecimal](#api-2) | `static Expr fromDecimal(BigDecimal number)` |
| [Expr::symbol](#api-3) | `static Expr symbol(String name)` |
| [Expr::ast](#api-4) | `Expr ast()` |
| [Expr::str](#api-5) | `String str()` |
| [Expr::toString](#api-6) | `String toString()` |
| [Expr::latex](#api-7) | `String latex()` |
| [Expr::kind](#api-8) | `NodeKind kind()` |
| [Expr::name](#api-9) | `String name()` |
| [Expr::childCount](#api-10) | `int64 childCount()` |
| [Expr::child](#api-11) | `Expr child(int64 index)` |
| [Expr::children](#api-12) | `List<Expr> children()` |
| [Expr::numerator](#api-13) | `BigInteger numerator()` |
| [Expr::denominator](#api-14) | `BigInteger denominator()` |
| [Expr::decimal](#api-15) | `BigDecimal decimal(int64 digits = 50)` |
| [Expr::leftAdd](#api-16) | `Expr leftAdd(Expr other)` |
| [Expr::leftSub](#api-17) | `Expr leftSub(Expr other)` |
| [Expr::leftMul](#api-18) | `Expr leftMul(Expr other)` |
| [Expr::leftDiv](#api-19) | `Expr leftDiv(Expr other)` |
| [Expr::leftEqual](#api-20) | `bool leftEqual(Expr other)` |
| [Expr::negate](#api-21) | `Expr negate(Expr operand)` |
| [Expr::pow](#api-22) | `Expr pow(Expr exponent)` |
| [Expr::pow](#api-23) | `Expr pow(int64 exponent)` |
| [Expr::simplify](#api-24) | `Expr simplify()` |
| [Expr::expand](#api-25) | `Expr expand()` |
| [Expr::factor](#api-26) | `Expr factor()` |
| [Expr::together](#api-27) | `Expr together()` |
| [Expr::differentiate](#api-28) | `Expr differentiate(String variable = "x", int64 order = 1)` |
| [Expr::integrate](#api-29) | `Expr integrate(String variable = "x")` |
| [Expr::substitute](#api-30) | `Expr substitute(String variable, Expr replacement)` |
| [Expr::limit](#api-31) | `Expr limit(String variable, Expr point)` |
| [Expr::taylor](#api-32) | `Expr taylor(String variable, Expr point, int64 order)` |
| [Expr::integrateDefinite](#api-33) | `Expr integrateDefinite(String variable, Expr lower, Expr upper)` |
| [Expr::sin](#api-34) | `Expr sin()` |
| [Expr::cos](#api-35) | `Expr cos()` |
| [Expr::tan](#api-36) | `Expr tan()` |
| [Expr::asin](#api-37) | `Expr asin()` |
| [Expr::acos](#api-38) | `Expr acos()` |
| [Expr::atan](#api-39) | `Expr atan()` |
| [Expr::exp](#api-40) | `Expr exp()` |
| [Expr::ln](#api-41) | `Expr ln()` |
| [Expr::log10](#api-42) | `Expr log10()` |
| [Expr::sqrt](#api-43) | `Expr sqrt()` |
| [Expr::floor](#api-44) | `Expr floor()` |
| [Expr::ceil](#api-45) | `Expr ceil()` |
| [Expr::round](#api-46) | `Expr round()` |
| [Expr::trunc](#api-47) | `Expr trunc()` |
| [Expr::fmod](#api-48) | `Expr fmod(Expr other)` |
| [parse](#api-49) | `Expr parse(String text)` |

<a id="api-0"></a>

### Expr::parse（1）

**声明**

```magel
static Expr parse(String text);
```

**行为**

从字符串解析表达式；整数与十进制常量均保留精确值。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `text` | `String text` | 输入文本，位置与长度通常按字节 | 无 |

**返回值**

`Expr`。从字符串解析表达式；整数与十进制常量均保留精确值。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::cas::Expr::parse(text);
```

**源码**：[source/std/cas.mgl:43](../../source/std/cas.mgl#L43)。

**完整示例**：[example/samples/cas.mgl](../../example/samples/cas.mgl)。

<a id="api-1"></a>

### Expr::fromInteger（2）

**声明**

```magel
static Expr fromInteger(BigInteger number);
```

**行为**

将现有大整数无损转换为表达式。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `number` | `BigInteger number` | 需要精确转换的数值 | 无 |

**返回值**

`Expr`。将现有大整数无损转换为表达式。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::cas::Expr::fromInteger(number);
```

**源码**：[source/std/cas.mgl:49](../../source/std/cas.mgl#L49)。

**完整示例**：[example/samples/cas.mgl](../../example/samples/cas.mgl)。

<a id="api-2"></a>

### Expr::fromDecimal（3）

**声明**

```magel
static Expr fromDecimal(BigDecimal number);
```

**行为**

将当前大浮点的精确十进制值转换为有理数表达式。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `number` | `BigDecimal number` | 需要精确转换的数值 | 无 |

**返回值**

`Expr`。将当前大浮点的精确十进制值转换为有理数表达式。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::cas::Expr::fromDecimal(number);
```

**源码**：[source/std/cas.mgl:51](../../source/std/cas.mgl#L51)。

**完整示例**：[example/samples/cas.mgl](../../example/samples/cas.mgl)。

<a id="api-3"></a>

### Expr::symbol（4）

**声明**

```magel
static Expr symbol(String name);
```

**行为**

建立单个自变量；传入复合表达式时报告错误。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `name` | `String name` | 名称 | 无 |

**返回值**

`Expr`。建立单个自变量；传入复合表达式时报告错误。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::cas::Expr::symbol(name);
```

**源码**：[source/std/cas.mgl:53](../../source/std/cas.mgl#L53)。

**完整示例**：[example/samples/cas.mgl](../../example/samples/cas.mgl)。

<a id="api-4"></a>

### Expr::ast（5）

**声明**

```magel
Expr ast();
```

**行为**

取得独立持有的 AST 根；其寿命不依赖原表达式。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`Expr`。取得独立持有的 AST 根；其寿命不依赖原表达式。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.ast();
```

**源码**：[source/std/cas.mgl:59](../../source/std/cas.mgl#L59)。

**完整示例**：[example/samples/cas.mgl](../../example/samples/cas.mgl)。

<a id="api-5"></a>

### Expr::str（6）

**声明**

```magel
String str();
```

**行为**

规范中缀文本，可再次解析。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`String`。规范中缀文本，可再次解析。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.str();
```

**源码**：[source/std/cas.mgl:61](../../source/std/cas.mgl#L61)。

**完整示例**：[example/samples/cas.mgl](../../example/samples/cas.mgl)。

<a id="api-6"></a>

### Expr::toString（7）

**声明**

```magel
String toString();
```

**行为**

统一值文本协议使用规范中缀表示。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`String`。统一值文本协议使用规范中缀表示。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.toString();
```

**源码**：[source/std/cas.mgl:63](../../source/std/cas.mgl#L63)。

**完整示例**：[example/samples/cas.mgl](../../example/samples/cas.mgl)。

<a id="api-7"></a>

### Expr::latex（8）

**声明**

```magel
String latex();
```

**行为**

LaTeX 数学文本。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`String`。LaTeX 数学文本。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.latex();
```

**源码**：[source/std/cas.mgl:65](../../source/std/cas.mgl#L65)。

**完整示例**：[example/samples/cas.mgl](../../example/samples/cas.mgl)。

<a id="api-8"></a>

### Expr::kind（9）

**声明**

```magel
NodeKind kind();
```

**行为**

根节点的语义种类。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`NodeKind`。根节点的语义种类。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.kind();
```

**源码**：[source/std/cas.mgl:67](../../source/std/cas.mgl#L67)。

**完整示例**：[example/samples/cas.mgl](../../example/samples/cas.mgl)。

<a id="api-9"></a>

### Expr::name（10）

**声明**

```magel
String name();
```

**行为**

符号、函数或命名常数的名称；其他节点返回空串。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`String`。符号、函数或命名常数的名称；其他节点返回空串。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.name();
```

**源码**：[source/std/cas.mgl:79](../../source/std/cas.mgl#L79)。

**完整示例**：[example/samples/cas.mgl](../../example/samples/cas.mgl)。

<a id="api-10"></a>

### Expr::childCount（11）

**声明**

```magel
int64 childCount();
```

**行为**

直接子节点数量。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`int64`。直接子节点数量。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.childCount();
```

**源码**：[source/std/cas.mgl:81](../../source/std/cas.mgl#L81)。

**完整示例**：[example/samples/cas.mgl](../../example/samples/cas.mgl)。

<a id="api-11"></a>

### Expr::child（12）

**声明**

```magel
Expr child(int64 index);
```

**行为**

取得独立持有的子树，检查下标范围。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `index` | `int64 index` | 从 0 开始的位置 | 无 |

**返回值**

`Expr`。取得独立持有的子树，检查下标范围。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.child(index);
```

**源码**：[source/std/cas.mgl:83](../../source/std/cas.mgl#L83)。

**完整示例**：[example/samples/cas.mgl](../../example/samples/cas.mgl)。

<a id="api-12"></a>

### Expr::children（13）

**声明**

```magel
List<Expr> children();
```

**行为**

以独立列表取得全部直接子树。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`List<Expr>`。以独立列表取得全部直接子树。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.children();
```

**源码**：[source/std/cas.mgl:89](../../source/std/cas.mgl#L89)。

**完整示例**：[example/samples/cas.mgl](../../example/samples/cas.mgl)。

<a id="api-13"></a>

### Expr::numerator（14）

**声明**

```magel
BigInteger numerator();
```

**行为**

数值节点的精确分子；非数值节点报告错误。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`BigInteger`。数值节点的精确分子；非数值节点报告错误。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.numerator();
```

**源码**：[source/std/cas.mgl:100](../../source/std/cas.mgl#L100)。

**完整示例**：[example/samples/cas.mgl](../../example/samples/cas.mgl)。

<a id="api-14"></a>

### Expr::denominator（15）

**声明**

```magel
BigInteger denominator();
```

**行为**

数值节点的正分母；整数返回 1。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`BigInteger`。数值节点的正分母；整数返回 1。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.denominator();
```

**源码**：[source/std/cas.mgl:102](../../source/std/cas.mgl#L102)。

**完整示例**：[example/samples/cas.mgl](../../example/samples/cas.mgl)。

<a id="api-15"></a>

### Expr::decimal（16）

**声明**

```magel
BigDecimal decimal(int64 digits = 50);
```

**行为**

用现有 BigDecimal 后端按指定位数舍入精确有理数。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `digits` | `int64 digits` | 十进制有效数字精度 | `50` |

**返回值**

`BigDecimal`。用现有 BigDecimal 后端按指定位数舍入精确有理数。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.decimal(digits);
```

**源码**：[source/std/cas.mgl:104](../../source/std/cas.mgl#L104)。

**完整示例**：[example/samples/cas.mgl](../../example/samples/cas.mgl)。

<a id="api-16"></a>

### Expr::leftAdd（17）

**声明**

```magel
Expr leftAdd(Expr other);
```

**行为**

四则运算直接组合 AST，常数计算保持精确有理数。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `other` | `Expr other` | 另一操作数 | 无 |

**返回值**

`Expr`。四则运算直接组合 AST，常数计算保持精确有理数。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.leftAdd(other);
```

**源码**：[source/std/cas.mgl:106](../../source/std/cas.mgl#L106)。

**完整示例**：[example/samples/cas.mgl](../../example/samples/cas.mgl)。

<a id="api-17"></a>

### Expr::leftSub（18）

**声明**

```magel
Expr leftSub(Expr other);
```

**行为**



**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `other` | `Expr other` | 另一操作数 | 无 |

**返回值**

`Expr`。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.leftSub(other);
```

**源码**：[source/std/cas.mgl:107](../../source/std/cas.mgl#L107)。

**完整示例**：[example/samples/cas.mgl](../../example/samples/cas.mgl)。

<a id="api-18"></a>

### Expr::leftMul（19）

**声明**

```magel
Expr leftMul(Expr other);
```

**行为**



**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `other` | `Expr other` | 另一操作数 | 无 |

**返回值**

`Expr`。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.leftMul(other);
```

**源码**：[source/std/cas.mgl:108](../../source/std/cas.mgl#L108)。

**完整示例**：[example/samples/cas.mgl](../../example/samples/cas.mgl)。

<a id="api-19"></a>

### Expr::leftDiv（20）

**声明**

```magel
Expr leftDiv(Expr other);
```

**行为**



**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `other` | `Expr other` | 另一操作数 | 无 |

**返回值**

`Expr`。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.leftDiv(other);
```

**源码**：[source/std/cas.mgl:109](../../source/std/cas.mgl#L109)。

**完整示例**：[example/samples/cas.mgl](../../example/samples/cas.mgl)。

<a id="api-20"></a>

### Expr::leftEqual（21）

**声明**

```magel
bool leftEqual(Expr other);
```

**行为**

两棵规范 AST 结构相等时为 true，不承诺证明任意数学恒等式。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `other` | `Expr other` | 另一操作数 | 无 |

**返回值**

`bool`。两棵规范 AST 结构相等时为 true，不承诺证明任意数学恒等式。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.leftEqual(other);
```

**源码**：[source/std/cas.mgl:111](../../source/std/cas.mgl#L111)。

**完整示例**：[example/samples/cas.mgl](../../example/samples/cas.mgl)。

<a id="api-21"></a>

### Expr::negate（22）

**声明**

```magel
Expr negate(Expr operand);
```

**行为**

精确取负。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `operand` | `Expr operand` | 待取负的表达式 | 无 |

**返回值**

`Expr`。精确取负。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.negate(operand);
```

**源码**：[source/std/cas.mgl:113](../../source/std/cas.mgl#L113)。

**完整示例**：[example/samples/cas.mgl](../../example/samples/cas.mgl)。

<a id="api-22"></a>

### Expr::pow（23）

**声明**

```magel
Expr pow(Expr exponent);
```

**行为**

组合并化简符号幂。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `exponent` | `Expr exponent` | 指数 | 无 |

**返回值**

`Expr`。组合并化简符号幂。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.pow(exponent);
```

**源码**：[source/std/cas.mgl:115](../../source/std/cas.mgl#L115)。

**完整示例**：[example/samples/cas.mgl](../../example/samples/cas.mgl)。

<a id="api-23"></a>

### Expr::pow（24）

**声明**

```magel
Expr pow(int64 exponent);
```

**行为**

使用整数指数建立幂。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `exponent` | `int64 exponent` | 指数 | 无 |

**返回值**

`Expr`。使用整数指数建立幂。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.pow(exponent);
```

**源码**：[source/std/cas.mgl:117](../../source/std/cas.mgl#L117)。

**完整示例**：[example/samples/cas.mgl](../../example/samples/cas.mgl)。

<a id="api-24"></a>

### Expr::simplify（25）

**声明**

```magel
Expr simplify();
```

**行为**

递归化简并返回新的 AST。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`Expr`。递归化简并返回新的 AST。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.simplify();
```

**源码**：[source/std/cas.mgl:119](../../source/std/cas.mgl#L119)。

**完整示例**：[example/samples/cas.mgl](../../example/samples/cas.mgl)。

<a id="api-25"></a>

### Expr::expand（26）

**声明**

```magel
Expr expand();
```

**行为**

展开乘法和非负整数幂。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`Expr`。展开乘法和非负整数幂。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.expand();
```

**源码**：[source/std/cas.mgl:121](../../source/std/cas.mgl#L121)。

**完整示例**：[example/samples/cas.mgl](../../example/samples/cas.mgl)。

<a id="api-26"></a>

### Expr::factor（27）

**声明**

```magel
Expr factor();
```

**行为**

对支持的多项式进行因式分解。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`Expr`。对支持的多项式进行因式分解。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.factor();
```

**源码**：[source/std/cas.mgl:123](../../source/std/cas.mgl#L123)。

**完整示例**：[example/samples/cas.mgl](../../example/samples/cas.mgl)。

<a id="api-27"></a>

### Expr::together（28）

**声明**

```magel
Expr together();
```

**行为**

合并有理式分母。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`Expr`。合并有理式分母。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.together();
```

**源码**：[source/std/cas.mgl:125](../../source/std/cas.mgl#L125)。

**完整示例**：[example/samples/cas.mgl](../../example/samples/cas.mgl)。

<a id="api-28"></a>

### Expr::differentiate（29）

**声明**

```magel
Expr differentiate(String variable = "x", int64 order = 1);
```

**行为**

对单个符号求指定阶导数，阶数范围为 0..1000。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `variable` | `String variable` | 单个自变量的名称 | `"x"` |
| `order` | `int64 order` | 导数或展开阶数，范围 0..1000 | `1` |

**返回值**

`Expr`。对单个符号求指定阶导数，阶数范围为 0..1000。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.differentiate(variable, order);
```

**源码**：[source/std/cas.mgl:127](../../source/std/cas.mgl#L127)。

**完整示例**：[example/samples/cas.mgl](../../example/samples/cas.mgl)。

<a id="api-29"></a>

### Expr::integrate（30）

**声明**

```magel
Expr integrate(String variable = "x");
```

**行为**

求不定积分；不支持时保留 integrate 函数节点，不伪造结果。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `variable` | `String variable` | 单个自变量的名称 | `"x"` |

**返回值**

`Expr`。求不定积分；不支持时保留 integrate 函数节点，不伪造结果。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.integrate(variable);
```

**源码**：[source/std/cas.mgl:129](../../source/std/cas.mgl#L129)。

**完整示例**：[example/samples/cas.mgl](../../example/samples/cas.mgl)。

<a id="api-30"></a>

### Expr::substitute（31）

**声明**

```magel
Expr substitute(String variable, Expr replacement);
```

**行为**

精确替换变量并化简。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `variable` | `String variable` | 单个自变量的名称 | 无 |
| `replacement` | `Expr replacement` | 正则替换文本 | 无 |

**返回值**

`Expr`。精确替换变量并化简。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.substitute(variable, replacement);
```

**源码**：[source/std/cas.mgl:131](../../source/std/cas.mgl#L131)。

**完整示例**：[example/samples/cas.mgl](../../example/samples/cas.mgl)。

<a id="api-31"></a>

### Expr::limit（32）

**声明**

```magel
Expr limit(String variable, Expr point);
```

**行为**

在有限点求支持的符号极限。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `variable` | `String variable` | 单个自变量的名称 | 无 |
| `point` | `Expr point` | 极限或展开中心表达式 | 无 |

**返回值**

`Expr`。在有限点求支持的符号极限。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.limit(variable, point);
```

**源码**：[source/std/cas.mgl:133](../../source/std/cas.mgl#L133)。

**完整示例**：[example/samples/cas.mgl](../../example/samples/cas.mgl)。

<a id="api-32"></a>

### Expr::taylor（33）

**声明**

```magel
Expr taylor(String variable, Expr point, int64 order);
```

**行为**

在指定点作 Taylor 展开，阶数范围为 0..1000。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `variable` | `String variable` | 单个自变量的名称 | 无 |
| `point` | `Expr point` | 极限或展开中心表达式 | 无 |
| `order` | `int64 order` | 导数或展开阶数，范围 0..1000 | 无 |

**返回值**

`Expr`。在指定点作 Taylor 展开，阶数范围为 0..1000。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.taylor(variable, point, order);
```

**源码**：[source/std/cas.mgl:135](../../source/std/cas.mgl#L135)。

**完整示例**：[example/samples/cas.mgl](../../example/samples/cas.mgl)。

<a id="api-33"></a>

### Expr::integrateDefinite（34）

**声明**

```magel
Expr integrateDefinite(String variable, Expr lower, Expr upper);
```

**行为**

求定积分，不能求值时报告错误。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `variable` | `String variable` | 单个自变量的名称 | 无 |
| `lower` | `Expr lower` | 定积分下界表达式 | 无 |
| `upper` | `Expr upper` | 定积分上界表达式 | 无 |

**返回值**

`Expr`。求定积分，不能求值时报告错误。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.integrateDefinite(variable, lower, upper);
```

**源码**：[source/std/cas.mgl:137](../../source/std/cas.mgl#L137)。

**完整示例**：[example/samples/cas.mgl](../../example/samples/cas.mgl)。

<a id="api-34"></a>

### Expr::sin（35）

**声明**

```magel
Expr sin();
```

**行为**

构建并化简 sin 数学函数的符号表达式。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`Expr`。构建并化简 sin 数学函数的符号表达式。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.sin();
```

**源码**：[source/std/cas.mgl:139](../../source/std/cas.mgl#L139)。

**完整示例**：[example/samples/cas.mgl](../../example/samples/cas.mgl)。

<a id="api-35"></a>

### Expr::cos（36）

**声明**

```magel
Expr cos();
```

**行为**

构建并化简 cos 数学函数的符号表达式。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`Expr`。构建并化简 cos 数学函数的符号表达式。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.cos();
```

**源码**：[source/std/cas.mgl:141](../../source/std/cas.mgl#L141)。

**完整示例**：[example/samples/cas.mgl](../../example/samples/cas.mgl)。

<a id="api-36"></a>

### Expr::tan（37）

**声明**

```magel
Expr tan();
```

**行为**

构建并化简 tan 数学函数的符号表达式。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`Expr`。构建并化简 tan 数学函数的符号表达式。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.tan();
```

**源码**：[source/std/cas.mgl:143](../../source/std/cas.mgl#L143)。

**完整示例**：[example/samples/cas.mgl](../../example/samples/cas.mgl)。

<a id="api-37"></a>

### Expr::asin（38）

**声明**

```magel
Expr asin();
```

**行为**

构建并化简 asin 数学函数的符号表达式。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`Expr`。构建并化简 asin 数学函数的符号表达式。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.asin();
```

**源码**：[source/std/cas.mgl:145](../../source/std/cas.mgl#L145)。

**完整示例**：[example/samples/cas.mgl](../../example/samples/cas.mgl)。

<a id="api-38"></a>

### Expr::acos（39）

**声明**

```magel
Expr acos();
```

**行为**

构建并化简 acos 数学函数的符号表达式。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`Expr`。构建并化简 acos 数学函数的符号表达式。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.acos();
```

**源码**：[source/std/cas.mgl:147](../../source/std/cas.mgl#L147)。

**完整示例**：[example/samples/cas.mgl](../../example/samples/cas.mgl)。

<a id="api-39"></a>

### Expr::atan（40）

**声明**

```magel
Expr atan();
```

**行为**

构建并化简 atan 数学函数的符号表达式。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`Expr`。构建并化简 atan 数学函数的符号表达式。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.atan();
```

**源码**：[source/std/cas.mgl:149](../../source/std/cas.mgl#L149)。

**完整示例**：[example/samples/cas.mgl](../../example/samples/cas.mgl)。

<a id="api-40"></a>

### Expr::exp（41）

**声明**

```magel
Expr exp();
```

**行为**

构建并化简 exp 数学函数的符号表达式。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`Expr`。构建并化简 exp 数学函数的符号表达式。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.exp();
```

**源码**：[source/std/cas.mgl:151](../../source/std/cas.mgl#L151)。

**完整示例**：[example/samples/cas.mgl](../../example/samples/cas.mgl)。

<a id="api-41"></a>

### Expr::ln（42）

**声明**

```magel
Expr ln();
```

**行为**

构建并化简 ln 数学函数的符号表达式。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`Expr`。构建并化简 ln 数学函数的符号表达式。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.ln();
```

**源码**：[source/std/cas.mgl:153](../../source/std/cas.mgl#L153)。

**完整示例**：[example/samples/cas.mgl](../../example/samples/cas.mgl)。

<a id="api-42"></a>

### Expr::log10（43）

**声明**

```magel
Expr log10();
```

**行为**

构建并化简 log10 数学函数的符号表达式。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`Expr`。构建并化简 log10 数学函数的符号表达式。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.log10();
```

**源码**：[source/std/cas.mgl:155](../../source/std/cas.mgl#L155)。

**完整示例**：[example/samples/cas.mgl](../../example/samples/cas.mgl)。

<a id="api-43"></a>

### Expr::sqrt（44）

**声明**

```magel
Expr sqrt();
```

**行为**

构建并化简 sqrt 数学函数的符号表达式。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`Expr`。构建并化简 sqrt 数学函数的符号表达式。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.sqrt();
```

**源码**：[source/std/cas.mgl:157](../../source/std/cas.mgl#L157)。

**完整示例**：[example/samples/cas.mgl](../../example/samples/cas.mgl)。

<a id="api-44"></a>

### Expr::floor（45）

**声明**

```magel
Expr floor();
```

**行为**

构建并化简 floor 数学函数的符号表达式。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`Expr`。构建并化简 floor 数学函数的符号表达式。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.floor();
```

**源码**：[source/std/cas.mgl:159](../../source/std/cas.mgl#L159)。

**完整示例**：[example/samples/cas.mgl](../../example/samples/cas.mgl)。

<a id="api-45"></a>

### Expr::ceil（46）

**声明**

```magel
Expr ceil();
```

**行为**

构建并化简 ceil 数学函数的符号表达式。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`Expr`。构建并化简 ceil 数学函数的符号表达式。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.ceil();
```

**源码**：[source/std/cas.mgl:161](../../source/std/cas.mgl#L161)。

**完整示例**：[example/samples/cas.mgl](../../example/samples/cas.mgl)。

<a id="api-46"></a>

### Expr::round（47）

**声明**

```magel
Expr round();
```

**行为**

构建并化简 round 数学函数的符号表达式。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`Expr`。构建并化简 round 数学函数的符号表达式。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.round();
```

**源码**：[source/std/cas.mgl:163](../../source/std/cas.mgl#L163)。

**完整示例**：[example/samples/cas.mgl](../../example/samples/cas.mgl)。

<a id="api-47"></a>

### Expr::trunc（48）

**声明**

```magel
Expr trunc();
```

**行为**

构建并化简 trunc 数学函数的符号表达式。

**参数**

无显式参数。实例方法使用当前接收者。

**返回值**

`Expr`。构建并化简 trunc 数学函数的符号表达式。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.trunc();
```

**源码**：[source/std/cas.mgl:165](../../source/std/cas.mgl#L165)。

**完整示例**：[example/samples/cas.mgl](../../example/samples/cas.mgl)。

<a id="api-48"></a>

### Expr::fmod（49）

**声明**

```magel
Expr fmod(Expr other);
```

**行为**

构建截断余数的符号函数节点。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `other` | `Expr other` | 另一操作数 | 无 |

**返回值**

`Expr`。构建截断余数的符号函数节点。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
obj.fmod(other);
```

**源码**：[source/std/cas.mgl:167](../../source/std/cas.mgl#L167)。

**完整示例**：[example/samples/cas.mgl](../../example/samples/cas.mgl)。

<a id="api-49"></a>

### parse（50）

**声明**

```magel
Expr parse(String text);
```

**行为**

解析字符串为精确表达式 AST。

**参数**

| 参数 | 类型／声明 | 含义 | 默认值 |
| --- | --- | --- | --- |
| `text` | `String text` | 输入文本，位置与长度通常按字节 | 无 |

**返回值**

`Expr`。解析字符串为精确表达式 AST。

**错误与边界**

遵循本页“公共约定与错误边界”。

**调用形式**

```magel
std::cas::parse(text);
```

**源码**：[source/std/cas.mgl:171](../../source/std/cas.mgl#L171)。

**完整示例**：[example/samples/cas.mgl](../../example/samples/cas.mgl)。
