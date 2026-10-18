# 08 Host integration and C ABI

Owned C results can name a `void release(void*)` symbol in the same library:
`String text() = "owned_text" ["free_owned"];` or
`int32[] numbers(int32 count) = "owned_numbers" [count, "free_owned"];`.
Magel copies results before releasing them, including failure paths. Without a release
symbol, the library retains ownership. C strings must be NUL terminated; string
arguments reject embedded NUL bytes. Use arrays for binary data. Layout validation
checks sizes, alignment and field bounds; it cannot infer the actual native signature
or validate arbitrary native pointers.

Scalar `fn` callbacks are supported synchronously on the calling thread. Native code
must invoke them before returning and must never retain their pointers. Asynchronous
callbacks and userdata-style APIs need a C wrapper. Callback failures are caught and
reported after the native function returns, without unwinding through C frames.

Complex ABI bridges are cached. `--abi-cache DIR` selects a directory;
`--abi-no-cache` disables caching. Use `magel run app.mgl --abi-prepare
--abi-write-bridge bridge.dll` to prepare a bridge without executing the program,
then `--abi-bridge bridge.dll` to load it explicitly. Generated-code identity must
match. Only load trusted native artifacts. `MAGEL_ABI_CXX` selects the bridge compiler.

`magel gen abi api.h --library ./native.dll --out bindings.mgl` imports a limited
C subset: fixed-width scalars, simple structs, integer enum constants, scalar typedefs
and synchronous scalar callback typedefs. Generated types use `C_`, functions `c_`.
Pointers, ownership, complex preprocessing, bitfields, unions and variadic declarations
require manual wrappers. Unsupported declarations fail before writing a partial binding.

A Magel program is not an island. This document covers two things: **how Magel calls host (C/C++)
code**, and **how a host program runs Magel logic**.

C interfaces declared with `export` require AOT. Both `magel run` and `magel svmb`
reject these declarations; `--check` can still validate the source. Build a shared
library with `magel aot app.mgl --out app.dll` (use `.so` with a Linux toolchain).
Library source needs at least one `export` and can omit `main`. `--out app.o` produces a
relocatable object for the host linker. It is not loaded directly by a dynamic loader.
Static `.lib` / `.a` output is unsupported. See [library output](09-toolchain.md#aot-library-output).
The build also emits a C header. Create an independent context with `magel_create`,
call `magel_<function>`, then release it with `magel_destroy`. See the
[C export and ABI contract examples](../examples/abi_contracts/).
Calling C libraries from Magel through `abi "library" { ... }` remains supported by the VM.

---

## 1. Three integration routes

| Route | Use it when | Cost |
| --- | --- | --- |
| `abi` block | calling C-ABI functions exported from a dynamic library | you need a `.dll` / `.so` |
| Host glue bindings | registering C++ functions with the compiler so Magel calls them like ordinary functions | a little C++ |
| `runMagel` embedding | a host program (including firmware) calling compiled Magel logic | needs an AOT artifact |

---

## 2. `abi` blocks: calling C directly

```magel
abi "./native.dll" {
    int32 native_add(int32 left, int32 right) = "abi_add";
    String native_name() = "abi_name";
}

int64 main() {
    println("{} {}", native_add((int32)20, (int32)22), native_name());
    return 0;
}
```

The syntax is:

```
abi "<dynamic library path>" {
    <return type> <Magel-side name>(<parameter list>) = "<symbol in the library>";
}
```

The right-hand side is the symbol actually exported by the library; the left-hand side is the name you
use in Magel. They may differ, which is handy for giving a batch of C functions a consistent Magel
naming style.

Complete examples: [`doc/examples/abi_dynamic/`](../examples/abi_dynamic/) (minimal) and
[`doc/examples/abi_buffers/`](../examples/abi_buffers/) (structs, arrays, optional references).

### Supported types

| Magel spelling | Meaning |
| --- | --- |
| `int8` … `int64`, `uint8` … `uint64`, `float32`, `float64`, `bool` | scalars, by value |
| `String` | string |
| `T mut&` / `T const&` | writable / read-only reference |
| `Option<T mut&>` / `Option<T const&>` | **nullable reference** (the host may receive null) |
| `T[]` | array view |
| `T[] f(...) = "..." [count]` | return length determined by a parameter |
| struct | by value; layout follows C rules by default |
| `struct X packed { … }` | packed layout (no alignment padding) |

```magel
abi "./native.dll" {
    void packed_update(Packed mut& value) = "packed_update";
    Option<Packed const&> packed_snapshot() = "packed_snapshot";
    Packed[] packed_numbers(int32 count) = "packed_numbers" [count];
    void packed_fill(Packed[] values, int32 count) = "packed_fill";
}
```

Points to note:

- Reference arguments are passed as `&x` at the call site: `update(&q);`, `bump(&q.count);`.
- Nullable references go through `Option<Packet mut&>`: pass `Option<Packet mut&>::None` for absent,
  and the host receives a null pointer. This is the **only** way "null" crosses the boundary —
  references inside Magel remain non-null.
- Arrays use `T[]` with the length supplied separately; for return values, `[count]` names the
  parameter that determines the length.
- `packed` structs match a C struct declared with `#pragma pack`.

---

## 3. Host glue bindings

To register C++ functions (rather than go through a dynamic library), write a `bindMagelGlue`:

```cpp
#include <cstdint>
#include <string>

struct ExportCounterTag;
using ExportCounter = magel_glue::Handle<ExportCounterTag, std::uint64_t>;

bool bindMagelGlue(magel_glue::Registry &registry, Memory::String *error)
{
    return registry.bindTyped<ExportCounter, std::uint64_t>(
               "demo_counter_from_uint",
               [](std::uint64_t arg0) { return ExportCounter{arg0}; }, error) &&
           registry.bindTyped<std::int32_t, Memory::String>(
               "demo_cpp_text_size",
               [](Memory::String arg0) { return static_cast<std::int32_t>(arg0.size()); }, error);
}
```

The template arguments are ordered `<return, parameters…>`, and the first string argument is the
**binding key**.

Conventions that go with it:

- Declare **nominal host types** with `magel_glue::Handle<Tag, AbiType>` — distinct tags are distinct
  types and cannot be confused.
- **Pass complex host objects across the boundary as integer handles**; never push a C++ object body
  into a Magel value.
- Generic functions get a suffix on instantiation: `demo_cpp_identity<int32>` binds under the key
  `demo_cpp_identity__int32`.

On the Magel side, declare them like ordinary functions:

```magel
type HostHandle<T> = uint64;

HostHandle<int32> demo_cpp_add(HostHandle<int32> left, HostHandle<int32> right);
int32 demo_cpp_text_size(String text);
T demo_cpp_identity<T>(T value);
```

To make a host type implement a Magel interface or conversion trait, use `impl`:

```magel
impl HostHandle<int32> : HostValue = demo_counter_to_uint;
impl uint64 : ExplicitConvert<HostHandle<int32>> = demo_counter_from_uint;
```

Conversions are invoked explicitly with `cast<T>(x)`.

Reference implementations: [`source/glue/cpp_glue.cpp`](../../source/glue/cpp_glue.cpp) (application glue) and
[`source/glue/std_glue.cpp`](../../source/glue/std_glue.cpp) (standard library glue). Each standard library module's
bindings are defined in `source/glue/std_*.hpp`.

---

### Calling Magel functions synchronously from glue

VM and AOT share `magel_glue::Function<Result(Args...)>`. Pass a named Magel function,
inline `fn`, or captured closure to a registered C++ callback and call the wrapper directly.
Execution resumes in the same program, sharing globals, heap and objects.

```magel
int64 hostApply(int64 fn(int64) function, int64 value);
int64 triple(int64 x) { return x * 3; }
int64 main() {
    assert(hostApply(triple, 7) == 21);
    assert(hostApply(int64 fn(int64 x) { x + 1 }, 7) == 8);
    return 0;
}
```

`function(value)` encodes arguments and decodes the result. Use
`function.tryInvoke(rawResult, value)` to check success explicitly, then decode the raw slot
with `Codec<Result>::decode(context, rawResult)`. The low-level equivalent is
`Context::invoke(functionAddress, encodedArguments, rawResult)`, also usable by MCU glue.

| Capability | VM | AOT |
| --- | --- | --- |
| Named functions, inline `fn`, copy/move closures | Supported | Supported |
| Integer, floating, boolean, void, references and higher-order arguments | Shared Codec interface | Shared Codec interface |
| Nested Magel → glue → Magel synchronous calls | Supported | Supported |
| Panic, null function and argument count failures | Stop execution and preserve the original error | Stop execution and preserve the original error |
| Installing custom glue | Attach the host Registry to Machine | Compile with `--glue` |
| C `export` interface | Unsupported | Supported |

Function addresses are runtime values or closure handles, not host machine function pointers.
Wrappers and contexts are valid only during the enclosing host callback, synchronously on the
same thread; do not cache them for asynchronous use. Wrappers do not copy or retain captures.
Resource values such as strings, lists and aggregates retain their individual Codec ownership
requirements; this interface does not automatically transfer arbitrary resource types.
Failed invocations remain runtime failures and must not be treated as ordinary default results.

See the [complete callback example](../../example/native-callbacks/README.md) for C++ bindings,
a VM host, commands and ten callback cases, and the
[VM/AOT differential regression](../../tests/magel_native_callbacks.py) for executable checks.

## 4. Embedding Magel in a host program

AOT output carries the entry point:

```cpp
static int runMagel(int argumentCount, const char *const *arguments,
                    void *buffer, std::size_t memoryBytes);
```

| Parameter | Meaning |
| --- | --- |
| `argumentCount` / `arguments` | arguments for Magel's `main` (program name excluded) |
| `buffer` | runtime memory buffer; pass `nullptr` to let the program allocate |
| `memoryBytes` | buffer size, i.e. the runtime memory ceiling |

The generated file wraps `int main(...)` in `#ifndef MAGEL_NO_MAIN`. **Define `MAGEL_NO_MAIN` before
compiling and only `runMagel` remains for your firmware to call.**

Typical use: carve out a static memory region at firmware startup, then call `runMagel` repeatedly to
run business logic; update the logic by replacing the data without touching the firmware. See
[Cross-platform deployment](06-deployment.md#4-embedding-into-a-host-program).

---

## 5. Type mapping cheat-sheet

| Magel | C / C++ |
| --- | --- |
| `int8` / `int16` / `int32` / `int64` | `int8_t` / `int16_t` / `int32_t` / `int64_t` |
| `uint8` … `uint64` | `uint8_t` … `uint64_t` |
| `float32` / `float64` | `float` / `double` |
| `bool` | `bool` |
| `char` | `char` |
| `String` | `Memory::String` (on the glue side) |
| `T mut&` | `T*` |
| `T const&` | `const T*` |
| `Option<T mut&>` | `T*`, may be null |
| `T[]` | `T*` plus a length parameter |
| struct | C struct with fields in the same order |
| `struct X packed { … }` | C struct with `#pragma pack(1)` |

---

## 6. Boundary cautions

- **References inside Magel are always non-null.** Only `Option<T mut&>` / `Option<T const&>` can send
  "absent" across the boundary, and only at the abi / glue layer.
- **Do not hold a pointer handed over by Magel for long on the host side.** A reference is valid only
  for the duration of the call.
- **Use handles for complex objects.** An integer handle is safer than a raw pointer and lets the host
  manage lifetime.
- **Networking, processes and SQL are blocked under embedded mode**, so the corresponding glue
  bindings are unavailable there — see
  [Cross-platform deployment](06-deployment.md#3-embedded-profile-and-standalone-vm).

---

## 7. Next

| You want | Where |
| --- | --- |
| Complete runnable examples | [`doc/examples/abi_dynamic/`](../examples/abi_dynamic/), [`abi_buffers/`](../examples/abi_buffers/), [`library/sqlite/`](../examples/sqlite/) |
| How the standard library reaches the host | [`source/glue/std_*.hpp`](../../source/glue) |
| Deploy to a board | [Cross-platform deployment](06-deployment.md) |
| AOT and command-line options | [Toolchain](09-toolchain.md) |
