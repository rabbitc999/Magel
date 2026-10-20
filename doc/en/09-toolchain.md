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
