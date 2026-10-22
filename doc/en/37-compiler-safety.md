# Compiler safety boundaries and focused regression tests

> Status: the changes below are in source, and parser regeneration plus compiler rebuilding are done; both the focused regression and the penetration suite pass locally. No penetration case is classified UB / CRASH / TIMEOUT any more — only diagnosable failures and normal output remain.

This page records protection for confirmed defects. It does not claim that the compiler has no other defects.

| Boundary | Behaviour |
| --- | --- |
| Integer data export beyond double precision | Parsed as 64-bit directly, preserving the signed minimum and unsigned maximum |
| Large floating values exported to the data area | Integer conversion range is checked first |
| BigFloat power and root internals | Exponent range checked before multiplication; precision growth avoids signed overflow |
| Owning `?` result as an enum constructor argument | The constructor takes the temporary and clears the original owning slot |
| Concurrent writers on one output prefix | Direct source execution loads its generated data from an in-memory snapshot |
| Float-to-integer conversion out of range | Runtime reports "float to integer out of range"; constant folding refuses out-of-range values |
| Decimal integer literal beyond int64 | Compile-time diagnostic (previously saturated silently to `INT64_MAX`); `-9223372036854775808` is the exception, where the unary minus types the 2^63 magnitude as the minimum |
| Floating literal overflowing to inf / NaN | Compile-time diagnostic (previously produced "undefined variable: inf.0") |
| JSON / TOML nesting depth | Beyond 256 levels is a parse diagnostic (previously overflowed the host stack) |
| Local fixed-size arrays | Compile-time "fixed array exceeds the current memory budget" (previously `std::bad_alloc` escaped as `terminate`), matching struct fields |
| Fixed-array dimension product | Accumulated in 64 bits and clamped at the int bound (previously `int` overflow produced wrong loop bounds and slot offsets) |
| Negative string slice endpoints | Runtime diagnostic (previously `"abcdef"[0:-2]` silently returned `abcdef`); `end == -1` remains the built-in ABI's "to the end" convention |
| Re-entrant container callbacks | `vecFold` / `vecMap` / `vecFilter` and predicate callbacks lease the iterator; `push` / `clear` inside them is rejected |
| Mutation during iteration | `push` / `set` share the "cannot move or remove list elements while a for-in reference is active" guard with `remove` |
| `INT64_MIN / -1` | Runtime overflow diagnostic (previously returned `-9223372036854775808`) |
| Shift amount beyond width | Runtime diagnostic; the valid range is 0..63 (previously masked with `& 63`, so `1 << 64` yielded `1`) |

Platform fixes avoid duplicate long/int64 constructors on LP64, delay JSON/TOML member template definitions until value types are complete, and avoid a glibc htons macro collision. The standalone VM CMake target declares its CAS, SQL generation, SQLite, compression, include and host dependencies.

Run `python tests/chali_compiler_safety.py`. On Linux, `--cxx clang++ --sanitize-native` enables ASan, UBSan and float-to-integer checks for native boundaries; `--native-only` runs only that portion. Reflection remains enabled in both frontend and runtime, with default maximum VM and quadruple optimization.

Penetration testing: `python tests/chali_penetration.py` (`--only SUBSTRING` narrows it down). It attacks numeric conversion, literals, container boundaries, ownership propagation and size budgets, then classifies each case as UB / CRASH / TIMEOUT / FAIL / OUTPUT — it collects facts only and leaves the verdict to the reader.

The tests cover repeated success/error/container propagation and overwrite disk data while optimization output is blocked. See [the native boundary test](../../tests/chali_native_safety.cpp). Large-number operations remain subject to host memory and time budgets. Delivery reports distinguish completed checks from deferred platform and long-running validation.
