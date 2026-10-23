# 13 Example projects

The earlier documents explain *how to write* ChaLi. This one lists **what already runs in the
repository** — for each example: where it lives, which capability it shows off, how to run it, how
it is verified, and what it needs beforehand.

Every command is run **from the repository root** (on Windows the compiler is `ChaLi.exe`; below it
is written `chali`).

---

## 1. At a glance

| Example | Location | What it demonstrates | Requires |
| --- | --- | --- | --- |
| [Turtle graphics](#31-turtle-graphics-turtle_winapi) | [`doc/examples/turtle_winapi/`](../examples/turtle_winapi/) | `abi` blocks + shared library + GDI canvas | Windows + MinGW |
| [Calculator (two ASTs)](#51-calculator-two-asts) | [`example/calculator/`](../../example/calculator/) | Recursive-descent parsing, recursive ownership under value semantics | none |
| [Annotations and reflection](#53-annotations-and-reflection) | [`example/reflection/`](../../example/reflection/) | Compile-time annotations, runtime metadata queries | reflection build macros |
| [Standard library companions](#54-standard-library-companions) | [`doc/examples/library/`](../examples/library/) | Four runnable programs: io / sqlite / watchdog / reflect | some need reflection |
| [C ABI companions](#55-c-abi-companions) | [`doc/examples/abi_buffers/`](../examples/abi_buffers/), [`abi_dynamic/`](../examples/abi_dynamic/) | Static buffers vs. dynamic library bindings | a C compiler |
| [`.svmb` evidence pack](#6-artifacts-and-reverse-engineering-the-svmb-evidence-pack) | [`doc/examples/svmb/`](../examples/svmb/) | A bytecode specimen, section-by-section anatomy, reproducible reversing-cost data | none |
| [Glue callbacks](../../example/native-callbacks/README.md) | [`example/native-callbacks/`](../../example/native-callbacks/) | Shared VM glue calling named functions, closures, references and higher-order callbacks | a C++ compiler |
| [Editor language servers](#72-editor-language-servers-and-the-vs-code-extension) | [`source/lsp/`](../../source/lsp), [`vscode/`](../../vscode/) | Macro pairing checks, semantic highlighting, real diagnostics | none / VS Code |

---

## 2. How to read this document

Every example uses the same four headings:

- **What it demonstrates** — which side of the language it is meant to prove;
- **How to run it** — the shortest command that works;
- **How it is verified** — the dedicated regression script (an example only earns the name if it passes);
- **Boundaries** — prerequisites, and what it deliberately does *not* do.

The examples fall into two groups; do not confuse them. **Application** examples (student records and graphics) show *what you can build*. **Capability** examples (calculator,
reflection, module compilation, standard-library companions) show *what a language or library
feature looks like*.

---

## 3. Desktop graphics and games

Desktop examples require an interactive desktop; see each example for headless verification.

### 3.1 Turtle graphics (turtle_winapi)

**How to run it**:

```powershell
python tests/chali_desktop.py --turtle-only
Set-Location .\build\chali-turtle-winapi
.\demo.exe            # 绘制 Koch 雪花；关窗退出
.\demo.exe close      # 绘制并保存后直接关闭（自动验收用）
```

The program writes `turtle.bmp` into the current directory.

**Boundaries**: needs Windows and a usable interactive desktop; the DLL must sit next to the exe.
A mismatch between the ChaLi declaration, the C function signature or the calling convention is a
C ABI boundary error — background in [Host integration](08-host-integration.md).

---

## 4. Database examples

### 4.1 Reflection-based student manager

**What it demonstrates**: `@Reflect` plus SQL model annotations derive the create-table
statement, query columns, JSON keys and sort-field validation; every user value is bound
as an SQL parameter; the field list is declared exactly once, in the model.

**How to run it** (from `example/reflection-students`):

```sh
chali main.chl students.db add '{"id":1,"name":"Zhang Wei","age":18,"className":"Class 3","score":92}'
chali main.chl students.db list score desc
chali main.chl students.db schema
```

**How it is verified**: exercise `add` / `update` / `get` / `delete` / `list` / `search` /
`schema`, plus malformed JSON, out-of-range age and score, unknown sort fields and a
missing id.

**Boundaries**: requires a reflection-enabled toolchain; changing model fields affects
existing databases (an old database missing the column is rejected with a "missing field"
diagnostic).

### 4.2 Other database examples

See the [SQL examples](../../example/sql-chali/README.md) and [reflection / dynamic SQL guide](36-reflection-serialization-sql.md).

## 5. Language and library capability examples

### 5.1 Calculator (two ASTs)

**What it demonstrates**: the same expression interpreter — lexing, recursive-descent parsing,
instruction generation, stack-machine execution and per-line error reporting — written twice with
**two AST representations**: a flat `List<Node>` with a child-index table, and a nested one whose
enum payload holds `List<Expr>`. The nested version exists to verify copying and cleanup of
recursively owned locals under value semantics.

**How to run it**:

Of the 12 default input lines, 7 succeed and 5 report application errors (including division by
zero, a syntax error, an undefined variable, and `min(x, y, z)` with too many arguments).

### 5.2 Module compilation

**How to run it**:

**How it is verified**: `python tests/chali_module_compilation.py`.
The relevant options (`--separate-modules`, `--module-cache`) are in
[Cross-platform deployment](06-deployment.md).

### 5.3 Annotations and reflection

**What it demonstrates**: compile-time user annotations (with defaults, positional and named
arguments) applied to interfaces, structs, enums, fields, static fields, constructors and methods;
then querying metadata at runtime and calling by name.

**How to run it**: the root toolchain already includes both front-end and runtime reflection by default:

```sh
chali --reflection example/reflection/annotations.chl --out build/annotation-example
```

**Boundaries**: annotations are type-checked and reflected declarations produce runtime metadata and restricted call bridges. The `std::reflect::` API is in the [library reference](../library/README.md).

### 5.4 Standard library companions

**What it demonstrates**: library usage that needs a little project layout.
[`doc/examples/library/`](../examples/library/) holds four: `io.chl` (file create / write / flush /
seek), `sqlite.chl` (in-memory SQLite and Unicode strings), `watchdog.chl` (disabled and
over-watermark branches) and `source/std/reflect.chl` (type and struct-field queries).

**How to run them**:

```sh
chali run doc/examples/library/io.chl -- build/library-example.txt
chali run doc/examples/library/sqlite.chl
```

**How it is verified**: `python tests/chali_library_reference.py` — it checks declarations and links
and executes 23 complete programs.

### 5.5 C ABI companions

**What it demonstrates**: two binding styles. [`abi_buffers/`](../examples/abi_buffers/) passes
static buffers; [`abi_dynamic/`](../examples/abi_dynamic/) goes through a shared library plus glue.
Both are small `native.c` + `demo.chl` pairs, handy as the minimal skeleton to copy when writing your
own bindings.

**How it is verified**: `python tests/chali_abi_values.py`.
Type mapping and `abi` block syntax are in [Host integration and C ABI](08-host-integration.md).

---

## 6. Artifacts and reverse engineering: the `.svmb` evidence pack

**What it demonstrates**: [`doc/examples/svmb/`](../examples/svmb/) is the evidence behind
[`.svmb` binary format anatomy and reverse-engineering difficulty](../15-svmb-format.md).
The specimen `control.svmb` is 662,750 bytes with SHA-256 `CF30A3C8…A757ABE`; the same source built
with `--no-debug` yields 249,681 bytes with SHA-256 `08554811…A6007476D`, containing **no path
strings at all**, so it is byte-identical no matter which directory produced it.

Every number in there is produced by [`collect.py`](../examples/svmb/collect.py), **never written by
hand**; when a tool is missing the corresponding section says "skipped" instead of inventing content.

**How to run it**:

```sh
chali run example/samples/control.chl --emit --svmb --out tmp/svmb-study/control
python doc/examples/svmb/collect.py --parser ./ChaLi.exe
```

(`collect.py` also accepts `--vm` / `--vm-opcode` / `--exported` to add the VM and exported-program
evidence.)

**Boundaries**: rebuilding is **not byte-identical to the specimen** — the debug section stores a
compile-time `.quad` path string for every instruction (measured: 33 bytes × 8204 = `+270732`
bytes), so building from a different directory changes the artifact size; the program body itself is
byte-identical. `svmb_dump.py` is a minimal parser using only the standard library.

---

## 7. Toolchain companions

### 7.1 Embedded Linux hosting

Build `source/stackvm/` and the native glue with the target Linux C++17 toolchain. Deploy the
`.svmb`, data and layout files together, and provide the system libraries used by the host.
The [deployment guide](06-deployment.md) describes the execution and resource boundaries.

### 7.2 Editor language servers and the VS Code extension

**What it demonstrates**: two complementary routes. The toolchain's built-in `chali lsp` (embedded
in the compiler, running the real front end, giving real error codes and precise positions) covers
the complete front end; the small C++17 program in [`source/lsp/`](../../source/lsp) **needs no compiler** and
offline-checks the pairing, arguments, calls and nesting of `$macro` / `$replace` / `$for`, plus
semantic highlighting.

**How to run them**:

```sh
chali lsp                      # 工具链自带
make -C lsp                    # 独立的宏编辑器语言服务器
```

The VS Code extension lives in [`vscode/`](../../vscode/); syntax highlighting, completion, go-to
and breakpoint debugging are described in its [README](../../vscode/README.md).

**How it is verified**: `python tests/chali_lsp_api.py`, `python tests/chali_lsp_symbols.py`,
`python tests/chali_native_lsp_parity.py`.

---

## 8. Regression entry points

| Command | Covers |
| --- | --- |
| `sh check.sh` | Every sample in `example/samples/` against its expected output |
| `python tests/chali_desktop.py [--turtle-only]` | Turtle graphics through WinAPI |
| `python tests/chali_recursive_ownership.py` | Calculator, both AST representations |
| `python tests/chali_annotations.py` | Annotations and reflection |
| `python tests/chali_library_reference.py` | Standard library companions |
| `python tests/chali_abi_values.py` | C ABI companions |

Checks requiring a graphical environment or database are dedicated regressions: desktop graphics need Windows and MinGW, while external database examples need their corresponding service and client library.

---

## 9. Next

| You want to | Go to |
| --- | --- |
| Deploy a program of your own | [Delivery security](05-delivery-security.md) → [Cross-platform deployment](06-deployment.md) |
| Connect ChaLi to a C/C++ host | [Host integration and C ABI](08-host-integration.md) |
| Learn the language from scratch | [《玩转 ChaLi 语言》](../book/README.md) (Chinese) |
| Look up a library function | [Library reference](../library/README.md) |
| Look up commands and project config | [Toolchain](09-toolchain.md) |
