# Any, explicit generics and data utilities

[Documentation index](README.md) · [Any, explicit generics and data utilities](../22-any-generics-data.md)

Generated runtime assets must match the compiler. Recompile source, regenerate host glue and rebuild caches after changing type contracts.

## Any and type checks

The public boxed type `Object` is now `Any`, available from the automatically loaded core module. Copying, destruction, interface conversion and borrow checks retain their existing rules. The new name does not permit local addresses to escape through a longer-lived box.

`nameof` and the box's `.name()` method have been removed. Internal dispatch tags are not application identifiers and must not be stored in files, protocols or databases.

| Removed spelling | Current spelling |
| --- | --- |
| `Object value` | `Any value` |
| `value.name() == nameof(int64)` | `isType<int64>(&value)` |
| `value.name() == 0` | `value.isEmpty()` |
| Checked `getValue()` extraction | Prefer `tryGet<T>(&value)`, returning `None` on a mismatch |
| Borrowed extraction | `tryGetReference<T>(&value)` or `tryGetMut<T>(&value)` |

```magel
int64 main() {
    Any value;
    assert(value.isEmpty());
    value.set((int64)7);
    assert(isType<int64>(&value));
    Option<int64> number = tryGet<int64>(&value);
    assert(number.unwrap() == 7);
    assert(!tryGet<String>(&value).isValue());
    println("Any: {}", number.unwrap());
    return 0;
}
```

`isType<T>` checks the exact stored type without numeric conversions. `tryGet<T>` copies an owned value. Reference extraction respects the lifetime of both the box and the original object; interface extraction also checks the implementation and receiver mutability. The existing `getValue` and `getReference` methods remain available and report runtime errors on mismatches.

Stable metadata identities, fields, methods and annotations belong to `std::reflect`. Its `getTypeId<T>()` still requires a reflection build and `--reflection`. The private `__typeTag` operation is restricted to core-library and generated reflection code.

## Any target conversions and heterogeneous containers

An explicit `Any` target automatically boxes supported concrete values. This applies to arguments, returns, explicit generic instantiations such as `identity<Any>(value)`, generic fields, enum payloads, and container element/key/value positions. Existing Any values copy directly without another boxing layer. A user-defined direct `ImplicitConvert<Any>` takes precedence.

```magel
T identity<T>(T value) { return value; }
int64 main()
{
    List<Any> values = {1, "two", true};
    values.push(1.5);
    Any value = identity<Any>("generic");
    Map<String, Any> record = {"count": 3, "name": "Magel"};
    Set<Any> unique;
    Any key = "key";
    unique.add(key);
    unique.add(key);
    assert(unique.len() == 1);
    value.clear();
    assert(value.isEmpty());
    return 0;
}
```

`List<Any>`, `Set<Any>`, and either or both Any positions in `Map<K,V>` are supported. The explicit target determines collection literals. Typed containers do not become dynamically typed, and omitted generic arguments do not default to Any. Return block/if/match branches must still agree on their concrete result type before the whole result is boxed.

`Any.clear()` requires a writable receiver, releases the owned payload, and restores the empty state. The box remains reusable. Boxing preserves exact types and evaluates the source once. Strings and containers retain owned value copies; full-expression and scope cleanup remain deterministic, without GC. Use `isType<T>`, `tryGet<T>`, or the reference extraction helpers.

**Any keys compare by identity.** Copies retain identity, so inserting the same box twice deduplicates and its copy can query a map entry. Independent boxing of equal payloads creates distinct keys. Preserve an `Any key` for heterogeneous-key queries; prefer `Map<String, Any>` for lookup by string contents. Boxing does not extend borrowed lifetimes. Multi-slot structs/enums/arrays retain the existing address-boxing restrictions; local or temporary storage cannot escape, and unsupported void/resource values cannot be boxed.

## Direct interface reference binding

```magel
interface Readable { int64 read(); };
struct Counter : Readable
{
    int64 value;
    int64 read() { return self.value; }
};
int64 consume(Readable const& view) { return view.read(); }
int64 main()
{
    Counter counter;
    counter.value = 7;
    Readable const& view = &counter;
    println("{} {}", view.read(), consume(&counter));
    return 0;
}
```

Interface reference targets accept references to concrete types implementing that interface in initializers, assignments, arguments, and returns. No Any intermediary is needed. Lowering directly creates the existing address-and-type-tag view and reuses interface dispatch. Read-only sources cannot acquire mutable permissions. Copying the view copies its handle, not the underlying object; the original object must outlive every view. Missing implementations and escaping local references remain compile-time errors.

## Explicit generic arguments

Missing type arguments no longer default to Any. Function inference and defaults explicitly written in a template declaration remain supported.

```magel
struct Box<T> { T value; };
struct ExplicitDefault<T = Any> { T value; };
int64 main() {
    Box<int64> number;
    number.value = 7;
    Box<Any> dynamic;
    dynamic.value.set("explicit");
    ExplicitDefault<> chosen;
    chosen.value.set(true);
    return 0;
}
```

For example, replace `Pair<int64>` with `Pair<int64, String>`, or explicitly choose `Pair<int64, Any>` when type erasure is required.

## New modules

| Module | Initial functionality | Errors and platform boundary |
| --- | --- | --- |
| [`std::encoding`](../library/std-encoding.md) | Base64, Base64URL, hexadecimal | Strict `Result` decoding; implemented in Magel |
| [`std::csv`](../library/std-csv.md) | Reading, writing, quoting, delimiters, error positions | Multiline quoted fields; one-based lines and byte columns; implemented in Magel |
| [`std::datetime`](../library/std-datetime.md) | Unix seconds, UTC dates, formatting and arithmetic | Gregorian years 1..9999; UTC ISO text at second precision; current time needs a host clock |
| [`std::io`](../library/std-io.md) | Byte streams, whole files, directory listing, metadata, creation and renaming | Stream bytes use FileApi; path operations use the desktop filesystem |
| [`std::cli`](../library/std-cli.md) | Long/short options, positional values, defaults, help and validation | Returns results without printing or exiting; implemented in Magel |
| [`std::utf8`](../library/std-utf8.md) | Validation, code points, counts and slicing | Strict UTF-8; code points are not grapheme clusters; byte APIs support U+0000 |

Use short names when unambiguous. Module reference pages link to complete executable examples.

```magel
import std::encoding;
import std::datetime;
import std::utf8;
int64 main() {
    List<uint8> bytes = decodeHex("666f6f").unwrap();
    println("{}", encodeBase64(bytes));
    DateTime utc = parseUtc("2024-02-29T12:34:56Z").unwrap();
    println("{}", formatUtc(addDays(utc, 1).unwrap()).unwrap());
    println("{}", sliceCodePoints("A中文😀", 1, 2).unwrap());
    return 0;
}
```

## Files and embedded hosts

`FileStream.readBytes(count)` returns a short list at EOF. Byte writes preserve zero bytes. Desktop, MCU stdio and FatFS adapters implement byte reads; older custom backends explicitly report an unsupported operation until adapted.

`readAllBytes` defaults to a 16 MiB limit that callers can change. `readAllText` reads the entire content but rejects embedded zero bytes; use byte APIs for binary files. Directory listing is one level deep and sorted. File metadata follows symbolic links. Rename replacement rules depend on the platform; these operations do not promise transactional or atomic writes.

Embedded filesystems are injected through FileApi. Desktop directory operations are not automatically available on a board. Date conversion does not depend on the operating system's timezone; `nowUtc` requires an actual UTC clock from the host.

## Binary intermediate artifacts and measurements

`emit` now writes `.quad` (MGQI v1), `.layout` (MGLY v1), and `.data.bin`. Quads share a string table and encode counts, positions and references with unsigned LEB128. Layout records retain names, offsets, sizes and strides. Raw `.data.bin` bytes remain unchanged. Duplicate `.data` text and `.data.h` arrays are no longer emitted; rebuilding the same prefix removes old copies.

The VM and AOT exporter detect binary formats automatically and still accept legacy text. Decoders check versions, overflow, truncation, string references and trailing data. This is serialization without encryption.

Measured on Windows / MinGW `-O2`, 2026-10-13, comparing identical content (bytes):

| Sample | Quad text → binary | Layout text → binary | Four old files → three new files | Reduction |
| --- | ---: | ---: | ---: | ---: |
| std_io_extended | 903,013 → 216,295 | 33,548 → 2,038 | 976,988 → 223,797 | 77.1% |
| std_cli | 764,630 → 184,923 | 26,979 → 2,574 | 822,671 → 191,457 | 76.7% |

Old totals include `.quad`, `.layout`, `.data`, `.data.bin`; new totals omit `.data`. The unchanged raw images contain 5,464 and 3,960 bytes respectively. Old `.data.h` arrays are excluded from this comparison.

The `magel-artifact-tests` CMake target benchmarks in-memory decoding for at least 250 ms per format, including structure allocation and destruction, excluding disk and compiler work. For the two samples, quad decoding averaged 19,213.7 → 2,766.85 µs (6.9×) and 16,294.9 → 2,398.31 µs (6.8×). Layout decoding averaged 300.935 → 3.969 µs and 284.044 → 4.734 µs. These are decoding measurements, not whole-compilation speedups. Optional text quad/layout path arguments allow measurements on other inputs. Tests also verify structural, source-location, assembled-instruction and layout equivalence and reject malformed inputs.
