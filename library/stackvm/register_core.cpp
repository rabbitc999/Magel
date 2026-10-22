/**
 * @file register_core.cpp
 * @brief 寄存器执行区域的准备、专用化与缓存。
 */
#include "runtime_internal.hpp"

namespace stackvm
{
using namespace runtimeDetail;
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
    Memory::Map<std::size_t, std::uint16_t> registers;
    Memory::Vector<std::size_t> work{ entry };
    const auto bind = [&](std::size_t pc) {
        const auto symbol = plan_.code[pc].symbol;
        const auto found = registers.find(symbol);
        if (found != registers.end()) return found->second;
        const auto index = static_cast<std::uint16_t>(result.operands.size());
        registers.emplace(symbol, index);
        result.operands.push_back(pc);
        return index;
    };
    const auto slotType = [&](std::size_t pc, bool &floating) {
        const auto *type = plan_.types[plan_.code[pc].symbol].type;
        floating = type && type->isFloat;
        return type ? encode(type->bits, type->isUnsigned) : std::uint8_t(0);
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
            if (internal.floatingLiteral)
            {
                indices.emplace(pc, absent);
                continue;
            }
            instruction.kind = Kind::Arithmetic;
            instruction.operation = static_cast<std::uint8_t>(internal.operation);
            map.span = static_cast<std::uint16_t>(internal.span);
            map.peak = 2;
            map.groups = 1;
            instruction.left = bind(internal.left);
            if (internal.literal)
                instruction.flags |= Literal;
            else
                instruction.right = bind(internal.right);
            instruction.immediate = internal.immediate;
            if (internal.kind == execution_detail::RegisterOp::IntegerBranch) instruction.flags |= Branch | End;
            if (internal.jumpOnZero) instruction.flags |= Zero;
            map.target = internal.target;
            instruction.operationType = encode(internal.bits, internal.unsignedResult);
            if (internal.storeResult)
            {
                instruction.flags |= Store;
                instruction.destination = bind(internal.destination);
                instruction.destinationType = slotType(internal.destination, floating);
            }
        }
        else if (pc + 3 < program_.code.size() && source.op == Op::Load && (program_.code[pc + 1].op == Op::Load || program_.code[pc + 1].op == Op::PushInt) && extendedInteger(program_.code[pc + 2].op) && program_.code[pc + 3].op == Op::Store && !hasLabel(pc + 1) && !hasLabel(pc + 2) && !hasLabel(pc + 3))
        {
            instruction.kind = Kind::Arithmetic;
            map.span = 4;
            map.peak = 2;
            map.groups = 1;
            instruction.operation = static_cast<std::uint8_t>(program_.code[pc + 2].op);
            instruction.left = bind(pc);
            if (program_.code[pc + 1].op == Op::PushInt)
                instruction.flags |= Literal;
            else
                instruction.right = bind(pc + 1);
            instruction.immediate = program_.code[pc + 1].intValue;
            instruction.flags |= Store;
            instruction.destination = bind(pc + 3);
            instruction.operationType = encode(static_cast<int>(program_.code[pc + 2].intValue), program_.code[pc + 2].unsignedOp);
            instruction.destinationType = slotType(pc + 3, floating);
        }
        else if (services_ && source.op == Op::Load && pc + 1 < program_.code.size() && !hasLabel(pc + 1) && program_.code[pc + 1].op == Op::Param)
        {
            instruction.kind = Kind::Service;
            instruction.left = bind(pc);
            instruction.operation = static_cast<std::uint8_t>(Op::Param);
            map.span = 2;
            map.peak = 1;
            map.groups = 1;
        }
        else if (services_ && source.op == Op::Load && pc + 2 < program_.code.size() && !hasLabel(pc + 1) && !hasLabel(pc + 2) && program_.code[pc + 1].op == Op::LoadAt && !program_.code[pc + 1].isFloat && program_.code[pc + 2].op == Op::Store)
        {
            instruction.kind = Kind::Service;
            instruction.left = bind(pc);
            instruction.operation = static_cast<std::uint8_t>(Op::LoadAt);
            map.span = 3;
            map.peak = 1;
            map.groups = 1;
            instruction.flags |= Store;
            instruction.destination = bind(pc + 2);
            instruction.destinationType = slotType(pc + 2, floating);
        }
        else if (services_ && source.op == Op::Load && pc + 2 < program_.code.size() && !hasLabel(pc + 1) && !hasLabel(pc + 2) && (program_.code[pc + 1].op == Op::Load || program_.code[pc + 1].op == Op::PushInt) && ((program_.code[pc + 2].op == Op::StoreAt && !program_.code[pc + 2].isFloat) || program_.code[pc + 2].op == Op::Check || program_.code[pc + 2].op == Op::CheckPointer))
        {
            instruction.kind = Kind::Service;
            instruction.left = bind(pc);
            instruction.operation = static_cast<std::uint8_t>(program_.code[pc + 2].op);
            map.span = 3;
            map.peak = 2;
            map.groups = 1;
            if (program_.code[pc + 1].op == Op::PushInt)
                instruction.flags |= Literal;
            else
                instruction.right = bind(pc + 1);
            instruction.immediate = program_.code[pc + 1].intValue;
        }
        else if (source.op == Op::Jmp && plan_.code[pc].target != absent)
        {
            instruction.kind = Kind::Jump;
            instruction.flags |= End;
            map.target = plan_.code[pc].target;
        }
        else if (pc + 1 < program_.code.size() && !hasLabel(pc + 1) && (source.op == Op::Load || source.op == Op::PushInt) && program_.code[pc + 1].op == Op::Store)
        {
            instruction.kind = Kind::Copy;
            map.span = 2;
            map.peak = 1;
            instruction.flags |= Store;
            if (source.op == Op::PushInt)
                instruction.flags |= Literal;
            else
                instruction.left = bind(pc);
            instruction.immediate = source.intValue;
            instruction.destination = bind(pc + 1);
            instruction.destinationType = slotType(pc + 1, floating);
        }
        else if (pc + 1 < program_.code.size() && !hasLabel(pc + 1) && source.op == Op::Load && (program_.code[pc + 1].op == Op::Jz || program_.code[pc + 1].op == Op::Jnz) && plan_.code[pc + 1].target != absent)
        {
            instruction.kind = Kind::Branch;
            map.span = 2;
            map.peak = 1;
            instruction.flags |= Branch | End;
            instruction.left = bind(pc);
            if (program_.code[pc + 1].op == Op::Jz) instruction.flags |= Zero;
            map.target = plan_.code[pc + 1].target;
        }
        else
        {
            indices.emplace(pc, absent);
            continue;
        }
        if (floating)
        {
            indices.emplace(pc, absent);
            continue;
        }
        const auto copy = pc + map.span;
        if (instruction.kind == Kind::Arithmetic && (instruction.flags & Store) && !(instruction.flags & Branch) && copy + 1 < program_.code.size() && program_.code[copy].op == Op::Load && program_.code[copy + 1].op == Op::Store && !hasLabel(copy) && !hasLabel(copy + 1) && plan_.code[copy].symbol == plan_.code[copy - 1].symbol)
        {
            const auto type = slotType(copy + 1, floating);
            if (!floating)
            {
                instruction.flags |= CopyResult;
                instruction.copyDestination = bind(copy + 1);
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
        instruction.opcode = (instruction.flags & Literal) ? Opcode::CopyImm64 : Opcode::Copy64;
        return;
    }
    if (instruction.kind != Kind::Arithmetic || !wide || !(instruction.flags & Store)) return;
    const auto operation = static_cast<Op>(instruction.operation);
    if (instruction.flags & Branch)
    {
        if (operation == Op::Lt) instruction.opcode = (instruction.flags & Literal) ? Opcode::Lt64BranchImm : Opcode::Lt64BranchRR;
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
