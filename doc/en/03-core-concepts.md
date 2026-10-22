# 03 Core concepts

ChaLi combines value semantics, non-null references and deterministic cleanup with independent
provenance/control-flow checking. See [current capabilities](17-capabilities.md) and the
[language reference](04-language-reference.md).

## 1. Value semantics and explicit move

```chali
struct Point { int64 x; int64 y; };
void shift(Point p) { p.x = p.x + 10; }
void main() {
    Point a;
    a.x = 1;
    a.y = 2;
    Point b = a;
    b.x = 99;
    shift(a);
    println("a=({}, {}) b=({}, {})", a.x, a.y, b.x, b.y);

}
```

```text
a=(1, 2) b=(99, 2)
```

Ordinary owned-value assignment and value parameters copy. Strings, containers and owned fields
follow value semantics, potentially requiring allocation and traversal. Borrow to avoid a copy or
move to transfer resources. Copying references still refers to the same source; reference fields do
not clone their targets. Host resources retain their own copyability contracts.

`move(T mut&)` transfers existing handles and owned fields without calling source close/drop.
The old binding cannot be read, written or rebound. A complete move on every incoming path permits
a new declaration using the same name, with a new storage identity. See [ownership transfer](23-ownership-move.md).

## 2. Non-null and optional references

```chali
T mut&     // 可变引用
T const&   // 只读引用
T &        // 与 T const& 等价
```

A reference binds a valid source. To represent an optional reference, use `Option<T const&>` or
`Option<T mut&>`: Value contains a valid reference; None contains no reference. This does not add
a nullable reference to the language.

```chali
int64 value = 7;
int64 mut& alias = &value;
alias = 9;
Option<int64 const&> optional = Option<int64 const&>::Value(&value);
```

Mutability and source lifetime must both be valid. Returning the address of a local, value parameter
or short-lived temporary, or storing a reference beyond its source, is rejected. Provenance propagates
through aliases, Option/Result, tuples, structs and containers. Escape diagnostics include CHL1204 and
the original file position.

## 3. Borrow provenance and control-flow checking

The independent checker models storage paths, merges branch/loop state and tracks move, drop and drop.
Access to moved or released sources and repeated moves follows the corresponding rules; CHL1205 is a
common diagnostic. Redeclaration after a complete move creates a new identity and does not redirect old aliases.

Multiple aliases may share an object, and writes through a mutable alias are observable through other
valid aliases. There is no global exclusive-borrow rule and no guarantee for arbitrary concurrent host
access. Checking covers modeled storage, aliases and control flow; dynamic indices, recursive structures,
cross-function side effects and raw host memory have explicit limits. See
[borrow and algorithm validation](../11-algorithm-validation.md).

## 4. No GC and deterministic cleanup

Owned values recursively clean up at lifetime end. Closeable closes a resource; Drop destroys it.
Implementing both gives close before drop. A struct implementing `Closeable` is not copyable
(a copy means two owners and two `close()` calls): transfer with `move(&x)` or share through a
reference. Explicit finalization avoids duplicate cleanup according to
lifecycle state; it does not restore an invalid value. Defer and scope(exit) register scope-exit actions.

```chali
struct Resource : Drop {
    String name;
    void drop() { println("drop {}", self.name); }
};
void main() {
    {
        Resource r;
        r.name = "conn";
        println("using");
    }
    println("after block");

}
```

```text
using
drop conn
after block
```

`T mut& p = heap T;` creates manually allocated storage that must be deleted as specified; ending the
reference's scope is not a promise to release its target. Files, processes, databases and other host
handles follow their Drop/Closeable contracts. See [validation](../11-algorithm-validation.md) for
non-idempotent destruction, allocation identities and automatic cleanup.

## 5. Memory and execution limits

## 6. Choosing a transfer mode

| Need | Mechanism |
| --- | --- |
| Independent owned value | Ordinary assignment/value argument under the actual type's copy contract |
| Read-only access without a copy | `T const&` or `T &`, keeping the source alive |
| Modify the source | `T mut&`, respecting permission and lifetime |
| Transfer resources | `move(&value)`; the old binding is unusable, with redeclaration after a complete move |
| Optional value or reference | `Option<T>` or `Option<T &>` |
| Recoverable application error | `Result<T,E>` and explicit propagation protocols |

## 7. Common diagnostics

| Diagnostic | Meaning and action |
| --- | --- |
| CHL1204 | Reference outlives its source; return an owned value or borrow caller-owned storage |
| CHL1205 | Access after move, destruction or release; correct operation order and binding identity |
| Type/overload mismatch | Check values/references, permissions, numeric range and signatures; casts do not bypass resource contracts |

## 8. Next

[Language reference](04-language-reference.md) · [Move](23-ownership-move.md) ·
[Samples](../../example/samples/) · [Deployment](06-deployment.md)
