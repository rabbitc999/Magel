# VM performance: a cross-language measurement of 0.28.2

This page measures the **current version (0.28.2)** on one machine against four execution
technologies: native code (C / C++ / Rust / Go), just-in-time compilation (Java `-Xbatch`, Node),
bytecode interpretation (Java `-Xint`) and pure interpretation (CPython). Every number is produced by
[`benchmarks/perf/run.py`](../../benchmarks/perf/run.py); the sources are in
[`benchmarks/perf/`](../../benchmarks/perf/), raw samples in
[`benchmarks/results/`](../../benchmarks/results/).

- **Integer loop**: **17.4×** C, and level with the JVM interpreter (75.1 ms vs 80.2 ms).
- **Float loop**: **192×** C, **15× slower** than the JVM interpreter. Turning the region core off
  costs only 1.5× here, so the cost sits in per-operation slot and type handling, not in dispatch.
- **Calls**: **350×** C and **2.7× slower than CPython** — the call entry is the most expensive path.
- **Containers**: **917×** C and **16× slower than CPython** — `push` and indexed reads both go
  through the container service.
- **Control group**: Java `-Xbatch` stays within 0.9–1.6× of native, Node matches C on floats (1.0×),
  CPython sits at 23–129×.

ChaLi's position is "**faster than CPython, slower than the JVM interpreter**": integer arithmetic has
caught up; floats, calls and containers have not.

## 1. Environment and nine configurations

| Item | Value |
| --- | --- |
| Date | 2026-10-23, Asia/Shanghai |
| System / CPU / RAM | Windows 10 10.0.19045 x64; AMD Ryzen 5 5600X (6C/12T); 32 GB |
| ChaLi | 0.28.2, GCC/MinGW 11.2.0 `-O2 -DNDEBUG`, reflection on, all default optimizations (region core + machine passes + slot reuse) |
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
   loop and reuses one result (our first run measured 0.000 ms for the C array case).
3. **Data-dependent array index** — `a[i & 15]` folds to a constant; `a[acc & 15]` must be fetched.

All nine configurations produce **byte-identical checksums** on all five workloads; the driver treats a
mismatch as a hard failure. That is the only evidence that they compute the same amount of work.

## 3. Results

Milliseconds, lower is better. Each cell is the median of 15 samples (3 processes × 5 timed rounds);
each process warms up once with `seed=0` first.

| Configuration | int_chain | float_chain | call | array | list |
| --- | ---: | ---: | ---: | ---: | ---: |
| **ChaLi 0.28.2 (StackVM)** | **75.056** | **250.577** | **309.853** | **335.767** | **891.680** |
| C (gcc -O2) | 4.316 | 1.305 | 0.886 | 6.906 | 0.972 |
| C++17 (g++ -O2) | 4.330 | 1.299 | 0.876 | 6.908 | 0.956 |
| Rust (rustc -O) | 0.865 | 1.299 | 1.084 | 6.224 | 0.994 |
| Go (default) | 4.625 | 1.541 | 1.036 | 7.234 | 1.074 |
| Java 22 `-Xint` | 80.155 | 16.450 | 31.106 | 86.213 | 14.304 |
| Java 22 `-Xbatch` | 4.998 | 1.303 | 1.083 | 6.168 | 1.512 |
| Node 26 (V8) | 71.932 | 1.301 | 15.970 | 59.254 | 8.897 |
| CPython 3.12 | 313.019 | 29.805 | 115.492 | 189.975 | 54.695 |

Ratio to C (same column divided by the C median):

| Configuration | int_chain | float_chain | call | array | list |
| --- | ---: | ---: | ---: | ---: | ---: |
| ChaLi 0.28.2 | 17.4 | 192.0 | 349.7 | 48.6 | 917.4 |
| C++17 | 1.0 | 1.0 | 1.0 | 1.0 | 1.0 |
| Rust | 0.2 | 1.0 | 1.2 | 0.9 | 1.0 |
| Go | 1.1 | 1.2 | 1.2 | 1.0 | 1.1 |
| Java `-Xint` | 18.6 | 12.6 | 35.1 | 12.5 | 14.7 |
| Java `-Xbatch` | 1.2 | 1.0 | 1.2 | 0.9 | 1.6 |
| Node 26 | 16.7 | 1.0 | 18.0 | 8.6 | 9.2 |
| CPython 3.12 | 72.5 | 22.8 | 130.4 | 27.5 | 56.3 |

Per iteration for ChaLi: integers **18.8 ns**, floats **251 ns**, calls **310 ns**, arrays **84 ns**,
one container element (one `push` plus one indexed read) about **2.2 µs**.

## 4. Reading the columns

**Native** (C, C++, Rust, Go) agree within about 20% and serve as the machine's floor.

**JIT** (Java `-Xbatch`, Node) reaches native once warm: Java lands at 0.9–1.6× everywhere, Node
matches C on floats. Node's integer column is 16.6× because JavaScript numbers are doubles: once
`acc * 48271` passes 2^31 it cannot use 32-bit integer instructions, and `&` would truncate to 32 bits
and break the checksum, so the loop pays for a double modulo. That is a language-semantics cost, not
a slow V8.

**Bytecode interpretation** (ChaLi, Java `-Xint`) is the like-for-like comparison. ChaLi is level on
integers (75.1 vs 80.2) and 3.9× faster on arrays, but 15× slower on floats, 10× slower on calls and
62× slower on containers.

**Pure interpretation** (CPython): ChaLi leads on integers (4.2×), floats (8.4×) and arrays (1.8×),
yet trails on calls (0.37×, i.e. 2.7× slower) and containers (0.06×, i.e. 16× slower). A statically
typed stack VM losing two basic operations to CPython means the bottleneck is not "interpretation" but
how those two paths are implemented.

## 5. Attribution and evidence

### 5.1 What the region core buys

Same programs with `CHALI_NO_REGISTER_CORE=1` (back to generic dispatch):

| Workload | Default | Region core off | Ratio |
| --- | ---: | ---: | ---: |
| int_chain | 75.1 ms | 1674.5 ms | 22.3× |
| array | 327.0 ms | 3224.4 ms | 9.9× |
| call | 306.7 ms | 717.5 ms | 2.3× |
| float_chain | 248.9 ms | 364.8 ms | 1.5× |
| list | 872.2 ms | 1245.5 ms | 1.4× |

Integer and array loops take almost all their speed from batching (22×, 10×). Floats and containers
gain only 1.4–1.5×, so their cost lives elsewhere.

### 5.2 Floats: service cost, not dispatch

Disabling the region core changes floats by only 1.5×, while the gap to C is 192× (251 ns vs 1.3 ns
per operation). Float operations run through slot lookup, type normalization and boxing on the generic
path in `source/stackvm/vm_execution.cpp`; integers are batched, floats are still served one by one.

### 5.3 Calls: 310 ns each

`Op::Call` handles a pending argument list, name-based parameter binding, a new frame and return-value
delivery. CPython's dedicated call opcodes do the same job in 115 ns and the JVM interpreter in 31.1 ns,
so 310 ns is the cost of this chain, not of interpretation as such.

### 5.4 Containers: the most expensive path

Split into two measurements (400,000 elements): pushing alone **455 ms**, push plus indexed reads
**903 ms** — about **1.1 µs** per `push` and per indexed read. Both go through the container service in
`source/glue/std_collection.cpp` (object header, growth check, value normalization, name-based
dispatch). The C version does 800,000 operations in 0.95 ms, roughly 1.2 ns each. This is the most
valuable thing to fix next: the VM is not slow, the per-operation container service is.

### 5.5 Why these numbers are not comparable with the 0.18.3 page

The older page summed `total += i`, which both compilers and the VM can reduce with induction
variables; this page uses a real dependency chain that nobody gets for free. Different workloads on a
different version do not compare, so the old table is gone rather than left to imply a trend.

## 6. Method and reproduction

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

## 7. Limitations

- Five kernels (loop arithmetic, dependency chain, calls, fixed arrays, dynamic arrays) do not cover
  strings, I/O, concurrency or reflection, and are not a language ranking.
- No sampling profile was taken, so §5.1–5.4 report mechanisms and switch comparisons, not quantified
  shares of total time.
- One platform (Windows + MinGW) and one CPU. Absolute numbers will move on Linux/GCC or ARM; the
  relative position is expected to hold but was not measured.
- Native and JIT samples are often sub-millisecond (C's call column is 0.87 ms) and are more sensitive
  to scheduling; read medians, not individual digits.
