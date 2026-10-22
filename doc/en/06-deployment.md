# 06 VM deployment

Magel programs always execute in the shared VM, **write once, run anywhere**: the same source compiles to bytecode that desktop, server and supported embedded Linux hosts can load. Deploy source for compilation on the host, or export `.svmb` in advance.

```sh
magel run app.mgl
magel svmb app.mgl out build/app
```

Bytecode deployment includes `.svmb`, `.data.bin`, `.layout` and application resources. The host validates format and feature bits, installs native bindings, and runs the program. `emit` provides quadruples for inspection or assembly; quadruples are not machine code.

A host application may itself be compiled into a native executable and embed bytecode. Magel code executes through `stackvm::Machine`.

The compiler embeds its standard library and runtime assets. Operating-system services, external libraries, databases and peripherals remain deployment dependencies when used.

The host owns the program, data, registry and borrowed resources. Configure memory and instruction budgets and check execution errors and resource statistics. Embedded Linux hosts use the same VM core with explicit feature and memory budgets. That is a
runtime capability, not a cross-platform delivery promise: rebuild the VM and glue for each target
architecture and recheck toolchain and system services.

Symbol compression is not encryption or a sandbox. Updating bytecode does not automatically migrate running stacks, closures or globals.

See [host integration](08-host-integration.md), [toolchain](09-toolchain.md) and [SVMB format](../15-svmb-format.md).
