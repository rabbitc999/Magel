# Return expressions, named closures and field derivation

[Documentation index](README.md) · [中文](../28-declaration-sugars.md)

The authoritative productions are in [`source/chali.gram`](../../source/chali.gram). ParseEngine generates the parser; ChaLi semantic functions construct and lower the AST. IR here means quadruples.

## Returning a control expression

```chali
String make() {
    return {
        String local = "owned";
        yield local;
    };
}
int32 choose(bool yes) { return if (yes) { 1 } else { 2 }; }
int64 count() { return loop { yield 3; }; }
```

Blocks, loops, if expressions and match expressions reuse value exits. Every normally completing path must produce a value, and branch result types must match exactly. Branches do not unify through numeric widening or user conversions. Integer literals still receive the enclosing function's return type as context.

```text
stmt := RETURN expr SEMI { M_ReturnValue(expr) }

Return(ControlExpr E)
  => Block(Declare hidden : FunctionResult = E, Return(hidden))
```

The AST owns a dedicated hidden result declaration, marked together with its control expression by `returnExpression`. `LowerLocalDecl` supplies the function's return type; `LoopValueType` checks precise branch types. `LowerLoop` creates the expression's join slot. Evaluation happens once, before scope cleanup and ordinary function return.

Nested return, `break level N`, `break all`, and early-returning `?` propagation are rejected within the returned control expression. Use explicit matching and value breaks. Validation stops at a nested `fn` or named function boundary, so independent callbacks retain their own return statements.


Generic functions follow the same path: `T take<T>(T value) { return { T local = value; yield local; }; }`. Loop, if and match can also return T. Monomorphization substitutes the concrete type before result-slot creation, branch checking and move/copy lowering. Generics do not bypass identical branch types, borrowing or cleanup checks. The regression covers integers, String and a struct owning String.

## Ownership and compile-time state

The implementation retains value copying for ordinary assignment and introduces no GC. Move selection uses the actual result receiver, rather than the nearest brace.

| Information | Implementation | Purpose |
| --- | --- | --- |
| Storage identity and projections | `MVar.emitted`, `Place.root/path` | Distinguish shadowed names, fields, indices and aliases |
| Declaration scope | `g_scope`, `MBlockContext.valueScopeDepth` | Classify internal owners and external sources |
| Owners and cleanup obligations | `g_ownedScopes` | Preserve Drop, Closeable, field and deferred cleanup order |
| Borrow source sets | Bind/Allocate flow nodes and `Fact` | Reject escaping references in results and closures |
| Invalid storage | `FlowState.invalid`: Moved/Freed/Dropped | Detect reads, writes and alias access after consumption |
| Branch and loop joins | `checkFlow` fixed-point analysis | Reject accesses invalid on any incoming path |

`PrepareBreakValue` handles tuple elements independently. Direct owned variables declared inside the receiving expression are consumed; outside variables are copied, and fresh temporaries are taken over. A compiler-only exit marker also permits consuming an internal immutable owner at the end of its lifetime. Source-level `move` still requires `T mut&`. References do not own their targets and cannot consume external storage. Returned references remain subject to source-lifetime checks.

```text
lowerBreak(value, receiver):
    for each tuple element or value:
        if direct owner is declared inside receiver:
            evaluate once; transfer into receiver.joinSlot
            mark source Moved; clear source without Drop
        else:
            copy into receiver.joinSlot
        verify borrowed sources outlive receiver
    clean scopes above receiver in reverse order
    jump receiver.exit

lowerReturnControl(E):
    hidden = evaluate E once through its joinSlot
    transfer hidden into the ordinary return slot
    clean remaining scopes while retaining the returned owner
    return the ordinary slot
```

Transfer/copy are compiler pseudocode, not new VM opcodes. Existing `LowerLanguageMove` performs live-memory checks, storage reads, source clearing and aggregate move marking. Existing string, object and aggregate paths perform copies. Cleanup remains on its control-flow edges and is protected by move state; consuming on one branch must not globally erase another branch's cleanup. Deferred callbacks cannot access an already consumed source through a dangling alias.

See [`M_ReturnValue`](../../source/semantics/return_expression_ast.cpp), [`PrepareBreakValue` and `LowerLanguageMove`](../../source/semantics/boxing.cpp), [`LowerLoop`](../../source/semantics/lowering.cpp), and [borrow dataflow](../../source/chali_borrow_flow.hpp).

Runtime arrays follow the same rule. An internal array in `return { String values[] = new[2]; ... yield values; };` transfers its length-bearing allocation; the source handle is cleared and source element cleanup is skipped. An external runtime array receives a new allocation and per-element deep copies of strings, objects and aggregates. Hidden result bindings accept length-bearing block, if, match and loop expressions as well as allocations and calls. This compiler-only consumption does not enable source-level `move array`; fixed arrays retain their existing storage/return constraints, and opaque host types are not assumed to own resources.

## Statement-level named functions

```chali
void main() {
    int64 value = 2;
    int64 read(int64 amount) { return value + amount; }
    int64 change(int64 amount) mut { value += amount; return value; }
    int64 snapshot() copy { return value; }
    int64 owned() move { return value; }
    int64 later(int64 amount);
    later = int64 fn(int64 amount) { return amount * 2; };
}
```

Local definitions appear only in `decl_stmt`, never in expression productions. Consequently, `a + int64 f() { ... }` is a syntax error.

```text
decl_stmt := type name '(' param_list ')' block
           | type name '(' param_list ')' MUT block
           | type name '(' param_list ')' COPY block
           | type name '(' param_list ')' MOVE block
           | type name '(' param_list ')' ';'

NamedFunction(T, name, params, body, capture)
  => Declare name : FnType(T, params) = Lambda(T, params, body, capture)
```

The explicit return type anchors checking. Default capture is const&; `mut` borrows writable state, `copy` creates private state, and `move` transfers ownership. The newly constructed closure is taken over by its binding. Subsequent ordinary function-value assignments retain copy semantics. A local bodyless declaration is a function-value variable for later assignment; a top-level bodyless declaration retains its existing host-function meaning.

Named closures reuse `fn` ownership and lifetime analysis. Returning or storing a closure that still borrows a local does not extend that local's lifetime and must fail compilation. The AST entry point is [`M_NamedFunction`](../../source/semantics/named_function_ast.cpp); there is no separate closure runtime.

## Field derivation

```chali
#derive(Equal, Hash, Clone)
struct Point { int64 x; String label; }
#derive(Equal, Hash, Clone)
enum Choice { None, Value(Point point) }
```

```text
derive_directive := DERIVE '(' iface_list ')'
top_decl := derive_directive struct_decl type_decl_end
          | derive_directive enum_decl type_decl_end
DerivedDecl(traits, declaration) => declaration.derives = traits
```

`#derive` is an individually registered compiler directive token. It attaches only to structs and enums. Only Equal, Hash and Clone are accepted. Drop, Closeable, unknown traits, duplicate requests and overwriting existing methods are rejected.

The semantic pass reads parsed field or variant-payload metadata and passes raw names or generated field fragments through existing `$macro`, `$for` and `$concat` templates. The macro engine receives text, performs no type checking and calls no Parser. After expansion finishes, the derive semantic pass uses the same generated parser to read ordinary generated methods, attach them to the original AST, and instantiate ordinary `Equal<bool,T>`, `Hash<T>` and `Clone<T>` interfaces. Normal type, interface, borrow and quadruple lowering then applies.

Equal compares fields with short circuiting and checks enum variants before payloads. Hash combines variant tags and field hashes; strings use UTF-8 bytes and floating zero is normalized so equal values hash equally. Clone copies built-in values and invokes Clone for nested nominal fields; enum cloning visits only the active variant. Generated methods perform normal operations without a derive interpreter, runtime field enumeration or reflection lookup.

Field protocols are validated after all interfaces are registered, including for unused types. Scalars and String have built-in semantics. A nested custom struct or enum must implement the corresponding interface, either manually or through its own derive request. References, function values, raw arrays and other fields without a matching semantic protocol are not silently compared by address or raw bytes; provide a suitable wrapper and explicit interfaces.

The core framework is [`derive.cpp`](../../source/semantics/derive.cpp):

```text
ExpandDerivedDeclarations():
    for each pending type:
        enforce the Equal / Hash / Clone allowlist
        source = expand existing macros over field metadata
        generated = generatedParser.parse(source)
        attach generated methods and concrete interfaces
RegisterTop()
ValidateDerivedFields()  // missing nested protocols always fail
LowerProgram()          // ordinary typed methods -> ordinary quadruples
```

## Closeable and operator derivation switches

Two conventions live in compiler directives: a struct that implements `Closeable` is
automatically `#NonCopyable`, and `#No<Interface>Replace` before a declaration removes that
interface's automatic derivation.

```chali
struct Handle : Closeable { int64 id; void close() { println("close {}", self.id); } }

#NoAddReplace
struct Chain : Add<int64, int64> {
    int64 v;
    int64 leftAdd(int64 other) { return self.v + other; }
}
```

```text
switch_directives := NO_REPLACE | NO_RIGHT | switch_directives NO_REPLACE | switch_directives NO_RIGHT
top_decl := switch_directives struct_decl type_decl_end
NO_REPLACE := "#No" [A-Za-z][A-Za-z0-9]* "Replace"
NO_RIGHT   := "#NoRight" [A-Za-z][A-Za-z0-9]*
```

`#No<Interface>Replace` fills `noReplace`, `#NoRight<Interface>` fills `noRight`. The semantic
layer checks that the interface exists, is binary, and really has derivation to cancel
(swap / duality / negation for the first, a right-hand form for the second); a typo, a
duplicate or an interface without derivation is an error. `FindOperator` consults them:

```text
FindOperator(op, left, right):
    direct match               -> use it (directives never affect direct calls)
    right-direction (RightXxx) -> unchanged
    symmetry swap              -> only if the receiver has no #No<Interface>Replace
    duality / negation         -> only if the via interface has no #No<Interface>Replace
```

`#NoRightXxx` therefore closes only the right-hand direction, while `#NoAddReplace` closes all
derivation for that interface; both are compiler directives placed before the declaration. Generic templates keep their directives during
`collectGenericDerives` and paste them back in front of every instantiation.

## Regressions

See [return ownership](../../example/samples/return_expression.chl), [named closures](../../example/samples/named_function.chl), [struct and enum derivation](../../example/samples/derive.chl), and the [semantic/lifetime gate](../../tests/chali_declaration_sugars.py).
