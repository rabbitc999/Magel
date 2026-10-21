# Resource lifetimes and borrow control flow

## Allocation identities without tombstones

`library/stackvm/allocation_index.hpp` stores active logical intervals and each physical root's latest generation and displacement width. Release removes the active interval immediately. No retired identities or sweeping boundaries are stored. Repeated release fails; physical reuse never revives an old logical reference. Generation exhaustion refuses reuse instead of wrapping, and repeated reuse of one physical root needs only one identity slot.

## Resource construction and cleanup modules

Explicit release uses `value.drop()` for heap references, arrays and owned resources. Neither `delete` nor a standalone `drop` statement exists. An ordinary type's unrelated method named `drop` remains an ordinary call.

`source/semantics/resource_lifetime.cpp` centralizes construction registration, Closeable/Drop calls, aggregate destruction, owner-slot cleanup and scope exits. Explicit termination, automatic cleanup and moves share the ownership contract. Executed release clears the owner slot without removing another branch's cleanup. Host close shuts down the operating-system resource; drop later destroys its still-owned record rather than retaining a released record as a tombstone.

The independently compiled `source/magel_resource_cleanup.cpp` consumes borrow control-flow facts. It merges drop for the same precise storage identity: remove a later operation when all incoming paths have terminated the resource, or guard it when only some paths have. Reinitialization, rebinding and allocation update the proof. Dynamic indices, ambiguous origins and opaque cross-call effects do not prove identity. Runtime release remains non-idempotent and reports MGL1205 for unmerged repeated releases; ordinary reads and writes still reject use after release.

Explicit close on a named aggregate uses a compiler-local closed flag, updated on the executed path and reset at construction. Automatic close and explicit drop share that flag without adding a runtime tombstone API.

## Full-expression temporary ownership

Fresh string arguments borrowed by printing, formatting and file writes are first evaluated into internal owning temporaries. Calls run before reverse-order cleanup at the full-expression boundary. Transferring a result to a named owner, return value or another temporary clears the source slot. Loop iterations cannot release a previous iteration's object again. Literals and empty slots need no extra owner; concatenation chains retain the single-join allocation path.

## VM and restart

The VM heap in `library/stackvm/parts/machine_natives.cpp` splits free intervals and merges adjacent released intervals. A watchdog restart ends every old execution context before resetting arenas and allocation identities. There is no sweeping API or implicit garbage collection.

## fn borrowing research

The capture-analysis organization was checked against Rust's
[upvar.rs](https://github.com/rust-lang/rust/blob/main/compiler/rustc_hir_typeck/src/upvar.rs)
and lifetime-constraint conversion against
[constraint_conversion.rs](https://github.com/rust-lang/rust/blob/main/compiler/rustc_borrowck/src/type_check/constraint_conversion.rs).
Magel retains its own scope-source facts and control-flow checker, with mutable borrowing
as its requested default capture rule.

Capture discovery lives in `CheckLambdaCaptures` in `source/semantics/generic_inference.cpp`;
`source/semantics/artifacts.cpp` prepares stable storage and `source/semantics/boxing.cpp` builds/restores
borrowed environments. `source/semantics/borrow_facts.cpp` propagates source lifetimes through
assignments, returns and aggregate fields. Assignment inference must discover captures
before recording a storage constraint. Copying a closure does not extend its sources.

Asynchronous event entry signatures require static callback sources, including public
stored callback fields. Capturing local variables cannot be hidden behind an ordinary
function-pointer conversion of an event registration entry. Synchronous algorithm callbacks
can borrow the caller's live variables.


## Checking later closure uses

Capture discovery and later uses are modeled separately. `FlowState::captures` records external borrowed places held by function values and aggregate fields. Copies, assignments, tuples and enum payloads propagate these sets; reference fields inside owned captures still depend on their external referents. Reading a callable checks whether a captured place was freed, moved or dropped on a reachable path. The callable itself and its captured sources are checked again after argument evaluation, preventing `callback(move(&source))` from invalidating its environment before invocation. Callable results conservatively inherit known external borrows from arguments and receivers. Reassignment replaces the previous capture set, and destruction after the closure's final use remains valid.

The design follows the explicit loan registration in Rust's [borrow_set.rs](https://github.com/rust-lang/rust/blob/main/compiler/rustc_borrowck/src/borrow_set.rs), capture-mode distinctions in [upvar.rs](https://github.com/rust-lang/rust/blob/main/compiler/rustc_hir_typeck/src/upvar.rs), and projection conflicts in [places_conflict.rs](https://github.com/rust-lang/rust/blob/main/compiler/rustc_borrowck/src/places_conflict.rs). It uses Magel's own place and control-flow analysis without copying Rust implementation code.

Checks cover invalidation and propagation visible within the current function. Opaque host code, raw memory writes and erased interprocedural side effects still depend on interface lifetime contracts. This analysis does not establish the absence of all undefined behavior.
