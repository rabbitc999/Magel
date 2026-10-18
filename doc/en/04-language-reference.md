# 04 Language reference

Magel syntax and semantics, by topic. For a first impression read the
[Language tour](02-language-tour.md); for the reasoning behind the design read
[Core concepts](03-core-concepts.md).

The authoritative form of the grammar is the [EBNF grammar reference](../14-grammar-ebnf.md). This document is
about how to use it.

> Except where noted, every snippet here was executed on a default build.

---

## 1. Lexical structure

| Category | Spelling | Notes |
| --- | --- | --- |
| Line comment | `// …` | |
| Block comment | `/* … */` | nestable |
| Identifier | `[A-Za-z_][A-Za-z0-9_]*` | case-sensitive |
| Integer literal | `42`, `0xFF`, `b1010`, `1u`, `0xffu`, `b_101u` | Lowercase `u`: unsigned, defaults to `uint64`; `-1u` is rejected |
| Float literal | `3.14`, `1e9`, `1.5f`, `1f` | Unsuffixed: `float64`; lowercase `f`: `float32` |
| Character literal | `'M'`, `'\n'` | type `char` |
| String literal | `"hello"` | type `std::lang::String` |
| Boolean | `true` / `false` | |

Statements end with `;`, blocks use `{ }`. Preprocessor directives start with `%` and macros with `$`;
see §14.

---

## 2. Types

### Primitive types

| Category | Types |
| --- | --- |
| Signed integers | `int8` `int16` `int32` `int64` |
| Unsigned integers | `uint8` `uint16` `uint32` `uint64` |
| Floating point | `float32` `float64` |
| Other scalars | `bool` `char` |
| String | `String` (i.e. `std::lang::String`) |
| Big numbers / high precision | `BigInteger` `BigDecimal` |

**No implicit narrowing.** `int64 -> int32` and `int64 -> char` both need an explicit cast:
`(int32)x` / `(char)x`.

An integer literal with no type context defaults to `int64`, but inside an expression it takes
its type from context:

- the target of an assignment / initialization / argument / `return` propagates inward — both
  literals in `int32 a = 1 + 2;` are `int32`, so the whole expression is `int32`;
- in binary arithmetic / bitwise operators the **other operand's type** counts too — with
  `int32 x = 1;`, the `1` in `x + 1` is `int32`, so no `(int32)1` is needed;
- comparisons and logical operators do not propagate (their result is not an operand type), and
  with no context at all (`auto n = 1 + 2;`) the literal stays `int64`;
- pinning happens only when the value fits: an out-of-range literal falls back to `int64` and is
  reported by the usual "narrowing needs a cast" rule — never silently truncated.

Runnable check: [`literal_context_type.mgl`](../../example/samples/literal_context_type.mgl).

### Compound types

| Spelling | Meaning |
| --- | --- |
| `T a[N]` | fixed-length array with length in the type |
| `T a[]` | array whose length is decided at run time, created with `new[n]` |
| `(A, B)` | tuple; members read via `.0` / `.1` |
| `R fn(A, B)` | function type |
| `T mut&` / `T const&` | writable / read-only reference (`T &` ≡ `T const&`) |
| `Option<T>` | maybe a value: `Value(v)` / `None` |
| `Result<T, E>` | maybe a failure: `Ok(v)` / `Err(e)` |
| `List<T>` `Map<K,V>` `Set<T>` | built-in containers |
| `std::lang::Any` | type-erased box, used for runtime polymorphism |

---

## 3. Declarations

```magel
const int64 LIMIT = 100;   // constant
int64 counter = 0;         // variable
auto inferred = LIMIT;     // inferred type
```

- Top-level declarations are global; declarations inside a function are local.
- Struct fields and static fields: §6.
- Array lengths may be constant expressions.

---

## 4. Functions

```magel
int64 add(int64 a, int64 b) { return a + b; }
void log(String message) { println("{}", message); }
```

- Parameters are **by value** (copies) by default. To avoid copying, or to allow mutation, write
  `T mut&` / `T const&`.
- Overloading is supported: selection is by argument types.
- Variadic trailing parameters are written `T... items`, used with `for-in` or `std::iter`.
- `static` methods belong to the type and are called `Type::method()`.
- The entry point is named `main`, returns `int64` or `void`, and takes no parameters or `std::lang::List<std::lang::String> arguments`. Normal completion or `return;` from `void main` produces process exit code 0; panic still fails.

### Multiple return values and generic tuples

Use a tuple as the return type. `return a, value;` is equivalent to `return (a, value);`.
Type parameters may occur in any tuple element and are replaced by explicit specialization or argument inference.
Existing assignment targets also provide context for generic return-type inference.

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

`c, str2 = ...;` and `(c, str2) = ...;` assign to existing writable targets. Declare new bindings with `auto (c, str2) = ...;`.
The complete source tuple is evaluated and saved once before targets are assigned from left to right, so
`a, b = (b, a);` or `(a, b) = (b, a);` swaps values. Variables, writable fields, array or container elements, and writable references
are supported; `a, (b, c) = ...;` matches nested tuples. Element counts, shapes and target types must match.
Const variables and read-only references cannot be assigned. Ordinary copy, drop, reference and lifetime
checks apply. Tuple members such as `.0` remain read-only.

### `move`: transfer ownership without copying

The language intrinsic has the signature `T move(T mut& source)`. Its result type is inferred from the reference target; no import is needed.

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

The argument is evaluated once. Existing resource handles and aggregate slots are transferred to the result, then the source storage is cleared and its value lifetime ends. Moving does not clone strings, containers, Any or closures and does not run the source's close/drop methods. The destination owns the transferred resources and eventually destroys them. Borrowed reference fields retain their original lifetime constraints.

Only writable references are accepted. Supported targets include scalars, strings, containers, Any, functions, concrete aggregates, and file/process handles. Arrays, interface values without a concrete layout, and opaque host types cannot be moved directly; concrete array elements can be moved.

Statically tracked reads and repeated moves after a move are rejected, including scalar reads and aliases. Assignment does not restore usability; a new declaration is required. Redeclaration requires the whole variable to have been moved on every incoming path. Moving one field leaves disjoint fields usable but prevents reading the complete aggregate.

Aggregate lifetime markers prevent duplicate destruction. Moving from manually allocated storage does not free its storage block; delete the original reference when appropriate. Ordinary assignment and ordinary value parameters still copy. Use references for calls that must avoid copying, and move explicitly where the result should take ownership. Initialization, assignment, returns and temporary reception transfer owned fields from the move result. See the [Ownership transfer](23-ownership-move.md).

The toolchain and MCU share the single `library/stackvm/` core. MCU builds trim host features with macros and use injected static pools.

### Default arguments

A parameter can have a default expression; calls may omit a trailing sequence of arguments:

```magel
int64 add(int64 a, int64 b = 10) { return a + b; }
String greet(String name = "Magel") { return "Hello, " + name; }
// add(2) == 12; add(2, 3) == 5; greet() == "Hello, Magel"
```

Free functions, instance methods, static methods, and constructors support defaults. Generic functions
can infer types from explicitly supplied arguments. Each omitted argument evaluates its default at
call time; explicitly supplied arguments skip that evaluation. Names resolve in the defining module's
global scope, independently of caller locals. Defaults cannot refer to parameters or `self`.
After a default parameter, only more default parameters or a final variadic parameter may follow.
Positional arguments bind from left to right, without skipping intermediate parameters.

Defaults are excluded from function types and overload signatures. Overload matching accepts omitted
trailing defaults and retains exact matching before numeric widening; multiple matches in the same
tier are ambiguous. When omitted defaults cause ambiguity, diagnostics list candidate parameter types,
names, default markers, and omitted parameters, with advice to supply distinguishing arguments or
adjust overload signatures. Function pointers and lambdas still require all fixed arguments in their `fn(...)`
type. Enum payload fields and interface method declarations do not support defaults yet.

### Constructors and `new`

A struct may define a constructor, called with `new(...)`:

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

Heap objects are written `T mut& p = heap T;` (no constructor arguments — the element type comes from
`p`'s type) and freed with `delete p;`.

`new(args)` carries no type information — **the declaration on the left decides which constructor
runs**:

| Form | Which path |
| --- | --- |
| `T v = new T(args);` | stack value, calls `T`'s **real constructor** (same name as `T`) |
| `T mut& p = heap new T(args);` | heap object, same real constructor, remember `delete p;` |
| `T v = T::create(args);` | **named constructor entry point** `static T create(...)` |
| `T v = T::otherFactory();` | an ordinary static factory (any name) |

```magel
struct Point {
    int64 x;
    int64 y;
    Point(int64 x, int64 y) { self.x = x; self.y = y; }             // real constructor
    static Point create(int64 x, int64 y) { self.x = x * 10; self.y = y * 10; }  // named entry point
};
int64 main() {
    Point o = new Point(3, 4);    // real constructor → 3,4
    Point mut& p = heap new Point(1, 2);  // real constructor + heap → 1,2
    Point q = Point::create(3, 4);  // named constructor entry point → 30,40
    println("{},{} {} {} {} {}", o.x, o.y, p.x, p.y, q.x, q.y);
    delete p;
    return 0;
}
```

```text
3,4 1 2 30 40
```

`static T create(...)` is a **named constructor entry point**: it also carries an implicit `self`, so the
body just writes into `self` (or `self.field`) and does **not** write `return` — the value is taken by
the call site. Discarding its result is an error.

---

## 5. Operators

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

Precedence follows C: unary > multiplicative > additive > shift > relational > equality > bitwise AND >
bitwise XOR > bitwise OR > logical AND > logical OR. When in doubt, parenthesise.

Compound assignment `+=` `-=` `*=` `/=` `%=` `&=` `|=` `^=` `<<=` `>>=` all exist.

`++` / `--` check actual storage targets. Within a full expression, an updated target cannot also be read or written by another operation.
`i = i++`, `i += i++`, `i++ + i`, and `i++ + ++i` report `MGL1301`.
Independent targets are allowed in `a = (i++) + (j++)`, `a += i++`, and `f(i++, j++)`.
Distinct fields and distinct constant indices are independent; dynamic indices conservatively overlap when their separation cannot be proved.
Direct reference aliases use their source identity. Parentheses, arguments, and short-circuit branches remain part of the enclosing full expression;
statements, declaration initializers, and the three for clauses are checked separately.

Assignment target locations are evaluated once: `a[f()] += 1` calls `f()` once, and both `a[i++] = 1` and
`a[i++] += 1` increment `i` once. Compound assignment saves the location before reading, calculating, and writing back.
The array element and its index variable are different targets. See [`expression_effects.mgl`](../../example/samples/expression_effects.mgl).

Structs may overload operators by inheriting the **left interfaces** `Add` / `Subtract` /
`Multiply` / `Divide` / `Modulo` / `Equal` / `NotEqual` / `Less` / `LessOrEqual` / `Greater` /
`GreaterOrEqual` / `And` / `Or` / `Xor` / `ShiftLeft` / `ShiftRight` (methods `leftAdd` and
friends); the other side of the operator needs its own implementation via the **right
interfaces** such as `RightAdd` (method `rightAdd`). Type arguments are written
`<result type, other operand type>`.

**Anything the compiler can derive, you don't have to write:**

- **Commutativity**: `+` `*` `==` `!=` `&` `|` `^` are commutative, so with only the left
  interface declared, `5 + a` works exactly like `a + 5` (the two operands are swapped for you).
  Subtraction / division / modulo / ordering / shifts are **not** commutative — the reverse
  direction needs an explicit `RightSubtract`-style right interface.
- **Comparison duality**: with only `Less` declared, `5 > a` (dual — same as `a < 5`), `a >= 5`
  (negated — `!(a < 5)`) and `5 <= a` (dual + negated) all work. `a > 5` does not: that needs
  `int64 < Vec`, so `Vec` has to provide `Greater`. `==` and `!=` negate each other.
- **Explicit beats derived**: with both `Add` and `RightAdd` declared, `5 + a` uses `RightAdd`
  and is not shadowed by commutativity. `&&` / `||` are built-in short-circuit operators and
  **cannot be overloaded** (the diagnostic says so directly).
- **Right-side switches**: commutativity and duality both borrow the right-hand operand as the
  receiver, so in `5 + a` what matters is what `a`'s type says —
  - **nothing declared**: derive as above; both `a + 5` and `5 + a` work;
  - **`RightAdd` declared**: the right direction is yours; `5 + a` only uses `rightAdd`, and if
    its parameter cannot take the left operand it is an error — it does **not** fall back to
    borrowing `Add`;
  - **`NoRightAdd` declared**: explicitly forbidden; `5 + a` is an error. It is an **empty
    marker interface** named `No` + the right interface name (`NoRightAdd`, `NoRightLess`, …)
    with no type arguments, and the left direction `a + 5` is unaffected. `NoRightXxx` wins
    over `RightXxx`.
- **Evaluation order**: swapping changes only which operand is the receiver, not when they are
  evaluated — still left to right (`bump() + makeA()` runs `bump()` first, then `makeA()`).
- **Same operand pair, different result types**: one type may implement both `Add<int64,int64>`
  and `Add<Vec,int64>` — the two `leftAdd(int64)` methods differ only in return type. Ordinary
  method overloading cannot do that (C++ cannot either — return type alone does not
  disambiguate), so operator interfaces **make an exception**: the mangled name carries the
  return type, and both survive. Which one is picked is decided by the **target type** —
  `int64 x = a + 3;` takes the `int64`-returning one, `Vec y = a + 3;` takes the `Vec`-returning
  one. With no target type (`auto z = a + 3;`) it is an error that asks you to say which —
  the compiler never silently picks one.

Runnable checks: [`operator_symmetry.mgl`](../../example/samples/operator_symmetry.mgl),
[`operator_right_direction.mgl`](../../example/samples/operator_right_direction.mgl),
[`operator_result_choice.mgl`](../../example/samples/operator_result_choice.mgl),
[`operator_swap_eval_order.mgl`](../../example/samples/operator_swap_eval_order.mgl). Rejected
forms:
[`operator_asymmetric_reverse.mgl`](../../example/samples-error/operator_asymmetric_reverse.mgl),
[`operator_right_forbidden.mgl`](../../example/samples-error/operator_right_forbidden.mgl),
[`operator_right_explicit.mgl`](../../example/samples-error/operator_right_explicit.mgl),
[`operator_result_ambiguous.mgl`](../../example/samples-error/operator_result_ambiguous.mgl),
[`logical_operator_overload.mgl`](../../example/samples-error/logical_operator_overload.mgl).

---

## 6. Structs

Declarations of `struct`, `enum`, and `interface` end at the closing `}`. The trailing semicolon is optional, and existing `};` declarations remain valid. Fields, abstract interface methods, and ordinary statements retain their semicolon requirements.

```magel
struct Point {
    int64 x;
    int64 y;

    int64 sum() { return self.x + self.y; }
    static Point origin() { Point p; p.x = 0; p.y = 0; return p; }
};
```

- Fields and methods share one pair of braces; inside a method `self` refers to the receiver.
- `static` fields/methods belong to the type: `Point::origin()`.
- Interfaces: `struct Circle : Shape { … }`, several interfaces comma-separated, all must be fully
  implemented.
- `Drop` (destructor) and `Closeable` (early close) — see
  [Core concepts](03-core-concepts.md#4-no-garbage-collection-deterministic-release).
- Constructors: §4.

---

## 7. Enums and match

```magel
enum Message {
    Quit,
    Move(int32 x, int32 y),
    Text(String text)
};
```

Variants may carry fields (algebraic data types). Handle them with `match`:

```magel
void show(Message message) {
    match (message) {
        Message::Quit => { println("quit"); }
        Message::Move(x, y) => { println("move {} {}", x, y); }
        Message::Text(text) => { println("text {}", text); }
    }
}
```

`match` checks **exhaustiveness**: omitting a variant is a compile error. `_` is the catch-all.

### Scalar match: constants and ranges

```magel
int64 grade(int64 score) {
    match (score) {
        100 => { return 3; }
        [90, 99] => { return 2; }
        [60, 89] => { return 1; }
        _ => { return 0; }
    }
    return 0;
}
```

Ranges support only the half-open `[a, b)` and closed `[a, b]` forms. Left-open ranges are rejected:

| Spelling | Meaning |
| --- | --- |
| `[a, b)` | `a <= x < b` |
| `(a, b]` | `a < x <= b` |
| `[a, b]` | `a <= x <= b` |

> Scalar branches accept **constants or ranges only**. A variable, function call or other expression
> produces: "match branch must be an integer / float / bool constant or a range pattern".

Integer match patterns with compile-time bounds are checked for overlapping coverage. `[15,25]` and `[10,20]`, or `15 =>` and `[15,20]`, produce warning `MGL1601`, regardless of which pattern comes first. `[10,15)` and `[15,20]` do not overlap. Empty ranges are ignored. Comparisons preserve full int64/uint64 precision and consider only values representable by the subject type. Constant-expression bounds are checked; runtime bounds are not inferred. Floating-point and string patterns are outside this warning.

Warnings do not prevent compilation or change branch order. Patterns are tested in source order, followed by their guards. Guarded patterns can also receive coverage warnings; this does not prove that a branch is unreachable. Duplicate unguarded single-value patterns remain compilation errors. CLI warnings include source locations, JSON uses `severity: "warning"`, and the editor displays warning severity.

### Guards

Bind a variable, then add a condition with `if`:

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

## 8. Interfaces and runtime polymorphism

```magel
interface Shape {
    int64 area();
    String describe() { return "shape"; }   // default implementation
};
```

- An interface holds methods only; methods may have default bodies, but **no fields**.
- Interfaces can inherit: `interface B : A { … }`.
- A struct may implement several interfaces; two requirements with the same signature count once
  (deduplicated).
- An interface name may be used as a reference target type: `Shape mut& view`. That is a "fat
  reference" (address + type id), so `.area()` picks the implementation **at run time** by type id.

Interface references bind directly to concrete references whose types implement the interface:

```magel
Circle circle;
circle.r = 4;
Shape const& view = &circle;
```

The same conversion works in assignments, arguments, and returns without an Any intermediary. A read-only source cannot become mutable. Views borrow their source; copying a view does not extend the concrete object's lifetime. Existing Any values can still expose interface views through checked reference extraction.

See [`example/samples/interface.mgl`](../../example/samples/interface.mgl).

---

## 9. Generics

```magel
int64 total<T: Shape>(T shape) { return shape.area(); }
```

- Type parameters follow the name: `<T>`, `<T, U>`.
- Constraints are written `T: Interface`; several are comma-separated.
- Generic structs work the same: `struct Triple<T: Valued, K, V> { … }`.
- Monomorphised at compile time; the call site is resolved statically, no vtable overhead.
- Calls may be explicit (`total<Circle>(c)`) or inferred.


An outer template can bind some parameters of an inner template while retaining its own parameters.
Concrete uses recursively generate complete instances:

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

The field of `TypeB<String>` becomes `TypeA<String, TypeC>`. `TypeB<int64>` has its own layout,
methods and static fields. A template with identical canonical arguments reuses its instance;
`String` and `std::lang::String`, for example, do not produce duplicates. Unique imported type names and their module-qualified spellings also share an instance.
Further nesting and reordered parameters work with generic functions, interface constraints,
enum payloads, containers, tuples, function values and references.
Inside methods, `item.first`, `item = value` and `&item` follow the same layout and ownership rules as `self.item`.

Default type arguments use `struct Box<T, V = T, W = TypeA<T, V>> { W value; };`.
Defaults may refer to earlier parameters and are bound from left to right, including nested expansion.
Self references and forward references are rejected. The same rule applies to unbound parameters with
explicit defaults during generic function inference; unbound parameters without explicit defaults are errors.
Omitted struct arguments require explicit types or defaults written in the template declaration; they no longer default to Any. Constraints are checked after concrete
nested instances have been generated. Monomorphisation preserves strings, characters and comments verbatim.

This is template composition; parameter-pattern partial specialisation with separate implementation bodies
has no syntax at present. Recursive value fields are rejected, while references to the same instance are allowed.
Expanding recursion is diagnosed: a compilation allows at most 1024 generic instances and rejects type-expression
nesting at 128 levels. The regression suite, [`tests/magel_generic_monomorphization.py`](../../tests/magel_generic_monomorphization.py),
covers interpretation, AOT, instance reuse and invalid programs.

---

## 10. Control flow

```magel
int64 main() {
    int64 i = 0;
    while (i < 3) { i = i + 1; }
    println("while={}", i);
    int64 n = 0;
    loop {
        n = n + 1;
        if (n >= 3) { yield n; }
    };
    println("loop={}", n);
    int64 total = 0;
    for (int64 k = 0; k < 5; k = k + 1) {
        if (k == 1) { continue; }
        total = total + k;
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

| Construct | Notes |
| --- | --- |
| `if / elif / else` | conditional branches |
| `while (cond) { }` | test before body |
| `for (init; cond; step) { }` | three-part |
| `loop { … yield v; }` | infinite loop; `yield` carries the result |
| `for (T x in c) { }` | iterate a container / iterator |
| `break` / `continue` / `return` | exit / continue / return |

> `loop { … }` is an **expression**; in statement position write `loop { … };`.

---

## 11. Containers

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

| Container | Common operations |
| --- | --- |
| `List<T>` | `push` `pop` `len` `at` `[]` `has` `remove` `clear` |
| `Map<K, V>` | `[]` `has` `remove` `len` `keys` `values` |
| `Set<T>` | `insert` `has` `remove` `len` |

Collection literals: `{1, 2, 3}` is a `Set`, `{"a": 1}` is a `Map`.

Iterator chains (`map` / `filter` / `collect`) need `import std::iter;` — see
[Language tour](02-language-tour.md#7-collections-and-iterators).

---

## 12. Modules and imports

```magel
module mylib::math;      // declare the module this file belongs to
import std::iter;        // import a module
import std::text;
```

- A module is declared with `module name::subname;`, usually on the first line.
- After `import` you can write fully qualified paths.
- The core module `std::lang` (`println` / `format` / `assert` / `len` / `Any` …) is loaded
  automatically and needs no `import`.
- Visibility, exports and cross-module generics: see
  [`example/samples/import_names.mgl`](../../example/samples/import_names.mgl).

---

## 13. Reference cheat-sheet

```magel
int64 value = 7;
int64 mut& alias = &value;    // writable reference
int64 const& view = &value;   // read-only reference (same as int64 &)
alias = 9;                    // writing through it writes value
```

```magel
T mut& owned = heap T;         // heap object (element type from owned's type)
delete owned;                  // free
```

Rules and restrictions (escape checking, `MGL1204`): see
[Core concepts](03-core-concepts.md#2-references-non-null-and-never-outliving-their-target).

---

## 14. Preprocessing and macros

```magel
%define DETAIL 1
$macro SQUARE(x)
((x) * (x))
$end_macro
```

| Directive | Effect |
| --- | --- |
| `%include "path"` | expand a file in place; the same file is expanded only once by default |
| `%include_once` / `%import` | synonyms for `%include`, expressing the intent explicitly |
| `%if` / `%elif` / `%else` / `%endif` | conditional compilation; inactive branches are dropped wholesale |
| `%ifndef` / `%ifdef` | branch on whether a symbol exists |
| `%define` / `%undefine` | define / remove a symbol (same as command-line `-D NAME`) |
| `%error` / `%warning` | error / warning inside an active branch |
| `$macro N(x) … $end_macro` | function-like macro, called `$N(…)` |
| `$macro N((x), ...(args)) … $end_macro` | explicitly group x and each variadic argument in an ordinary macro |
| `$macro expr N(x, ...args) … $end_macro` | group every argument and the full result; redundant `(x)` and `...(args)` annotations are allowed |
| `$macro N() … $end_macro` | object macro, called `$N` |
| `$stringify(…)` / `$concat(a,b)` / `$line` | macro-body helpers |
| `$delete_macro N` | remove a macro |

Points to remember:

- Definition annotations do not change calls: `$N(x + 1, y * 2)`. Bare parameters in ordinary macros retain textual substitution. `$replace` follows the same rules.
- The old `!` definition prefix, backtick parameter annotations and `$!` calls are removed and rejected. `$concat` and `$stringify` always read the original argument text.
- Directive lines and dropped lines are replaced by **blank lines**, so line numbers after
  preprocessing match the source one-for-one and errors point at the file you actually wrote.
- A directive counts only at **line start**; a `%` inside a string is not a directive (write `%%` for
  a literal percent).
- Symbols from `%define` are for **conditional compilation only**; they are not expression constants.

---

## 15. Annotations and reflection

```magel
@annotation Route(path: String, level: int32 = 2)

@Route("/health", level: 1)
int64 health() { return 200; }
```

- `@annotation Name(params)` declares an annotation type; parameters may have defaults.
- Annotations can sit on structs, interfaces, enums, fields, methods, constructors and functions.
- At run time you can query annotations on declarations, call functions by name, and dispatch
  through interfaces.

**Build requirement**: the root toolchain defaults to `MAGEL_WITH_REFLECTION=1` and
`STACKVM_WITH_REFLECTION=1`; source compilation enables reflection by default. `make reflection`
remains an explicit enable target. `--reflection` explicitly requires support and `--no-reflection`
disables it, rejecting annotations/reflection use. Only explicit trimmed builds set both macros to 0.
Generated compiler projects and MCU hosts have their own build configuration.

Reflection artifacts require a capable target VM. Compile explicitly with `--no-reflection` for the
default MCU VM. Dynamic-call and signature limits are specified in the [reflection document](../19-reflection.md).

Full examples: [`example/reflection/`](../../example/reflection/) (`annotations.mgl`, `calls.mgl`,
`dispatch.mgl`, `queries.mgl`).

---

## 16. The C boundary: `abi` blocks

```magel
abi "./native.dll" {
    int64 c_function(int64 x) = "c_function";
}
```

An `abi` block imports C ABI functions from a library. Use AOT-only `export` to expose Magel functions to C.
Type mapping, nullable references and glue bindings: see
[Host integration and C ABI](08-host-integration.md).

---

## 17. Common built-ins

| Function | Effect |
| --- | --- |
| `println(fmt, …)` | formatted output with a newline |
| `format(fmt, …)` | format into a string |
| `assert(cond)` | assert; aborts on failure |
| `len(x)` | length of a string / container |
| `{}` inside `println` | positional placeholder; the type is inferred |

More capabilities (`json` / `toml` / `regex` / `sql` / `net` / `linalg` …): see
[Standard library overview](07-stdlib-overview.md).

---

## 18. Next

| You want | Where |
| --- | --- |
| The authoritative grammar | [EBNF grammar reference](../14-grammar-ebnf.md) |
| More runnable samples | [`example/samples/`](../../example/samples/) |
| Constructs expected to fail | [`example/samples-error/`](../../example/samples-error/) |
| Commands and compile options | [Toolchain](09-toolchain.md) |

### Idempotent destruction and release

Repeated `drop()` and `delete` have no additional effect. Assignment starts a new lifecycle; `const &` still cannot call `drop()`. Reads and writes after release, and reads after move, produce `MGL1205`; a moved binding cannot be written or rebound; a new declaration is required. The default heap reuses physical memory without reusing logical allocation identities, so repeated deletion through old references cannot affect new objects. See [borrow control flow and algorithm validation](11-algorithm-validation.md).

The program entry `main` must return `int64` or `void`. Function pointers can be array elements: `int64 fn(int64) a[] = new[2];`; assign elements before calling `auto callback = a[0]; callback(value);`. Function types compose with arrays, tuples, containers and nested function types. Write an array-returning function type as `(int64[]) fn()` to distinguish it from an array of function pointers.

Prefer return-first syntax: `int64 fn(int64, int64) callback` and `int64 fn(int64 x) { x + 1 }`. Starting with **0.12.0**, the old arrow forms are rejected. Omit the arrow for inferred results: `fn(int64 x) { x + 1 }`. Regenerate generic host glue and rebuild old caches when upgrading.

Within the same block, a variable can be redeclared after its entire value has been moved, including with a different type: `String a="1"; String b=std::lang::move(&a); String a="2";`. The declaration allocates independent storage; old references still identify the old object. Every path reaching the declaration must have moved the entire previous value. Moving a field, moving on only some branches, or assigning to the old variable after moving does not permit redeclaration.

For Any type checks and safe extraction, use `isType<T>`, `tryGet<T>`, `tryGetReference<T>` and `tryGetMut<T>`. `nameof` and the box’s `.name()` have been removed; see the [Any, explicit generics and data utilities](22-any-generics-data.md).

An anonymous function returning another function may group its return type: `(int64 fn(int64)) fn(int64 fn(int64) a, int64 fn(int64) b) { int64 fn copy(int64 x) { a(b(x)) } }`. Neither trailing expression has a semicolon: the outer expression returns a closure and the inner expression returns an integer. The returned closure must own `a` and `b` rather than borrow parameters from an invocation that has ended. See the [function and closure examples](../book/15-函数值与闭包.md).

## fn capture modes

External locals are captured by `const&` by default; `fn const(...)` is equivalent. Reads observe later changes in the source, while writes are rejected. `fn mut(...)` captures by `mut&` and writes back to the source. `fn copy(...)` owns mutable independent copies. `fn move(...)` transfers captured values into the closure and forbids reading or writing the old bindings; declare new bindings before reuse. Copying a closure duplicates owned state and preserves references to borrowed sources. Reference captures must not outlive their sources; owned captures may escape only when their contents do not borrow shorter-lived storage. Capture lists use `fn copy[a, b] mut[c] const[d] move[e](int64 x) { ... }`; unlisted external locals use `const&` by default. One trailing default mode may override this: `fn copy[a, b] mut(int64 x)` borrows other locals by `mut&`. Multiple defaults, such as `copy[a, b] mut const(...)`, are rejected.

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

Duplicate names, unknown variables, parameter conflicts, and mut/move captures of read-only sources are rejected. Listed variables are captured even when unused in the body. Capture applies to the whole variable. Copying or moving a reference does not own its referent or extend its lifetime. Direct global access uses global storage; explicitly list a global for a snapshot. Convert array values to containers or explicit references. File and process handles support move capture but cannot be copied, including through closure copies.

## Integer intervals in for-in

`for(auto item in source)` and `for(item in source)` share element type inference. Lists and other iterators preserve their element type; integer intervals infer it from the endpoints. Explicit types still undergo static compatibility checks.

`for (uint8 x in [0,255]) { ... }` yields 256 values, from 0 through 255; `[0,255)` excludes 255. All eight fixed-width signed and unsigned integer types are supported. Omitted variable types are inferred from the endpoints; ordinary integer literals default to int64.

Intervals are internal lazy `Iterator<T>` values using the existing for-in protocol, without allocating a list. Bounds are evaluated once each, from left to right. Both endpoints must fit T, including an exclusive upper endpoint. Constant violations are rejected at compile time; dynamic values are checked before iteration. int8 spans -128..127, so `for(int8 x in [0,255])` is rejected. Reversed intervals are errors; [a,a) is empty and [a,a] yields one value.

A closed interval finishes at its last value without incrementing the maximum integer. uint8 255 and uint64 18446744073709551615 are valid inclusive upper bounds. Changing the iteration variable does not change the iterator state. Existing break, continue, return and lambda capture rules apply.

## Breaking out of nested loops

`break level N;` counts from the current loop and exits the Nth enclosing loop, resuming after that loop. `break level 1;` is equivalent to `break;`. `break all;` exits every enclosing loop in the current function and resumes after the outermost loop; `break level all;` is equivalent. `level` and `all` are recognized only in this position, so `all` remains available as an ordinary variable name.

Only `loop`, `while`, `do-while`, `for` and `for-in` count. Ordinary blocks, value blocks, `if` and `match` do not count. N must be a positive decimal integer literal, not a variable or expression. Zero, negative numbers, excessive depths and use outside a loop are rejected. Exits cannot cross function or `fn` boundaries. These forms carry no result; targeting a loop expression that requires a value is rejected. Use `yield value;` to supply a result.

```magel
loop {
    while (true) {
        for (int64 i = 0; i < 10; i++) {
            if (i == 3) { break level 2; }
        }
    }
    // The for and while loops have ended; execution is still inside loop.
    break all;
};
```

Scope cleanup runs from inner to outer scopes, including `defer`, `scope(exit)` and resource cleanup. Statements and update expressions bypassed by the exit do not run. See the [nested-loop exit example](../../example/samples/break_level.mgl).

## Block expressions and interpolated output

A nonempty statement block can produce a value in an initializer, assignment, argument or return. Every normally completing path must exit using `yield value;`. Ordinary statement blocks retain their behavior; empty `{}` remains a collection literal.

```magel
int64 main() {
    int32 a = { int32 b = 3; yield b; };
    a = { int32 b = 4; yield b; };
    print(f"a={a};");
    println(f"next={a + 1}");
    return 0;
}
```

A value exit (`yield`) selects the nearest result-receiving expression, crossing intervening while, do-while, for, for-in and statement-position loop constructs. Nested loop, block, if and match expressions used in initializers, assignments, arguments or returns still receive their own exit values. Plain `break;` exits only the nearest loop and does not produce a result for an enclosing expression. `yield (a, b);` forms one tuple: each element is evaluated once from left to right, with element types constrained by the receiving tuple type. A block expression does not create a loop, so `continue;` searches for an enclosing loop. Results are saved before local resources are cleaned up. Strings and lists retain value semantics, and references to expired locals cannot escape.

For example, a loop can directly complete an enclosing block assignment, while nested assignment blocks receive their own results:

```magel
(int64, String) found = {
    for (int64 i = 0; i < 10; i++) {
        if (i == 3) { yield (i, "found"); }
    }
    yield (-1, "missing");
};
int64 a = { int64 b = { yield 1; }; yield b + 1; }; // a == 2
```

See the [value break sample](../../example/samples/break_values.mgl) for nested loops and cleanup checks.

A standalone brace statement remains an ordinary scope even with a trailing semicolon; it does not create a disposable value receiver. A `yield value;` without an enclosing receiving expression is an error. In nested initializers, `yield 5;` completes the inner loop for z and `yield z + 1;` completes the outer loop for y. A final `yield y + 1;` inside a standalone outer block still has no receiver.

`print` and `println` accept a single interpolated string or `String` expression. Braces and percent signs in the resulting text are printed literally. Traditional `println("{}", value)` formatting remains supported; interpolated output does not accept additional format arguments.

A hygienic variadic macro can build a list:

```magel
$macro vec(type, ...values)
{
    List<type> $0;
$for item in values
    { type $1 = item; $0.push($1); }
$end_for
    yield $0;
}
$end_macro
```

Use `List<int32> values = $vec(int32, 1, 2, 3);` or `values = $vec(int32, 4, 5);`. `$0` and `$1` are hygienic names. Each argument is evaluated once, received using the requested type and appended in order with `push`. No elements produces an empty list. See the runnable [block example](../../example/samples/block_expression.mgl) and [output example](../../example/samples/print_interpolation.mgl).

Use `fn` for anonymous functions; the old `lambda` syntax has been removed: `auto add = fn(int32 x) { x + 1 };`. An explicit result type precedes `fn`: `auto add = int32 fn(int32 x) { return x + 1; };`. Capture modes and lists use `fn`. `int32 fn(int32)` still denotes a function type.

A `fn` expression can be passed directly to a named function or a function value. Each callback is
checked against its parameter's function type, which also provides context for omitted parameter
types. See [`fn_inline_arguments.mgl`](../../example/samples/fn_inline_arguments.mgl) for a nested
function result combined with tuple values.


## Moving and cleaning up value exits

Explicit `yield i;` prefers moving a writable local variable declared inside the receiving value expression, equivalent to `yield std::lang::move(&i);` for that source. Variables outside that expression remain usable and are copied. Crossing loops does not change this boundary. Direct variables in tuples, including nested tuples, follow the same rule. Const values, read-only sources, reference bindings, fields and indexing retain existing value semantics; use explicit `move(&value)` when a transfer is required. If scope exit callbacks are present, an independent result is conservatively saved so callbacks can still read or modify the source.

Expressions such as `yield i + 1;` and `yield makeText();` are evaluated once and their result temporaries are copied or adopted; the expression itself is not treated as an addressable variable. Compound expressions such as tuples retain existing construction and value-parameter rules before the exit adopts their final result. Ordinary branch tail expressions `{ value }` retain their existing value semantics. Explicitly construct a copy using string concatenation, container copy or a type-provided copying method when needed. To force a move of an external variable, write `yield move(&external);`; subsequent access follows the existing moved-binding restrictions.

The result is saved before exit callbacks and automatic cleanup. Moving clears the source storage and ends its binding lifetime without invoking close/drop on the source; the final receiver owns cleanup. Types implementing both Closeable and Drop close before dropping. Copies own independent resources, so source and result are cleaned up at their respective lifetime ends. Exits across loops clean up every scope that is left without releasing resources already transferred to the result.

Moving does not extend borrowed lifetimes, including references stored inside aggregates. The [move and cleanup sample](../../example/samples/break_move.mgl) checks evaluation counts, raw resource handle identity, external variable copying, destructor counts, cleanup order and VM/AOT behavior. See [ownership transfer](23-ownership-move.md) for general rules.


## Default value text and string conversion

Every non-void value supports `String toString()` and conversion to String. Numbers display their values; booleans display `true`/`false`. Structs display field values in declaration order as `{value, value}`, List/arrays as `[...]`, Set as `{...}`, Map as `{key: value, ...}`, and tuples as `(...)`. Nested strings are double-quoted and escaped; a top-level string returns its own text. Any displays its payload, or `Any()` when empty. Enums display their variant and payloads. References, functions and opaque resources display `<type>` without dereferencing. Depth and cycle guards bound recursion. This text is not a reversible serialization format.

A user-defined no-argument `String toString()` supplies an implicit String conversion when no dedicated conversion exists. An existing `ImplicitConvert<String>` supplies a missing no-argument toString. If both are explicit, each retains its own behavior. `cast<String>` first chooses `ExplicitConvert<String>`, then implicit conversion or default value text. When only `toString(String style)` exists, the generated no-argument method supplies an empty style. Standard ToString and ImplicitConvert<String> generic constraints recognize the default capability; unrelated user interfaces with the same name do not.

Struct `{}` formatting reuses the no-argument method; styled overloads retain their behavior. Conversion evaluates its source once, preserves ownership, cleans up temporary sources, and works without reflection. Nested user methods must be read-only. Container snapshots isolate callback reentry and mutation, and callback failures preserve their original diagnostics. See the [value-text sample](../../example/samples/value_text.mgl).

## Returning control expressions

`return { ... yield value; };`, `return if ...;`, `return match ...;` and `return loop ...;` receive the value in a hidden slot before cleanup and return. All result branches require identical types. Direct owned locals, including const bindings, move on yield; outer variables copy. Nested return, break level / break all, escaping local references and deferred cleanup that still borrows the moved source are rejected. These special return rules extend ordinary value exits. See [declarations and return expressions](28-declaration-sugars.md).

## Named function declarations

Statements accept `int64 add(int64 x) { return x + 1; }` and a local function binding prototype `int64 callback(int64 x);`. The declared result anchors checking. Captures default to const&, with explicit mut, copy or move before the body. Capture state moves into the binding; borrowed captures cannot outlive their sources. This syntax is absent from expression productions. See [named function rules](28-declaration-sugars.md).

## Field derivation

`#derive(Equal, Hash, Clone)` before a struct or enum uses existing text macros to generate field-based methods, followed by normal semantic checking and quadruple lowering. Nested fields must implement each requested interface. Drop and Closeable derivation is forbidden. Generic methods are generated after instantiation. See [derivation rules](28-declaration-sugars.md).
