#pragma once
#include "instruction_select.hpp"

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
    std::size_t count = 0;  ///< 实际槽数（复用后小于 slots.size()）。
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
 * @return 内部执行序列。
 * @note 交给指令选择模块：模式表只融合能证明等价的窗口。
 */
inline Memory::Vector<RegisterInstruction> lowerRegisters(const Program &program)
{
    return selection::lower(program);
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
            registers = program.internalCode;
        else
            registers = lowerRegisters(program);
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
     * @brief 是不是编译器临时量（`__temp_*`、`__chali_ret_v$*`）。
     *
     * 临时量只在本函数的一次激活里写了再读，因此必须每次激活一份：递归时内层调用
     * 会往同名临时量里写，跨激活共享一份存储就会覆写外层挂起的结果。
     * @param symbol 整数身份。
     * @return 临时量返回 true。
     */
    bool scratch(std::size_t symbol) const
    {
        return symbol != absent && symbol < scratch_.size() && scratch_[symbol];
    }
    /**
     * @brief 获取函数的局部槽布局：活跃性 + 线性扫描，槽位在存活区间不重叠时复用。
     * @param entry 入口下标。
     * @return 稳定的布局指针。
     */
    const FrameLayout *layout(std::size_t entry);

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
            scratch_.push_back(name.rfind("__temp_", 0) == 0 || name.rfind("__chali_ret_v$", 0) == 0);
        }
        return inserted.first->second;
    }
    const Program &program_;
    Memory::Map<std::size_t, Memory::UniquePtr<FrameLayout>> layouts_;
    Memory::Vector<std::size_t> seen_;
    Memory::Vector<bool> scratch_;  ///< 与 types 平行：这个名字是不是编译器临时量。
    std::size_t generation_ = 0;
};
}  // namespace execution_detail
}  // namespace stackvm
