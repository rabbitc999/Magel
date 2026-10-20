# 08 Host integration and C ABI

Magel integrates with hosts through the shared VM and native registries. Magel can call C/C++ functions; a host can load and run Magel bytecode.

## Running Magel in a host

Load `.svmb` with `stackvm::loadProgramBinary`, load data and layout with `DataArea::load`, configure `stackvm::Machine` with data and a native registry, then call `Machine::run`.
See the [VM host example](../../example/native-callbacks/vm_host.cpp). The host must register the standard glue used by the program, including collections. The VM core has no dedicated collection storage or methods.

The host maintains the program, data, registry and borrowed resources for the required lifetime. Execution results report exit status, errors, output and live resource counts.

## C++ glue and synchronous callbacks

Bind functions with `magel_glue::Registry::bindTyped` or `bindTypedWithContext`. Complex host values cross the boundary through typed handles or generic `NativeRecord` records.

Invoke Magel functions and closures synchronously with `magel_glue::Function` or `NativeCallContext::invoke`. Argument counts, return values, closure environments, reentry and failures follow the VM protocol. Runtime function addresses must not be cast to machine function pointers. Callback wrappers and contexts are used synchronously on their owning thread during the host call.

List uses ArrayList. Standard glue selects hash or array storage for Map and Set according to key Hash support. Map iterators yield `Pair<K,V>`.

## Calling C dynamic libraries

An `abi "library" { ... }` declaration imports native C symbols into the VM. Scalars, references, structures, arrays, optional references, callbacks and returned-memory release follow the ABI contract and must match the actual C signatures and layouts.
Complex calls may require a C++ toolchain to prepare a foreign-call bridge; the Magel program continues to execute in the VM.

See the [ABI examples](../examples/abi_contracts/).

Reflection is enabled together in the default compiler and VM. MCU features are explicitly trimmed by the host. Native failures retain VM diagnostics; the VM does not promise a complete sandbox.

See [toolchain](09-toolchain.md), [capabilities](17-capabilities.md) and [callbacks](../../example/native-callbacks/README.md).
