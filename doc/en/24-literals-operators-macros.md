# Context-typed literals, operator inference and macros

[文档目录](README.md) · [Context-typed literals, operator inference and macros](../24-literals-operators-macros.md)

This guide covers integer literals now take their type from context, operator overloading infers more on its own (commutativity, comparison duality, right-hand switches, multiple result types), and the macro DSL is tightened by removing `$match` and the legacy `##` / `#param` shorthands. The first two let you write fewer casts and fewer right-hand interfaces; the third leaves exactly one way to write a macro and halves the diagnostic surface.


## Integer literals take their type from context

Literals are no longer always `int64`. The target type of an assignment, initialisation, argument, or `return` flows into the expression, and binary arithmetic and bitwise operators take their type from the other operand. Comparisons and logical operators are excluded; with no context a literal is still `int64`.

```magel
int32 twice(int32 value) { return value * 2; }
int64 main() {
    int32 a = 1;
    int32 b = a + 1;                 /* 1 is int32 here; no (int32)1 needed */
    int32 sum = 1 + 2;               /* both sides literal: typed by the int32 slot */
    println("{} {} {}", a, b, sum);
    return 0;
}
```

```text
1 2 3
```

A decimal literal that does not fit the target type falls back to the default type and is rejected by the existing "narrowing requires a cast" diagnostic — **it is never silently truncated**:

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

Hexadecimal and binary literals are still read as bit patterns, so `int8 x = 0xFF;` is unaffected.

## Operator overloading: commutativity and comparison duality

`+` `*` `==` `!=` `&` `|` `^` are commutative: a type that declares only the left interface also works on the right-hand side, because the compiler swaps the two operands. `-` `/` `%`, ordering comparisons and shifts are not commutative; the reversed form still needs an explicit right-hand interface.

Declaring only `Less` is enough for `5 > a` (dual), `a >= 5` (negated) and `5 <= a` (dual + negated). `a > 5` is not inferred — that would need `int64 < T`, so the type must also declare `Greater`.

```magel
struct Vec : Add<int64, int64>, Less<bool, int64> {
    int64 v;
    int64 leftAdd(int64 other) { return self.v + other; }
    bool leftLess(int64 other) { return self.v < other; }
};
int64 main() {
    Vec a;
    a.v = 10;
    println("{} {}", a + 5, 5 + a);     /* commutativity */
    println("{} {}", a < 5, 5 > a);     /* dual */
    println("{} {}", a >= 5, 5 <= a);   /* negated / dual + negated */
    return 0;
}
```

```text
15 15
0 0
1 1
```

`&&` and `||` are built-in short-circuit operators and **cannot be overloaded**. The diagnostic now says so directly instead of falling through to the generic "cannot find `Add` / `RightAdd`" message: short-circuiting must decide the left operand first, which does not fit the operator-overloading model of evaluating both operands and then calling a method.

## Right-hand switches: `RightXxx` / `NoRightXxx`

Both commutativity and duality borrow the right operand as the receiver, so a type can state its intent:

- Implement `RightAdd` and the right-hand side is yours: `5 + m` uses `rightAdd` only; if its parameter does not fit, that is an error and it no longer falls back to `Add`.
- Implement the empty marker interface `NoRightAdd` (the name is `No` + the right interface name, with no type argument) and the right-hand direction is forbidden outright: `5 + f` fails, while `f + 5` is unaffected.
- `NoRightXxx` wins over `RightXxx`, and an explicit right interface wins over commutativity inference.

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

Writing `5 + f` instead of `f + 5`:

```text
magel：右向被显式禁止：Forbid 实现了 NoRightAdd，int64 + Forbid 不再推导；要去掉该标记接口，或改为实现 RightAdd<R,T>
```

## One operand pair, several result types

`Add<int64,int64>` and `Add<Vec,int64>` can now coexist: two `leftAdd(int64)` overloads that differ only in return type, which ordinary method overloading cannot express. The **target type** of the expression picks one:

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
    int64 x = a + 3;                 /* the int64 one: 5 + 3 */
    Vec y = a + 3;                   /* the Vec one: 5 * 3 */
    println("{} {}", x, y.v);
    return 0;
}
```

```text
8 15
```

With no target type (`auto z = a + 3;`) the compiler reports the candidates:

```text
magel：运算符 + 有多条声明匹配同一对操作数 A / int64，返回类型分别是 int64、Vec（先强转再算；若它们只是返回类型不同，给表达式一个目标类型就能定）
```

**Swapping operands does not change evaluation order.** When `bump() + makeA()` becomes `makeA().leftAdd(bump())`, both operands are still evaluated left to right as written (the left one is parked in a temporary first), so side effects happen in the same order as they would without the swap.

## Macro text concatenation and stringification

The macro layer carried two sets of redundant spellings. Both are unsupported:

- **`$match` / `$when` / `$end_when` / `$else` / `$end_else` / `$end_match`** are no longer directives. They were a miniature DSL stacked on top of a textual substitution macro — rarely used, but with a very large diagnostic surface. To generate code per branch, use `$for` together with overloads that distinguish shapes by argument count.
- **`##` pasting and `#param` stringification** are no longer directives either. Use `$concat(a, b)` and `$stringify(x)`. The former `##` / `#param` spellings have been removed and are rejected.

Writing either is a syntax error:

```text
Syntax error: …：认不出来的字符
```

The recommended spellings:

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

Names such as `match` and `when` no longer occupy keywords, so they can be used as macro names. Note that `$concat` only pastes identifiers, so a parameter used for pasting **must not be back-quoted** — back-quoting wraps it in parentheses, and the result is no longer an identifier.

## Ordered bracket groups in text macros

Macros expand before the Lexer. Arguments remain raw text; grouping neither builds AST nodes nor checks argument types. The keyword is lowercase `$macro`, with the expansion body on following lines and `$end_macro` as its terminator.

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

The declaration determines the order of `< >`, `[ ]`, `( )`, and `{ }`. Repeated groups are supported: `$macro FOO()()` and `$macro BAR[](x)()` are called as `$FOO()()` and `$BAR[](value)()`. The existing single empty `NAME()` declaration still defines an object macro called as `$NAME`.

Each group accepts fixed parameters and one final `..rest`, for example `<T, ..Types>[..Attrs](x, ..Args){..Body}`. Names must be unique across the declaration. Each variadic capture may be empty and stays inside its own group. `$for` can traverse each group's tail independently, including nested loops across groups. Existing single-parenthesis `...rest`, argument protection, and `expr` macros remain supported.

Only commas at the current nesting level split arguments. Nested brackets, strings, and comments shield their commas; joining a variadic capture preserves the original comma separators and surrounding text. `List<List<int64>>` closes character by character without operator tokenization. Unpaired `<` and `>` inside other bracket groups remain ordinary text, allowing comparisons and shifts. Paired angle brackets participate in grouping; parentheses can disambiguate comparison expressions.

Brace arguments may span multiple lines and contain nested groups. Brackets in strings and comments do not close groups. Internal markers retain file and line origins, plus column offsets for captured fragments. Before lexing, markers are removed and line breaks and spaces restore diagnostic positions, including code after a call on the same line and subsequent lines. Bracket kind, order, structure, or binding mismatches report `宏Token分组不匹配` rather than type errors. See [`magel_macro_groups_test.cpp`](../../tests/magel_macro_groups_test.cpp).

Inside a macro body, `$n` names an expansion-local temporary. The decimal integer n has no fixed width and may include a sign; `$0010`, `$+10` and `$10` identify the same slot. Normalization uses text rather than host integer conversion. Separate macro calls remain isolated, strings and comments are preserved, and placeholders remain invalid outside macro bodies.

## Migration checklist

| You used to write | Write this instead |
| --- | --- |
| `int32 x = a + (int32)1;` | `int32 x = a + 1;` (literals are context-typed) |
| `5 + a` failed with "cannot find `RightAdd`" | Usually inferred now; implement `RightAdd<R,T>` to customise it, `NoRightAdd` to forbid it |
| the whole `$match` / `$when` / `$end_match` set | `$for` plus overloads that key on argument count |
| `a ## b` | `$concat(a, b)` |
| `#param` | `$stringify(param)` |

Nothing to change for: comparisons and logical operators, which are not context-typed; `-` `/` `%`, ordering comparisons and shifts, which are still not commutative; and `&&` / `||`, which were never overloadable. Borrow checking covers provenance escape and move/release control flow, including MGL1204 and MGL1205 diagnostics.

## Regression coverage

- Ten new regressions: `literal_context_type`, `operator_symmetry`, `operator_right_direction`, `operator_result_choice`, `operator_swap_eval_order`, plus the five error samples `operator_asymmetric_reverse`, `operator_right_forbidden`, `operator_right_explicit`, `operator_result_ambiguous` and `logical_operator_overload`.
- Removed: `macro_match.mgl` and its seven error samples, plus `macro_paste_missing_side` and `macro_stringify_unknown`, which only applied to the removed spellings.
