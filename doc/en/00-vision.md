# 00 Vision: Business logic as an independently deliverable asset

> **Write once, run anywhere.**
> One logic, from MCU to cloud; value semantics, non-null references, no GC.

> This document is not about syntax. It answers: **why does Magel exist, what does it solve for you,
> and where will it disappoint you.**

---

## 1. Three dilemmas of delivery

### Dilemma 1: source code is both the asset and the biggest exposure

You write business logic as source, and that source is among the most valuable things your company
owns. But when you deliver, you have only two options:

- Ship source — the customer receives your entire design; change one number and it becomes their
  product;
- Ship binaries — then you accept "rebuild once per platform", and every platform has a different
  toolchain, dependencies and build environment.

On-premise deployment pushes this contradiction to the extreme: the customer requires deployment in
their own data center, and you do not want their ops team reading your core algorithm.

### Dilemma 2: business logic is held hostage by the platform

The same set of rules gets written several times over because it runs in different places: Go in the
cloud, C++ on the desktop, C inside the device, TypeScript on the front end. Nominally it is "one
business logic"; in reality it is four codebases to maintain, test and break separately. Changing one
pricing rule means syncing four places, and they never truly sync.

### Dilemma 3: memory safety and productivity seem mutually exclusive

Play it safe and you accept the borrow checker's learning curve, or a garbage collector's pauses and
unpredictable memory footprint. Go fast, stay in control, run on a board with no operating system,
and you are back to hand-written resource management, dangling pointers and leaks. "Languages with a
GC don't run on embedded" and "without a GC you will get it wrong" look like a binary choice.

---

## 2. Magel's answers

### Answer 1: from source to bytecode — logic runs without its source

Magel source compiles to `.svmb` bytecode, executed by StackVM. **What you deliver is bytecode, not
source.**

You can deliver bytecode, compress its symbol table, or AOT-export a standalone native executable. Choose the form for each recipient; see [Delivery security](05-delivery-security.md). Symbol compression removes readable names while preserving executable logic, which remains available for analysis.

### Answer 2: from MCU to cloud — one logic crosses every boundary

The same Magel program runs several ways: interpret it directly while developing; export `.svmb` for
delivery; AOT-compile to a native executable for local deployment. All three share one set of types,
value semantics and resource-management rules — **what changes is the execution mode, not the logic
you wrote.**

This is not a slogan. There is a real-hardware record on STM32F407: compile, trim and package on the
PC, push to an STM32F407VET6 board via H7TOOL or UART1, and execute through a fixed-capacity virtual
machine calling on-board HAL glue; integer and double-precision programs terminate normally, the
read-back Flash image matches, and single / double click and long-press events render correctly on
the OLED.
(That separate project was later removed from the repository as requested; the record is still in Git
history. This historical run **does not** mean every current language feature has been re-validated
on the board.)

The current H743 project supplies SDRAM, FatFS, USART1, LCD and key adapters; see the [board project](../../example/mcu/FK-STM32H743XIH6/README.md). Desktop hosts and board ports follow the same division of labour: **Magel
source describes the business, the binary holds the executable program, the target platform provides
the runtime and the host interface.**

### Answer 3: value semantics, non-null references and borrow checking

- Ordinary owned values copy; explicit move transfers resources.
- Non-null `T const&` / `T mut&` references express permissions. Option represents optional references without constructing a null reference.
- Scope and Drop/Closeable provide deterministic owned-value cleanup.
- Independent borrow checking tracks source scope and moves, destruction and releases across control flow.

Valid aliases may share objects. Checking does not impose a global exclusive-borrow rule or cover arbitrary
external host memory. Large copies cost work, and manual allocations/host resources retain explicit contracts.
See [core concepts](03-core-concepts.md#3-borrow-provenance-and-control-flow-checking) and
[current capabilities](17-capabilities.md).

---

## 3. Who Magel fits

**Embedded and IoT vendors**
The same control logic must be reused across models and chips; firmware is updated onto devices and
you would rather the on-board binary not be trivially readable. Magel's `.svmb` + external memory
buffer + soft-restart watchdog are built for this.

**SaaS companies that must deliver on-premise**
The customer wants it in their own data center. You can ship bytecode with compressed symbols, keep the mapping file yourself — locate problems through the mapping when they
happen, while keeping your source files and symbol mapping private.

**Toolchain vendors needing one logic across many ends**
One set of rules must run in the cloud, on the desktop and inside the device. Magel lets you maintain
**one** source, pick the execution mode per target, adapt platform-specific parts through host
interfaces, and reuse platform-independent parts as-is.

---

## 4. Who should not use Magel

Read this section carefully. It matters more than everything above.

- **Projects that need a mature ecosystem and a huge third-party library catalogue.** Magel's standard
  library covers containers, text, numerics, networking, databases and concurrency, but its ecosystem
  cannot compare with mainstream languages. If your project leans heavily on off-the-shelf libraries,
  Magel will slow you down.
- **Teams that need a mature IDE, hot reload and rich debug visualisation.** The VS Code extension
  already supports syntax highlighting, diagnostics, completion, go-to-definition, document outline,
  formatting and breakpoint debugging, but overall toolchain maturity is still a work in progress.
- **Scenarios chasing maximum runtime performance.** Magel prioritises predictability and
  portability. AOT can export a native program, but do not expect it to beat hand-optimised C/C++.
- **Anyone expecting shipped logic to be impossible to reverse engineer.** See the
  [boundary statement in Delivery security](05-delivery-security.md#boundaries).
- **Cross-platform UI, web front-end frameworks, mobile development.** These are not Magel's target
  domains.

---

## 5. Design principles

| Principle | Meaning |
| --- | --- |
| **Value semantics** | A copy is a copy. Ordinary assignment preserves its source; `move` explicitly transfers ownership from the source |
| **Non-null references** | A reference is never null; read-only and writable are written in the type |
| **No GC** | Resources are released deterministically at the end of scope; memory footprint is predictable |
| **Deterministic cleanup** | `Drop` / `Closeable` give explicit release points, with automatic cleanup covering common code |
| **Multiple execution levels** | Interpretation, `.svmb` bytecode, AOT native program — one logic, three delivery forms |
| **Clear over clever** | The language prefers refusing combinations it cannot prove safe over letting you rely on implicit behaviour |

---

## Next

- Want to run something first → [5-minute quickstart](01-quickstart.md)
- Want to see what the language looks like → [Language tour](02-language-tour.md)
- Evaluating delivery and deployment → [Delivery security](05-delivery-security.md) · [Cross-platform deployment](06-deployment.md)
