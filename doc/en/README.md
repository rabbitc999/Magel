# Magel Documentation

See [current capabilities and limits](17-capabilities.md) for the shared implementation contract and build defaults.

**Write once, run anywhere.** One logic, from MCU to cloud; value semantics, non-null references, no GC.

Magel turns business logic into an asset you can deliver on its own: write it once and run the same
logic from desktop down to the embedded board; what you hand over is bytecode or a native program,
not source; there is no garbage collector, and resources are released deterministically when they
leave scope.

Read the current language/toolchain version from [`VERSION`](../../VERSION).

For AI-assisted Magel programming, use the repository's [Magel programming skill](../../skills/magel-programming/SKILL.md), covering syntax choices, API lookup, macros and execution checks.
Changes: [`CHANGELOG.md`](../../CHANGELOG.md).

> This is the English edition. The Chinese edition is the default and is the one maintained first —
> see [中文文档](../README.md). Code snippets are identical in both editions and are executed before
> they are published.

---

## What do you want to do?

| Who you are | Start here |
| --- | --- |
| New to Magel | [Vision](00-vision.md) → [5-minute quickstart](01-quickstart.md) |
| Evaluating it for a project | [Vision](00-vision.md) (includes "who should not use it") → [Cross-platform deployment](06-deployment.md) |
| Ready to write code | [Language tour](02-language-tour.md) → [Language reference](04-language-reference.md) |
| Want to understand value semantics and references | [Core concepts](03-core-concepts.md) |
| Handing the program to a customer / flashing a device | [Delivery security](05-delivery-security.md) → [Cross-platform deployment](06-deployment.md) |
| Want to know what the standard library has | [Standard library overview](07-stdlib-overview.md) |
| Looking for something to run right away | [Example projects](13-projects.md) (GUI, web, gradebook, board-side VM…) |
| Need to talk to a C/C++ host | [Host integration and C ABI](08-host-integration.md) |
| Looking up commands, config, tests | [Toolchain](09-toolchain.md) |
| Querying APIs in a terminal or checking editor caches | [Command line manual](../man/en/README.md) → [LSP incremental hints and caching](../man/en/lsp.md) |
| Looking for the authoritative grammar | [EBNF grammar reference](../14-grammar-ebnf.md) |
| Wondering how this language came to be | [99 The Magel story](99-magel-story.md) |
| Comparing VM execution performance | [VM optimization and Lua comparison](12-vm-performance.md) |
| Want to contribute / see the roadmap | [Roadmap and ecosystem](10-roadmap.md) |

---

## Document structure

| No. | Document | Audience | What it covers |
| --- | --- | --- | --- |
| [00](00-vision.md) | Vision | Decision makers, prospective users | The three dilemmas of software delivery, Magel's answers, who it fits / who it does not |
| [01](01-quickstart.md) | 5-minute quickstart | First-time users | Install, first program, command cheat-sheet |
| [02](02-language-tour.md) | Language tour | Experienced developers | Walk through what Magel looks like with runnable snippets |
| [03](03-core-concepts.md) | Core concepts | Users going deeper | Value semantics, non-null references, resource management without a GC |
| [04](04-language-reference.md) | Language reference | Day-to-day writers | Syntax, semantics, examples and common errors by topic, including generic nesting, default types and monomorphisation limits |
| [05](05-delivery-security.md) | Delivery security | Teams shipping outward | Delivery forms, the four steps, reverse-engineering cost, boundary statement |
| [06](06-deployment.md) | Cross-platform deployment | People doing deployment | Three execution modes, desktop / embedded / server, memory and watchdog |
| [07](07-stdlib-overview.md) | Standard library overview | Anyone asking "what can it do" | Existing modules and core APIs by capability |
| [08](08-host-integration.md) | Host integration and C ABI | Integrators | Glue bindings, `abi` blocks, type mapping, nullable references |
| [09](09-toolchain.md) | Toolchain | Daily `magel` users | Commands, `magel.toml`, compile options, tests, LSP, display language |
| [10](10-roadmap.md) | Roadmap and ecosystem | Contributors, long-term users | Short/mid/long-term goals and verifiable milestones |
| [13](13-projects.md) | Example projects | Anyone who wants to run something | Application and capability examples in the repository: location, commands, verification and prerequisites |
| [14](../14-grammar-ebnf.md) | EBNF grammar reference | Anyone checking grammar details | Every production and token declaration generated from `source/magel.gram`; the readable form of the authoritative grammar |
| [15](../15-svmb-format.md) | `.svmb` binary format | Format analysts and RE assessors | Container and section layout, byte-level decoding, reverse-engineering cost |
| [16](../16-compact-symbols.md) | Nameless compact symbols | Anyone tracking size and diagnostic privacy | `--compress-symbols` format, tooling and space comparison |
| [17](17-capabilities.md) | Current capabilities | Evaluation and integration | Language, VM/AOT, borrowing, reflection, libraries, MCU and tooling contracts |
| [18](../18-panic-trace.md) | Panic and source lines | Anyone debugging crashes | Failure output, call stack, source location, compact symbol-hiding policy |
| [19](../19-reflection.md) | Annotations and reflection | Anyone using annotations / dynamic calls | Declaration metadata, typed queries, closed-value dynamic invocation |
| [20](../20-memory-allocator.md) | Memory allocator | Anyone wiring host memory | Pool and capacity stats, allocation range checks, external buffers |
| [22](22-tombstone-research.md) | Tombstone implementation research | Runtime contributors | Original storage, allocation/release paths, sweep and restart boundaries, lambda borrowing research |
| [99](99-magel-story.md) | The Magel story | Anyone curious how it got here | From Magic-C to Magel: which walls it hit, what it deleted, what is still unfinished |

## More entry points

- Multiple return values and generic tuples: [language reference §4](04-language-reference.md#multiple-return-values-and-generic-tuples), [interpreter / AOT regression](../../tests/magel_tuple_returns.py).

## Companion resources

- [`example/samples/`](../../example/samples/) — runnable formal samples; expected stdout in [`example/expected/`](../../example/expected/).
- [`example/samples-error/`](../../example/samples-error/) — samples expected to fail; diagnostics in [`example/expected-error/`](../../example/expected-error/).
- [`doc/examples/`](../examples/) — examples that need a companion project layout (ABI, SQLite, svmb, GUI).
- The companion projects under [`doc/examples/`](../examples/) are catalogued in [13 Example projects](13-projects.md).
- [Project README](../../README.en.md) — build, quick usage, debugging, configuration (English;
  [中文版](../../README.md)).
- [Full grammar reference (EBNF)](../14-grammar-ebnf.md) — the authoritative form of the grammar, generated from [`source/magel.gram`](../../source/magel.gram).

- [11 Borrow control flow and algorithm validation](11-algorithm-validation.md): independent dataflow checks, heap structures, DFS/BFS and NOIP2015 brute-force comparisons.

- [Any, explicit generics and data utilities](22-any-generics-data.md)

- [Ownership transfer](23-ownership-move.md)

- [Context-typed literals, operator inference and macros](24-literals-operators-macros.md)

- [Execution and delivery](25-execution-delivery.md)

- [Custom error propagation and if expressions](26-error-propagation-if.md)

- [Matching, interpolation, generics and scope cleanup](27-match-interpolation-scope.md)

- [Return expressions, named closures and field derivation](28-declaration-sugars.md)
- Value breaks and loop traversal: [language reference](04-language-reference.md#block-expressions-and-interpolated-output), [complete sample](../../example/samples/break_values.mgl), [VM / AOT regressions](../../tests/magel_block_expression.py).

- Implicit moves in value breaks: [semantics and cleanup order](04-language-reference.md#moving-and-cleaning-up-value-breaks), [resource identity and destructor sample](../../example/samples/break_move.mgl).
