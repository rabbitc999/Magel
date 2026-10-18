/**
 * @brief 四元式基本块及其前驱，函数入口不继承调用方的常量。
 */
struct OptimizationBlock
{
    std::size_t begin = 0, end = 0;
    Memory::Vector<std::size_t> predecessors;
    bool root = false;
};

/**
 * @brief 在控制流合流及循环回边上求可用整数常量的不动点。
 * @param code 四元式序列。
 * @param declared 已声明类型。
 * @param report 优化统计。
 * @return 是否替换了跨块常量或重复表达式。
 * @note 调用及内存副作用清空状态；分析未收敛时不应用常量结果。
 */
bool optimizeControlFlow(Memory::Vector<Quad> &code, const Memory::Map<Memory::String, TypeInfo> &declared,
                         OptimizationReport &report)
{
    if (code.empty()) return false;
    using Constants = Memory::Map<Memory::String, Memory::String>;
    Memory::Vector<OptimizationBlock> blocks;
    Memory::Vector<std::size_t> owner(code.size());
    Memory::Map<Memory::String, std::size_t> labels;
    bool start = true;
    for (std::size_t i = 0; i < code.size(); ++i)
    {
        const auto &op = code[i].op;
        if (start || op == "label")
        {
            if (!blocks.empty()) blocks.back().end = i;
            blocks.push_back({});
            blocks.back().begin = i;
            blocks.back().root = blocks.size() == 1;
        }
        owner[i] = blocks.size() - 1;
        if (op == "label") labels[code[i].arg(2)] = owner[i];
        start = op == "j" || op == "jz" || op == "jnz" || op == "ret" || op == "halt";
    }
    blocks.back().end = code.size();
    for (std::size_t b = 0; b < blocks.size(); ++b)
    {
        const auto &last = code[blocks[b].end - 1];
        if (last.op == "j" || last.op == "jz" || last.op == "jnz")
        {
            const auto target = labels.find(last.arg(2));
            if (target != labels.end()) blocks[target->second].predecessors.push_back(b);
        }
        if (b + 1 < blocks.size() && last.op != "j" && last.op != "ret" && last.op != "halt")
            blocks[b + 1].predecessors.push_back(b);
    }
    // 非跳转引用可导出函数入口，包括函数指针和反射元数据。
    for (const auto &quad : code)
        if (quad.op != "label" && quad.op != "j" && quad.op != "jz" && quad.op != "jnz")
            for (const auto &arg : quad.args)
            {
                const auto label = labels.find(arg);
                if (label != labels.end()) blocks[label->second].root = true;
            }
    for (auto &block : blocks)
        if (block.predecessors.empty()) block.root = true;
    const auto transfer = [&](const OptimizationBlock &block, Constants state) {
        for (std::size_t i = block.begin; i < block.end; ++i)
        {
            const auto &quad = code[i];
            Memory::String suffix;
            const auto base = splitType(quad.op, suffix);
            const auto written = writesTo(base);
            if (base == ":=")
            {
                auto value = quad.arg(0);
                const auto known = state.find(value);
                if (known != state.end()) value = known->second;
                state.erase(quad.arg(2));
                bool number = false, floating = false;
                std::int64_t integer = 0;
                double real = 0;
                Memory::String text;
                if (readLiteral(value, number, floating, integer, real, text) && number && !floating)
                {
                    const auto type = declared.find(quad.arg(2));
                    if (type == declared.end())
                        state[quad.arg(2)] = value;
                    else if (!type->second.isFloat)
                        state[quad.arg(2)] = Memory::toString(normalizeInt(integer, type->second));
                }
            }
            else if (base == "mul" || base == "div")
                state.clear();
            else if (written >= 0)
                state.erase(quad.arg(static_cast<std::size_t>(written)));
            else if (base == "decl")
                state.erase(quad.arg(2));
            else if (base != "label" && base != "j" && base != "jz" && base != "jnz" && base != "param" && base != "write" && base != "putc" && base != "print" && base != "ret" && base != "halt")
                state.clear();
        }
        return state;
    };
    Memory::Vector<Constants> inputs(blocks.size()), outputs(blocks.size());
    Memory::Vector<bool> ready(blocks.size(), false);
    bool stable = false;
    // 有限预算只限制分析成本；耗尽时放弃结果，不提交半收敛状态。
    for (std::size_t round = 0; round <= blocks.size() * 2 + 1; ++round)
    {
        bool changed = false;
        for (std::size_t b = 0; b < blocks.size(); ++b)
        {
            Constants input;
            bool known = blocks[b].root;
            if (!blocks[b].root)
                for (const auto pred : blocks[b].predecessors)
                {
                    if (!ready[pred]) continue;
                    if (!known)
                    {
                        input = outputs[pred];
                        known = true;
                    }
                    else
                        for (auto it = input.begin(); it != input.end();)
                        {
                            const auto other = outputs[pred].find(it->first);
                            if (other == outputs[pred].end() || other->second != it->second)
                                it = input.erase(it);
                            else
                                ++it;
                        }
                }
            if (!known) continue;
            auto output = transfer(blocks[b], input);
            if (!ready[b] || inputs[b] != input || outputs[b] != output) changed = true;
            inputs[b] = std::move(input);
            outputs[b] = std::move(output);
            ready[b] = true;
        }
        if (!changed)
        {
            stable = true;
            break;
        }
    }
    bool changed = false;
    if (stable)
        for (std::size_t b = 0; b < blocks.size(); ++b)
        {
            if (!ready[b]) continue;
            auto state = inputs[b];
            for (std::size_t i = blocks[b].begin; i < blocks[b].end; ++i)
            {
                auto &quad = code[i];
                Memory::String suffix;
                const auto base = splitType(quad.op, suffix);
                for (const auto index : optimizationReads(base))
                    if (index < quad.args.size() && (base != "write" || !suffix.empty()))
                    {
                        const auto value = state.find(quad.arg(index));
                        if (value != state.end())
                        {
                            quad.args[index] = value->second;
                            ++report.crossBlockConstants;
                            changed = true;
                        }
                    }
                OptimizationBlock single;
                single.begin = i;
                single.end = i + 1;
                state = transfer(single, std::move(state));
            }
        }
    // 局部公共子表达式只重用无声明临时值，保留首次计算的错误和实际数值类型。
    for (const auto &block : blocks)
    {
        Memory::Map<Memory::Vector<Memory::String>, Memory::String> expressions;
        for (std::size_t i = block.begin; i < block.end; ++i)
        {
            auto &quad = code[i];
            Memory::String suffix;
            const auto base = splitType(quad.op, suffix);
            const auto written = writesTo(base);
            if (written < 0 || base == "read" || base == "getc" || base == "mul" || base == "div")
            {
                if (base != "label" && base != "param" && base != "write" && base != "putc") expressions.clear();
                continue;
            }
            const auto destination = quad.arg(static_cast<std::size_t>(written));
            for (auto it = expressions.begin(); it != expressions.end();)
                if (it->second == destination || it->first[1] == destination || it->first[2] == destination)
                    it = expressions.erase(it);
                else
                    ++it;
            const bool eligible = base == "+" || base == "-" || base == "*" || base == "/" || base == "%" || base == "cvt" || base == "neg" || base == "~" || base == "&" || base == "|" || base == "^" || base == "<<" || base == ">>" || base == "==" || base == "!=" || base == "<" || base == "<=" || base == ">" || base == ">=";
            if (!eligible || declared.count(destination) || !optimizationTemporary(destination) || destination == quad.arg(0) || destination == quad.arg(1)) continue;
            Memory::Vector<Memory::String> key{ quad.op, quad.arg(0), quad.arg(1) };
            const auto found = expressions.find(key);
            if (found != expressions.end())
            {
                quad.op = ":=";
                quad.args = { found->second, "_", destination };
                ++report.commonExpressionsRemoved;
                changed = true;
            }
            else
                expressions.emplace(std::move(key), destination);
        }
    }
    return changed;
}
