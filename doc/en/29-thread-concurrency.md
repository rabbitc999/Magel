# Threads, channels and explicit events

[Documentation index](README.md) · [中文](../29-thread-concurrency.md)

## Creation and isolation

The module is `std::thread` and its public nominal handle type is `Thread`. Source execution requires `--threads`; embedded hosts enable `Machine::setThreadsEnabled(true)`. Each worker has its own VM stack, heap and global snapshot. Equal variable names do not imply sharing. Thread creation consumes operating-system resources and is not an unbounded task scheduler.

`create(task)` accepts a static, noncapturing `void fn()`. `createWith<T>(task, value)` copies the initial argument for a static `void fn(T)`. Initial arguments, messages and channels share the same value-transfer checks: scalars, strings and recursively copyable aggregates are supported; references, noncopyable resources, borrowed aggregates and capturing closures cannot cross threads. Static function values remain VM identifiers and never become machine function pointers.

`Thread` remains distinct from integers and can be stored in lists and structs. Handles belong to one VM run and cannot be persisted across runs.

## Completion, cancellation and failure

`wait` joins a target and propagates its original failure. `waitFor(thread, millis)` and `tryWait(thread)` return true after successful joining, false on timeout. Timeout neither cancels the target nor consumes the handle. A joined target cannot be joined again.

`waitResult` and `waitResultFor` return `ThreadResult` with status, the original error and available source trace. `state` returns RUNNING, SUCCEEDED, FAILED or CANCELLED, including after joining. Symbol compression limits available source names.

`requestStop` requests cooperative cancellation. Execution boundaries and VM waits, receives, sleep and channel operations observe it. `sleepFor` is cancellable; `stopRequested` queries the flag. Cancellation terminates execution with CANCELLED rather than entering an arbitrary user cleanup branch. Blocking host I/O can observe cancellation only after returning to the VM; use host timeouts or asynchronous polling for long I/O.

Ordinary failures stay in the target result; explicit waiting can propagate them, and groups cancel sibling tasks and gather results. Panic retains run-level propagation. Exiting releases VM storage and owned host resource records. Cancellation and failure do not execute arbitrary user Drop, defer or scope(exit) code. To run application cleanup, request normal shutdown through a business message and then join. Main execution requests stop and joins remaining children before finishing.

## Bounded communication

`std::concurrency` defines these operation statuses:

| Status | Value | Meaning |
| --- | --- | --- |
| READY | 0 | Operation completed |
| TIMEOUT | 1 | Wait expired or nonblocking receive has no value |
| CLOSED | 2 | Queue closed, target exited or channel reclaimed |
| CANCELLED | 3 | Execution cancelled |
| FULL | 4 | Nonblocking send found a full queue |
| FAILED | 5 | Thread/task failed |
| INVALID | 6 | Invalid handle |

`Receive<T>` contains status and value; read value only when isReady() succeeds. Communication timeouts use nonnegative milliseconds, -1 means unlimited, and values below -1 fail.

Each thread inbox holds at most 256 messages. `std::message::sendThread<T>` / `receiveThread<T>` retain blocking operations; sendFor/trySend and receiveFor/tryReceive supply bounded and nonblocking operations. `std::thread` also wraps String messages. `closeInbox()` rejects new messages and allows queued messages to drain. Senders must handle closed or exited targets.

Use **`std::concurrency::Channel<T>`** across threads; `std::collections` also has a local queue with a different contract. Channel capacity is 1..1048576 with at most 1024 active channels per run. Copies share the queue handle, while payloads are copied by value. Multiple producers and consumers use a bounded queue with backpressure.

send/receive, sendFor/receiveFor and trySend/tryReceive provide blocking, timed and nonblocking operations. close is idempotent, rejects new sends, drains queued values and then returns CLOSED. Closed empty channels reclaim their slots. Waiters retain queue storage while blocked, including during close. A default zero handle is invalid. Explicitly close unused channels; run teardown releases remaining slots.

Shared<T> get/set/swap are individually atomic; get followed by set is not a transaction. AtomicInt64.fetchAdd returns the old value with 64-bit wrapping arithmetic. Arbitrary shared object references and user callbacks while holding runtime locks are not exposed.

## Groups and pools

ThreadGroup.start creates and registers a task; add takes responsibility for joining an existing task. cancelAll requests cancellation. waitAll joins all tasks, cancels siblings on ordinary failure, returns whether all succeeded and retains the first failure in error. Repeated waitAll returns the cached result. Completed groups reject additions. Scope exit cancels and joins unfinished tasks. Do not separately join handles owned by a group. Duplicate entries, the current thread and the main thread are rejected. Copying ThreadGroup, ThreadPool or Timer values does not clone their tasks or timer sources. Use references or move to preserve a single cleanup owner instead of copying owners.

ThreadPool.create(workers, capacity) creates fixed workers and a bounded static-task queue. submit/submitFor accept noncapturing void fn() tasks. closeAndWait closes submissions, drains queued work and joins workers; task failure stops the group. cancelAll cancels workers and discards unstarted work; closeAndWait then collects results. Drop cancels and joins. Worker count is 1..1024, subject to host resources.

Use pools for short computation. Long blocking I/O occupies workers; prefer dedicated threads, host timeouts or asynchronous I/O. Pool tasks must avoid waiting for unstarted work in the same pool or indefinitely submitting to their own full queue.

## Explicit events and timers

std::event runs application handlers only after explicit receive/receiveFor/tryReceive. Event fields are timer, tag, source and missed; host events have timer 0. Each execution context has a bounded 256-event queue. Full delivery returns FULL so the host can drop, merge or retry deliberately.

NativeCallContext.eventPoster() and Machine.eventPoster() return weak-reference posting functions. Posting never runs a user callback. Closed, finished or destroyed targets return CLOSED; a poster may safely outlive its host callback. Posting functions and bounded queue storage are owned by the host and do not retain the VM arena. Raw Machine pointers are not asynchronous cross-thread invocation handles.

Timer.afterMillis/afterNanos create one-shot timers; everyMillis/everyNanos create periodic timers. Timers start immediately. Durations must be positive; creation failure yields an invalid timer. A context allows at most 1024 timers. Explicit receive computes deadlines using a shared monotonic-clock service without one OS thread per timer. This is not hard real time. Overdue periods merge into one notification and missed counts additional elapsed periods. stop and Drop remove the source and its queued notices. event.close disables timers while allowing queued events to drain.

std::watchdog checks deadlines and kicks directly at VM execution checks without user event handlers. Budgets use nanoseconds with a host time callback, otherwise executed instructions. Disabled watchdogs preserve the default highest optimization path.

The coroutine module, coroutine handles/scheduling and old interrupt-handler registration/implicit callback delivery are removed. Language block-expression yield and level break keep their value semantics. Migrate callback consumers to explicit receive loops. Retry NET_PENDING with bounded waits or sleepFor instead of busy polling.

Runnable samples: [channel](../../example/samples/thread_channel.mgl), [pool](../../example/samples/thread_pool.mgl), [events](../../example/samples/event_timer.mgl). API: [threads](../library/std-thread.md), [communication](../library/std-concurrency.md), [events](../library/std-event.md).
