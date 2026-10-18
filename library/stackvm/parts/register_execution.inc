/* 新核心只在无观察者的同步区间运行；复杂操作返回既有运行时服务。
 * 每批最多 4096 次分派，再次经过 panic / restart / 中断安全点。 */
if (registerExecution_ && optimizeExecution_ && !trace_ && !debugHook_ && !instructionHook_ && !timeCallback_ &&
#if !defined(STACKVM_WITH_THREADS) || STACKVM_WITH_THREADS
    !interruptMailbox_ && !threadsEnabled_ &&
#endif
    timers.running == 0 && timers.pending.empty() && (plan.code[pc].internal != absent || instruction.op == Op::Jmp || instruction.op == Op::Load || instruction.op == Op::PushInt))
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
                if (!loadMemory(static_cast<std::size_t>(a), false, value)) return false;
                out = value.integer;
                return true;
            }
            if (operation == Op::StoreAt) return storeMemory(static_cast<std::size_t>(a), false, Slot::fromInt(b));
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
