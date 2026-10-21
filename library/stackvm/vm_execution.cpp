/**
 * @file vm_execution.cpp
 * @brief 指令主循环、执行安全点与快速分派。
 */
#include "runtime_internal.hpp"
#include "thread_runtime.hpp"
#include "concurrent_call_state.hpp"

namespace stackvm
{
using namespace runtimeDetail;
/**
 * @brief 建立虚拟机独立的协作停止标志。
 * @param trace 是否记录逐指令执行轨迹。
 */
Machine::Machine(bool trace) : control_(std::make_shared<ExecutionControl>()), trace_(trace) {}
/**
 * @brief 设置协作停止请求，执行与等待安全点将观察此原子标志。
 */
void Machine::requestStop() { control_->requested.store(true, std::memory_order_release); }
#if STACKVM_WITH_THREADS
/**
 * @brief 返回当前运行的弱引用投递器，不捕获 Machine 裸指针。
 * @return 运行结束后稳定返回关闭状态的投递器。
 */
EventPoster Machine::eventPoster()
{
    const std::weak_ptr<EventQueue> target = events_;
    return [target](RuntimeEvent event) { return postEvent(target, event); };
}

#endif

/**
 * @brief 设置断点集合（指令下标）。
 * @param instructions 断点位置；重复项会被忽略，空表等于清空。
 */
void Machine::setBreakpoints(Memory::Vector<std::size_t> instructions)
{
    breakpoints_.clear();
    for (const std::size_t instruction : instructions) breakpoints_.insert(instruction);
}

/**
 * @brief 执行一个组装好的程序。
 * @param program 程序。
 * @param input `read` / `getc` 的输入项（按顺序取）。
 * @param stepLimit 指令条数上限，防止死循环；传 `0` 表示不设上限（缺省为 0）。
 * @return 执行结果。
 */
RunResult Machine::run(const Program &program, const Memory::Vector<Memory::String> &input,
                       std::size_t stepLimit)
{
    RunResult result;
    Memory::Vector<Memory::String> featureErrors;
    if (!validateProgramMetadata(program, &featureErrors))
    {
        result.error = featureErrors.front();
        return result;
    }
    // 已请求重启时不再分配线程或事件资源，保证提前返回的投递器保持关闭。
    if (Memory::restartSignal().pending())
    {
        result.recoverable = true;
        result.recoverableReason = static_cast<int>(Memory::restartSignal().reason());
        return result;
    }
    if (threadId_ == 0u) control_->requested.store(false, std::memory_order_release);
#if STACKVM_WITH_THREADS
    events_ = std::make_shared<EventQueue>();
#endif
    // 本次运行生效的调试表：调用方临时挂的优先，其次是程序自带的那张。
    debugInfo_ = debugOverride_ != nullptr ? debugOverride_ : (program.debug.empty() ? nullptr : &program.debug);
    currentLocation_ = SourceLocation{};
    debugFrames_.clear();
#if STACKVM_WITH_THREADS
    if (sharedRuntime_ == nullptr) sharedRuntime_ = Memory::makeShared<SharedRuntime>();
    if (threadsEnabled_ && threadRuntime_ == nullptr)
    {
        threadRuntime_ = Memory::makeShared<ThreadRuntime>();
        threadRuntime_->workers.emplace(0u, Memory::makeShared<ThreadRuntime::Worker>());
        threadRuntime_->workers.at(0u)->control = control_;
    }
#endif
    if (timeCallback_)
    {
        result.timingAvailable = true;
        result.startedNanoseconds = timeCallback_();
    }
    Memory::Vector<Slot> stack;
    Memory::Map<Memory::String, Slot> globals;
    using execution_detail::absent;
    execution_detail::ExecutionPlan plan(program);
    register_core::Engine registerEngine(program, plan, registerJumpFolding_, registerSpecialization_, registerServices_);
    Memory::Vector<std::int64_t *> registerSlots;
    execution_detail::DataCapacityIndex dataCapacity(data_);
    const register_core::Region *boundRegion = nullptr;
    Memory::Vector<Slot> globalSlots(plan.types.size());
    Memory::Vector<bool> globalBound(plan.types.size(), false);
    // 只对内部固定入口预绑定；宿主入口额外校验注册表版本。
    Memory::Vector<const SlotNativeEntry *> internalEntries(plan.types.size(), nullptr);
    Memory::Vector<const NativeRegistry::Entry *> externalEntries(plan.types.size(), nullptr);
    Memory::Vector<std::size_t> externalRevisions(plan.types.size(), absent);
    /* 看门狗状态按运行保存，用户事件仅在显式接收时消费。 */
    WatchdogRuntime watchdog;
#if STACKVM_WITH_THREADS
    RuntimeEvent receivedEvent;
#endif
    // 调用帧栈：每个活动调用独占一帧，返回后清空值并可限量缓存存储，
    // 递归的每层因此各有各的局部变量存储。
    Memory::Vector<Memory::UniquePtr<Frame>> frames;
    // 只缓存已返回的小帧；活动递归帧始终独占存储。
    Memory::Vector<Memory::UniquePtr<Frame>> returnedFrames;
    Memory::Vector<PendingArg> pending;
    // 堆：紧跟在数据区后面长（没有数据区时从地址 1 开始，0 留给空指针）。
    const std::size_t heapBase = (data_ == nullptr || data_->size() == 0u) ? kRuntimeCellBytes : data_->size();
#if STACKVM_WITH_DEFAULT_HEAP
    Heap defaultHeap;
    HeapAllocator &heap = heapAllocator_ == nullptr ? static_cast<HeapAllocator &>(defaultHeap) : *heapAllocator_;
#else
    /* 裁剪配置不编默认字节堆（`STACKVM_WITH_DEFAULT_HEAP=0`）：必须由宿主注入字节池
     *（`Machine::setHeapAllocator`）。没注入时用空堆，全部分配明确失败。 */
    NoHeapAllocator noHeap;
    HeapAllocator &heap = heapAllocator_ == nullptr ? static_cast<HeapAllocator &>(noHeap) : *heapAllocator_;
#endif
    heap.reset(heapBase);
    std::size_t inputIndex = 0;
    bool exitRequested = false;
    int exitCode = 0;
    // 运行期对象表：`bint`（大整数）与 vec / set / map 三种容器的"值"就是这里的句柄。
    // 跟着这次 run 走，一条 `(drop, 句柄, _, _)` 或 `(free, 句柄, _, _)` 就能还回去。
    ObjectStore objects;
    std::size_t pc = 0;
    // 内建符号在 VM 端按同一 FNV 算法绑定，不把名字写进文件。
    Memory::Map<Memory::String, Memory::String> nativeNames;
    Memory::Map<Memory::String, std::size_t> dataSymbols;
    if (program.compactSymbols)
    {
        for (const auto &[name, ignored] : kSlotNativeEntries)
        {
            (void)ignored;
            nativeNames.emplace(program.symbolKey(name), name);
        }
        for (const char *name : { "__std_thread_create", "__std_channel_send", "__std_channel_take", "__std_mail_send", "std::concurrency::__channelCreate", "std::concurrency::__channelClose", "std::concurrency::__poll", "std::concurrency::__fetchAdd", "std::event::__receive", "std::event::__timer", "std::event::__destroy", "std::event::__source", "std::event::__tag", "std::event::__missed", "std::event::__id", "std::event::__post", "std::event::__close", "std::thread::requestStop", "std::thread::stopRequested", "std::thread::state", "std::thread::__error", "std::thread::__trace", "std::thread::sleepFor", "std::thread::__waitResult", "__std_message_receive_thread", "__std_message_send_thread", "__std_shared_create", "__std_shared_get", "__std_shared_set", "__std_shared_swap", "main", "std::thread::create", "std::thread::current", "std::thread::parent", "std::thread::receive", "std::thread::send", "std::thread::wait", "std::thread::waitFor" })
            nativeNames.emplace(program.symbolKey(Memory::String(name)), name);
        if (data_)
            for (const auto &[name, offset] : data_->symbols)
                dataSymbols.emplace(program.symbolKey(name), offset);
    }
    Memory::Vector<const Memory::String *> nativeAliases(plan.types.size(), nullptr);
    Memory::Vector<bool> specialEntries(plan.types.size(), false);
    if (optimizeExecution_)
        for (const auto &item : plan.symbols)
        {
            const auto symbol = item.second;
            const auto alias = nativeNames.find(item.first);
            const auto &name = alias == nativeNames.end() ? item.first : alias->second;
            specialEntries[symbol] = name.compare(0, 14, "std::event::__") == 0 || name.compare(0, 20, "std::concurrency::__") == 0 || name.compare(0, 6, "__std_") == 0 || name.compare(0, 13, "std::thread::") == 0 || name.compare(0, 13, "__std_message_") == 0 || name.compare(0, 12, "__std_shared_") == 0;
            internalEntries[symbol] = kSlotNatives.find(name);
            if (alias != nativeNames.end()) nativeAliases[symbol] = &alias->second;
        }
    /**
     * @brief 仅在宿主注册表版本变化时重新查找入口，包括此前未注册的名称。
     */
    const NativeRegistry *cachedRegistry = nativeRegistry_;
    const auto resolveExternal = [&](std::size_t symbol, const Memory::String &name) -> const NativeRegistry::Entry * {
        if (cachedRegistry != nativeRegistry_)
        {
            std::fill(externalRevisions.begin(), externalRevisions.end(), absent);
            cachedRegistry = nativeRegistry_;
        }
        if (!nativeRegistry_) return nullptr;
        if (!optimizeExecution_ || symbol == absent) return nativeRegistry_->find(name);
        if (externalRevisions[symbol] != nativeRegistry_->revision())
        {
            externalEntries[symbol] = nativeRegistry_->find(name);
            externalRevisions[symbol] = nativeRegistry_->revision();
        }
        return externalEntries[symbol];
    };
    bool entryPending = !entryLabel_.empty();
    bool jumped = false;                     ///< 本步是否改过 pc（跳转 / 调用 / 返回）
    Memory::Vector<Memory::String> numbers;  // write 的数值：多个值之间一个空格
    /**
     * @brief 地址空间：低地址是数据区（编译期布局），再往后是堆。
     *
     * `(load, 地址, …)` / `(store, 地址, …)` 不区分对象在哪一段，按地址落位。
     * @param address 地址。
     * @return 落在堆区返回 true，否则当数据区。
     */
    auto inHeap = [&](std::size_t address) { return heap.contains(address); };

    /**
     * @brief 按地址读取一个 u8 或八字节标量，保留活跃内存检查。
     * @param address 地址。
     * @param isFloat 是否按 IEEE-754 双精度解释。
     * @param out 结果。
     * @param bytes 访问宽度；1 表示 u8，其余调用使用 8。
     * @return 成功返回 true。
     */
    auto loadMemory = [&](std::size_t address, bool isFloat, Slot &out, std::size_t bytes = 8u) -> bool {
        if (bytes == 1u)
        {
            char value = 0;
            const bool okay = inHeap(address) ? heap.readBytes(address, &value, 1u) :
                                                (data_ && data_->readBytes(address, &value, 1u));
            if (okay) out = Slot::fromInt(static_cast<unsigned char>(value));
            return okay;
        }
        if (inHeap(address))
        {
            if (isFloat)
            {
                double real = 0.0;
                if (!heap.readDouble(address, real)) return false;
                out = Slot::fromDouble(real);
                return true;
            }
            std::int64_t integer = 0;
            if (!heap.readInt(address, integer)) return false;
            out = Slot::fromInt(integer);
            return true;
        }
        if (data_ == nullptr) return false;
        if (isFloat)
        {
            double real = 0.0;
            if (!data_->readDouble(address, real)) return false;
            out = Slot::fromDouble(real);
            return true;
        }
        std::int64_t integer = 0;
        if (!data_->readInt(address, integer)) return false;
        out = Slot::fromInt(integer);
        return true;
    };

    /**
     * @brief 按地址写入一个 u8 或八字节标量，保留活跃内存检查。
     * @param address 地址。
     * @param isFloat 是否按 IEEE-754 双精度写。
     * @param value 值。
     * @param bytes 访问宽度；1 表示 u8，其余调用使用 8。
     * @return 成功返回 true。
     */
    auto storeMemory = [&](std::size_t address, bool isFloat, const Slot &value, std::size_t bytes = 8u) -> bool {
        if (bytes == 1u)
        {
            const unsigned char byte = static_cast<unsigned char>(nativeInt(value));
            const char *source = reinterpret_cast<const char *>(&byte);
            return inHeap(address) ? heap.writeBytes(address, source, 1u) :
                                     (data_ && data_->writeBytes(address, source, 1u));
        }
        if (inHeap(address))
        {
            return isFloat ? heap.writeDouble(address, value.number()) : heap.writeInt(address, (value.isFloat ? static_cast<std::int64_t>(nativeInt(value)) : value.integer));
        }
        if (data_ == nullptr) return false;
        return isFloat ? data_->writeDouble(address, value.number()) : data_->writeInt(address, (value.isFloat ? static_cast<std::int64_t>(nativeInt(value)) : value.integer));
    };

    /**
     * 追加一个数值输出（与 transfer 的示例套件口径一致）。
     */
    auto emitNumber = [&](Memory::String text) {
        if (!numbers.empty()) result.output += ' ';
        numbers.push_back(std::move(text));
        result.output += numbers.back();
    };

    /**
     * 追加一段**文本**输出：挂了输出下沉就先实时推给宿主（宿主日志接口），
     * `keepText=false` 时不再累积进 `RunResult::text`。`print` / `println` / `putChar`
     * 的指令与原生函数路径都走它，保证下沉看到的字节与累积文本完全一致。
     */
    auto emitText = [&](const char *bytes, std::size_t count) {
        if (bytes == nullptr || count == 0u) return;
        if (outputSink_ != nullptr) outputSink_(outputUser_, bytes, count);
        if (outputKeepText_) result.text.append(bytes, count);
    };

    /* ---- 可恢复终止（软重启的两个触发共用一条通道） ---------------------------
     * ① 分配失败：在**失败点只写标志**（`Memory::restartSignal()`，见
     *    `allocator/magel_allocator.hpp`），当前这次申请由 arena 的应急预留兜住，于是当前
     *    指令照常跑完、容器保持自洽；
     * ② 显式请求：`std::watchdog::requestRestart()` → `__std_request_restart()` 写同一份标志。
     * VM 在**下一条指令边界（安全点）**看到标志就停在这里，把本轮标成 `recoverable`。
     * **不抛异常、不 longjmp、不在失败点跳走**。
     * panic 的判定永远排在前面：panic 不是可恢复终止。 */
    /**
     * @brief 有没有待处理的软重启触发（分配失败**或**显式请求）。
     */
    auto restartPending = [&]() { return Memory::restartSignal().pending(); };
    /**
     * @brief 在安全点以"可恢复终止"结束本次 run()。
     *
     * 这里**只写标量**、不拼错误串：池可能刚耗尽（应急预留有限），一格式化就可能再撞一次。
     * 结构化原因（类别 / 申请字节 / 池内快照）由宿主从 `Memory::restartSignal()`
     * 读；`result.error` 里如果有内容，那是失败点自己的普通错误文案，一并留给宿主。
     */
    auto stopForRestart = [&]() {
        result.ok = false;
        result.recoverable = true;
        result.recoverableReason = static_cast<int>(Memory::restartSignal().reason());
#if STACKVM_WITH_HOST_EXCEPTION_GUARD
        if (!result.error.empty()) return;
        /* 桌面口径可以安全地补一句可读文案（结构化原因仍在信号位里）。池可能刚耗尽，
         * 所以拼字符串本身也要兜住异常 —— 拼不出来就只留结构化标记，宿主照旧能判可恢复。 */
        try
        {
            result.error = Memory::restartSignal().reason() == Memory::WatchdogReason::ExplicitRequest ? Memory::String("软重启：程序显式请求重启（可恢复终止）") : Memory::String("软重启：Magel 运行内存池用尽（可恢复终止）");
        } catch (...)
        {
        }
#endif
    };

    /**
     * @brief 按数字身份找变量；共享汇编入口与动态回调保留名称回退。
     */
    auto findDense = [&](std::size_t symbol) -> Slot * {
        if (symbol == absent) return nullptr;
        if (!frames.empty() && frames.back()->layout)
        {
            auto &frame = *frames.back();
            const auto local = frame.layout->slots.find(symbol);
            if (local != frame.layout->slots.end() && frame.bound[local->second]) return &frame.slots[local->second];
        }
        return globalBound[symbol] ? &globalSlots[symbol] : nullptr;
    };
    /**
     * @brief 找变量槽，保留局部优先、全局回退及动态回调临时变量。
     */
    auto findSlot = [&](const Memory::String &name) -> Slot * {
        if (!frames.empty())
        {
            const auto local = frames.back()->locals.find(name);
            if (local != frames.back()->locals.end()) return &local->second;
        }
        if (optimizeExecution_)
        {
            if (auto *slot = findDense(plan.find(name))) return slot;
        }
        const auto global = globals.find(name);
        return global == globals.end() ? nullptr : &global->second;
    };
    /**
     * @brief 按已解析类型归一化写入值，未声明临时量保持原始形态。
     */
    const auto normalizeStored = [&](std::size_t symbol, Slot value) {
        const auto *type = symbol == absent ? nullptr : plan.types[symbol].type;
        if (type && !value.isText)
        {
            if (type->isFloat)
                value = Slot::fromDouble(normalizeFloat(value.number(), type->bits));
            else
                value = Slot::fromInt(normalizeInt(value.isFloat ? static_cast<std::int64_t>(nativeInt(value)) : value.integer, *type));
        }
        return value;
    };
    /**
     * @brief 对预绑定指令直接读取局部槽，全局与共享汇编路径保持原有可见性。
     */
    const auto readPrepared = [&](std::size_t index) -> Slot * {
        const auto &prepared = plan.code[index];
        if (!optimizeExecution_ || prepared.shared || (!frames.empty() && !frames.back()->locals.empty()))
            return findSlot(program.code[index].name);
        if (!frames.empty() && prepared.owner == frames.back()->layout && prepared.local != execution_detail::absentRegister && frames.back()->bound[prepared.local])
            return &frames.back()->slots[prepared.local];
        if (!frames.empty() && prepared.owner != frames.back()->layout) return findSlot(program.code[index].name);
        return prepared.symbol != absent && globalBound[prepared.symbol] ? &globalSlots[prepared.symbol] : nullptr;
    };
    /**
     * @brief 写变量，普通指令走稠密存储，未登记回调目的地走原有动态表。
     */
    auto assign = [&](const Memory::String &name, const Slot &value) {
        const auto symbol = plan.find(name);
        const auto stored = normalizeStored(symbol, value);
        if (!frames.empty())
        {
            const auto local = frames.back()->locals.find(name);
            if (local != frames.back()->locals.end())
            {
                local->second = stored;
                return;
            }
        }
        if (optimizeExecution_ && symbol != absent)
        {
            if (auto *slot = findDense(symbol))
            {
                *slot = stored;
                return;
            }
            globalSlots[symbol] = stored;
            globalBound[symbol] = true;
            return;
        }
        globals[name] = stored;
    };
    /**
     * @brief 按预解析局部槽直接写入，未执行声明时仍写全局。
     */
    const auto storePrepared = [&](std::size_t index, const Slot &value) {
        const auto &prepared = plan.code[index];
        if (!optimizeExecution_ || prepared.shared || (!frames.empty() && (prepared.owner != frames.back()->layout || !frames.back()->locals.empty())))
        {
            assign(program.code[index].name, value);
            return;
        }
        const auto stored = normalizeStored(prepared.symbol, value);
        if (!frames.empty() && prepared.local != execution_detail::absentRegister && frames.back()->bound[prepared.local])
            frames.back()->slots[prepared.local] = stored;
        else
        {
            globalSlots[prepared.symbol] = stored;
            globalBound[prepared.symbol] = true;
        }
    };
    /**
     * @brief 为调用帧分配该函数实际需要的槽位，不按整个程序的符号数扩容。
     */
    const auto prepareFrame = [&](Frame &frame, std::size_t entry) {
        if (!optimizeExecution_) return;
        frame.layout = plan.layout(entry);
        frame.slots.resize(frame.layout->slots.size());
        frame.bound.resize(frame.slots.size(), false);
    };
    /**
     * @brief 绑定参数到稠密槽，动态汇编参数名保留回退。
     */
    const auto bindParameter = [&](Frame &frame, const Memory::String &name, const Slot &value) {
        if (frame.layout)
        {
            const auto slot = frame.layout->slots.find(plan.find(name));
            if (slot != frame.layout->slots.end())
            {
                frame.slots[slot->second] = value;
                frame.bound[slot->second] = true;
                return;
            }
        }
        frame.locals[name] = value;
    };

    /**
     * @brief 把 VM 的调用帧栈翻译成"源码级调用栈"。
     *
     * 调用者的位置取该帧的**返回地址**（就是调用点后面那条指令），当前帧用 `at`。
     * 无调试表时可用可选的函数符号和指令位置；宏关闭后只支持显式调试。
     */
    auto debugFramesOf = [&](std::size_t at) {
        Memory::Vector<DebugFrame> snapshot;
#if !STACKVM_WITH_PANIC_TRACE
        if (debugInfo_ == nullptr) return snapshot;
#endif
        snapshot.reserve(frames.size() + 1u);
        std::size_t callerIndex = 0u;
        for (const auto &frame : frames)
        {
            DebugFrame entry;
            entry.threadId = threadId_;
            if (frame)
            {
                /* 帧里存的是**返回地址**（调用指令的下一条），往回退一条才是调用点本身。 */
                const std::size_t callSite = frame->returnIp > 0u ? frame->returnIp - 1u : 0u;
                entry.instruction = callSite;
                if (debugInfo_ != nullptr)
                    if (const SourceLocation *caller = debugInfo_->lookup(callSite))
                    {
                        entry.function = caller->function;
                        entry.location = *caller;
                    }
            }
#if STACKVM_WITH_PANIC_TRACE
            if (entry.function.empty()) entry.function = callerIndex == 0u ? (entryLabel_.empty() ? Memory::String("(入口)") : program.symbolKey(entryLabel_)) : frames[callerIndex - 1u]->function;
#endif
            ++callerIndex;
            snapshot.push_back(std::move(entry));
        }
        DebugFrame current;
        current.threadId = threadId_;
        current.instruction = at;
        if (debugInfo_ != nullptr)
            if (const SourceLocation *where = debugInfo_->lookup(at)) current.location = *where;
        current.function = current.location.function;
#if STACKVM_WITH_PANIC_TRACE
        if (current.function.empty()) current.function = frames.empty() ? (entryLabel_.empty() ? Memory::String("(入口)") : program.symbolKey(entryLabel_)) : frames.back()->function;
#endif
        snapshot.push_back(std::move(current));
        return snapshot;
    };

    auto fail = [&](const Memory::String &message) {
        result.ok = false;
        result.error = message;
        if (program.compactSymbols || !panicDetailsEnabled_)
        {
            result.failureLocation = SourceLocation{};
            result.sourceTrace.clear();
            currentLocation_ = SourceLocation{};
            debugFrames_.clear();
        }
        else
        {
            if (debugInfo_ != nullptr)
                if (const SourceLocation *where = debugInfo_->lookup(pc)) result.failureLocation = *where;
            currentLocation_ = result.failureLocation;
#if STACKVM_WITH_PANIC_TRACE
            if (result.sourceTrace.empty()) result.sourceTrace = debugFramesOf(pc);
            debugFrames_ = result.sourceTrace;
#else
            debugFrames_.clear();
#endif
        }
#if STACKVM_WITH_THREADS
        if (message.rfind("panic", 0u) == 0u && threadRuntime_)
        {
            std::lock_guard<std::mutex> guard(threadRuntime_->mutex);
            if (!threadRuntime_->panicked.load(std::memory_order_relaxed))
            {
                threadRuntime_->panicError = message;
                threadRuntime_->panicThread = threadId_;
#if STACKVM_WITH_PANIC_TRACE
                threadRuntime_->panicTrace = result.sourceTrace;
                threadRuntime_->panicLocation = result.failureLocation;
#endif
                threadRuntime_->closing = true;
                threadRuntime_->panicked.store(true, std::memory_order_release);
                threadRuntime_->changed.notify_all();
            }
        }
#endif /* STACKVM_WITH_THREADS */
    };

#if STACKVM_WITH_THREADS
    /**
     * @brief 子线程错误传播保留子线程的真实帧，不以父线程等待点替代。
     */
    auto failFromThread = [&](const Memory::String &message, const RunResult &child) {
        fail(message);
#if STACKVM_WITH_PANIC_TRACE
        if (!program.compactSymbols && panicDetailsEnabled_ && !child.sourceTrace.empty())
        {
            result.sourceTrace = child.sourceTrace;
            result.failureLocation = child.failureLocation;
            debugFrames_ = result.sourceTrace;
            currentLocation_ = result.failureLocation;
        }
#else
        (void)child;
#endif
    };

    /**
     * @brief 在安全点接受全程序 panic，以首个发布者的错误与栈为准。
     */
    auto stopForPanic = [&]() -> bool {
        if (!threadRuntime_ || !threadRuntime_->panicked.load(std::memory_order_acquire)) return false;
        std::lock_guard<std::mutex> guard(threadRuntime_->mutex);
        result.ok = false;
        result.error = threadRuntime_->panicThread == 0u ? threadRuntime_->panicError :
                                                           "线程 " + Memory::toString(threadRuntime_->panicThread) + " 失败：" + threadRuntime_->panicError;
        result.sourceTrace.clear();
        result.failureLocation = SourceLocation{};
#if STACKVM_WITH_PANIC_TRACE
        if (!program.compactSymbols && panicDetailsEnabled_)
        {
            result.sourceTrace = threadRuntime_->panicTrace;
            result.failureLocation = threadRuntime_->panicLocation;
        }
#endif
        debugFrames_ = result.sourceTrace;
        currentLocation_ = result.failureLocation;
        return true;
    };
#else
    /**
     * @brief 裁剪配置没有线程：没有跨线程 panic 要在这里收，安全点检查恒为"没停"。
     */
    auto stopForPanic = [&]() -> bool { return false; };
#endif /* STACKVM_WITH_THREADS */

    /**
     * @brief 进一层函数：绑实参 → 在堆上开一帧 → 跳过去。直接调用与间接调用共用。
     *
     * `positional = false`（直接调用）：实参按**调用点写着的形参名**绑进新帧
     * （前端知道 callee 的形参名，逐个写出来了）。
     * `positional = true`（函数指针）：调用点不知道会调到谁，所以按**位置**绑 ——
     * 表来自 `program.parameters`（组装时从"函数标签前面那段 decl"扫出来的）。
     *
     * @param label 目标函数标签。
     * @param returnDest 返回值写回哪个变量（可空）。
     * @param positional 是否按位置绑实参。
     * @return 成功返回 true。
     */
    auto enterFunction = [&](const Memory::String &label, const Memory::String &returnDest,
                             bool positional) -> bool {
        std::size_t entry = absent;
        if (optimizeExecution_ && pc < program.code.size() && program.code[pc].op == Op::Call && program.code[pc].name == label)
            entry = plan.code[pc].target;
        else
        {
            const auto target = program.labels.find(program.symbolKey(label));
            if (target != program.labels.end()) entry = target->second;
        }
        if (entry == absent)
        {
            fail("函数入口不存在（也不在原生函数表里）：" + label);
            pending.clear();
            return false;
        }
        // 每层调用取得独占帧；优先复用已清空的小帧，缓存为空时分配。
        Memory::UniquePtr<Frame> frame;
        if (optimizeExecution_ && registerExecution_ && registerServices_ && !returnedFrames.empty())
        {
            frame = std::move(returnedFrames.back());
            returnedFrames.pop_back();
        }
        else
            frame = Memory::makeUnique<Frame>();
        prepareFrame(*frame, entry);
        frame->returnIp = pc + 1u;
        frame->returnDest = returnDest;
#if STACKVM_WITH_PANIC_TRACE
        if (!program.compactSymbols && panicDetailsEnabled_) frame->function = program.symbolKey(label);
#endif

        if (positional)
        {
            const auto names = program.parameters.find(program.symbolKey(label));
            // 零参数函数不产生 decl，旧字节码的形参表因此没有该入口。
            const std::size_t arity = names == program.parameters.end() ? 0u : names->second.size();
            if (arity != pending.size())
            {
                fail("通过函数指针调用 " + label + " 需要 " + Memory::toString(arity) + " 个参数，实到 " + Memory::toString(pending.size()) + " 个");
                pending.clear();
                return false;
            }
            for (std::size_t slot = 0; slot < pending.size(); ++slot)
            {
                bindParameter(*frame, names->second[slot], pending[slot].value);
            }
            pending.clear();
        }
        else
        {
            // 形参按名字绑定进新帧（类型在 callee 的 decl 里声明过）。
            for (const PendingArg &argument : pending)
            {
                const auto type = program.types.find(argument.name);
                Slot value = argument.value;
                if (type != program.types.end() && !value.isText)
                {
                    if (type->second.isFloat)
                        value = Slot::fromDouble(normalizeFloat(value.number(), type->second.bits));
                    else
                        value = Slot::fromInt(normalizeInt(
                            value.isFloat ? (value.isFloat ? static_cast<std::int64_t>(nativeInt(value)) : value.integer) : value.integer, type->second));
                }
                bindParameter(*frame, argument.name, value);
            }
            pending.clear();
        }
        frames.push_back(std::move(frame));
        pc = entry;
        jumped = true;
        return true;
    };

    Memory::Function<bool()> executeOne;
    Memory::Function<bool(const Memory::String &, const Memory::String &)> callNative;
    /* 等待与执行共用停止和看门狗检查。 */
    Memory::Function<void()> checkExecution;

#if STACKVM_WITH_THREADS
    /**
     * @brief 在条件成立、执行失败或单调时钟截止时间到达前等待线程通知，并检查取消和看门狗。
     * @param guard 调用方持有的线程运行时锁；检查安全点时暂时释放。
     * @param ready 持锁检查的完成条件，唤醒后重新检查以处理虚假唤醒。
     * @param deadline 最晚等待时间；默认最大时间点表示不限制等待时间。
     * @note 返回时仍持有 guard，调用方负责区分完成、超时和失败。
     */
    const auto waitForChange = [&](std::unique_lock<std::mutex> &guard,
                                   const Memory::Function<bool()> &ready,
                                   std::chrono::steady_clock::time_point deadline = std::chrono::steady_clock::time_point::max()) {
        if (ready() || std::chrono::steady_clock::now() >= deadline) return;
        for (;;)
        {
            guard.unlock();
            checkExecution();
            guard.lock();
            if (ready() || !result.error.empty() || result.cancelled || result.recoverable) return;
            const auto now = std::chrono::steady_clock::now();
            if (now >= deadline) return;
            threadRuntime_->changed.wait_until(guard, std::min(deadline, now + std::chrono::milliseconds(1)));
        }
    };
#endif /* STACKVM_WITH_THREADS */
    NativeCallContextImpl::Invoker invokeFunction;
    std::size_t nativeCallbackDepth = 0u;

    invokeFunction = [&](std::int64_t functionPointer,
                         const Memory::Vector<NativeValue> &arguments,
                         NativeValue &returnValue) -> bool {
        if (functionPointer == 0 || data_ == nullptr)
        {
            fail(functionPointer == 0 ? "胶水层调用了空函数指针" : "没有加载数据区，胶水层无法解析函数指针");
            return false;
        }
        std::int64_t environment = 0;
        const auto target = closureTarget(objects, functionPointer, environment);
        const Memory::String label = target ? data_->readCString(static_cast<std::size_t>(target)) : Memory::String();
        if (label.empty())
        {
            fail("胶水层函数指针没有指向有效函数名：" + Memory::toString(functionPointer));
            return false;
        }

        const std::size_t callerPc = pc;
        const bool callerJumped = jumped;
        const std::size_t callerFrames = frames.size();
        const std::size_t callerStack = stack.size();
        Memory::Vector<PendingArg> callerPending = std::move(pending);
        pending.clear();
        for (std::size_t index = 0; index < arguments.size(); ++index)
        {
            const NativeValue &argument = arguments[index];
            if (argument.isText())
            {
                fail("胶水层调用 MAGEL 函数时，文本参数必须先由 Codec<string> 编码");
                break;
            }
            pending.push_back(PendingArg{ "__magel_p" + Memory::toString(index),
                                          argument.isFloat() ? Slot::fromDouble(argument.floating) : Slot::fromInt(argument.integer) });
        }

        if (environment) pending.push_back(PendingArg{ "__magel_environment", Slot::fromInt(environment) });
        const Memory::String destination = "__magel_native_callback_" + Memory::toString(result.steps) + "_" + Memory::toString(callerFrames);
        ++nativeCallbackDepth;
        bool entered = result.error.empty();
        if (entered && callNative(label, destination))
        {
            entered = result.error.empty();
        }
        else if (entered)
        {
            entered = enterFunction(label, destination, true);
            while (entered && result.error.empty() && frames.size() > callerFrames)
            {
                if (!executeOne()) break;
            }
            if (entered && result.error.empty() && frames.size() != callerFrames)
            {
                fail("胶水层调用的 MAGEL 函数没有正常 return");
                entered = false;
            }
        }

        const auto returned = globals.find(destination);
        if (entered && result.error.empty() && returned != globals.end())
        {
            returnValue = returned->second.isFloat ? NativeValue::fromFloat(returned->second.real) : NativeValue::fromInt(returned->second.integer);
        }
        else if (entered && result.error.empty())
        {
            returnValue = NativeValue::fromInt(0);
        }
        globals.erase(destination);
        if (stack.size() > callerStack) stack.resize(callerStack);
        pending = std::move(callerPending);
        pc = callerPc;
        jumped = callerJumped;
        --nativeCallbackDepth;
        return entered && result.error.empty();
    };

    /**
     * 调用 VM 内部运行时或宿主注册表；不建普通 MAGEL 调用帧。
     */
    Memory::Vector<Memory::UniquePtr<Memory::Vector<Slot>>> internalArgumentBuffers;
    std::size_t internalArgumentDepth = 0;
    /**
     * @brief 执行已绑定内部入口；同步回调递归使用独立参数缓冲，普通调用复用容量。
     */
    const auto applyInternal = [&](const SlotNativeEntry *internal, const Memory::String &name, const Memory::String &returnDest) -> bool {
        if (internal->second.arity >= 0 && pending.size() != static_cast<std::size_t>(internal->second.arity))
        {
            fail("原生函数 " + name + " 需要 " + Memory::toString(internal->second.arity) + " 个参数，实到 " + Memory::toString(pending.size()) + " 个");
            pending.clear();
            return true;  // 已经报错了，算"处理过"
        }
        if (internalArgumentDepth == internalArgumentBuffers.size())
            internalArgumentBuffers.push_back(Memory::makeUnique<Memory::Vector<Slot>>());
        auto &arguments = *internalArgumentBuffers[internalArgumentDepth++];
        struct ArgumentDepthGuard
        {
            std::size_t &depth;
            /**
             * @brief 恢复原生回调调用前的参数栈深度。
             */
            ~ArgumentDepthGuard() { --depth; }
        } argumentGuard{ internalArgumentDepth };
        arguments.clear();
        arguments.reserve(pending.size());
        for (const PendingArg &argument : pending) arguments.push_back(argument.value);
        pending.clear();

        NativeContext context;
        context.heap = &heap;
        context.data = data_;
        context.text = &result.text; /* printf 和 putc / print 共用这条文本流 */
        context.textSink = outputSink_;
        context.textSinkUser = outputUser_;
        context.textKeep = &outputKeepText_;
        context.input = &input;
        context.inputIndex = &inputIndex;
        context.programArguments = &programArguments_;
        context.invoker = &invokeFunction;
        context.objects = &objects;
        context.watchdog = &watchdog;
        context.machine = this;
        context.threadId = threadId_;
        context.exitRequested = &exitRequested;
        context.exitCode = &exitCode;
        context.hostServices = hostServices_;
        const Slot value = internal->second.apply(context, arguments);
        if (!context.memoryFault.empty()) context.error = context.memoryFault;
        if (!context.error.empty())
        {
            fail(context.error);
            return true;
        }
        if (exitRequested)
        {
            result.ok = true;
            result.exitCode = exitCode;
            pc = program.code.size();
            jumped = true;
            return true;
        }
        if (trace_ && !value.isFloat && isObjectHandle(value.integer))
            result.trace.push_back("    => object " + Memory::toString(value.integer));
        if (!returnDest.empty()) assign(returnDest, value);
        return true;
    };
    ConcurrentCallState concurrentState{ program, pending, heap, objects, result, nativeCallbackDepth, stepLimit, assign, fail, emitText, checkExecution
#if STACKVM_WITH_THREADS
                                         ,
                                         receivedEvent, failFromThread, waitForChange
#endif
    };
    callNative = [&](const Memory::String &rawName, const Memory::String &returnDest) -> bool {
        const auto symbol = pc < program.code.size() && &rawName == &program.code[pc].name ? plan.code[pc].symbol : plan.find(rawName);
        if (optimizeExecution_ && symbol != absent && internalEntries[symbol])
            return applyInternal(internalEntries[symbol], nativeAliases[symbol] ? *nativeAliases[symbol] : rawName, returnDest);

        if (optimizeExecution_ && symbol != absent && !specialEntries[symbol] && pc < program.code.size() && &rawName == &program.code[pc].name && plan.code[pc].target != absent && !resolveExternal(symbol, rawName)) return false;
        const auto alias = nativeNames.find(rawName);
        const Memory::String &name = alias == nativeNames.end() ? rawName : alias->second;
        std::shared_ptr<TransferPacket> startupPacket;
        const bool typedCreate = name == "__std_thread_create";
        if (auto handled = callTransferService(concurrentState, name, returnDest, startupPacket, typedCreate)) return *handled;
        if (auto handled = callMailboxService(concurrentState, name, returnDest, startupPacket, typedCreate)) return *handled;
        if (auto handled = callThreadService(concurrentState, name, returnDest, startupPacket, typedCreate)) return *handled;
        /* 内部 C++ 运行时（字符串 / 容器）：参数与结果都按 Slot 走。 */
        const auto internal = optimizeExecution_ && symbol != absent ? internalEntries[symbol] : kSlotNatives.find(name);
        if (internal == kSlotNatives.end())
        {
            const NativeRegistry::Entry *external = resolveExternal(symbol, name);
            if (external == nullptr) return false;
            if (external->arity != static_cast<std::size_t>(-1) && pending.size() != external->arity)
            {
                fail("宿主函数 " + name + " 需要 " + Memory::toString(external->arity) + " 个参数，实到 " + Memory::toString(pending.size()) + " 个");
                pending.clear();
                return true;
            }
            Memory::Vector<NativeValue> arguments;
            arguments.reserve(pending.size());
            for (const PendingArg &argument : pending)
                arguments.push_back(argument.value.isFloat ? NativeValue::fromFloat(argument.value.real) : NativeValue::fromInt(argument.value.integer));
            pending.clear();

            NativeContext runtime;
            runtime.heap = &heap;
            runtime.data = data_;
            runtime.text = &result.text;
            runtime.textSink = outputSink_;
            runtime.textSinkUser = outputUser_;
            runtime.textKeep = &outputKeepText_;
            runtime.input = &input;
            runtime.inputIndex = &inputIndex;
            runtime.programArguments = &programArguments_;
            runtime.objects = &objects;
            runtime.watchdog = &watchdog;
            runtime.machine = this;
            runtime.threadId = threadId_;
            runtime.exitRequested = &exitRequested;
            runtime.exitCode = &exitCode;
            runtime.hostServices = hostServices_;
            NativeCallContextImpl context(runtime, invokeFunction, &result.error);
            const auto callback = external->callback;
            NativeValue value;
#if STACKVM_WITH_HOST_EXCEPTION_GUARD
            try
            {
                value = callback(context, arguments);
            } catch (const std::exception &exception)
            {
                runtime.error = "宿主函数 " + name + " 抛出异常：" + exception.what();
            } catch (...) { runtime.error = "宿主函数 " + name + " 抛出未知异常"; }
#else
            value = callback(context, arguments);
#endif
            if (!runtime.memoryFault.empty()) runtime.error = runtime.memoryFault;
            if (!runtime.error.empty())
            {
                fail(runtime.error);
                return true;
            }
            if (!returnDest.empty())
            {
                if (value.isText())
                {
                    const std::size_t address = nativeAllocString(runtime, value.text);
                    if (!runtime.error.empty())
                    {
                        fail(runtime.error);
                        return true;
                    }
                    assign(returnDest, Slot::fromInt(static_cast<std::int64_t>(address)));
                }
                else
                    assign(returnDest, value.isFloat() ? Slot::fromDouble(value.floating) : Slot::fromInt(value.integer));
            }
            return true;
        }
        return applyInternal(internal, name, returnDest);
    };

    checkExecution = [&]() {
        if (stopForPanic()) return;
        if (control_->requested.load(std::memory_order_acquire) && result.error.empty() && !result.recoverable)
        {
            result.cancelled = true;
            result.error = "执行已取消";
        }
        if (watchdog.period != 0)
        {
            watchdog.now = timeCallback_ ? timeCallback_() : static_cast<std::uint64_t>(result.steps);
            if (watchdog.deadline == 0) watchdog.deadline = watchdog.period > std::numeric_limits<std::uint64_t>::max() - watchdog.now ? std::numeric_limits<std::uint64_t>::max() : watchdog.now + watchdog.period;
            if (watchdog.now >= watchdog.deadline)
            {
                Memory::restartSignal().requestRestart();
                stopForRestart();
            }
        }
    };

    executeOne = [&]() -> bool {
        checkExecution();
        if (result.cancelled || result.recoverable || !result.error.empty()) return false;
        if (pc >= program.code.size()) return false;
        /* 下一条指令边界：分配失败在这里被消费成"可恢复终止"。
         * panic 判定（上面那行）与它自己的错误串都排在前面，所以 panic 永远优先。 */
        if (restartPending() && result.error.empty())
        {
            stopForRestart();
            return false;
        }
        if (!result.error.empty()) return false;
        if (stepLimit != 0u && result.steps >= stepLimit)
        {
            fail("指令条数超过上限（" + Memory::toString(stepLimit) + "），可能是死循环");
            return false;
        }
        const Instruction &instruction = program.code[pc];
        const std::size_t executedPc = pc;

        // 调试：断点命中或单步模式下，**先回调再执行**（所以断点看到的是"即将执行的那条"）。
        // 没挂调试回调时"停下"没有意义，直接跳过，普通运行零开销。
        if (debugHook_ != nullptr && (singleStep_ || breakpoints_.count(pc) != 0u))
        {
            DebugEvent event;
            event.instruction = pc;
            event.steps = result.steps;
            event.opcode = instruction.toString();
            if (debugInfo_ != nullptr)
            {
                if (const SourceLocation *where = debugInfo_->lookup(pc)) event.location = *where;
            }
            event.breakpoint = !singleStep_ || breakpoints_.count(pc) != 0u;
            event.frames = debugFramesOf(pc);
            currentLocation_ = event.location;
            debugFrames_ = event.frames;
            const DebugAction action = debugHook_(event);
            if (action == DebugAction::Abort)
            {
                fail("调试器中止在 " + event.location.toString() + "（第 " + Memory::toString(pc) + " 条指令）");
                return false;
            }
            singleStep_ = action == DebugAction::StepInto;
        }
        /* 新核心只在无观察者的同步区间运行；复杂操作返回既有运行时服务。
         * 每批最多 4096 次分派，再次经过 panic / restart / 中断安全点。 */
        if (registerExecution_ && optimizeExecution_ && !trace_ && !debugHook_ && !instructionHook_ && !timeCallback_ && watchdog.period == 0 && (plan.code[pc].internal != absent || instruction.op == Op::Jmp || instruction.op == Op::Load || instruction.op == Op::PushInt))
        {
            const auto &region = registerEngine.region(pc);
            if (Memory::restartSignal().pending())
            {
                stopForRestart();
                return false;
            }
            bool bound = boundRegion == &region;
            if (!bound)
            {
                registerSlots.clear();
                bound = !region.code.empty() && !Memory::restartSignal().pending();
                if (bound)
                    for (const auto operand : region.operands)
                    {
                        Slot *slot = readPrepared(operand);
                        if (!slot || slot->isFloat || slot->isText)
                        {
                            bound = false;
                            break;
                        }
                        registerSlots.push_back(&slot->integer);
                    }
                boundRegion = bound ? &region : nullptr;
            }
            if (bound)
            {
                const auto budget = stepLimit == 0 ? static_cast<std::size_t>(-1) : stepLimit - result.steps;
                const auto capacity = stackLimit_ == 0 ? static_cast<std::size_t>(-1) :
                                                         (stack.size() <= stackLimit_ ? stackLimit_ - stack.size() : 0);
                /* 服务只接触数值或 pending，不声明变量、不切换帧，不使绑定地址失效。
                 * 失败前不提交副作用；交还原执行器逐条报告原始 PC 和栈状态。 */
                const auto services = [&](Op operation, std::size_t at, std::int64_t a, std::int64_t b, std::int64_t &out) {
                    if (operation == Op::Param)
                    {
                        pending.push_back(PendingArg{ program.code[at + 1].name, Slot::fromInt(a) });
                        return true;
                    }
                    if (operation == Op::LoadAt)
                    {
                        Slot value;
                        if (!loadMemory(static_cast<std::size_t>(a), false, value, program.code[at + 1].intValue == 1 ? 1u : 8u)) return false;
                        out = value.integer;
                        return true;
                    }
                    if (operation == Op::StoreAt) return storeMemory(static_cast<std::size_t>(a), false, Slot::fromInt(b), program.code[at + 2].intValue == 1 ? 1u : 8u);
                    if (operation == Op::Check) return a >= 0 && a < b;
                    if (operation == Op::CheckPointer)
                    {
                        if (a < 0) return false;
                        const auto address = static_cast<std::size_t>(b);
                        const auto stride = static_cast<std::size_t>(program.code[at + 2].intValue > 0 ? program.code[at + 2].intValue : 8);
                        std::size_t capacity = 0;
                        if (inHeap(address))
                        {
                            if (!heap.capacityAt(address, capacity)) return false;
                        }
                        else if (data_)
                        {
                            if (!dataCapacity.capacityAt(address, capacity)) return false;
                        }
                        // 用除法判界，避免下标乘步长和加法溢出。
                        return capacity >= stride && static_cast<std::uint64_t>(a) < capacity / stride;
                    }
                    return false;
                };
                const auto progress = register_core::Engine::executeWithServices(region, registerSlots, pc, budget, capacity,
                                                                                 registerBlockExecution_, registerThreadedDispatch_, services);
                if (progress.dispatches)
                {
                    result.steps += progress.steps;
                    result.fusedGroups += progress.groups;
                    result.registerDispatches += progress.dispatches;
                    ++result.registerEntries;
                    result.maxStackDepth = std::max(result.maxStackDepth, stack.size() + progress.peak);
                    pc = progress.pc;
                    return pc < program.code.size();
                }
            }
        }

        boundRegion = nullptr;
        /* 两端共用的整数四指令融合；原始 PC、逻辑步数、栈深及错误位置保持不变。
         * 有指令边界观察者时逐条执行，避免跨过断点、钩子、计时器或异步中断安全点。 */
        if (optimizeExecution_ && plan.code[pc].internal != execution_detail::absent && !trace_ && debugHook_ == nullptr && !instructionHook_ && !timeCallback_ && watchdog.period == 0 && (stepLimit == 0 || stepLimit - result.steps >= plan.registers[plan.code[pc].internal].span) && (stackLimit_ == 0 || (stack.size() <= stackLimit_ && stackLimit_ - stack.size() >= 2)))
        {
            const auto &internal = plan.registers[plan.code[pc].internal];
            const Slot *left = readPrepared(internal.left);
            const Slot immediate = internal.floatingLiteral ? Slot::fromDouble(internal.realImmediate) : Slot::fromInt(internal.immediate);
            const Slot *right = internal.literal ? &immediate : readPrepared(internal.right);
            if (left && right && (left->isFloat || right->isFloat) && !left->isText && !right->isText && internal.operation != Op::LtU && internal.operation != Op::LeU && internal.operation != Op::GtU && internal.operation != Op::GeU)
            {
                const auto operation = internal.operation;
                const double a = left->number(), b = right->number();
                Slot value;
                switch (operation)
                {
                case Op::Add: value = Slot::fromDouble(normalizeFloat(a + b, internal.bits)); break;
                case Op::Sub: value = Slot::fromDouble(normalizeFloat(a - b, internal.bits)); break;
                case Op::Mul: value = Slot::fromDouble(normalizeFloat(a * b, internal.bits)); break;
                case Op::Eq: value = Slot::fromInt(a == b); break;
                case Op::Ne: value = Slot::fromInt(a != b); break;
                case Op::Lt:
                case Op::LtU: value = Slot::fromInt(a < b); break;
                case Op::Le:
                case Op::LeU: value = Slot::fromInt(a <= b); break;
                case Op::Gt:
                case Op::GtU: value = Slot::fromInt(a > b); break;
                case Op::Ge:
                case Op::GeU: value = Slot::fromInt(a >= b); break;
                default: return false;
                }
                if (internal.storeResult) storePrepared(internal.destination, value);
                const bool zero = internal.storeResult ? normalizeStored(plan.code[internal.destination].symbol, value).number() == 0.0 : value.number() == 0.0;
                result.steps += internal.span;
                ++result.fusedGroups;
                result.maxStackDepth = std::max(result.maxStackDepth, stack.size() + 2);
                pc = internal.kind == execution_detail::RegisterOp::IntegerBranch && (zero == internal.jumpOnZero) ? internal.target : pc + internal.span;
                return pc < program.code.size();
            }
            if (left && right && !left->isFloat && !right->isFloat && !left->isText && !right->isText)
            {
                const auto operation = internal.operation;
                const auto a = left->integer, b = right->integer;
                std::int64_t value = 0;
                switch (operation)
                {
                case Op::Add: value = static_cast<std::int64_t>(static_cast<std::uint64_t>(a) + static_cast<std::uint64_t>(b)); break;
                case Op::Sub: value = static_cast<std::int64_t>(static_cast<std::uint64_t>(a) - static_cast<std::uint64_t>(b)); break;
                case Op::Mul: value = static_cast<std::int64_t>(static_cast<std::uint64_t>(a) * static_cast<std::uint64_t>(b)); break;
                case Op::Eq: value = a == b; break;
                case Op::Ne: value = a != b; break;
                case Op::Lt: value = a < b; break;
                case Op::Le: value = a <= b; break;
                case Op::Gt: value = a > b; break;
                case Op::Ge: value = a >= b; break;
                case Op::LtU: value = static_cast<std::uint64_t>(a) < static_cast<std::uint64_t>(b); break;
                case Op::LeU: value = static_cast<std::uint64_t>(a) <= static_cast<std::uint64_t>(b); break;
                case Op::GtU: value = static_cast<std::uint64_t>(a) > static_cast<std::uint64_t>(b); break;
                case Op::GeU: value = static_cast<std::uint64_t>(a) >= static_cast<std::uint64_t>(b); break;
                default: break;
                }
                if (operation == Op::Add || operation == Op::Sub || operation == Op::Mul)
                {
                    TypeInfo type;
                    type.bits = internal.bits;
                    type.isUnsigned = internal.unsignedResult;
                    value = normalizeInt(value, type);
                }
                if (internal.storeResult)
                    storePrepared(internal.destination, Slot::fromInt(value));
                const bool zero = internal.kind == execution_detail::RegisterOp::IntegerBranch && internal.storeResult ?
                                      normalizeStored(plan.code[internal.destination].symbol, Slot::fromInt(value)).number() == 0.0 :
                                      value == 0;
                result.steps += internal.span;
                ++result.fusedGroups;
                result.maxStackDepth = std::max(result.maxStackDepth, stack.size() + 2);
                pc = internal.kind == execution_detail::RegisterOp::IntegerBranch && (zero == internal.jumpOnZero) ? internal.target : pc + internal.span;
                return pc < program.code.size();
            }
        }

        ++result.steps;
        result.maxStackDepth = std::max(result.maxStackDepth, stack.size());
        if (trace_)
        {
            result.trace.push_back("[" + Memory::toString(pc) + "] " + instruction.toString());
        }

        // 所有压栈统一经过容量检查；上限按执行上下文分别计算。
        auto push = [&](Slot value) -> bool {
            if (stackLimit_ != 0u && stack.size() >= stackLimit_)
            {
                fail("求值栈超过上限（" + Memory::toString(stackLimit_) + " 个槽，第 " + Memory::toString(pc) + " 条指令 " + instruction.toString() + "）");
                return false;
            }
            stack.push_back(std::move(value));
            result.maxStackDepth = std::max(result.maxStackDepth, stack.size());
            return true;
        };

        // 需要弹栈的指令统一在这里取。
        auto pop = [&](Slot &out) -> bool {
            if (stack.empty())
            {
                fail("求值栈为空（第 " + Memory::toString(pc) + " 条指令 " + instruction.toString() + "）");
                return false;
            }
            out = stack.back();
            stack.pop_back();
            return true;
        };
        jumped = false;
        switch (instruction.op)
        {
        case Op::PushInt: push(Slot::fromInt(instruction.intValue)); break;
        case Op::PushDouble: push(Slot::fromDouble(instruction.doubleValue)); break;
        case Op::PushText: push(Slot::fromText(instruction.textValue)); break;
        case Op::Load:
        {
            const Slot *slot = readPrepared(pc);
            if (slot == nullptr)
            {
                fail("读未定义的变量：" + instruction.name);
                break;
            }
            push(*slot);
            break;
        }
        case Op::Store:
        {
            Slot slot;
            if (!pop(slot)) break;
            storePrepared(pc, slot);
            break;
        }
        case Op::Declare:
        {
            if (optimizeExecution_)
            {
                const auto &prepared = plan.code[pc];
                const auto symbol = prepared.symbol;
                const auto zero = plan.types[symbol].type && plan.types[symbol].type->isFloat ? Slot::fromDouble(0) : Slot::fromInt(0);
                if (!frames.empty())
                {
                    auto &frame = *frames.back();
                    const auto slot = frame.layout->slots.find(symbol);
                    if (slot != frame.layout->slots.end() && !frame.bound[slot->second])
                    {
                        frame.slots[slot->second] = zero;
                        frame.bound[slot->second] = true;
                    }
                }
                else if (!globalBound[symbol])
                {
                    globalSlots[symbol] = zero;
                    globalBound[symbol] = true;
                }
            }
            else
            {
                auto &scope = frames.empty() ? globals : frames.back()->locals;
                if (scope.find(instruction.name) == scope.end())
                {
                    const auto type = program.types.find(instruction.name);
                    scope[instruction.name] = type != program.types.end() && type->second.isFloat ? Slot::fromDouble(0) : Slot::fromInt(0);
                }
            }
            break;
        }
        case Op::Neg:
        {
            Slot slot;
            if (!pop(slot)) break;
            if (slot.isFloat || instruction.isFloat)
                push(Slot::fromDouble(
                    normalizeFloat(-slot.number(), static_cast<int>(instruction.intValue))));
            else
            {
                TypeInfo type;
                type.bits = static_cast<int>(instruction.intValue);
                type.isUnsigned = instruction.unsignedOp;
                push(Slot::fromInt(normalizeInt(-slot.integer, type)));
            }
            break;
        }
        case Op::Not:
        {
            Slot slot;
            if (!pop(slot)) break;
            TypeInfo type;
            type.bits = static_cast<int>(instruction.intValue);
            type.isUnsigned = instruction.unsignedOp;
            push(Slot::fromInt(normalizeInt(~slot.integer, type)));
            break;
        }
        case Op::Add:
        case Op::Sub:
        case Op::Mul:
        case Op::Div:
        case Op::Mod:
        {
            Slot right;
            Slot left;
            if (!pop(right) || !pop(left)) break;
            const bool useFloat = left.isFloat || right.isFloat;
            if (useFloat)
            {
                const double a = left.number();
                const double b = right.number();
                if ((instruction.op == Op::Div || instruction.op == Op::Mod) && b == 0.0)
                {
                    fail("浮点除零");
                    break;
                }
                double value = 0.0;
                switch (instruction.op)
                {
                case Op::Add: value = a + b; break;
                case Op::Sub: value = a - b; break;
                case Op::Mul: value = a * b; break;
                case Op::Div: value = a / b; break;
                default: value = std::fmod(a, b); break;
                }
                push(Slot::fromDouble(
                    normalizeFloat(value, static_cast<int>(instruction.intValue))));
                break;
            }
            const std::int64_t a = left.integer;
            const std::int64_t b = right.integer;
            if ((instruction.op == Op::Div || instruction.op == Op::Mod) && b == 0)
            {
                fail("整数除零");
                break;
            }
            std::int64_t value = 0;
            if (instruction.unsignedOp && (instruction.op == Op::Div || instruction.op == Op::Mod))
            {
                const std::uint64_t ua = static_cast<std::uint64_t>(a);
                const std::uint64_t ub = static_cast<std::uint64_t>(b);
                value = static_cast<std::int64_t>(instruction.op == Op::Div ? ua / ub : ua % ub);
            }
            else
            {
                switch (instruction.op)
                {
                case Op::Add: value = static_cast<std::int64_t>(static_cast<std::uint64_t>(a) + static_cast<std::uint64_t>(b)); break;
                case Op::Sub: value = static_cast<std::int64_t>(static_cast<std::uint64_t>(a) - static_cast<std::uint64_t>(b)); break;
                case Op::Mul: value = static_cast<std::int64_t>(static_cast<std::uint64_t>(a) * static_cast<std::uint64_t>(b)); break;
                case Op::Div: value = (a == INT64_MIN && b == -1) ? a : a / b; break;
                default: value = (a == INT64_MIN && b == -1) ? 0 : a % b; break;
                }
            }
            TypeInfo type;
            type.bits = static_cast<int>(instruction.intValue);
            type.isUnsigned = instruction.unsignedOp;
            push(Slot::fromInt(normalizeInt(value, type)));
            break;
        }
        case Op::Shl:
        case Op::Shr:
        case Op::ShrU:
        {
            Slot right;
            Slot left;
            if (!pop(right) || !pop(left)) break;
            const int shift = static_cast<int>(right.integer & 63);
            const auto bits = static_cast<std::uint64_t>(left.integer);
            const auto value = instruction.op == Op::Shl ? static_cast<std::int64_t>(bits << shift) :
                               instruction.op == Op::Shr ? (left.integer >> shift) :
                                                           static_cast<std::int64_t>(bits >> shift);
            TypeInfo type;
            type.bits = static_cast<int>(instruction.intValue);
            type.isUnsigned = instruction.unsignedOp;
            push(Slot::fromInt(normalizeInt(value, type)));
            break;
        }
        case Op::BitAnd:
        case Op::BitOr:
        case Op::BitXor:
        {
            Slot right;
            Slot left;
            if (!pop(right) || !pop(left)) break;
            std::int64_t value = 0;
            if (instruction.op == Op::BitAnd)
                value = left.integer & right.integer;
            else if (instruction.op == Op::BitOr)
                value = left.integer | right.integer;
            else
                value = left.integer ^ right.integer;
            push(Slot::fromInt(value));
            break;
        }
        case Op::Eq:
        case Op::Ne:
        case Op::Lt:
        case Op::Le:
        case Op::Gt:
        case Op::Ge:
        case Op::LtU:
        case Op::LeU:
        case Op::GtU:
        case Op::GeU:
        {
            Slot right;
            Slot left;
            if (!pop(right) || !pop(left)) break;
            bool value = false;
            if (instruction.op == Op::Eq || instruction.op == Op::Ne)
            {
                const bool equal = (left.isFloat || right.isFloat) ? (left.number() == right.number()) : (left.integer == right.integer);
                value = (instruction.op == Op::Eq) ? equal : !equal;
            }
            else
            {
                const bool unsignedCompare = (instruction.op == Op::LtU) || (instruction.op == Op::LeU) || (instruction.op == Op::GtU) || (instruction.op == Op::GeU);
                const double a = left.number();
                const double b = right.number();
                if (unsignedCompare)
                {
                    const std::uint64_t ua = static_cast<std::uint64_t>(left.integer);
                    const std::uint64_t ub = static_cast<std::uint64_t>(right.integer);
                    if (instruction.op == Op::LtU)
                        value = ua < ub;
                    else if (instruction.op == Op::LeU)
                        value = ua <= ub;
                    else if (instruction.op == Op::GtU)
                        value = ua > ub;
                    else
                        value = ua >= ub;
                }
                else if (!left.isFloat && !right.isFloat)
                {
                    if (instruction.op == Op::Lt)
                        value = left.integer < right.integer;
                    else if (instruction.op == Op::Le)
                        value = left.integer <= right.integer;
                    else if (instruction.op == Op::Gt)
                        value = left.integer > right.integer;
                    else
                        value = left.integer >= right.integer;
                }
                else
                {
                    if (instruction.op == Op::Lt)
                        value = a < b;
                    else if (instruction.op == Op::Le)
                        value = a <= b;
                    else if (instruction.op == Op::Gt)
                        value = a > b;
                    else
                        value = a >= b;
                }
            }
            push(Slot::fromInt(value ? 1 : 0));
            break;
        }
        case Op::ToDouble:
        {
            Slot slot;
            if (!pop(slot)) break;
            push(Slot::fromDouble(
                normalizeFloat(!slot.isFloat && instruction.unsignedOp ? static_cast<double>(static_cast<std::uint64_t>(slot.integer)) : slot.number(), static_cast<int>(instruction.intValue))));
            break;
        }
        case Op::ToInt:
        {
            Slot slot;
            if (!pop(slot)) break;
            TypeInfo type;
            type.bits = static_cast<int>(instruction.intValue);
            type.isUnsigned = instruction.unsignedOp;
            push(Slot::fromInt(
                normalizeInt(slot.isFloat ? static_cast<std::int64_t>(nativeInt(slot)) : slot.integer, type)));
            break;
        }
        case Op::MulWide:
        {
            Slot right;
            Slot left;
            if (!pop(right) || !pop(left)) break;
            const int bits = (instruction.intValue > 0 && instruction.intValue <= 64) ? static_cast<int>(instruction.intValue) : 32;
            const std::uint64_t mask = (bits >= 64) ? ~0ULL : ((1ULL << bits) - 1ULL);
            const std::uint64_t a = static_cast<std::uint64_t>(left.integer) & mask;
            const std::uint64_t b = static_cast<std::uint64_t>(right.integer) & mask;
            const std::uint64_t product = a * b;
            push(Slot::fromInt(static_cast<std::int64_t>(product & mask)));            // lo
            push(Slot::fromInt(static_cast<std::int64_t>((product >> bits) & mask)));  // hi
            break;
        }
        case Op::DivWide:
        {
            Slot right;
            Slot left;
            if (!pop(right) || !pop(left)) break;
            const int bits = (instruction.intValue > 0 && instruction.intValue <= 64) ? static_cast<int>(instruction.intValue) : 32;
            const std::uint64_t mask = (bits >= 64) ? ~0ULL : ((1ULL << bits) - 1ULL);
            const std::uint64_t ua = static_cast<std::uint64_t>(left.integer) & mask;
            const std::uint64_t ub = static_cast<std::uint64_t>(right.integer) & mask;

            std::int64_t quotient = 0;
            std::int64_t remainder = 0;
            if (instruction.unsignedOp)
            {
                if (ub == 0u)
                {
                    fail("双字除法除零");
                    break;
                }
                quotient = static_cast<std::int64_t>(ua / ub);
                remainder = static_cast<std::int64_t>(ua % ub);
            }
            else
            {
                TypeInfo signedType = typeOf("i32");
                signedType.bits = bits;
                signedType.isUnsigned = false;
                signedType.isFloat = false;
                const std::int64_t sa = normalizeInt(static_cast<std::int64_t>(ua), signedType);
                const std::int64_t sb = normalizeInt(static_cast<std::int64_t>(ub), signedType);
                if (sb == 0)
                {
                    fail("双字除法除零");
                    break;
                }
                quotient = sa / sb;
                remainder = sa % sb;
            }
            push(Slot::fromInt(quotient));   // q
            push(Slot::fromInt(remainder));  // r
            break;
        }
        case Op::Jmp:
        {
            const auto target = plan.code[pc].target;
            if (target == absent)
            {
                fail("跳转目标不存在：" + instruction.name);
                break;
            }
            pc = target;
            jumped = true;
            break;
        }
        case Op::Jz:
        case Op::Jnz:
        {
            Slot slot;
            if (!pop(slot)) break;
            const bool zero = (slot.number() == 0.0);
            const bool take = (instruction.op == Op::Jz) ? zero : !zero;
            if (take)
            {
                const auto target = plan.code[pc].target;
                if (target == absent)
                {
                    fail("跳转目标不存在：" + instruction.name);
                    break;
                }
                pc = target;
                jumped = true;
            }
            break;
        }
        case Op::Print:
        {
            Slot slot;
            if (!pop(slot)) break;
            switch (instruction.format)
            {
            case PrintFormat::Unsigned:
                emitNumber(Memory::toString(static_cast<std::uint64_t>(slot.integer)));
                break;
            case PrintFormat::Float:
                emitNumber(formatDouble(slot.number()));
                break;
            case PrintFormat::Text:
                emitText(slot.text.data(), slot.text.size());
                break;
            default:
                emitNumber(Memory::toString(static_cast<std::int64_t>(nativeInt(slot))));
                break;
            }
            break;
        }
        case Op::PrintText:
            emitText(instruction.textValue.data(), instruction.textValue.size());
            break;
        case Op::Putc:
        {
            Slot slot;
            if (!pop(slot)) break;
            const char byte = static_cast<char>(static_cast<int64_t>(nativeInt(slot)) & 0xFF);
            emitText(&byte, 1u);
            break;
        }
        case Op::Read:
        case Op::Getc:
        {
            if (inputIndex >= input.size())
            {
                fail("输入不够（第 " + Memory::toString(inputIndex + 1u) + " 项缺失）");
                break;
            }
            const Memory::String &item = input[inputIndex++];
            if (instruction.op == Op::Getc)
            {
                const int code = item.empty() ? 0 : static_cast<unsigned char>(item[0]);
                assign(instruction.name, Slot::fromInt(code));
                break;
            }
            const TypeInfo info = [&] {
                const auto found = program.types.find(instruction.name);
                return (found == program.types.end()) ? typeOf("") : found->second;
            }();
            bool isFloat = false;
            std::int64_t intValue = 0;
            double doubleValue = 0.0;
            if (!parseNumber(item, isFloat, intValue, doubleValue))
            {
                fail("输入 '" + item + "' 不是数字（变量 " + instruction.name + "）");
                break;
            }
            if (info.isFloat)
                assign(instruction.name, Slot::fromDouble(normalizeFloat(
                                             isFloat ? doubleValue : static_cast<double>(intValue), info.bits)));
            else
                assign(instruction.name, Slot::fromInt(isFloat ? static_cast<std::int64_t>(doubleValue) : intValue));
            break;
        }
        case Op::Param:
        {
            Slot slot;
            if (!pop(slot)) break;
            pending.push_back(PendingArg{ instruction.name, slot });
            break;
        }
        case Op::Alloc:
        {
            Slot size;
            if (!pop(size)) break;
            const std::int64_t bytes = size.isFloat ? 0 : size.integer;
            if (bytes <= 0)
            {
                fail("alloc 的字节数必须是正数（拿到 " + Memory::toString(bytes) + "）");
                break;
            }
            if (static_cast<std::uint64_t>(bytes) > std::numeric_limits<std::size_t>::max())
            {
                fail("alloc 的字节数超出地址空间");
                break;
            }
            const std::size_t address = heap.allocate(static_cast<std::size_t>(bytes));
            if (address == 0u)
            {
                fail("堆内存不足：申请 " + Memory::toString(bytes) + " 字节，当前在用 " + Memory::toString(heap.liveBytes()) + " 字节");
                break;
            }
            objects.beginStorage(address, static_cast<std::size_t>(bytes), true);
            push(Slot::fromInt(static_cast<std::int64_t>(address)));
            break;
        }
        case Op::Free:
        {
            Slot address;
            if (!pop(address)) break;
            /* 句柄区间（`bint` / 容器）走对象表；其余按堆地址严格释放。 */
            const std::int64_t handle = static_cast<std::int64_t>(nativeInt(address));
            if (trace_) result.trace.push_back("    release " + Memory::toString(handle));
            if (isObjectHandle(handle))
            {
                if (!objects.release(handle))
                {
                    fail("free 失败：这个对象已经释放过了（句柄 " + Memory::toString(handle) + "）");
                }
                break;
            }
            Memory::String reason;
            std::size_t capacity = 0;
            const auto target = static_cast<std::size_t>(nativeInt(address));
            if (heap.capacityAt(target, capacity)) objects.beginStorage(target, capacity, true);
            if (!heap.release(target, &reason))
            {
                fail("free 失败：" + reason);
                break;
            }
            break;
        }
        case Op::Drop:
        {
            /* 编译器用空槽表示没有拥有值；有效地址严格释放，失效身份不能静默通过。 */
            Slot address;
            if (!pop(address)) break;
            if (trace_) result.trace.push_back("    drop " + Memory::toString(static_cast<std::int64_t>(nativeInt(address))));
            /* 对象表同样严格检查资源身份。 */
            const std::int64_t handle = static_cast<std::int64_t>(nativeInt(address));
            if (isObjectHandle(handle))
            {
                if (!objects.release(handle)) fail("[MGL1205] drop 的对象已经释放或句柄无效");
                break;
            }
            const std::size_t target = static_cast<std::size_t>(nativeInt(address));
            if (target == 0u || !inHeap(target)) break;
            Memory::String reason;
            std::size_t capacity = 0;
            if (heap.capacityAt(target, capacity)) objects.beginStorage(target, capacity, true);
            if (!heap.release(target, &reason)) fail("[MGL1205] drop 失败：" + reason);
            break;
        }
        case Op::Check:
        {
            // `(check, 下标, 上限, 标签)`：弹栈取上限与下标，越界就终止运行。
            Slot limit;
            Slot index;
            if (!pop(limit) || !pop(index)) break;
            const std::int64_t position = static_cast<std::int64_t>(nativeInt(index));
            const std::int64_t length = static_cast<std::int64_t>(nativeInt(limit));
            if (position < 0 || position >= length)
            {
                fail("数组下标越界：" + (instruction.name.empty() ? Memory::String("数组") : instruction.name) + " 的下标 " + Memory::toString(position) + "，长度 " + Memory::toString(length));
                break;
            }
            break;
        }
        case Op::CheckPointer:
        {
            // `(checkptr, 下标, 基址, 步长, 标签)`：按"基址所在那一块"的容量查这个元素。
            Slot base;
            Slot index;
            if (!pop(base) || !pop(index)) break;
            const std::int64_t position = static_cast<std::int64_t>(nativeInt(index));
            const std::int64_t stride =
                (instruction.intValue > 0) ? instruction.intValue : 8;
            const std::size_t address = static_cast<std::size_t>(nativeInt(base));
            const Memory::String label =
                instruction.name.empty() ? Memory::String("这一块") : instruction.name;

            // 1. 下标不能为负。
            if (position < 0)
            {
                fail("数组下标越界：" + label + " 的下标 " + Memory::toString(position) + "（不能为负）");
                break;
            }
            // 2. 找出基址落在哪一块里（堆块按分配大小；数据区按符号 size=）。
            std::size_t capacity = 0;
            bool found = false;
            if (inHeap(address))
            {
                found = heap.capacityAt(address, capacity);  // 没分配 / 已经 free → false
            }
            else if (data_ != nullptr)
            {
                found = dataCapacity.capacityAt(address, capacity);
            }
            if (!found || capacity == 0u)
            {
                fail("panic：数组下标检查失败：" + label + " 的地址 " + Memory::toString(address) + " 不在任何一块已分配内存里（没分配、已经 free，或者不是数组的起始地址）");
                break;
            }
            // 3. 这个元素（基址 + 下标 × 步长，共 stride 字节）必须整块落在容量里。
            if (capacity < static_cast<std::size_t>(stride) || static_cast<std::uint64_t>(position) >= capacity / static_cast<std::size_t>(stride))
            {
                fail("数组下标越界：" + label + " 的下标 " + Memory::toString(position) + "（这一块只有 " + Memory::toString(capacity) + " 字节，元素 " + Memory::toString(stride) + " 字节）");
                break;
            }
            break;
        }
        case Op::Call:
        {
            if (entryPending && frames.empty() && instruction.name == program.symbolKey("main"))
            {
                entryPending = false;
                pending.clear();
#if STACKVM_WITH_THREADS
                if (threadRuntime_ && threadRuntime_->workers.at(threadId_)->initial)
                {
                    NativeContext context;
                    context.heap = &heap;
                    context.data = data_;
                    context.objects = &objects;
                    Slot initial;
                    if (!restoreTransfer(context, *threadRuntime_->workers.at(threadId_)->initial, &initial))
                    {
                        fail(context.error);
                        break;
                    }
                    pending.clear();
                    pending.push_back(PendingArg{ "", initial });
                }
#endif
                if (!enterFunction(entryLabel_, instruction.name2, true)) break;
                break;
            }
            if (callNative(instruction.name, instruction.name2)) break;
            if (!enterFunction(instruction.name, instruction.name2, false)) break;
            break;
        }
        case Op::CallIndirect:
        {
            /* 函数指针：值就是"数据区里存着函数标签的那串字节"的地址（0 = 空指针）。 */
            Slot pointer;
            if (!pop(pointer)) break;
            const std::size_t address = static_cast<std::size_t>(nativeInt(pointer));
            if (address == 0u)
            {
                fail("调用了空函数指针（null）");
                pending.clear();
                break;
            }
            if (data_ == nullptr)
            {
                fail("没有加载数据区，无法通过函数指针取函数名");
                pending.clear();
                break;
            }
            std::int64_t environment = 0;
            const auto target = closureTarget(objects, static_cast<std::int64_t>(address), environment);
            const Memory::String label = target ? program.symbolKey(data_->readCString(static_cast<std::size_t>(target))) : Memory::String();
            if (label.empty())
            {
                fail("函数指针指向的地址 " + Memory::toString(address) + " 上没有函数名");
                pending.clear();
                break;
            }
            if (environment) pending.push_back(PendingArg{ "__magel_environment", Slot::fromInt(environment) });
            if (callNative(label, instruction.name2)) break;
            if (!enterFunction(label, instruction.name2, true)) break;
            break;
        }
        case Op::Ret:
        {
            Slot value;
            if (!pop(value)) break;
            if (frames.empty())
            {
                pc = program.code.size();
                jumped = true;
                break;
            }
            // 返回时先摘下本帧并写回返回值，再销毁局部值；小帧仅缓存空存储。
            // 活动递归层始终拥有各自的槽位，不会被缓存复用。
            Memory::UniquePtr<Frame> frame = std::move(frames.back());
            frames.pop_back();
            if (!frame->returnDest.empty()) assign(frame->returnDest, value);
            pc = frame->returnIp;
            if (optimizeExecution_ && registerExecution_ && registerServices_ && returnedFrames.size() < 8 && frame->slots.size() <= 256)
            {
                // 清空所有值及声明状态，保留容量；文本等宿主资源仍在函数返回时释放。
                frame->locals.clear();
                frame->slots.clear();
                frame->bound.clear();
                frame->returnDest.clear();
#if STACKVM_WITH_PANIC_TRACE
                frame->function.clear();
#endif
                frame->layout = nullptr;
                returnedFrames.push_back(std::move(frame));
            }
            jumped = true;
            break;
        }
        case Op::Halt:
            result.ok = true;
            pc = program.code.size();
            jumped = true;
            break;
        case Op::AddrOf:
        {
            if (data_ == nullptr)
            {
                fail("没有加载数据区，无法取符号地址：" + instruction.name);
                break;
            }
            const auto &symbolTable = program.compactSymbols ? dataSymbols : data_->symbols;
            const auto found = symbolTable.find(instruction.name);
            if (found == symbolTable.end())
            {
                fail("数据区里没有符号：" + instruction.name);
                break;
            }
            push(Slot::fromInt(static_cast<std::int64_t>(found->second)));
            break;
        }
        case Op::LoadAt:
        {
            Slot address;
            if (!pop(address)) break;
            const std::size_t target = static_cast<std::size_t>(nativeInt(address));
            if (data_ == nullptr && !inHeap(target))
            {
                fail("panic：没有加载数据区，无法按地址取值：地址 " + Memory::toString(target));
                break;
            }
            Slot value;
            if (!loadMemory(target, instruction.isFloat, value, instruction.intValue == 1 ? 1u : 8u))
            {
                const Memory::String where = inHeap(target) ? "堆" : "数据区";
                fail("panic：" + where + "越界读：地址 " + Memory::toString(target) + (inHeap(target) ? "（没分配或已经 free 过）" : ""));
                break;
            }
            push(value);
            break;
        }
        case Op::StoreAt:
        {
            Slot value;
            Slot address;
            if (!pop(value) || !pop(address)) break;
            const std::size_t target = static_cast<std::size_t>(nativeInt(address));
            if (data_ == nullptr && !inHeap(target))
            {
                fail("panic：没有加载数据区，无法按地址写值：地址 " + Memory::toString(target));
                break;
            }
            if (!storeMemory(target, instruction.isFloat, value, instruction.intValue == 1 ? 1u : 8u))
            {
                const Memory::String where = inHeap(target) ? "堆" : "数据区";
                fail("panic：" + where + "越界写：地址 " + Memory::toString(target) + (inHeap(target) ? "（没分配或已经 free 过）" : ""));
                break;
            }
            break;
        }
        }

        /* 指令钩子是同步宿主观测入口；事件投递不会隐式执行用户函数。 */
        if (hookInterval_ != 0u && instructionHook_ && result.steps % hookInterval_ == 0u)
        {
            const Memory::String text = instruction.toString();
            const std::size_t separator = text.find(' ');
            instructionHook_(InstructionHookInfo{
                result.steps, executedPc, text.substr(0u, separator) });
        }
        if (!result.error.empty() || result.cancelled || result.recoverable) return false;
        if (!jumped) ++pc;
        return pc < program.code.size();
    };

    while (executeOne()) {}

#if STACKVM_WITH_THREADS
    if (threadId_ == 0u && threadRuntime_ != nullptr)
    {
        {
            std::lock_guard<std::mutex> guard(threadRuntime_->mutex);
            threadRuntime_->closing = true;
            for (const auto &item : threadRuntime_->workers)
                if (item.first != 0u && !item.second->done) item.second->control->requested.store(true, std::memory_order_release);
        }
        threadRuntime_->changed.notify_all();
        for (;;)
        {
            std::shared_ptr<ThreadRuntime::Worker> worker;
            std::size_t id = 0u;
            {
                std::lock_guard<std::mutex> guard(threadRuntime_->mutex);
                for (const auto &item : threadRuntime_->workers)
                {
                    if (item.first == 0u || item.second->joined) continue;
                    id = item.first;
                    worker = item.second;
                    worker->joined = true;
                    break;
                }
            }
            if (worker == nullptr) break;
            if (worker->host.joinable()) worker->host.join();
            if (!worker->reported)
            {
                emitText(worker->result.text.data(), worker->result.text.size());
                if (!worker->result.output.empty())
                {
                    if (!result.output.empty()) result.output += ' ';
                    result.output += worker->result.output;
                }
                if (!worker->result.ok && !worker->result.cancelled && result.error.empty())
                    failFromThread("线程 " + Memory::toString(id) + " 失败：" + worker->result.error, worker->result);
            }
        }
        stopForPanic();
        threadRuntime_.reset();
    }
#else
    /* 裁剪配置没有线程运行时：线程收尾（关闭 / join / 合并子线程输出）整段不存在。 */
#endif /* STACKVM_WITH_THREADS */

    stopForPanic();
    /* 兜住"失败发生在最后一条指令之后 / 循环外"的场合：失败点自己 fail() 过（error 非空）
     * 也算可恢复终止，除非那是 panic —— panic 绝不能走软重启。 */
    if (restartPending() && !result.recoverable && result.error.rfind("panic", 0u) != 0u)
        stopForRestart();
    if (result.error.empty() && !result.ok && !result.recoverable && !result.cancelled)
    {
        // 掉出程序尾也算正常结束（组装时补过 Halt，这里兜底）。
        result.ok = true;
    }
    for (const std::int64_t handle : objects.materializedObjects) objects.release(handle);
    for (const std::size_t address : objects.materializedHeap)
    {
        Memory::String ignored;
        heap.release(address, &ignored);
    }
    // 堆统计：停机时还有多少字节没 free —— 非 0 就是漏了，交给调用方决定报不报。
    result.heapPeakBytes = heap.peakBytes();
    result.heapLiveBytes = heap.liveBytes();
    result.heapLiveBlocks = heap.liveBlocks();
    result.liveObjects = objects.liveCount();
    if (result.timingAvailable)
    {
        result.finishedNanoseconds = timeCallback_();
        result.elapsedNanoseconds = result.finishedNanoseconds >= result.startedNanoseconds ? result.finishedNanoseconds - result.startedNanoseconds : 0u;
    }
    // 收尾：把最后的位置与调用栈留成快照（出错时 fail() 已经填过，这里补成功停机的场合）。
    if (debugInfo_ != nullptr && result.ok && result.sourceTrace.empty())
    {
        if (const SourceLocation *where = debugInfo_->lookup(pc)) result.failureLocation = *where;
        result.sourceTrace = debugFramesOf(pc);
        currentLocation_ = result.failureLocation;
        debugFrames_ = result.sourceTrace;
    }
#if STACKVM_WITH_THREADS
    closeEventQueue(events_);
    events_.reset();
#endif
    // 本轮全部执行上下文已停下，下一轮 reset 清空分配与物理槽。
    return result;
}
}  // namespace stackvm
