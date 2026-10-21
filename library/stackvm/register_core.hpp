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
    Engine(const Program &program, const execution_detail::ExecutionPlan &plan, bool foldJumps = true, bool specialize = true, bool services = true);
    /**
     * @brief 获取最多 256 条内部指令的区域。
     * @param entry 原始入口。
     * @return 缓存区域。
     */
    const Region &region(std::size_t entry);
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
                            std::size_t entry, std::size_t budget, std::size_t capacity, bool batch = true, bool threaded = false);
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
     */
    static void specialize(Instruction &instruction);
    /**
     * @brief 识别可能失败的算术或服务，隔离为单指令块。
     * @param instruction 指令。
     * @return 是否可能陷阱。
     */
    static bool mayTrap(const Instruction &instruction);
    /**
     * @brief 跟随最多 32 条跳转，遇环则保留原路径。
     * @param entry 起始 PC。
     * @param steps 折叠步数。
     * @return 最终 PC。
     */
    std::size_t followJumps(std::size_t entry, std::uint8_t &steps) const;
    /**
     * @brief 识别扩展整数操作。
     * @param op 操作码。
     * @return 是否支持。
     */
    static bool extendedInteger(Op op);
    /**
     * @brief 检查指令的标签入口。
     * @param pc 指令位置。
     * @return 是否有标签。
     */
    bool hasLabel(std::size_t pc) const;
    const Program &program_;
    const execution_detail::ExecutionPlan &plan_;
    bool foldJumps_ = true, specialize_ = true, services_ = true;
    Memory::Map<std::size_t, Memory::UniquePtr<Region>> regions_;
};
#undef MAGEL_REGISTER_OPS
}  // namespace register_core
}  // namespace stackvm
