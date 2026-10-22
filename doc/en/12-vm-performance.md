# VM performance: VM, JVM, Python, Lua and C++

This report retains historical measurements of ChaLi 0.18.3 / 0.18.2 on 2026-10-15, on Windows 10 10.0.19045 x64,
Ryzen 5 5600X (6 cores / 12 threads), 32 GB RAM.

These measurements are not a rerun of the current VERSION; see [current capabilities](17-capabilities.md).

## Configuration and results

Both ChaLi versions use their own compiler and bytecode, reflection enabled and all default IR/VM optimizations. Their VM runners use GCC/MinGW 11.2.0 `-O2 -DNDEBUG`. JVM uses Oracle Java 22.0.1+8-16, CPython is 3.12.3,
and Lua is official 5.5.1 built with the MinGW `-O2` target without JIT. No handwritten closed-form sums are used.

The following are second-round medians in milliseconds. Each round has seven samples per configuration.

| Configuration | Integer sum | Branches | float64 sum | Calls | Array reads |
| --- | ---: | ---: | ---: | ---: | ---: |
| ChaLi 0.18.3 VM | 135.138 | 33.099 | 353.936 | 609.619 | 85.360 |
| ChaLi 0.18.2 VM | 134.522 | 32.659 | 491.668 | 633.749 | 84.069 |
| JVM without JIT (-Xint) | 145.578 | 22.043 | 25.071 | 26.915 | 16.734 |
| JVM with JIT (-Xbatch) | 2.302 | 0.377 | 1.299 | 0.231 | 0.479 |
| Python | 400.934 | 61.285 | 71.810 | 72.427 | 51.250 |
| Lua | 57.000 | 11.000 | 11.000 | 16.000 | 11.000 |
| C++ | 2.200 | 0.431 | 1.295 | 0.218 | 0.433 |

Workloads are a 10,000,000-iteration integer sum, 1,000,000 alternating branches, 2,000,000 float64 additions,
1,000,000 calls to `mix(x)=x*17`, and 1,000,000 reads cycling through `[1,2,3,4]`.
Checksums are respectively `49999995000000`, `-500000`, `1000000`, `8499991500000`, `2500000`.
All bounds are runtime inputs and every sample is checked. Two rounds × five workloads × seven configurations
× seven samples give **490 measured runs**, excluding warmups.

Both rounds and all samples remain available; small percentage changes are not automatically claimed as speedups. Each float VM loop has 30,000,030 logical steps.

## Code examples and reasons

See [all 30 complete test sources](examples/vm-performance.md), or the
[Chinese report](../12-vm-performance.md) for inline integer-loop examples in each language.
The same ChaLi source is compiled by both versions; the same Java source/class runs with and without JIT.

- HotSpot's diagnostic log confirms `Case_calls::mix (6 bytes) inline (hot)`. The C++ loop has no call to
  `mix` and advances an induction variable by 17. The calls workload therefore permits normal compiler
  optimization and does not measure a forced function-call cost.
- HotSpot with JIT and native C++ execute optimized native loops; `-Xint` interprets Java bytecode instead.
  The JIT measurement follows 20 complete warmups and represents hot execution, not cold startup.
- ChaLi's integer region core batches internal instructions but is not a native-code JIT. Its new float
  fusion reduces dispatch while preserving steps and rounding; floats still use slots and runtime checks.
- CPython's warmed float loop shows `BINARY_OP_ADD_FLOAT`. Static typing alone does not eliminate ChaLi's
  slot, normalization and interpreter-service overhead; Python is faster in some of these workloads.
- ChaLi calls retain parameter binding and frame services; Lua bytecode also retains `CALL`. ChaLi arrays
  use bounds/pointer-capacity checks, while Lua tables, Python lists, Java arrays and C++ arrays have different
  representations and checking semantics. C++ source has no per-access runtime array bounds check.

[Diagnostic evidence](../benchmarks/vm-baselines-evidence-0.18.3.json) was collected after timing, separately
from measured runs. It includes inlining logs, C++ assembly, adaptive Python bytecode and Lua bytecode.
There is no sampling profile, so these mechanisms are not presented as quantified shares of total time.

## Method and reproduction

Every configuration excludes process startup, compilation and loading. ChaLi `steady_clock` timing includes
execution preparation and checksum output. These boundaries are disclosed,
not corrected using estimated overhead. Lua has coarse Windows timing resolution, and sub-millisecond
native/JIT samples should be interpreted with the full raw samples.

One process produces one measured sample. The script rotates configuration order over seven repeats, runs serially, and completes all builds before
measurement. Two independent rounds are retained separately, using medians rather than best samples.

Build both version-specific VM runners using their own headers and archives:

```sh
g++ -std=c++17 -O2 -DNDEBUG -DSTACKVM_WITH_REFLECTION=1 -DCHALI_CUSTOM_ALLOCATOR=1 -Ilibrary -Ilibrary/stackvm tests/benchmark_vm_program.cpp build/chali-stackvm/libstackvm.a -pthread -o build/vm-program-benchmark.exe
```

Use [benchmark_vm_lua.py](../../tests/benchmark_vm_lua.py) first to download/build official Lua 5.5.1.
The new script verifies the archive SHA-256 again:
`1c4b4068d67061f2a2231ad2b5422e77acea1487ea9890f6320af614f4373dce`.
The [previous seven-configuration dataset](../benchmarks/vm-baselines-0.18.3.json) is archived separately.
Earlier Lua-only [first](../benchmarks/vm-lua-0.18.3.json) and [repeat](../benchmarks/vm-lua-0.18.3-repeat.json)
rounds and the [dispatch comparison](../benchmarks/register-dispatch-0.18.3.json) remain archives.
They are not mixed into this report. Results describe these workloads, not a general language ranking or
timing on physical embedded Linux hardware.
