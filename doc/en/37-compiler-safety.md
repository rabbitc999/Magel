# Compiler safety boundaries and focused regression tests

> Handoff status: the changes below are in source, and parser regeneration plus compiler rebuilding are done; the focused regression passes locally. Penetration testing continues; see [TASK.md](../../TASK.md) for pending items.

This page records protection for confirmed defects. It does not claim that the compiler has no other defects.

| Boundary | Behaviour |
| --- | --- |
| Integer data export beyond double precision | Parsed as 64-bit directly, preserving the signed minimum and unsigned maximum |
| Large floating values exported to the data area | Integer conversion range is checked first |
| BigFloat power and root internals | Exponent range checked before multiplication; precision growth avoids signed overflow |
| Owning `?` result as an enum constructor argument | The constructor takes the temporary and clears the original owning slot |
| Concurrent writers on one output prefix | Direct source execution loads its generated data from an in-memory snapshot |
| Float-to-integer conversion out of range | Runtime reports "float to integer out of range"; constant folding refuses out-of-range values |
| Decimal integer literal beyond int64 | Compile-time diagnostic (previously saturated silently to `INT64_MAX`) |
| Floating literal overflowing to inf / NaN | Compile-time diagnostic (previously produced "undefined variable: inf.0") |
| JSON / TOML nesting depth | Beyond 256 levels is a parse diagnostic (previously overflowed the host stack) |

Platform fixes avoid duplicate long/int64 constructors on LP64, delay JSON/TOML member template definitions until value types are complete, and avoid a glibc htons macro collision. The standalone VM CMake target declares its CAS, SQL generation, SQLite, compression, include and host dependencies.

Run `python tests/magel_compiler_safety.py`. On Linux, `--cxx clang++ --sanitize-native` enables ASan, UBSan and float-to-integer checks for native boundaries; `--native-only` runs only that portion. Reflection remains enabled in both frontend and runtime, with default maximum VM and quadruple optimization.

Penetration testing: `python tests/magel_penetration.py` (`--only SUBSTRING` narrows it down). It attacks numeric conversion, literals, container boundaries, ownership propagation and size budgets, then classifies each case as UB / CRASH / TIMEOUT / FAIL / OUTPUT — it collects facts only and leaves the verdict to the reader.

The tests cover repeated success/error/container propagation and overwrite disk data while optimization output is blocked. See [the native boundary test](../../tests/magel_native_safety.cpp). Large-number operations remain subject to host memory and time budgets. Delivery reports distinguish completed checks from deferred platform and long-running validation.
