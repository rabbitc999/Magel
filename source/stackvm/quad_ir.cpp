/**
 * @file quad_ir.cpp
 * @brief 从四元式构建基本块、控制流图与定义-使用链。
 */
#include "quad_ir.hpp"
#include "runtime_internal.hpp"

namespace stackvm
{
namespace ir
{
using namespace runtimeDetail;
namespace
{
/**
 * @brief 是否是空操作数或占位符 `_`。
 * @param text 操作数原文。
 * @return 是占位返回 true。
 */
bool placeholder(const Memory::String &text)
{
    return text.empty() || text == "_";
}

/**
 * @brief 是否是字面量（数字或引号字符串），字面量不进值表。
 * @param text 操作数原文。
 * @return 是字面量返回 true。
 */
bool literal(const Memory::String &text)
{
    if (placeholder(text)) return true;
    const char front = text.front();
    if (front == '"' || front == '\'') return true;
    bool isFloat = false;
    std::int64_t integer = 0;
    double real = 0.0;
    return parseNumber(text, isFloat, integer, real);
}

/**
 * @brief 一条指令的分类结果：定义、使用、标签与标志位。
 */
struct Shape
{
    Memory::String defs[kMaxDefinitions];  ///< 定义的值名。
    Memory::String uses[kMaxUses];         ///< 使用的值名。
    std::size_t defCount = 0;              ///< 有效定义个数。
    std::size_t useCount = 0;              ///< 有效使用个数。
    Memory::String target;                 ///< 跳转 / 陷阱 / 调用目标。
    bool barrier = false;                  ///< 有副作用。
    bool terminator = false;               ///< 是否终止基本块。

    /**
     * @brief 登记一个定义。
     * @param name 值名。
     */
    void define(const Memory::String &name)
    {
        if (defCount < kMaxDefinitions) defs[defCount++] = name;
    }

    /**
     * @brief 登记一个使用。
     * @param name 值名。
     */
    void use(const Memory::String &name)
    {
        if (useCount < kMaxUses) uses[useCount++] = name;
    }

    /**
     * @brief 登记一个使用，占位符与字面量跳过。
     * @param name 值名。
     */
    void useValue(const Memory::String &name)
    {
        if (!literal(name)) use(name);
    }
};

/**
 * @brief 按基础操作符给出这条四元式的分类。
 * @param quad 四元式。
 * @param base 去掉类型后缀的操作符。
 * @param shape 输出的分类结果。
 * @note 未识别的操作符按"全部操作数都是使用、且有副作用"处理，宁可保守也不能漏副作用。
 */
void classify(const Quad &quad, const Memory::String &base, Shape &shape)
{
    static const Memory::Map<Memory::String, bool> kBinary = {
        { "+", true },
        { "-", true },
        { "*", true },
        { "/", true },
        { "%", true },
        { "&", true },
        { "|", true },
        { "^", true },
        { "<<", true },
        { ">>", true },
        { "==", true },
        { "!=", true },
        { "<", true },
        { "<=", true },
        { ">", true },
        { ">=", true },
    };
    if (base == "decl")
    {
        shape.define(quad.arg(2));
        return;
    }
    if (base == "label")
    {
        shape.target = quad.arg(2);
        return;
    }
    if (base == "halt")
    {
        shape.terminator = true;
        shape.barrier = true;
        return;
    }
    if (base == "ret")
    {
        shape.useValue(quad.arg(0));
        shape.terminator = true;
        shape.barrier = true;
        return;
    }
    if (base == ":=" || base == "neg" || base == "~" || base == "cvt")
    {
        shape.useValue(quad.arg(0));
        shape.define(quad.arg(2));
        return;
    }
    if (kBinary.find(base) != kBinary.end())
    {
        shape.useValue(quad.arg(0));
        shape.useValue(quad.arg(1));
        shape.define(quad.arg(2));
        return;
    }
    if (base == "mul" || base == "div")
    {
        shape.useValue(quad.arg(0));
        shape.useValue(quad.arg(1));
        shape.define(quad.arg(2));
        shape.define(quad.arg(3));
        return;
    }
    if (base == "j")
    {
        shape.target = quad.arg(2);
        shape.terminator = true;
        return;
    }
    if (base == "jz" || base == "jnz")
    {
        shape.useValue(quad.arg(0));
        shape.target = quad.arg(2);
        shape.terminator = true;
        return;
    }
    if (base == "write" || base == "putc")
    {
        shape.useValue(quad.arg(0));
        shape.barrier = true;
        return;
    }
    if (base == "print")
    {
        shape.barrier = true;
        return;
    }
    if (base == "param")
    {
        shape.useValue(quad.arg(0));
        shape.barrier = true;
        return;
    }
    if (base == "read" || base == "getc")
    {
        shape.define(quad.arg(2));
        shape.barrier = true;
        return;
    }
    if (base == "call")
    {
        shape.define(quad.arg(2));
        shape.target = quad.arg(0);
        shape.barrier = true;
        return;
    }
    if (base == "callptr")
    {
        shape.useValue(quad.arg(0));
        shape.define(quad.arg(2));
        shape.barrier = true;
        return;
    }
    if (base == "addr")
    {
        shape.define(quad.arg(2));
        return;
    }
    if (base == "alloc")
    {
        shape.useValue(quad.arg(0));
        shape.define(quad.arg(2));
        shape.barrier = true;
        return;
    }
    if (base == "free" || base == "drop")
    {
        shape.useValue(quad.arg(0));
        shape.barrier = true;
        return;
    }
    if (base == "load")
    {
        shape.useValue(quad.arg(0));
        shape.define(quad.arg(2));
        shape.barrier = true;
        return;
    }
    if (base == "store")
    {
        shape.useValue(quad.arg(0));
        shape.useValue(quad.arg(1));
        shape.barrier = true;
        return;
    }
    if (base == "check")
    {
        shape.useValue(quad.arg(0));
        shape.useValue(quad.arg(1));
        shape.target = quad.arg(2);
        shape.barrier = true;
        return;
    }
    if (base == "checkptr")
    {
        shape.useValue(quad.arg(0));
        shape.useValue(quad.arg(1));
        shape.target = quad.arg(3);
        shape.barrier = true;
        return;
    }
    for (std::size_t index = 0; index < quad.args.size(); ++index) shape.useValue(quad.arg(index));
    shape.barrier = true;
}

/**
 * @brief 把块下标列表拼成 `b1,b2` 形式。
 * @param blocks 块下标列表。
 * @return 文本；空列表返回 `-`。
 */
Memory::String joinBlocks(const Memory::Vector<std::size_t> &blocks)
{
    if (blocks.empty()) return "-";
    Memory::String text;
    for (std::size_t index = 0; index < blocks.size(); ++index)
    {
        if (index != 0u) text += ",";
        text += "b" + Memory::toString(blocks[index]);
    }
    return text;
}
}  // namespace

Module build(const QuadFile &file)
{
    Module module;
    module.name = file.name;
    const std::size_t count = file.quads.size();
    module.instructions.reserve(count);

    Memory::Map<Memory::String, std::size_t> lookup;
    const auto valueOf = [&](const Memory::String &name, const TypeInfo &type, bool parameter, bool global) {
        const auto found = lookup.find(name);
        std::size_t index = absent;
        if (found == lookup.end())
        {
            index = module.values.size();
            Value value;
            value.name = name;
            value.type = type;
            value.parameter = parameter;
            value.global = global;
            module.values.push_back(std::move(value));
            lookup[name] = index;
        }
        else
        {
            index = found->second;
            Value &value = module.values[index];
            if (value.type.name.empty() && !type.name.empty()) value.type = type;
            value.parameter = value.parameter || parameter;
            value.global = value.global || global;
        }
        return index;
    };

    // 1. 逐条分类，同时把值表建起来：定义与使用都记到值上，方便后续做活跃性与分配。
    for (std::size_t index = 0; index < count; ++index)
    {
        const Quad &quad = file.quads[index];
        Memory::String typeName;
        const Memory::String base = splitType(quad.op, typeName);
        Instruction instruction;
        instruction.quad = index;
        instruction.op = base;
        instruction.typeName = typeName;
        instruction.args = quad.args;

        Shape shape;
        classify(quad, base, shape);
        instruction.target = shape.target;
        instruction.barrier = shape.barrier;
        instruction.terminator = shape.terminator;

        const TypeInfo declared = typeName.empty() ? typeOf("") : typeOf(typeName);
        for (std::size_t slot = 0; slot < shape.defCount; ++slot)
        {
            if (placeholder(shape.defs[slot])) continue;
            TypeInfo type = declared;
            if (base == "decl") type = typeOf(quad.arg(0));
            const bool parameter = false;
            const bool global = (base == "decl") && (quad.arg(1) == "global");
            const std::size_t value = valueOf(shape.defs[slot], type, parameter, global);
            module.values[value].definitions.push_back(index);
            if (instruction.defCount < kMaxDefinitions) instruction.defs[instruction.defCount++] = value;
        }
        for (std::size_t slot = 0; slot < shape.useCount; ++slot)
        {
            if (placeholder(shape.uses[slot])) continue;
            const std::size_t value = valueOf(shape.uses[slot], typeOf(""), false, false);
            module.values[value].uses.push_back(index);
            if (instruction.useCount < kMaxUses) instruction.uses[instruction.useCount++] = value;
        }
        module.instructions.push_back(std::move(instruction));
    }

    if (count == 0u) return module;

    // 2. 切基本块：标签起新块，终止指令之后也起新块。
    Memory::Vector<std::size_t> starts;
    starts.push_back(0u);
    for (std::size_t index = 0; index < count; ++index)
    {
        const Instruction &instruction = module.instructions[index];
        // 标签与"终止指令的下一条"可能指向同一条四元式（`j` 后面紧跟标签），只切一次。
        if (index != 0u && instruction.op == "label" && starts.back() != index) starts.push_back(index);
        if (instruction.terminator && index + 1u < count && starts.back() != index + 1u) starts.push_back(index + 1u);
    }

    Memory::Map<Memory::String, std::size_t> blockOfLabel;
    module.blocks.reserve(starts.size());
    for (std::size_t index = 0; index < starts.size(); ++index)
    {
        Block block;
        block.index = index;
        block.begin = starts[index];
        block.end = (index + 1u < starts.size()) ? starts[index + 1u] : count;
        const Instruction &first = module.instructions[block.begin];
        if (first.op == "label")
        {
            block.label = first.target;
            if (!block.label.empty() && blockOfLabel.find(block.label) == blockOfLabel.end())
                blockOfLabel[block.label] = index;
        }
        module.blocks.push_back(std::move(block));
    }

    const auto targetBlock = [&](const Memory::String &label) {
        const auto found = blockOfLabel.find(label);
        return found == blockOfLabel.end() ? absent : found->second;
    };

    // 3. 连边：终止指令决定后继，其余落到下一块。
    for (std::size_t index = 0; index < module.blocks.size(); ++index)
    {
        Block &block = module.blocks[index];
        const Instruction &last = module.instructions[block.end - 1u];
        const std::size_t fallthrough = (index + 1u < module.blocks.size()) ? index + 1u : absent;
        if (last.op == "j")
        {
            const std::size_t target = targetBlock(last.target);
            if (target != absent) block.successors.push_back(target);
        }
        else if (last.op == "jz" || last.op == "jnz")
        {
            const std::size_t target = targetBlock(last.target);
            if (target != absent) block.successors.push_back(target);
            if (fallthrough != absent) block.successors.push_back(fallthrough);
        }
        else if (last.op == "ret" || last.op == "halt")
        {
            // 出口块没有后继。
        }
        else if (fallthrough != absent)
        {
            block.successors.push_back(fallthrough);
        }
    }
    for (std::size_t index = 0; index < module.blocks.size(); ++index)
    {
        for (const std::size_t successor : module.blocks[index].successors)
        {
            Block &target = module.blocks[successor];
            target.predecessors.push_back(index);
            if (successor <= index)
            {
                target.loopHeader = true;
                module.blocks[index].backEdgeSource = true;
                module.backEdges.push_back(index);
            }
        }
    }

    // 4. 认函数入口：`call` 指向的标签，或"没人往里跳、且以 ret/halt 收尾"的标签块。
    Memory::Map<Memory::String, bool> called;
    Memory::Map<Memory::String, bool> jumped;
    for (const Instruction &instruction : module.instructions)
    {
        if (instruction.op == "call" && !instruction.target.empty()) called[instruction.target] = true;
        if ((instruction.op == "j" || instruction.op == "jz" || instruction.op == "jnz") && !instruction.target.empty())
            jumped[instruction.target] = true;
    }
    module.entry = 0u;
    Memory::Vector<std::size_t> entries;
    for (const Block &block : module.blocks)
    {
        if (block.label.empty()) continue;
        const Instruction &last = module.instructions[block.end - 1u];
        const bool returns = last.op == "ret" || last.op == "halt";
        const bool isCalled = called.find(block.label) != called.end();
        const bool isJumped = jumped.find(block.label) != jumped.end();
        if (isCalled || (!isJumped && returns)) entries.push_back(block.index);
    }
    for (std::size_t index = 0; index < entries.size(); ++index)
    {
        Function function;
        function.entry = entries[index];
        function.name = module.blocks[function.entry].label;
        function.externEntry = called.find(function.name) != called.end();
        const std::size_t limit = (index + 1u < entries.size()) ? entries[index + 1u] : module.blocks.size();
        for (std::size_t block = function.entry; block < limit; ++block) function.blocks.push_back(block);
        const std::size_t labelQuad = module.blocks[function.entry].begin;
        for (std::size_t cursor = labelQuad; cursor > 0u;)
        {
            const Instruction &previous = module.instructions[--cursor];
            if (previous.op != "decl" || previous.args.size() < 3u || previous.args[1] != "local") break;
            function.parameters.push_back(previous.args[2]);
        }
        for (std::size_t left = 0, right = function.parameters.size(); left + 1u < right; ++left, --right)
            std::swap(function.parameters[left], function.parameters[right - 1u]);
        module.functions.push_back(std::move(function));
    }
    return module;
}

Memory::String definitionName(const Module &module, const Instruction &instruction, std::size_t index)
{
    if (index >= instruction.defCount || instruction.defs[index] == absent) return "_";
    return module.values[instruction.defs[index]].name;
}

Memory::String useName(const Module &module, const Instruction &instruction, std::size_t index)
{
    if (index >= instruction.useCount || instruction.uses[index] == absent) return "_";
    return module.values[instruction.uses[index]].name;
}

Memory::String dump(const Module &module)
{
    Memory::String text = "ir module " + module.name + "\n";
    text += "ir counts functions " + Memory::toString(module.functions.size()) + " blocks " + Memory::toString(module.blocks.size()) + " values " + Memory::toString(module.values.size()) + " backedges " + Memory::toString(module.backEdges.size()) + "\n";

    Memory::Map<std::size_t, const Function *> owner;
    for (const Function &function : module.functions)
    {
        for (const std::size_t block : function.blocks) owner[block] = &function;
    }
    Memory::Map<std::size_t, bool> header;
    for (const Function &function : module.functions) header[function.entry] = true;

    for (const Block &block : module.blocks)
    {
        const auto function = owner.find(block.index);
        if (header.find(block.index) != header.end() && function != owner.end())
        {
            text += "function " + function->second->name + " entry b" + Memory::toString(block.index) + " blocks " + Memory::toString(function->second->blocks.size());
            if (!function->second->parameters.empty())
            {
                text += " params ";
                for (std::size_t index = 0; index < function->second->parameters.size(); ++index)
                {
                    if (index != 0u) text += ",";
                    text += function->second->parameters[index];
                }
            }
            text += "\n";
        }
        text += "  b" + Memory::toString(block.index) + " quads[" + Memory::toString(block.begin) + "," + Memory::toString(block.end) + ") preds " + joinBlocks(block.predecessors) + " succs " + joinBlocks(block.successors);
        if (block.backEdgeSource) text += " backedge";
        if (block.loopHeader) text += " loop";
        text += "\n";
        for (std::size_t index = block.begin; index < block.end; ++index)
        {
            const Instruction &instruction = module.instructions[index];
            const Memory::String spelled = instruction.typeName.empty() ? instruction.op : instruction.op + ":" + instruction.typeName;
            text += "    q" + Memory::toString(index) + " " + spelled + "(";
            for (std::size_t slot = 0; slot < instruction.args.size(); ++slot)
            {
                if (slot != 0u) text += ", ";
                text += instruction.args[slot];
            }
            text += ") defs ";
            bool any = false;
            for (std::size_t slot = 0; slot < instruction.defCount; ++slot)
            {
                if (slot != 0u) text += ",";
                text += definitionName(module, instruction, slot);
                any = true;
            }
            if (!any) text += "-";
            text += " uses ";
            any = false;
            for (std::size_t slot = 0; slot < instruction.useCount; ++slot)
            {
                if (slot != 0u) text += ",";
                text += useName(module, instruction, slot);
                any = true;
            }
            if (!any) text += "-";
            if (instruction.barrier) text += " barrier";
            if (instruction.terminator) text += " terminator";
            text += "\n";
        }
    }
    return text;
}
}  // namespace ir
}  // namespace stackvm
