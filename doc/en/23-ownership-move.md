# Ownership transfer

[Documentation index](README.md) · [Ownership transfer](../23-ownership-move.md)

`move(T mut&)` now transfers existing resources instead of copying and destroying the source. Ordinary assignment and ordinary value parameters still copy. Receiving a move result directly transfers strings, containers, Any, closures and owned aggregate fields without deep copies.

```magel
struct Packet { String text; List<int64> values; };
int64 main() {
    Packet source;
    source.text = "payload";
    source.values.push(42);
    Packet result = move(&source);
    Packet source;
    source.text = "next";
    println("{} {}", result.text, result.values[0]);
    return 0;
}
```

The source reference is evaluated once. Its handles and slots are transferred, source storage is cleared, and its value lifetime ends. Moving does not run source close/drop methods; the final owner performs cleanup. Code that relied on destruction side effects during a move must change. Close external resources explicitly when immediate closure is required.

The old binding cannot be read, written or rebound after a move. Declare a new binding to reuse the name. Scalars also participate in moved-value checks. Redeclaration requires a complete move on every incoming path; moving one field or only one branch is insufficient. Disjoint fields remain usable after a partial move, while reading the entire aggregate is rejected.

Borrowed fields retain their original lifetime constraints. Static analysis covers tracked variables, fields, aliases and control flow; it does not promise to track arbitrary raw memory or external host behavior. Runtime aggregate markers continue to protect against repeated moves and destruction.

File and process handles can be transferred. Arrays, interface values without concrete layouts and opaque host types cannot be moved directly; concrete array elements can be moved. Manually allocated source storage still needs delete after its value is moved; freeing that block does not destroy the resources now owned by the destination.

Regression tests compare raw handle identity for strings, List, Map, Set, Any and closures and check destructor counts, nested fields, return chains, discarded results, self-move assignment, loops, redeclarations, file writes, interpreter execution, AOT and MCU behavior.

## fn captures and moving closures

External locals are captured by `const&` by default; `fn const(...)` is equivalent. Reads observe later changes in the source, while writes are rejected. `fn mut(...)` captures by `mut&` and writes back to the source. `fn copy(...)` owns mutable independent copies. `fn move(...)` transfers captured values into the closure and forbids reading or writing the old bindings; declare new bindings before reuse. Copying a closure duplicates owned state and preserves references to borrowed sources. Reference captures must not outlive their sources; owned captures may escape only when their contents do not borrow shorter-lived storage. Capture lists use `fn copy[a, b] mut[c] const[d] move[e](int64 x) { ... }`; unlisted external locals use `const&` by default. One trailing default mode may override this: `fn copy[a, b] mut(int64 x)` borrows other locals by `mut&`. Multiple defaults, such as `copy[a, b] mut const(...)`, are rejected.

```magel
int64 n = 1;
auto read = int64 fn() { n };
auto write = int64 fn mut(int64 x) { n += x; return n; };
auto counter = int64 fn copy[n]() { n += 1; return n; };
n = 10;
assert(read() == 10);
assert(write(1) == 11);
assert(counter() == 2);
auto transferred = int64 fn move[n]() { n += 1; return n; };
int64 n = 20;
assert(transferred() == 12);
assert(n == 20);
```

Duplicate names, unknown variables, parameter conflicts, and mut/move captures of read-only sources are rejected. Listed variables are captured even when unused in the body. Capture applies to the whole variable. Copying or moving a reference does not own its referent or extend its lifetime. Direct global access uses global storage; explicitly list a global for a snapshot. Convert array values to containers or explicit references. File and process handles support move capture but cannot be copied, including through closure copies.

## Match range migration

Match accepts only half-open `[a,b)` and closed `[a,b]` ranges; `(a,b)` and `(a,b]` are rejected for integer, floating-point and string subjects. Integer endpoints may be adjusted for discrete values. Use guards or if/elif when floating-point or string comparisons require an exclusive lower bound. Ordinary tuple expressions keep their existing meaning.

## Integer intervals in for-in

`for (uint8 x in [0,255]) { ... }` yields 256 values, from 0 through 255; `[0,255)` excludes 255. All eight fixed-width signed and unsigned integer types are supported. Omitted variable types are inferred from the endpoints; ordinary integer literals default to int64.

Intervals are internal lazy `Iterator<T>` values using the existing for-in protocol, without allocating a list. Bounds are evaluated once each, from left to right. Both endpoints must fit T, including an exclusive upper endpoint. Constant violations are rejected at compile time; dynamic values are checked before iteration. int8 spans -128..127, so `for(int8 x in [0,255])` is rejected. Reversed intervals are errors; [a,a) is empty and [a,a] yields one value.

A closed interval finishes at its last value without incrementing the maximum integer. uint8 255 and uint64 18446744073709551615 are valid inclusive upper bounds. Changing the iteration variable does not change the iterator state. Existing break, continue, return and lambda capture rules apply.


## Value breaks

Explicit `yield i;` prefers moving writable locals inside the receiving value expression, copies external variables and adopts temporary results. Exit callbacks retain access to a valid source through conservative result copying. See [moving and cleaning up value exits](04-language-reference.md#moving-and-cleaning-up-value-exits).
