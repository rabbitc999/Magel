# Custom error propagation and if expressions

## Conditional expressions

Use `if` in initializers, assignments, arguments, returns and nested expressions. A simple branch contains one expression without a semicolon. A statement branch produces its result using `yield value;`.

```magel
int32 value;
value = if (ready) { 1 } elif (cached) { 2 } else {
    int32 fallback = 3;
    yield fallback;
};
```

Conditions are tested in order. Once a branch matches, later conditions and other branches are not evaluated. Value contexts require an else branch and a result on every normal exit. A statement such as `{ value; }` discards its value. Statement branches require an explicit `yield value;`.

Expected types flow from the surrounding initialization, assignment, parameter or return type. Unconstrained integer literals remain int64. Incompatible branches are rejected; no implicit Any boxing is introduced.

`yield value;` exits the nearest result-receiving block, if, match or loop expression, crossing intervening loops that do not receive a value. `yield (a, b);` produces one tuple. Nested expressions used in initializers, assignments, arguments or returns still receive their own value exits. Plain break and continue retain their loop meaning. Return exits the current function. Results are saved before branch cleanup; references to expired branch locals are rejected.

## Custom propagation

The automatically imported std::lang module defines TryBranch<T,R>, Try<T,R>, TryMove<T,R> and FromResidual<F,R>. The source explicitly declares its unique output T and residual R. The enclosing return type F must accept exactly R through a static fromResidual method. Static interface requirements are checked at compile time and are not dispatched through interface objects.

A named value uses the read-only branch method. Explicit `move(&value)?` uses intoBranch. Owned temporaries prefer the move protocol and may fall back to the read protocol. The source is evaluated once. Propagation performs normal cleanup and exits the current function or lambda.

Result<T,E> uses ErrorResidual<E>; Option<T> uses NoneResidual. Different success types can share an error exit without confusing errors with absence. Custom sources can produce ErrorResidual<E> to propagate into a standard Result; custom return types can accept the same residual to receive standard errors. See the [custom propagation example](../../example/samples/try_custom.mgl).

## Error adapters

- `result.mapErr<F>(mapper)` maps only Err values.
- `option.okOr<E>(error)` constructs an error for None; arguments are evaluated normally.
- `option.okOrElse<E>(factory)` calls the error factory only for None.
- `result.ok()` explicitly discards error information.

The propagation operator works in conditional branches and still exits the enclosing function. See the [conditional expressions](../../example/samples/if_expression.mgl) and [error adapters](../../example/samples/error_adapters.mgl) samples, which `sh check.sh` runs on both VM and AOT.

## Migration and tooling

The standard choose and chooseLazy functions are removed. Replace them with `if (condition) { a } else { b }`. The new expression evaluates only the selected branch; review side effects when migrating eager choose calls. User-defined functions with those names remain ordinary functions.

The LSP infers conditional and propagation results for auto hover and member completion. Hover over ? to inspect source, output, residual and the enclosing return type.

Interactive terminal output is streamed before the pause prompt. std::lang::pause flushes stdout and stderr before waiting; non-interactive input never waits for Enter. Pipeline output retains the host's buffered collection behavior.
