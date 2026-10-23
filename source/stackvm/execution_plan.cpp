/**
 * @file execution_plan.cpp
 * @brief 执行准备：按函数收集活跃性视图并分配帧槽。
 */
#include "runtime_internal.hpp"
#include "register_allocate.hpp"

namespace stackvm
{
namespace execution_detail
{
const FrameLayout *ExecutionPlan::layout(std::size_t entry)
{
    auto found = layouts_.find(entry);
    if (found != layouts_.end()) return found->second.get();
    auto owned = Memory::makeUnique<FrameLayout>();
    auto *result = owned.get();
    layouts_.emplace(entry, std::move(owned));
    if (entry >= code.size()) return result;

    // 1. 形参按位置固定（固定 ABI）：同一个入口只对应一张形参表。
    allocation::Body body;
    for (const auto &parameters : program_.parameters)
    {
        const auto label = program_.labels.find(parameters.first);
        if (label == program_.labels.end() || label->second != entry) continue;
        for (const auto &name : parameters.second) body.parameters.push_back(find(name));
        break;
    }

    /* 2. 只走真实可达边，不跨越调用进入被调函数；顺便记下每个位置的定义、使用与后继。
     *    槽位只分配给"本函数自己的"符号：形参、声明过的局部与编译器临时量，其余走全局路径。
     *    表按**位置**建，不按全局 PC：帧布局是执行期惰性算的，按 PC 建表就是每调用一个新
     *    函数都在运行期 arena 上摊一张"整个程序那么大"的表，会把程序自己的堆挤掉。 */
    Memory::Map<std::size_t, std::size_t> positionOf;                   // PC → 位置
    Memory::Vector<std::pair<std::size_t, std::size_t>> rawSuccessors;  // 位置 → 两个后继 PC
    /* 编译器临时量（`__temp_*`、`__chali_ret_v$*`）也配帧槽：它们只在本函数一次激活里
     * 写了再读，共享一份跨激活存储会让递归的内层调用覆写外层挂起的结果
     * （`return f(n-1) + f(n-1)` 第一趟的返回值会被第二趟递归覆盖，f(3) 算出 4 而不是 8）。 */
    Memory::Set<std::size_t> scratchSeen;
    const auto takeScratch = [&](std::size_t symbol) {
        if (symbol == absent || !scratch(symbol)) return;
        if (scratchSeen.insert(symbol).second) body.declared.push_back(symbol);
    };
    Memory::Vector<std::size_t> work{ entry };
    ++generation_;
    while (!work.empty())
    {
        if (Memory::restartSignal().pending()) return result;
        const auto index = work.back();
        work.pop_back();
        if (index >= code.size() || seen_[index] == generation_) continue;
        seen_[index] = generation_;
        positionOf.emplace(index, body.order.size());
        body.order.push_back(index);
        const auto &instruction = program_.code[index];
        if (instruction.op == Op::Declare)
        {
            /* 声明过的局部与形参固定配帧槽；编译器临时量在上面单独收集（也是帧槽），
             * 其余未声明名字是真正的全局变量，留在全局表。 */
            body.declared.push_back(code[index].symbol);
        }
        else if (instruction.op == Op::Call || instruction.op == Op::CallIndirect)
            takeScratch(find(instruction.name2));  // 调用结果临时量：返回值写回的目标。
        else
            takeScratch(code[index].symbol);  // Store 的目标、Load 的来源都可能落在临时量上。
        std::pair<std::size_t, std::size_t> successors{ absent, absent };
        const std::size_t next = index + 1u;
        if (instruction.op == Op::Ret || instruction.op == Op::Halt)
        {
            // 出口没有后继。
        }
        else if (instruction.op == Op::Jmp)
        {
            successors.first = code[index].target;
        }
        else if (instruction.op == Op::Jz || instruction.op == Op::Jnz)
        {
            successors.first = code[index].target;
            if (next < code.size()) successors.second = next;
        }
        else if (next < code.size())
        {
            successors.first = next;
        }
        rawSuccessors.push_back(successors);
        if (instruction.op == Op::Ret || instruction.op == Op::Halt) continue;
        if (instruction.op == Op::Jmp || instruction.op == Op::Jz || instruction.op == Op::Jnz)
        {
            if (code[index].target != absent) work.push_back(code[index].target);
            if (instruction.op == Op::Jmp) continue;
        }
        work.push_back(next);
    }
    // 第二遍：把 PC 换成位置下标，填出定义 / 使用 / 后继。（"没走到过的 PC"一律 absent。）
    body.successors.assign(body.order.size(), { absent, absent });
    body.defines.assign(body.order.size(), absent);
    body.uses.assign(body.order.size(), absent);
    for (std::size_t at = 0; at < body.order.size(); ++at)
    {
        const std::size_t index = body.order[at];
        const auto &instruction = program_.code[index];
        if (instruction.op == Op::Declare || instruction.op == Op::Store || instruction.op == Op::Read || instruction.op == Op::Getc)
            body.defines[at] = code[index].symbol;
        else if (instruction.op == Op::Load)
            body.uses[at] = code[index].symbol;
        else if (instruction.op == Op::Call || instruction.op == Op::CallIndirect)
            body.defines[at] = find(instruction.name2);
        for (std::size_t edge = 0; edge < 2u; ++edge)
        {
            const std::size_t successor = edge == 0u ? rawSuccessors[at].first : rawSuccessors[at].second;
            if (successor == absent) continue;
            const auto found = positionOf.find(successor);
            if (found == positionOf.end()) continue;
            if (edge == 0u)
                body.successors[at].first = found->second;
            else
                body.successors[at].second = found->second;
        }
    }

    /* 2.5 浮点形态推断：只认"这条指令真按浮点算"的名字。
     *   判据三条：① 已解析类型写着浮点（形参、声明过的局部）；
     *   ② 带 `isFloat` 的二元 / 比较运算的目标与它的变量操作数；
     *   ③ 浮点立即数直接写进的目标（`PushDouble; Store`）。
     *   转换指令（`ToDouble` 之类）不算——它的操作数还是整数，算进来会把整数槽误标成浮点。 */
    Memory::Set<std::size_t> realSeen;
    const auto markReal = [&](std::size_t symbol) {
        if (symbol != absent) realSeen.insert(symbol);
    };
    for (const std::size_t symbol : body.parameters)
        if (symbol < types.size() && types[symbol].type != nullptr && types[symbol].type->isFloat) markReal(symbol);
    for (const std::size_t symbol : body.declared)
        if (symbol < types.size() && types[symbol].type != nullptr && types[symbol].type->isFloat) markReal(symbol);
    for (const std::size_t index : body.order)
    {
        const auto &instruction = program_.code[index];
        if (instruction.op == Op::PushDouble)
        {
            if (index + 1u < program_.code.size() && program_.code[index + 1u].op == Op::Store) markReal(code[index + 1u].symbol);
            continue;
        }
        if (!instruction.isFloat) continue;
        const bool binary = instruction.op == Op::Add || instruction.op == Op::Sub || instruction.op == Op::Mul ||
                            instruction.op == Op::Div || instruction.op == Op::Mod || instruction.op == Op::Eq ||
                            instruction.op == Op::Ne || instruction.op == Op::Lt || instruction.op == Op::Le ||
                            instruction.op == Op::Gt || instruction.op == Op::Ge;
        if (!binary && instruction.op != Op::Neg) continue;
        if (index + 1u < program_.code.size() && program_.code[index + 1u].op == Op::Store) markReal(code[index + 1u].symbol);
        if (!binary) continue;
        for (std::size_t back = 1u; back <= 2u; ++back)
        {
            if (index < back) break;
            if (program_.code[index - back].op == Op::Load) markReal(code[index - back].symbol);
        }
    }
    for (const std::size_t symbol : realSeen) body.reals.push_back(symbol);

    // 3. 活跃性 + 线性扫描：存活区间不重叠的变量共用槽位。
    allocation::Layout assigned;
    if (const char *reuse = std::getenv("CHALI_NO_SLOT_REUSE"); reuse != nullptr && *reuse != '\0')
    {
        // 对照口径：形参在前、其余按声明顺序一人一个槽（复用前的行为）。
        Memory::Map<std::size_t, bool> bound;
        const auto take = [&](std::size_t symbol) {
            if (symbol == absent || bound.find(symbol) != bound.end()) return;
            bound.emplace(symbol, true);
            assigned.slots.push_back({ symbol, assigned.count++ });
        };
        for (const std::size_t symbol : body.parameters) take(symbol);
        for (const std::size_t pc : body.order)
        {
            if (program_.code[pc].op == Op::Declare) take(code[pc].symbol);
            else if (code[pc].symbol != absent && scratch(code[pc].symbol)) take(code[pc].symbol);
            const auto &instruction = program_.code[pc];
            if (instruction.op == Op::Call || instruction.op == Op::CallIndirect)
                take(find(instruction.name2));
        }
    }
    else
    {
        assigned = allocation::assign(body);
    }
    for (const auto &[symbol, slot] : assigned.slots) result->slots.emplace(symbol, slot);
    result->count = assigned.count;  // 帧按实际槽数扩容，不再按符号数。
    if (const char *dump = std::getenv("CHALI_DUMP_FRAME"); dump != nullptr && *dump != '\0')
    {
        Memory::Map<std::size_t, Memory::String> names;
        for (const auto &item : symbols) names.emplace(item.second, item.first);
        std::fprintf(stderr, "帧布局 entry=%zu 符号 %zu 槽 %zu\n", entry, result->slots.size(), assigned.count);
        for (const auto &[symbol, slot] : result->slots)
        {
            const auto found = names.find(symbol);
            std::fprintf(stderr, "  %-48s -> %zu\n", found == names.end() ? "?" : found->second.c_str(), slot);
        }
    }
    // 4. 把槽号写进预绑定指令；跨函数共享的指令保留动态查找。
    for (const auto index : body.order)
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
}  // namespace execution_detail
}  // namespace stackvm
