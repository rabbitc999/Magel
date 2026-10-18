# Matching, interpolation, generics and scope cleanup

This guide describes matching expressions, string interpolation, generic inference and scope cleanup.

## Match expressions

Match works in initializers, assignments, arguments, return values and nested expressions. A simple arm uses `{ value }` without a semicolon inside the block; a block with several statements produces its value through `yield value;`. The enclosing declaration or assignment still ends with a semicolon.

All normal arms must produce compatible values of the same type. Without `_`, an enum match must cover every variant; guarded arms do not prove exhaustive coverage. Non-enum value matches require a fallback. Only the selected arm runs.

Value payloads are copied in expression matches, including owned strings and structures. Changing a copy does not change the subject. Reference payloads retain their reference identity and cannot be used to mutate the subject or escape a finished branch. Existing statement matches keep their binding rules and accept a trailing semicolon.

## Interpolated strings

```magel
int64 f = 3;
String text = f"value={f + 1}, nested={f"inner {f}"}, \{literal\}";
```

Only f immediately followed by a double quote starts interpolation; f remains an ordinary identifier elsewhere. Each brace pair contains a complete expression. Arguments evaluate once from left to right. Lexical normalization separates text and expression boundaries, while the generated parser reads the expressions. Formatting uses the existing `std::lang::format` semantics and runtime implementation.

Ordinary string escapes apply; `\{` and `\}` produce literal braces. Ordinary strings do not interpolate. Nested interpolated strings expand from the inside out. Empty expressions and unmatched delimiters are compile errors.

## Associated types

```magel
interface Container { type Item; Item get(int64 index); };
struct Numbers : Container {
    type Item = int64;
    Item get(int64 index) { return index; }
};
C.Item first<C: Container>(C container) { return container.get(0); }
```

An interface declares an associated type with `type Item;`; an implementation supplies `type Item = T;` in its body. C.Item resolves to the associated type of C. Associated types participate in signature checking and interface inheritance. Missing or cyclic definitions and mismatched method signatures are errors. Module-level type aliases keep their existing behavior.

## Const generics and fixed array fields

```magel
struct Buffer<T, N: int64> { T data[N]; int64 len() { return N; } };
struct Matrix<T, rows: int64, cols: int64> { T data[rows][cols]; };
Buffer<int64, 4> values;
values.data[3] = 7;
```

Integer parameters are compile-time values. They evaluate to canonical integers and join type arguments in specialization identity. Equal types and sizes share a specialization; different sizes get different specializations. Defaults such as `N: int64 = 4` are supported. Runtime variables are rejected.

Fixed array fields are stored inline; two-dimensional arrays use row-major order. Sizes must be positive and fit the configured memory budget. Structure copies include array elements, and owned elements follow the existing copy and cleanup rules. Runtime arrays keep their existing syntax and semantics.

## Defer and scope(exit)

```magel
int64 main() {
    int64 value = 1;
    defer println("last {}", value);
    scope(exit) { println("first"); }
    value = 2;
    return 0;
}
```

Registration does not run the body. Normal scope exit, return, panic, break, continue and propagation through `?` run registered callbacks once. Callbacks in the same scope run in reverse registration order and read variables at exit time. They run before automatic resource cleanup. Defer handles cleanup outside ownership; Drop and Closeable keep their automatic behavior.

## Macro expansion

- `$if condition`: select a branch using a compile-time constant.
- `$elif condition`: test another condition if no previous branch was selected.
- `$else`: select the fallback branch.
- `$end_if`: end the condition block and check nesting with for blocks.
- `$let name = value`: bind local text for the current macro invocation.
- `$argc`: read the number of actual arguments.
- `$count(...)`: count a variadic tail or text list.
- `$typecheck(T, expr)`: check an expression using assignment compatibility, without an arbitrary cast.

Conditions support integer arithmetic, comparisons, logical operations and string equality. Logical evaluation short-circuits. Parameters and local bindings may supply constants; runtime variables cannot. Recursive expansion may terminate through a condition, with a maximum depth of 64 and 100000 expansions per preprocessing run. Arity errors remain compile-time errors. The removed match DSL, `##` and `#parameter` aliases are not restored.

The VM, AOT and diagnostic regressions live in the samples run by `sh check.sh` and the matching suites under `tests/`.
