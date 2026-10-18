# 10 Roadmap and ecosystem

Read [current capabilities](17-capabilities.md), [`VERSION`](../../VERSION) and
[`CHANGELOG.md`](../../CHANGELOG.md) for implementation and release history. This page describes
future work without presenting plans as implemented features.

## Existing foundation

Static types, value semantics, non-null references, independent borrow/control-flow checking, generics,
interfaces and closures are implemented. Source execution, shared VM bytecode and C++-based AOT work,
with runnable C/C++ integration examples. The [API index](../library/README.md) defines library coverage.
The H743 port uses the shared VM and has a standalone project and hardware records. Native LSP, DAP,
tests, formatting, online dependency locking and offline caches exist. The root toolchain includes reflection by default.

## Priorities

| Direction | Current boundary and next work |
| --- | --- |
| Diagnostics | Continue source positions, diagnostic codes and localization; do not promise uniform translation of every error |
| Documentation | Derive defaults and capabilities from implementation; label historical measurements by the actual tested version |
| Debugging | Add DAP variables, scopes and expression evaluation while retaining breakpoint, stepping and stack regressions |
| Reflection deployment | Current default artifacts require reflection; investigate demand-driven flags and metadata trimming with explicit compatibility rules |
| Distribution/platforms | Keep compiler, extension, artifact versions, dependencies and target builds aligned |
| Robustness | Expand long-running, exhaustion, invalid-host, concurrency and board-budget validation |

## Continuing work

- Dependencies already support hashed sources, fixed Git commits, transitive locks and offline caches; improve distribution, portability and the package ecosystem.
- AOT uses C++ compilers; improve generated code, build times and module caches.
- Verify correctness and performance under the highest default VM/IR optimizations; retain reproducible historical benchmarks.
- Extend libraries and host adapters from actual project needs, describing HTTP/TLS, SQL drivers and GUI backends separately.
- Keep one shared VM core and distinguish front-end embedded gates, MCU build macros and board glue.

## Principles

No GC, non-null references and deterministic owned-value cleanup remain the contract; Option expresses
optional references. Borrow checking follows the documented alias contract and does not cover arbitrary
host memory behavior. Symbol compression does not prevent reverse engineering. Confirmed defects are
not preserved merely for compatibility. See the [semantic contract](../21-semantics-frozen.md).

## Contributing and validation

Reports should include a minimal program, command, platform, configuration and output. Change grammar
and semantics at their authoritative sources. Keep Chinese, English, book sources and generated output
aligned. Local gates support development; release evidence states the scope actually executed.
See the [toolchain](09-toolchain.md) and [`AGENTS.md`](../../AGENTS.md).
