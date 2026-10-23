/**
 * @file quad_ir.hpp
 * @brief 四元式的规范化中间表示：基本块、控制流图、值与定义-使用链。
 *
 * 后端（指令选择、寄存器分配、机器级优化）不再直接读四元式文本，而是读这里的结果：
 * 四元式先被切成基本块并连成控制流图，操作数被规范化成值（变量或临时）的下标，
 * 每条指令都记下它定义了什么、使用了什么。`.quad` 文本仍然保留，只作为调试视图。
 *
 * 本层**不改写**四元式，也不做 SSA 构造：值是"名字"而不是"版本"，一个变量可以有多个
 * 定义点（`definitions` 是列表）。按支配关系插 phi 的 SSA 化属于后续阶段，届时以这里的
 * 控制流图与定义-使用链为输入。
 */
#pragma once
#include "quad_model.hpp"

namespace stackvm
{
namespace ir
{
/**
 * @brief 不存在的下标。
 */
inline constexpr std::size_t absent = static_cast<std::size_t>(-1);

/**
 * @brief 一条指令最多定义几个值（双字乘除同时产出 lo / hi）。
 */
inline constexpr std::size_t kMaxDefinitions = 2;

/**
 * @brief 一条指令最多使用几个值（`store` 用两个）。
 */
inline constexpr std::size_t kMaxUses = 4;

/**
 * @brief 一个值：变量或编译器临时，按名字归并。
 */
struct Value
{
    Memory::String name;                      ///< 值名（变量名或临时名）。
    TypeInfo type;                            ///< 类型；无法确定时保持未声明口径。
    Memory::Vector<std::size_t> definitions;  ///< 定义它的四元式下标，按出现顺序。
    Memory::Vector<std::size_t> uses;         ///< 使用它的四元式下标，按出现顺序。
    bool parameter = false;                   ///< 是否函数形参。
    bool global = false;                      ///< 是否 `decl` 里的全局槽。
};

/**
 * @brief 一条规范化指令：源四元式加上解析好的值与分类标记。
 */
struct Instruction
{
    std::size_t quad = absent;                                        ///< 源四元式下标。
    Memory::String op;                                                ///< 基础操作符（已去掉类型后缀）。
    Memory::String typeName;                                          ///< 类型后缀，可为空。
    Memory::String target;                                            ///< 跳转或陷阱标签，可为空。
    Memory::Vector<Memory::String> args;                              ///< 原始操作数文本，供调试转储使用。
    std::size_t defs[kMaxDefinitions] = { absent, absent };           ///< 定义的值下标。
    std::size_t uses[kMaxUses] = { absent, absent, absent, absent };  ///< 使用的值下标。
    std::size_t defCount = 0;                                         ///< 有效定义个数。
    std::size_t useCount = 0;                                         ///< 有效使用个数。
    bool barrier = false;                                             ///< 有副作用，优化不得越过或重排。
    bool terminator = false;                                          ///< 是否基本块终止指令。
};

/**
 * @brief 一个基本块：四元式下标区间 `[begin, end)`。
 */
struct Block
{
    std::size_t index = 0;                     ///< 块下标（与 `Module::blocks` 一致）。
    std::size_t begin = 0;                     ///< 起始四元式下标（含）。
    std::size_t end = 0;                       ///< 结束四元式下标（不含）。
    Memory::String label;                      ///< 块首标签，可为空。
    Memory::Vector<std::size_t> successors;    ///< 后继块。
    Memory::Vector<std::size_t> predecessors;  ///< 前驱块。
    bool loopHeader = false;                   ///< 是否是回边目标（循环头）。
    bool backEdgeSource = false;               ///< 是否带着回边跳回前面的块。
};

/**
 * @brief 一个函数：入口块与其后到下一个函数入口之间的全部块。
 */
struct Function
{
    Memory::String name;                        ///< 函数名（入口标签）。
    std::size_t entry = absent;                 ///< 入口块下标。
    Memory::Vector<std::size_t> blocks;         ///< 属于本函数的块，按线性顺序。
    Memory::Vector<Memory::String> parameters;  ///< 形参名，按实参顺序。
    bool externEntry = false;                   ///< 是否是 `call` 直接指向的入口。
};

/**
 * @brief 一份四元式的规范化结果。
 */
struct Module
{
    Memory::String name;                       ///< 模块名（取自四元式文件）。
    std::size_t entry = absent;                ///< 程序入口块（通常是调用 `main` 的引导块）。
    Memory::Vector<Instruction> instructions;  ///< 逐条指令，下标与四元式下标一致。
    Memory::Vector<Value> values;              ///< 值表。
    Memory::Vector<Block> blocks;              ///< 基本块，按四元式线性顺序。
    Memory::Vector<Function> functions;        ///< 函数表，按入口块顺序。
    Memory::Vector<std::size_t> backEdges;     ///< 回边的源块下标。
};

/**
 * @brief 把四元式构建成规范化中间表示。
 * @param file 已解析（并通常已优化）的四元式文件。
 * @return 中间表示；四元式为空时返回只有入口标记的空模块。
 * @note 不改写 `file`；不构造 SSA。
 */
[[nodiscard]] Module build(const QuadFile &file);

/**
 * @brief 把中间表示打印成稳定的多行文本，供调试转储与回归比对。
 * @param module 中间表示。
 * @return 文本，每行以换行结尾。
 */
[[nodiscard]] Memory::String dump(const Module &module);

/**
 * @brief 取一条指令定义的第 index 个值名。
 * @param module 中间表示。
 * @param instruction 指令。
 * @param index 第几个定义。
 * @return 值名；越界或未定义时返回操作数占位 `_`。
 */
[[nodiscard]] Memory::String definitionName(const Module &module, const Instruction &instruction, std::size_t index);

/**
 * @brief 取一条指令使用的第 index 个值名。
 * @param module 中间表示。
 * @param instruction 指令。
 * @param index 第几个使用。
 * @return 值名；越界或未使用时返回操作数占位 `_`。
 */
[[nodiscard]] Memory::String useName(const Module &module, const Instruction &instruction, std::size_t index);
}  // namespace ir
}  // namespace stackvm
