# 04 语言参考

按主题查阅 Magel 的语法与语义。想先有个整体印象，看[语言速览](02-language-tour.md)；想理解为什么这样设计，看[核心概念](03-core-concepts.md)。

文法的权威形式是 [EBNF 语法参考](14-grammar-ebnf.md)。本篇讲怎么用。

> 本篇的片段除注明外都在默认构建上实测运行过。

---

## 1. 词法

| 类别 | 写法 | 备注 |
| --- | --- | --- |
| 单行注释 | `// …` | |
| 块注释 | `/* … */` | 可嵌套 |
| 标识符 | `[A-Za-z_][A-Za-z0-9_]*` | 区分大小写 |
| 整数字面量 | `42`、`0xFF`、`b1010`、`1u`、`0xffu`、`b_101u` | 小写 `u` 表示无符号，默认 `uint64`；禁止 `-1u` |
| 浮点字面量 | `3.14`、`1e9`、`1.5f`、`1f` | 无后缀为 `float64`，小写 `f` 后缀为 `float32` |
| 字符字面量 | `'M'`、`'\n'` | 类型是 `char` |
| 字符串字面量 | `"hello"` | 类型是 `std::lang::String` |
| 布尔 | `true` / `false` | |

语句以 `;` 结尾，块用 `{ }`。预处理指令以 `%` 开头、宏以 `$` 开头，见 §14。

同一程序中内容相同的普通字符串字面量共享一份生成期数据，跨函数使用也会合并，空串同样处理。
字面量不可直接修改，也不能取得指向其字节的可变引用。
保存到独立 `String` 值时仍沿用值复制和确定性清理规则，修改一个变量不改变另一个变量。

---

## 2. 类型

### 基础类型

| 类别 | 类型 |
| --- | --- |
| 有符号整数 | `int8` `int16` `int32` `int64` |
| 无符号整数 | `uint8` `uint16` `uint32` `uint64` |
| 浮点 | `float32` `float64` |
| 其他标量 | `bool` `char` |
| 字符串 | `String`（即 `std::lang::String`） |
| 大数 / 高精度 | `BigInteger` `BigDecimal` |

`char` 保存一个 UTF-8 字节，取值范围是 0–255，与 `uint8` 共用 u8 运算及单字节内存读写路径。
`char[]` 和 `uint8[]` 的相邻元素地址相差一个字节，加载零扩展，存储截断高位，算术按 u8 回绕。
字符字面量必须恰好解码为一个字节；中文等多字节文本使用字符串，其索引和长度按 UTF-8 字节计算。
结构体仍遵循字段槽布局，字节字段的实际读写宽度不代表结构体整体采用紧凑排列。

**整数的隐式转换只放行「变宽」**：同符号时目标位宽不减（`int8 -> int64`、`uint8 -> uint32`），
无符号到有符号要求目标**严格更宽**（`uint8 -> int16` 可以，`uint8 -> int8` 不行）；
整型 → 浮点、`float32 -> float64` 也自动放行。其余方向都要显式强转
`(int32)x` / `(char)x` —— 缩窄（`int64 -> int32`）、有符号到无符号（`int32 -> uint32`）、
浮点到整数、`char` / `bool` 与整数互转。

整型字面量落进整型槽位时按**值**判范围：`uint8 b = 255;` 可以，`int8 a = 200;` 报
「整数字面量超出取值范围」（不再静默回绕）；十六进制 / 二进制字面量（`0xFF` / `b1010`）
按**位模式**处理，不做范围检查。

**没有类型上下文的整数字面量默认是 `int64`，一进表达式就按上下文定型**：

- 赋值 / 初始化 / 实参 / `return` 的目标类型会往下传：`int32 a = 1 + 2;` 里的两个字面量
  都按 `int32` 定，整个表达式也是 `int32`。
- 二元算术 / 位运算里**另一侧操作数的类型**也说了算：`int32 x = 1;` 之后 `x + 1` 里的
  `1` 取 `int32`，不必写 `(int32)1`；`int8 n = 100; n + 20` 同理。
- 比较与逻辑不参与这层定型（结果不是操作数类型），没有上下文、两边都是字面量时
  （`auto n = 1 + 2;`）仍是 `int64`。
- 定型只看**装不装得下**：字面量超出目标类型范围就退回默认 `int64`，由「缩窄要强转」
  报错，不会静默截断。

可运行验证见 [`literal_context_type.mgl`](../example/samples/literal_context_type.mgl)。

### 复合类型

| 写法 | 含义 |
| --- | --- |
| `T a[N]` | 长度写进类型的定长数组 |
| `T a[]` | 长度运行时决定的数组，`new[n]` 创建 |
| `(A, B)` | 元组，成员按 `.0` / `.1` 读 |
| `R fn(A, B)` | 函数类型 |
| `T mut&` / `T const&` | 可变引用 / 只读引用（`T &` ≡ `T const&`） |
| `Option<T>` | 可能有值：`Value(v)` / `None` |
| `Result<T, E>` | 可能失败：`Ok(v)` / `Err(e)` |
| `List<T>` `Map<K,V>` `Set<T>` | 内建容器 |
| `std::lang::Any` | 类型擦除的盒子，用于运行期多态 |

---

## 3. 声明

```magel
const int64 LIMIT = 100;   // 常量
int64 counter = 0;         // 变量
auto inferred = LIMIT;     // 类型推导
```

- 顶层声明是全局的，函数内是局部的。
- 结构体字段、静态字段见 §6。
- 数组长度可以是常量表达式。

---

## 4. 函数

```magel
int64 add(int64 a, int64 b) { return a + b; }
void log(String message) { println("{}", message); }
```

- 参数默认是**按值**传入（副本）。要避免复制或被修改，写 `T mut&` / `T const&`。
- 运行时数组也按值传参：`T[] items` 复制长度及元素，修改副本不影响实参。
  `T[] & items` 与 `T[] const& items` 是只读引用，`T[] mut& items` 是可变引用。
  引用绑定使用 `&array`；已有数组引用可以直接转传。只读引用不能写入元素、
  取得元素的可变引用或转传给可变引用形参，局部数组的引用不能逃出其生命周期。
  拥有资源的数组元素沿用各自的复制与释放规则。
- 支持重载：按实参类型选。
- 变长尾参写 `T... items`，配合 `for-in` 或 `std::iter`。
- `static` 方法是类型级函数，用 `Type::method()` 调。
- 入口名为 `main`，返回类型为 `int64` 或 `void`，参数为空或 `std::lang::List<std::lang::String> arguments`。`void main` 正常完成或执行 `return;` 时，进程退出码为 0；panic 仍以失败退出。

### 多值返回与泛型元组

函数的多值返回类型写成元组。`return a, value;` 等价于 `return (a, value);`，
类型参数可出现在任意元组元素中，并随函数实例化或实参推导替换；已有赋值目标的类型也可参与返回类型推导。

```magel
(int32, T) func<T>(T value) {
    int32 a = 1;
    return a, value;
}
int64 main() {
    String str1 = "aaa", str2;
    int32 c;
    c, str2 = func<String>(str1);
    assert(c == 1 && str2 == "aaa");
    return 0;
}
```

`c, str2 = ...;` 和 `(c, str2) = ...;` 向**已有左值**赋值；声明新变量仍写 `auto (c, str2) = ...;`。
右侧完整元组先求值并保存一次，再按从左到右的顺序写入各目标，因此 `a, b = (b, a);` 和 `(a, b) = (b, a);` 都可以交换值。
目标可以是变量、可写字段、数组或容器元素、可写引用；`a, (b, c) = ...;` 对应嵌套元组。
元组的数量、嵌套形状和每个目标的类型必须匹配，const 变量与只读引用不能写入。
此语句沿用普通赋值的复制、析构、引用与生命周期检查，不会让元组自身的 `.0` 等成员变为可写。
单值 `return value;` 和原来的括号元组写法保持原有语义。

<a id="langmove复制后销毁源值"></a>
<a id="langmove转移源值所有权"></a>

### `move`：转移源值所有权

语言内建 `move` 的签名为 `T move(T mut& source)`，结果类型从引用目标推导，无须导入模块。

```magel
struct Message { String text; };
int64 main() {
    Message source;
    source.text = "hello";
    Message result = move(&source);
    println("{}", result.text);
    return 0;
}
```

执行顺序是：实参求值一次 → 将原有资源句柄及聚合存储槽转移给结果 → 清空源存储并结束源值生命周期 → 返回拥有型结果。移动期间不深拷贝字符串、容器、Any 或闭包，也不调用源值的 `close` / `drop`；接收方最终负责析构。引用字段仍指向原来的借用对象，移动不会延长借用寿命。

仅接受 `T mut&`；普通值、只读引用和 const 来源都会编译报错。标量、字符串、容器、Any、函数值、具体结构体和文件／进程句柄可以移动。数组、无具体布局的接口值和不透明宿主类型仍不支持直接移动；可以移动数组中的具体元素。

源值在静态可跟踪的控制流与别名路径中不能被读取或再次移动，包括标量；禁止通过重新赋值恢复；须重新声明新的同名绑定后使用。整个变量在所有到达路径上完整移动后，允许重新声明同名变量。部分字段移动后，其他不重叠字段仍可使用，但不能读取完整聚合。

源聚合及其嵌套字段的生命周期标记确保退出作用域时不会重复析构。`new` 分配的手动引用被移动后保留原存储，仍须 `drop` 该引用以释放存储块。普通赋值和普通按值参数继续采用复制规则；需要传递而不复制时使用引用形参，在需要接管结果的位置显式 `move`。移动结果直接用于初始化、赋值、返回及临时值接收时，转移拥有字段。

参见 [资源所有权移动](23-ownership-move.md)。

### 默认参数

形参后可以写 `= 表达式`，调用时省略连续的尾部实参：

```magel
int64 add(int64 a, int64 b = 10) { return a + b; }
String greet(String name = "Magel") { return "Hello, " + name; }
// add(2) == 12；add(2, 3) == 5；greet() == "Hello, Magel"
```

普通函数、实例方法、静态方法和构造器均支持默认参数，泛型函数可从显式实参推导类型。
默认值在每次省略该实参的调用时求值；显式传参时不求值。表达式中的名称在定义模块的
全局作用域解析，不受调用方局部同名变量影响，不能引用形参或 `self`。
默认参数之后只能继续声明默认参数或最后一个变长参数；位置实参仍从左向右绑定，不能跳过中间参数。

默认值不属于函数类型或重载签名。重载匹配允许省略有默认值的尾参，沿用类型精确匹配优先、
其次数值加宽的规则；同一档有多个候选时报歧义，不按省略参数的数量猜测。
默认尾参省略导致歧义时，诊断会列出各候选的参数类型、名称、默认值标记和省略的参数，
并提示显式补充能区分候选的实参或调整重载签名。
函数指针和 lambda 的调用仍须按其 `fn(...)` 类型传入完整的固定实参。
枚举载荷字段与接口方法声明暂不支持默认参数。

### 构造器与 `new`

结构体可以定义构造器，调用时用 `new(...)`：

```magel
struct Counter {
    static int64 alive;
    int64 value;
    Counter(int64 value) { self.value = value; Counter::alive = Counter::alive + 1; }
    static int64 count() { return Counter::alive; }
};
int64 main() {
    Counter c = new Counter(7);
    println("value={} alive={}", c.value, Counter::count());
    return 0;
}
```

```text
value=7 alive=1
```

堆上对象写 `T mut& p = heap T;`（**不写构造实参**：元素类型由 `p` 的类型定），用 `p.drop();` 释放。

**用哪个构造器由声明的类型决定**：

| 写法 | 走哪条路 |
| --- | --- |
| `T v = new T(实参);` | 栈上值，调 `T` 的**真构造器**（与 `T` 同名）；没有匹配的真构造器时落到具名构造入口 |
| `T mut& p = heap new T(实参);` | 堆对象，同样调真构造器，记得 `p.drop();` |
| `T v = T::create(实参);` | **具名构造入口** `static T create(...)` |
| `T v = T::其他工厂();` | 普通静态工厂（名字任取） |

```magel
struct Point {
    int64 x;
    int64 y;
    Point(int64 x, int64 y) { self.x = x; self.y = y; }             // 真构造器
    static Point create(int64 x, int64 y) { self.x = x * 10; self.y = y * 10; }  // 具名构造入口
};
int64 main() {
    Point o = new Point(3, 4);          // 真构造器 → 3,4
    Point mut& p = heap new Point(1, 2);  // 真构造器 + 堆 → 1,2
    Point q = Point::create(3, 4);        // 具名构造入口 → 30,40
    println("{},{} {} {} {} {}", o.x, o.y, p.x, p.y, q.x, q.y);
    p.drop();
    return 0;
}
```

```text
3,4 1 2 30 40
```

`static T create(...)` 是**具名构造入口**（固定使用 `create`），隐式带 `self`：体里往 `self`（或 `self.字段`）写值即可，
**不写 `return`**，返回值由调用点接住。丢弃它的返回值会报错。

泛型结构体的 `create` 可从实参推导所属类型的参数，例如 `Pair::create(key, value)` 与 `Pair::create((key, value))`。实参不能唯一确定类型时，显式写出 `Pair<K, V>::create(...)`。

`new` / 构造相关的写法汇总：

| 写法 | 含义 |
| --- | --- |
| `heap T;` | 在堆上分配 `T`，类型由左值推导（必须赋给 `T mut&`） |
| `new T(实参);` | 显式写出构造类型 `T` 的构造调用 |
| `heap new T(实参);` | 堆上构造（左值为 `T mut&` 时即堆构造） |

旧的裸写法 `T v = new;` / `T v = new(实参);` 已删除，一律按语法错误处理。

```magel
struct Point {
    int64 x;
    int64 y;
    Point(int64 x, int64 y) { self.x = x; self.y = y; }
    static Point create(int64 x, int64 y) { self.x = x; self.y = y; }
};
int64 main() {
    Point mut& a = heap Point;            // heap T
    Point mut& b = new Point(3, 4);       // new T(args)
    Point mut& c = heap new Point(5, 6);  // heap new T(args)
    println("{} {} {} {}", a.x, b.x, b.y, c.y);
    a.drop(); b.drop(); c.drop();
    return 0;
}
```

`heap` 是保留关键字（`HEAP` token），不能用作变量名；它只在 `heap T` / `heap new T(...)` 引导的
分配位置有意义。`heap T` 的 `T` 只作文法识别，真正的分配 / 构造类型仍由左值推导，
因此 `Point p = heap Point;`（左值是值而非引用）依旧报错，与旧规则一致。

---

## 5. 运算符

```magel
int64 main() {
    int64 a = 17;
    int64 b = 5;
    println("{} {} {} {} {}", a + b, a - b, a * b, a / b, a % b);
    println("{} {} {}", a & b, a | b, a ^ b);
    println("{} {}", a << 1, a >> 1);
    println("{} {} {}", a > b, a == b, (a > 10 && b < 10) || false);
    a += 3;
    println("{}", a);
    return 0;
}
```

```text
22 12 85 3 2
1 21 20
17 8
1 0 1
20
```

优先级与 C 一致：一元 > 乘除取模 > 加减 > 移位 > 关系 > 相等 > 位与 > 位异或 > 位或 > 逻辑与 > 逻辑或。拿不准就加括号。

复合赋值 `+=` `-=` `*=` `/=` `%=` `&=` `|=` `^=` `<<=` `>>=` 都有。

`++` / `--` 按实际目标检查冲突：同一完整表达式内，自增减目标不能再被其他操作读取或写入。
因此 `i = i++`、`i += i++`、`i++ + i`、`i++ + ++i` 会报告 `MGL1301`；
`a = (i++) + (j++)`、`a += i++` 和 `f(i++, j++)` 在目标互不影响时合法。
不同字段和不同常量下标可独立更新；动态下标无法证明不同位置时保守判为可能重叠，直接引用别名按来源判断。
括号、实参和短路分支都属于所在完整表达式，语句、声明初始化式及 for 三个部分分别检查。

赋值目标的位置只求值一次：`a[f()] += 1` 只调用一次 `f()`，`a[i++] = 1` 和
`a[i++] += 1` 都只让 `i` 自增一次。复合赋值先保存位置，再读取原值、计算结果并写回。
数组元素 `a[i]` 与下标变量 `i` 是不同目标，因此下标自增本身合法。
可运行验证见 [`expression_effects.mgl`](../example/samples/expression_effects.mgl)。

结构体可以重载运算符：继承 `Add` / `Subtract` / `Multiply` / `Divide` / `Modulo` /
`Equal` / `NotEqual` / `Less` / `LessOrEqual` / `Greater` / `GreaterOrEqual` / `And` /
`Or` / `Xor` / `ShiftLeft` / `ShiftRight` 这些**左接口**（方法名 `leftAdd` 这类），
右操作数那侧要单独实现就继承 `RightAdd` 这类**右接口**（方法名 `rightAdd`）。
接口的类型实参写作 `<返回类型, 另一操作数类型>`。

**能推的就不用手写**：

- **交换律**：`+` `*` `==` `!=` `&` `|` `^` 满足交换律，只声明了左接口时 `5 + a` 与
  `a + 5` 一样能用（自动把两个操作数换个位置）。减法 / 除法 / 取模 / 大小比较 / 移位
  **不交换**，反向必须显式声明 `RightSubtract` 这类右接口。
- **比较对偶**：只声明 `Less` 时，`5 > a`（对偶，等价于 `a < 5`）、`a >= 5`（取反，
  `!(a < 5)`）、`5 <= a`（对偶 + 取反）都推得出来；`a > 5` 推不出来 —— 那要
  `int64 < Vec`，得给 `Vec` 补 `Greater`。`==` 与 `!=` 互相取反。
- **显式声明优先**：同时写了 `Add` 与 `RightAdd`，`5 + a` 走 `RightAdd`，不会被交换律顶掉。
  `&&` / `||` 是内建短路运算符，**不参与重载**（报错会直接说明这一点）。
- **右向开关**：交换律与对偶都会把右操作数借来当接收者，所以写 `5 + a` 时要看 `a` 那侧
  表了什么态 ——
  - **什么都没写**：按上面两条推，`a + 5` 与 `5 + a` 都能用；
  - **写了 `RightAdd`**：右向归你自己，`5 + a` 只认 `rightAdd`；形参接不住就报错，
    **不会**退回借 `Add` 兜底；
  - **写了编译器指令 `#NoRightAdd`**：明令禁止，`5 + a` 报错。它写在 `struct` 前面，
    名字是 `#NoRight` + 左接口名（`#NoRightAdd` / `#NoRightLess` …），没有类型实参，
    也不进继承列表；左向 `a + 5` 不受影响。`#NoRightXxx` 优先于 `RightXxx`。
- **关掉推导**：在 `struct` 前写编译器指令 `#NoAddReplace`，这个类型的 `Add` 就**不再参与
  任何自动推导** —— `5 + a` 不换边、`a >= 5` 也不由 `Less` 取反推出来。名字是
  `#No` + 接口名 + `Replace`（`#NoLessReplace` / `#NoMultiplyReplace` …），可以连写多条。
  接口必须是本类型确实实现、且本来有推导可关的二元接口，写错报错。它只关推导，不动直接
  写法：左向 `a + 5` 照常调用 `leftAdd`，自己显式实现 `RightAdd` 也照常走。与 `#NoRightAdd`
  的分工是：`#NoRightAdd` 只关右向那一条路，`#NoAddReplace` 关掉这个接口的所有推导。
- **求值顺序**：换边只是把两个操作数换个位置**调用**，求值顺序仍按源码从左到右
  （`bump() + makeA()` 先跑 `bump()`，再 `makeA()`）。
- **同一对操作数、不同返回类型**：一个类型可以同时继承 `Add<int64,int64>` 与
  `Add<Vec,int64>` —— 两个 `leftAdd(int64)` 只在返回类型上不同。普通方法重载做不到这件事
  （C++ 也一样，光靠返回类型分不开），运算符接口**开了这个口子**：方法修饰名会带上返回
  类型，两条都活下来。挑哪条由**目标类型**定 —— `int64 x = a + 3;` 走返回 `int64` 的，
  `Vec y = a + 3;` 走返回 `Vec` 的。没有目标类型（`auto z = a + 3;`）会报错让你指明，
  **不会悄悄挑一条**。

可运行验证见 [`operator_symmetry.mgl`](../example/samples/operator_symmetry.mgl)、
[`operator_right_direction.mgl`](../example/samples/operator_right_direction.mgl)、
[`operator_result_choice.mgl`](../example/samples/operator_result_choice.mgl)、
[`operator_swap_eval_order.mgl`](../example/samples/operator_swap_eval_order.mgl)、
[`no_replace.mgl`](../example/samples/no_replace.mgl)（`#NoAddReplace` 关推导）；
报错例子见
[`operator_asymmetric_reverse.mgl`](../example/samples-error/operator_asymmetric_reverse.mgl)（只声明左减法）、
[`operator_right_forbidden.mgl`](../example/samples-error/operator_right_forbidden.mgl)（`#NoRightAdd` 禁止右向）、
[`operator_right_explicit.mgl`](../example/samples-error/operator_right_explicit.mgl)（`RightAdd` 形参接不住）、
[`operator_no_add_replace.mgl`](../example/samples-error/operator_no_add_replace.mgl)（`#NoAddReplace` 关掉换边）、
[`operator_result_ambiguous.mgl`](../example/samples-error/operator_result_ambiguous.mgl)（没有目标类型）、
[`logical_operator_overload.mgl`](../example/samples-error/logical_operator_overload.mgl)（`&&` 不可重载）。

---

## 6. 结构体

`struct`、`enum` 和 `interface` 声明在右花括号 `}` 处结束，末尾分号可省略，原有 `};` 写法仍然有效。结构体字段、接口抽象方法和普通语句的分号要求不变。

```magel
struct Point {
    int64 x;
    int64 y;

    int64 sum() { return self.x + self.y; }
    static Point origin() { Point p; p.x = 0; p.y = 0; return p; }
};
```

- 字段与方法在同一对花括号里，方法体里用 `self` 指代自己。
- `static` 字段/方法属于类型，用 `Point::origin()` 调。
- 可实现接口：`struct Circle : Shape { … }`，多个接口用逗号分隔，必须完全实现。
- 可实现 `Drop`（析构）与 `Closeable`（提前关闭），见[核心概念](03-core-concepts.md#4-无垃圾回收确定性释放)。
- 构造器见 §4。

---

## 7. 枚举与 match

```magel
enum Message {
    Quit,
    Move(int32 x, int32 y),
    Text(String text)
};
```

变体可带字段（代数数据类型）。`match` 处理：

```magel
void show(Message message) {
    match (message) {
        Message::Quit => { println("quit"); }
        Message::Move(x, y) => { println("move {} {}", x, y); }
        Message::Text(text) => { println("text {}", text); }
    }
}
```

match 检查**穷尽性**：漏掉变体是编译错误。`_` 兜底。

### 标量 match：常量与区间

```magel
int64 grade(int64 score) {
    match (score) {
        100 => { return 3; }
        std::lang::rangeEq(90, 99) => { return 2; }
        std::lang::rangeEq(60, 89) => { return 1; }
        _ => { return 0; }
    }
    return 0;
}
```

整数和 `char` 的区间模式使用标准调用：

| 写法 | 含义 |
| --- | --- |
| `std::lang::range<T>(a, b)` | `a <= x < b` |
| `std::lang::rangeEq<T>(a, b)` | `a <= x <= b` |

端点类型能够推导时可以省略 `<T>`。区间模式只适用于整数与 `char`；浮点和 String 的有序条件使用 `if` 守卫。自定义类型实现 `Equal<T>` 后可以作为 match 主体，分支值通过该协议比较；主体只求值一次，分支仍按源码顺序选择。

整数 match 会对能在编译期确定的模式检查覆盖范围：`rangeEq(15,25)` 与 `rangeEq(10,20)`、`15 =>` 与 `rangeEq(15,20)` 均重叠，报告 `MGL1601` 警告。`range(10,15)` 与 `rangeEq(15,20)` 不重叠，空区间不参与检查。比较保留完整 int64/uint64 精度，并只考虑主体整数类型可表示的值。常量表达式端点参与检查，运行时端点不作静态推断。

警告不阻止编译或改变分支顺序；模式仍从前到后检查，有守卫时先匹配模式再检查守卫。带守卫的模式也可能收到覆盖范围重叠警告，但这不表示该分支必然不可达。重复的无守卫单值模式仍沿用已有编译错误。命令行显示警告位置，JSON 使用 `severity: "warning"`，编辑器以警告显示。

### 守卫

绑定变量后再加条件，用 `if`：

```magel
enum Choice { Value(String text) };
int64 kind(Choice choice) {
    match (choice) {
        Choice::Value(text) if text == "good" => { return 1; }
        Choice::Value(text) => { return 0; }
    }
    return -1;
}
```

```text
kind(Choice::Value("good")) = 1
kind(Choice::Value("bad"))  = 0
```

---

## 8. 接口与运行期多态

```magel
interface Shape {
    int64 area();
    String describe() { return "shape"; }   // 默认实现
};
```

- 接口里只有方法，可以有默认方法体，**不能定义字段**。
- 接口可继承：`interface B : A { … }`。
- 结构体可实现多个接口，同名同签名的要求只算一条（去重）。
- 接口名可作引用目标类型：`Shape mut& view`。那是"胖引用"（地址 + 类型号），`.area()` 在**运行期**按类型号挑实现。

接口视图可直接绑定实现类型的引用，声明、赋值、实参及返回值都适用，不需要 Any 中转：

```magel
Circle circle;
circle.r = 4;
Shape const& view = &circle;
```

只读来源不能绑定可写视图；复制接口视图不延长具体对象的寿命，局部对象的视图不能逃出作用域。已经装箱的 Any 仍可通过 `getReference`／`tryGetReference` 提取接口视图。

完整例子见 [`example/samples/interface.mgl`](../example/samples/interface.mgl)。Any 的安全提取使用 `isType<T>(&box)`、`tryGet<T>(&box)`、`tryGetReference<T>(&box)` 和 `tryGetMut<T>(&box)`；`nameof` 与盒子的 `.name()` 已删除，见 [Any、显式泛型与通用数据工具](22-any-generics-data.md)。

---

## 9. 泛型

```magel
int64 total<T: Shape>(T shape) { return shape.area(); }
```

- 类型参数写在名字后面：`<T>`、`<T, U>`。
- 约束写 `T: Interface`，多个约束用逗号。
- 泛型结构体同样支持：`struct Triple<T: Valued, K, V> { … }`。
- 编译期单态化，调用点静态确定，没有虚表开销。
- 调用可显式写 `total<Circle>(c)`，也可让编译器推。

支持在外层模板内部分绑定内层模板的参数；外层仍保持泛型，使用具体类型时才递归生成完整实例：

```magel
struct TypeC { int64 value; };
struct TypeA<T, V> { T first; V second; };
struct TypeB<T> { TypeA<T, TypeC> item; };

int64 main() {
    TypeB<String> b;
    b.item.first = "nested";
    b.item.second.value = 7;
    assert(b.item.first == "nested" && b.item.second.value == 7);
    return 0;
}
```

`TypeB<String>` 的字段会成为 `TypeA<String, TypeC>`；`TypeB<int64>` 得到独立的布局、方法与静态字段。
同一模板与相同的归一化类型实参复用实例，例如 `String` 与 `std::lang::String` 不会重复生成；唯一的导入类型名称与其模块限定名称也复用同一实例。
可以继续嵌套、交换参数位置，也可以把实例用于泛型函数、接口约束、枚举载荷、容器、元组、函数值和引用。
方法里的 `item.first`、`item = value`、`&item` 与显式 `self.item` 使用相同的字段布局和所有权规则。

默认类型参数写作 `struct Box<T, V = T, W = TypeA<T, V>> { W value; };`。
默认类型可以引用前面的参数，按从左到右绑定并递归展开；不能引用自身或后面的参数。
泛型函数推导中未绑定且具有显式默认类型的参数同样采用此规则；没有显式默认类型的未绑定参数会报错。
缺少的类型参数不再自动采用 `Any`；必须显式指定，或由模板声明明确给出默认值。接口约束在具体嵌套实例生成后统一检查。
单态化保留字符串、字符与注释的内容，不把其中的 `T` 改成类型名。

此处的部分绑定是模板组合；普通泛型类型和函数目前没有按参数模式另写实现体的模板偏特化语法。泛型闭包的特化见后文。
值字段不能直接或间接包含自身；同一实例的引用递归允许存在。不断扩大的递归类型会报错，
每次编译最多生成 1024 个泛型实例，类型表达式嵌套达到 128 层时停止展开并给出诊断。
综合回归见 [`tests/magel_generic_monomorphization.py`](../tests/magel_generic_monomorphization.py)，
覆盖VM 执行、实例复用及非法代码诊断。

---

## 10. 控制流

```magel
int64 main() {
    int64 i = 0;
    while (i < 3) { i = i + 1; }
    println("while={}", i);
    int64 n = 0;
    loop {
        n = n + 1;
        if (n >= 3) { yield n; }
    }
    println("loop={}", n);
    int64 total = 0;
    {
        int64 k = 0;
        while (k < 5) {

        if (k == 1) { { k = k + 1; continue; } }
        total = total + k;

            k = k + 1;
        }
    }
    println("for={}", total);
    return 0;
}
```

```text
while=3
loop=3
for=9
```

| 结构 | 说明 |
| --- | --- |
| `if / elif / else` | 条件分支 |
| `while (cond) { }` | 先判断后执行 |
| `loop { … yield v; }` | 无限循环，`yield` 交出结果 |
| `for (T x in c) { }` | 遍历容器/迭代器 |
| `break` / `continue` / `return` | 跳出 / 继续 / 返回 |

> `loop { … }` 是**表达式**；写在语句位置时不补分号，被变量定义、赋值、实参或返回值接走时
> 按外层语句收尾（`int64 v = loop { … };`）。

---

## 11. 容器

```magel
int64 main() {
    List<int64> xs;
    xs.push(1);
    xs.push(2);
    xs.push(3);
    println("len={} at1={} has2={}", xs.len(), xs.at(1), xs.has(2));
    Map<String, int64> m;
    m["a"] = 1;
    println("m={}", m["a"]);
    return 0;
}
```

```text
len=3 at1=2 has2=1
m=1
```

| 容器 | 常用操作 |
| --- | --- |
| `List<T>` | `push` `pop` `len` `at` `[]` `has` `remove` `clear` |
| `Map<K, V>` | `[]` `has` `remove` `len` `keys` `values` |
| `Set<T>` | `insert` `has` `remove` `len` |

集合字面量：`[1, 2, 3]` 是 `List`，`{1, 2, 3}` 是 `Set`，`{"a": 1}` 是 `Map`。空列表写 `List<T> values = [];`；空花括号只接受 Set 或 Map 上下文。

迭代器链（`map` / `filter` / `collect`）需要 `import std::iter;`，见[语言速览](02-language-tour.md#7-集合与迭代器)。

---

## 12. 模块与导入

```magel
module mylib::math;      // 声明本文件所属模块
import std::iter;        // 导入模块
import std::text;
```

- 模块用 `module 名字::子名字;` 声明，通常放在文件首行。
- `import` 后可写完整限定名，也可使用唯一可见的短名；同名冲突时使用限定名明确归属。
- 核心模块 `std::lang`（`println` / `format` / `assert` / `len` / `Any` …）自动加载，无需 `import`。
- 可见性、导出与跨模块泛型见 [`example/samples/import_names.mgl`](../example/samples/import_names.mgl)。

---

## 13. 引用速查

```magel
int64 value = 7;
int64 mut& alias = &value;    // 可变引用
int64 const& view = &value;   // 只读引用（等价 int64 &）
alias = 9;                    // 写引用 = 写 value
```

```magel
T mut& owned = heap T;         // 堆对象（元素类型由 owned 的类型定）
owned.drop();                  // 释放
```

规则与限制（逃逸检查、`MGL1204`）见[核心概念](03-core-concepts.md#2-引用非空且不能活得比目标久)。

---

## 14. 预处理与宏

`%if`、`%elif`、`%else`、`%endif` 控制源码条件编译。标准模块用 `import` 导入。

`$macro A` 使用 `$A`，`$macro A()` 使用 `$A()`，有参数的定义使用相同的显式参数组。生成解析器建立宏 AST，语义分析进行卫生替换和类型推导，四元式生成前完成展开。普通模板局部声明自动卫生，实参保留调用者绑定，辅助名字在定义处查找。无需自动括号，也不使用编号临时变量。

宏模板中的辅助名称必须在定义处可见；调用者的同名局部变量不能补足缺失的声明。辅助名称未声明时，编译器报告未声明变量，不将它作为整数参与块表达式结果类型匹配。

```magel
$macro ADD<T:a>(a, b)
cast<T>(a + b)
$end_macro
int64 main() {
    int32 value = 1;
    int32 result = $ADD(value, 2);
    return 0;
}
```

`<T:a>` 从实参静态类型推导；`<T>` 从外层期望结果类型推导，显式类型优先。`[]`、`()`、`{}` 是严格有序参数组，`...rest` / `..rest` 是末尾变长形参，`$for` 遍历片段列表。`$match x is expr` 支持 `expr`、`ident`、`type`、`block`、`stmt`，`$else is` 顺序选分支，`$end_match` 结束。`$regex x is "模式"` 使用完整 ECMAScript 正则匹配原文，`$end_regex` 结束。未选分支不进入最终语义检查。

`$stringify(x)` 读取原文，`$concat(a, b)` 仅产生标识符，`$line` / `$file` / `$module` 保留来源。`$replace A(x) x + 1` 为单行宏。完整规则与可运行示例见 [卫生宏](24-literals-operators-macros.md#编译期卫生-ast-宏)。

## 15. 注解与反射

```magel
@annotation Route(path: String, level: int32 = 2)

@Route("/health", level: 1)
int64 health() { return 200; }
```

- `@annotation 名字(参数)` 声明一个注解类型，参数可带默认值。
- 注解可贴在结构体、接口、枚举、字段、方法、构造器、函数上。
- 运行期可查询声明上的注解、按名调用函数、做接口分派。

**构建要求**：根目录工具链默认同时编入 `MAGEL_WITH_REFLECTION=1` 与
`STACKVM_WITH_REFLECTION=1`，单次编译默认启用反射。`make reflection` 保留为显式启用入口。

- `--reflection` 显式要求支持；前端或运行时缺少能力时按配置报错。
- `--no-reflection` 显式关闭，关闭后使用注解或反射会被拒绝。
- 显式裁剪构建才覆盖两项宏为 0；不能用根目录默认值推断所有独立生成工程和 embedded Linux 宿主的配置。

反射产物要求目标 VM 编入反射；目标显式关闭反射时，编译源程序须使用 `--no-reflection`。反射当前的签名及动态调用边界见[反射说明](19-reflection.md)。

完整例子见 [`example/reflection/`](../example/reflection/)（`annotations.mgl`、`calls.mgl`、`dispatch.mgl`、`queries.mgl`）。

---

## 16. 与 C 的边界：`abi` 块

`opcode` 声明胶水拥有的不透明对象类型，记录布局由 ABI 或 C++ 胶水管理：

```magel
opcode Number {
    new = createNumber;
    drop = destroyNumber;
    copy = copyNumber;
};

#NonCopyable
opcode FileHandle {
    new = openFile;
    drop = closeFile;
};
```

`struct` 可以声明任意数量的 `opcode` 类型字段；每个字段分别维护自己的值与生命周期。

声明体必须绑定创建方法 `new` 和销毁方法 `drop`。可复制类型还必须绑定 `copy`，
复制方法须创建独立的宿主记录。`#NonCopyable` 类型省略 `copy`，禁止复制初始化、
赋值及按值传参、返回时的复制；它仍可通过 `T target = move(&source);` 转移所有权，
按值传参与返回也可显式使用 `move(&value)`。此规则适用于所有 opcode 类型。
实现 `Closeable` 的 struct 走同一条检查：复制一份就是两份所有权，`close()` 会跑两遍，
因此它自动按不可复制处理，无须再写指令（见[声明糖](28-declaration-sugars.md#closeable-与运算符推导开关)）。
移动不调用 copy，源绑定随后失效，最终接收所有权的一方负责调用 drop。
`Type::new(...)` 调用创建方法。opcode 是内部保存不透明胶水句柄的特殊结构体，声明体还可包含
普通成员变量、实例方法及静态方法。普通字段由语言保存、复制和销毁；它们不描述宿主对象的内存布局。
方法可读取 `self.opcode` 并将其传给胶水。该内部句柄仅允许在声明模块内访问，创建后只读，
不能赋值、复合赋值或取得可写引用。反射沿用 `@Reflect` 暴露规则，可读取 opcode 对象及公开字段，
但排除内部 `opcode` 句柄。语言服务不提示该内部句柄。这套机制不改变字符串与数组的值语义。
`String`、`BigInteger`、`BigDecimal`、`File` 和 `Process` 均由 `std::lang` 中的
`opcode` 声明定义，前端不为它们保留独立的 `Kind` 枚举项。`string` 不是 `String` 的类型别名。

```magel
abi "./native.dll" {
    int64 c_function(int64 x) = "c_function";
}
```

`abi` 块声明按 C ABI 调用外部库函数。宿主通过共享 VM 加载与运行程序。类型映射、可空引用、胶水绑定详见[宿主集成与 C ABI](08-host-integration.md)。

---

## 17. 常用内建函数

| 函数 | 作用 |
| --- | --- |
| `println(fmt, …)` | 格式化输出并换行 |
| `format(fmt, …)` | 格式化为字符串 |
| `assert(cond)` | 断言，失败即中止 |
| `len(x)` | 字符串 / 容器长度 |
| `println` 里的 `{}` | 位置占位符，类型由实参推导 |

更多能力（`json` / `toml` / `regex` / `sql` / `net` / `linalg` …）见[标准库概览](07-stdlib-overview.md)。

---

## 18. 下一步

| 你想 | 去哪 |
| --- | --- |
| 权威文法 | [EBNF 语法参考](14-grammar-ebnf.md) |
| 更多可运行样例 | [`example/samples/`](../example/samples/) |
| 预期报错的写法 | [`example/samples-error/`](../example/samples-error/) |
| 命令行与编译选项 | [工具链使用](09-toolchain.md) |

### 销毁、释放与临时所有权

显式释放统一调用 `value.drop()`：堆引用、数组及资源值使用同一调用形式，不提供独立释放语句；实现 Drop 的结构体继续沿用协议方法。它们共用资源终结与合并机制。底层释放是非幂等的操作，不能重复销毁同一生命周期。编译器在控制流图上自动合并可证明属于同一生命周期的重复 drop，释放后读写仍报错；跨调用等无法静态判定且未合并的重复释放由运行时报告 `MGL1205`。重新赋值开始新生命周期；`const &` 仍不能调用 `drop()`。释放后读写以及移动后读取产生 `MGL1205`；已移动的原绑定也禁止写入，只有新的声明可以创建可用绑定。默认堆复用物理内存但不复用逻辑分配身份，未被编译器合并的旧引用再次释放会报错且不影响新对象。释放立即移除活跃记录，不生成墓碑，也没有清扫 API。打印、格式化和文件写入等借用实参的调用会将新建字符串保存在完整表达式的拥有型临时变量中，调用完成后清理；所有权转移则清空源槽。分支上的显式释放不会取消其他路径的自动清理。详见[借用控制流与算法验证](11-algorithm-validation.md)。

函数入口 `main` 的返回类型必须是 `int64` 或 `void`，不接受 `uint64`、较窄整数、浮点数或引用。函数指针可作数组元素：`int64 fn(int64) a[] = new[2];`，赋值后可用 `auto callback = a[0]; callback(value);` 调用。固定数组、元组、容器和嵌套函数类型可组合；返回运行时数组的函数类型写作 `(int64[]) fn()`，圆括号将数组返回类型与函数指针数组区分开。

函数类型和匿名函数的返回类型统一前置：`int64 fn(int64, int64) callback`、`int64 fn(int64 x) { x + 1 }`。旧 `fn(int64, int64) -> int64` 与 `fn(int64 x) -> int64 { x + 1 }` 已移除并报错；推导返回类型使用 `fn(int64 x) { x + 1 }`，不写箭头。升级后重新生成泛型宿主胶水并重建旧缓存。

同一块内，变量完整移动后可以用声明语法重新定义同名变量（也可使用不同类型）：`String a="1"; String b=move(&a); String a="2";`。新声明分配独立存储，旧引用仍指向旧对象，不能访问已移动对象。所有到达新声明的控制流路径都必须已完整移动旧变量；只移动字段或仅部分分支移动时，不允许重复声明。

返回函数值的匿名函数也可以将返回类型括起：`(int64 fn(int64)) fn(int64 fn(int64) a, int64 fn(int64) b) { int64 fn copy(int64 x) { a(b(x)) } }`。外层末尾的闭包表达式与内层末尾的调用表达式都不加分号，分别返回函数值与整数；返回的闭包必须拥有 `a`、`b`，不能借用本次调用的局部形参。完整例子见[函数值与闭包](book/15-函数值与闭包.md)。

## fn 捕获方式

外部局部变量默认按 `const&` 捕获，`fn const(...)` 与默认写法等价：能看到来源的新值，但不能修改来源。`fn mut(...)` 按 `mut&` 捕获并允许写回。`fn copy(...)` 捕获可修改的独立副本；`fn move(...)` 把来源的拥有值移入闭包，原绑定禁止读写，须重新声明新的同名变量后使用。复制闭包会复制内部拥有状态，引用捕获仍指向同一来源。按引用捕获不得超出来源寿命；拥有捕获仅在其中不包含短寿命引用时可以返回。捕获列表写作 `fn copy[a, b] mut[c] const[d] move[e](int64 x) { ... }`，未列出的外部局部变量默认按 `const&` 捕获；列表后可追加一个默认方式，如 `fn copy[a, b] mut(int64 x)` 表示其余变量按 `mut&` 捕获。默认方式只能声明一次，`copy[a, b] mut const(...)` 会报错。

```magel
int64 n = 1;
auto read = int64 fn() { n };
auto write = int64 fn mut(int64 x) { n += x; return n; };
auto counter = int64 fn copy[n]() { n += 1; return n; };
auto transferred = int64 fn move[n]() { n += 1; return n; };
n = 10;
assert(read() == 10);
assert(write(1) == 11);
assert(counter() == 2);
assert(transferred() == 2);
```

列表中重复的名称、不存在的变量、与形参重名的变量，以及对只读来源的 `mut` / `move` 捕获会被拒绝。显式列表中的变量即使未在函数体使用也会捕获。捕获的是整个变量；copy/move 一个引用值不会取得其所指对象的所有权，也不会延长所指对象的寿命。全局变量的未限定直接访问仍使用全局存储；需要快照时在列表中显式列出。数组值需先转换为容器或显式引用。文件和进程句柄可 move 捕获，无法复制；复制包含这些句柄的闭包会被拒绝。

同一变量先被引用捕获（默认 `const&` 或 `fn mut`）、随后被另一个闭包 `move` 捕获，并不单独构成错误：如果借用闭包的最后一次使用发生在 `move` 之前，可以通过检查。来源已移动、销毁或释放后再使用借用闭包，会报 `MGL1205`；复制闭包不会消除它对来源的借用。来源寿命不足而逃逸的闭包报 `MGL1204`。需要两个闭包继续独立使用时，可用 `copy` 保存快照；复制或移动引用值仍不会延长所指对象的寿命。此检查覆盖编译器可追踪的借用来源，原始内存及不透明宿主操作仍须遵守接口约定。

## 泛型闭包与特化

`auto f = fn<T>(T value) { value };` 定义一个带类型参数的闭包。`f(1)`、`f("text")` 从实参推导 `T`，`f<int64>(1)` 显式选择类型；返回类型由具体实现体推导。不能从实参确定且没有默认值的类型参数会报错，不自动退化为 Any。

完整特化和偏特化使用同一个闭包名称声明，实参先确定类型，再选择唯一最具体的匹配：

```magel
auto f = fn<T, U>(T first, U second) { 0 };
f<T, int64> = fn(T first, int64 second) { 1 };
f<int64, int64> = fn(int64 first, int64 second) { 2 };
assert(f(true, false) == 0);
assert(f(true, 3) == 1);
assert(f(2, 3) == 2);
```

特化在编译期选择，不能在运行时替换实现。重复定义和无法确定唯一最具体匹配的偏特化会报错。各实现沿用主定义的捕获方式，不重新捕获；同一个闭包在不同类型调用之间共享捕获状态。复制整个闭包时，拥有的状态独立复制，借用仍关联原来源。

结构体允许直接声明 `auto f = fn<T>(T value) { value };` 成员，并在结构体内声明该成员的特化；实例使用 `s.f(1)` 或 `s.f<String>("text")`。默认初始化按成员的声明作用域解析捕获，不捕获构造调用者的同名局部变量。现有闭包规则仍禁止隐式捕获 `self` 和接收者字段。

普通闭包和泛型闭包都可以作为泛型类型参数 `F` 的值，例如 `int64 apply<F>(F f) { return f(3); }`，调用 `apply(callback)` 自动推导 `F`。`Box<F>` 等泛型聚合可保存闭包值；泛型闭包保留声明身份及捕获环境，不被擦除为一个固定签名的普通函数指针。所有借用和移动限制继续适用。

### 外层泛型中的函数值声明

外层类型或函数的 `T` 具体化后，`T fn(T, int32)` 就有确定的返回类型和参数签名。
结构体字段及函数体内的局部变量都可以这样声明：

```magel
struct CallbackPair<T> {
    T fn(T, int32) a;
    int32 fn(T, int32) b;
};

int32 useCallbacks<T>(int32 c) {
    T fn(T, int32) a;
    int32 fn(T, int32) b;
    return c;
}
```

调用前须赋予有效函数或闭包；字段也可保存 `copy` 捕获闭包，复制聚合时复制其拥有的环境。
枚举可以把函数值声明为变体载荷，例如 `enum Callbacks<T> { Pair(T fn(T, int32) a, int32 fn(T, int32) b) };`。
变体列表之后目前只允许声明方法，不允许增加公共实例字段。普通 opcode 支持确定签名的函数值字段；
`opcode A<T>` 泛型声明目前不受支持。

## for-in 区间与前进协议

`for (auto item in source)`、`for (item in source)` 与显式类型写法共用迭代协议。`std::lang::range(a, b)` 表示左闭右开区间，`std::lang::rangeEq(a, b)` 包含两端；两种调用均可显式指定 `<T>`。

整数的八种固定宽度类型和 `char` 直接支持区间计算。`for (uint8 x in std::lang::rangeEq<uint8>(0, 255)) { ... }` 依次产生 256 项；换为 `range<uint8>` 则产生 255 项。两个端点从左到右各求值一次，均须能由 T 表示，包括半开区间的上端点。可折叠整数常量错误在编译期拒绝，动态端点在循环前检查；反向区间报错。`range(a, a)` 为空，`rangeEq(a, a)` 恰好产生一项。到达最大闭上界后直接结束，不计算最大值加一。

其他值类型需要实现 `NextValue<T>`，其方法为 `Option<T> nextValue(T end, bool inclusive)`。区间独立保存起始游标与终点；每轮在游标上调用该方法，由实现产生下一项、推进接收者状态并判断是否结束，结束时返回 None。自定义前进协议用于遍历；match 的 range / rangeEq 模式仅支持整数和 char。

## 带 else 的循环值表达式

`for-in` 与 `while` 可以作为初始化、赋值、实参或返回位置的值表达式，必须提供 `else` 结果。循环体中的 `yield value;` 结束整个值表达式并交出结果；没有交出值时，遍历结束、条件不成立或无值 break 会进入 else。`continue` 开始下一轮。单一值分支可写 `{ value }`，复杂分支用 `yield value;`。

```magel
int64 main()
{
    int64 first = for (auto value in std::lang::range(1, 8))
    {
        if (value % 2 == 0)
        {
            yield value;
        }
    }
    else { -1 };
    int64 fallback = while (false) { 1 } else { 7 };
    assert(first == 2 && fallback == 7);
    return 0;
}
```

所有可达结果分支按接收位置检查类型。String、数组与可复制聚合的结果保持值语义；不可复制 opcode 可以通过新建结果或显式 move 转移所有权，结果在循环局部资源清理之前保存。

## 指定层数退出循环

`break level N;` 从当前循环算起，退出第 N 层包围当前位置的循环，并从目标循环之后继续；`break level 1;` 等价于 `break;`。`break all;` 退出当前函数内所有包围当前位置的循环，从最外层循环之后继续；`break level all;` 与 `break all;` 等价。`level` / `all` 只在该语法位置识别，仍可作普通变量名。`break level` 之后必须跟正整数字面量层数或 `all`；`break level foo;`、`break level 0;`、`break level -1;` 均报错。

只计算 `loop`、`while` 和 `for-in`，普通块、赋值块、`if`、`match` 均不计层数。N 必须是正十进制整数字面量，不能使用变量或表达式；零、负数、超出当前嵌套层数以及在循环外使用均报错。退出不能跨越函数或 `fn` 边界。层级退出不携带值，目标若为需要结果的循环表达式则报错，仍应使用 `yield 值;` 提供结果。

```magel
loop {
    while (true) {
        {
            int64 i = 0;
            while (i < 10) {

            if (i == 3) { break level 2; }

                i++;
            }
        }
    }
    // 已退出 for 和 while，仍在 loop 内。
    break all;
};
```

跨层退出沿用作用域清理规则，从内到外执行 `defer`、`scope(exit)` 与资源清理，且不执行被跳过的循环后续语句或更新表达式。完整检查见[层级退出示例](../example/samples/break_level.mgl)。

## 块表达式与插值输出

`eprint`、`eprintln` 将文本写入宿主标准错误流；格式规则与 `print`、`println` 一致，
支持 `{}`、从 1 开始的位置索引、数值格式和插值字符串。`eprintln` 追加一个 LF 换行，
两者成功返回 `0`，不会把文本加入 VM 的标准输出结果。空参数 `eprintln()` 只输出换行。
单个 `String` 表达式按原文写入，不再解析其中的花括号或百分号。

文件使用 `File.print` / `File.println`、`FileStream.print` / `FileStream.println`，
或 `std::io::filePrint` / `filePrintln`，同样支持格式串与变长实参。普通文件接口返回
写入字节数，流接口返回 `Result<int64, IoError>`；换行计入字节数，关闭或只读错误沿用
既有写入行为。省略文本时，文件 `println` 只写一个 LF，不自动刷新或关闭文件。

```magel
import std::io;

void main() {
    eprintln("任务 {} 失败", 7);
    FileStream log = FileStream::open("run.log", "wb").take();
    log.print("count={}", 3).unwrap();
    log.println(" hex={:X}", 255).unwrap();
    log.close();
}
```

非空语句块可以产生一个值，用于初始化、已有变量赋值、实参或返回值。每条正常完成的路径必须通过 `yield 值;` 退出；普通语句块仍按原有规则执行，空 `{}` 保留集合字面量含义。`yield` 只在该糖里作关键字，作为普通标识符仍可用（如用户自定义的 `yield()`），但不能写成 `yield(参数)` 调用形式。

```magel
int64 main() {
    int32 a = { int32 b = 3; yield b; };
    a = { int32 b = 4; yield b; };
    print(f"a={a};");
    println(f"next={a + 1}");
    return 0;
}
```

`yield 值;` 选择最近接收结果的值出口，穿透中间的语句 while、for-in 及语句位置的 loop。内层用于初始化、赋值、实参或返回值的 loop、块、if、match 表达式仍接收自己的退出值。`break;` 只结束最近循环，不向外层表达式提供结果。`yield (a, b);` 以元组交出结果，多个值从左到右各求值一次；接收位置的元组类型逐项约束退出值。块表达式不建立循环，`continue;` 继续寻找外层循环。结果在清理块内局部资源之前保存，字符串与列表遵循值语义，不能返回借用已结束局部变量的引用。

例如，循环内可以直接完成外层块的赋值，嵌套赋值块仍各自接收结果：

```magel
(int64, String) found = {
    {
        int64 i = 0;
        while (i < 10) {

        if (i == 3) { yield (i, "found"); }

            i++;
        }
    }
    yield (-1, "missing");
};
int64 a = { int64 b = { yield 1; }; yield b + 1; }; // a == 2
```

独立花括号语句即使带尾分号也只是普通作用域，不自动建立可丢弃的值接收方；没有外层值表达式接收时，其中的 `yield 值;` 报错。嵌套初始化 `int64 y = loop { int64 z = loop { yield 5; }; yield z + 1; };` 中，内层退出给 z，外层退出给 y；最外层若仅为独立语句块，最后的 `yield y + 1;` 仍没有接收方。

完整的嵌套循环与清理检查见[带值退出示例](../example/samples/break_values.mgl)。

`print`、`println` 接受单个插值字符串或 `String` 表达式，按原文输出结果，不再解释其中的花括号和百分号。传统 `println("{}", value)` 格式保持有效；插值输出不再附加额外格式参数。

卫生变长宏可使用块表达式构造列表：

```magel
$macro vec(T, ...values)
{
    List<T> result;
$for item in values
    { T itemValue = item; result.push(itemValue); }
$end_for
    yield result;
}
$end_macro
```

调用 `List<int32> values = $vec(int32, 1, 2, 3);` 或 `values = $vec(int32, 4, 5);`。普通局部声明自动卫生；局部暂存按指定类型接收实参，每个元素只求值一次，再通过 `push` 顺序追加。省略元素得到空列表。完整可运行测试见 [块表达式样例](../example/samples/block_expression.mgl)与[插值输出样例](../example/samples/print_interpolation.mgl)。

匿名函数使用 `fn`，旧 `lambda` 语法已移除：`auto add = fn(int32 x) { x + 1 };`，显式返回类型写 `auto add = int32 fn(int32 x) { return x + 1; };`。`fn const`、`fn mut`、`fn copy`、`fn move` 及显式捕获列表沿用匿名函数规则；`int32 fn(int32)` 仍表示函数类型。

`fn` 表达式可以直接作为普通函数或函数值调用的实参，无须先保存到变量。每个回调按对应形参的
函数类型检查；省略的参数类型从该形参推导。嵌套函数返回值与元组组合见
[`fn_inline_arguments.mgl`](../example/samples/fn_inline_arguments.mgl)。

## 默认值文本与字符串协议

每个非 void 值支持 `String toString()`，并可转换到 `String`。整数展示数值、布尔展示 `true`/`false`；结构体按字段声明顺序展示为 `{值, 值}`，List/数组为 `[...]`，Set 为 `{...}`，Map 为 `{键: 值, ...}`，元组为 `(...)`。嵌套字符串加双引号并转义，顶层字符串返回自身文本。Any 按实际载荷展示，空 Any 为 `Any()`；枚举展示变体与载荷。引用、函数与不透明宿主资源显示 `<类型>`，不为格式化解引用。递归有深度限制并检测重复对象路径；文本不保证可逆序列化。

用户的无参 `String toString()` 自动成为缺少专用实现时的隐式字符串转换；已有 `ImplicitConvert<String>` 则补出转调其实现的无参 `toString()`。两者都显式存在时分别保持自身实现。`cast<String>` 优先专用 `ExplicitConvert<String>`，再复用隐式转换或默认值文本。只有 `toString(String style)` 时，无参版本传入空样式。标准 `ToString` 和 `ImplicitConvert<String>` 泛型约束认可默认能力；同名的用户接口不因此自动满足。

结构体的 `{}` 格式化复用其无参 `toString`；带样式重载保持原有行为。默认转换只求值一次并保留源值所有权，临时源正常清理，无须反射。嵌套的用户方法必须只读；容器快照隔离回调重入修改，回调失败保留原始诊断。完整例见 [默认值文本](../example/samples/value_text.mgl)与[书稿](book/12-字符串与文本.md)。

## 带值退出的移动与清理

显式 `yield i;` 优先移动当前值表达式内部的可写局部变量，等价于对该变量使用 `yield std::lang::move(&i);`；外部仍存活的变量按值复制。判断以接收结果的值表达式为边界，跨越循环不会改变边界。元组中的直接变量逐项应用同一规则，嵌套元组同样处理。const、只读来源、引用绑定、字段和下标保留已有值语义；需要显式转移来源时使用 `move(&value)`。存在作用域退出回调时保守保存独立结果，保证回调仍可读取或修改来源。

`yield i + 1;`、`yield makeText();` 等表达式只求值一次，再复制或接管结果临时值，不把表达式本身当作可寻址变量。元组等复合表达式先沿用现有构造与按值传参规则，再由退出接管最终结果。普通分支末尾的 `{ value }` 保留原有值语义。需要强制复制可使用字符串拼接、容器 copy 或类型提供的复制方法；需要强制移动外部变量可显式写 `yield move(&external);`，之后原绑定遵守移动后访问限制。

结果先保存，再执行退出路径上的延迟回调和自动清理。移动清空源存储并结束源绑定的生命周期，不调用源值的 close/drop；最终接收方负责资源回收。同时实现 Closeable 与 Drop 时先 close、后 drop。复制产生独立拥有值，来源与结果分别按各自生命周期清理。跨循环退出清理所有离开的作用域，不能释放已交给结果的资源。

移动不延长借用寿命，直接引用及聚合内的引用都不能指向已经退出的局部对象。[移动与清理样例](../example/samples/break_move.mgl)验证求值次数、原始资源句柄身份、外部变量复制、析构次数、清理顺序及 VM 行为；[所有权移动](23-ownership-move.md)给出通用规则。

## 返回控制表达式

`return { ... yield value; };`、`return if ...;`、`return match ...;` 和 `return loop ...;` 先在隐藏结果槽中接收值，再清理作用域并返回。所有结果分支类型必须完全一致。表达式内部的拥有值局部变量（包括 const 绑定）通过 `break` 隐式移动，外部变量复制；引用不能逃逸局部生命周期。禁止嵌套 return 和 break level / break all，仍借用移动来源的延迟清理也会报错。此处的专门规则区别于上面的普通带值退出；详见[声明与返回表达式](28-declaration-sugars.md)。

## 具名函数声明

语句位置可声明 `int64 add(int64 x) { return x + 1; }`，或先声明 `int64 callback(int64 x);` 再赋函数值。返回类型提供检查锚点，默认捕获 const&；块前可显式写 mut、copy 或 move。捕获状态移入声明绑定，借用不能逃逸来源。该语法不进入表达式产生式，见[具名函数规则](28-declaration-sugars.md)。

## #derive 字段派生

在 struct 或 enum 前写 `#derive(Equal, Hash, Clone)`，通过语义字段元数据生成受限接口方法，再由通常的语义检查与四元式降级处理。嵌套字段必须实现对应接口，禁止派生 Drop 或 Closeable。泛型声明在实例化后生成方法，见[派生规则](28-declaration-sugars.md)。
