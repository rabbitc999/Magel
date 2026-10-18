#pragma once

/**
 * @file execution_plan.hpp
 * @brief 两端共用的执行准备层，不修改字节码或源码指令下标。
 */
namespace stackvm
{
namespace execution_detail
{
/**
 * @brief 未绑定的槽位或指令。
 */
constexpr std::size_t absent = static_cast<std::size_t>(-1);
/**
 * @brief 紧凑虚拟寄存器编号，最大值保留为未绑定标记。
 */
using RegisterIndex = std::uint16_t;
constexpr RegisterIndex absentRegister = static_cast<RegisterIndex>(-1);
/**
 * @brief 函数的稠密局部槽布局；声明是否已执行由调用帧单独记录。
 */
struct FrameLayout
{
    Memory::Map<std::size_t, std::size_t> slots;
};
/**
 * @brief 单条原始指令的运行期绑定，原名仍留在 Program 中供诊断使用。
 */
struct PreparedInstruction
{
    std::size_t symbol = absent, target = absent;
    RegisterIndex local = absentRegister;
    const FrameLayout *owner = nullptr;
    bool shared = false;
    std::size_t internal = absent;
};
/**
 * @brief 程序符号的整数身份与已解析类型。
 */
struct PreparedSymbol
{ const TypeInfo *type = nullptr; };
/**
 * @brief 仅降低内部指令，不分配运行期符号及帧布局。
 * @param program 基础 VM 程序。
 * @param includeFloatLiterals 是否加入仅运行期使用的浮点立即数融合。
 * @return 内部执行序列。
 */
inline Memory::Vector<RegisterInstruction> lowerRegisters(const Program &program, bool includeFloatLiterals = false)
{
    Memory::Vector<RegisterInstruction> registers;
    Memory::Vector<bool> entry(program.code.size(), false);
    for (const auto &label : program.labels)
        if (label.second < entry.size()) entry[label.second] = true;
    const auto target = [&](std::size_t index) {
        const auto found = program.labels.find(program.code[index].name);
        return found == program.labels.end() ? absent : found->second;
    };
    for (std::size_t i = 0; i + 3 < program.code.size(); ++i)
    {
        const auto &a = program.code[i], &b = program.code[i + 1], &op = program.code[i + 2], &store = program.code[i + 3];
        const bool arithmetic = op.op == Op::Add || op.op == Op::Sub || op.op == Op::Mul;
        const bool comparison = op.op == Op::Eq || op.op == Op::Ne || op.op == Op::Lt || op.op == Op::Le || op.op == Op::Gt || op.op == Op::Ge || op.op == Op::LtU || op.op == Op::LeU || op.op == Op::GtU || op.op == Op::GeU;
        const bool operands = a.op == Op::Load && (b.op == Op::Load || b.op == Op::PushInt || (includeFloatLiterals && b.op == Op::PushDouble));
        const bool boundary = !entry[i + 1] && !entry[i + 2] && !entry[i + 3];
        const bool branch = comparison && (store.op == Op::Jz || store.op == Op::Jnz) && target(i + 3) != absent;
        if (operands && boundary && (((arithmetic || comparison) && store.op == Op::Store) || branch))
        {
            registers.emplace_back();
            auto &internal = registers.back();
            internal.kind = branch ? RegisterOp::IntegerBranch : RegisterOp::IntegerStore;
            internal.operation = op.op;
            internal.left = i;
            internal.right = i + 1;
            internal.destination = i + 3;
            internal.target = target(i + 3);
            internal.literal = b.op == Op::PushInt || b.op == Op::PushDouble;
            internal.floatingLiteral = b.op == Op::PushDouble;
            internal.realImmediate = b.doubleValue;
            internal.immediate = b.intValue;
            internal.bits = static_cast<int>(op.intValue);
            internal.unsignedResult = op.unsignedOp;
            internal.jumpOnZero = store.op == Op::Jz;
            internal.storeResult = !branch;
            // 四元式比较通常先保存布尔临时值，再装载它用于跳转；保留保存的副作用。
            if (!branch && comparison && i + 5 < program.code.size() && !entry[i + 4] && !entry[i + 5] && program.code[i + 4].op == Op::Load && program.code[i + 4].name == store.name && (program.code[i + 5].op == Op::Jz || program.code[i + 5].op == Op::Jnz) && target(i + 5) != absent)
            {
                internal.kind = RegisterOp::IntegerBranch;
                internal.span = 6;
                internal.target = target(i + 5);
                internal.jumpOnZero = program.code[i + 5].op == Op::Jz;
            }
        }
    }
    return registers;
}
/**
 * @brief 运行前解析符号、跳转及函数局部槽，运行中不更改原始程序。
 */
class ExecutionPlan
{
public:
    Memory::Map<Memory::String, std::size_t> symbols;
    Memory::Vector<PreparedSymbol> types;
    Memory::Vector<PreparedInstruction> code;
    Memory::Vector<RegisterInstruction> registers;
    /**
     * @brief 构造本次运行的执行准备数据。
     * @param program 不变的原始程序。
     */
    explicit ExecutionPlan(const Program &program, bool usePrepared = true) : program_(program), seen_(program.code.size(), 0)
    {
        code.resize(program.code.size());
        if (Memory::restartSignal().pending()) return;
        Memory::Vector<bool> entry(code.size(), false);
        for (const auto &label : program.labels)
            if (label.second < entry.size()) entry[label.second] = true;
        for (std::size_t i = 0; i < code.size(); ++i)
        {
            if (Memory::restartSignal().pending()) return;
            const auto &source = program.code[i];
            code[i].symbol = intern(source.name);
            intern(source.name2);
            if (source.op == Op::Jmp || source.op == Op::Jz || source.op == Op::Jnz || source.op == Op::Call)
            {
                const auto target = program.labels.find(source.name);
                if (target != program.labels.end()) code[i].target = target->second;
            }
        }
        for (const auto &parameters : program.parameters)
            for (const auto &name : parameters.second) intern(name);
        if (usePrepared && program.preparedInternal)
        {
            registers = program.internalCode;
            // 浮点立即数仅在本次执行准备中补齐，不改变已发布的内部指令格式。
            for (const auto &item : lowerRegisters(program, true))
                if (item.floatingLiteral) registers.push_back(item);
        }
        if (!(usePrepared && program.preparedInternal)) registers = lowerRegisters(program, true);
        for (std::size_t i = 0; i < registers.size(); ++i) code[registers[i].left].internal = i;
        // 直接调用和有形参的间接入口提前准备；零参数动态入口首次进入时补齐。
        for (const auto &parameters : program.parameters)
        {
            const auto label = program.labels.find(parameters.first);
            if (label != program.labels.end()) layout(label->second);
        }
        for (const auto &instruction : code)
            if (instruction.target != absent)
            {
                const auto index = static_cast<std::size_t>(&instruction - code.data());
                if (program.code[index].op == Op::Call) layout(instruction.target);
            }
    }
    /**
     * @brief 将名字解析为稳定整数身份，动态回调临时名称不自动驻留。
     * @param name 名字。
     * @return 已登记身份或 absent。
     */
    std::size_t find(const Memory::String &name) const
    {
        const auto found = symbols.find(name);
        return found == symbols.end() ? absent : found->second;
    }
    /**
     * @brief 获取函数的局部槽布局，遍历真实可达边，不跨越调用进入被调函数。
     * @param entry 入口下标。
     * @return 稳定的布局指针。
     */
    const FrameLayout *layout(std::size_t entry)
    {
        auto found = layouts_.find(entry);
        if (found != layouts_.end()) return found->second.get();
        auto owned = Memory::makeUnique<FrameLayout>();
        auto *result = owned.get();
        layouts_.emplace(entry, std::move(owned));
        for (const auto &parameters : program_.parameters)
        {
            const auto label = program_.labels.find(parameters.first);
            if (label != program_.labels.end() && label->second == entry)
                for (const auto &name : parameters.second) bind(*result, find(name));
        }
        Memory::Vector<std::size_t> work{ entry }, visited;
        ++generation_;
        while (!work.empty())
        {
            if (Memory::restartSignal().pending()) return result;
            const auto index = work.back();
            work.pop_back();
            if (index >= code.size() || seen_[index] == generation_) continue;
            seen_[index] = generation_;
            visited.push_back(index);
            const auto &instruction = program_.code[index];
            if (instruction.op == Op::Declare) bind(*result, code[index].symbol);
            if (instruction.op == Op::Ret || instruction.op == Op::Halt) continue;
            if (instruction.op == Op::Jmp || instruction.op == Op::Jz || instruction.op == Op::Jnz)
            {
                if (code[index].target != absent) work.push_back(code[index].target);
                if (instruction.op == Op::Jmp) continue;
            }
            work.push_back(index + 1);
        }
        for (const auto index : visited)
        {
            auto &prepared = code[index];
            if (prepared.shared) continue;
            if (prepared.owner && prepared.owner != result)
            {
                prepared.owner = nullptr;
                prepared.local = absentRegister;
                prepared.shared = true;
                continue;
            }
            prepared.owner = result;
            const auto local = result->slots.find(prepared.symbol);
            if (local != result->slots.end())
            {
                if (local->second < absentRegister)
                    prepared.local = static_cast<RegisterIndex>(local->second);
                else
                    prepared.shared = true;  // 极端大函数保留宽槽路径，禁止编号截断。
            }
        }
        return result;
    }

private:
    /**
     * @brief 登记符号与类型。
     * @param name 原始名字。
     * @return 整数身份。
     */
    std::size_t intern(const Memory::String &name)
    {
        if (name.empty()) return absent;
        const auto inserted = symbols.emplace(name, types.size());
        if (inserted.second)
        {
            const auto type = program_.types.find(name);
            types.push_back({ type == program_.types.end() ? nullptr : &type->second });
        }
        return inserted.first->second;
    }
    /**
     * @brief 为函数登记一个稠密槽。
     * @param layout 函数布局。
     * @param symbol 符号身份。
     * @return 无。
     */
    static void bind(FrameLayout &layout, std::size_t symbol)
    {
        if (symbol != absent) layout.slots.emplace(symbol, layout.slots.size());
    }
    const Program &program_;
    Memory::Map<std::size_t, Memory::UniquePtr<FrameLayout>> layouts_;
    Memory::Vector<std::size_t> seen_;
    std::size_t generation_ = 0;
};
}  // namespace execution_detail
}  // namespace stackvm
