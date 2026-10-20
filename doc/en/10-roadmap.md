# 10 Roadmap and ecosystem

Read [current capabilities](17-capabilities.md), [`VERSION`](../../VERSION) and
[`CHANGELOG.md`](../../CHANGELOG.md) for implementation and release history. This page describes
future work without presenting plans as implemented features.

## Existing foundation

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
