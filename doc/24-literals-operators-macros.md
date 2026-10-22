# 字面量定型、运算符推导与宏

[返回文档目录](README.md) · [字面量定型、运算符推导与宏](en/24-literals-operators-macros.md)

本文介绍：整数字面量按上下文定型、运算符重载会自己推导（交换律 / 比较对偶 / 右向开关 / 多个返回类型），以及把宏 DSL 收口——删掉 `$match` 那一套和 `##` / `#形参` 两个旧写法。前两项让你少写强转和右接口，第三项让宏只剩一套写法、报错面减半。


## 整数字面量按上下文定型

字面量不再一律按 `int64` 走：赋值 / 初始化 / 实参 / `return` 的目标类型会传进表达式；二元算术与位运算按另一侧操作数的类型定型。比较与逻辑不参与这层定型，没有上下文时仍是默认 `int64`。

```magel
int32 twice(int32 value) { return value * 2; }
int64 main() {
    int32 a = 1;
    int32 b = a + 1;                 /* 1 按 int32 定，不必写 (int32)1 */
    int32 sum = 1 + 2;               /* 两边都是字面量：按 int32 槽位定型 */
    println("{} {} {}", a, b, sum);
    return 0;
}
```

```text
1 2 3
```

十进制字面量超出目标类型范围就退回默认类型，由既有的「缩窄要强转」报错，**不会静默截断**：

```magel
int64 main() {
    int8 x = 200;
    println("{}", x);
    return 0;
}
```

```text
magel: 整数字面量 200 超出 int8 的取值范围（-128..127）
```

十六进制 / 二进制照旧按位模式看待，`int8 x = 0xFF;` 不受影响。

## 运算符重载：交换律与比较对偶

`+` `*` `==` `!=` `&` `|` `^` 满足交换律：类型只声明了左接口时，右操作数那侧也能用，编译器自动交换两个操作数。`-` `/` `%`、大小比较和移位不交换，反向仍须显式声明右接口。

只声明 `Less` 时，`5 > a`（对偶）、`a >= 5`（取反）、`5 <= a`（对偶 + 取反）都推得出来；`a > 5` 推不出来——那要 `int64 < T`，得给类型补 `Greater`。

```magel
struct Vec : Add<int64, int64>, Less<bool, int64> {
    int64 v;
    int64 leftAdd(int64 other) { return self.v + other; }
    bool leftLess(int64 other) { return self.v < other; }
};
int64 main() {
    Vec a;
    a.v = 10;
    println("{} {}", a + 5, 5 + a);     /* 交换律 */
    println("{} {}", a < 5, 5 > a);     /* 对偶 */
    println("{} {}", a >= 5, 5 <= a);   /* 取反 / 对偶 + 取反 */
    return 0;
}
```

```text
15 15
0 0
1 1
```

`&&` / `||` 是内建短路运算符、**不参与重载**。报错不再落到「找不到 `Add` / `RightAdd`」的通用文案，而是直接说明它们不可重载——短路要先决定左边，与运算符重载「先求两个操作数再调方法」的模型对不上。

## 右向开关：`RightXxx` / `#NoRightXxx`

交换律与比较对偶都要把右操作数借来当接收者，所以类型可以表态：

- 实现 `RightAdd` → 右向归它自己，`5 + m` 只认 `rightAdd`，形参接不住就报错、不再回落到 `Add`；
- 在 `struct` 前写编译器指令 `#NoRightAdd`（`#NoRight` + 左接口名，无类型实参）→ 禁止右向，`5 + f` 直接报错，左向 `f + 5` 不受影响；
- `#NoRightXxx` 优先于 `RightXxx`，显式声明的右接口优先于交换律推断。

```magel
struct Money : Add<int64, int64>, RightAdd<int64, int64> {
    int64 cents;
    int64 leftAdd(int64 other) { return self.cents + other; }    /* m + 5 */
    int64 rightAdd(int64 other) { return other - self.cents; }   /* 5 + m */
};
#NoRightAdd
struct Forbid : Add<int64, int64> {
    int64 v;
    int64 leftAdd(int64 other) { return self.v + other; }
};
int64 main() {
    Money m;
    m.cents = 120;
    println("{} {}", m + 5, 5 + m);
    Forbid f;
    f.v = 7;
    println("{}", f + 5);
    return 0;
}
```

```text
125 -115
12
```

把 `f + 5` 写成 `5 + f`：

```text
magel: 右向被编译器指令关闭：Forbid 声明了 #NoRightAdd，int64 + Forbid 不再推导；要去掉该指令，或改为实现 RightAdd<R,T>
```

## 同一对操作数、多个返回类型

`Add<int64,int64>` 与 `Add<Vec,int64>` 可以共存——两个 `leftAdd(int64)` 只在返回类型上不同，这是普通方法重载做不到的事。挑哪条由表达式的**目标类型**定：

```magel
struct Vec { int64 v; };
struct A : Add<int64,int64>, Add<Vec,int64> {
    int64 v;
    int64 leftAdd(int64 other) { return self.v + other; }
    Vec leftAdd(int64 other) {
        Vec r;
        r.v = self.v * other;
        return r;
    }
};
int64 main() {
    A a;
    a.v = 5;
    int64 x = a + 3;                 /* 返回 int64 的那条：5 + 3 */
    Vec y = a + 3;                   /* 返回 Vec 的那条：5 * 3 */
    println("{} {}", x, y.v);
    return 0;
}
```

```text
8 15
```

没有目标类型（`let z = a + 3;`）会报错并列出候选返回类型：

```text
magel: 运算符 + 有多条声明匹配同一对操作数 A / int64，返回类型分别是 int64、Vec（先强转再算；若它们只是返回类型不同，给表达式一个目标类型就能定）
```

**换边不改变求值顺序**：`bump() + makeA()` 变成 `makeA().leftAdd(bump())` 时，两个操作数仍按源码从左到右求值（左操作数先钳进临时量），副作用顺序与不换边时一致。

## 编译期卫生 AST 宏

`$macro` 定义由生成解析器建立语法树，语义分析按依赖展开，类型推导与展开交错进行，四元式生成前完成。宏不把替换结果拼成源码重新解析。参数保留原文、位置、语法候选及调用处绑定；引入的普通局部声明自动卫生，辅助名称在定义模块查找。

```magel
$macro SQUARE(x)
x * x
$end_macro
$macro ANSWER
42
$end_macro
$macro EMPTY()
42
$end_macro
int64 main() {
    int64 temporary = 10;
    println("{} {} {}", $SQUARE(1 + 2), $ANSWER, $EMPTY());
    return 0;
}
```

实参 `1 + 2` 是完整表达式，替换进乘法的两个操作数，结果为 9。无需形参括号保护，也不自动给输出加括号、缓存参数或插入作用域；`$SQUARE(next())` 仍调用两次 `next()`。编号临时变量、宏头 `expr` 标注及形参保护标注均已删除。语句宏的调用写完整分号；宏体的语句直接插入调用所在列表。需要块表达式时，在模板中显式书写 `{ ... yield value; }`。

## 参数组、变长片段及卫生绑定

裸定义 `$macro A` 只能用 `$A`；空组定义 `$macro A()` 只能用 `$A()`。`[]`、`()`、`{}` 按声明顺序严格匹配，允许重复组。每组支持固定形参和一个末尾变长形参 `...rest` 或 `..rest`，空捕获合法。固定形态优先于适用的变长形态。

```magel
$macro ACCUMULATE(out)[...values]
$for value in values
int64 temporary = value;
out += temporary;
$end_for
$end_macro
int64 main() {
    int64 temporary = 100;
    int64 total = 0;
    $ACCUMULATE(total)[1, 2, 3];
    println("{} {}", total, temporary);
    return 0;
}
```

结果是 `6 100`。每轮展开中的局部声明互相独立，实参 `out` 保留调用者的绑定。变长片段可转发到另一宏，或在函数参数列表中逐项展开；`$for` 只遍历变长形参。形参出现在声明名位置时必须传入单个标识符，该声明可供后续调用者代码使用。成员名与变量名按各自语法角色处理。

## 宏泛型

尖括号仅表示宏类型参数，不能作为任意文本参数组：

```magel
$macro CONVERT<T:value>(value)
cast<T>(value)
$end_macro
$macro RESULT<T>(value)
cast<T>(value)
$end_macro
int64 main() {
    int32 source = 42;
    let same = $CONVERT(source);
    int32 expected = $RESULT(42);
    int64 explicitValue = $RESULT<int64>(42);
    return 0;
}
```

`T:value` 只从指定实参的静态类型推导；未指定来源的 `T` 只从外层期望结果类型推导。显式类型优先，不执行参数、不自动转换、不改换推导来源。`let x = $RESULT(42)` 缺少期望类型，编译失败。首版不支持宏泛型值参数、特化、偏特化或任意函数的编译期执行。

## 五类完整语法匹配

```magel
$macro CLASSIFY(value)
$match value is ident
1
$else is expr
2
$else is type
3
$else is block
4
$else is stmt
5
$else
0
$end_match
$end_macro
```

只支持 `expr`、`ident`、`type`、`block`、`stmt` 五类，使用同一生成解析器完整消费原始片段。按书写顺序选择首个成立分支；`ident` 也可能是 `expr`，因此顺序有意义。语法成立不代表名称存在或类型合法；只有最终展开的节点进入名称、类型、捕获、借用和资源检查。无匹配且无兜底不生成内容。未选分支的模板结构仍须合法。大括号参数组不等于块片段：传入一个完整块写 `$A{{ statements; }}`。

## 正则、原文与来源

```magel
$macro DECIMAL(value)
$regex value is "[0-9]+"
value
$else
0
$end_regex
$end_macro
```

`$regex` 使用 C++ `std::regex` 的 ECMAScript 语法对去除首尾空白的完整参数原文匹配，保留内部注释、引号和换行。字符串按语言规则解码一次，合法与非法模式均缓存；非法模式报编译错误。正则捕获组仅用于匹配，不导出捕获变量。

`$stringify(value)` 返回实参原文的字符串，不先执行或展开参数。`$concat(left, right)` 只生成单个标识符，不能生成任意代码；模板中生成的名字保持定义处上下文，不能借拼名捕获调用者局部变量。直接源码中的拼名按该源码上下文查找。`$line`、`$file`、`$module` 可直接使用；模板内取调用处来源，嵌套参数转发保留原始来源。

`$replace ADD(a, b) a + b` 是同一 AST 机制的单行定义。宏递归与规模有预算，失败诊断保留调用、定义和参数来源链。编辑器宏预览按宏调用逐张生成卫生 AST 展开图，每张图都是自包含页面，可直接放进网页；类型提示读取最终静态类型，不显示内部绑定编号。

### 跨模块调用与限定名

宏定义登记在**定义它的模块**里，调用按模块可见性解析：

- 未限定写法（`$assert(...)`）：先查调用所在模块，再查 `import` 进来的模块；`std::lang`
  始终可见，所以标准库提供的宏不用 import。可见范围内出现多个同名定义时报错。
- 限定写法（`$std::lang::assert(...)`）：直接点名「模块 + 宏名」，模块路径从最长前缀往下试。
  名字被同名的函数 / 变量挡住，或者想明确指定用哪个模块的宏时用这种写法。限定调用同样
  受可见性约束：`std::lang` 始终可见，其余模块必须先 `import`。
- 两种方式展开出的 AST 完全一致，`$line` / `$file` / `$module` 取的都是调用点。

```magel
module qualified;

int64 main() {
    int64 n = 3;
    $std::lang::assert(n > 0);              /* 限定名：直接点 std::lang 的宏 */
    $std::lang::assert(n < 10, "n 得是个位数");
    return 0;
}
```

限定名找不到对应定义时报「未定义宏」，位置指向调用点。

回归见 [宏 AST](../tests/magel_ast_macros.py)、[分组与来源](../tests/magel_macro_groups.py)、[静态类型](../tests/magel_macro_generics.py) 和 [语法及正则](../tests/magel_macro_branches.py)。
