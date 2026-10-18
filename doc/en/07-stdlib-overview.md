# 07 Standard library overview

Magel's built-in modules, grouped by capability. Everything needs an `import` except the core module
`std::lang`.

```magel
import std::math;
import std::json;
int64 main() {
    println("sin0={} sqrt16={}", std::math::sin(0.0), std::math::sqrt(16.0));
    match (std::json::parse("{\"a\": 1}")) {
        Result<std::json::JsonValue, String>::Ok(doc) => {
            println("isObject={} len={}", doc.isObject(), doc.len());
        }
        Result<std::json::JsonValue, String>::Err(e) => { println("err={}", e); }
    }
    return 0;
}
```

```text
sin0=0 sqrt16=4
isObject=1 len=1
```

---

## 1. Core module `std::lang` (auto-loaded)

No `import` needed.

| Capability | Examples |
| --- | --- |
| Output and formatting | `println(fmt, …)`, `format(fmt, …)` |
| Assertions | `assert(cond)` |
| Length | `len(x)` (strings / containers) |
| Strings | `std::lang::String`; index to read a character |
| Containers | `List<T>`, `Map<K,V>`, `Set<T>` |
| Type-erased box | `std::lang::Any` (for runtime polymorphism) |
| Math constants | `std::lang::Math::PI`, `std::lang::Math::E` |
| Big numbers | `BigInteger`, `BigDecimal` |

---

## 2. By capability

### Math and numerics

| Module | Contents |
| --- | --- |
| `std::math` | `sin` `cos` `sqrt` and friends; `abs` `min` `max` are generic entry points (`min`/`max` take any number of arguments) while `clamp` keeps `int64`/`float64` overloads; constants live in `std::lang::Math` |
| `std::numerics` | scalar numerical methods: derivative, Simpson integration, bisection, RK4, Newton |
| `std::numtheory` | integer number theory: module-level generic `gcd` `lcm` (any number of arguments), `addMod` `mulMod` `powMod` `modInverse`, deterministic Miller-Rabin `isPrime` |
| `std::complex` | complex numbers `Complex<T>` with arithmetic traits |
| `std::linalg` | dense vectors and matrices, `luDecompose` `symmetricEigen` `svd` `jordan2` `diagonalize2` |
| `std::random` | deterministic LCG and entropy seeding; handle-based state, implements `Drop` |
| `std::signal` | DFT / FFT / inverse; radix-2 FFT requires a power-of-two length |

### Text and data formats

| Module | Contents |
| --- | --- |
| `std::text` | `Text::find` `split` `join` `replace` `trim` `toUpper` `padStart` … |
| `std::json` | `parse` → `Result<JsonValue, String>`; `isInt` `isObject` `len` `asInt` `asString` … |
| `std::toml` | TOML document access, isomorphic to the JSON API |
| `std::regex` | patterns are cached host-side by string; Magel holds only an integer handle |
| `std::parse` | string → integer parsing with `ParseError` |
| `std::binary` | fixed-width decoding, byte order and bounds checking |
| `std::path` | path joining, file name, extension, normalisation |
| `std::compress` | text compress / decompress; the result is **binary** `List<uint8>`, never print it as a string |

### Concurrency

| Module | Contents |
| --- | --- |
| `std::coroutine` | shared coroutines |
| `std::thread` | isolated threads (`IsolatedThread`) |
| `std::message` | messages across coroutines / threads |
| `std::concurrency` | explicit shared slots `Shared<T>`; ordinary globals stay isolated between threads |
| `std::event` | interrupt events and software timers; after `start()` they fire periodically at instruction boundaries |

> `SharedCoroutine` / `IsolatedThread` / `Message` are **three separate modules** and cannot be
> merged — they have same-named host functions `create` / `wait` / `receive`.

### System and environment

| Module | Contents |
| --- | --- |
| `std::io` | file I/O with error type `IoError` (`OpenFailed` `ReadFailed` `WriteFailed` `SeekFailed` `Closed`) |
| `std::net` | TCP plus a minimal HTTP client; sockets live in host glue, Magel sees only an `int32` handle |
| `std::sql` | MySQL / SQLite / PostgreSQL / SQL Server; SQL is executed by the engine, Magel sends and fetches |
| `std::process` | processes and system calls; command pipes are held by a host handle pool |
| `std::clock` | monotonic clock `steady_clock`; take differences only, never treat it as wall time |
| `std::memory` | `Span<T>` (pointer + length view); its iterator returns **element pointers**, so `for (T mut& p in span)` writes directly |
| `std::watchdog` | soft-restart watchdog (both the watermark and the timeout dimension are off by default), one implementation shared by desktop and board |

### Iteration

| Module | Contents |
| --- | --- |
| `std::iter` | `range`, lazy `map` / `filter` / `collect` chains, the `IterNext<T>` interface |

`std::probe` is a test stub, not for applications.

---

## 3. Usage conventions

**Handle-based resources must be closed explicitly.** Values from `std::json`, `std::regex`,
`std::sql` and similar hold host resources: call `close()`, or implement `Closeable` and let scope do
it. Closing twice returns `false` and does not crash.

**Compressed output is binary.** `std::compress` output contains NUL bytes and has type
`std::lang::List<uint8>`; only the decompressed result is a `std::lang::String`.

**The front-end `[embedded]` profile blocks selected modules.** It rejects std::net, std::process,
std::sql, std::io, std::thread, std::coroutine and std::concurrency. Standalone MCU hosts separately
choose build macros, filesystems, coroutine support and glue; see [deployment](06-deployment.md).

**Migrated modules now need `import`.** These symbols used to be visible automatically through
`source/std/lang.mgl`'s injected body; now everything except core `std::lang` must be imported explicitly.

---

## 4. Where to find complete APIs

The standard library source is itself the best reference, and each module header documents its
capabilities:

- [`source/std/`](../../source/std) — all module sources, `source/std/<name>.mgl`
- [`source/glue/`](../../source/glue) — host-side C++ glue; `source/glue/std_*.hpp` is authoritative for each module's
  bindings
- [`example/samples/`](../../example/samples/) — runnable samples organised by feature
- Per-domain API reference: [`doc/library/`](../library/README.md) has one page per module
  (`std-json.md`, `std-toml.md`, `std-regex.md`, `std-linalg.md`, `std-complex.md`, `std-net.md`,
  `std-sql.md`, …)

---

## 5. Next

| You want | Where |
| --- | --- |
| Call your own C/C++ from Magel | [Host integration and C ABI](08-host-integration.md) |
| Capabilities beyond the standard library | [Language reference](04-language-reference.md) |
| Runnable samples for a feature | [`example/samples/`](../../example/samples/) |

0.14.0 adds `std::encoding`, `std::csv`, `std::datetime`, `std::cli`, `std::utf8` and extended file operations. See the [Any, explicit generics and data utilities](22-any-generics-data.md).
