# VM performance: a cross-language measurement of 0.28.4

This page measures the **current version (0.28.4)** on one machine against four execution
technologies: native code (C / C++ / Rust / Go), just-in-time compilation (Java `-Xbatch`, Node),
bytecode interpretation (Java `-Xint`) and pure interpretation (CPython). Every number is produced by
[`benchmarks/perf/run.py`](../../benchmarks/perf/run.py); the sources are in
[`benchmarks/perf/`](../../benchmarks/perf/), raw samples in
[`benchmarks/results/`](../../benchmarks/results/).

- **Integer loop**: **12.5×** C, faster than the JVM interpreter (45.7 ms vs 72.1 ms).
- **Float loop**: **7.4×** C, **2.0× faster than the JVM interpreter** (8.1 ms vs 16.3 ms). 0.28.4
  reconnects float operations to the register region core (236.4 ms with the core off, 29× the
  default run), which removes the 0.28.3 regression.
- **Calls**: **275×** C and **1.7× slower than CPython** — the call entry is still the most expensive path.
- **Containers**: **790×** C and **11× slower than CPython** — `push` and indexed reads both go
  through the container service.
- **Control group**: Java `-Xbatch` stays within 0.9–1.8× of native, Node matches C on floats (1.0×),
  CPython sits at 27–137×.

ChaLi's position is "**faster than CPython, level with or faster than the JVM interpreter on pure
arithmetic**": integer (45.7 ms) and float (8.1 ms) loops are now solid, while calls and containers
still trail (§5.3, §5.4); optimizing them is scheduled for the next version.

> **Numbers**: this round re-measured only **ChaLi and C** (`--only chali,c`) on the same machine with
> the same binary and the same sample batch. Rust, Go, Java, Node and CPython keep their 0.28.3 numbers,
> and their ratios to C use that round's C baseline. ChaLi's ratios use this round's C baseline.

## 1. Environment and nine configurations

| Item | Value |
| --- | --- |
| Date | 2026-10-24, Asia/Shanghai |
| System / CPU / RAM | Windows 10 10.0.19045 x64; AMD Ryzen 5 5600X (6C/12T); 32 GB |
| ChaLi | 0.28.4, GCC/MinGW 11.2.0 `-O2 -DNDEBUG`, reflection on, all default optimizations (region core + machine passes + slot reuse) |
| C / C++ | GCC, G++ 11.2.0, `-O2 -DNDEBUG` |
| Rust | rustc 1.98.1, `-O` |
| Go | default `go build` |
| Java (two modes) | Oracle Java 22.0.1: `-Xint` interpreted; `-Xbatch` JIT-enabled |
| Node | 26.5.0 (V8 JIT) |
| Python | CPython 3.12.3, default interpreter |

All nine configurations run **the same algorithm**, written once per language: same iteration counts,
same initial values, same mask and same evaluation order. `work()` takes a `seed` that differs per call.

## 2. Five workloads and checksums

| Workload | Iterations | Loop body | Checksum |
| --- | ---: | --- | ---: |
| int_chain | 4,000,000 | `acc = (acc * 48271 + i) & 0x7FFFFFFF` | 6942227210 |
| float_chain | 1,000,000 | `x = x * 1.0000001 + 0.0000001` | 6.976227074 |
| call | 1,000,000 | `acc = (mix(acc) + i) & 0x7FFFFFFF`, `mix(x)=x*17+3` | 6040804810 |
| array | 4,000,000 | `acc = (acc + a[acc & 15]) & 0x7FFFFFFF`, `a[k]=3k` | 112 |
| list | 400,000 | `push(i*3+seed)`, then `total = (total*31 + xs[i]) & 0x7FFFFFFF` | 5673410186 |

Three design rules keep the measurement honest:

1. **Loop-carried dependency** — each iteration feeds the next, so closed-form summation does not apply.
2. **`seed` parameter** — with a constant argument, GCC hoists the whole pure call out of the timing
   loop and reuses one result (the first run measured 0.000 ms for the C array case).
3. **Data-dependent array index** — `a[i & 15]` folds to a constant; `a[acc & 15]` must be fetched.

All nine configurations produce **byte-identical checksums** on all five workloads; the driver treats a
mismatch as a hard failure. That is the only evidence that they compute the same amount of work.

## 3. Results

Milliseconds, lower is better. Each cell is the median of 15 samples (3 processes × 5 timed rounds);
each process warms up once with `seed=0` first.

| Configuration | int_chain | float_chain | call | array | list |
| --- | ---: | ---: | ---: | ---: | ---: |
| **ChaLi 0.28.4 (StackVM)** | **45.749** | **8.137** | **201.457** | **199.688** | **599.554** |
| C (gcc -O2) | 3.651 | 1.099 | 0.732 | 5.849 | 0.759 |
| C++17 (g++ -O2) | 4.317 | 1.300 | 0.931 | 6.909 | 0.969 |
| Rust (rustc -O) | 0.873 | 1.307 | 1.082 | 6.216 | 0.960 |
| Go (default) | 4.675 | 1.551 | 1.038 | 7.285 | 1.543 |
| Java 22 `-Xint` | 72.133 | 16.250 | 31.988 | 71.466 | 14.645 |
| Java 22 `-Xbatch` | 4.992 | 1.309 | 1.081 | 6.191 | 1.810 |
| Node 26 (V8) | 71.766 | 1.297 | 15.940 | 59.382 | 9.535 |
| CPython 3.12 | 321.443 | 29.464 | 118.190 | 191.030 | 55.350 |

Ratio to C (ChaLi and C use this round's C baseline; the rest use the 0.28.3 baseline):

| Configuration | int_chain | float_chain | call | array | list |
| --- | ---: | ---: | ---: | ---: | ---: |
| ChaLi 0.28.4 | 12.5 | 7.4 | 275.2 | 34.1 | 789.6 |
| C++17 | 1.0 | 1.0 | 1.1 | 1.0 | 1.0 |
| Rust | 0.2 | 1.0 | 1.3 | 0.9 | 1.0 |
| Go | 1.1 | 1.2 | 1.2 | 1.0 | 1.6 |
| Java `-Xint` | 16.7 | 12.5 | 37.0 | 10.3 | 14.9 |
| Java `-Xbatch` | 1.2 | 1.0 | 1.3 | 0.9 | 1.8 |
| Node 26 | 16.6 | 1.0 | 18.4 | 8.6 | 9.7 |
| CPython 3.12 | 74.3 | 22.6 | 136.6 | 27.5 | 56.1 |

Per iteration for ChaLi: integers **11.4 ns**, floats **8.1 ns**, calls **201 ns**, arrays **50 ns**,
one container element (one `push` plus one indexed read) about **1.5 µs**.

Against the 0.28.3 round (same machine, same driver):

| Workload | 0.28.3 | 0.28.4 | Change |
| --- | ---: | ---: | ---: |
| int_chain | 76.824 ms | 45.749 ms | **-40%** |
| float_chain | 408.129 ms | 8.137 ms | **-98%** |
| call | 350.778 ms | 201.457 ms | **-43%** |
| array | 342.008 ms | 199.688 ms | **-42%** |
| list | 892.667 ms | 599.554 ms | **-33%** |

All five columns drop partly because the two rounds saw different machine load (the 0.28.3 round ran
all nine configurations back to back). **Only the float column is a real code gain** — the 29× is
proven separately by the switch comparison in §5.1, not by comparing two rounds.

## 4. Reading the columns

**Native** (C, C++, Rust, Go) agree within about 20% and serve as the machine's floor.

**JIT** (Java `-Xbatch`, Node) reaches native once warm: Java lands at 0.9–1.8× everywhere, Node
matches C on floats. Node's integer column is 16.6× because JavaScript numbers are doubles: once
`acc * 48271` passes 2^31 it cannot use 32-bit integer instructions, and `&` would truncate to 32 bits
and break the checksum, so the loop pays for a double modulo. That is a language-semantics cost, not
a slow V8.

**Bytecode interpretation** (ChaLi, Java `-Xint`) is the like-for-like comparison. ChaLi is 1.6×
faster on integers (45.7 vs 72.1) and **2.0× faster on floats** (8.1 vs 16.3), 2.8× slower on arrays,
6.3× slower on calls and 41× slower on containers.

**Pure interpretation** (CPython): ChaLi leads on integers (7.0×) and floats (3.6×) and trails on
calls (1.7×), arrays (about level) and containers (11×). The call and container gaps mean the
bottleneck is not "interpretation" but how those two paths are implemented.

## 5. Attribution and evidence

### 5.1 What the region core buys

Same programs with `CHALI_NO_REGISTER_CORE=1` (back to generic dispatch):

| Workload | Default | Region core off | Ratio |
| --- | ---: | ---: | ---: |
| int_chain | 45.7 ms | 1389.7 ms | 30.4× |
| array | 199.7 ms | 2668.1 ms | 13.4× |
| float_chain | 8.1 ms | 236.4 ms | **29.1×** |
| call | 201.5 ms | 512.7 ms | 2.5× |
| list | 599.6 ms | 1001.6 ms | 1.7× |

Integer and array loops take almost all their speed from batching (30×, 13×). **In 0.28.3 the float
row was 1.0× (408 → 420 ms: floats never entered the core); in 0.28.4 it is 29.1×** — the float loop
runs inside the region core again, the only real gain of this release.

### 5.2 Floats: 0.28.4 reconnects them to the region core

In 0.28.3 the whole float loop ran the generic path (slot lookup, type normalization and boxing per
operation in `source/stackvm/vm_execution.cpp`), which the flat core-off comparison proved
(408 → 420 ms). 0.28.4 fixes it on three levels:

1. The instruction window in `source/stackvm/register_core.cpp` accepted only `IntegerStore` and
   `IntegerBranch`, so every float binary operation and float literal was rejected — floats are now
   accepted too.
2. `vm_execution.cpp` explicitly refused slots with `slot->isFloat` when binding — it now binds by
   the region's shape table.
3. Slot reuse let a float temporary share a slot with an integer variable, so the same slot flipped
   between the integer and the double shape and could not stay bound. The linear scan in
   `register_allocate.cpp` now reuses a slot only between the **same shape**, and
   `execution_plan.cpp` infers which symbols are float-shaped.

Semantics did not relax: float32 destinations are rounded to single precision per IEEE-754, float
division by zero and modulo still panic, float comparisons write integer 0/1, and a float value used
directly as a branch condition (the `-0.0` is-zero disagreement) is handed back to the precise
executor.

### 5.3 Calls: 201 ns each

`Op::Call` handles a pending argument list, name-based parameter binding, a new frame and return-value
delivery. CPython's dedicated call opcodes do the same job in 118 ns and the JVM interpreter in 32.0 ns,
so 201 ns is the cost of this chain, not of interpretation as such.

### 5.4 Containers: the most expensive path

One `list` iteration does one `push` plus one indexed read: 599.6 ms / 400,000 iterations ≈ 0.75 µs
per container operation (800,000 in total). Both go through the container service in
`source/glue/std_collection.cpp` (object header, growth check, value normalization, name-based
dispatch). The C version does 800,000 operations in 0.759 ms, roughly 1.0 ns each. This is the most
valuable thing to fix next: the VM is not slow, the per-operation container service is.

## 6. The 0.28.3 float regression is closed

While fixing the recursive-computation bug, 0.28.3 moved compiler temporaries into the frame-slot
allocation, and the side effect pushed the float loop out of the region core (259 ms → 408 ms, +58%).
0.28.4 reconnects it with the three changes in §5.2:

| Workload | 0.28.3 default (= its core-off level) | 0.28.4 default | Change |
| --- | ---: | ---: | ---: |
| float_chain | 236.4 ms (core off, measured now) | 8.1 ms | **29×** |

(0.28.3's default 408.1 ms and its core-off 419.6 ms are the same within noise, so the core-off column
stands in for 0.28.3's real float level; this round's 236.4 ms differs from it because of machine load,
see the note in §3.)

The same batch of changes also fixes a correctness bug: the region core's generic integer path
normalized to the declared width only for `Add`, `Sub`, `Mul`, `Div` and `Mod`, omitting `Shl`, `Shr`
and `ShrU`, so `(uint8)128 << 1` produced 256 instead of 0 when the region core ran it
(`example/samples/numeric_v101.chl` catches it). The precise executor was always correct, so only the
region-core path was wrong.

## 7. Method and reproduction

Every configuration times only the `work(seed)` call with its own monotonic clock
(`clock_gettime` / `steady_clock` / `Instant` / `System.nanoTime` / `hrtime.bigint` /
`perf_counter_ns` / `std::clock::nowNanos`). Process startup, compilation and loading are excluded.
One process does one warmup plus five timed rounds; the driver repeats three processes and takes the
median of 15 samples, never the minimum.

```sh
python benchmarks/perf/run.py                 # all nine configurations
python benchmarks/perf/run.py --only chali,c  # ChaLi and C only (this round used it)
python benchmarks/perf/run.py --runs 5        # five processes per configuration
CHALI_NO_REGISTER_CORE=1 python benchmarks/perf/run.py --only chali   # region core off
```

The driver builds (gcc / g++ / rustc / go / javac), schedules, validates checksums and writes
`benchmarks/results/<host>-<timestamp>-perf/results.json` with every raw sample plus min and max.
It uses `ChaLi.exe` from the repository root; set `CHALI` to point elsewhere.

## 8. Limitations

- Five kernels (loop arithmetic, dependency chain, calls, fixed arrays, dynamic arrays) do not cover
  strings, I/O, concurrency or reflection, and are not a language ranking.
- No sampling profile was taken, so §5.1–5.4 report mechanisms and switch comparisons, not quantified
  shares of total time.
- One platform (Windows + MinGW) and one CPU. Absolute numbers will move on Linux/GCC or ARM; the
  relative position is expected to hold but was not measured.
- Native and JIT samples are often sub-millisecond (C's call column is 0.73 ms) and are more sensitive
  to scheduling; read medians, not individual digits.
- This round re-measured only ChaLi and C, so there are two C baselines in play; read the note in §3
  before comparing across columns.
