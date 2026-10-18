# 02 语言速览

用 14 段可运行的小代码，走一遍 Magel 长什么样。

- 每段都真的编译运行过，输出写在代码下面；想自己跑，把代码存成 `x.mgl` 然后 `magel run x.mgl`。
- 本篇只展示**表面语法**。为什么值语义要这样设计、引用什么时候非法、资源什么时候释放，都在[核心概念](03-core-concepts.md)；完整的语法条目在[语言参考](04-language-reference.md)。

如果你写过 C、Java、Go 或 Rust，大部分片段会一眼看懂——这是刻意的。Magel 借用 C 系的表达式与块语法，把学习成本压在你已经会的部分上。

---

## 1. 类型与变量

```magel
int64 main() {
    int8 small = 127;
    int64 count = 0;
    float64 ratio = 3.5;
    bool ready = true;
    char initial = 'M';
    String name = "Magel";
    println("{} {} {} {} {} {}", small, count, ratio, ready, initial, name);
    return 0;
}
```

```text
127 0 3.5 1 M Magel
```

整数显式标宽：`int8` / `int16` / `int32` / `int64`，无符号是 `uint8` … `uint64`；浮点是 `float32` / `float64`。隐式转换只放行**变宽**（`int8 -> int64`、`uint8 -> int16`、整型 → 浮点、`float32 -> float64`）；缩窄和换符号要显式强转 `(int32)x`。

浮点字面量无后缀时为 `float64`；从 v0.18.1 起，小写 `f` 后缀固定为 `float32`，
例如 `1f`、`1.25f`、`1e2f`。`auto value = 1.25f;` 推导为 `float32`，
可直接传给 `float32` 参数。带后缀的字面量先按 32 位浮点舍入，之后即使扩宽为
`float64` 也不会恢复精度，例如 `float64 rounded = 16777217f;` 的值为 `16777216`。
大写 `F` 不属于此后缀语法；十六进制整数中的 `f` 仍是十六进制数字。


整数的小写 `u` 后缀表示无符号字面量，例如 `1u`、`0xffu`、`b_101u`。
无上下文时默认 `uint64`，支持完整范围 `0u` 到 `18446744073709551615u`；
在无符号整型上下文中按目标位宽检查，例如 `uint8 value = 255u;` 合法，`256u` 越界。
换成有符号类型需要显式转换。`-1u`、`-(1u)`、`-+1u` 在编译期报错；
需要有符号负数可写 `-1` 或 `-(int64)1u`。这条限制针对字面量的一元负号，
无符号二元运算仍按位宽回绕，例如 `0u - 1u` 得到 `uint64` 最大值。

`println` 的 `{}` 是位置占位符，不是 `printf` 的 `%d`——类型由实参推导，写错了编译期就报。

`String` 是 `std::lang::String` 的简写，来自自动加载的核心模块 `lang`。没有命名冲突时前缀可省。

---

## 2. 控制流

```magel
int64 classify(int64 value) {
    if (value < 0) { return -1; }
    elif (value == 0) { return 0; }
    else { return 1; }
}
int64 main() {
    int64 sum = 0;
    for (int64 i = 1; i <= 5; i = i + 1) { sum = sum + i; }
    println("sum={} classify(7)={}", sum, classify(7));
    return 0;
}
```

```text
sum=15 classify(7)=1
```

`if / elif / else`、`while`、`for(init; cond; step)` 都在。另外两种循环形态：

- `loop { … yield value; }` —— 带值跳出，`yield` 后面可以跟表达式；
- `for (T item in collection) { … }` —— 遍历容器。

`match` 见下一节。

---

## 3. 枚举与模式匹配

```magel
enum Message {
    Quit,
    Move(int32 x, int32 y),
    Text(String text)
};
void show(Message message) {
    match (message) {
        Message::Quit => { println("quit"); }
        Message::Move(x, y) => { println("move {} {}", x, y); }
        Message::Text(text) => { println("text {}", text); }
    }
}
int64 main() {
    show(Message::Quit);
    show(Message::Move(3, 7));
    show(Message::Text("hello"));
    return 0;
}
```

```text
quit
move 3 7
text hello
```

枚举变体可以带字段（代数数据类型）。`match` 支持字面量、区间、卫语句（`when`），并且检查穷尽性——漏掉一个变体是编译错误，不是运行期兜底。

---

## 4. 值语义与引用（浅尝）

```magel
int64 main() {
    String original = "hello";
    String copy = original;
    copy[0] = 'H';
    println("{} {}", original, copy);
    int64 value = 7;
    int64 mut& ref = &value;
    ref = 9;
    println("{}", value);
    return 0;
}
```

```text
hello Hello
9
```

`copy = original` 复制的是值，改 `copy` 不影响 `original`——没有别名共享，也没有"谁拥有它"的问题。

引用写作 `T mut&`（可变）或 `T const&`（只读），`T &` 等价于 `T const&`。引用**非空**：它必须指向一个确实存在的东西，没有 `null`，也就没有空指针解引用。

这里只看语法。借用来源、移动或释放后的访问会在什么情况下被拒绝，见[核心概念](03-core-concepts.md)。

---

## 5. 可能没有的值：Option 与 Result

```magel
Option<int64> find_even(int64 value) {
    if (value % 2 == 0) { return Option<int64>::Value(value); }
    return Option<int64>::None;
}
int64 main() {
    match (find_even(3)) {
        Option<int64>::Value(v) => { println("value {}", v); }
        Option<int64>::None => { println("none"); }
    }
    return 0;
}
```

```text
none
```

> 有值变体叫 **`Value`**，不是 Rust 的 `Some`。这是最容易踩的一个名字差异。

失败可携带信息的版本是 `Result<T, E>`，配合 `?` 提前返回：

```magel
Result<int64, String> parse_value(bool valid) {
    if (valid) { return Result<int64, String>::Ok(20); }
    return Result<int64, String>::Err("bad input");
}
Result<int64, String> doubled(bool valid) {
    int64 value = parse_value(valid)?;
    return Result<int64, String>::Ok(value * 2);
}
int64 main() {
    match (doubled(true)) {
        Result<int64, String>::Ok(v) => { println("ok={}", v); }
        Result<int64, String>::Err(e) => { println("err={}", e); }
    }
    return 0;
}
```

```text
ok=40
```

`expr?` 在 `Err` 时立刻把错误返回给调用者，在 `Ok` 时取出里面的值。没有异常，错误是返回值的一部分。

---

## 6. 结构体、接口与泛型

```magel
interface Shape { int64 area(); };
struct Circle : Shape {
    int64 r;
    int64 area() { return 3 * r * r; }
};
struct Square : Shape {
    int64 side;
    int64 area() { return side * side; }
};
int64 total<T: Shape>(T shape) { return shape.area(); }
int64 main() {
    Circle c;
    c.r = 2;
    Square s;
    s.side = 3;
    println("{} {}", total<Circle>(c), total<Square>(s));
    return 0;
}
```

```text
12 9
```

`T: Shape` 是接口约束。泛型在编译期单态化，调用点静态确定，没有虚表开销。

需要**运行期**多态时，把对象装进 `std::lang::Any` 再按接口取出引用（`Shape mut& view = boxed.getReference();`），具体类型在运行时决定。完整例子见 [`example/samples/interface.mgl`](../example/samples/interface.mgl)。

结构体还能写构造器（`T v = new T(...)` 调用）、`Drop` 析构逻辑、运算符重载——见[语言参考](04-language-reference.md)。

---

## 7. 集合与迭代器

```magel
struct Player {
    String name;
    int64 score;
};
int64 main() {
    List<Player> players;
    Player a;
    a.name = "amy";
    a.score = 30;
    Player b;
    b.name = "bob";
    b.score = 55;
    players.push(a);
    players.push(b);
    int64 best = 0;
    for (Player p in players) {
        if (p.score > best) { best = p.score; }
    }
    println("best={} count={}", best, players.len());
    return 0;
}
```

```text
best=55 count=2
```

`List<T>`、`Map<K, V>`、`Set<T>` 是内建容器，`push` / `len` / `[]` / `has` / `remove` 等为常用操作。`for-in` 遍历的是值。

链式迭代需要 `import std::iter;`：

```magel
import std::iter;
int64 square(int64 value) { return value * value; }
bool even(int64 value) { return value % 2 == 0; }
int64 main() {
    auto picked = std::iter::range(1, 6).map(square).filter(even).collect();
    println("{} {}", picked.len(), picked[0]);
    return 0;
}
```

```text
2 4
```

`range(1, 6)` 是左闭右开；`map` / `filter` 惰性执行，`collect()` 才真正求值。

---

## 8. 数组与元组

```magel
int64 main() {
    int64 fixed[3];
    fixed[0] = 1;
    fixed[1] = 2;
    fixed[2] = 3;
    int64 total = 0;
    for (int64 i = 0; i < 3; i = i + 1) { total = total + fixed[i]; }
    int64 runtime[] = new[4];
    runtime[0] = 10;
    println("fixed={} runtime0={}", total, runtime[0]);
    return 0;
}
```

```text
fixed=6 runtime0=10
```

两种数组：长度写进类型的 `T a[N]`，和长度在运行时决定的 `T a[] = new[n]`。越界访问会被检查到并给出定位，不是未定义行为。

元组用圆括号，成员按 `.0` / `.1` 读：

```magel
(int64, String) describe(int64 id) {
    return (id, "player");
}
int64 main() {
    auto info = describe(7);
    println("{} {}", info.0, info.1);
    return 0;
}
```

```text
7 player
```

---

## 9. 函数类型与 fn 闭包

```magel
int64 main() {
    int64 fn(int64) square = int64 fn(int64 x) { x * x };
    int64 base = 5;
    auto offset = fn(int64 x) { x + base };
    println("{} {}", square(4), offset(4));
    return 0;
}
```

```text
16 9
```

`R fn(A, B)` 是函数类型，可以写在变量、形参、字段上。Lambda 用 `fn(参数) { 体 }`，返回类型可省（由体推导）。外部局部变量默认按 `const&` 捕获，`fn const(...)` 与默认写法等价：能看到来源的新值，但不能修改来源。`fn mut(...)` 按 `mut&` 捕获并允许写回。`fn copy(...)` 捕获可修改的独立副本；`fn move(...)` 把来源的拥有值移入闭包，原绑定禁止读写，须重新声明新的同名变量后使用。复制闭包会复制内部拥有状态，引用捕获仍指向同一来源。按引用捕获不得超出来源寿命；拥有捕获仅在其中不包含短寿命引用时可以返回。捕获列表写作 `fn copy[a, b] mut[c] const[d] move[e](int64 x) { ... }`，未列出的外部局部变量默认按 `const&` 捕获；列表后可追加一个默认方式，如 `fn copy[a, b] mut(int64 x)` 表示其余变量按 `mut&` 捕获。默认方式只能声明一次，`copy[a, b] mut const(...)` 会报错。

---

## 10. 字符串

```magel
import std::text;
int64 main() {
    String name = "Magel";
    String joined = name + " " + format("{}", 2026);
    println("{}", joined);
    println("first={} upper={}", name[0], std::text::Text::toUpper(name));
    return 0;
}
```

```text
Magel 2026
first=M upper=MAGEL
```

`+` 拼接，`[]` 取字符，`format` 格式化。切分、查找、大小写、去空白等工具在 `std::text::Text` 里（`find` / `split` / `join` / `replace` / `trim` / `padStart` …），需要 `import std::text;`。

---

## 11. 预处理与宏

```magel
%define DETAIL 1
$macro SQUARE(x)
((x) * (x))
$end_macro
int64 main() {
%if DETAIL
    println("detail square5={}", $SQUARE(5));
%else
    println("brief");
%endif
    return 0;
}
```

```text
detail square5=25
```

以 `%` 开头的指令在解析之前处理：`%include` 展开文件、`%if` / `%elif` / `%else` / `%endif` 条件编译、`%define` 定义符号（等价命令行 `-D NAME`）。

以 `$` 开头的是宏：`$macro NAME(x) … $end_macro` 定义，调用写 `$NAME(…)`；不带形参的对象宏写 `$macro NAME()` / 调用 `$NAME`。宏体里还能用 `$stringify(...)`、`$concat(a, b)`、`$line`。

> `%define` 的符号只用于条件编译，不能当表达式里的常量用。

---

## 12. 错误处理的一瞥

编译期抓到的错误会给出行列位置，并在源码下面画出波浪号：

```text
语法错误：hello.mgl 第 2 行第 8 列：期望 标识符 / 常量(const) / 可变(mut) / 新建(new) 等 17 个之一，实际是 实现(impl)
   |
 2 | impl Foo { }
   |        ~~~~
```

内部记号名会翻译成人话：`分号(;)`、`左圆括号(()`、`右花括号(})`。报错语言跟随环境（`zh` / `en` / `ja`），可用 `MAGEL_LANG` 覆盖，详见[工具链使用](09-toolchain.md)。

---

## 13. 还没出现的部分

速览到此为止。下面这些是 Magel 的另一半，各有专门文档：

| 特性 | 一句话 | 去哪看 |
| --- | --- | --- |
| 值语义与所有权 | 普通赋值复制，`move` 显式转移所有权 | [核心概念](03-core-concepts.md) |
| 非空引用与逃逸检查 | 引用不能活得比它指的东西久 | [核心概念](03-core-concepts.md) |
| 无 GC 的资源管理 | `Drop` / `Closeable`，离开作用域确定性释放 | [核心概念](03-core-concepts.md) |
| `abi` 块与 C ABI | 声明外部函数 / 导出 Magel 函数给 C | [宿主集成与 C ABI](08-host-integration.md) |
| 注解与反射 | 编译期元数据，运行期可查询、可按名调用 | [语言参考](04-language-reference.md) |
| 协程与并发 | `std::coroutine`、`std::thread` | [标准库概览](07-stdlib-overview.md) |
| 字节码交付 | `.svmb`、符号压缩 | [交付安全](05-delivery-security.md) |
| 嵌入式与 AOT | 本地代码生成、内存池、看门狗 | [跨平台部署](06-deployment.md) |

---

## 14. 下一步

| 你想 | 去哪 |
| --- | --- |
| 弄明白值语义和引用 | [核心概念](03-core-concepts.md) |
| 查某个语法怎么写 | [语言参考](04-language-reference.md) |
| 看更多可运行样例 | [`example/samples/`](../example/samples/) |
| 建一个正式工程 | `magel new my-app`，见[工具链使用](09-toolchain.md) |

函数类型和匿名函数的返回类型统一前置：`int64 fn(int64, int64) callback`、`int64 fn(int64 x) { x + 1 }`。旧 `fn(int64, int64) -> int64` 与 `fn(int64 x) -> int64 { x + 1 }` 已移除并报错；推导返回类型使用 `fn(int64 x) { x + 1 }`，不写箭头。升级后重新生成泛型宿主胶水并重建旧缓存。
