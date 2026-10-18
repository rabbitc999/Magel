// ---------------------------------------------------------------------------
//  AOT 运行时桥
// ---------------------------------------------------------------------------

/**
 * @brief 将整数封装为运行时值。
 * @param value 待转换、保存或参与计算的值。
 * @retval 保存将整数封装为运行时值结果的对象。
 */
RuntimeValue RuntimeValue::ofInt(std::int64_t value)
{
    RuntimeValue result;
    result.integer = value;
    return result;
}

/**
 * @brief 将双精度浮点数封装为运行时值。
 * @param value 待转换、保存或参与计算的值。
 * @retval 保存将双精度浮点数封装为运行时值结果的对象。
 */
RuntimeValue RuntimeValue::ofFloat(double value)
{
    RuntimeValue result;
    result.isFloat = true;
    result.real = value;
    return result;
}

/**
 * @brief 将文本封装为运行时值。
 * @param value 待转换、保存或参与计算的值。
 * @retval 保存将文本封装为运行时值结果的对象。
 */
RuntimeValue RuntimeValue::ofText(Memory::String value)
{
    RuntimeValue result;
    result.isText = true;
    result.text = std::move(value);
    return result;
}

namespace
{

/**
 * @brief `RuntimeValue` → 解释器内部的 `Slot`。
 * @param value 待转换、保存或参与计算的值。
 * @retval 封装为原生值的运行时结果。
 */
Slot bridgeToSlot(const RuntimeValue &value)
{
    if (value.isText) return Slot::fromText(value.text);
    if (value.isFloat) return Slot::fromDouble(value.real);
    return Slot::fromInt(value.integer);
}

/**
 * @brief 将虚拟机内部值槽转换为桥接运行时值。
 *
 * `Slot` → `RuntimeValue`。
 * @param slot 待转换或传递的虚拟机值槽。
 * @retval 保存将虚拟机内部值槽转换为桥接运行时值结果的对象。
 */
RuntimeValue bridgeFromSlot(const Slot &slot)
{
    if (slot.isText) return RuntimeValue::ofText(slot.text);
    if (slot.isFloat) return RuntimeValue::ofFloat(slot.real);
    return RuntimeValue::ofInt(slot.integer);
}

/**
 * @brief 桥版协程槽：**一条宿主线程**当"绿色线程"的调用栈。
 *
 * AOT 生成的是普通 C++ 函数，`yield` 没法在函数中途保存 / 恢复调用栈，所以每个协程
 * 单开一条宿主线程、用 `go` / `yielded` 两个标志和调度器做**协作式**交接：任一时刻
 * 只有一条协程在跑，它们共享入口那台 `RuntimeBridge`（全局 / 堆 / 对象表都是同一份，
 * 与解释器里协程共享同一台 Machine 一致）。
 */
struct BridgeCoroutine
{
    std::int64_t fptr = 0;
    int state = 0;  ///< 0 Ready / 1 Suspended / 2 Running / 3 Waiting / 4 Receiving / 5 Done
    std::size_t parent = 0;
    std::size_t waitingFor = 0;
    Memory::Vector<std::shared_ptr<TransferPacket>> inbox;
    std::thread host;
    bool started = false;
    bool go = false;
    bool yielded = false;
    bool finished = false;
    bool failed = false;
    Memory::String error;
};

struct BridgeCoroutineRuntime
{
    std::mutex mutex;
    std::condition_variable changed;
    Memory::Vector<std::shared_ptr<BridgeCoroutine>> slots;  ///< [0] = 入口线程占位。
    bool closing = false;
};

/**
 * 当前线程正在跑哪个协程（入口线程 = 0）。
 */
thread_local std::size_t g_bridgeCoroutine = 0u;

/**
 * 桥协程状态（与 `BridgeCoroutine::state` 一致）。
 */
enum : int
{
    kBridgeCoroutineReady = 0,
    kBridgeCoroutineSuspended = 1,
    kBridgeCoroutineRunning = 2,
    kBridgeCoroutineWaiting = 3,
    kBridgeCoroutineReceiving = 4,
    kBridgeCoroutineDone = 5,
};

}  // namespace

struct RuntimeBridge::Impl
{
    OutputSink outputSink = nullptr;
    void *outputUser = nullptr;
    bool outputKeepText = true;
#if STACKVM_WITH_PANIC_TRACE
    mutable std::mutex traceMutex;
    Memory::Map<std::thread::id, Memory::Vector<DebugFrame>> activeTrace;
    Memory::Vector<DebugFrame> failureTrace;
#endif
    DataArea *data = nullptr;
    const NativeRegistry *registry = nullptr;
    FileApi *fileApi = nullptr;
    bool threadsEnabled = false;
    HeapAllocator *heapAllocator = nullptr;
    Heap defaultHeap;
    HeapAllocator *heap = &defaultHeap;
    ObjectStore objects;
    Memory::Vector<std::uint64_t> globals;  ///< 全局变量块（`McGlobals`，8 字节对齐）。
    Memory::String text;
    Memory::Vector<Memory::String> input;
    std::size_t inputIndex = 0;
    Memory::Vector<Memory::String> programArguments;
    int bigFloatPrecision = 34;
    bool exitRequested = false;
    int exitCode = 0;
    NativeInvoker invokerFn;  ///< `NativeContext::invoker` 用（胶水回调）。

    /* 多线程 / 消息 / 协程 / 定时器（AOT 桥复用的既有运行时）。 */
    std::shared_ptr<ThreadRuntime> threads;              ///< 同一进程的所有线程共享。
    std::shared_ptr<SharedRuntime> shared;               ///< `std::concurrency::Shared` 的取值表。
    std::shared_ptr<BridgeCoroutineRuntime> coroutines;  ///< 协程调度（每条桥一套）。
    TimerRuntime timers;                                 ///< `std::event::Timer` 定时器表。
    std::size_t threadId = 0u;
    std::size_t parentThreadId = 0u;
    std::size_t boundaryCount = 0u;
    std::size_t steps = 0u;
    bool interruptActive = false;
    InstructionHook instructionHook;  ///< 执行线程上的宿主泵钩子。
    std::size_t hookInterval = 0u;
    std::size_t hookSteps = 0u;
    bool hookActive = false;  ///< 防止宿主钩子重入。
    bool ownsRuntime = true;  ///< 主桥负责建线程组；子线程桥共享父桥的组。
    Memory::RuntimeAllocator *allocator = nullptr;
    std::size_t heapBase = kRuntimeCellBytes;
};

/**
 * @brief 按宿主线程隔离 AOT 函数帧。
 * @param function 函数标签。
 */
void RuntimeBridge::enterTrace(const Memory::String &function)
{
#if STACKVM_WITH_PANIC_TRACE
    std::lock_guard<std::mutex> guard(impl_->traceMutex);
    DebugFrame frame;
    frame.function = function;
    frame.location.function = function;
    frame.threadId = impl_->threadId;
    impl_->activeTrace[std::this_thread::get_id()].push_back(std::move(frame));
#else
    (void)function;
#endif
}
/**
 * @brief 更新当前线程最内层帧的原始源码位置。
 * @param file 文件。
 * @param line 行号。
 */
void RuntimeBridge::locateTrace(const Memory::String &file, std::int32_t line)
{
#if STACKVM_WITH_PANIC_TRACE
    std::lock_guard<std::mutex> guard(impl_->traceMutex);
    auto found = impl_->activeTrace.find(std::this_thread::get_id());
    if (found != impl_->activeTrace.end() && !found->second.empty())
    {
        auto &frame = found->second.back();
        if (frame.location.file != file) frame.location.file = file;
        frame.location.line = line;
    }
#else
    (void)file;
    (void)line;
#endif
}
/**
 * @brief 更新字面量文件名的源码位置，热路径不创建临时拥有字符串。
 * @param file 以零结尾的文件名；空指针表示空文件名。
 * @param line 原始源码行号。
 * @note 文件变化时才复制；始终沿用调用帧的互斥保护。
 */
void RuntimeBridge::locateTrace(const char *file, std::int32_t line)
{
#if STACKVM_WITH_PANIC_TRACE
    std::lock_guard<std::mutex> guard(impl_->traceMutex);
    auto found = impl_->activeTrace.find(std::this_thread::get_id());
    if (found != impl_->activeTrace.end() && !found->second.empty())
    {
        auto &frame = found->second.back();
        const char *name = file ? file : "";
        if (frame.location.file != name) frame.location.file = name;
        frame.location.line = line;
    }
#else
    (void)file;
    (void)line;
#endif
}
/**
 * @brief 在错误帧退出之前保留第一份快照。
 */
void RuntimeBridge::leaveTrace()
{
#if STACKVM_WITH_PANIC_TRACE
    std::lock_guard<std::mutex> guard(impl_->traceMutex);
    auto found = impl_->activeTrace.find(std::this_thread::get_id());
    if (found == impl_->activeTrace.end()) return;
    if (failed() && impl_->failureTrace.empty()) impl_->failureTrace = found->second;
    if (!found->second.empty()) found->second.pop_back();
    if (found->second.empty()) impl_->activeTrace.erase(found);
#endif
}
/**
 * @brief 保存子线程已完成的失败快照。
 * @param trace 子线程栈。
 */
void RuntimeBridge::adoptFailureTrace(const Memory::Vector<DebugFrame> &trace)
{
#if STACKVM_WITH_PANIC_TRACE
    std::lock_guard<std::mutex> guard(impl_->traceMutex);
    if (impl_->failureTrace.empty() && !trace.empty()) impl_->failureTrace = trace;
#else
    (void)trace;
#endif
}
/**
 * @brief 读取错误快照。
 * @return 从外到内的 AOT 调用帧。
 */
Memory::Vector<DebugFrame> RuntimeBridge::callStack() const
{
#if STACKVM_WITH_PANIC_TRACE
    std::lock_guard<std::mutex> guard(impl_->traceMutex);
    return impl_->failureTrace;
#else
    return {};
#endif
}

/**
 * @brief 原生 panic 请求停止整个 MAGEL 线程组。
 */
void RuntimeBridge::publishPanic()
{
#if STACKVM_WITH_PANIC_TRACE
    {
        std::lock_guard<std::mutex> guard(impl_->traceMutex);
        const auto active = impl_->activeTrace.find(std::this_thread::get_id());
        if (active != impl_->activeTrace.end()) impl_->failureTrace = active->second;
    }
#endif
    if (!impl_->threads) return;
    auto group = impl_->threads;
    {
        std::lock_guard<std::mutex> guard(group->mutex);
        if (!group->panicked.load(std::memory_order_relaxed))
        {
            group->panicError = error_;
            group->panicThread = impl_->threadId;
#if STACKVM_WITH_PANIC_TRACE
            group->panicTrace = callStack();
#endif
            group->closing = true;
            group->panicked.store(true, std::memory_order_release);
        }
    }
    group->changed.notify_all();
    if (impl_->coroutines)
    {
        std::lock_guard<std::mutex> guard(impl_->coroutines->mutex);
        impl_->coroutines->closing = true;
        impl_->coroutines->changed.notify_all();
    }
}
/**
 * @brief 保存错误并传播全程序 panic。
 * @param message 错误消息。
 */
void RuntimeBridge::fail(const Memory::String &message)
{
    error_ = message;
    if (message.rfind("panic", 0u) == 0u) publishPanic();
}

/**
 * @brief 初始化 AOT 与虚拟机之间的运行时桥接状态。
 */
RuntimeBridge::RuntimeBridge() : impl_(Memory::makeUnique<Impl>())
{
    /* 胶水回调：把宿主 `NativeValue` 参数转成 `RuntimeValue` 后交给 AOT 侧的函数指针
     * 分派器；AOT 侧没装分派器就报"不支持"。 */
    impl_->invokerFn = [this](std::int64_t functionPointer,
                              const Memory::Vector<NativeValue> &arguments,
                              NativeValue &resultValue) -> bool {
        if (!invoker_)
        {
            error_ = "当前 AOT 运行时没有装函数指针分派器";
            return false;
        }
        Memory::Vector<RuntimeValue> converted;
        converted.reserve(arguments.size());
        for (const NativeValue &argument : arguments)
        {
            if (argument.isText())
                converted.push_back(RuntimeValue::ofText(argument.text));
            else if (argument.isFloat())
                converted.push_back(RuntimeValue::ofFloat(argument.floating));
            else
                converted.push_back(RuntimeValue::ofInt(argument.integer));
        }
        RuntimeValue returned;
        if (!invoker_(functionPointer, converted, returned))
        {
            if (error_.empty()) error_ = "AOT 函数指针调用失败";
            return false;
        }
        if (returned.isText)
            resultValue = NativeValue::fromText(returned.text);
        else if (returned.isFloat)
            resultValue = NativeValue::fromFloat(returned.real);
        else
            resultValue = NativeValue::fromInt(returned.integer);
        return true;
    };
}

/**
 * @brief 释放桥接运行时及其关联对象。
 */
RuntimeBridge::~RuntimeBridge() = default;

/**
 * @brief 挂上自定义 VM 字节堆；分配器生命周期须覆盖 run()。传 nullptr 恢复默认实现。
 *
 * 同一对象可连续执行多个程序；每次 run() 都会先调用分配器的 reset(base)。
 * @param allocator 运行时字节堆分配器。
 */
void RuntimeBridge::setHeapAllocator(HeapAllocator *allocator)
{
    heapAllocator_ = allocator;
    impl_->heapAllocator = allocator;
    impl_->heap = (allocator == nullptr) ? static_cast<HeapAllocator *>(&impl_->defaultHeap) : allocator;
}

/**
 * @brief 设置传递给被执行程序的命令行参数。
 * @param arguments 按调用顺序保存的实参列表。
 */
void RuntimeBridge::setProgramArguments(Memory::Vector<Memory::String> arguments)
{
    impl_->programArguments = std::move(arguments);
}

/**
 * @brief 设置按函数地址调用的宿主回调。
 * @param invoker 调用运行时函数的宿主回调。
 */
void RuntimeBridge::setInvoker(
    Memory::Function<bool(std::int64_t, const Memory::Vector<RuntimeValue> &, RuntimeValue &)>
        invoker)
{
    invoker_ = std::move(invoker);
}

/**
 * @brief 生成代码的分派器：把函数指针（数据区里的标签地址）与实参交给 AOT 生成的
 *        `magel_aot_callptr`，`rt` 是**要跑的桥**。
 *
 * 线程创建与协程 / 中断交付都靠它：线程会新建一台桥再调回来（互不干扰的堆 /
 * 对象表 / 全局），协程与中断交付则直接用调用方的桥。
 * @param runner 执行函数体的宿主回调。
 */
void RuntimeBridge::setFunctionRunner(
    Memory::Function<bool(RuntimeBridge &, std::int64_t,
                          const Memory::Vector<RuntimeValue> &, RuntimeValue &)>
        runner)
{
    runner_ = std::move(runner);
}

/**
 * @brief 设置全局数据区所需的字节数。
 * @param bytes 内存区域或数据的字节数。
 */
void RuntimeBridge::setGlobalsSize(std::size_t bytes) { globalsSize_ = bytes; }

/**
 * @brief 取得当前全局数据区的内存块。
 * @retval 对应对象的指针。
 */
void *RuntimeBridge::globalsBlock() const
{
    return impl_->globals.empty() ? nullptr : static_cast<void *>(impl_->globals.data());
}

/**
 * @brief 初始化一次 AOT 程序执行所需的数据与调度状态。
 * @param heapBase 字节堆在虚拟地址空间中的起始地址。
 * @retval 满足检查条件或操作完成时为 true，检查失败或无法完成时为 false。
 */
bool RuntimeBridge::begin(std::size_t heapBase)
{
    impl_->data = data_;
    impl_->registry = registry_;
    impl_->fileApi = fileApi_;
    impl_->threadsEnabled = threadsEnabled_;
    impl_->heapAllocator = heapAllocator_;
    impl_->heap = (heapAllocator_ == nullptr) ? static_cast<HeapAllocator *>(&impl_->defaultHeap) : heapAllocator_;
    if (heapBase == 0u)
    {
        heapBase = (data_ == nullptr || data_->size() == 0u) ? kRuntimeCellBytes : data_->size();
    }
    impl_->heapBase = heapBase;
    impl_->heap->reset(heapBase);
    impl_->objects.objects.clear();
    impl_->objects.next = kObjectHandleBase;
    impl_->objects.materializedHeap.clear();
    impl_->objects.materializedObjects.clear();
    impl_->text.clear();
    impl_->input.clear();
    impl_->inputIndex = 0u;
    impl_->bigFloatPrecision = 34;
    impl_->exitRequested = false;
    impl_->exitCode = 0;
    /* 全局变量块：每个桥一份（线程各自独立，协程共享）。 */
    impl_->globals.assign(globalsSize_ == 0u ? 1u : ((globalsSize_ + 7u) / 8u), 0u);
    impl_->timers.slots.clear();
    impl_->timers.pending.clear();
    impl_->timers.running = 0u;
    impl_->boundaryCount = 0u;
    impl_->steps = 0u;
    impl_->interruptActive = false;
    impl_->instructionHook = {};
    impl_->hookInterval = 0u;
    impl_->hookSteps = 0u;
    impl_->hookActive = false;
    impl_->allocator = Memory::currentRuntimeAllocator();
    if (impl_->ownsRuntime)
    {
        if (!impl_->shared) impl_->shared = Memory::makeShared<SharedRuntime>();
        if (impl_->threadsEnabled)
        {
            impl_->threads = Memory::makeShared<ThreadRuntime>();
            impl_->threads->workers.emplace(0u, Memory::makeShared<ThreadRuntime::Worker>());
            impl_->threadId = 0u;
            impl_->parentThreadId = 0u;
        }
        if (!impl_->threadsEnabled) impl_->threads.reset();
        impl_->coroutines = Memory::makeShared<BridgeCoroutineRuntime>();
        impl_->coroutines->slots.push_back(Memory::makeShared<BridgeCoroutine>());
    }
    else if (!impl_->coroutines)
    {
        impl_->coroutines = Memory::makeShared<BridgeCoroutineRuntime>();
        impl_->coroutines->slots.push_back(Memory::makeShared<BridgeCoroutine>());
    }
#if STACKVM_WITH_PANIC_TRACE
    {
        std::lock_guard<std::mutex> guard(impl_->traceMutex);
        impl_->activeTrace.clear();
        impl_->failureTrace.clear();
    }
#endif
    error_.clear();
    return true;
}

/**
 * @brief 结束本次程序执行并清理桥接状态。
 */
void RuntimeBridge::end()
{
    if (impl_->coroutines)
    {
        {
            std::lock_guard<std::mutex> guard(impl_->coroutines->mutex);
            impl_->coroutines->closing = true;
            impl_->coroutines->changed.notify_all();
        }
        for (auto &slot : impl_->coroutines->slots)
            if (slot && slot->host.joinable()) slot->host.join();
    }
    if (impl_->ownsRuntime && impl_->threads)
    {
        {
            std::lock_guard<std::mutex> guard(impl_->threads->mutex);
            impl_->threads->closing = true;
        }
        impl_->threads->changed.notify_all();
        for (auto &item : impl_->threads->workers)
        {
            if (item.first == 0u) continue;
            if (item.second->host.joinable()) item.second->host.join();
            if (!item.second->reported)
            {
                impl_->text += item.second->result.text;
                if (!item.second->result.ok && error_.empty())
                {
                    error_ = "线程 " + Memory::toString(item.first) + " 失败：" + item.second->result.error;
                    adoptFailureTrace(item.second->result.sourceTrace);
                }
                item.second->reported = true;
            }
        }
    }
    if (impl_->threads && impl_->threads->panicked.load(std::memory_order_acquire)) boundary();
    impl_->instructionHook = {};
    impl_->hookInterval = 0u;
    impl_->objects.objects.clear();
    impl_->objects.materializedHeap.clear();
    impl_->objects.materializedObjects.clear();
    // 线程与协程均已结束，下一轮初始化之前撤销旧释放身份。
    if (Memory::restartSignal().pending()) impl_->heap->sweep();
}

/**
 * @brief 指令边界：交付到期软件定时器与队列里的中断事件（AOT 在每个基本块开头调用）。
 * @return 有运行期错误返回 false。
 */
bool RuntimeBridge::boundary()
{
    Impl &impl = *impl_;
    if (impl.threads && impl.threads->panicked.load(std::memory_order_acquire))
    {
        auto group = impl.threads;
        std::lock_guard<std::mutex> guard(group->mutex);
        error_ = group->panicThread == 0u ? group->panicError :
                                            "线程 " + Memory::toString(group->panicThread) + " 失败：" + group->panicError;
#if STACKVM_WITH_PANIC_TRACE
        {
            std::lock_guard<std::mutex> traceGuard(impl.traceMutex);
            impl.failureTrace = group->panicTrace;
        }
#endif
        return false;
    }
    if (!error_.empty()) return false;
    if (impl.interruptActive) return true;
    if (impl.hookActive) return true;
    if (impl.hookInterval && impl.instructionHook && ++impl.hookSteps % impl.hookInterval == 0u)
    {
        auto hook = impl.instructionHook;  // 允许钩子在执行时注销自身。
        impl.hookActive = true;
        try
        {
            hook(InstructionHookInfo{ impl.hookSteps, 0u, g_bridgeCoroutine, "aot-boundary" });
        } catch (const std::exception &exception)
        {
            impl.hookActive = false;
            fail(Memory::String("AOT 执行钩子失败：") + exception.what());
            return false;
        } catch (...)
        {
            impl.hookActive = false;
            fail("AOT 执行钩子抛出未知异常");
            return false;
        }
        impl.hookActive = false;
        if (!error_.empty()) return false;
    }
    if (impl.timers.running == 0u && impl.timers.pending.empty()) return true;
    ++impl.steps;
    ++impl.boundaryCount;
    /* 定时器按 16 拍一次节流（与解释器一致），省掉每次读时钟。 */
    const bool checkTimers = impl.timers.running != 0u && (impl.boundaryCount & 0xfu) == 0u;
    const std::uint64_t now = checkTimers ? static_cast<std::uint64_t>(impl.steps) : 0u;
    impl.interruptActive = true;
    std::size_t head = 0u;
    for (;;)
    {
        if (checkTimers)
        {
            for (std::size_t index = 0; index < impl.timers.slots.size(); ++index)
            {
                TimerSlot &slot = impl.timers.slots[index];
                if (!slot.alive || !slot.running || slot.period <= 0) continue;
                if (slot.thread != impl.threadId) continue;
                if (slot.deadline < 0)
                    slot.deadline = static_cast<std::int64_t>(now) + slot.period;
                if (static_cast<std::uint64_t>(slot.deadline) > now) continue;
                slot.deadline = static_cast<std::int64_t>(now) + slot.period;
                impl.timers.pending.push_back(
                    InterruptEvent{ static_cast<std::int64_t>(index + 1u), 0, 0, slot.thread });
            }
        }
        if (head >= impl.timers.pending.size()) break;
        const InterruptEvent event = impl.timers.pending[head++];
        if (event.thread != impl.threadId) continue;
        std::int64_t callback = event.callback;
        if (event.timer > 0)
        {
            const std::size_t index = static_cast<std::size_t>(event.timer - 1);
            if (index >= impl.timers.slots.size()) continue;
            TimerSlot &slot = impl.timers.slots[index];
            if (!slot.alive || slot.thread != impl.threadId) continue;
            if (callback == 0) callback = slot.callback;
            if (callback == 0) continue;
            ++slot.ticks;
        }
        else if (callback == 0)
        {
            continue;
        }
        RuntimeValue ignored;
        const bool delivered =
            runner_ && runner_(*this, callback, Memory::Vector<RuntimeValue>{ RuntimeValue::ofInt(event.source) }, ignored);
        if (!delivered || !error_.empty()) break;
    }
    impl.timers.pending.clear();
    impl.interruptActive = false;
    return error_.empty();
}

/**
 * @brief 从桥接调度器选取下一个可运行线程。
 * @param after 选取可运行线程时的起始线程编号。
 * @retval 从桥接调度器选取下一个可运行线程。
 */
std::size_t RuntimeBridge::bridgeNextRunnable(std::size_t after)
{
    auto runtime = impl_->coroutines;
    if (!runtime) return 0u;
    const std::size_t count = runtime->slots.size();
    if (count <= 1u) return count;
    for (std::size_t offset = 1u; offset <= count; ++offset)
    {
        const std::size_t index = (after + offset) % count;
        const int state = runtime->slots[index]->state;
        if (state == kBridgeCoroutineReady || state == kBridgeCoroutineSuspended) return index;
    }
    return count;
}

/**
 * @brief 在桥接执行边界推进线程调度。
 * @param id 待查找或操作的对象编号。
 * @retval 满足检查条件或操作完成时为 true，检查失败或无法完成时为 false。
 */
bool RuntimeBridge::bridgeSchedule(std::size_t id)
{
    auto runtime = impl_->coroutines;
    if (!runtime || id >= runtime->slots.size())
    {
        error_ = "AOT：协程句柄越界";
        return false;
    }
    auto slot = runtime->slots[id];
    {
        std::unique_lock<std::mutex> guard(runtime->mutex);
        if (slot->finished || slot->state == kBridgeCoroutineDone) return error_.empty();
        if (!slot->started)
        {
            slot->started = true;
            slot->state = kBridgeCoroutineRunning;
            slot->go = true;
            RuntimeBridge *self = this;
            const std::size_t coroutineId = id;
            const std::int64_t fptr = slot->fptr;
            auto group = runtime;
            slot->host = std::thread([self, group, slot, coroutineId, fptr]() {
                Memory::AllocationScope scope(self->impl_->allocator);
                g_bridgeCoroutine = coroutineId;
                {
                    std::unique_lock<std::mutex> lock(group->mutex);
                    group->changed.wait(lock, [&] { return slot->go || group->closing; });
                    if (group->closing)
                    {
                        slot->finished = true;
                        slot->failed = true;
                        slot->error = "程序结束，协程被中止";
                        group->changed.notify_all();
                        return;
                    }
                    slot->go = false;
                    slot->state = kBridgeCoroutineRunning;
                }
                RuntimeValue ignored;
                const bool ok = self->runner_ && self->runner_(*self, fptr, Memory::Vector<RuntimeValue>{}, ignored);
                {
                    std::lock_guard<std::mutex> lock(group->mutex);
                    slot->finished = true;
                    if (!ok || self->failed())
                    {
                        slot->failed = true;
                        if (slot->error.empty()) slot->error = self->error();
                    }
                    group->changed.notify_all();
                }
            });
        }
        else
        {
            slot->state = kBridgeCoroutineRunning;
            slot->go = true;
        }
        runtime->changed.notify_all();
        runtime->changed.wait(guard, [&] { return slot->yielded || slot->finished; });
        slot->yielded = false;
    }
    if (slot->finished)
    {
        std::lock_guard<std::mutex> guard(runtime->mutex);
        slot->state = kBridgeCoroutineDone;
        for (auto &other : runtime->slots)
        {
            if (other && other->state == kBridgeCoroutineWaiting && other->waitingFor == id)
            {
                other->state = kBridgeCoroutineReady;
                other->waitingFor = 0u;
            }
        }
        if (slot->failed && error_.empty())
            error_ = slot->error.empty() ? Memory::String("协程运行失败") : slot->error;
    }
    return error_.empty();
}

/**
 * @brief 判断是否已注册指定原生函数。
 * @param name 名称文本。
 * @retval 已注册指定原生函数时为 true，否则为 false。
 */
bool RuntimeBridge::hasNative(const Memory::String &name) const
{
    if (kSlotNatives.find(name) != kSlotNatives.end()) return true;
    return registry_ != nullptr && registry_->find(name) != nullptr;
}

/**
 * @brief 查找并调用已注册的原生函数。
 * @param name 名称文本。
 * @param args 按接口约定顺序传入的实参列表。
 * @param result 接收处理结果的对象。
 * @retval 满足检查条件或操作完成时为 true，检查失败或无法完成时为 false。
 */
bool RuntimeBridge::callNative(const Memory::String &name,
                               const Memory::Vector<RuntimeValue> &args, RuntimeValue &result)
{
    if (impl_->threads && impl_->threads->panicked.load(std::memory_order_acquire)) return boundary();
    if (!error_.empty()) return false;

    /* 0. 桥特判：消息 / 共享值 / 线程 / 协程 —— 解释器在 `Machine::run` 里特判的同一批，
     *    AOT 把同样的语义搬到桥上；定时器 / 中断继续走下面的 `kSlotNatives`（挂上
     *    `context.timers` / `context.threadId` 即可）。 */
    const bool messageSendThread = name == "__vm_message_send_thread";
    const bool messageReceiveThread = name == "__vm_message_receive_thread";
    const bool messageSendCoroutine = name == "__vm_message_send_coroutine";
    const bool messageReceiveCoroutine = name == "__vm_message_receive_coroutine";
    const bool sharedCreate = name == "__vm_shared_create";
    const bool sharedGet = name == "__vm_shared_get";
    const bool sharedSet = name == "__vm_shared_set";
    const bool sharedSwap = name == "__vm_shared_swap";
    if (messageSendThread || messageReceiveThread || messageSendCoroutine || messageReceiveCoroutine || sharedCreate || sharedGet || sharedSet || sharedSwap)
    {
        const bool send = messageSendThread || messageSendCoroutine;
        const bool withTarget = send || sharedGet || sharedSet || sharedSwap;
        const bool withValue = send || sharedCreate || sharedSet || sharedSwap;
        const std::size_t wanted = (withTarget ? 1u : 0u) + 3u + (withValue ? 1u : 0u);
        if (args.size() != wanted)
        {
            error_ = "消息或共享值调用参数个数错误：" + name;
            return false;
        }
        if ((messageSendThread || messageReceiveThread) && (!impl_->threadsEnabled || !impl_->threads))
        {
            error_ = "多线程未启用；解释运行或 C++ 导出时传 --threads";
            return false;
        }
        auto bridgeContext = [&]() {
            NativeContext context;
            context.heap = impl_->heap;
            context.data = data_;
            context.text = &impl_->text;
            context.textSink = impl_->outputSink;
            context.textSinkUser = impl_->outputUser;
            context.textKeep = &impl_->outputKeepText;
            context.input = &impl_->input;
            context.inputIndex = &impl_->inputIndex;
            context.programArguments = &impl_->programArguments;
            context.objects = &impl_->objects;
            context.timers = &impl_->timers;
            context.threadId = impl_->threadId;
            context.bigFloatPrecision = &impl_->bigFloatPrecision;
            context.exitRequested = &impl_->exitRequested;
            context.exitCode = &impl_->exitCode;
            context.fileApi = fileApi_;
            context.invoker = &impl_->invokerFn;
            return context;
        };
        NativeContext context = bridgeContext();
        const std::size_t base = withTarget ? 1u : 0u;
        const std::int64_t target = withTarget ? args[0].integer : 0;
        const int kind = static_cast<int>(args[base].integer);
        const std::int64_t layout = args[base + 1u].integer;
        Memory::String type;
        const bool validType = nativeReadCString(
            context, static_cast<std::size_t>(args[base + 2u].integer), &type);
        if (!validType || kind < 0 || kind > 4)
        {
            error_ = "消息或共享值的类型描述无效";
            return false;
        }
        std::shared_ptr<TransferPacket> packet;
        if (withValue)
        {
            packet = Memory::makeShared<TransferPacket>();
            if (!captureTransfer(context, bridgeToSlot(args.back()), kind, layout, type,
                                 packet.get()))
            {
                error_ = context.error.empty() ? Memory::String("消息值无法复制") : context.error;
                return false;
            }
        }
        if (sharedCreate || sharedGet || sharedSet || sharedSwap)
        {
            if (!impl_->shared) impl_->shared = Memory::makeShared<SharedRuntime>();
            std::shared_ptr<TransferPacket> previous;
            std::int64_t handle = target;
            {
                std::lock_guard<std::mutex> guard(impl_->shared->mutex);
                if (sharedCreate)
                {
                    handle = impl_->shared->next++;
                    impl_->shared->values.emplace(handle, std::move(packet));
                }
                else
                {
                    const auto found = impl_->shared->values.find(target);
                    if (found != impl_->shared->values.end() && found->second->type == type)
                    {
                        previous = found->second;
                        if (sharedSet || sharedSwap) found->second = std::move(packet);
                    }
                }
            }
            if (sharedCreate)
            {
                result = RuntimeValue::ofInt(handle);
                return true;
            }
            if (!previous)
            {
                error_ = "std::lang::Shared 类型不匹配或句柄无效：" + type;
                return false;
            }
            if (sharedSet)
            {
                result = RuntimeValue::ofInt(0);
                return true;
            }
            Slot restored;
            NativeContext restoreContext = bridgeContext();
            if (!restoreTransfer(restoreContext, *previous, &restored))
            {
                error_ = restoreContext.error.empty() ? Memory::String("共享值无法还原") : restoreContext.error;
                return false;
            }
            result = bridgeFromSlot(restored);
            return true;
        }
        if (send)
        {
            if (messageSendThread)
            {
                auto group = impl_->threads;
                std::lock_guard<std::mutex> guard(group->mutex);
                const auto found = target < 0 ? group->workers.end() : group->workers.find(static_cast<std::size_t>(target));
                if (found == group->workers.end() || found->second->done || group->closing)
                {
                    error_ = "std::thread::send 收到了无效或已结束的线程句柄";
                    return false;
                }
                found->second->inbox.push_back(std::move(packet));
                group->changed.notify_all();
                result = RuntimeValue::ofInt(0);
                return true;
            }
            auto runtime = impl_->coroutines;
            if (!runtime || target < 0 || static_cast<std::size_t>(target) >= runtime->slots.size())
            {
                error_ = "发送消息的协程句柄无效";
                return false;
            }
            std::lock_guard<std::mutex> guard(runtime->mutex);
            auto receiver = runtime->slots[static_cast<std::size_t>(target)];
            if (!receiver || receiver->state == kBridgeCoroutineDone)
            {
                error_ = "发送消息的协程句柄无效或已结束";
                return false;
            }
            receiver->inbox.push_back(std::move(packet));
            if (receiver->state == kBridgeCoroutineReceiving)
                receiver->state = kBridgeCoroutineReady;
            runtime->changed.notify_all();
            result = RuntimeValue::ofInt(0);
            return true;
        }
        /* receive */
        if (messageReceiveThread)
        {
            auto group = impl_->threads;
            std::shared_ptr<TransferPacket> got;
            {
                std::unique_lock<std::mutex> guard(group->mutex);
                const auto self = group->workers.find(impl_->threadId);
                if (self == group->workers.end())
                {
                    error_ = "当前线程不在线程组里";
                    return false;
                }
                self->second->receiving = true;
                group->changed.notify_all();
                for (;;)
                {
                    if (!self->second->inbox.empty() || group->closing || group->deadlocked()) break;
                    guard.unlock();
                    boundary();
                    guard.lock();
                    if (!error_.empty()) return false;
                    group->changed.wait_for(guard, std::chrono::milliseconds(1));
                }
                self->second->receiving = false;
                group->changed.notify_all();
                if (self->second->inbox.empty())
                {
                    Memory::String message = group->closing ? Memory::String("线程收件箱已关闭") : Memory::String("线程接收消息形成死锁");
                    for (const auto &item : group->workers)
                    {
                        if (item.first == 0u || item.second->reported) continue;
                        if (item.second->done && !item.second->result.ok && !item.second->result.error.empty())
                        {
                            item.second->reported = true;
                            message = "线程 " + Memory::toString(item.first) + " 失败：" + item.second->result.error;
                            adoptFailureTrace(item.second->result.sourceTrace);
                            break;
                        }
                    }
                    error_ = message;
                    return false;
                }
                got = self->second->inbox.front();
                self->second->inbox.pop_front();
            }
            if (got->type != type)
            {
                error_ = "线程消息类型不匹配：期望 " + type + "，收到 " + got->type;
                return false;
            }
            NativeContext restoreContext = bridgeContext();
            Slot restored;
            if (!restoreTransfer(restoreContext, *got, &restored))
            {
                error_ = restoreContext.error.empty() ? Memory::String("线程消息无法还原") : restoreContext.error;
                return false;
            }
            result = bridgeFromSlot(restored);
            return true;
        }
        /* 协程收消息：入口线程驱动调度器，协程线程交还控制权等下次被放行。 */
        auto runtime = impl_->coroutines;
        if (!runtime)
        {
            error_ = "协程运行时没有建起来";
            return false;
        }
        std::shared_ptr<TransferPacket> got;
        if (g_bridgeCoroutine != 0u)
        {
            auto slot = runtime->slots[g_bridgeCoroutine];
            for (;;)
            {
                {
                    std::lock_guard<std::mutex> guard(runtime->mutex);
                    if (!slot->inbox.empty())
                    {
                        got = slot->inbox.front();
                        slot->inbox.erase(slot->inbox.begin());
                    }
                }
                if (got) break;
                std::unique_lock<std::mutex> guard(runtime->mutex);
                slot->state = kBridgeCoroutineReceiving;
                slot->yielded = true;
                runtime->changed.notify_all();
                runtime->changed.wait(guard, [&] { return slot->go || runtime->closing; });
                if (runtime->closing)
                {
                    error_ = "程序结束，协程被中止";
                    return false;
                }
                slot->go = false;
                slot->state = kBridgeCoroutineRunning;
            }
        }
        else
        {
            auto mainSlot = runtime->slots[0];
            for (;;)
            {
                {
                    std::lock_guard<std::mutex> guard(runtime->mutex);
                    if (!mainSlot->inbox.empty())
                    {
                        got = mainSlot->inbox.front();
                        mainSlot->inbox.erase(mainSlot->inbox.begin());
                    }
                }
                if (got) break;
                {
                    std::lock_guard<std::mutex> guard(runtime->mutex);
                    mainSlot->state = kBridgeCoroutineReceiving;
                }
                const std::size_t next = bridgeNextRunnable(0u);
                if (next >= runtime->slots.size())
                {
                    error_ = "协程接收消息形成死锁";
                    return false;
                }
                if (!bridgeSchedule(next)) return false;
                std::lock_guard<std::mutex> guard(runtime->mutex);
                mainSlot->state = kBridgeCoroutineRunning;
            }
        }
        if (got->type != type)
        {
            error_ = "协程消息类型不匹配：期望 " + type + "，收到 " + got->type;
            return false;
        }
        NativeContext restoreContext = bridgeContext();
        Slot restored;
        if (!restoreTransfer(restoreContext, *got, &restored))
        {
            error_ = restoreContext.error.empty() ? Memory::String("协程消息无法还原") : restoreContext.error;
            return false;
        }
        result = bridgeFromSlot(restored);
        return true;
    }

    if (name.rfind("std::thread::", 0u) == 0u)
    {
        if (name != "std::thread::create" && name != "std::thread::wait" && name != "std::thread::send" && name != "std::thread::receive" && name != "std::thread::current" && name != "std::thread::parent")
        {
            error_ = "AOT 运行时不认识符号：" + name;
            return false;
        }
        if (!impl_->threadsEnabled || !impl_->threads)
        {
            error_ = "多线程未启用；解释运行或 C++ 导出时传 --threads";
            return false;
        }
        const std::size_t wanted = (name == "std::thread::create" || name == "std::thread::wait") ? 1u : (name == "std::thread::send") ? 2u :
                                                                                                                                         0u;
        if (args.size() != wanted)
        {
            error_ = "线程函数 " + name + " 需要 " + Memory::toString(wanted) + " 个参数，实到 " + Memory::toString(args.size()) + " 个";
            return false;
        }
        if (name == "std::thread::current" || name == "std::thread::parent")
        {
            result = RuntimeValue::ofInt(static_cast<std::int64_t>(
                name == "std::thread::current" ? impl_->threadId : impl_->parentThreadId));
            return true;
        }
        if (name == "std::thread::create")
        {
            const std::int64_t fptr = args[0].integer;
            if (isObjectHandle(fptr))
            {
                error_ = "线程异步入口不支持带捕获的 lambda";
                return false;
            }
            if (fptr == 0 || data_ == nullptr)
            {
                error_ = "std::thread::create 收到了无效任务函数指针";
                return false;
            }
            auto group = impl_->threads;
            std::size_t id = 0u;
            auto worker = Memory::makeShared<ThreadRuntime::Worker>();
            {
                std::lock_guard<std::mutex> guard(group->mutex);
                if (group->closing)
                {
                    error_ = "主线程已经结束，不能再创建线程";
                    return false;
                }
                id = group->nextId++;
                group->workers.emplace(id, worker);
            }
            DataArea *data = data_;
            const NativeRegistry *registry = registry_;
            FileApi *api = fileApi_;
            auto shared = impl_->shared;
            Memory::Vector<Memory::String> arguments = impl_->programArguments;
            const std::size_t globalsSize = globalsSize_;
            const std::size_t heapBase = impl_->heapBase;
            const std::size_t parentId = impl_->threadId;
            auto runner = runner_;
            Memory::RuntimeAllocator *allocator = impl_->allocator;
            worker->host = std::thread([worker, group, shared, data, registry, api, arguments, globalsSize,
                                        heapBase, parentId, id, fptr, runner, allocator]() mutable {
                Memory::AllocationScope scope(allocator);
                RuntimeBridge child;
                child.setData(data);
                child.setNativeRegistry(registry);
                child.setFileApi(api);
                child.setThreadsEnabled(true);
                child.setProgramArguments(std::move(arguments));
                child.setGlobalsSize(globalsSize);
                child.setFunctionRunner(runner);
                child.impl_->ownsRuntime = false;
                child.impl_->threads = group;
                child.impl_->shared = shared;
                child.impl_->threadId = id;
                child.impl_->parentThreadId = parentId;
                child.begin(heapBase);
                RuntimeValue returned;
                const bool ok = runner && runner(child, fptr, Memory::Vector<RuntimeValue>{}, returned);
                worker->result.ok = ok && !child.failed();
                if (!worker->result.ok)
                {
                    worker->result.error = child.error();
                    worker->result.sourceTrace = child.callStack();
                }
                worker->result.text = child.output();
                child.end();
                {
                    std::lock_guard<std::mutex> guard(group->mutex);
                    worker->done = true;
                    group->changed.notify_all();
                }
            });
            result = RuntimeValue::ofInt(static_cast<std::int64_t>(id));
            return true;
        }
        if (name == "std::thread::send")
        {
            const std::int64_t target = args[0].integer;
            const Memory::String message = readCString(args[1].integer);
            auto group = impl_->threads;
            std::lock_guard<std::mutex> guard(group->mutex);
            const auto found = target < 0 ? group->workers.end() : group->workers.find(static_cast<std::size_t>(target));
            if (found == group->workers.end() || found->second->done || group->closing)
            {
                error_ = "std::thread::send 收到了无效或已结束的线程句柄";
                return false;
            }
            auto packet = Memory::makeShared<TransferPacket>();
            packet->type = "string";
            packet->value.isText = true;
            packet->value.text = message;
            found->second->inbox.push_back(std::move(packet));
            group->changed.notify_all();
            result = RuntimeValue::ofInt(0);
            return true;
        }
        if (name == "std::thread::receive")
        {
            auto group = impl_->threads;
            Memory::String message;
            {
                std::unique_lock<std::mutex> guard(group->mutex);
                const auto self = group->workers.find(impl_->threadId);
                if (self == group->workers.end())
                {
                    error_ = "当前线程不在线程组里";
                    return false;
                }
                self->second->receiving = true;
                group->changed.notify_all();
                for (;;)
                {
                    if (!self->second->inbox.empty() || group->closing || group->deadlocked()) break;
                    guard.unlock();
                    boundary();
                    guard.lock();
                    if (!error_.empty()) return false;
                    group->changed.wait_for(guard, std::chrono::milliseconds(1));
                }
                self->second->receiving = false;
                group->changed.notify_all();
                if (self->second->inbox.empty())
                {
                    error_ = group->closing ? Memory::String("线程收件箱已关闭") : Memory::String("线程接收消息形成死锁");
                    return false;
                }
                if (self->second->inbox.front()->type != "string")
                {
                    error_ = "线程消息类型不匹配：期望 string，收到 " + self->second->inbox.front()->type;
                    return false;
                }
                message = self->second->inbox.front()->value.text;
                self->second->inbox.pop_front();
            }
            NativeContext context;
            context.heap = impl_->heap;
            context.data = data_;
            const std::size_t address = nativeAllocString(context, message);
            if (address == 0u)
            {
                error_ = context.error.empty() ? Memory::String("线程消息字符串分配失败") : context.error;
                return false;
            }
            result = RuntimeValue::ofInt(static_cast<std::int64_t>(address));
            return true;
        }
        /* std::thread::wait */
        const std::int64_t raw = args[0].integer;
        auto group = impl_->threads;
        std::shared_ptr<ThreadRuntime::Worker> worker;
        {
            std::unique_lock<std::mutex> guard(group->mutex);
            const auto found = raw <= 0 ? group->workers.end() : group->workers.find(static_cast<std::size_t>(raw));
            if (found == group->workers.end() || static_cast<std::size_t>(raw) == impl_->threadId || found->second->joined)
            {
                error_ = "std::thread::wait 收到了无效、自身或已等待的线程句柄";
                return false;
            }
            worker = found->second;
            worker->joined = true;
            const auto self = group->workers.find(impl_->threadId);
            if (self != group->workers.end())
            {
                self->second->joining = true;
                self->second->joinTarget = static_cast<std::size_t>(raw);
            }
            group->changed.notify_all();
            for (;;)
            {
                if (worker->done || group->closing || (group->deadlocked() && !worker->receiving))
                    break;
                guard.unlock();
                boundary();
                guard.lock();
                if (!error_.empty()) return false;
                group->changed.wait_for(guard, std::chrono::milliseconds(1));
            }
            if (self != group->workers.end())
            {
                self->second->joining = false;
                self->second->joinTarget = 0u;
            }
            group->changed.notify_all();
            if (!worker->done)
            {
                worker->joined = false;
                error_ = group->closing ? Memory::String("线程等待被主程序结束中断") : Memory::String("线程等待形成死锁");
                return false;
            }
        }
        if (worker->host.joinable()) worker->host.join();
        impl_->text += worker->result.text;
        if (!worker->result.output.empty())
        {
            if (!impl_->text.empty()) impl_->text += ' ';
            impl_->text += worker->result.output;
        }
        worker->reported = true;
        if (!worker->result.ok)
        {
            error_ = "线程 " + Memory::toString(raw) + " 失败：" + worker->result.error;
            adoptFailureTrace(worker->result.sourceTrace);
            return false;
        }
        result = RuntimeValue::ofInt(0);
        return true;
    }

    if (name.rfind("std::coroutine::", 0u) == 0u)
    {
        const bool coroutineCreate = name == "std::coroutine::create";
        const bool coroutineWait = name == "std::coroutine::wait";
        const bool coroutineYield = name == "std::coroutine::yield" || name == "std::coroutine::yelid";
        const bool coroutineSend = name == "std::coroutine::send";
        const bool coroutineReceive = name == "std::coroutine::receive";
        const bool coroutineIdentity = name == "std::coroutine::current" || name == "std::coroutine::parent";
        if (!coroutineCreate && !coroutineWait && !coroutineYield && !coroutineSend && !coroutineReceive && !coroutineIdentity)
        {
            error_ = "AOT 运行时不认识符号：" + name;
            return false;
        }
        if (!impl_->coroutines)
        {
            impl_->coroutines = Memory::makeShared<BridgeCoroutineRuntime>();
            impl_->coroutines->slots.push_back(Memory::makeShared<BridgeCoroutine>());
        }
        auto runtime = impl_->coroutines;
        const std::size_t wanted = coroutineSend ? 2u : (coroutineYield || coroutineReceive || coroutineIdentity) ? 0u :
                                                                                                                    1u;
        if (args.size() != wanted)
        {
            error_ = "协程函数 " + name + " 需要 " + Memory::toString(wanted) + " 个参数，实到 " + Memory::toString(args.size()) + " 个";
            return false;
        }
        if (coroutineCreate)
        {
            const std::int64_t fptr = args[0].integer;
            if (isObjectHandle(fptr))
            {
                error_ = "协程异步入口不支持带捕获的 lambda";
                return false;
            }
            if (fptr == 0)
            {
                error_ = "std::coroutine::create 收到了空函数指针";
                return false;
            }
            auto slot = Memory::makeShared<BridgeCoroutine>();
            slot->fptr = fptr;
            slot->state = kBridgeCoroutineReady;
            slot->parent = g_bridgeCoroutine;
            std::size_t index = 0u;
            {
                std::lock_guard<std::mutex> guard(runtime->mutex);
                runtime->slots.push_back(slot);
                index = runtime->slots.size() - 1u;
            }
            result = RuntimeValue::ofInt(static_cast<std::int64_t>(index));
            return true;
        }
        if (coroutineIdentity)
        {
            const std::size_t id = name == "std::coroutine::current" ? g_bridgeCoroutine : (g_bridgeCoroutine < runtime->slots.size() ? runtime->slots[g_bridgeCoroutine]->parent : 0u);
            result = RuntimeValue::ofInt(static_cast<std::int64_t>(id));
            return true;
        }
        if (coroutineSend)
        {
            const std::int64_t raw = args[0].integer;
            const Memory::String message = readCString(args[1].integer);
            if (raw < 0 || static_cast<std::size_t>(raw) >= runtime->slots.size())
            {
                error_ = "std::coroutine::send 收到了无效句柄或字符串";
                return false;
            }
            auto packet = Memory::makeShared<TransferPacket>();
            packet->type = "string";
            packet->value.isText = true;
            packet->value.text = message;
            std::lock_guard<std::mutex> guard(runtime->mutex);
            auto target = runtime->slots[static_cast<std::size_t>(raw)];
            if (!target || target->state == kBridgeCoroutineDone)
            {
                error_ = "std::coroutine::send 收到了无效句柄或字符串";
                return false;
            }
            target->inbox.push_back(std::move(packet));
            if (target->state == kBridgeCoroutineReceiving)
                target->state = kBridgeCoroutineReady;
            runtime->changed.notify_all();
            result = RuntimeValue::ofInt(0);
            return true;
        }
        if (coroutineYield)
        {
            if (g_bridgeCoroutine == 0u) return true;
            auto slot = runtime->slots[g_bridgeCoroutine];
            std::unique_lock<std::mutex> guard(runtime->mutex);
            slot->state = kBridgeCoroutineSuspended;
            slot->yielded = true;
            runtime->changed.notify_all();
            runtime->changed.wait(guard, [&] { return slot->go || runtime->closing; });
            if (runtime->closing)
            {
                error_ = "程序结束，协程被中止";
                return false;
            }
            slot->go = false;
            slot->state = kBridgeCoroutineRunning;
            result = RuntimeValue::ofInt(0);
            return true;
        }
        if (coroutineReceive)
        {
            /* 与 `__vm_message_receive_coroutine` 同路：这里返回 string 快照。 */
            std::shared_ptr<TransferPacket> got;
            if (g_bridgeCoroutine != 0u)
            {
                auto slot = runtime->slots[g_bridgeCoroutine];
                for (;;)
                {
                    {
                        std::lock_guard<std::mutex> guard(runtime->mutex);
                        if (!slot->inbox.empty())
                        {
                            got = slot->inbox.front();
                            slot->inbox.erase(slot->inbox.begin());
                        }
                    }
                    if (got) break;
                    std::unique_lock<std::mutex> guard(runtime->mutex);
                    slot->state = kBridgeCoroutineReceiving;
                    slot->yielded = true;
                    runtime->changed.notify_all();
                    runtime->changed.wait(guard, [&] { return slot->go || runtime->closing; });
                    if (runtime->closing)
                    {
                        error_ = "程序结束，协程被中止";
                        return false;
                    }
                    slot->go = false;
                    slot->state = kBridgeCoroutineRunning;
                }
            }
            else
            {
                auto mainSlot = runtime->slots[0];
                for (;;)
                {
                    {
                        std::lock_guard<std::mutex> guard(runtime->mutex);
                        if (!mainSlot->inbox.empty())
                        {
                            got = mainSlot->inbox.front();
                            mainSlot->inbox.erase(mainSlot->inbox.begin());
                        }
                    }
                    if (got) break;
                    {
                        std::lock_guard<std::mutex> guard(runtime->mutex);
                        mainSlot->state = kBridgeCoroutineReceiving;
                    }
                    const std::size_t next = bridgeNextRunnable(0u);
                    if (next >= runtime->slots.size())
                    {
                        error_ = "协程接收消息形成死锁";
                        return false;
                    }
                    if (!bridgeSchedule(next)) return false;
                    std::lock_guard<std::mutex> guard(runtime->mutex);
                    mainSlot->state = kBridgeCoroutineRunning;
                }
            }
            if (got->type != "string")
            {
                error_ = "协程消息类型不匹配：期望 string，收到 " + got->type;
                return false;
            }
            NativeContext context;
            context.heap = impl_->heap;
            context.data = data_;
            const std::size_t address = nativeAllocString(context, got->value.text);
            if (address == 0u)
            {
                error_ = context.error.empty() ? Memory::String("协程消息字符串分配失败") : context.error;
                return false;
            }
            result = RuntimeValue::ofInt(static_cast<std::int64_t>(address));
            return true;
        }
        /* std::coroutine::wait */
        const std::int64_t raw = args[0].integer;
        if (raw <= 0 || static_cast<std::size_t>(raw) >= runtime->slots.size())
        {
            error_ = "std::coroutine::wait 收到了无效句柄：" + Memory::toString(raw);
            return false;
        }
        const std::size_t target = static_cast<std::size_t>(raw);
        if (target == g_bridgeCoroutine)
        {
            error_ = "协程不能等待自己";
            return false;
        }
        result = RuntimeValue::ofInt(0);
        if (g_bridgeCoroutine == 0u)
        {
            std::size_t after = 0u;
            bool first = true;
            for (;;)
            {
                const int state = runtime->slots[target]->state;
                if (state == kBridgeCoroutineDone || runtime->slots[target]->finished) return true;
                std::size_t next;
                if (first && (state == kBridgeCoroutineReady || state == kBridgeCoroutineSuspended))
                    next = target;
                else
                    next = bridgeNextRunnable(after);
                first = false;
                if (next >= runtime->slots.size())
                {
                    error_ = "协程等待形成死锁";
                    return false;
                }
                {
                    std::lock_guard<std::mutex> guard(runtime->mutex);
                    runtime->slots[0]->state = kBridgeCoroutineWaiting;
                    runtime->slots[0]->waitingFor = target;
                }
                if (!bridgeSchedule(next)) return false;
                {
                    std::lock_guard<std::mutex> guard(runtime->mutex);
                    runtime->slots[0]->state = kBridgeCoroutineRunning;
                }
                after = next;
            }
        }
        auto slot = runtime->slots[g_bridgeCoroutine];
        for (;;)
        {
            if (runtime->slots[target]->state == kBridgeCoroutineDone) return true;
            std::unique_lock<std::mutex> guard(runtime->mutex);
            slot->state = kBridgeCoroutineWaiting;
            slot->waitingFor = target;
            slot->yielded = true;
            runtime->changed.notify_all();
            runtime->changed.wait(guard, [&] { return slot->go || runtime->closing; });
            if (runtime->closing)
            {
                error_ = "程序结束，协程被中止";
                return false;
            }
            slot->go = false;
            slot->state = kBridgeCoroutineRunning;
        }
    }

    /* 1. 解释器内部 C++ 运行时（字符串 / 容器 / 大数 / 对象表 …）。 */
    const auto internal = kSlotNatives.find(name);
    if (internal != kSlotNatives.end())
    {
        NativeContext context;
        context.heap = impl_->heap;
        context.data = data_;
        context.text = &impl_->text;
        context.textSink = impl_->outputSink;
        context.textSinkUser = impl_->outputUser;
        context.textKeep = &impl_->outputKeepText;
        context.input = &impl_->input;
        context.inputIndex = &impl_->inputIndex;
        context.programArguments = &impl_->programArguments;
        context.objects = &impl_->objects;
        context.timers = &impl_->timers;
        context.machine = nullptr;
        context.threadId = impl_->threadId;
        context.bigFloatPrecision = &impl_->bigFloatPrecision;
        context.exitRequested = &impl_->exitRequested;
        context.exitCode = &impl_->exitCode;
        context.fileApi = (fileApi_ != nullptr) ? fileApi_ : nullptr;
        context.invoker = &impl_->invokerFn;

        Memory::Vector<Slot> slots;
        slots.reserve(args.size());
        for (const RuntimeValue &value : args) slots.push_back(bridgeToSlot(value));
        try
        {
            result = bridgeFromSlot(internal->second.apply(context, slots));
            if (!context.memoryFault.empty()) context.error = context.memoryFault;
        } catch (const std::exception &exception)
        {
            error_ = "运行时符号 " + name + " 抛出异常：" + exception.what();
            return false;
        } catch (...)
        {
            error_ = "运行时符号 " + name + " 抛出未知异常";
            return false;
        }
        if (!context.error.empty())
        {
            fail(context.error);
            return false;
        }
        return true;
    }

    /* 2. 宿主注册表（标准胶水 / abi 块 / --glue）。 */
    const NativeRegistry::Entry *external =
        registry_ == nullptr ? nullptr : registry_->find(name);
    if (external == nullptr)
    {
        error_ = "AOT 运行时不认识符号：" + name;
        return false;
    }
    if (args.size() != external->arity)
    {
        error_ = "宿主函数 " + name + " 需要 " + Memory::toString(external->arity) + " 个参数，实到 " + Memory::toString(args.size()) + " 个";
        return false;
    }
    Memory::Vector<NativeValue> arguments;
    arguments.reserve(args.size());
    for (const RuntimeValue &value : args)
    {
        if (value.isText)
            arguments.push_back(NativeValue::fromText(value.text));
        else if (value.isFloat)
            arguments.push_back(NativeValue::fromFloat(value.real));
        else
            arguments.push_back(NativeValue::fromInt(value.integer));
    }
    NativeContext context;
    context.heap = impl_->heap;
    context.data = data_;
    context.text = &impl_->text;
    context.textSink = impl_->outputSink;
    context.textSinkUser = impl_->outputUser;
    context.textKeep = &impl_->outputKeepText;
    context.input = &impl_->input;
    context.inputIndex = &impl_->inputIndex;
    context.programArguments = &impl_->programArguments;
    context.objects = &impl_->objects;
    context.machine = nullptr;
    context.timers = &impl_->timers;
    context.threadId = impl_->threadId;
    context.instructionHookSetter = [this](std::size_t interval, InstructionHook hook) {
        impl_->hookInterval = interval;
        impl_->hookSteps = 0u;
        impl_->instructionHook = std::move(hook);
    };
    context.interruptPosterFn = [this](std::int64_t timer, std::int64_t source) {
        impl_->timers.pending.push_back(InterruptEvent{ timer, source, 0, impl_->threadId });
    };
    context.bigFloatPrecision = &impl_->bigFloatPrecision;
    context.exitRequested = &impl_->exitRequested;
    context.exitCode = &impl_->exitCode;
    context.fileApi = fileApi_;
    context.invoker = &impl_->invokerFn;
    NativeCallContextImpl callContext(context, impl_->invokerFn, &error_);
    NativeValue returned;
    try
    {
        returned = external->callback(callContext, arguments);
    } catch (const std::exception &exception)
    {
        error_ = "宿主函数 " + name + " 抛出异常：" + exception.what();
        return false;
    } catch (...)
    {
        error_ = "宿主函数 " + name + " 抛出未知异常";
        return false;
    }
    if (!context.memoryFault.empty()) context.error = context.memoryFault;
    if (!context.error.empty())
    {
        fail(context.error);
        return false;
    }
    if (returned.isText())
        result = RuntimeValue::ofText(returned.text);
    else if (returned.isFloat())
        result = RuntimeValue::ofFloat(returned.floating);
    else
        result = RuntimeValue::ofInt(returned.integer);
    return true;
}

/**
 * @brief 直接读取静态标量，不构造 RuntimeValue。
 * @param address 运行时虚拟地址。
 * @param out 成功时接收读取结果。
 * @retval 成功返回 true，非法地址按原内存访问路径报告错误并返回 false。
 */
bool RuntimeBridge::loadInteger(std::int64_t address, std::int64_t &out)
{
    const std::size_t target = static_cast<std::size_t>(address);
    if (impl_->heap->contains(target))
    {
        const bool okay = impl_->heap->readInt(target, out);
        if (!okay) fail("panic：读堆内存失败：地址 " + Memory::toString(address));
        return okay;
    }
    if (data_ == nullptr)
    {
        fail("panic：没有数据区，无法按地址读");
        return false;
    }
    const bool okay = data_->readInt(target, out);
    if (!okay) fail("panic：读数据区失败：地址 " + Memory::toString(address));
    return okay;
}

/**
 * @brief 直接读取静态标量，不构造 RuntimeValue。
 * @param address 运行时虚拟地址。
 * @param out 成功时接收读取结果。
 * @retval 成功返回 true，非法地址按原内存访问路径报告错误并返回 false。
 */
bool RuntimeBridge::loadFloat(std::int64_t address, double &out)
{
    const std::size_t target = static_cast<std::size_t>(address);
    if (impl_->heap->contains(target))
    {
        const bool okay = impl_->heap->readDouble(target, out);
        if (!okay) fail("panic：读堆内存失败：地址 " + Memory::toString(address));
        return okay;
    }
    if (data_ == nullptr)
    {
        fail("panic：没有数据区，无法按地址读");
        return false;
    }
    const bool okay = data_->readDouble(target, out);
    if (!okay) fail("panic：读数据区失败：地址 " + Memory::toString(address));
    return okay;
}

/**
 * @brief 直接写入静态标量，不构造 RuntimeValue。
 * @param address 运行时虚拟地址。
 * @param value 待写入的标量值。
 * @retval 成功返回 true，非法地址按原内存访问路径报告错误并返回 false。
 */
bool RuntimeBridge::storeInteger(std::int64_t address, std::int64_t value)
{
    const std::size_t target = static_cast<std::size_t>(address);
    if (impl_->heap->contains(target))
    {
        const bool okay = impl_->heap->writeInt(target, value);
        if (!okay) fail("panic：写堆内存失败：地址 " + Memory::toString(address));
        return okay;
    }
    if (data_ == nullptr)
    {
        fail("panic：没有数据区，无法按地址写");
        return false;
    }
    const bool okay = data_->writeInt(target, value);
    if (!okay) fail("panic：写数据区失败：地址 " + Memory::toString(address));
    return okay;
}

/**
 * @brief 直接写入静态标量，不构造 RuntimeValue。
 * @param address 运行时虚拟地址。
 * @param value 待写入的标量值。
 * @retval 成功返回 true，非法地址按原内存访问路径报告错误并返回 false。
 */
bool RuntimeBridge::storeFloat(std::int64_t address, double value)
{
    const std::size_t target = static_cast<std::size_t>(address);
    if (impl_->heap->contains(target))
    {
        const bool okay = impl_->heap->writeDouble(target, value);
        if (!okay) fail("panic：写堆内存失败：地址 " + Memory::toString(address));
        return okay;
    }
    if (data_ == nullptr)
    {
        fail("panic：没有数据区，无法按地址写");
        return false;
    }
    const bool okay = data_->writeDouble(target, value);
    if (!okay) fail("panic：写数据区失败：地址 " + Memory::toString(address));
    return okay;
}

/**
 * @brief 读取动态值，静态地址检查委托给对应标量路径。
 * @param address 运行时虚拟地址。
 * @param isFloat 是否按浮点读取。
 * @param out 成功时接收带类型结果。
 * @retval 读取成功返回 true，否则保留错误并返回 false。
 */
bool RuntimeBridge::loadMemory(std::int64_t address, bool isFloat, RuntimeValue &out)
{
    if (isFloat)
    {
        double value = 0;
        if (!loadFloat(address, value)) return false;
        out = RuntimeValue::ofFloat(value);
    }
    else
    {
        std::int64_t value = 0;
        if (!loadInteger(address, value)) return false;
        out = RuntimeValue::ofInt(value);
    }
    return true;
}
/**
 * @brief 将动态值转换为目标标量后写入，沿用原有转换规则。
 * @param address 运行时虚拟地址。
 * @param isFloat 是否按浮点写入。
 * @param value 待写入的带类型值。
 * @retval 写入成功返回 true，否则保留错误并返回 false。
 */
bool RuntimeBridge::storeMemory(std::int64_t address, bool isFloat, const RuntimeValue &value)
{
    return isFloat ? storeFloat(address, value.isFloat ? value.real : value.number()) :
                     storeInteger(address, value.isFloat ? static_cast<std::int64_t>(value.number()) : value.integer);
}

/**
 * @brief 计算程序数据区中指定偏移对应的地址。
 * @param symbol 数据区或函数的符号名称。
 * @param address 运行时内存地址。
 * @retval 满足检查条件或操作完成时为 true，检查失败或无法完成时为 false。
 */
bool RuntimeBridge::dataAddress(const Memory::String &symbol, std::int64_t &address) const
{
    if (data_ == nullptr)
    {
        error_ = "没有数据区，无法取符号地址：" + symbol;
        return false;
    }
    const auto found = data_->symbols.find(symbol);
    if (found == data_->symbols.end())
    {
        error_ = "数据区里没有符号：" + symbol;
        return false;
    }
    address = static_cast<std::int64_t>(found->second);
    return true;
}

/**
 * @brief 从虚拟机内存读取零结尾字符串。
 * @param address 运行时内存地址。
 * @retval 生成或读取的结果文本。
 */
Memory::String RuntimeBridge::readCString(std::int64_t address) const
{
    NativeContext context;
    context.heap = impl_->heap;
    context.data = data_;
    Memory::String text;
    if (!nativeReadCString(context, static_cast<std::size_t>(address), &text))
    {
        if (!context.memoryFault.empty()) const_cast<RuntimeBridge *>(this)->fail(context.memoryFault);
        return Memory::String();
    }
    return text;
}

/**
 * @brief 解析函数值及隐式环境。
 * @param pointer 函数值。
 * @param environment 环境输出。
 * @return 标签地址。
 */
std::int64_t RuntimeBridge::resolveFunction(std::int64_t pointer, std::int64_t &environment)
{
    const auto target = closureTarget(impl_->objects, pointer, environment);
    if (!target) fail("函数值已经释放或不是有效入口");
    return target;
}

/**
 * @brief 校验数组索引是否位于有效范围。
 * @param index 待访问的元素索引。
 * @param limit 索引、步数或资源使用的上限。
 * @param label 诊断或跳转使用的标签。
 * @retval 满足检查条件或操作完成时为 true，检查失败或无法完成时为 false。
 */
bool RuntimeBridge::checkIndex(std::int64_t index, std::int64_t limit, const Memory::String &label)
{
    if (index < 0 || index >= limit)
    {
        error_ = "数组下标越界：" + (label.empty() ? Memory::String("数组") : label) + " 的下标 " + Memory::toString(index) + "，长度 " + Memory::toString(limit);
        return false;
    }
    return true;
}

/**
 * @brief 校验指针访问是否落在允许的内存范围。
 * @param index 待访问的元素索引。
 * @param base 起始地址、基值或目标表达式。
 * @param stride 相邻元素之间的字节步长。
 * @param label 诊断或跳转使用的标签。
 * @retval 满足检查条件或操作完成时为 true，检查失败或无法完成时为 false。
 */
bool RuntimeBridge::checkPointer(std::int64_t index, std::int64_t base, std::int64_t stride,
                                 const Memory::String &label)
{
    if (index < 0)
    {
        error_ = "数组下标越界：" + (label.empty() ? Memory::String("这一块") : label) + " 的下标 " + Memory::toString(index) + "（不能为负）";
        return false;
    }
    const std::size_t address = static_cast<std::size_t>(base);
    std::size_t capacity = 0u;
    bool found = false;
    if (impl_->heap->contains(address))
    {
        found = impl_->heap->capacityAt(address, capacity);
    }
    else if (data_ != nullptr)
    {
        for (const auto &item : data_->symbols)
        {
            const std::size_t begin = item.second;
            const auto sizeAt = data_->sizes.find(item.first);
            const std::size_t bytes = (sizeAt == data_->sizes.end()) ? 0u : sizeAt->second;
            if (bytes == 0u) continue;
            if (address >= begin && address < begin + bytes)
            {
                capacity = begin + bytes - address;
                found = true;
                break;
            }
        }
    }
    if (!found || capacity == 0u)
    {
        fail("panic：数组下标检查失败：" + (label.empty() ? Memory::String("这一块") : label) + " 的地址 " + Memory::toString(base) + " 不在任何一块已分配内存里");
        return false;
    }
    const std::int64_t step = (stride > 0) ? stride : 8;
    if (static_cast<std::uint64_t>(index) >= capacity / static_cast<std::uint64_t>(step))
    {
        error_ = "数组下标越界：" + (label.empty() ? Memory::String("这一块") : label) + " 的下标 " + Memory::toString(index) + "（这一块只有 " + Memory::toString(capacity) + " 字节，元素 " + Memory::toString(step) + " 字节）";
        return false;
    }
    return true;
}

/**
 * @brief 分配指定字节数的运行时内存。
 * @param bytes 内存区域或数据的字节数。
 * @param address 运行时内存地址。
 * @retval 满足检查条件或操作完成时为 true，检查失败或无法完成时为 false。
 */
bool RuntimeBridge::allocate(std::int64_t bytes, std::int64_t &address)
{
    if (bytes <= 0)
    {
        error_ = "alloc 的字节数必须是正数（拿到 " + Memory::toString(bytes) + "）";
        return false;
    }
    if (static_cast<std::uint64_t>(bytes) > std::numeric_limits<std::size_t>::max())
    {
        error_ = "alloc 的字节数超出地址空间";
        return false;
    }
    const std::size_t allocated = impl_->heap->allocate(static_cast<std::size_t>(bytes));
    if (allocated == 0u)
    {
        error_ = "堆内存不足：申请 " + Memory::toString(bytes) + " 字节";
        return false;
    }
    impl_->objects.beginStorage(allocated, static_cast<std::size_t>(bytes), true);
    address = static_cast<std::int64_t>(allocated);
    return true;
}

/**
 * @brief 释放指定地址对应的运行时资源。
 * @param handle 待操作的运行时句柄。
 * @param soft 是否允许按软释放规则处理无效地址。
 * @retval 满足检查条件或操作完成时为 true，检查失败或无法完成时为 false。
 */
bool RuntimeBridge::release(std::int64_t handle, bool soft)
{
    if (isObjectHandle(handle))
    {
        if (!impl_->objects.find(handle) && handle < impl_->objects.next) return true;
        if (!impl_->objects.release(handle) && !soft)
        {
            error_ = "free 失败：这个对象已经释放过了（句柄 " + Memory::toString(handle) + "）";
            return false;
        }
        return true;
    }
    const std::size_t target = static_cast<std::size_t>(handle);
    std::size_t capacity = 0;
    if (impl_->heap->capacityAt(target, capacity)) impl_->objects.beginStorage(target, capacity, true);
    if (soft)
    {
        if (target == 0u || !impl_->heap->contains(target)) return true;
        Memory::String reason;
        impl_->heap->release(target, &reason);
        return true;
    }
    Memory::String reason;
    if (!impl_->heap->release(target, &reason))
    {
        error_ = "free 失败：" + reason;
        return false;
    }
    return true;
}

/**
 * @brief 判断地址对应的运行时分配是否仍然存活。
 * @param handle 待操作的运行时句柄。
 * @retval 地址对应的运行时分配仍然存活时为 true，否则为 false。
 */
bool RuntimeBridge::alive(std::int64_t handle) const
{
    if (isObjectHandle(handle))
    {
        return const_cast<ObjectStore &>(impl_->objects).find(handle) != nullptr;
    }
    std::size_t capacity = 0;
    return impl_->heap->capacityAt(static_cast<std::size_t>(handle), capacity);
}

/**
 * @brief 取得本次程序执行积累的输出文本。
 * @retval 生成或读取的结果文本。
 */
const Memory::String &RuntimeBridge::output() const { return impl_->text; }

/**
 * @brief 记录当前处理过程的错误信息。
 * @retval 生成或读取的结果文本。
 */
const Memory::String &RuntimeBridge::error() const { return error_; }

/**
 * @brief 清空本次程序执行积累的输出文本。
 */
void RuntimeBridge::clearOutput() { impl_->text.clear(); }

/**
 * @brief 读取运行时字节堆的峰值占用。
 * @retval 运行时字节堆的峰值字节数。
 */
std::int64_t RuntimeBridge::heapPeak() const
{
    return static_cast<std::int64_t>(impl_->heap->peakBytes());
}

/**
 * @brief 配置 AOT 文本的即时输出。
 * @param sink 字节回调。
 * @param user 上下文。
 * @param keepText 是否保留文本副本。
 */
void RuntimeBridge::setOutputSink(OutputSink sink, void *user, bool keepText)
{
    impl_->outputSink = sink;
    impl_->outputUser = user;
    impl_->outputKeepText = keepText;
}
/**
 * @brief 将文本追加到运行时输出。
 * @param text 待处理的文本。
 */
void RuntimeBridge::appendOutput(const Memory::String &text)
{
    if (impl_->outputSink) impl_->outputSink(impl_->outputUser, text.data(), text.size());
    if (!impl_->outputSink || impl_->outputKeepText) impl_->text += text;
}

/**
 * @brief 将运行时值转换为文本并追加到输出。
 * @param value 待转换、保存或参与计算的值。
 */
void RuntimeBridge::appendValue(const RuntimeValue &value)
{
    char buffer[64];
    if (value.isFloat)
    {
        std::snprintf(buffer, sizeof(buffer), "%.10g", value.real);
        appendOutput(buffer);
    }
    else
    {
        std::snprintf(buffer, sizeof(buffer), "%lld", static_cast<long long>(value.integer));
        appendOutput(buffer);
    }
}

/**
 * @brief 将单个字符追加到运行时输出。
 * @param code 错误码、字符值或待分析的源码文本。
 */
void RuntimeBridge::appendChar(int code) { appendOutput(Memory::String(1u, static_cast<char>(code & 0xFF))); }

/**
 * @brief 从运行时输入中读取下一个值。
 * @param getCharacter 是否按单个字符读取输入。
 * @param isFloat 是否按浮点数表示读写数据。
 * @param out 接收处理结果的输出对象。
 * @retval 满足检查条件或操作完成时为 true，检查失败或无法完成时为 false。
 */
bool RuntimeBridge::readInput(bool getCharacter, bool isFloat, RuntimeValue &out)
{
    if (impl_->inputIndex >= impl_->input.size())
    {
        out = isFloat ? RuntimeValue::ofFloat(0.0) : RuntimeValue::ofInt(0);
        return true;
    }
    const Memory::String &item = impl_->input[impl_->inputIndex++];
    if (getCharacter)
    {
        out = RuntimeValue::ofInt(item.empty() ? 0 : static_cast<std::int64_t>(static_cast<unsigned char>(item.front())));
        return true;
    }
    if (isFloat)
        out = RuntimeValue::ofFloat(std::strtod(item.c_str(), nullptr));
    else
        out = RuntimeValue::ofInt(std::strtoll(item.c_str(), nullptr, 10));
    return true;
}
