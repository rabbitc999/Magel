/**
 * @file register_core.cpp
 * @brief 寄存器执行区域的准备、专用化与缓存。
 */
#include "runtime_internal.hpp"

namespace stackvm
{
using namespace runtimeDetail;
namespace
{
/**
 * @brief 是否是浮点窗口能直接算的比较（无符号那四个在精确执行器上按整数比，不在这里接）。
 * @param operation 操作码。
 * @return 是返回 true。
 */
bool compares(Op operation)
{
    switch (operation)
    {
    case Op::Eq:
    case Op::Ne:
    case Op::Lt:
    case Op::Le:
    case Op::Gt:
    case Op::Ge: return true;
    default: return false;
    }
}
}  // namespace
namespace register_core
{
/**
 * @brief 建立核心。
 * @param program 程序。
 * @param plan 执行计划。
 * @param foldJumps 是否合并跳转。
 * @param specialize 是否选择专用操作码。
 * @param services 是否降低运行时服务操作。
 */
Engine::Engine(const Program &program, const execution_detail::ExecutionPlan &plan, bool foldJumps, bool specialize, bool services) : program_(program), plan_(plan), foldJumps_(foldJumps), specialize_(specialize), services_(services)
{
}

/**
 * @brief 获取最多 256 条内部指令的区域。
 * @param entry 原始入口。
 * @return 缓存区域。
 */
const Region &Engine::region(std::size_t entry)

{
    const auto found = regions_.find(entry);
    if (found != regions_.end()) return *found->second;
    auto owned = Memory::makeUnique<Region>();
    auto &result = *owned;
    Memory::Map<std::size_t, std::size_t> indices;
    Memory::Map<std::uint64_t, std::uint16_t> registers;
    Memory::Vector<std::size_t> work{ entry };
    /* 槽位按「符号 + 形态」登记：同一个名字不可能同时按整数和双精度绑定，
     * 先用 claim 判一次，冲突的指令整条不进区域（否则运行期永远绑不上，白白回退）。 */
    const auto keyOf = [](std::size_t symbol, bool real) {
        return (static_cast<std::uint64_t>(symbol) << 1) | (real ? 1ull : 0ull);
    };
    const auto claim = [&](std::size_t pc, bool real) {
        const auto symbol = plan_.code[pc].symbol;
        if (symbol == absent) return true;
        return registers.find(keyOf(symbol, real)) != registers.end() || registers.find(keyOf(symbol, !real)) == registers.end();
    };
    const auto bind = [&](std::size_t pc, bool real) {
        const auto symbol = plan_.code[pc].symbol;
        std::uint16_t index = 0;
        if (symbol != absent)
        {
            const auto key = keyOf(symbol, real);
            const auto found = registers.find(key);
            if (found != registers.end()) return found->second;
            index = static_cast<std::uint16_t>(result.operands.size());
            registers.emplace(key, index);
        }
        else
            index = static_cast<std::uint16_t>(result.operands.size());
        result.operands.push_back(pc);
        result.reals.push_back(real ? 1u : 0u);
        return index;
    };
    const auto slotType = [&](std::size_t pc, bool &floating) {
        const auto *type = plan_.types[plan_.code[pc].symbol].type;
        floating = type && type->isFloat;
        return type ? encode(type->bits, type->isUnsigned) : std::uint8_t(0);
    };
    const auto realSlot = [&](std::size_t pc) {
        const auto *type = plan_.types[plan_.code[pc].symbol].type;
        return type != nullptr && type->isFloat;
    };
    while (!work.empty() && result.code.size() < 256 && !Memory::restartSignal().pending())
    {
        const auto pc = work.back();
        work.pop_back();
        if (pc >= program_.code.size() || indices.count(pc)) continue;
        Instruction instruction;
        Mapping map;
        map.pc = pc;
        bool floating = false;
        const auto &source = program_.code[pc];
        if (plan_.code[pc].internal != absent)
        {
            const auto &internal = plan_.registers[plan_.code[pc].internal];
            // 区域执行整数与浮点两种窗口；宽乘除、拷贝与一元运算留在精确执行器上。
            const bool usableWindow = internal.kind == execution_detail::RegisterOp::IntegerStore ||
                                      internal.kind == execution_detail::RegisterOp::IntegerBranch ||
                                      internal.kind == execution_detail::RegisterOp::FloatStore ||
                                      internal.kind == execution_detail::RegisterOp::FloatBranch;
            if (!usableWindow || internal.destination2 != absent)
            {
                indices.emplace(pc, absent);
                continue;
            }
            const bool compare = compares(internal.operation);
            const bool branching = internal.kind == execution_detail::RegisterOp::IntegerBranch ||
                                   internal.kind == execution_detail::RegisterOp::FloatBranch;
            /* 浮点窗口：运算、浮点立即数或选择了浮点形态的窗口都按双精度跑。
             * 比较的结果与精确执行器一致地写成整数 0 / 1（BoolResult），
             * 浮点值直接当分支条件的形态（`-0.0` 判零会有分歧）交还精确执行器。 */
            const bool real = internal.floatOperation || internal.floatingLiteral ||
                              internal.kind == execution_detail::RegisterOp::FloatStore ||
                              internal.kind == execution_detail::RegisterOp::FloatBranch;
            if (real && branching && !compare)
            {
                indices.emplace(pc, absent);
                continue;
            }
            if (!claim(internal.left, real) || (!internal.literal && !claim(internal.right, real)) ||
                (internal.storeResult && !claim(internal.destination, real && !compare)))
            {
                indices.emplace(pc, absent);
                continue;
            }
            instruction.kind = Kind::Arithmetic;
            instruction.operation = static_cast<std::uint8_t>(internal.operation);
            map.span = static_cast<std::uint16_t>(internal.span);
            map.peak = 2;
            map.groups = 1;
            if (real) instruction.flags |= Float;
            if (real && compare) instruction.flags |= BoolResult;
            instruction.left = bind(internal.left, real);
            instruction.immediate = internal.floatingLiteral ? realBits(internal.realImmediate) : internal.immediate;
            if (internal.literal)
                instruction.flags |= Literal;
            else
                instruction.right = bind(internal.right, real);
            if (branching) instruction.flags |= Branch | End;
            if (internal.jumpOnZero) instruction.flags |= Zero;
            map.target = internal.target;
            instruction.operationType = encode(internal.bits, internal.unsignedResult);
            if (internal.storeResult)
            {
                instruction.flags |= Store;
                instruction.destination = bind(internal.destination, real && !compare);
                instruction.destinationType = slotType(internal.destination, floating);
            }
        }
        else if (pc + 3 < program_.code.size() && source.op == Op::Load && (program_.code[pc + 1].op == Op::Load || program_.code[pc + 1].op == Op::PushInt) && extendedInteger(program_.code[pc + 2].op) && !program_.code[pc + 2].isFloat && program_.code[pc + 3].op == Op::Store && !hasLabel(pc + 1) && !hasLabel(pc + 2) && !hasLabel(pc + 3))
        {
            instruction.kind = Kind::Arithmetic;
            map.span = 4;
            map.peak = 2;
            map.groups = 1;
            instruction.operation = static_cast<std::uint8_t>(program_.code[pc + 2].op);
            if (!claim(pc, false) || (program_.code[pc + 1].op != Op::PushInt && !claim(pc + 1, false)) || !claim(pc + 3, false))
            {
                indices.emplace(pc, absent);
                continue;
            }
            instruction.left = bind(pc, false);
            if (program_.code[pc + 1].op == Op::PushInt)
                instruction.flags |= Literal;
            else
                instruction.right = bind(pc + 1, false);
            instruction.immediate = program_.code[pc + 1].intValue;
            instruction.flags |= Store;
            instruction.destination = bind(pc + 3, false);
            instruction.operationType = encode(static_cast<int>(program_.code[pc + 2].intValue), program_.code[pc + 2].unsignedOp);
            instruction.destinationType = slotType(pc + 3, floating);
        }
        else if (services_ && source.op == Op::Load && pc + 1 < program_.code.size() && !hasLabel(pc + 1) && program_.code[pc + 1].op == Op::Param)
        {
            if (!claim(pc, false))
            {
                indices.emplace(pc, absent);
                continue;
            }
            instruction.kind = Kind::Service;
            instruction.left = bind(pc, false);
            instruction.operation = static_cast<std::uint8_t>(Op::Param);
            map.span = 2;
            map.peak = 1;
            map.groups = 1;
        }
        else if (services_ && source.op == Op::Load && pc + 2 < program_.code.size() && !hasLabel(pc + 1) && !hasLabel(pc + 2) && program_.code[pc + 1].op == Op::LoadAt && !program_.code[pc + 1].isFloat && program_.code[pc + 2].op == Op::Store)
        {
            if (!claim(pc, false) || !claim(pc + 2, false))
            {
                indices.emplace(pc, absent);
                continue;
            }
            instruction.kind = Kind::Service;
            instruction.left = bind(pc, false);
            instruction.operation = static_cast<std::uint8_t>(Op::LoadAt);
            map.span = 3;
            map.peak = 1;
            map.groups = 1;
            instruction.flags |= Store;
            instruction.destination = bind(pc + 2, false);
            instruction.destinationType = slotType(pc + 2, floating);
        }
        else if (services_ && source.op == Op::Load && pc + 2 < program_.code.size() && !hasLabel(pc + 1) && !hasLabel(pc + 2) && (program_.code[pc + 1].op == Op::Load || program_.code[pc + 1].op == Op::PushInt) && ((program_.code[pc + 2].op == Op::StoreAt && !program_.code[pc + 2].isFloat) || program_.code[pc + 2].op == Op::Check || program_.code[pc + 2].op == Op::CheckPointer))
        {
            if (!claim(pc, false) || (program_.code[pc + 1].op != Op::PushInt && !claim(pc + 1, false)))
            {
                indices.emplace(pc, absent);
                continue;
            }
            instruction.kind = Kind::Service;
            instruction.left = bind(pc, false);
            instruction.operation = static_cast<std::uint8_t>(program_.code[pc + 2].op);
            map.span = 3;
            map.peak = 2;
            map.groups = 1;
            if (program_.code[pc + 1].op == Op::PushInt)
                instruction.flags |= Literal;
            else
                instruction.right = bind(pc + 1, false);
            instruction.immediate = program_.code[pc + 1].intValue;
        }
        else if (source.op == Op::Jmp && plan_.code[pc].target != absent)
        {
            instruction.kind = Kind::Jump;
            instruction.flags |= End;
            map.target = plan_.code[pc].target;
        }
        else if (pc + 1 < program_.code.size() && !hasLabel(pc + 1) && (source.op == Op::Load || source.op == Op::PushInt || source.op == Op::PushDouble) && program_.code[pc + 1].op == Op::Store)
        {
            // 浮点拷贝（含浮点立即数）与整数拷贝走同一条 64 位搬运，槽的形态写进绑定表。
            const bool sourceReal = source.op == Op::PushDouble || (source.op == Op::Load && realSlot(pc));
            /* 目标不是 64 位浮点（真有 float32 时）得按 IEEE-754 收单精度，
             * 通用处理器只按整数回绕，这种拷贝交还精确执行器。先判再绑，免得留下半条指令。 */
            if (sourceReal && (slotType(pc + 1, floating) & 127) != 0)
            {
                indices.emplace(pc, absent);
                continue;
            }
            if ((source.op == Op::Load && !claim(pc, sourceReal)) || !claim(pc + 1, sourceReal))
            {
                indices.emplace(pc, absent);
                continue;
            }
            instruction.kind = Kind::Copy;
            map.span = 2;
            map.peak = 1;
            instruction.flags |= Store;
            if (source.op == Op::PushInt)
            {
                instruction.flags |= Literal;
                instruction.immediate = source.intValue;
            }
            else if (source.op == Op::PushDouble)
            {
                instruction.flags |= Literal | Float;
                instruction.immediate = realBits(source.doubleValue);
            }
            else
            {
                if (sourceReal) instruction.flags |= Float;
                instruction.left = bind(pc, sourceReal);
            }
            instruction.destination = bind(pc + 1, sourceReal);
            instruction.destinationType = slotType(pc + 1, floating);
        }
        else if (pc + 1 < program_.code.size() && !hasLabel(pc + 1) && source.op == Op::Load && (program_.code[pc + 1].op == Op::Jz || program_.code[pc + 1].op == Op::Jnz) && plan_.code[pc + 1].target != absent)
        {
            if (!claim(pc, false))
            {
                indices.emplace(pc, absent);
                continue;
            }
            instruction.kind = Kind::Branch;
            map.span = 2;
            map.peak = 1;
            instruction.flags |= Branch | End;
            instruction.left = bind(pc, false);
            if (program_.code[pc + 1].op == Op::Jz) instruction.flags |= Zero;
            map.target = plan_.code[pc + 1].target;
        }
        else
        {
            indices.emplace(pc, absent);
            continue;
        }
        // 目标是浮点槽却没走浮点窗口（隐式转换之类）时交还精确执行器。
        if (floating && !(instruction.flags & Float))
        {
            indices.emplace(pc, absent);
            continue;
        }
        const auto copy = pc + map.span;
        if (instruction.kind == Kind::Arithmetic && (instruction.flags & Store) && !(instruction.flags & Branch) && copy + 1 < program_.code.size() && program_.code[copy].op == Op::Load && program_.code[copy + 1].op == Op::Store && !hasLabel(copy) && !hasLabel(copy + 1) && plan_.code[copy].symbol == plan_.code[copy - 1].symbol)
        {
            bool copyFloating = false;
            const auto type = slotType(copy + 1, copyFloating);
            const bool copyReal = (instruction.flags & Float) && !(instruction.flags & BoolResult);
            /* 顺带写回的槽必须和结果的形态一致：整数结果不写浮点槽，反之亦然。 */
            if (copyFloating == copyReal && claim(copy + 1, copyReal))
            {
                instruction.flags |= CopyResult;
                instruction.copyDestination = bind(copy + 1, copyReal);
                instruction.copyType = type;
                map.span += 2;
            }
        }
        map.next = pc + map.span;
        if (foldJumps_ && instruction.kind == Kind::Arithmetic && !(instruction.flags & Branch) && map.next < program_.code.size() && program_.code[map.next].op == Op::Jmp && !hasLabel(map.next) && plan_.code[map.next].target != absent)
        {
            instruction.flags |= End;
            ++map.span;
            map.next = plan_.code[map.next].target;
        }
        if (foldJumps_)
        {
            if (instruction.kind != Kind::Jump) map.next = followJumps(map.next, map.nextSteps);
            if (instruction.kind == Kind::Jump || (instruction.flags & Branch)) map.target = followJumps(map.target, map.targetSteps);
        }
        if (map.nextSteps) instruction.flags |= End;
        if (specialize_) specialize(instruction);
        indices.emplace(pc, result.code.size());
        result.code.push_back(instruction);
        result.mapping.push_back(map);
        // 顺序后继先展开，使常见直线块的热指令连续。
        if (instruction.kind == Kind::Jump || (instruction.flags & Branch)) work.push_back(map.target);
        if (instruction.kind != Kind::Jump) work.push_back(map.next);
    }
    for (std::size_t index = 0; index < result.code.size(); ++index)
    {
        auto &instruction = result.code[index];
        const auto &map = result.mapping[index];
        const auto next = indices.find(map.next), target = indices.find(map.target);
        if (next != indices.end() && next->second != absent) instruction.nextIndex = static_cast<std::uint16_t>(next->second);
        if (target != indices.end() && target->second != absent) instruction.targetIndex = static_cast<std::uint16_t>(target->second);
    }
    for (std::size_t entryIndex = 0; entryIndex < result.code.size(); ++entryIndex)
    {
        Block block;
        auto index = static_cast<std::uint16_t>(entryIndex);
        while (index != noIndex && block.count < 16)
        {
            const auto &instruction = result.code[index];
            const auto &map = result.mapping[index];
            if (block.count && mayTrap(instruction)) break;
            ++block.count;
            block.last = index;
            block.steps += map.span;
            block.groups += map.groups;
            block.peak = std::max(block.peak, map.peak);
            if ((instruction.flags & End) || mayTrap(instruction) || instruction.nextIndex != index + 1) break;
            index = instruction.nextIndex;
        }
        const auto &last = result.mapping[block.last];
        block.steps += std::max(last.nextSteps, last.targetSteps);
        result.blocks.push_back(block);
    }
    auto inserted = regions_.emplace(entry, std::move(owned));
    return *inserted.first->second;
}

/**
 * @brief 执行最多 4096 次分派，复杂边界交还精确执行器。
 * @param region 区域。
 * @param slots 整数地址。
 * @param entry 入口。
 * @param budget 步数预算。
 * @param capacity 栈容量。
 * @param batch 是否批量检查。
 * @param threaded 是否标签地址分派。
 * @return 本批进度。
 */
Progress Engine::execute(const Region &region, const Memory::Vector<std::int64_t *> &slots,
                         std::size_t entry, std::size_t budget, std::size_t capacity, bool batch, bool threaded)

{
    const auto unavailable = [](Op, std::size_t, std::int64_t, std::int64_t, std::int64_t &) { return false; };
    return executeWithServices(region, slots, entry, budget, capacity, batch, threaded, unavailable);
}

/**
 * @brief 选择常见整数操作的专用处理器。
 * @param instruction 待降低指令。
 */
void Engine::specialize(Instruction &instruction)

{
    const bool wide = !(instruction.destinationType & 127) && !(instruction.copyType & 127) && !(instruction.operationType & 127);
    const bool real = (instruction.flags & Float) != 0;
    if (instruction.kind == Kind::Jump)
    {
        instruction.opcode = Opcode::Jump;
        return;
    }
    if (instruction.kind == Kind::Branch)
    {
        instruction.opcode = Opcode::Branch;
        return;
    }
    if (instruction.kind == Kind::Copy && wide)
    {
        if (real)
            instruction.opcode = (instruction.flags & Literal) ? Opcode::F64CopyImm : Opcode::F64Copy;
        else
            instruction.opcode = (instruction.flags & Literal) ? Opcode::CopyImm64 : Opcode::Copy64;
        return;
    }
    if (instruction.kind != Kind::Arithmetic || !wide || !(instruction.flags & Store)) return;
    const auto operation = static_cast<Op>(instruction.operation);
    if (instruction.flags & Branch)
    {
        if (operation == Op::Lt)
            instruction.opcode = real ? ((instruction.flags & Literal) ? Opcode::F64LtBranchImm : Opcode::F64LtBranchRR) :
                                        ((instruction.flags & Literal) ? Opcode::Lt64BranchImm : Opcode::Lt64BranchRR);
        return;
    }
    /* 浮点只专用加、减、乘：除法可能除零（要交还精确执行器报「浮点除零」），
     * 取模要走 `fmod`，两者都留在通用处理器上；宽位（float32）归一化同理。 */
    if (real)
    {
#define CHALI_REGISTER_REAL_SELECT(name, op)                                                                     \
    if (operation == op)                                                                                         \
    {                                                                                                            \
        instruction.opcode = (instruction.flags & CopyResult) ?                                                  \
                                 ((instruction.flags & Literal) ? Opcode::name##CopyImm : Opcode::name##CopyRR) : \
                                 ((instruction.flags & Literal) ? Opcode::name##Imm : Opcode::name##RR);         \
        return;                                                                                                  \
    }
        CHALI_REGISTER_REAL_SELECT(F64Add, Op::Add)
        CHALI_REGISTER_REAL_SELECT(F64Sub, Op::Sub)
        CHALI_REGISTER_REAL_SELECT(F64Mul, Op::Mul)
#undef CHALI_REGISTER_REAL_SELECT
        return;
    }
#define CHALI_REGISTER_SELECT(name, op)                                                                           \
    if (operation == op)                                                                                          \
    {                                                                                                             \
        instruction.opcode = (instruction.flags & CopyResult) ?                                                   \
                                 ((instruction.flags & Literal) ? Opcode::name##CopyImm : Opcode::name##CopyRR) : \
                                 ((instruction.flags & Literal) ? Opcode::name##Imm : Opcode::name##RR);          \
        return;                                                                                                   \
    }
    CHALI_REGISTER_SELECT(Add64, Op::Add)
    CHALI_REGISTER_SELECT(Sub64, Op::Sub)
    CHALI_REGISTER_SELECT(Mul64, Op::Mul)
#undef CHALI_REGISTER_SELECT
}

/**
 * @brief 识别可能失败的算术或服务，隔离为单指令块。
 * @param instruction 指令。
 * @return 是否可能陷阱。
 */
bool Engine::mayTrap(const Instruction &instruction)

{
    return instruction.kind == Kind::Service || (instruction.kind == Kind::Arithmetic && (instruction.operation == static_cast<std::uint8_t>(Op::Div) || instruction.operation == static_cast<std::uint8_t>(Op::Mod)));
}

/**
 * @brief 跟随最多 32 条跳转，遇环则保留原路径。
 * @param entry 起始 PC。
 * @param steps 折叠步数。
 * @return 最终 PC。
 */
std::size_t Engine::followJumps(std::size_t entry, std::uint8_t &steps) const

{
    std::size_t visited[32];
    auto pc = entry;
    steps = 0;
    while (steps < 32 && pc < program_.code.size() && program_.code[pc].op == Op::Jmp && plan_.code[pc].target != absent)
    {
        for (std::uint8_t i = 0; i < steps; ++i)
            if (visited[i] == pc)
            {
                steps = 0;
                return entry;
            }
        visited[steps++] = pc;
        pc = plan_.code[pc].target;
    }
    for (std::uint8_t i = 0; i < steps; ++i)
        if (visited[i] == pc)
        {
            steps = 0;
            return entry;
        }
    return pc;
}

/**
 * @brief 识别扩展整数操作。
 * @param op 操作码。
 * @return 是否支持。
 */
bool Engine::extendedInteger(Op op)

{
    return op == Op::Div || op == Op::Mod || op == Op::Shl || op == Op::Shr || op == Op::ShrU || op == Op::BitAnd || op == Op::BitOr || op == Op::BitXor;
}

/**
 * @brief 检查指令的标签入口。
 * @param pc 指令位置。
 * @return 是否有标签。
 */
bool Engine::hasLabel(std::size_t pc) const

{
    for (const auto &label : program_.labels)
        if (label.second == pc) return true;
    return false;
}
}  // namespace register_core
}  // namespace stackvm
