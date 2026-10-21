# Compiler safety boundaries and focused regression tests

> Handoff status: the changes below are in source, but parser regeneration, compiler rebuilding and full acceptance remain incomplete. See [TASK.md](../../TASK.md) for evidence and pending work.

This page records protection for confirmed defects. It does not claim that the compiler has no other defects. Further random exploration is paused at the user's request.

Integer data export parses 64-bit integers directly, preserving values beyond double precision, the signed minimum and the unsigned maximum. Floating data export checks the integer conversion range. BigFloat checks exponent multiplication before evaluating it and rejects precision values that overflow internal arithmetic.

When an owning `?` result becomes an enum constructor argument, the constructor takes its temporary ownership and clears the original slot. Direct source execution loads its generated data from an in-memory snapshot, so another process overwriting the same output prefix cannot replace the running program's data. Exported files should still use distinct prefixes.

Platform fixes avoid duplicate long/int64 constructors on LP64, delay JSON/TOML member template definitions until value types are complete, and avoid a glibc htons macro collision. The standalone VM CMake target declares its CAS, SQL generation, SQLite, compression, include and host dependencies.

Run `python tests/magel_compiler_safety.py`. On Linux, `--cxx clang++ --sanitize-native` enables ASan, UBSan and float-to-integer checks for native boundaries; `--native-only` runs only that portion. Reflection remains enabled in both frontend and runtime, with default maximum VM and quadruple optimization.

The tests cover repeated success/error/container propagation and overwrite disk data while optimization output is blocked. See [the native boundary test](../../tests/magel_native_safety.cpp). Large-number operations remain subject to host memory and time budgets. Delivery reports distinguish completed checks from deferred platform and long-running validation.
