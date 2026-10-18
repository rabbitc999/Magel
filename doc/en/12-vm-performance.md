# VM performance: VM, AOT, JVM, Python, Lua and C++

This report retains historical measurements of Magel 0.18.3 / 0.18.2 on 2026-10-15, on Windows 10 10.0.19045 x64,
Ryzen 5 5600X (6 cores / 12 threads), 32 GB RAM. JVM is split into **without just-in-time compilation**
(`-Xint`) and **with just-in-time compilation** (HotSpot `-Xbatch`), with two version-specific AOT configurations, giving nine configurations.

These measurements are not a rerun of the current VERSION; see [current capabilities](17-capabilities.md).

## Configuration and results

Both Magel versions use their own compiler and bytecode, reflection enabled and all default IR/VM optimizations.
Their VM runners use GCC/MinGW 11.2.0 `-O2 -DNDEBUG`. JVM uses Oracle Java 22.0.1+8-16, CPython is 3.12.3,
and Lua is official 5.5.1 built with the MinGW `-O2` target without JIT. C++ is a directly compiled C++17
program (`-O2 -DNDEBUG`), **not Magel AOT**. Each Magel version also builds its own AOT executable with `--opt-level=2 --unchecked --keep-intermediate`. No handwritten closed-form sums are used.

The following are second-round medians in milliseconds. Each round has seven samples per configuration.

| Configuration | Integer sum | Branches | float64 sum | Calls | Array reads |
| --- | ---: | ---: | ---: | ---: | ---: |
| Magel 0.18.3 VM | 135.138 | 33.099 | 353.936 | 609.619 | 85.360 |
| Magel 0.18.2 VM | 134.522 | 32.659 | 491.668 | 633.749 | 84.069 |
| Magel 0.18.3 AOT | 2.170 | 0.439 | 1.300 | 0.227 | 36.900 |
| Magel 0.18.2 AOT | 2.170 | 0.440 | 1.300 | 0.439 | 33.500 |
| JVM without JIT (-Xint) | 145.578 | 22.043 | 25.071 | 26.915 | 16.734 |
| JVM with JIT (-Xbatch) | 2.302 | 0.377 | 1.299 | 0.231 | 0.479 |
| Python | 400.934 | 61.285 | 71.810 | 72.427 | 51.250 |
| Lua | 57.000 | 11.000 | 11.000 | 16.000 | 11.000 |
| C++ | 2.200 | 0.431 | 1.295 | 0.218 | 0.433 |

Workloads are a 10,000,000-iteration integer sum, 1,000,000 alternating branches, 2,000,000 float64 additions,
1,000,000 calls to `mix(x)=x*17`, and 1,000,000 reads cycling through `[1,2,3,4]`.
Checksums are respectively `49999995000000`, `-500000`, `1000000`, `8499991500000`, `2500000`.
All bounds are runtime inputs and every sample is checked. Two rounds × five workloads × nine configurations
× seven samples give **630 measured runs**, excluding warmups.

The VM and AOT version deltas are reported separately in the [Chinese report](../12-vm-performance.md).
Both rounds and all samples remain available; small percentage changes are not automatically claimed as speedups.
Each float VM loop has 30,000,030 logical steps. AOT measures native execution, not VM instruction dispatch.

## Code examples and reasons

See [all 30 complete test sources](examples/vm-performance.md), or the
[Chinese report](../12-vm-performance.md) for inline integer-loop examples in each language.
The same Magel source is compiled by both versions; the same Java source/class runs with and without JIT.

- HotSpot's diagnostic log confirms `Case_calls::mix (6 bytes) inline (hot)`. The C++ loop has no call to
  `mix` and advances an induction variable by 17. The calls workload therefore permits normal compiler
  optimization and does not measure a forced function-call cost.
- HotSpot with JIT and native C++ execute optimized native loops; `-Xint` interprets Java bytecode instead.
  The JIT measurement follows 20 complete warmups and represents hot execution, not cold startup.
- Magel's integer region core batches internal instructions but is not a native-code JIT. Its new float
  fusion reduces dispatch while preserving steps and rounding; floats still use slots and runtime checks.
- CPython's warmed float loop shows `BINARY_OP_ADD_FLOAT`. Static typing alone does not eliminate Magel's
  slot, normalization and interpreter-service overhead; Python is faster in some of these workloads.
- Magel calls retain parameter binding and frame services; Lua bytecode also retains `CALL`. Magel arrays
  use bounds/pointer-capacity checks, while Lua tables, Python lists, Java arrays and C++ arrays have different
  representations and checking semantics. C++ source has no per-access runtime array bounds check.

[Diagnostic evidence](../benchmarks/vm-baselines-evidence-0.18.3.json) was collected after timing, separately
from measured runs. It includes inlining logs, C++ assembly, adaptive Python bytecode and Lua bytecode.
There is no sampling profile, so these mechanisms are not presented as quantified shares of total time.

Both AOT versions run compiled native code rather than dispatching VM instructions. Generated scalar code
contains direct control flow and normalization helpers; array code retains explicit `rt.checkIndex`,
`rt.checkPointer` and memory services. `--unchecked` disables optional allocation guards and panic traces,
not every explicit bounds check. The C++ baseline remains a separate hand-written native program.
The first three AOT cases differ by less than 1% between versions. Calls take roughly 45%–48% less time
in 0.18.3, but both hotspot loops have the same `add / add 17 / cmp / jne` instructions and no call to `mix`.
They have different addresses/alignment; layout or other fixed overhead remains a hypothesis, not a quantified cause.
Array reads take about 10% more time in 0.18.3 across both rounds, a regression signal retained for investigation.
[AOT build evidence](../benchmarks/vm-baselines-aot-evidence-0.18.3.json) records version-specific runtime
markers, source hashes, flags and generated loop/check snippets, collected after timing.

## Method and reproduction

Every configuration excludes process startup, compilation and loading. Magel `steady_clock` timing includes
execution preparation and checksum output. JVM `nanoTime`, Python `perf_counter_ns`, C++ `steady_clock`,
Lua `os.clock`, and Magel AOT `std::clock::nowNanos` enclose the calculation; their output is outside timing. These boundaries are disclosed,
not corrected using estimated overhead. Lua has coarse Windows timing resolution, and sub-millisecond
native/JIT samples should be interpreted with the full raw samples.

One process produces one measured sample. Non-JIT configurations, including both AOT programs, warm once; HotSpot JIT warms 20 times.
The script rotates configuration order over seven repeats, runs serially, and completes all builds before
measurement. Two independent rounds are retained separately, using medians rather than best samples.

Build both version-specific VM runners using their own headers and archives:

```sh
g++ -std=c++17 -O2 -DNDEBUG -DSTACKVM_WITH_REFLECTION=1 -DMAGEL_CUSTOM_ALLOCATOR=1 -Ilibrary -Ilibrary/stackvm tests/benchmark_vm_program.cpp build/magel-stackvm/libstackvm.a -pthread -o build/vm-program-benchmark.exe
```

Use [benchmark_vm_lua.py](../../tests/benchmark_vm_lua.py) first to download/build official Lua 5.5.1.
The new script verifies the archive SHA-256 again:
`1c4b4068d67061f2a2231ad2b5422e77acea1487ea9890f6320af614f4373dce`.
The exact local command was:

```sh
python tests/benchmark_vm_baselines.py --magel magel.exe --runner tmp/optimization103/runner.exe --baseline-magel tmp/optimization103/magel-0.18.2.exe --baseline-runner tmp/optimization103/baseline-runner.exe --repeats 7 --rounds 2 --reuse-builds --output doc/benchmarks/vm-baselines-aot-0.18.3.json
```

The script builds ten AOT executables, using compiler copies and working directories outside the repository
to force each version's embedded runtime. This avoids an old exporter accidentally reading new disk assets.
Only the build phase uses two parallel jobs; timing starts after all builds finish. `--reuse-builds` verifies
source/compiler/toolchain/options/asset-mode fingerprints and executable hashes before reuse.
The AOT harness reads a fresh limit after warmup, times `calculate`, and compares warmup/result after timing.

Replace these local binary paths with your separately built v0.18.3 and v0.18.2 artifacts.
[benchmark_vm_baselines.py](../../tests/benchmark_vm_baselines.py) generates all input sources and binaries
under `build/vm-baselines/`, verifies results, and records versions, flags, steps, source hashes and binary hashes
in [the complete two-round dataset](../benchmarks/vm-baselines-aot-0.18.3.json).
This documentation update does not change the compiler binary or published v0.18.3 tag.

The [previous seven-configuration dataset](../benchmarks/vm-baselines-0.18.3.json) is archived separately.
Earlier Lua-only [first](../benchmarks/vm-lua-0.18.3.json) and [repeat](../benchmarks/vm-lua-0.18.3-repeat.json)
rounds and the [dispatch comparison](../benchmarks/register-dispatch-0.18.3.json) remain archives.
They are not mixed into this report. Results describe these workloads, not a general language ranking or
timing on physical MCU hardware.

## STM32H743 hardware measurements

The [FK-STM32H743XIH6 project](../../example/mcu/FK-STM32H743XIH6/README.md) records
DWT cycles on a physical 480 MHz Cortex-M7. The metadata, runtime and byte pools use
2 / 16 / 4 MiB of SDRAM. Clang builds the firmware with `-Os`; the shared VM retains
its highest default execution optimizations.

For the same 10,000-step integer recurrence, two boots produced VM medians of
**8.146217 ms** and **8.143513 ms**, approximately **391 cycles per iteration** or
**1.23 million iterations per second**. The current native C++ host callback produced
medians of **0.410529 ms** and **0.528200 ms**, giving ratios of **19.84** and **15.42**.
Its loop condition still reads a host argument, and the interval includes VM dispatch
and callback boundaries. It is not a bare C++ or AOT performance ceiling.

Both runs retain the slower first sample. Waiting until the self-test had finished
before attaching SWD did not remove that effect; its exact source has not been isolated.
VM dispatch, runtime values and control flow add work beyond the native machine loop.
SDRAM, caches, interrupts and host glue also contribute, without separate attribution
in this experiment. The [board report](../../example/mcu/FK-STM32H743XIH6/README.md)
contains code, firmware hashes and limitations; the raw records are
[the initial capture](../../example/mcu/FK-STM32H743XIH6/results/benchmark.json) and
[the isolated repeat](../../example/mcu/FK-STM32H743XIH6/results/benchmark-isolated.json).
These physical MCU times cannot be compared directly with desktop times as a hardware-independent ranking.

## Checked AOT and JDK comparison (2026-10-16)

Both Magel builds use `-O3 --checked`, reflection, and the full quadruple optimizer. The baseline is commit `12c4dc4`; the candidate is the current 1.0.0 working tree. Each kernel runs 1,000,000 iterations, with three independent process rounds and five measurements per round. The table reports the median of 15 measurements in milliseconds. Every checksum is verified. Startup, compilation and printing are excluded; array/list initialization and cleanup are included.

| Kernel | Before | After | Speedup | JDK 8 | JDK 22 |
| --- | ---: | ---: | ---: | ---: | ---: |
| Integer recurrence | 387.8156 | 138.2660 | 2.80× | 3.0202 | 3.4046 |
| Fixed array | 1046.6111 | 353.9385 | 2.96× | 0.7019 | 0.4898 |
| Heap array | 1301.1306 | 428.8107 | 3.03× | 0.7017 | 0.4881 |
| Container | 2941.7599 | 2115.1724 | 1.39× | 2.3144 | 3.5630 |
| Function calls | 883.2179 | 387.9722 | 2.28× | 3.0226 | 3.4214 |

The checked improvements combine typed scalar memory access, array success paths, per-frame native argument reuse, and reduced source-location string copying. Checks and panic traces remain enabled. Java uses Temurin 8u504-b01 and Oracle 22.0.1, with `-Xms256m -Xmx256m -Xbatch` and 20 warmup iterations per process; AOT uses one warmup. Java uses `long[64]` for both array kernels and `ArrayList<Long>` for the container, while Magel uses its own arrays and `List<int64>`. These are algorithm comparisons with different layouts and JIT behavior. Java remains substantially faster on these kernels; the results do not imply an overall language ranking.

[Raw measurements and hashes](../benchmarks/aot-optimization-2026-10-16.json); [benchmark runner](../../tests/benchmark_aot_optimization.py).
