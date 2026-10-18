/* 两端共用的整数四指令融合；原始 PC、逻辑步数、栈深及错误位置保持不变。
 * 有指令边界观察者时逐条执行，避免跨过断点、钩子、计时器或异步中断安全点。 */
if (optimizeExecution_ && plan.code[pc].internal != execution_detail::absent && !trace_ && debugHook_ == nullptr && !instructionHook_ && !timeCallback_ &&
#if !defined(STACKVM_WITH_THREADS) || STACKVM_WITH_THREADS
    !interruptMailbox_ && !threadsEnabled_ &&
#endif
    timers.running == 0 && timers.pending.empty() && (stepLimit == 0 || stepLimit - result.steps >= plan.registers[plan.code[pc].internal].span) && (stackLimit_ == 0 || (stack.size() <= stackLimit_ && stackLimit_ - stack.size() >= 2)))
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
