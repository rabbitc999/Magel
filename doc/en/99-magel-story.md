# 99 The Magel Story: A Language Carved by Deletion

> **Write once, run anywhere.**
> One logic, from MCU to cloud; value semantics, non-null references, no GC.
>
> This is not a syntax manual. It is the record of **how this language became what it is**:
> where it started, which walls it hit, what it deleted, and what is still unfinished.
> For current syntax see the [language reference](04-language-reference.md); for day-by-day
> changes see [`CHANGELOG.md`](../../CHANGELOG.md).

Seen as it stands today, Magel reads easily as "a C-flavoured static language with a VM".
But that shape was not designed in one pass — it was **arrived at by hitting walls**, and each
wall cost a batch of deletions. So in this story, everything that actually moved the language
forward is **subtraction**: deleting raw pointers, deleting null references, deleting alias
semantics, deleting temporaries nobody owned, deleting an entire delivery path, deleting the spare
spelling of the macro DSL.

> There is an earlier version of this story written with a "block world" metaphor (25 chapters);
> it was removed together with the old developer-facing archive (it remains in Git history).
> This document replaces it and keeps only what is still true today.

---

## 1. The beginning: something that looked like a course assignment

It was not called Magel at first — it was **Magic-C**. The starting point was plain: C-style
declarations, expressions, `if`, loops and functions, compiled down to **quadruples** — intuitive,
good for teaching, and convenient for comparing before and after an optimization. It looked like a
course assignment, and "C subset" was how it described itself.

Pointers were written `T mut*`. Everything was as plain and as dangerous as in C: you could declare
`int64 mut* p = &a;`, read and write through it, and do pointer arithmetic. It outgrew that sign
fairly quickly: structs gained methods, enums could carry payloads, generics could be constrained by
interfaces, lambdas became typed callbacks, and modules could be packed into `.svmb` and loaded by
another machine.

```c
int64 main() {
    println("hello, Magel");
    return 0;
}
```

That `hello, Magel` looks uneventful now, but behind it the language has changed its name once,
deleted an entire pointer system, and rewritten its value semantics. The rest of this story follows
those turns in order.

### Three nicknames, three ways of looking at it

The language has picked up a few names that help explain it. They describe the same map; they are
not a timeline of official renames:

- **C+-** — keep C's declarations, expressions and control flow (the plus), tighten null references,
  address arithmetic and fuzzy conversions (the minus).
- **cust** — C's shape with Rust-flavoured resource concerns: who owns a value, where a reference
  came from, when it is released. It names a direction, not a copy of the ownership system —
  Magel has no move semantics you are required to master.
- **cava** — you write C-style source, you can deliver `.svmb`, and StackVM runs it.

Today the only official name is **Magel**, the tool is `magel`, and the source extension is `.mgl`.

---

## 2. Wall one: pointers do not survive in a world without a GC

Pointers broke first. With no garbage collector, `T mut*` brought three problems at once: null
pointers, dangling pointers, and out-of-bounds addresses. These were not theoretical: a string
address could travel along an assignment, a return value or a function-pointer call and land in an
integer slot; `&&` and `||` lit up both sides before doing a bitwise operation; a non-`void`
function that ran off the end quietly produced zero.

The fix was not a patch at each entry point — it was **replacing the mechanism**:

- `T mut*` / `T const*` were removed outright, replaced by **`T mut&` / `T const&`**;
- the `null` keyword, compound reference types and address arithmetic were deleted; a reference must
  be initialized, and the "absent" case is written `Option<T mut&>`;
- `T &` is sugar for `T const&`; a mutable reference converts to a read-only one implicitly, the
  reverse must be explicit;
- addresses are no longer produced by arithmetic — observing one goes through `address`;
- across the language boundary, a nullable single-object C pointer is expressed as
  `Option<T mut&>` / `Option<T const&>`, which the C++ glue maps to
  `std::optional<Reference<T>>`; a null handed back by the host becomes `None`.

```c
int64 main() {
    int64 count = 1;
    int64 mut& current = &count;
    current = current + 1;
    println("count={} current={}", count, current);
    return 0;
}
```

This prints `count=2 current=2`. It looks almost exactly like the C pointer version, but the nature
has changed: **`&count` no longer "takes an address", it "binds a reference"** — there is no null
reference, no `p + 1`, no forced `*p` dereference.

The same round funnelled type checking into one common entry point: assignment, initialization,
return and call all pass through the same target-type check. `bool` and numbers require an explicit
conversion; `&&` / `||` / `!` accept only `bool` and really short-circuit; a non-`void` function has
to prove that every reachable path returns.

---

## 3. Wall two: it claimed value semantics, but had aliases

The second wall hid in the most basic place. **`string a = b;` used to be an alias** — changing `a`
changed `b`. The language said value semantics was the default, but on its most-used type it was not.

So there was an explicit breaking change: **strings became value types** — a copy really allocates
storage of its own. The same rule then spread to `std::lang::List<T>` / `Set<T>` / `Map<K,V>`,
`BigInteger` / `BigDecimal`, and to the elements of structs, enums, tuples and arrays: borrowed
values are copied, freshly constructed ones are taken over, and the old value is released before it
is overwritten. When an aggregate dies, it releases recursively by leaf, with `drop()` / `close()`
running before that recursive release.

```c
int64 main() {
    String a = "left";
    String b = a;
    b = "right";
    println("a={} b={}", a, b);
    return 0;
}
```

This prints `a=left b=right`. It reads as nothing special now; before the change, `a` would have
turned into `right` as well.

**The cost showed up immediately.** Once value semantics was actually true, somebody had to own the
temporaries: who owns the intermediate value in `t.f1().f2()`? The answer at the time was **a
prohibition** — you may not call a method directly on a temporary that owns resources; `f().g()` had
to be written as `T t = f(); t.g();`, or the compiler rejected it.

The prohibition was correct. It was also only a tourniquet.

---

## 4. Wall three: let the compiler catch every intermediate value

The prohibition saved memory and cost ergonomics: iterator pipelines, `t.f1().f2()`,
`(t.f1() + 1).f2() + 2` — all of them needed hand-written temporaries. That is not something a
language should push onto its users.

The real fix landed in the compiler, in three layers:

1. **Chained temporaries are saved by the compiler**, and at the end of the complete expression they
   are `close`d / `drop`ped in reverse creation order, released recursively and their internal slots
   cleared. The final result is saved or handed over first; branches that short-circuiting never
   built are never destructed.
2. **When a named reference borrows from a temporary**, the owning temporary is extended to the end
   of the declaration block instead of being released early; a user index (one with an
   `Index<R,K>` implementation) that returns by value also counts as "freshly constructed", which
   avoids a deep copy leaking.
3. **Soft release clears the slot immediately** (`MakeDrop`), and fixed-length arrays are registered
   so their elements are released when the block exits — those two fixed the classic failure where a
   reused address got released a second time from a stale slot.

The prohibition was then lifted. Code that used to be split apart by hand can now be chained:

```c
List<int64> make() {
    List<int64> xs;
    xs.push(1);
    xs.push(2);
    return xs;
}

int64 main() {
    println("len={}", make().len());
    return 0;
}
```

This prints `len=2` — the list produced by `make()` is caught by the compiler and collected when it
is no longer needed. With that, iterator pipelines could finally be written as pipelines:

```c
import std::iter;
int64 square(int64 value) { return value * value; }

int64 main() {
    auto picked = std::iter::range(1, 10).map(square).take(3);
    List<int64> values = picked.collect();
    println("{} {} {}", values[0], values[1], values[2]);
    return 0;
}
```

This prints `1 4 9`.

### What you borrow, you may not carry away

Non-null references answer "is there an object"; the harder question remains: **until when does the
object exist**. Returning a reference to a local, carrying a reference out of a temporary, hiding a
read-only source inside a mutable container — all of these have to be traced back to their source.

Magel added **origin tracking** (lifetimes are decided by lexical scope, while origin identity
propagates through aggregates): it tracks reference aliases and aggregates that contain
references, refuses to return the address of a local, of a by-value parameter, or of a local field,
and blocks the detours through `Option`, tuples, pattern bindings and containers. The diagnostic
code is **`MGL1204`**, and it points at a source location.

Origin tracking and independent control-flow checking now form the borrow checker: the former checks
escape, while the latter follows move, drop, delete and subsequent access, with diagnostics including
MGL1204 and MGL1205. Aliases may share an object without a global exclusive-borrow rule.
Cross-function effects, dynamic indices, recursive paths and raw host memory have explicit limits;
see [validation](../11-algorithm-validation.md) and [core concepts](03-core-concepts.md).

---

## 5. A new name, and new responsibilities: 0.7.0

On 2026-10-10 the project officially took the name **Magel** and reset the version to **0.7.0**:

- the directory became `magel/`, the tools `magel` / `magel-emit` / `magel-generator`, the source
  extension `.mgl`, and the project files `magel.toml` / `magel.lock`;
- compiler macros were unified under the `MAGEL_` prefix, while the VM kept its `STACKVM_` capability
  contract and the `.svmb` binary protocol kept its meaning;
- Chinese / English display was added: `--lang`, `MAGEL_LANG` and system locale
  detection. User strings, file paths and JSON protocols are not translated, and diagnostic codes do
  not change with language.

One thing has to be said clearly: **the 0.0.1–0.6.2 records come before 0.7.0, they are not its
successors.** The numbers were shifted once more on 2026-10-15: every version from this one onward
moved down, and the former 2.5.3 became language **0.18.0**. The mapping lives in the
[renumbering note in CHANGELOG](../../CHANGELOG.md). The current one is
[`VERSION`](../../VERSION): incompatible changes bump the major, compatible additions the minor,
fixes the patch.

The three languages also left a hole that is still open: **only syntax errors exist in all three
languages**. Type mismatches and undefined symbols are still Chinese, because those messages live in
the compiler core rather than in the diagnostic presentation layer. It is listed as a short-term item
in the [roadmap](10-roadmap.md).

---

## 6. The delivery line: from "it runs" to "it can be handed over"

Language capability is one line of progress; **the evolution of delivery forms is another**, and it
is where today's positioning comes from.

| Stage | What you hand over | Limitation |
| --- | --- | --- |
| Quadruples | a readable intermediate form | like laying the whole circuit out on the ground: it works, it does not travel |
| `.svmb` bytecode (PESVMB v4) | instructions, data, types, module exports and host symbols to bind | names are still there; the file itself is readable |
| Compacted symbols (MCVM0004) | an artifact whose names became stable FNV-1a 64 ids, ULEB128-encoded | the map file stays on your side and must not ship with the artifact |
| AOT native program | a native executable for the target platform | a different platform needs a rebuild; it is no longer the same bytecode |

Along the way **an entire path was deleted**: the embedded-VM export (generating the VM sources
together with the program and compiling them). It used to be the third leg; maintaining it cost more
than it returned — so `magel vm`, `--mode vm` and `--target cpp` were removed together, and today
only **interpretation, bytecode + StackVM, and AOT** remain.

The board side went through one real acceptance run as well: compile, trim and package on the PC,
update to an **STM32F407VET6** board, and run with a fixed-capacity VM calling the on-board HAL glue;
integer and double-precision programs finished normally and the Flash image read back identical.
That separate project has since been removed from the repository on request, and the record is still
readable from Git history — **that historical run does not mean every current language feature has
been re-verified on the board.** See [vision](00-vision.md) for the current state and boundaries.

The same principle runs through the standard library: **capability belongs to modules, not to the VM
instruction set**. `sin`, `cos` and `pow` nearly became instructions and ended up in `std::math`; the
VM keeps only the minimum needed to execute the language. The standard library now has more than
twenty modules covering math, text, data formats, concurrency, networking, SQL and more — see the
[standard library overview](07-stdlib-overview.md).

---

## 7. Wall four: do not make users write what the compiler already knows

The first three walls were all "fix it because it broke". This round was not a breakage but
**friction**: the code ran and the results were right, yet every other expression carried a
second half that the compiler already knew and still asked a human to write out.

### 7.1 Casts until your hands hurt: literals take their type from context

Integer literals used to be `int64`, always. Mix one narrow type into an expression and you had to
add the cast by hand:

```c
int32 b = a + (int32)1;     /* 1 is int64; it will not compile without the cast */
```

That line does not express intent — it supplies a fact the compiler already has: `a` is `int32`,
the result goes into an `int32`, so what else could that `1` be?

0.16.0 lets the **target type flow down into the expression**. The target of an assignment,
initialisation, argument, or `return` flows in, and binary arithmetic and bitwise operators take
their type from the other operand. The line above is now just `int32 b = a + 1;`.

```c
int32 twice(int32 value) { return value * 2; }
int64 main() {
    int32 a = 1;
    int32 b = a + 1;                 /* 1 is typed as int32 */
    int32 sum = 1 + 2;               /* both sides literal: typed by the int32 slot */
    println("{} {} {}", a, b, sum);
    return 0;
}
```

Output: `1 2 3`. Comparisons and logical operators are excluded from this typing, and with no
context a literal is still `int64`. A decimal literal that does not fit the target type falls back to
the default type and is rejected by the existing "narrowing requires a cast" diagnostic —
**it is never silently truncated** (`int8 x = 200;` still fails). Hexadecimal and binary literals
are still read as bit patterns.

### 7.2 Every operator written twice: commutativity and duality

The second piece of friction was in operator overloading. Adding `+` to your own type meant
implementing both `Add` and `RightAdd` — `a + 5` used the first, `5 + a` the second. But `+` is
commutative; `a + 5` and `5 + a` are the same operation, so why write it twice?

0.16.0 taught the compiler two inferences:

- **Commutativity**: `+` `*` `==` `!=` `&` `|` `^` are commutative, so a type that declares only
  the left interface also works on the right — the compiler swaps the operands.
  `-` `/` `%`, ordering comparisons and shifts are not commutative and still need an explicit
  right-hand interface.
- **Comparison duality**: declaring only `Less` is enough for `5 > a` (dual), `a >= 5` (negated)
  and `5 <= a` (dual + negated).

```c
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
    return 0;
}
```

Output: `15 15` and `0 0`.

Swapping borrows the right operand as the receiver, so a type can state its intent: implement
`RightAdd<R,T>` to take the right-hand side back, or the empty marker interface `NoRightAdd` to
forbid it outright (`5 + f` fails, while `f + 5` still works). **Swapping never changes evaluation
order** — both operands are still evaluated left to right as written, so side effects happen in the
same order as they would without the swap.

One operand pair may also declare several result types (`Add<int64,int64>` and `Add<Vec,int64>`
side by side, two `leftAdd(int64)` overloads differing only in return type — something ordinary
method overloading cannot express). The target type of the expression picks one; with no target
type the compiler reports the candidates.

`&&` and `||` are explicitly excluded from overloading: short-circuiting must decide the left
operand first, which does not fit the "evaluate both operands, then call a method" model. The
diagnostic now says so directly instead of falling through to the generic "cannot find `Add`"
message.

### 7.3 Subtractions in the same round: the spare macro spellings

The same round also removed two things: the `$match` macro DSL, and `##` pasting / `#param`
stringification. The reasoning is the same as above: **they were not new capabilities, just a
second spelling of something that already existed**. `$match` was a miniature DSL stacked on top of
a textual substitution macro — rarely used, but with a very large diagnostic surface. And `##` /
`#param` were compatibility aliases for `$concat` / `$stringify` and have now been removed; keeping both spellings
doubled the diagnostic surface for no gain. See the deletion list in the next section for details.

---

## 8. The deletion list

Half the shape of this language comes from what was removed:

| Removed | Replaced by | Why |
| --- | --- | --- |
| `T mut*` / address arithmetic / `null` | `T mut&` / `T const&`, `Option<T mut&>` | raw pointers do not survive without a GC |
| `switch` / `goto` / `int` / `uint` / `float` / `double` | `match`, fixed-width `int64` / `uint32` / `float64` | one public name per type |
| `using` | core `lang` visible by default, qualification omittable when unambiguous | fewer signs, and a clear name resolution order |
| `extern` / `extern cast` | `abi` blocks plus host signature declarations | make the boundary explicit, no implicit external symbols |
| `printf` / bare `putchar` / input writing into variables | `println` / `putChar` / `inputLine()` etc. returning expressions | types live in the signature, not inferred from the target variable |
| `generic<T>` prefix | postfix `T name<T>` / `struct Box<T>` | matches the call syntax; `generic` returns to being an identifier |
| `.mcpp` host description language | `abi "..."` blocks plus `--glue FILE.cpp` | maintaining another grammar for the description layer cost more than the registration code it saved |
| `IterChain` eager chaining | `std::iter` lazy adapters | ownership of the owning intermediates was unclear |
| embedded-VM export | AOT plus `.svmb` | the shortest of the three legs and the most expensive to maintain |
| `magel zip` | `tools/package_compiler.py` | the tool itself need not ship a compression implementation just to be released |
| macro `##` pasting / `#param` stringification | `$concat(a, b)` / `$stringify(x)` | a second spelling of the same thing, doubling the diagnostic surface |
| the `$match` / `$when` / `$else` macro DSL | `$for` plus overloads that key on argument count | a miniature DSL stacked on a text substitution macro: rarely used, huge diagnostic surface |

Removed things **get no privileges**: there are no dedicated migration hints, and writing one is an
ordinary error. Expecting old syntax to slip past the new rules is the same as not writing it.

---

## 9. What is still unfinished

Where the story stops is not "it will get better"; it is **what is genuinely not done today**:

- **Semantic diagnostics exist only in Chinese.** Syntax errors have all three languages; type
  mismatches and undefined symbols do not.
- **The reflection feature bit is stamped unconditionally.** If the compiler was built with
  reflection, even `hello.mgl` gets a `# magel-features: reflection` header, so the artifact demands
  a VM that also has it — and the board-side `mcu_vm` defaults to off and refuses to load it. It
  should only be stamped when the program actually uses annotations, `@Reflect` or `import std::reflect`.
- **Lifetime checking is not a complete proof.** Arbitrary cross-function writes, every alias after
  an explicit `delete`, concurrent invalidation, and references held by the host still rest on
  contracts. AOT defaults to `checked` with runtime allocation guards and panic traces, while explicit
  `unchecked` disables optional checks. Arbitrary deployment artifacts cannot be described as always having complete memory guardrails.
- **Performance has not been measured systematically.** AOT can emit a native program; do not expect
  it to beat hand-optimized C/C++.
- **Ecosystem size and IDE maturity.** The standard library covers the common capabilities but
  cannot compare with a mainstream language's third-party libraries; the VS Code extension already
  does highlighting, diagnostics, completion, navigation and breakpoint debugging, but the toolchain
  is still under construction.

Each of these is listed in the [roadmap](10-roadmap.md) and in
[who should not use Magel](00-vision.md#4-who-should-not-use-magel).
**Stating the boundary is what lets the next person know where to start repairing.**

---

## 10. Where to read next

| You want to know | Go to |
| --- | --- |
| Why it exists and whether it fits you | [00 Vision](00-vision.md) |
| Get it running first | [01 Quickstart](01-quickstart.md) |
| What the language looks like | [02 Language tour](02-language-tour.md) |
| Details of value semantics / non-null references / no GC | [03 Core concepts](03-core-concepts.md) |
| Authoritative syntax and semantics | [04 Language reference](04-language-reference.md) |
| How to hand a program over | [05 Delivery security](05-delivery-security.md) → [06 Cross-platform deployment](06-deployment.md) |
| What comes next | [10 Roadmap](10-roadmap.md) |
| Every individual change | [`CHANGELOG.md`](../../CHANGELOG.md) |

> Every snippet in this story is executed by `tmp/story_check.sh` (`sh tmp/story_check.sh`), and the
> Chinese edition uses the same snippets.
