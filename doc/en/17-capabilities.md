# 17 Current capabilities and implementation limits

Loops use `while`, `loop`, and `for (... in ...)`. Both `for-in` and `while` support value expressions with `else` and `yield`. List literals use `[a, b, c]`; Set literals use `{a, b, c}`. Intervals use `std::lang::range(a, b)` (half open) and `rangeEq(a, b)` (inclusive). Integers and char support intervals directly; custom iteration types implement `NextValue<T>`. Interval match patterns accept integers and char, while ordinary custom value patterns use `Equal<T>`.

[Documentation index](README.md) · [中文](../17-capabilities.md)

This page describes the current checkout. Read the version from [`VERSION`](../../VERSION) and
release history from [`CHANGELOG.md`](../../CHANGELOG.md). Grammar, standard-library sources, build
configuration and regressions define the implementation. Historical measurements retain the actual
tested version, configuration and date; they do not automatically describe current builds or other targets.

## Language and resources

Support includes [return control expressions, statement-level named closures and field derivation](28-declaration-sugars.md). Hidden result bindings preserve internal moves, external copies and borrow cleanup. Named functions reuse fn captures; `#derive` generates only Equal/Hash/Clone, validates field protocols and never generates Drop.

Generic closures support `fn<T>`, inference from argument types, full specialization and uniquely most specific partial specialization. They can be direct struct members and can be passed or stored as values of a generic type parameter `F`. Calls with different types share one closure environment, preserving ordinary capture, copy and borrow rules. See the [language reference](04-language-reference.md).

Sources and macros are shared through `module` / `import`; preprocessing does not support textual file inclusion. Macros expand as text before the Lexer, with ordered and repeated groups of four bracket kinds, independent `..rest` captures, and multiline brace arguments. Captured fragments and code following a call retain their original line and column positions. See [ordered bracket groups](24-literals-operators-macros.md#ordered-bracket-groups-in-text-macros).

Entry points support `int64 main` and `void main`, with no parameters or `List<String>` arguments. A void entry exits with code 0 on normal completion; panic still fails. Trailing semicolons on `struct`, `enum`, and `interface` declarations are optional.

| Area | Implemented capability and contract |
| --- | --- |
| Types | Fixed-width integers, float32/float64, bool, char, String, big numbers; arrays, tuples, structs, payload enums, containers and function types |
| Abstraction | Overloads, generic monomorphization, interface constraints, associated types, constant generics, default type parameters, default interface methods and runtime interface dispatch |
| Function values | Anonymous functions use `fn`; inline arguments, nested higher-order functions and const/mut/copy/move captures are supported |
| Control flow | Match, guards, integer singleton/range overlap warnings, destructuring, ranges, if expressions, block expressions with `yield value;`, tuple exits with `yield (a, b);`, nested-loop exits with `break level N;` / `break all;`, implicit moves of direct variables and value breaks across non-value loops, macros and conditional compilation |
| Errors and cleanup | Option, Result, custom error propagation, `?`, panic, defer, scope(exit), Drop and Closeable |
| Ownership | Ordinary owned-value assignment and value arguments copy; move transfers resources. The old binding cannot be read, written or rebound; a complete move permits a new declaration using that name |
| References | Non-null read-only/mutable references; `Option<T const&>` and `Option<T mut&>` represent an optional reference, with None constructing no reference |
| Checking | Independent provenance and control-flow checking tracks locals, fields, aliases, moves, releases and closure captures; callable uses validate captured sources, preserving dependencies through copies, destructuring and aggregates; alias sharing is allowed, without a global exclusive-borrow rule |

Owned values have deterministic cleanup without GC. References, host resources and aggregates containing
references retain their individual contracts. Manually allocated storage must be released as specified.
Static checking does not cover arbitrary raw memory, external host behavior or every concurrent invalidation.
String indexing and slicing use UTF-8 bytes; use `std::utf8` for code-point operations.
Ordinary string literals share generated data by byte contents and cannot be modified or mutably borrowed; independent String values retain value copying.
Runtime array parameters `T[]` copy the length and elements. `T[] &` / `T[] const&`
borrow read-only arrays; `T[] mut&` borrows a mutable array. Array references retain
element layout, runtime length and the source lifetime.
See the [language reference](04-language-reference.md), [core concepts](03-core-concepts.md) and
[move rules](23-ownership-move.md).

Explicit Any targets support contextual boxing in generics and heterogeneous List/Set/Map, exact extraction, clearing, and borrow checks. Interface references bind directly to implementing concrete references without an Any intermediary; views still borrow their source. See [Any and interface binding](22-any-generics-data.md).

Every non-void value has default `toString()` and String conversion: structs `{...}`, List/arrays `[...]`, Set/Map `{...}`, and tuples `(...)`, with quoted and escaped nested strings. User toString and ImplicitConvert<String> supply missing counterparts; dedicated explicit conversions retain priority. Reflection is not required. See [value-text rules](04-language-reference.md#default-value-text-and-string-conversion).

## Execution, optimization and delivery

| Route | Implementation | Host requirements |
| --- | --- | --- |
| Source | Front-end checks and quadruple lowering followed by VM execution | Compiler and required host services |
| Bytecode | Shared VM validates and loads `.svmb` | Compatible VM features, glue and companion data |
| Embedding | Host executes bytecode with `stackvm::Machine` | VM host and target resource adapters |

IR means quadruples. Full quadruple optimization and the highest supported VM optimizations are enabled by default. Source and bytecode share the VM; there is no JIT or native program exporter.

Symbol compression is not encryption or a sandbox. Program updates do not automatically migrate running stacks, closures or globals.

## Host integration and reflection

Magel calls C libraries through `abi` and C++ through a Registry. References, structs, arrays, optional
references, returned-memory release and limited header bindings follow explicit ABI contracts; declarations
must match the actual C signatures. Glue calls named functions, inline fn values and closures through
`NativeCallContext::invoke` or `magel_glue::Function`. VM share the interface, argument-count checks
and error propagation. Wrappers and contexts are valid only synchronously, on the same thread, during
the enclosing host callback. Resource values retain their Codec ownership requirements.
See [host integration](08-host-integration.md) and the [callback example](../../example/native-callbacks/README.md).

The root toolchain defaults to `MAGEL_WITH_REFLECTION=1` and `STACKVM_WITH_REFLECTION=1`, with reflection
enabled for source compilation; use `--no-reflection` to disable it. Generated compiler projects and MCU
hosts have their own documented build defaults. Reflection supports annotations, explicitly exposed
metadata and restricted dynamic calls; it does not create arbitrary runtime types or offer general field
writes and arbitrary resource-value invocation. It carries no hard-real-time guarantee.
See the [reflection specification](../19-reflection.md).

## Libraries, MCU and tools

Libraries cover containers/algorithms, iterators, math, numerical methods, text/encoding, JSON/TOML/CSV,
files/directories, TCP/basic HTTP, SQL drivers, coroutines, isolated threads, messages, explicit shared
values, events and watchdogs. The [API index](../library/README.md) defines the module list; overview
pages do not maintain a separate module count. HTTP excludes TLS; external databases need their
clients and services. Asynchronous event callbacks have separate lifetime contracts.

Desktop and MCU builds share one VM core. `library/stackvm_mcu/` supplies a host without exceptions/RTTI,
pools and platform adapters. The front-end `[embedded]` module gate and standalone MCU build macros/glue
are separate configurations. The H743 port provides SDRAM, FatFS, USART1, an LCD selector and keys,
and starts one benchmark by default. Its 2/16/4 MiB pools total 22 MiB, which is not a small-SRAM footprint.
The host can return to its menu after normal exit or language panic; hardware Faults, host corruption
and unrecoverable exhaustion remain platform concerns. See [deployment](06-deployment.md), the
[MCU host](../../library/stackvm_mcu/README.md) and [H743 project](../../example/mcu/FK-STM32H743XIH6/README.md).

The CLI provides projects, checks, formatting, tests and dependency locks/caches. LSP provides diagnostics,
completion, hover, signatures and cross-file navigation, plus complete inferred `auto` type hover, optional type inlays,
grouped macro signatures, expansion origins, references and rename based on unique bindings. Ownership diagnostics
can link related declarations and cleanup locations. Ambiguous overloads or macro template bindings reject refactoring;
see [LSP details](../man/en/lsp.md). DAP provides breakpoints, stepping and stacks;
variable inspection and expression evaluation are not implemented. Dependencies support local directories,
fixed Git commits and hashed HTTP/HTTPS archives. Feature implementation, regression evidence and
long-term production validation are separate claims.

Tool interfaces support Chinese and English; unsupported system locales fall back to English and explicit Japanese selection is rejected. `man book`, `man keyword`, and `man skill` query embedded book, keyword and AI SKILL data. The VS Code package includes the same SKILL.

API manuals require `magel man api`; omitting `api` produces a usage error. `magel man macro` displays embedded macro help.

`opcode T { new = create; drop = destroy; copy = clone; };` declares an opaque glue object.
Copies call the declared glue function. `#NonCopyable` disables copying and omits copy.
A struct may declare multiple opcode fields. BigInteger, BigDecimal, File and Process
records and operations reside in standard library glue; the VM core manages generic NativeRecord
shells and a host service table. File and Process are noncopyable. Process is the public type name.
List, Map and Set use parameterized opcode types and opaque standard library records; there is no
frontend Container Kind or dedicated VM collection Kind or method implementation. List uses ArrayList
storage. Map/Set use HashMap/HashSet storage for intrinsic scalar keys and custom keys implementing
Hash<T>; custom keys with Equal<bool,T> but no Hash<T> use ArrayMap/ArraySet with linear lookup.
Hash collisions are resolved with Equal. Map and Set promise neither sorted nor insertion order,
including keyAt, valAt, at, iteration and default text. String isolation remains unfinished.


Image is a deeply copied opcode whose dimensions and contiguous RGBA pixels belong to glue. width()/height() query dimensions; pixels() returns an independent list snapshot. TcpSocket and TcpListener are noncopyable opcodes that close automatically at scope exit; explicit move transfers ownership, and each connection retains its own read status and error. Generic Vector<T> and Matrix<T> retain their aggregate API; ScalarArray<T> stores built-in integer and floating-point values in a ScalarStorage opcode with independent copies. Standard glue implements string concatenation, slicing, comparison, search and numeric-to-text conversion. Internal entry points use the __std_ prefix; the VM retains execution control, memory safety and closures.

Network async methods use nonblocking polling and NET_PENDING, allowing callers to yield between polls. connectAsync currently accepts numeric IPv4 addresses only; synchronous connect retains hostname resolution. sendAsync returns a partial byte count and accepts an offset; receiveLineAsync preserves incomplete lines with a 1 MiB buffer limit.
