# 13 Example projects

The earlier documents explain *how to write* Magel. This one lists **what already runs in the
repository** — for each example: where it lives, which capability it shows off, how to run it, how
it is verified, and what it needs beforehand.

Every command is run **from the repository root** (on Windows the compiler is `magel.exe`; below it
is written `magel`).

---

## 1. At a glance

| Example | Location | What it demonstrates | Requires |
| --- | --- | --- | --- |
| [Minimal GUI](#31-minimal-gui-gui) | [`example/gui/`](../../example/gui) | Windows, widgets, and events delivered through the runtime poster | Windows + MinGW, or Qt5 |
| [Turtle graphics](#32-turtle-graphics-turtle_winapi) | [`doc/examples/turtle_winapi/`](../examples/turtle_winapi/) | `abi` blocks + shared library + GDI canvas | Windows + MinGW |
| [Web student records](#41-web-student-records) | [`example/web-students/`](../../example/web-students/) | HTTP via `std::net`, SQLite via `std::sql`, same-origin page | none |
| [Gradebook · console](#42-gradebook--console-edition) | [`example/sql-magel/example/ipage-student/`](../../example/sql-magel/example/ipage-student) | A real MySQL database, a four-role permission matrix, menu pruning | MySQL |
| [Gradebook · split front end and back end](#43-gradebook--split-front-end-and-back-end) | [`example/sql-magel/example/ipage-student-server/`](../../example/sql-magel/example/ipage-student-server) | A JSON-only back end, separately hosted front end, cross-origin CORS | MySQL + static server |
| [Calculator (two ASTs)](#51-calculator-two-asts) | [`example/calculator/`](../../example/calculator/) | Recursive-descent parsing, recursive ownership under value semantics | none |
| [Module compilation](#52-module-compilation) | [`example/module-compilation/`](../../example/module-compilation/) | Partial rebuild of a multi-module AOT project | none |
| [Annotations and reflection](#53-annotations-and-reflection) | [`example/reflection/`](../../example/reflection/) | Compile-time annotations, runtime metadata queries | reflection build macros |
| [Standard library companions](#54-standard-library-companions) | [`doc/examples/library/`](../examples/library/) | Four runnable programs: io / sqlite / watchdog / reflect | some need reflection |
| [C ABI companions](#55-c-abi-companions) | [`doc/examples/abi_buffers/`](../examples/abi_buffers/), [`abi_dynamic/`](../examples/abi_dynamic/) | Static buffers vs. dynamic library bindings | a C compiler |
| [`.svmb` evidence pack](#6-delivery-and-reverse-engineering-the-svmb-evidence-pack) | [`doc/examples/svmb/`](../examples/svmb/) | A bytecode specimen, section-by-section anatomy, reproducible reversing-cost data | none |
| [Board-side VM](#71-board-side-vm-mcu_vm) | [`source/stackvm_mcu/`](../../source/stackvm_mcu) | A `.svmb` runtime built with `-fno-exceptions` | a C++ compiler |
| [Glue callbacks](../../example/native-callbacks/README.md) | [`example/native-callbacks/`](../../example/native-callbacks/) | Shared VM/AOT glue calling named functions, closures, references and higher-order callbacks | a C++ compiler |
| [Editor language servers](#72-editor-language-servers-and-the-vs-code-extension) | [`source/lsp/`](../../source/lsp), [`vscode/`](../../vscode/) | Macro pairing checks, semantic highlighting, real diagnostics | none / VS Code |

---

## 2. How to read this document

Every example uses the same four headings:

- **What it demonstrates** — which side of the language it is meant to prove;
- **How to run it** — the shortest command that works;
- **How it is verified** — the dedicated regression script (an example only earns the name if it passes);
- **Boundaries** — prerequisites, and what it deliberately does *not* do.

The examples fall into two groups; do not confuse them. **Application** examples (GUI, web
student records, gradebook) show *what you can build*. **Capability** examples (calculator,
reflection, module compilation, standard-library companions) show *what a language or library
feature looks like*.

---

## 3. Desktop and GUI

All three need **a usable interactive desktop** (headless environments should use the Qt offscreen
path given in each section).

### 3.1 Minimal GUI (gui)

**What it demonstrates**: the Magel side only declares [`gui.mgl`](../../example/gui/gui.mgl); windows and
widgets live in a host library ([`example/gui/winapi/gui.c`](../../example/gui/winapi/gui.c) or
[`example/gui/qt/gui_qt.cpp`](../../example/gui/qt/gui_qt.cpp)). Only fixed-width integers and UTF-8 strings cross
the boundary — **no window handles or widget structures are passed**.

Events travel the same "happen asynchronously, deliver at a safe point" path as software timers:
a GUI event returns through a C callback to the glue layer, the runtime poster queues it, and the
VM / AOT delivers it to a Magel interrupt function at the next execution boundary.

**How to run it**:

```powershell
python tests/magel_desktop.py        # WinAPI：出 GUI 与海龟两个产物
```

```sh
python gui/test_qt.py                # Qt offscreen: build, UTF-8 round trip, event injection, pumpEvery
```

To run the Qt build by hand: `sh gui/qt/build.sh build/magel-gui-qt`, then
`magel aot gui/examples/counter.mgl -I gui --glue gui/glue.cpp --out build/magel-gui-qt/counter`.

**How it is verified**: `tests/magel_desktop.py` (WinAPI), `example/gui/test_qt.py` (Qt offscreen),
`example/gui/test_winapi.py`, `example/gui/test_program.py`.

**Boundaries**: the client area is limited to 100–4096 pixels and widget IDs range from 1 to 65535
and must be unique; the shared library has to sit in the same working directory as the executable
(the glue locates it by a relative path); the event convention is "button click = widget ID (>0),
window close = -1, Qt periodic event = 0".

### 3.2 Turtle graphics (turtle_winapi)

**What it demonstrates**: the smallest complete C ABI desktop example.
[`demo.mgl`](../examples/turtle_winapi/demo.mgl) sequences turtle paths through host function
declarations, [`turtle.c`](../examples/turtle_winapi/turtle.c) builds into `turtle.dll` and creates
the window and GDI canvas with WinAPI, and [`glue.cpp`](../examples/turtle_winapi/glue.cpp) is
embedded into a standalone program by `magel aot --glue`. **The VM never touches `HWND` / `HDC` /
canvas memory** — all canvas state belongs to the window thread, and Magel calls are handed
synchronously to the window procedure via `SendMessageW`.

**How to run it**:

```powershell
python tests/magel_desktop.py --turtle-only
Set-Location .\build\magel-turtle-winapi
.\demo.exe            # 绘制 Koch 雪花；关窗退出
.\demo.exe close      # 绘制并保存后直接关闭（自动验收用）
```

The program writes `turtle.bmp` into the current directory.

**Boundaries**: needs Windows and a usable interactive desktop; the DLL must sit next to the exe.
A mismatch between the Magel declaration, the C function signature or the calling convention is a
C ABI boundary error — background in [Host integration](08-host-integration.md).

---

## 4. Web and server side

### 4.1 Web student records

**What it demonstrates**: Magel as its own server — `std::net` listens for HTTP, `std::sql` talks to
a real SQLite file, and the browser calls **same-origin** JSON endpoints. No web framework and no
front-end dependencies. It supports create, list, edit and delete, filtering by student number,
name or e-mail, with uniqueness and range checks on the server.

**How to run it**:

```powershell
.\magel.exe run example/web-students/main.mgl --out build/web-students -- build/students.db 8080
```

Then open `http://127.0.0.1:8080/`. SQLite is statically built in — no DLL to prepare.
Arguments after `--` go straight to the Magel `main` (in order: database file, port, request limit,
page path, ready file); SQLite does **not** create directories, so `build/` has to exist first.

**How it is verified**:

```sh
python tests/magel_web_students.py            # interpreted + AOT, 62 real HTTP requests per back end
python tests/magel_web_students_browser.py --browser "<path to Chrome or Edge>"
```

The second one drives the form in a real browser and checks HTML-injection protection plus the
actual SQL rows on disk.

**Boundaries**: a local, single-connection, sequential text HTTP server. No TLS, chunked transfer,
keep-alive, file upload, login or permission management. The `CREATE TABLE` statements, auto
increment and `last_insert_rowid()` are SQLite dialect — switching back ends means rewriting those
SQL statements too.

### 4.2 Gradebook · console edition

**What it demonstrates**: the login and CRUD of ipage-student (a Java + Spring MVC + MyBatis course
project) rewritten in Magel as a console program, **without the ipage framework**, reading and
writing ipage-student's own `student` database. The four-role permission matrix (3 / 5 / 5 / 6 menu
entries for student, teacher, administrator and super administrator), the "filter the menu first,
then check again on selection" rule, and printing the blast radius before a delete all live in a
single file.

**How to run it**:

```sh
magel sql-magel/example/ipage-student/main.mgl                      # 库已存在
magel sql-magel/example/ipage-student/main.mgl -- --init ../ipage-student/student.sql
magel sql-magel/example/ipage-student/main.mgl -- --answers sql-magel/tests/input_ipage.txt
```

`--answers` feeds scripted replies, which is how the self-check and the demo run it.

**How it is verified**: `sh sql-magel/tests/check-ipage.sh` (it should print `ok ipage`).
The self-check **never touches the real database**: it rebuilds an isolated `student_check` database
from `example/sql-magel/tests/fixture_ipage.sql` with `--reinit`, so auto-increment keys start at 1 every
time, and the test database is back to its initial state when the script finishes.

**Boundaries**: passwords are compared in plaintext (that is how they are stored in `student.sql`) —
**matching the existing style takes priority over hardening**. See the
[SQL interface](../../example/sql-magel/doc/01-sql-library.md).

### 4.3 Gradebook · split front end and back end

**What it demonstrates**: the console edition upgraded into a **split front end and back end**
application. The back end [`main.mgl`](../../example/sql-magel/example/ipage-student-server/main.mgl) serves
only `/api/*` JSON endpoints (`/` and `/index.html` return 404), while the front end
[`web/`](../../example/sql-magel/example/ipage-student-server/web) is plain static assets hosted by **a
separate process**. The two are not same-origin and talk over CORS.

| Concern | Approach |
| --- | --- |
| Who serves the page | The back end serves no pages; static assets are deployed separately |
| Cross-origin | Every response (including the `OPTIONS` preflight) carries CORS headers, narrowable with `--allow-origin` |
| Identity | `Authorization: Bearer <64 hex digits>`; sessions live in memory and die on restart |
| Permissions | The permission table is the single source of truth and the back end checks every action; the front end only renders what `/api/session` reports |
| Injection | The front end writes user data with `textContent` only, never by building HTML |

**How to run it** (two terminals):

```sh
# ① 后端（库已存在；不存在就加 -- --init ../ipage-student/student.sql）
magel run sql-magel/example/ipage-student-server/main.mgl --out build/ipage-server -- \
    --listen-port 8080

# ② 前端静态服务器
python -m http.server 5500 --directory sql-magel/example/ipage-student-server/web
```

Open `http://127.0.0.1:5500/`; if the back end is not on the default 8080, override with
`http://127.0.0.1:5500/?api=http://127.0.0.1:9000` (the front end's only coupling point is
`web/config.js`).

**How it is verified**:

```sh
python tests/magel_ipage_student_server.py            # run 与 aot 各 5 组，各 207 个真实请求
python tests/magel_ipage_student_server_browser.py --browser "<path to Chrome or Edge>"
```

The API script covers login / logout / bad tokens, the visible modules and per-action thresholds of
all four roles, CRUD plus aggregate statistics for the five modules, the CORS preflight, the fact
that the back end serves no pages, and HTTP message boundaries. The browser script starts another
static server in a thread and runs the whole flow **genuinely cross-origin**: sign in, menus and
buttons appearing or disappearing by permission, create/edit/find/delete, the statistics page, and
injection protection.

**Boundaries**: the "3 attempts" limit on a stateless HTTP back end is counted by the front end, so
calling the API directly has no attempt limit; sessions have no expiry and no refresh mechanism;
passwords are still compared in plaintext. Shipping this for real would require hashing, TLS,
session expiry and rate limiting.

---

## 5. Language and library capability examples

### 5.1 Calculator (two ASTs)

**What it demonstrates**: the same expression interpreter — lexing, recursive-descent parsing,
instruction generation, stack-machine execution and per-line error reporting — written twice with
**two AST representations**: a flat `List<Node>` with a child-index table, and a nested one whose
enum payload holds `List<Expr>`. The nested version exists to verify copying and cleanup of
recursively owned locals under value semantics.

**How to run it**:

```sh
magel run example/calculator/calc.mgl
magel run example/calculator/calc_nested.mgl
magel aot example/calculator/calc.mgl --out build/calculator && build/calculator
```

Of the 12 default input lines, 7 succeed and 5 report application errors (including division by
zero, a syntax error, an undefined variable, and `min(x, y, z)` with too many arguments).

**How it is verified**: `python tests/magel_recursive_ownership.py` — it checks interpretation, AOT,
the values and error reports of both representations, and resource diagnostics.

### 5.2 Module compilation

**What it demonstrates**: the AOT path for multi-module projects — per-module native object caching,
partial rebuild, dependency invalidation and a final link. The layout lives in
[`example/module-compilation/magel.toml`](../../example/module-compilation/magel.toml).

**How to run it**:

```sh
magel aot example/module-compilation/main.mgl --separate-modules --out build/module-demo
```

**How it is verified**: `python tests/magel_module_compilation.py`.
The relevant options (`--separate-modules`, `--module-cache`) are in
[Cross-platform deployment](06-deployment.md).

### 5.3 Annotations and reflection

**What it demonstrates**: compile-time user annotations (with defaults, positional and named
arguments) applied to interfaces, structs, enums, fields, static fields, constructors and methods;
then querying metadata at runtime and calling by name.

**How to run it**: the root toolchain already includes both front-end and runtime reflection by default:

```sh
magel --reflection example/reflection/annotations.mgl --out build/annotation-example
```

**How it is verified**: `python tests/magel_annotations.py` (it generates and actually runs both a VM
and an AOT program).

**Boundaries**: annotations are type-checked and reflected declarations produce runtime metadata and restricted call bridges. The `std::reflect::` API is in the [library reference](../library/README.md).

### 5.4 Standard library companions

**What it demonstrates**: library usage that needs a little project layout.
[`doc/examples/library/`](../examples/library/) holds four: `io.mgl` (file create / write / flush /
seek), `sqlite.mgl` (in-memory SQLite and Unicode strings), `watchdog.mgl` (disabled and
over-watermark branches) and `source/std/reflect.mgl` (type and struct-field queries).

**How to run them**:

```sh
magel run doc/examples/library/io.mgl -- build/library-example.txt
magel run doc/examples/library/sqlite.mgl
```

**How it is verified**: `python tests/magel_library_reference.py` — it checks declarations and links
and executes 23 complete programs.

### 5.5 C ABI companions

**What it demonstrates**: two binding styles. [`abi_buffers/`](../examples/abi_buffers/) passes
static buffers; [`abi_dynamic/`](../examples/abi_dynamic/) goes through a shared library plus glue.
Both are small `native.c` + `demo.mgl` pairs, handy as the minimal skeleton to copy when writing your
own bindings.

**How it is verified**: `python tests/magel_abi_values.py`.
Type mapping and `abi` block syntax are in [Host integration and C ABI](08-host-integration.md).

---

## 6. Delivery and reverse engineering: the `.svmb` evidence pack

**What it demonstrates**: [`doc/examples/svmb/`](../examples/svmb/) is the evidence behind
[`.svmb` binary format anatomy and reverse-engineering difficulty](../15-svmb-format.md).
The specimen `control.svmb` is 662,750 bytes with SHA-256 `CF30A3C8…A757ABE`; the same source built
with `--no-debug` yields 249,681 bytes with SHA-256 `08554811…A6007476D`, containing **no path
strings at all**, so it is byte-identical no matter which directory produced it.

Every number in there is produced by [`collect.py`](../examples/svmb/collect.py), **never written by
hand**; when a tool is missing the corresponding section says "skipped" instead of inventing content.

**How to run it**:

```sh
magel run example/samples/control.mgl --emit --svmb --out tmp/svmb-study/control
python doc/examples/svmb/collect.py --parser ./magel.exe
```

(`collect.py` also accepts `--vm` / `--vm-opcode` / `--exported` to add the VM and exported-program
evidence.)

**Boundaries**: rebuilding is **not byte-identical to the specimen** — the debug section stores a
compile-time `.quad` path string for every instruction (measured: 33 bytes × 8204 = `+270732`
bytes), so building from a different directory changes the artifact size; the program body itself is
byte-identical. `svmb_dump.py` is a minimal parser using only the standard library.

---

## 7. Toolchain companions

### 7.1 Board-side VM (mcu_vm)

**What it demonstrates**: not "another interpreter" but **a compilation stance** — the same VM
source built under board constraints, targeting H743 (roughly 1 MB RAM / 2 MB Flash) as a floor.
`-fno-exceptions` makes `try` / `catch` / `throw` **fail to compile**: exception freedom is enforced
by the compiler, not by review.

**How to run it**:

```sh
make -C source/stackvm_mcu                 # 出 build/stackvm_mcu/mcu-host（桌面可跑的板端宿主）
build/stackvm_mcu/mcu-host program.svmb data.data.bin data.layout
make -C source/stackvm_mcu show-flags      # 打印这次实际用的口径
```

**How it is verified**: `python tests/mcu_vm.py --sweep` (already part of `ci.sh`).
The full stance table is in [`source/stackvm_mcu/README.md`](../../source/stackvm_mcu/README.md).

The physical FK-STM32H743XIH6 project is in
[`example/mcu/FK-STM32H743XIH6/`](../../example/mcu/FK-STM32H743XIH6/README.md).
It places the large VM pools in SDRAM, uses USART1 for diagnostics and FatFS for loading programs,
and provides an LCD program selector controlled by physical buttons. Normal exit and panic return
to the selector. The board libraries are included, and the default firmware runs the benchmark once
at startup. After timing completes, MODE/ENTER or the USART1 virtual key commands `key mode` /
`key enter` return to the selector. A DWT cycle benchmark compares the VM with the same integer kernel in native C++.

### 7.2 Editor language servers and the VS Code extension

**What it demonstrates**: two complementary routes. The toolchain's built-in `magel lsp` (embedded
in the compiler, running the real front end, giving real error codes and precise positions) covers
the complete front end; the small C++17 program in [`source/lsp/`](../../source/lsp) **needs no compiler** and
offline-checks the pairing, arguments, calls and nesting of `$macro` / `$replace` / `$for`, plus
semantic highlighting.

**How to run them**:

```sh
magel lsp                      # 工具链自带
make -C lsp                    # 独立的宏编辑器语言服务器
```

The VS Code extension lives in [`vscode/`](../../vscode/); syntax highlighting, completion, go-to
and breakpoint debugging are described in its [README](../../vscode/README.md).

**How it is verified**: `python tests/magel_lsp_api.py`, `python tests/magel_lsp_symbols.py`,
`python tests/magel_native_lsp_parity.py`.

---

## 8. Regression entry points

| Command | Covers |
| --- | --- |
| `sh check.sh` | Every sample in `example/samples/` against its expected output |
| `python tests/magel_desktop.py [--turtle-only]` | Two desktop artifacts: WinAPI GUI / turtle |
| `python gui/test_qt.py <dir>` (Linux) | Qt back end: build, UTF-8, event injection and `pumpEvery` |
| `python tests/magel_web_students.py` | Web student records: interpreted + AOT |
| `python tests/magel_web_students_browser.py --browser …` | Web student records in a real browser |
| `sh sql-magel/tests/check-ipage.sh` | Gradebook console edition (never touches the real database) |
| `python tests/magel_ipage_student_server.py` | Gradebook split edition: the API |
| `python tests/magel_ipage_student_server_browser.py --browser …` | Gradebook split edition: cross-origin browser flow |
| `python tests/magel_recursive_ownership.py` | Calculator, both AST representations |
| `python tests/magel_module_compilation.py` | Modular AOT and partial rebuild |
| `python tests/magel_annotations.py` | Annotations and reflection |
| `python tests/magel_library_reference.py` | Standard library companions |
| `python tests/magel_abi_values.py` | C ABI companions |
| `python tests/mcu_vm.py --sweep` | Board-side VM stance |

The entries that need a graphical environment or a database are **not part of `ci.sh`**; they are
dedicated regressions: the desktop ones want Windows + MinGW (or Qt5), and the gradebook wants a
local MySQL running.

---

## 9. Next

| You want to | Go to |
| --- | --- |
| Ship a program of your own | [Delivery security](05-delivery-security.md) → [Cross-platform deployment](06-deployment.md) |
| Connect Magel to a C/C++ host | [Host integration and C ABI](08-host-integration.md) |
| Learn the language from scratch | [《玩转 Magel 语言》](../book/README.md) (Chinese) |
| Look up a library function | [Library reference](../library/README.md) |
| Look up commands and project config | [Toolchain](09-toolchain.md) |
