# Context-typed literals, operator inference and macros

[文档目录](README.md) · [Context-typed literals, operator inference and macros](../24-literals-operators-macros.md)

This guide covers integer literals now take their type from context, operator overloading infers more on its own (commutativity, comparison duality, right-hand switches, multiple result types), and the macro DSL has exactly one form. The first two let you write fewer casts and fewer right-hand interfaces; the third leaves exactly one way to write a macro and halves the diagnostic surface.


## Integer literals take their type from context

Literals are no longer always `int64`. The target type of an assignment, initialisation, argument, or `return` flows into the expression, and binary arithmetic and bitwise operators take their type from the other operand. Comparisons and logical operators are excluded; with no context a literal is still `int64`.

```chali
int32 twice(int32 value) { return value * 2; }
void main() {
    int32 a = 1;
    int32 b = a + 1;                 /* 1 is int32 here; no (int32)1 needed */
    int32 sum = 1 + 2;               /* both sides literal: typed by the int32 slot */
    println("{} {} {}", a, b, sum);

}
```

```text
1 2 3
```

A decimal literal that does not fit the target type falls back to the default type and is rejected by the existing "narrowing requires a cast" diagnostic — **it is never silently truncated**:

```chali
void main() {
    int8 x = 200;
    println("{}", x);

}
```

```text
chali: 整数字面量 200 超出 int8 的取值范围（-128..127）
```

Hexadecimal and binary literals are still read as bit patterns, so `int8 x = 0xFF;` is unaffected.

## Operator overloading: commutativity and comparison duality

`+` `*` `==` `!=` `&` `|` `^` are commutative: a type that declares only the left interface also works on the right-hand side, because the compiler swaps the two operands. `-` `/` `%`, ordering comparisons and shifts are not commutative; the reversed form still needs an explicit right-hand interface.

Declaring only `Less` is enough for `5 > a` (dual), `a >= 5` (negated) and `5 <= a` (dual + negated). `a > 5` is not inferred — that would need `int64 < T`, so the type must also declare `Greater`.

```chali
struct Vec : Add<int64, int64>, Less<bool, int64> {
    int64 v;
    int64 leftAdd(int64 other) { return self.v + other; }
    bool leftLess(int64 other) { return self.v < other; }
};
void main() {
    Vec a;
    a.v = 10;
    println("{} {}", a + 5, 5 + a);     /* commutativity */
    println("{} {}", a < 5, 5 > a);     /* dual */
    println("{} {}", a >= 5, 5 <= a);   /* negated / dual + negated */

}
```

```text
15 15
0 0
1 1
```

`&&` and `||` are built-in short-circuit operators and **cannot be overloaded**. The diagnostic now says so directly instead of falling through to the generic "cannot find `Add` / `RightAdd`" message: short-circuiting must decide the left operand first, which does not fit the operator-overloading model of evaluating both operands and then calling a method.

## Right-hand switches: `RightXxx` / `#NoRightXxx`

Both commutativity and duality borrow the right operand as the receiver, so a type can state its intent:

- Implement `RightAdd` and the right-hand side is yours: `5 + m` uses `rightAdd` only; if its parameter does not fit, that is an error and it no longer falls back to `Add`.
- Write the compiler directive `#NoRightAdd` before the `struct` (`#NoRight` + the left interface name, no type argument) and the right-hand direction is forbidden outright: `5 + f` fails, while `f + 5` is unaffected.
- `#NoRightXxx` wins over `RightXxx`, and an explicit right interface wins over commutativity inference.

```chali
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
void main() {
    Money m;
    m.cents = 120;
    println("{} {}", m + 5, 5 + m);
    Forbid f;
    f.v = 7;
    println("{}", f + 5);

}
```

```text
125 -115
12
```

Writing `5 + f` instead of `f + 5`:

```text
chali: 右向被编译器指令关闭：Forbid 声明了 #NoRightAdd，int64 + Forbid 不再推导；要去掉该指令，或改为实现 RightAdd<R,T>
```

## One operand pair, several result types

`Add<int64,int64>` and `Add<Vec,int64>` can now coexist: two `leftAdd(int64)` overloads that differ only in return type, which ordinary method overloading cannot express. The **target type** of the expression picks one:

```chali
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
void main() {
    A a;
    a.v = 5;
    int64 x = a + 3;                 /* the int64 one: 5 + 3 */
    Vec y = a + 3;                   /* the Vec one: 5 * 3 */
    println("{} {}", x, y.v);

}
```

```text
8 15
```

With no target type (`let z = a + 3;`) the compiler reports the candidates:

```text
chali: 运算符 + 有多条声明匹配同一对操作数 A / int64，返回类型分别是 int64、Vec（先强转再算；若它们只是返回类型不同，给表达式一个目标类型就能定）
```

**Swapping operands does not change evaluation order.** When `bump() + makeA()` becomes `makeA().leftAdd(bump())`, both operands are still evaluated left to right as written (the left one is parked in a temporary first), so side effects happen in the same order as they would without the swap.

## Compile-time hygienic AST macros

The generated parser builds macro definitions, calls and syntax fragments. Semantic analysis expands hygienic ASTs while resolving type dependencies, before quadruple generation. Expansion does not concatenate source and reparse it. Arguments retain their original spelling, location and caller bindings; ordinary declarations introduced by a template are hygienic, and helper names resolve in the definition module.

```chali
$macro SQUARE(x)
x * x
$end_macro
$macro ANSWER
42
$end_macro
$macro EMPTY()
42
$end_macro
void main() {
    println("{} {} {}", $SQUARE(1 + 2), $ANSWER, $EMPTY());

}
```

`1 + 2` remains a complete operand, so the square is 9. Expansion adds no automatic parentheses, argument cache, closure or scope. `$SQUARE(next())` still evaluates `next()` twice. Write a semicolon after a statement macro invocation. Templates splice statements into the surrounding list; a block expression requires an explicit `{ ... yield value; }` in the template.

## Groups, variadic fragments and bindings

`$macro A` requires `$A`; `$macro A()` requires `$A()`. Square, round and curly groups must match in order and shape; repeated groups are allowed. Each group can end with `...rest` or `..rest`, including an empty capture. An applicable fixed signature takes priority over a variadic signature.

```chali
$macro ACCUMULATE(out)[...values]
$for value in values
int64 temporary = value;
out += temporary;
$end_for
$end_macro
void main() {
    int64 temporary = 100;
    int64 total = 0;
    $ACCUMULATE(total)[1, 2, 3];
    println("{} {}", total, temporary);

}
```

This prints `6 100`. Each iteration has distinct declaration identities without an added runtime block. Variadic fragments can be forwarded to another macro or spread in a function argument list. `$for` traverses variadic parameters only. A parameter in a declaration-name position requires a single identifier; that declaration is visible to subsequent caller code. Member selectors and variable references have separate syntax roles.

## Macro type parameters

Angle brackets are reserved for type parameters:

```chali
$macro CONVERT<T:value>(value)
cast<T>(value)
$end_macro
$macro RESULT<T>(value)
cast<T>(value)
$end_macro
void main() {
    int32 source = 42;
    let same = $CONVERT(source);
    int32 expected = $RESULT(42);
    int64 explicitValue = $RESULT<int64>(42);

}
```

`T:value` uses only that argument's static type. A parameter without a source uses only the expected result type. Explicit types take priority. Inference does not evaluate arguments, insert conversions or switch sources. `let x = $RESULT(42)` fails because no expected type is available. Macro value parameters, specialization, partial specialization and arbitrary compile-time function execution are outside this interface.

## Complete syntax matching

```chali
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

The five categories are exactly `expr`, `ident`, `type`, `block` and `stmt`. The same generated parser must consume the whole fragment. The first matching branch wins; an identifier may also be an expression. Syntax recognition does not imply valid names or types. Only selected output enters final name, type, capture, borrow and resource checks, while all template syntax must be valid. No match without a fallback emits nothing. A curly argument group is a delimiter rather than an implicit block: pass a complete block as `$A{{ statements; }}`.

## Regular expressions, spelling and origins

```chali
$macro DECIMAL(value)
$regex value is "[0-9]+"
value
$else
0
$end_regex
$end_macro
```

`$regex` uses the full C++ `std::regex` ECMAScript syntax and matches the entire argument spelling after trimming outer whitespace. Internal comments, quotes and newlines remain significant. The language string is decoded once; compiled patterns and invalid patterns are cached. Invalid patterns are compile errors. Capture groups are available inside the regex but do not export variables.

`$stringify(value)` returns original argument text without evaluating or expanding it. `$concat(left, right)` produces a single identifier only. A template-generated identifier keeps its definition context and cannot capture a caller's local binding. Direct source concatenation uses that source context. `$line`, `$file` and `$module` are available directly and refer to the invocation inside a template; forwarding retains original argument origins.

`$replace ADD(a, b) a + b` is a single-line definition using the same AST mechanism. Expansion has recursion and size budgets. Diagnostics retain invocation, definition and argument chains. The editor previews hygienic ASTs one graph per macro call; each graph is a self-contained page that can be embedded in a web page. Type hints come from final static types without exposing internal binding identifiers.

See [AST tests](../../tests/chali_ast_macros.py), [groups and origins](../../tests/chali_macro_groups.py), [static types](../../tests/chali_macro_generics.py) and [syntax and regex branches](../../tests/chali_macro_branches.py).
