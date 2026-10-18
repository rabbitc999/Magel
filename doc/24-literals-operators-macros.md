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
magel：整数字面量 200 超出 int8 的取值范围（-128..127）
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

## 右向开关：`RightXxx` / `NoRightXxx`

交换律与比较对偶都要把右操作数借来当接收者，所以类型可以表态：

- 实现 `RightAdd` → 右向归它自己，`5 + m` 只认 `rightAdd`，形参接不住就报错、不再回落到 `Add`；
- 实现空标记接口 `NoRightAdd`（名字为 `No` + 右接口名，无类型实参）→ 明令禁止右向，`5 + f` 直接报错，左向 `f + 5` 不受影响；
- `NoRightXxx` 优先于 `RightXxx`，显式声明的右接口优先于交换律推断。

```magel
struct Money : Add<int64, int64>, RightAdd<int64, int64> {
    int64 cents;
    int64 leftAdd(int64 other) { return self.cents + other; }    /* m + 5 */
    int64 rightAdd(int64 other) { return other - self.cents; }   /* 5 + m */
};
struct Forbid : Add<int64, int64>, NoRightAdd {
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
magel：右向被显式禁止：Forbid 实现了 NoRightAdd，int64 + Forbid 不再推导；要去掉该标记接口，或改为实现 RightAdd<R,T>
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

没有目标类型（`auto z = a + 3;`）会报错并列出候选返回类型：

```text
magel：运算符 + 有多条声明匹配同一对操作数 A / int64，返回类型分别是 int64、Vec（先强转再算；若它们只是返回类型不同，给表达式一个目标类型就能定）
```

**换边不改变求值顺序**：`bump() + makeA()` 变成 `makeA().leftAdd(bump())` 时，两个操作数仍按源码从左到右求值（左操作数先钳进临时量），副作用顺序与不换边时一致。

## 宏文本拼接与字符串化

宏这一层以前叠了两套「多余的写法」，当前均不支持：

- **`$match` / `$when` / `$end_when` / `$else` / `$end_else` / `$end_match`** 不再是指令。它本质是在文本替换宏上又叠了一层迷你 DSL——用的人少、报错面却很大。要按分支生成代码，改用 `$for` 配合「按实参个数重载」分形态。
- **`##` 拼接与 `#形参` 字符串化**也不再是指令。拼名字与取源码文本统一用 `$concat(a, b)` / `$stringify(x)`。`##` / `#形参` 已移除，当前不接受这些写法。

写了就是语法错误：

```text
Syntax error: …：认不出来的字符
```

推荐写法：

```magel
$macro SUM(...rest)
    $for v in rest
        + (v)
    $end_for
$end_macro
int64 main() {
    int64 total = 0 $SUM(1, 2, 3, 4);
    println("{}", total);
    return 0;
}
```

```text
10
```

```magel
$macro SHOW(expr)
println("{} = {}", $stringify(expr), expr);
$end_macro
int64 main() {
    int64 a = 6;
    $SHOW(a * 7)
    return 0;
}
```

```text
a * 7 = 42
```

`match`、`when` 这类名字不再占关键字，可以拿来当宏名。`$concat` 只拼标识符，读取原始实参，不受定义端括号标注影响。

## 有序多括号文本宏

宏在 Lexer 之前展开；参数是原始文本，不构建 AST，也不检查参数类型。`$macro` 保持全小写，宏体仍写在声明后的行中，以 `$end_macro` 结束。

```magel
$macro CODE<..Types>[..Attrs](..Args){..Body}
    println("types: {}", $stringify(Types));
    println("attrs: {}", $stringify(Attrs));
    println("args: {}", $stringify(Args));
    Body
$end_macro

void main() {
    $CODE<int64, String>[readonly](x, y){
        println("multi-line body");
        if (true) {
            println("nested braces");
        }
    }
}
```

定义中的 `< >`、`[ ]`、`( )`、`{ }` 可以任意排序和重复，如 `$macro FOO()()`、`$macro BAR[](x)()`。调用必须提供定义中对应顺序的各组；`$FOO()()` 与 `$BAR[](value)()` 均消耗全部声明组。现有单组 `NAME()` 仍表示对象宏，以 `$NAME` 调用。

每组可使用固定形参和一个末尾 `..rest`，例如 `<T, ..Types>[..Attrs](x, ..Args){..Body}`；不同组独立绑定，形参名称不能重复。每组变长捕获可以为空，不会吞入下一组；`$for` 可分别遍历各组尾参，也可嵌套遍历。旧单圆括号宏的 `...rest`、参数括号保护与 `expr` 写法继续有效。

实参按当前层的逗号切分，嵌套括号、字符串与注释内的逗号不切分；变长参数重新连接时保留原有逗号及其两侧文本。`List<List<int64>>` 中的 `>>` 按两个字符闭合，不经过运算符词法分析。非尖括号组内不配对的 `<` / `>` 保留为普通文本，可写比较与移位；成对的尖括号参与分组，含歧义的比较表达式可用圆括号明确边界。

大括号实参可以跨任意多行并包含其他括号组，字符串和注释中的括号不参与闭合。宏体和捕获实参使用内部来源标记记录文件与行，捕获片段额外记录列偏移；标记在送入 Lexer 前移除，通过分行和空格占位恢复诊断位置，调用后同一行及后续行也恢复原位置。括号种类、顺序、结构或参数绑定不匹配时报“宏Token分组不匹配”，不解释为类型错误。回归见 [`magel_macro_groups_test.cpp`](../tests/magel_macro_groups_test.cpp)。

宏体中的 `$n` 为当前展开专属的临时名；n 可为任意长度十进制整数，允许正负号，`$0010`、`$+10` 与 `$10` 相同。编号以文本归一化，不受宿主整数上限限制。不同宏调用互相隔离，字符串和注释保持原样；普通源码中仍不能直接使用这些占位符。

## 迁移清单

| 你原来写的 | 现在改成 |
| --- | --- |
| `int32 x = a + (int32)1;` | `int32 x = a + 1;`（字面量按上下文定型） |
| `5 + a` 报「找不到 `RightAdd`」 | 通常已能推导；要自定义右向就实现 `RightAdd<R,T>`，要禁止就实现 `NoRightAdd` |
| `$match` / `$when` / `$end_match` 那一套 | 用 `$for` + 按实参个数重载分形态 |
| `a ## b` | `$concat(a, b)` |
| `#形参` | `$stringify(形参)` |

不需要动的地方：比较与逻辑运算不参与字面量定型；`-` `/` `%`、大小比较、移位仍不交换；`&&` / `||` 本来就不能重载。借用检查包括来源逃逸与控制流中的移动、释放检查，诊断包括 MGL1204 与 MGL1205。

## 回归覆盖

- 新增十份回归：`literal_context_type`、`operator_symmetry`、`operator_right_direction`、`operator_result_choice`、`operator_swap_eval_order`，以及 `operator_asymmetric_reverse`、`operator_right_forbidden`、`operator_right_explicit`、`operator_result_ambiguous`、`logical_operator_overload` 五份报错样例。
- 删除 `macro_match.mgl` 与 7 份 `$match` 报错样例，以及只对旧写法有效的 `macro_paste_missing_side`、`macro_stringify_unknown` 两份报错样例。
