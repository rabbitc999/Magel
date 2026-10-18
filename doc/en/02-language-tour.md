# 02 Language tour

Fourteen runnable snippets walking through what Magel looks like.

- Each one is compiled and executed for real; the output is printed below it. To run one yourself,
  save it as `x.mgl` and run `magel run x.mgl`.
- This tour shows **surface syntax only**. Why value semantics are designed this way, when a reference
  is rejected, when resources are released — all in [Core concepts](03-core-concepts.md). The complete
  syntax entries are in [Language reference](04-language-reference.md).

If you have written C, Java, Go or Rust, most of this will look familiar at a glance — deliberately so.
Magel borrows C-family expression and block syntax so the learning cost lands on what you already know.

---

## 1. Types and variables

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

Integers carry their width explicitly: `int8` / `int16` / `int32` / `int64`; unsigned is `uint8` …
`uint64`; floating point is `float32` / `float64`. There is no implicit narrowing — cast explicitly with `(int32)x`.

Floating literals without a suffix have type `float64`. Since v0.18.1, the lowercase
`f` suffix fixes the type to `float32`: `1f`, `1.25f`, and `1e2f` are examples.
`auto value = 1.25f;` infers `float32` and can be passed directly to a `float32`
parameter. The literal is rounded to 32-bit precision before any widening:
`float64 rounded = 16777217f;` therefore stores `16777216`.
Uppercase `F` is not a suffix; `f` in a hexadecimal integer remains a hexadecimal digit.


The lowercase `u` suffix marks an unsigned integer literal: `1u`, `0xffu`, or `b_101u`.
Without context its type is `uint64`, covering `0u` through `18446744073709551615u`.
An unsigned integer context checks the target width: `uint8 value = 255u;` is valid,
while `256u` is out of range. Converting to a signed type requires an explicit cast.
`-1u`, `-(1u)`, and `-+1u` are compile-time errors; use `-1` or `-(int64)1u`
for a signed negative value. This restriction concerns unary minus on the literal;
unsigned binary arithmetic still wraps, so `0u - 1u` yields the maximum `uint64` value.

`{}` in `println` is a positional placeholder, not `printf`'s `%d` — the type is inferred from the
argument, and a mistake is a compile-time error.

`String` is shorthand for `std::lang::String`, from the automatically loaded core module `lang`. Drop the
prefix when there is no name clash.

---

## 2. Control flow

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

`if / elif / else`, `while` and `for(init; cond; step)` are all present. Two more loop shapes:

- `loop { … yield v; }` — loop with a value result; `yield` carries the expression;
- `for (T item in collection) { … }` — iterate a container.

`match` is next.

---

## 3. Enums and pattern matching

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

Enum variants can carry fields (algebraic data types). `match` supports literals, ranges and guards
(`if`), and checks exhaustiveness — missing a variant is a compile error, not a runtime fallback.

---

## 4. Value semantics and references (a first look)

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

`copy = original` copies the value; mutating `copy` does not touch `original` — no aliasing, no
"who owns it" question.

References are written `T mut&` (writable) or `T const&` (read-only); `T &` is equivalent to
`T const&`. References are **non-null**: they must point at something that really exists. There is no
`null`, and therefore no null-pointer dereference.

Syntax only, for now. When the engine rejects a reference (for example, storing it somewhere that
outlives its target), and how move/release control-flow checking works: [Core concepts](03-core-concepts.md).

---

## 5. A value that may be absent: Option and Result

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

> The present variant is called **`Value`**, not Rust's `Some`. This is the easiest name to trip over.

The version whose failure carries information is `Result<T, E>`, with `?` for early return:

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

`expr?` returns the error to the caller immediately on `Err`, and unwraps the value on `Ok`. There are
no exceptions; errors are part of the return value.

---

## 6. Structs, interfaces and generics

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

`T: Shape` is an interface constraint. Generics are monomorphised at compile time; the call site is
resolved statically, with no vtable overhead.

For **runtime** polymorphism, box the object into `std::lang::Any` and take an interface reference out
(`Shape mut& view = boxed.getReference();`); the concrete type is decided at run time. See
[`example/samples/interface.mgl`](../../example/samples/interface.mgl).

Structs can also declare constructors (called as `T v = new T(...)`), `Drop` destructors and operator
overloads — see [Language reference](04-language-reference.md).

---

## 7. Collections and iterators

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

`List<T>`, `Map<K, V>` and `Set<T>` are built in, with `push` / `len` / `[]` / `has` / `remove` and
friends. `for-in` iterates by value.

Chained iteration needs `import std::iter;`:

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

`range(1, 6)` is half-open; `map` / `filter` are lazy and `collect()` forces evaluation.

---

## 8. Arrays and tuples

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

Two kinds of array: `T a[N]` with the length in the type, and `T a[] = new[n]` whose length is
decided at run time. Out-of-bounds access is detected and reported with a location; it is not
undefined behaviour.

Tuples use parentheses and members are read via `.0` / `.1`:

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

## 9. Function types and fn closures

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

`R fn(A, B)` is the function type and can appear on variables, parameters and fields. Lambdas use
`fn(params) { body }`; the return type may be omitted (inferred from the body). External locals are captured by `const&` by default; `fn const(...)` is equivalent. Reads observe later changes in the source, while writes are rejected. `fn mut(...)` captures by `mut&` and writes back to the source. `fn copy(...)` owns mutable independent copies. `fn move(...)` transfers captured values into the closure and forbids reading or writing the old bindings; declare new bindings before reuse. Copying a closure duplicates owned state and preserves references to borrowed sources. Reference captures must not outlive their sources; owned captures may escape only when their contents do not borrow shorter-lived storage. Capture lists use `fn copy[a, b] mut[c] const[d] move[e](int64 x) { ... }`; unlisted external locals use `const&` by default. One trailing default mode may override this: `fn copy[a, b] mut(int64 x)` borrows other locals by `mut&`. Multiple defaults, such as `copy[a, b] mut const(...)`, are rejected.

---

## 10. Strings

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

`+` concatenates, `[]` indexes a character, `format` formats. Splitting, searching, case
conversion and trimming live in `std::text::Text` (`find` / `split` / `join` / `replace` / `trim` /
`padStart` …) and need `import std::text;`.

---

## 11. Preprocessing and macros

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

Directives starting with `%` run before parsing: `%include` expands a file, `%if` / `%elif` / `%else`
/ `%endif` do conditional compilation, `%define` defines a symbol (equivalent to the command-line
`-D NAME`).

Things starting with `$` are macros: `$macro NAME(x) … $end_macro` defines one, called as
`$NAME(…)`; a parameterless object macro is written `$macro NAME()` and called `$NAME`. Inside a macro
body you can also use `$stringify(...)`, `$concat(a, b)` and `$line`.

> Symbols from `%define` are for conditional compilation only; they are not constants usable in
> expressions.

---

## 12. A glance at error reporting

Compile-time errors give a line and column and draw a squiggle under the source:

```text
語法錯誤：hello.mgl 第 2 行第 8 列：期望 标识符 / 常量(const) / 可变(mut) / 新建(new) 等 17 个之一，实际是 实现(impl)
   |
 2 | impl Foo { }
   |        ~~~~
```

Internal token names are translated into plain language: `分号(;)`, `左圆括号(()`, `右花括号(})`.
Diagnostics follow the environment language (`zh` / `en` / `ja`), overridable via `MAGEL_LANG`; see
[Toolchain](09-toolchain.md). With `MAGEL_LANG=en` the same error reads:

```text
Syntax error: hello.mgl:2:8: expected one of identifier / constant(const) / mutable(mut) / new(new) (17 candidates), found impl(impl)
   |
 2 | impl Foo { }
   |        ~~~~
```

---

## 13. What is not covered here

The tour stops here. The other half of Magel has its own documents:

| Feature | One line | Where |
| --- | --- | --- |
| Value semantics and ownership | Ordinary assignment copies; `move` explicitly transfers ownership from the source | [Core concepts](03-core-concepts.md) |
| Non-null references and escape checking | A reference cannot outlive its target | [Core concepts](03-core-concepts.md) |
| Resource management without a GC | `Drop` / `Closeable`, deterministic release at end of scope | [Core concepts](03-core-concepts.md) |
| `abi` blocks and the C ABI | Declare external functions / export Magel functions to C | [Host integration and C ABI](08-host-integration.md) |
| Annotations and reflection | Compile-time metadata, queryable and callable at run time | [Language reference](04-language-reference.md) |
| Coroutines and concurrency | `std::coroutine`, `std::thread` | [Standard library overview](07-stdlib-overview.md) |
| Bytecode delivery | `.svmb`, symbol compression | [Delivery security](05-delivery-security.md) |
| Embedded and AOT | Native code generation, memory pool, watchdog | [Cross-platform deployment](06-deployment.md) |

---

## 14. Next

| You want | Where |
| --- | --- |
| Understand value semantics and references | [Core concepts](03-core-concepts.md) |
| Look up how to write something | [Language reference](04-language-reference.md) |
| More runnable samples | [`example/samples/`](../../example/samples/) |
| Start a real project | `magel new my-app`; see [Toolchain](09-toolchain.md) |
