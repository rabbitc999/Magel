# 09 Toolchain and commands

Magel executes programs through the shared VM. The front end preprocesses, parses and checks source, then lowers it to quadruples and VM instructions.

```sh
magel run app.mgl
magel check app.mgl
magel check app.mgl json
magel emit app.mgl out build/app
magel svmb app.mgl out build/app
```

A direct source file also runs; following arguments are passed to the program. `emit` writes `.quad`, `.data.bin` and `.layout`; `svmb` additionally writes VM bytecode. The compiler embeds the standard library and its required assets.

## Development tools

```sh
magel new demo name demo
magel test tests backend interpret
magel fmt check src
magel deps lock project . offline
magel man List
magel lsp
magel gen compiler out build/compiler
magel clear out build/compiler
```

The generator creates standalone compiler projects. Hosts integrate through bytecode, `stackvm::Machine` and native registries.

Front-end and VM reflection are enabled together by default; `--no-reflection` disables it for one compilation. Quadruples and VM execution use the highest supported optimizations by default. `--memory-size`, `--step-limit` and `--threads` configure VM resources and isolated threads.

Project execution modes are `interpret` and `svmb`. Native output and native compiler settings are unsupported. See [projects](13-projects.md), [deployment](06-deployment.md) and the [SVMB specification](../15-svmb-format.md).

## Source modules and incremental builds

C/C++ filenames under `source/` use descriptive module names with underscores. Existing class, member and
function names are retained. Standard bindings live in `source/glue/std_xxx.{hpp,cpp}`: headers expose registration
interfaces, while platform adapters, record layouts and algorithms compile in their own `.cpp` files.
`std_binding.hpp` provides a registry forward declaration. Templates for structured data and value conversion
remain in headers where their definitions are required. The VM core continues to use generic host protocols.

Generic expansion, closures, project manifests and diagnostics compile through `magel_generic.cpp`,
`magel_lambda.cpp`, `magel_generic_closure.cpp`, `magel_package.cpp` and `magel_report.cpp`.
`macro/` independently compiles hygienic binding, structural substitution, registration, regex matching,
original fragments and editor header models. `syntax/` provides ordinary source grouping and constant
expressions; `editor/` writes real compiler analysis. AST declarations live in `magel_ast.hpp`.
The authoritative grammar assembles registration, hygienic expansion, generic closures and lowering
from `semantics/`. Language services live in `lsp/`; the compiler generator lives in `generator/`.

After grammar changes, run `sh gen.sh`, then `make -j4`. Compiler-generated `.d` files track actual include
relationships for ordinary C++ changes. `make` automatically checks standard-library, glue and manual inputs;
`tools/embed_stdlib.py` generates the assets, which compile once as `magel_stdlib_embedded.o`. Public headers
no longer include the compressed data implementation. A generation stamp tracks inputs, and unchanged output
is not rewritten. Exported compiler projects also link these implementations as separate CMake sources.

A host includes `magel_glue.hpp` and the required `std_xxx.hpp`, then links the matching `.cpp` files or host-record
library. Registration headers do not implicitly include all implementations. A complete command is provided in
the [synchronous callback example](../../example/native-callbacks/README.md).

## Independently compiled library modules

`library/simple_ast` compiles types, node construction, quadruple generation and data export separately. `library/bignum` separates integer multiplication, division, number theory, bit operations, conversion and decimal roots. Public headers retain the interfaces; Make dependency files track actual includes. Embedded assets and exported compiler projects carry all implementation units. Each library also provides a CMake target for independent builds. See the [AST library](../../library/simple_ast/README.md) and [big-number library](../../library/bignum/README.md) for file responsibilities.
