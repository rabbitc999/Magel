/**
 * @file instruction_select.cpp
 * @brief 模式表驱动的指令选择实现。
 */
#include "instruction_select.hpp"
#include "runtime_internal.hpp"

namespace stackvm
{
namespace selection
{
using namespace runtimeDetail;
namespace
{
constexpr std::size_t absent = static_cast<std::size_t>(-1);

/**
 * @brief 进程内累计统计，只用于 `--stats`。
 * @return 统计引用。
 */
Stats &accumulated()
{
    static Stats stats;
    return stats;
}

/**
 * @brief 是否是可按整数二元窗口融合的算术 / 位运算。
 * @param op 操作码。
 * @return 是返回 true。
 */
bool integerBinary(Op op)
{
    switch (op)
    {
    case Op::Add:
    case Op::Sub:
    case Op::Mul:
    case Op::Div:
    case Op::Mod:
    case Op::BitAnd:
    case Op::BitOr:
    case Op::BitXor:
    case Op::Shl:
    case Op::Shr:
    case Op::ShrU: return true;
    default: return false;
    }
}

/**
 * @brief 是否是比较运算（十个）。
 * @param op 操作码。
 * @return 是比较返回 true。
 */
bool comparison(Op op)
{
    switch (op)
    {
    case Op::Eq:
    case Op::Ne:
    case Op::Lt:
    case Op::Le:
    case Op::Gt:
    case Op::Ge:
    case Op::LtU:
    case Op::LeU:
    case Op::GtU:
    case Op::GeU: return true;
    default: return false;
    }
}

/**
 * @brief 是否可融合的一元运算。
 * @param op 操作码。
 * @return 是返回 true。
 */
bool unary(Op op)
{
    return op == Op::Neg || op == Op::Not;
}

/**
 * @brief 是否是双结果乘除。
 * @param op 操作码。
 * @return 是返回 true。
 */
bool wide(Op op)
{
    return op == Op::MulWide || op == Op::DivWide;
}

/**
 * @brief 窗口匹配器：只在能证明等价时才给出目标指令。
 */
class Matcher
{
public:
    /**
     * @brief 建立匹配器。
     * @param program 栈程序。
     */
    explicit Matcher(const Program &program) : program_(program), entry_(program.code.size(), false)
    {
        for (const auto &label : program.labels)
            if (label.second < entry_.size()) entry_[label.second] = true;
    }

    /**
     * @brief 在指定 PC 上按模式表选择一条目标指令。
     * @param pc 起始 PC。
     * @param out 选中的目标指令。
     * @return 选中返回 true。
     */
    bool match(std::size_t pc, execution_detail::RegisterInstruction &out) const
    {
        return matchWide(pc, out) || matchCompareBranch(pc, out) || matchBranch(pc, out) || matchBinary(pc, out) || matchUnary(pc, out) || matchCopy(pc, out);
    }

private:
    /**
     * @brief 窗口内部是否没有任何标签入口。
     * @param first 窗口首个 PC。
     * @param last 窗口最后一个 PC（含）。
     * @return 干净返回 true。
     */
    bool clear(std::size_t first, std::size_t last) const
    {
        for (std::size_t pc = first + 1u; pc <= last && pc < entry_.size(); ++pc)
            if (entry_[pc]) return false;
        return true;
    }

    /**
     * @brief 取跳转目标。
     * @param pc 跳转指令位置。
     * @return 目标 PC；标签不存在返回 absent。
     */
    std::size_t targetOf(std::size_t pc) const
    {
        const auto found = program_.labels.find(program_.code[pc].name);
        return found == program_.labels.end() ? absent : found->second;
    }

    /**
     * @brief 是否是可压栈的操作数（变量或立即数）。
     * @param pc 指令位置。
     * @return 是返回 true。
     */
    bool operand(std::size_t pc) const
    {
        const Op op = program_.code[pc].op;
        return op == Op::Load || op == Op::PushInt || op == Op::PushDouble;
    }

    /**
     * @brief 是否是变量装载。
     * @param pc 指令位置。
     * @return 是返回 true。
     */
    bool loaded(std::size_t pc) const { return program_.code[pc].op == Op::Load; }

    /**
     * @brief 是否是条件跳转。
     * @param pc 指令位置。
     * @return 是返回 true。
     */
    bool conditional(std::size_t pc) const
    {
        const Op op = program_.code[pc].op;
        return op == Op::Jz || op == Op::Jnz;
    }

    /**
     * @brief 按操作数填充通用字段。
     * @param out 目标指令。
     * @param pc 窗口起点。
     * @param right 右操作数位置。
     * @param value 运算指令。
     */
    void operands(execution_detail::RegisterInstruction &out, std::size_t pc, std::size_t right, std::size_t value) const
    {
        out.left = pc;
        out.right = right;
        out.operation = program_.code[value].op;
        out.bits = static_cast<int>(program_.code[value].intValue);
        out.unsignedResult = program_.code[value].unsignedOp;
        out.literal = program_.code[right].op == Op::PushInt || program_.code[right].op == Op::PushDouble;
        out.floatingLiteral = program_.code[right].op == Op::PushDouble;
        out.realImmediate = program_.code[right].doubleValue;
        out.immediate = program_.code[right].intValue;
        out.floatOperation = program_.code[value].isFloat;
    }

    /**
     * @brief 双结果乘除：`Load a; Load|PushInt b; mul|div; Store hi; Store lo`。
     * @param pc 窗口起点。
     * @param out 目标指令。
     * @return 匹配返回 true。
     */
    bool matchWide(std::size_t pc, execution_detail::RegisterInstruction &out) const
    {
        const std::size_t count = program_.code.size();
        if (pc + 4u >= count) return false;
        const Op first = program_.code[pc].op, second = program_.code[pc + 1u].op, value = program_.code[pc + 2u].op;
        if (first != Op::Load || (second != Op::Load && second != Op::PushInt)) return false;
        if (!wide(value) || program_.code[pc + 3u].op != Op::Store || program_.code[pc + 4u].op != Op::Store) return false;
        if (!clear(pc, pc + 4u)) return false;
        out = execution_detail::RegisterInstruction{};
        out.kind = execution_detail::RegisterOp::Wide;
        operands(out, pc, pc + 1u, pc + 2u);
        out.destination = pc + 3u;   ///< 先写 hi / r。
        out.destination2 = pc + 4u;  ///< 再写 lo / q。
        out.span = 5u;
        return true;
    }

    /**
     * @brief 比较加分支：`Load a; 操作数; cmp; Store t; Load t; Jz|Jnz L`。
     * @param pc 窗口起点。
     * @param out 目标指令。
     * @return 匹配返回 true。
     */
    bool matchCompareBranch(std::size_t pc, execution_detail::RegisterInstruction &out) const
    {
        const std::size_t count = program_.code.size();
        if (pc + 5u >= count) return false;
        if (!loaded(pc) || !operand(pc + 1u) || !comparison(program_.code[pc + 2u].op)) return false;
        if (program_.code[pc + 3u].op != Op::Store || !loaded(pc + 4u) || !conditional(pc + 5u)) return false;
        if (program_.code[pc + 4u].name != program_.code[pc + 3u].name) return false;
        const std::size_t target = targetOf(pc + 5u);
        if (target == absent || !clear(pc, pc + 5u)) return false;
        out = execution_detail::RegisterInstruction{};
        out.kind = execution_detail::RegisterOp::IntegerBranch;
        operands(out, pc, pc + 1u, pc + 2u);
        out.destination = pc + 3u;
        out.target = target;
        out.jumpOnZero = program_.code[pc + 5u].op == Op::Jz;
        out.storeResult = true;
        out.span = 6u;
        return true;
    }

    /**
     * @brief 比较直接接分支：`Load a; 操作数; cmp; Jz|Jnz L`。
     * @param pc 窗口起点。
     * @param out 目标指令。
     * @return 匹配返回 true。
     */
    bool matchBranch(std::size_t pc, execution_detail::RegisterInstruction &out) const
    {
        const std::size_t count = program_.code.size();
        if (pc + 3u >= count) return false;
        if (!loaded(pc) || !operand(pc + 1u) || !comparison(program_.code[pc + 2u].op) || !conditional(pc + 3u)) return false;
        const std::size_t target = targetOf(pc + 3u);
        if (target == absent || !clear(pc, pc + 3u)) return false;
        out = execution_detail::RegisterInstruction{};
        out.kind = execution_detail::RegisterOp::IntegerBranch;
        operands(out, pc, pc + 1u, pc + 2u);
        out.target = target;
        out.jumpOnZero = program_.code[pc + 3u].op == Op::Jz;
        out.storeResult = false;
        out.span = 4u;
        return true;
    }

    /**
     * @brief 二元运算写回：`Load a; 操作数; op; Store t`，可再带一次"写完立刻重装再写"。
     * @param pc 窗口起点。
     * @param out 目标指令。
     * @return 匹配返回 true。
     */
    bool matchBinary(std::size_t pc, execution_detail::RegisterInstruction &out) const
    {
        const std::size_t count = program_.code.size();
        if (pc + 3u >= count) return false;
        const Op value = program_.code[pc + 2u].op;
        if (!loaded(pc) || !operand(pc + 1u)) return false;
        if (!integerBinary(value) && !comparison(value)) return false;
        if (program_.code[pc + 3u].op != Op::Store || !clear(pc, pc + 3u)) return false;
        out = execution_detail::RegisterInstruction{};
        out.kind = comparison(value) ? execution_detail::RegisterOp::IntegerStore :
                                       (program_.code[pc + 2u].isFloat ? execution_detail::RegisterOp::FloatStore : execution_detail::RegisterOp::IntegerStore);
        operands(out, pc, pc + 1u, pc + 2u);
        out.destination = pc + 3u;
        out.storeResult = true;
        out.span = 4u;
        // 比较结果常被立刻重装用于跳转或复用；跟着一段 `Load t; Store u` 时一并写完。
        if (comparison(value) && pc + 5u < count && loaded(pc + 4u) && program_.code[pc + 5u].op == Op::Store && program_.code[pc + 4u].name == program_.code[pc + 3u].name && clear(pc + 4u, pc + 5u))
        {
            out.kind = execution_detail::RegisterOp::IntegerStore;
            out.destination2 = pc + 5u;
            out.span = 6u;
        }
        return true;
    }

    /**
     * @brief 一元运算写回：`Load a; neg|~; Store t`。
     * @param pc 窗口起点。
     * @param out 目标指令。
     * @return 匹配返回 true。
     */
    bool matchUnary(std::size_t pc, execution_detail::RegisterInstruction &out) const
    {
        const std::size_t count = program_.code.size();
        if (pc + 2u >= count) return false;
        if (!operand(pc) || !unary(program_.code[pc + 1u].op) || program_.code[pc + 2u].op != Op::Store) return false;
        if (!clear(pc, pc + 2u)) return false;
        out = execution_detail::RegisterInstruction{};
        out.kind = execution_detail::RegisterOp::Unary;
        operands(out, pc, pc, pc + 1u);
        out.destination = pc + 2u;
        out.span = 3u;
        return true;
    }

    /**
     * @brief 纯拷贝：`Load|Push 立即数; Store t`。
     * @param pc 窗口起点。
     * @param out 目标指令。
     * @return 匹配返回 true。
     */
    bool matchCopy(std::size_t pc, execution_detail::RegisterInstruction &out) const
    {
        const std::size_t count = program_.code.size();
        if (pc + 1u >= count) return false;
        if (!operand(pc) || program_.code[pc + 1u].op != Op::Store) return false;
        if (!clear(pc, pc + 1u)) return false;
        out = execution_detail::RegisterInstruction{};
        out.kind = execution_detail::RegisterOp::Copy;
        out.left = pc;
        out.operation = program_.code[pc].op;
        out.literal = program_.code[pc].op != Op::Load;
        out.floatingLiteral = program_.code[pc].op == Op::PushDouble;
        out.immediate = program_.code[pc].intValue;
        out.realImmediate = program_.code[pc].doubleValue;
        out.destination = pc + 1u;
        out.span = 2u;
        return true;
    }

    const Program &program_;      ///< 栈程序。
    Memory::Vector<bool> entry_;  ///< 有标签入口的 PC。
};
}  // namespace

Memory::Vector<execution_detail::RegisterInstruction> lower(const Program &program, Stats *stats)
{
    Memory::Vector<execution_detail::RegisterInstruction> result;
    const Matcher matcher(program);
    const std::size_t count = program.code.size();
    result.reserve(count / 2u + 1u);
    Stats local;
    for (std::size_t pc = 0; pc < count;)
    {
        execution_detail::RegisterInstruction instruction;
        if (!matcher.match(pc, instruction))
        {
            ++pc;
            continue;
        }
        ++local.instructions;
        local.covered += instruction.span;
        const auto kind = static_cast<std::size_t>(instruction.kind);
        if (kind < 7u) ++local.byKind[kind];
        const auto operation = static_cast<std::size_t>(instruction.operation);
        if (operation < 64u) ++local.byOperation[operation];
        result.push_back(instruction);
        pc += instruction.span;
    }
    local.total = count;
    Stats &total = accumulated();
    total.instructions += local.instructions;
    total.covered += local.covered;
    total.total += local.total;
    for (std::size_t kind = 0; kind < 7u; ++kind) total.byKind[kind] += local.byKind[kind];
    for (std::size_t operation = 0; operation < 64u; ++operation) total.byOperation[operation] += local.byOperation[operation];
    if (stats != nullptr) *stats = local;
    return result;
}

Stats lastStats() { return accumulated(); }

void resetStats() { accumulated() = Stats{}; }

Memory::String describe(const Stats &stats)
{
    const std::size_t permille = stats.total == 0u ? 0u : (stats.covered * 1000u + stats.total / 2u) / stats.total;
    Memory::String text = "指令选择：目标指令 " + Memory::toString(stats.instructions) + " 条，覆盖栈指令 " + Memory::toString(stats.covered) + "/" + Memory::toString(stats.total) + "（" + Memory::toString(permille / 10u) + "." + Memory::toString(permille % 10u) + "%，通用回退 " + Memory::toString(stats.total - stats.covered) + " 条）";
    return text;
}
}  // namespace selection
}  // namespace stackvm
