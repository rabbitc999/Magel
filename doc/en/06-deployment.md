# 06 Cross-platform deployment

One Magel logic, three ways to run it. This document covers how to choose, how to configure, and how
to connect embedded boards and host programs.

---

## 1. Three execution modes

| Mode | Command | Startup | Needs a runtime | Good for |
| --- | --- | --- | --- | --- |
| **Interpreted** | `magel run app.mgl` | compiles every run | the Magel toolchain | development, scripts, tools |
| **Bytecode + StackVM** | `magel svmb …` → loaded by the VM | fast | StackVM | delivery, embedded, program image updates |
| **AOT native program** | `magel aot …` | No VM loading; actual startup depends on the program | Runtime services compiled in | Native delivery, performance-sensitive code |

### Interpreted

```sh
magel run src/main.mgl
```

Preprocess → parse and check → lower to quadruples → run, all in one process. Edit and see the result
immediately.

### Bytecode

```sh
magel svmb src/main.mgl --out build/app
```

Produces `build/app.svmb` for StackVM to load. Choosing and hardening the delivery form:
[Delivery security](05-delivery-security.md).

### AOT native program

AOT also produces `.dll` / `.so` shared libraries or relocatable `.o` files with a C header.
Library source may omit `main` and must declare at least one public `export`. Static archives
are unsupported. See [library output](09-toolchain.md#aot-library-output) for target and linking rules.

```sh
magel aot src/main.mgl --out build/app --opt-level=2
```

Generates C++ and compiles it into a native program for the target platform. The artifact keeps
the name given to `--out`; on Windows a name without an extension becomes `build/app.exe`,
since the system only recognises executables by extension. The default is `checked`, enabling optional allocation guards and panic traces. Explicit
`unchecked` disables these optional checks; array bounds checks are still retained. An explicit checked build:

```sh
magel aot src/main.mgl --out build/app checked opt 2
```

| Option | Effect |
| --- | --- |
| `--out path` | artifact prefix |
| `opt LEVEL` / `--opt-level=N` | optimisation level |
| `checked` / `unchecked` | keep runtime checks or not (default `checked`) |
| `--separate-modules` | cache per-module native object files and link once (faster increments) |
| `--module-cache DIR` | directory for module object files |

---

## 2. Desktop

All three modes work. Distribution guidance:

- Internal tools, updated often → bytecode + VM;
- End users, startup speed matters → AOT;
- Cross-platform without maintaining several builds → bytecode (one `.svmb` runs on the VM on every
  platform).

The default runtime memory pool is **512 MiB**; adjust with `--memory-size bytes` (minimum 64).

---

## 3. Embedded profile and standalone VM

The front-end profile is configured in magel.toml:

```toml
[embedded]
enabled = true
restrict_modules = ["std::math"]     # 在内置禁用之外再追加
max_memory = 1048576                 # 运行内存上限（字节）
```

This profile rejects std::net, std::process, std::sql, std::io, std::thread, std::coroutine and
std::concurrency. Additional restrictions only extend the list. It controls compile-time imports
and the memory ceiling, with command-line precedence.

The standalone [MCU host](../../source/stackvm_mcu/README.md) is another consumer of the same VM core,
built without exceptions, RTTI and OS threads, with injected pools, filesystems and board glue.
The front-end blocklist does not describe every standalone host's filesystem/coroutine support,
and a smaller memory ceiling alone does not supply platform adapters.

The [H743 project](../../example/mcu/FK-STM32H743XIH6/README.md) integrates SDRAM, FatFS, USART1,
LCD and keys with 2/16/4 MiB pools. Compile with `--no-reflection` for its default VM.
Shared capabilities preserve language semantics; modules, feature bits, glue and budgets must match the target.

---

## 4. Embedding into a host program

AOT output carries a directly callable entry point:

```cpp
static int runMagel(int argumentCount, const char *const *arguments,
                    void *buffer, std::size_t memoryBytes);
```

| Parameter | Meaning |
| --- | --- |
| `argumentCount` / `arguments` | arguments passed to Magel's `main` (program name excluded) |
| `buffer` | runtime memory buffer; pass `nullptr` to let the program allocate |
| `memoryBytes` | buffer size, i.e. the runtime memory ceiling |

The generated file wraps `int main(...)` in `#ifndef MAGEL_NO_MAIN` — **define `MAGEL_NO_MAIN` and
compile, and only `runMagel` remains for your firmware to call**, with no `main` of its own.

The static entry has translation-unit linkage for hosts integrating the generated C++. Use AOT `export` and its generated header for a C interface.
AOT logic changes require recompilation. Replacing bytecode updates a VM program image, without live migration of stacks or objects.

Full glue bindings, type mapping and `abi` blocks: [Host integration and C ABI](08-host-integration.md).

---

## 5. Memory and watchdog

| Option | Effect |
| --- | --- |
| `--memory-size bytes` | runtime heap capacity (default 512 MiB, minimum 64) |
| `--watchdog` | enable the soft-restart watchdog |
| `--no-watchdog` | disable (default) |
| `--max-restarts N` | how many soft restarts are allowed (default 3, implies `--watchdog`) |
| `--watchdog-watermark N` | memory watermark in bytes (implies `--watchdog`) |
| `--step-limit N` | cap on instruction count for this run |

The watchdog re-runs the program on **allocation failure / explicit request / watermark breach /
unfed timeout** rather than letting the process die. Long-running services can use it to contain a one-off fault
within "restart the logic once" — see `std::watchdog`.

---

## 6. Server side

Magel can be the server itself: `std::net` listens for HTTP, `std::sql` talks to a real SQLite file,
and the browser calls same-origin JSON endpoints — no web framework needed.

A complete runnable example: [`example/web-students/`](../../example/web-students/) (student records:
create / read / update / delete plus filtering).

Same-origin is only one arrangement. A **split front end and back end** works too: the back end
serves only `/api/*` JSON (pages return 404), the front end is a separate set of static assets hosted
by another process, and the two talk over CORS — see
[`example/sql-magel/example/ipage-student-server/`](../../example/sql-magel/example/ipage-student-server)
(a real MySQL database, login sessions, four roles). The trade-offs are in
[Example projects §4](13-projects.md#4-web-and-server-side).

Note: under embedded mode `std::net` and `std::sql` are blocked, so do not use `[embedded]` for server
scenarios.

---

## 7. Choosing at a glance

| Scenario | Recommendation |
| --- | --- |
| Development | interpreted |
| Desktop delivery to end users | AOT (`checked` or `unchecked` as performance requires) |
| Source must stay hidden | compressed bytecode or AOT — [Delivery security](05-delivery-security.md) |
| MCU / board | bytecode + a standalone MCU host using the shared core |
| Business logic called from firmware | AOT + `MAGEL_NO_MAIN` + `runMagel` |
| Long-running service | bytecode or AOT + `--watchdog` |

---

## 8. Next

| You want | Where |
| --- | --- |
| Harden the artifact | [Delivery security](05-delivery-security.md) |
| Interoperate with C/C++ | [Host integration and C ABI](08-host-integration.md) |
| All command-line options | [Toolchain](09-toolchain.md) |
| The board-oriented VM host | [`source/stackvm_mcu/`](../../source/stackvm_mcu) |
