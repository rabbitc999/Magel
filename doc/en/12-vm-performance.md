# VM performance: a cross-language measurement of 0.28.3

This page measures the **current version (0.28.3)** on one machine against four execution
technologies: native code (C / C++ / Rust / Go), just-in-time compilation (Java `-Xbatch`, Node),
bytecode interpretation (Java `-Xint`) and pure interpretation (CPython). Every number is produced by
[`benchmarks/perf/run.py`](../../benchmarks/perf/run.py); the sources are in
[`benchmarks/perf/`](../../benchmarks/perf/), raw samples in
[`benchmarks/results/`](../../benchmarks/results/).

- **Integer loop**: **17.8×** C, level with the JVM interpreter (76.8 ms vs 72.1 ms).
- **Float loop**: **313×** C, **25× slower** than the JVM interpreter. In 0.28.3 the float loop no
  longer qualifies for the register region core (419.6 ms with the core off, same as the default
  run), so the whole path falls back to generic dispatch.
- **Calls**: **406×** C and **3.0× slower than CPython** — the call entry is the most expensive path.
- **Containers**: **905×** C and **16× slower than CPython** — `push` and indexed reads both go
  through the container service.
- **Control group**: Java `-Xbatch` stays within 0.9–1.8× of native, Node matches C on floats (1.0×),
  CPython sits at 27–137×.

ChaLi's position is "**faster than CPython, slower than the JVM interpreter**": integer arithmetic has
caught up; floats, calls and containers have not. Floats are a new 0.28.3 regression and calls are
about 10% slower than 0.28.2 (see §6); optimizing them is scheduled for the next version.

## 1. Environment and nine configurations

| Item | Value |
| --- | --- |
| Date | 2026-10-23, Asia/Shanghai |
| System / CPU / RAM | Windows 10 10.0.19045 x64; AMD Ryzen 5 5600X (6C/12T); 32 GB |
| ChaLi | 0.28.3, GCC/MinGW 11.2.0 `-O2 -DNDEBUG`, reflection on, all default optimizations (region core + machine passes + slot reuse) |
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
| **ChaLi 0.28.3 (StackVM)** | **76.824** | **408.129** | **350.778** | **342.008** | **892.667** |
| C (gcc -O2) | 4.325 | 1.303 | 0.865 | 6.940 | 0.986 |
| C++17 (g++ -O2) | 4.317 | 1.300 | 0.931 | 6.909 | 0.969 |
| Rust (rustc -O) | 0.873 | 1.307 | 1.082 | 6.216 | 0.960 |
| Go (default) | 4.675 | 1.551 | 1.038 | 7.285 | 1.543 |
| Java 22 `-Xint` | 72.133 | 16.250 | 31.988 | 71.466 | 14.645 |
| Java 22 `-Xbatch` | 4.992 | 1.309 | 1.081 | 6.191 | 1.810 |
| Node 26 (V8) | 71.766 | 1.297 | 15.940 | 59.382 | 9.535 |
| CPython 3.12 | 321.443 | 29.464 | 118.190 | 191.030 | 55.350 |

Ratio to C (same column divided by the C median):

| Configuration | int_chain | float_chain | call | array | list |
| --- | ---: | ---: | ---: | ---: | ---: |
| ChaLi 0.28.3 | 17.8 | 313.2 | 405.5 | 49.3 | 905.4 |
| C++17 | 1.0 | 1.0 | 1.1 | 1.0 | 1.0 |
| Rust | 0.2 | 1.0 | 1.3 | 0.9 | 1.0 |
| Go | 1.1 | 1.2 | 1.2 | 1.0 | 1.6 |
| Java `-Xint` | 16.7 | 12.5 | 37.0 | 10.3 | 14.9 |
| Java `-Xbatch` | 1.2 | 1.0 | 1.3 | 0.9 | 1.8 |
| Node 26 | 16.6 | 1.0 | 18.4 | 8.6 | 9.7 |
| CPython 3.12 | 74.3 | 22.6 | 136.6 | 27.5 | 56.1 |

Per iteration for ChaLi: integers **19.2 ns**, floats **408 ns**, calls **351 ns**, arrays **86 ns**,
one container element (one `push` plus one indexed read) about **2.2 µs**.

## 4. Reading the columns

**Native** (C, C++, Rust, Go) agree within about 20% and serve as the machine's floor.

**JIT** (Java `-Xbatch`, Node) reaches native once warm: Java lands at 0.9–1.8× everywhere, Node
matches C on floats. Node's integer column is 16.6× because JavaScript numbers are doubles: once
`acc * 48271` passes 2^31 it cannot use 32-bit integer instructions, and `&` would truncate to 32 bits
and break the checksum, so the loop pays for a double modulo. That is a language-semantics cost, not
a slow V8.

**Bytecode interpretation** (ChaLi, Java `-Xint`) is the like-for-like comparison. ChaLi is level on
integers (76.8 vs 72.1) and 4.8× slower on arrays, but 25× slower on floats, 11× slower on calls and
61× slower on containers.

**Pure interpretation** (CPython): ChaLi leads on integers (4.2×) but trails on floats (13.9×),
calls (3.0×), arrays (1.8×) and containers (16×). Integer arithmetic is the only advantage left;
the call and container gaps mean the bottleneck is not "interpretation" but how those two paths
are implemented.

## 5. Attribution and evidence

### 5.1 What the region core buys

Same programs with `CHALI_NO_REGISTER_CORE=1` (back to generic dispatch):

| Workload | Default | Region core off | Ratio |
| --- | ---: | ---: | ---: |
| int_chain | 76.8 ms | 1936.7 ms | 25.2× |
| array | 342.0 ms | 3591.3 ms | 10.5× |
| call | 350.8 ms | 874.2 ms | 2.5× |
| list | 892.7 ms | 1400.1 ms | 1.6× |
| float_chain | 408.1 ms | 419.6 ms | 1.0× |

Integer and array loops take almost all their speed from batching (25×, 10×). **Floats are 1.0× —
the 0.28.3 float loop no longer qualifies for the region core**, which is the main cause of the
regression described in §6.

### 5.2 Floats: the whole loop fell out of the region core

In 0.28.2 the float loop still benefited from the region core (turning it off cost 1.5×); in 0.28.3
turning it off changes nothing (408 → 420 ms), so floats now run the generic path end to end: slot
lookup, type normalization and boxing per operation in `source/stackvm/vm_execution.cpp`. Against C
the gap is 313× (408 ns vs 1.3 ns per operation). Reconnecting float loops to the region core is the
goal for the next version.

### 5.3 Calls: 351 ns each

`Op::Call` handles a pending argument list, name-based parameter binding, a new frame and return-value
delivery. CPython's dedicated call opcodes do the same job in 118 ns and the JVM interpreter in 32.0 ns,
so 351 ns is the cost of this chain, not of interpretation as such.

### 5.4 Containers: the most expensive path

One `list` iteration does one `push` plus one indexed read: 892.7 ms / 400,000 iterations ≈ 1.1 µs
per container operation (800,000 in total). Both go through the container service in
`source/glue/std_collection.cpp` (object header, growth check, value normalization, name-based
dispatch). The C version does 800,000 operations in 0.986 ms, roughly 1.2 ns each. This is the most
valuable thing to fix next: the VM is not slow, the per-operation container service is.

## 6. The 0.28.3 regression: the price of recursion correctness

0.28.3 fixed a recursive-computation bug (compiler temporaries shared one storage across recursive
activations, so an inner call overwrote values pending in outer frames). The fix puts compiler
temporaries into the frame-slot allocation: every activation now prepares slots for temporaries,
declare and store paths gained scratch branches, and dense layouts grew. Two paths got slower:

| Workload | 0.28.2 (control run) | 0.28.3 | Change |
| --- | ---: | ---: | ---: |
| float_chain | 259.0 ms | 408.1 ms | **+58%** |
| call | 331.7 ms | 350.8 ms | +6% |
| int_chain | 81.2 ms | 76.8 ms | unchanged |
| array | 345.8 ms | 342.0 ms | unchanged |
| list | 941.5 ms | 892.7 ms | unchanged |

The control run re-ran the 0.28.2 binary on the same day and machine (`--only chali --runs 5`),
not the old high-load page.

The +58% on floats is not "each float operation got pricier" — **the whole float loop fell out of the
region core**: after temporaries joined the slot layout, the §5.1 comparison shows floats unchanged
with the core off, i.e. the default run already sits on the generic path. In 0.28.2 floats ran inside
the core (≈259 ms); 0.28.3 falls back to the generic path (≈420 ms), matching the 0.28.2 core-off
level (364.8 ms) plus this round's extra frame preparation.

Correctness first: these regressions are accepted, and the optimizations (re-qualifying float loops
for the region core, slimming call-frame preparation) are scheduled for the next version.

## 7. Method and reproduction

Every configuration times only the `work(seed)` call with its own monotonic clock
(`clock_gettime` / `steady_clock` / `Instant` / `System.nanoTime` / `hrtime.bigint` /
`perf_counter_ns` / `std::clock::nowNanos`). Process startup, compilation and loading are excluded.
One process does one warmup plus five timed rounds; the driver repeats three processes and takes the
median of 15 samples, never the minimum.

```sh
python benchmarks/perf/run.py                 # all nine configurations
python benchmarks/perf/run.py --only chali,c  # ChaLi and C only
python benchmarks/perf/run.py --runs 5        # five processes per configuration
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
- Native and JIT samples are often sub-millisecond (C's call column is 0.87 ms) and are more sensitive
  to scheduling; read medians, not individual digits.
