#pragma once
/**
 * @file register_core.hpp
 * @brief 独立整数核心；紧凑指令、跳转折叠及基本块执行。
 */
namespace stackvm
{
namespace register_core
{
using execution_detail::absent;
constexpr std::uint16_t noIndex = 65535;
/**
 * @brief 核心指令种类。
 */
enum class Kind : std::uint8_t
{
    Arithmetic,
    Copy,
    Jump,
    Branch,
    Service
};
/**
 * @brief 降低阶段确定的热指令标志。
 */
enum Flag : std::uint8_t
{
    Literal = 1,
    Store = 2,
    Branch = 4,
    Zero = 8,
    CopyResult = 16,
    End = 32
};
#define MAGEL_REGISTER_OPS(X) \
    X(Generic)                \
    X(Copy64)                 \
    X(CopyImm64)              \
    X(Jump)                   \
    X(Branch)                 \
    X(Add64RR)                \
    X(Add64Imm)               \
    X(Add64CopyRR)            \
    X(Add64CopyImm)           \
    X(Sub64RR)                \
    X(Sub64Imm)               \
    X(Sub64CopyRR)            \
    X(Sub64CopyImm)           \
    X(Mul64RR)                \
    X(Mul64Imm)               \
    X(Mul64CopyRR)            \
    X(Mul64CopyImm)           \
    X(Lt64BranchRR)           \
    X(Lt64BranchImm)
/**
 * @brief 常见整数操作的专用操作码。
 */
enum class Opcode : std::uint8_t
{
#define MAGEL_REGISTER_ENUM(name) name,
    MAGEL_REGISTER_OPS(MAGEL_REGISTER_ENUM)
#undef MAGEL_REGISTER_ENUM
};
/**
 * @brief 热指令，仅保存操作数、紧凑类型编码及区域后继。
 */
struct Instruction
{
    std::int64_t immediate = 0;
    std::uint16_t left = 0, right = 0, destination = 0, copyDestination = 0, nextIndex = noIndex, targetIndex = noIndex;
    Opcode opcode = Opcode::Generic;
    Kind kind = Kind::Jump;
    std::uint8_t operation = static_cast<std::uint8_t>(Op::Add), flags = 0;
    std::uint8_t operationType = 0, destinationType = 0, copyType = 0;
};
static_assert(sizeof(Instruction) <= 32, "寄存器热指令不得超过 32 字节");
static_assert(static_cast<unsigned>(Op::StoreAt) < 256, "基础操作码必须能装入一个字节");
/**
 * @brief 冷映射：原始 PC、连续跨度及折叠跳转的边成本。
 */
struct Mapping
{
    std::size_t pc = 0, next = absent, target = absent;
    std::uint16_t span = 1;
    std::uint8_t nextSteps = 0, targetSteps = 0, peak = 0, groups = 0;
};
/**
 * @brief 直线块摘要；终止边成本取最大值，提交时改为实际成本。
 */
struct Block
{
    std::size_t steps = 0;
    std::uint16_t count = 0, last = 0;
    std::uint8_t peak = 0, groups = 0;
};
/**
 * @brief 无函数调用、无声明的整数区域；服务操作不改变已绑定槽地址。
 */
struct Region
{
    Memory::Vector<Instruction> code;
    Memory::Vector<Mapping> mapping;
    Memory::Vector<Block> blocks;
    Memory::Vector<std::size_t> operands;
};
/**
 * @brief 单批执行进度。
 */
struct Progress
{ std::size_t pc = 0, steps = 0, dispatches = 0, groups = 0, peak = 0, blocks = 0; };
/**
 * @brief 编码整数类型，低七位为位宽，零为 64 位，高位为无符号。
 * @param bits 位宽。
 * @param isUnsigned 是否无符号。
 * @return 紧凑编码。
 */
inline std::uint8_t encode(int bits, bool isUnsigned)
{
    return static_cast<std::uint8_t>((bits <= 0 || bits >= 64 ? 0 : bits) | (isUnsigned ? 128 : 0));
}
/**
 * @brief 按编码回绕整数。
 * @param value 原值。
 * @param type 类型编码。
 * @return 归一化值。
 */
inline std::int64_t normalize(std::int64_t value, std::uint8_t type)
{
    const auto width = type & 127;
    if (!width) return value;
    const auto mask = (std::uint64_t(1) << width) - 1;
    auto bits = static_cast<std::uint64_t>(value) & mask;
    if (!(type & 128) && (bits & (std::uint64_t(1) << (width - 1)))) bits |= ~mask;
    return static_cast<std::int64_t>(bits);
}
/**
 * @brief 专用回绕算术，在编译期选择操作。
 * @tparam operation 加、减或乘。
 * @param a 左值。
 * @param b 右值。
 * @return 64 位回绕结果。
 */
template <Op operation>
inline std::int64_t binary(std::int64_t a, std::int64_t b)
{
    const auto left = static_cast<std::uint64_t>(a), right = static_cast<std::uint64_t>(b);
    if constexpr (operation == Op::Add) return static_cast<std::int64_t>(left + right);
    if constexpr (operation == Op::Sub) return static_cast<std::int64_t>(left - right);
    return static_cast<std::int64_t>(left * right);
}
/**
 * @brief 按入口缓存控制流区域，不缓存调用帧地址。
 */
class Engine
{
public:
    /**
     * @brief 建立核心。
     * @param program 程序。
     * @param plan 执行计划。
     * @param foldJumps 是否合并跳转。
     * @param specialize 是否选择专用操作码。
     * @param services 是否降低运行时服务操作。
     */
    Engine(const Program &program, const execution_detail::ExecutionPlan &plan, bool foldJumps = true, bool specialize = true, bool services = true) : program_(program), plan_(plan), foldJumps_(foldJumps), specialize_(specialize), services_(services) {}
    /**
     * @brief 获取最多 256 条内部指令的区域。
     * @param entry 原始入口。
     * @return 缓存区域。
     */
    const Region &region(std::size_t entry)
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
     * @brief 查询标签地址分派的编译器支持。
     * @return 支持返回真。
     */
    static constexpr bool threadedAvailable()
    {
#if defined(__GNUC__) && !defined(STACKVM_REGISTER_NO_THREADED)
        return true;
#else
        return false;
#endif
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
    static Progress execute(const Region &region, const Memory::Vector<std::int64_t *> &slots,
                            std::size_t entry, std::size_t budget, std::size_t capacity, bool batch = true, bool threaded = false)
    {
        const auto unavailable = [](Op, std::size_t, std::int64_t, std::int64_t, std::int64_t &) { return false; };
        return executeWithServices(region, slots, entry, budget, capacity, batch, threaded, unavailable);
    }
    /**
     * @brief 使用不移动槽位的运行时服务执行区域，服务失败时保留原始操作位置。
     * @tparam Services 服务回调类型。
     * @param region 区域。
     * @param slots 已绑定槽。
     * @param entry 原始入口。
     * @param budget 步数。
     * @param capacity 栈容量。
     * @param batch 是否批量。
     * @param threaded 是否标签地址分派。
     * @param services 接收操作、原始 PC、两个整数和输出的回调。
     * @return 已提交的执行进度。
     */
    template <class Services>
    static Progress executeWithServices(const Region &region, const Memory::Vector<std::int64_t *> &slots,
                                        std::size_t entry, std::size_t budget, std::size_t capacity, bool batch, bool threaded, const Services &services)
    {
#if defined(__GNUC__) && !defined(STACKVM_REGISTER_NO_THREADED)
        if (threaded) return executeImpl<true>(region, slots, entry, budget, capacity, batch, services);
#else
        (void)threaded;
#endif
        return executeImpl<false>(region, slots, entry, budget, capacity, batch, services);
    }

private:
    /**
     * @brief 共用处理器并按模板参数选择分派。
     * @tparam threaded 是否标签地址分派。
     * @tparam Services 服务回调类型。
     * @param region 区域。
     * @param slots 地址。
     * @param entry 入口。
     * @param budget 步数。
     * @param capacity 容量。
     * @param batch 是否批量。
     * @param services 运行时服务。
     * @return 本批进度。
     */
    template <bool threaded, class Services>
    static Progress executeImpl(const Region &region, const Memory::Vector<std::int64_t *> &slots,
                                std::size_t entry, std::size_t budget, std::size_t capacity, bool batch, const Services &services)
    {
        Progress progress;
        progress.pc = entry;
        auto index = region.code.empty() ? noIndex : std::uint16_t(0);
        auto *const *registers = slots.data();
        while (index != noIndex && progress.dispatches < 4096)
        {
            auto block = region.blocks[index];
            if (!batch || block.count > 4096 - progress.dispatches || block.steps > budget - progress.steps || block.peak > capacity)
            {
                const auto &map = region.mapping[index];
                block.count = 1;
                block.last = index;
                block.steps = map.span + std::max(map.nextSteps, map.targetSteps);
                block.groups = map.groups;
                block.peak = map.peak;
                if (block.steps > budget - progress.steps || block.peak > capacity) break;
            }
            auto remaining = block.count;
            bool take = false;
            std::int64_t value = 0;
            const Instruction *instruction = nullptr;
        dispatch:
            instruction = &region.code[index];
            take = false;
#if defined(__GNUC__) && !defined(STACKVM_REGISTER_NO_THREADED)
            if constexpr (threaded)
            {
#define MAGEL_REGISTER_ADDRESS(name) &&handler_##name,
                static const void *const handlers[] = { MAGEL_REGISTER_OPS(MAGEL_REGISTER_ADDRESS) };
#undef MAGEL_REGISTER_ADDRESS
                goto *handlers[static_cast<std::uint8_t>(instruction->opcode)];
            }
#define MAGEL_REGISTER_CASE(name) \
    case Opcode::name: handler_##name : __attribute__((unused));
#else
#define MAGEL_REGISTER_CASE(name) case Opcode::name:
#endif
            switch (instruction->opcode)
            {
                MAGEL_REGISTER_CASE(Generic)
                {
                    if (instruction->kind == Kind::Service)
                    {
                        const auto a = *registers[instruction->left];
                        const auto b = (instruction->flags & Literal) ? instruction->immediate : *registers[instruction->right];
                        if (!services(static_cast<Op>(instruction->operation), region.mapping[index].pc, a, b, value)) goto finished;
                    }
                    else if (instruction->kind == Kind::Arithmetic)
                    {
                        const auto a = *registers[instruction->left];
                        const auto b = (instruction->flags & Literal) ? instruction->immediate : *registers[instruction->right];
                        const auto operation = static_cast<Op>(instruction->operation);
                        if ((operation == Op::Div || operation == Op::Mod) && !b) goto finished;
                        switch (operation)
                        {
                        case Op::Add: value = binary<Op::Add>(a, b); break;
                        case Op::Sub: value = binary<Op::Sub>(a, b); break;
                        case Op::Mul: value = binary<Op::Mul>(a, b); break;
                        case Op::Div: value = (instruction->operationType & 128) ? static_cast<std::int64_t>(static_cast<std::uint64_t>(a) / static_cast<std::uint64_t>(b)) : (a == INT64_MIN && b == -1 ? a : a / b); break;
                        case Op::Mod: value = (instruction->operationType & 128) ? static_cast<std::int64_t>(static_cast<std::uint64_t>(a) % static_cast<std::uint64_t>(b)) : (a == INT64_MIN && b == -1 ? 0 : a % b); break;
                        case Op::BitAnd: value = a & b; break;
                        case Op::BitOr: value = a | b; break;
                        case Op::BitXor: value = a ^ b; break;
                        case Op::Shl: value = static_cast<std::int64_t>(static_cast<std::uint64_t>(a) << (b & 63)); break;
                        case Op::Shr: value = a >> (b & 63); break;
                        case Op::ShrU: value = static_cast<std::int64_t>(static_cast<std::uint64_t>(a) >> (b & 63)); break;
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
                        default: value = 0; break;
                        }
                        if (operation == Op::Add || operation == Op::Sub || operation == Op::Mul || operation == Op::Div || operation == Op::Mod)
                            value = normalize(value, instruction->operationType);
                    }
                    else if (instruction->kind == Kind::Copy)
                        value = (instruction->flags & Literal) ? instruction->immediate : *registers[instruction->left];
                    else if (instruction->kind == Kind::Branch)
                        value = *registers[instruction->left];
                    if (instruction->flags & Store)
                    {
                        value = normalize(value, instruction->destinationType);
                        *registers[instruction->destination] = value;
                    }
                    if (instruction->flags & CopyResult) *registers[instruction->copyDestination] = normalize(value, instruction->copyType);
                    take = instruction->kind == Kind::Jump || ((instruction->flags & Branch) && ((value == 0) == bool(instruction->flags & Zero)));
                    goto nodeDone;
                }
                MAGEL_REGISTER_CASE(Copy64) * registers[instruction->destination] = *registers[instruction->left];
                goto nodeDone;
                MAGEL_REGISTER_CASE(CopyImm64) * registers[instruction->destination] = instruction->immediate;
                goto nodeDone;
                MAGEL_REGISTER_CASE(Jump)
                take = true;
                goto nodeDone;
                MAGEL_REGISTER_CASE(Branch)
                take = (*registers[instruction->left] == 0) == bool(instruction->flags & Zero);
                goto nodeDone;
#define MAGEL_REGISTER_BINARY(name, operation)                                                                               \
    MAGEL_REGISTER_CASE(name##RR)                                                                                            \
    *registers[instruction->destination] = binary<operation>(*registers[instruction->left], *registers[instruction->right]); \
    goto nodeDone;                                                                                                           \
    MAGEL_REGISTER_CASE(name##Imm)                                                                                           \
    *registers[instruction->destination] = binary<operation>(*registers[instruction->left], instruction->immediate);         \
    goto nodeDone;                                                                                                           \
    MAGEL_REGISTER_CASE(name##CopyRR)                                                                                        \
    value = binary<operation>(*registers[instruction->left], *registers[instruction->right]);                                \
    *registers[instruction->destination] = value;                                                                            \
    *registers[instruction->copyDestination] = value;                                                                        \
    goto nodeDone;                                                                                                           \
    MAGEL_REGISTER_CASE(name##CopyImm)                                                                                       \
    value = binary<operation>(*registers[instruction->left], instruction->immediate);                                        \
    *registers[instruction->destination] = value;                                                                            \
    *registers[instruction->copyDestination] = value;                                                                        \
    goto nodeDone;
                MAGEL_REGISTER_BINARY(Add64, Op::Add)
                MAGEL_REGISTER_BINARY(Sub64, Op::Sub)
                MAGEL_REGISTER_BINARY(Mul64, Op::Mul)
#undef MAGEL_REGISTER_BINARY
                MAGEL_REGISTER_CASE(Lt64BranchRR)
                value = *registers[instruction->left] < *registers[instruction->right];
                *registers[instruction->destination] = value;
                take = (value == 0) == bool(instruction->flags & Zero);
                goto nodeDone;
                MAGEL_REGISTER_CASE(Lt64BranchImm)
                value = *registers[instruction->left] < instruction->immediate;
                *registers[instruction->destination] = value;
                take = (value == 0) == bool(instruction->flags & Zero);
                goto nodeDone;
            }
        nodeDone:
            index = take ? instruction->targetIndex : instruction->nextIndex;
            if (--remaining) goto dispatch;
            {
                const auto &map = region.mapping[block.last];
                progress.pc = take ? map.target : map.next;
                progress.steps += block.steps - std::max(map.nextSteps, map.targetSteps) + (take ? map.targetSteps : map.nextSteps);
                progress.dispatches += block.count;
                progress.groups += block.groups;
                if (block.count > 1) ++progress.blocks;
                progress.peak = std::max(progress.peak, static_cast<std::size_t>(block.peak));
            }
        }
    finished:
#undef MAGEL_REGISTER_CASE
        return progress;
    }
    /**
     * @brief 选择常见整数操作的专用处理器。
     * @param instruction 待降低指令。
     * @return 无。
     */
    static void specialize(Instruction &instruction)
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
#define MAGEL_REGISTER_SELECT(name, op)                                                                           \
    if (operation == op)                                                                                          \
    {                                                                                                             \
        instruction.opcode = (instruction.flags & CopyResult) ?                                                   \
                                 ((instruction.flags & Literal) ? Opcode::name##CopyImm : Opcode::name##CopyRR) : \
                                 ((instruction.flags & Literal) ? Opcode::name##Imm : Opcode::name##RR);          \
        return;                                                                                                   \
    }
        MAGEL_REGISTER_SELECT(Add64, Op::Add)
        MAGEL_REGISTER_SELECT(Sub64, Op::Sub)
        MAGEL_REGISTER_SELECT(Mul64, Op::Mul)
#undef MAGEL_REGISTER_SELECT
    }
    /**
     * @brief 识别可能失败的算术或服务，隔离为单指令块。
     * @param instruction 指令。
     * @return 是否可能陷阱。
     */
    static bool mayTrap(const Instruction &instruction)
    {
        return instruction.kind == Kind::Service || (instruction.kind == Kind::Arithmetic && (instruction.operation == static_cast<std::uint8_t>(Op::Div) || instruction.operation == static_cast<std::uint8_t>(Op::Mod)));
    }
    /**
     * @brief 跟随最多 32 条跳转，遇环则保留原路径。
     * @param entry 起始 PC。
     * @param steps 折叠步数。
     * @return 最终 PC。
     */
    std::size_t followJumps(std::size_t entry, std::uint8_t &steps) const
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
    static bool extendedInteger(Op op)
    {
        return op == Op::Div || op == Op::Mod || op == Op::Shl || op == Op::Shr || op == Op::ShrU || op == Op::BitAnd || op == Op::BitOr || op == Op::BitXor;
    }
    /**
     * @brief 检查指令的标签入口。
     * @param pc 指令位置。
     * @return 是否有标签。
     */
    bool hasLabel(std::size_t pc) const
    {
        for (const auto &label : program_.labels)
            if (label.second == pc) return true;
        return false;
    }
    const Program &program_;
    const execution_detail::ExecutionPlan &plan_;
    bool foldJumps_ = true, specialize_ = true, services_ = true;
    Memory::Map<std::size_t, Memory::UniquePtr<Region>> regions_;
};
#undef MAGEL_REGISTER_OPS
}  // namespace register_core
}  // namespace stackvm
