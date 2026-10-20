# 00 Vision: Business logic as an independently deliverable asset

> **Write once, run anywhere.**
> One logic, from embedded Linux to cloud; value semantics, non-null references, no GC.

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

### Answer 2: from desktop to embedded Linux

Desktop and embedded Linux hosts use the same `library/stackvm/` execution core. The target needs a
C++17 toolchain, sufficient memory and the system libraries required by its program. Build the host
and native glue for the target architecture and runtime. Magel source describes the business,
bytecode carries the program, and the host supplies files, networking, clocks and other services.

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

**Embedded Linux and IoT vendors**
Reuse application logic across Linux devices, deploy bytecode updates and adapt system services through native glue. Resource budgets and process recovery follow the target configuration.

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

---

## 5. Design principles

| Principle | Meaning |
| --- | --- |
| **Value semantics** | A copy is a copy. Ordinary assignment preserves its source; `move` explicitly transfers ownership from the source |
| **Non-null references** | A reference is never null; read-only and writable are written in the type |
| **No GC** | Resources are released deterministically at the end of scope; memory footprint is predictable |
| **Deterministic cleanup** | `Drop` / `Closeable` give explicit release points, with automatic cleanup covering common code |
| **Clear over clever** | The language prefers refusing combinations it cannot prove safe over letting you rely on implicit behaviour |

---

## Next

- Want to run something first → [5-minute quickstart](01-quickstart.md)
- Want to see what the language looks like → [Language tour](02-language-tour.md)
- Evaluating delivery and deployment → [Delivery security](05-delivery-security.md) · [Cross-platform deployment](06-deployment.md)
