# 09 Toolchain and commands

ChaLi executes programs through the shared VM. The front end preprocesses, parses and checks source, then lowers it to quadruples and VM instructions.

```sh
chali run app.chl
chali check app.chl
chali check app.chl json
chali emit app.chl out build/app
chali svmb app.chl out build/app
```

A direct source file also runs; following arguments are passed to the program. `emit` writes `.quad`, `.data.bin` and `.layout`; `svmb` additionally writes VM bytecode. The compiler embeds the standard library and its required assets.

## Development tools

```sh
chali new demo name demo
chali test tests backend interpret
chali fmt check src
chali deps lock project . offline
chali man List
chali lsp
chali gen compiler out build/compiler
chali clear out build/compiler
```

The generator creates standalone compiler projects. Hosts integrate through bytecode, `stackvm::Machine` and native registries.

Reflection is a fixed, non-trimmable part of the language: the front end and VM always build it in and compilation always runs the full reflection pass; `--no-reflection` only decides whether this artifact carries reflection metadata and reduces no workload. Quadruples and VM execution use the highest supported optimizations by default. `--memory-size`, `--step-limit` and `--threads` configure VM resources and isolated threads.

Back-end debugging switches change diagnostics or the execution path, never program semantics. They come in two groups:

| Switch | Effect |
| --- | --- |
| `CHALI_NO_OPT=1` | Skip semantic-level quadruple simplification (`quad_optimizer`) |
| `CHALI_DUMP_OPT=1` | Print the simplified quadruples to stderr |
| `CHALI_NO_MACHINE_OPT=1` | Disable the whole machine-level layer (inlining, fold/propagate, GVN, DCE, strength reduction, LICM) |
| `CHALI_NO_OPT_INLINE` / `_STRENGTH` / `_FOLD` / `_GVN` / `_DCE` / `_LICM` | Disable one pass only — bisect when a pass changes semantics |
| `CHALI_DUMP_MACHINE=1` | Print the quadruples after machine-level optimization to stderr |
| `CHALI_DUMP_IR=1` | Print the normalized IR (basic blocks, predecessors and successors, def-use, back edges) |
| `CHALI_DUMP_FRAME=1` | Print each function's frame layout (symbol → slot) plus the allocator self-check result |
| `CHALI_NO_SLOT_REUSE=1` | Fall back to "parameters first, then one slot per declaration in order" |
| `CHALI_ONE_SLOT_PER_SYMBOL=1` | One slot per symbol but still through the allocator — separates aliasing bugs from numbering bugs |
| `CHALI_NO_REGISTER_CORE=1` | Disable the register execution core in favour of the generic stack interpreter |

All three comparisons (two cores, before/after slot reuse, before/after optimization) must agree byte for byte (text may differ only in CRLF/LF and ignorable whitespace). `tests/vm_differential.py` guards this: it runs the same source under five switch combinations (default, stack interpreter core, no slot reuse, no machine-level optimization, inlining only off) and compares exit code, stdout, diagnostics and call stack. The only relaxation is `ip` inside the call stack, which necessarily shifts when an optimization changes bytecode length.

Quadruple counts, instruction-selection coverage and frame slots before/after reuse are all visible in `--stats`:

```text
机器级优化：四元式 16810 → 16628（折叠 2，传播 4，合并 1，削减 16，外提 0，内联 6 处（+114 条），删除 296）
指令选择：目标指令 3062 条，覆盖栈指令 10492/26865（39.1%，通用回退 16373 条）
帧槽分配：362 个函数，符号 1622 个 → 槽 1346 个（省掉 276 个）
```

Project execution modes are `interpret` and `svmb`. Native output and native compiler settings are unsupported. See [projects](13-projects.md), [deployment](06-deployment.md) and the [SVMB specification](../15-svmb-format.md).

## Source modules and incremental builds

C/C++ filenames under `source/` use descriptive module names with underscores. Existing class, member and
function names are retained. Standard bindings live in `source/glue/std_xxx.{hpp,cpp}`: headers expose registration
interfaces, while platform adapters, record layouts and algorithms compile in their own `.cpp` files.
`std_binding.hpp` provides a registry forward declaration. Templates for structured data and value conversion
remain in headers where their definitions are required. The VM core continues to use generic host protocols.

Generic expansion, closures, project manifests and diagnostics compile through `chali_generic.cpp`,
`chali_lambda.cpp`, `chali_generic_closure.cpp`, `chali_package.cpp` and `chali_report.cpp`.
`macro/` independently compiles hygienic binding, structural substitution, registration, regex matching,
original fragments and editor header models. `syntax/` provides ordinary source grouping and constant
expressions; `editor/` writes real compiler analysis. AST declarations live in `chali_ast.hpp`.
The authoritative grammar assembles registration, hygienic expansion, generic closures and lowering
from `semantics/`. Language services live in `lsp/`; the compiler generator lives in `generator/`.

After grammar changes, run `sh gen.sh`, then `make -j4`. Compiler-generated `.d` files track actual include
relationships for ordinary C++ changes. `make` automatically checks standard-library, glue and manual inputs;
`tools/embed_stdlib.py` generates the assets, which compile once as `chali_stdlib_embedded.o`. Public headers
no longer include the compressed data implementation. A generation stamp tracks inputs, and unchanged output
is not rewritten. Exported compiler projects also link these implementations as separate CMake sources.

A host includes `chali_glue.hpp` and the required `std_xxx.hpp`, then links the matching `.cpp` files or host-record
library. Registration headers do not implicitly include all implementations. A complete command is provided in
the [synchronous callback example](../../example/native-callbacks/README.md).

## Independently compiled library modules

`source/simple_ast` compiles types, node construction, quadruple generation and data export separately. `source/bignum` separates integer multiplication, division, number theory, bit operations, conversion and decimal roots. Public headers retain the interfaces; Make dependency files track actual includes. Embedded assets and exported compiler projects carry all implementation units. Each library also provides a CMake target for independent builds. See the [AST library](../../source/simple_ast/README.md) and [big-number library](../../source/bignum/README.md) for file responsibilities.
