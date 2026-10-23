/**
 * @file simple_ast.cpp
 * @brief AST 类型推导、表达式与语句降级及四元式程序装配。
 */

#include "ast_storage_detail.hpp"
#include "simple_ast.hpp"

#include <cstdlib>
#include <cstring>
#include <cmath>
#include <functional>
#include <iomanip>
#include <limits>
#include <sstream>

namespace simple_ast
{
namespace
{

using Kind = Type::Kind;
using TypeEnv = Memory::Map<Memory::String, TypePtr>;

/**
 * 生成期状态。
 */
struct Emitter
{
    CodegenResult result;
    TypeEnv env;
    Memory::Vector<Memory::String> dataLines;
    Memory::Vector<Memory::String> lines;
    Memory::Map<Memory::String, Memory::String> literalNames;  ///< 当前生成单元内按字节内容合并的普通字面量。
    Memory::Set<Memory::String> reserved;                      ///< 程序里已经用掉的名字，内部临时名要避开。
    std::size_t tempIndex = 0;
    std::size_t labelIndex = 0;
    std::size_t lambdaIndex = 0;

    /**
     * @brief 记录当前处理过程的错误信息。
     * @param message 待报告或输出的信息文本。
     */
    void error(const Memory::String &message) { result.diagnostics.push_back(message); }
    Memory::String sourceFile;
    std::size_t sourceLine = 0;
    bool hasSourceMarker = false;
    /**
     * @brief 向生成结果追加一条文本指令。
     * @param text 待处理的文本。
     */
    void emit(const Memory::String &text)
    {
        if (sourceLine)
        {
            Memory::OutputStringStream marker;
            marker << "# chali-source: " << sourceLine << " " << std::quoted(sourceFile) << "\n";
            lines.push_back(marker.str() + text);
            hasSourceMarker = true;
        }
        else if (hasSourceMarker)
        {
            lines.push_back("# chali-source: 0 \"\"\n" + text);
            hasSourceMarker = false;
        }
        else
            lines.push_back(text);
    }
    /**
     * @brief 分配一个不会与用户变量撞名的内部临时变量名称。
     *
     * 名字统一取 `__temp_t<n>`：小驼峰 / 下划线之外的写法不会出现在普通变量上，
     * 基本不会撞。真撞上了（用户偏要写 `__temp_t1`）就往后续号，绝不覆盖用户槽——
     * 覆盖会让用户变量顶掉传引用临时槽里已经登记好的析构，退出函数时那块存储就漏了。
     *
     * @retval 全程序唯一的内部临时名。
     */
    Memory::String temp()
    {
        for (;;)
        {
            Memory::String name = "__temp_t" + Memory::toString(++tempIndex);
            if (reserved.count(name) != 0u) continue;
            reserved.insert(name);
            return name;
        }
    }
    /**
     * @brief 创建带指定提示前缀的跳转标签。
     * @param hint 生成标签使用的名称提示。
     * @retval 生成或读取的结果文本。
     */
    Memory::String label(const Memory::String &hint) { return hint + "_" + Memory::toString(++labelIndex); }
    /**
     * @brief 添加数据区条目及其类型信息。
     * @param name 名称文本。
     * @param body 待处理的函数体或循环体。
     * @param type 类型描述。
     */
    void data(const Memory::String &name, const Memory::String &body, const TypePtr &type)
    {
        /* 同名数据项只登记一次（`Node::namedStr` 要的是"同一个东西只有一份"）。 */
        for (const CodegenResult::Entry &entry : result.entries)
        {
            if (entry.name == name) return;
        }
        dataLines.push_back("# " + name + " : " + type->describe());
        if (!type->meta.empty()) dataLines.push_back("#   meta: " + type->meta);
        dataLines.push_back(name + ": " + body);
        result.entries.push_back(CodegenResult::Entry{ name, type, body });
    }
};

/**
 * @brief 收集一段语法树里会写进四元式目标操作数的名字。
 *
 * `(decl, T, local, 名字)` 与 `(:=, 值, _, 名字)` 的目标名都是用户自己起的，
 * 生成器的内部临时名必须避开这些名字，否则会把用户变量的存储抢走。
 *
 * @param node 待扫描的语法节点，空指针直接返回。
 * @param names 输出：收集到的名字集合。
 */
void collectReservedNames(const NodePtr &node, Memory::Set<Memory::String> &names)
{
    if (!node) return;
    if (node->kind == Node::Kind::Declare || node->kind == Node::Kind::Assign) names.insert(node->text);
    for (const NodePtr &kid : node->kids) collectReservedNames(kid, names);
}

/**
 * 字面量的原文；不是字面量返回空串。
 */
/**
 * @brief 在发射节点期间保留来源，递归退出恢复调用者位置。
 */
struct SourceScope
{
    Emitter &emitter;
    Memory::String file;
    std::size_t line;
    /**
     * @brief 保存并切换当前节点的源码位置。
     * @param e 接收生成指令的发射器。
     * @param node 待处理的语法节点。
     */
    SourceScope(Emitter &e, const NodePtr &node) : emitter(e), file(e.sourceFile), line(e.sourceLine)
    {
        if (node && node->sourceLine)
        {
            e.sourceFile = node->sourceFile;
            e.sourceLine = node->sourceLine;
        }
    }
    /**
     * @brief 恢复进入节点前的源码位置。
     */
    ~SourceScope()
    {
        emitter.sourceFile = std::move(file);
        emitter.sourceLine = line;
    }
};

/**
 * @brief 将常量节点转换为四元式使用的字面量文本。
 * @param node 待处理的语法节点。
 * @retval 生成或读取的结果文本。
 */
Memory::String literalText(const NodePtr &node)
{
    if (!node) return Memory::String();
    switch (node->kind)
    {
    case Node::Kind::IntLit: return Memory::toString(node->intValue);
    case Node::Kind::FloatLit:
    {
        // 浮点字面量一律带小数点：`2.0` 写成 `2` 的话，四元式的文本形态会被当成整数，
        // 后端要么截断要么按整数压栈，浮点语义就丢了。
        //
        Memory::OutputStringStream buffer;
        buffer << std::setprecision(std::numeric_limits<double>::max_digits10)
               << node->floatValue;
        Memory::String text = buffer.str();
        if (text.find_first_of(".eE") == Memory::String::npos) text += ".0";
        return text;
    }
    case Node::Kind::BoolLit: return node->intValue != 0 ? "1" : "0";
    case Node::Kind::NullLit: return "0";
    default: return Memory::String();
    }
}

/**
 * @brief 数据区里的一个字面量：**浮点槽一定带小数点**。
 *
 * `2` 和 `2.0` 写进 `.data.bin` 是完全不同的 8 个字节：整型按整数写、浮点按
 * IEEE-754 写。所以只要槽位类型是浮点，整数写法（隐式转换来的 `0` / `2`）就补成
 * `0.0` / `2.0` —— 数据条目的正文既是给人看的，也决定了二进制怎么解释。
 *
 * @param node 字面量节点。
 * @param slot 槽位类型（元素 / 字段）。
 * @return 正文里的文本；不是字面量返回空串（调用方走 `emitExpr`）。
 */
Memory::String literalTextFor(const NodePtr &node, const TypePtr &slot)
{
    Memory::String text = literalText(node);
    if (!text.empty() && slot && (slot->kind == Type::Kind::Float) && (text.find_first_of(".eE") == Memory::String::npos))
    {
        text += ".0";  // 隐式转换：整数写法落进浮点槽
    }
    return text;
}

/**
 * @brief 推断表达式类型（字面量 / 变量 / 下标 / 成员）。
 * @param node 待处理的语法节点。
 * @param env 当前类型推导环境。
 * @retval 对应的类型描述对象。
 */
TypePtr infer(const NodePtr &node, const TypeEnv &env)
{
    if (!node) return Type::i32();
    if (node->declared && (node->declared->kind == Kind::Int || node->declared->kind == Kind::Uint || node->declared->kind == Kind::Float))
        return node->declared;
    switch (node->kind)
    {
    case Node::Kind::IntLit: return Type::i32();
    case Node::Kind::FloatLit: return Type::f64();
    case Node::Kind::BoolLit: return Type::boolean();
    case Node::Kind::StringLit: return Type::stringType();
    case Node::Kind::ByteArrayLit: return node->declared;
    case Node::Kind::Var:
    {
        const auto found = env.find(node->text);
        return found == env.end() ? Type::i32() : found->second;
    }
    case Node::Kind::Index:
    {
        const TypePtr target = infer(node->kids.empty() ? nullptr : node->kids[0], env);
        return target->element ? target->element : Type::i32();
    }
    case Node::Kind::CheckIndex:
    case Node::Kind::CheckPointer:
        return infer(node->kids.empty() ? nullptr : node->kids[0], env);  // 值就是下标
    case Node::Kind::LoadInt:
        return node->declared ? node->declared : Type::i64();
    case Node::Kind::Offset:
    {
        const TypePtr target = infer(node->kids.empty() ? nullptr : node->kids[0], env);
        return target->element ? target->element : Type::i32();
    }
    case Node::Kind::Member:
    {
        const TypePtr target = infer(node->kids.empty() ? nullptr : node->kids[0], env);
        for (const auto &field : target->fields)
            if (field.first == node->text) return field.second;
        return Type::i32();
    }
    case Node::Kind::StructNew:
    case Node::Kind::EnumNew:
    {
        // 类型名也放在 env 里（生成前会把声明的复合类型登记进去），这样 `Vec{…} + Vec{…}`
        // 才能查到运算符重载。
        const auto found = env.find(node->typeName.empty() ? node->text : node->typeName);
        return found == env.end() ? Type::i32() : found->second;
    }
    case Node::Kind::Alloc: return Type::i64();  // 堆地址（句柄）
    case Node::Kind::Call:
    {
        // 普通函数 / lambda 的返回类型：generate() 把 `函数标签 → 返回类型` 也放进 env。
        const auto found = env.find(node->text);
        return found == env.end() ? Type::i32() : found->second;
    }
    case Node::Kind::CallPointer:
        return Type::i32();  // 返回类型由调用方（前端）自己掌握
    case Node::Kind::MethodCall:
    {
        // 方法调用：`Type.method` → 返回类型；这样 `(a + b) + c` 才能继续脱糖。
        const TypePtr owner = infer(node->kids.empty() ? nullptr : node->kids[0], env);
        const auto found = env.find(owner->name + "." + node->text);
        return found == env.end() ? Type::i32() : found->second;
    }
    case Node::Kind::Binary:
    {
        const Memory::String &op = node->text;
        const bool compare = (op == "==" || op == "!=" || op == "<" || op == "<=" || op == ">" || op == ">=" || op == "&&" || op == "||");
        if (compare) return Type::boolean();
        // 复合类型参与运算时，结果是运算符方法声明的返回类型（链式重载要用）。
        const TypePtr left = infer(node->kids.empty() ? nullptr : node->kids[0], env);
        for (const auto &item : left->operators)
        {
            if (item.first != op) continue;
            const auto found = env.find(left->name + "." + item.second);
            if (found != env.end()) return found->second;
        }
        const TypePtr right = infer(node->kids.size() < 2u ? nullptr : node->kids[1], env);
        if (left->kind == Kind::Float || right->kind == Kind::Float)
            return Type::make(Kind::Float, std::max(left->kind == Kind::Float ? left->bits : 0,
                                                    right->kind == Kind::Float ? right->bits : 0));
        if ((left->kind == Kind::Int || left->kind == Kind::Uint) && (right->kind == Kind::Int || right->kind == Kind::Uint))
            return Type::make(left->kind == Kind::Uint || right->kind == Kind::Uint ? Kind::Uint : Kind::Int,
                              std::max(left->bits, right->bits));
        return left;
    }
    case Node::Kind::Unary:
    {
        if (node->text == "!") return Type::boolean();
        return infer(node->kids.empty() ? nullptr : node->kids[0], env);
    }
    case Node::Kind::Cast: return node->declared ? node->declared : Type::i32();
    case Node::Kind::Sequence:
        return infer(node->kids.empty() ? nullptr : node->kids.back(), env);
    case Node::Kind::Declare: return node->declared ? node->declared : Type::i32();
    default: return Type::i32();
    }
}

Memory::String emitExpr(const NodePtr &node, Emitter &emitter);
void emitStmt(const NodePtr &node, Emitter &emitter);

/**
 * @brief lambda：抽成独立函数，返回数据区里的闭包句柄。
 * @param node 待处理的语法节点。
 * @param emitter 接收降级指令与诊断的发射器。
 * @retval 生成或读取的结果文本。
 */
Memory::String emitLambda(const NodePtr &node, Emitter &emitter)
{
    const Memory::String name = "lambda_" + Memory::toString(++emitter.lambdaIndex);
    const Memory::Vector<Memory::String> saved = std::move(emitter.lines);
    emitter.lines.clear();
    if (!node->kids.empty()) emitStmt(node->kids[0], emitter);
    emitter.emit("(ret, 0, _, _)");
    Memory::OutputStringStream body;
    body << "{ " << name;
    for (const Memory::String &line : emitter.lines) body << " | " << line;
    body << " }";
    emitter.lines = saved;
    emitter.data(name, body.str(), Type::i32());
    return name;
}

/**
 * @brief 运算符重载：查复合类型的 operators 表，找到就脱糖成方法调用。
 * @param type 类型描述。
 * @param op 运算符文本或类别。
 * @retval 生成或读取的结果文本。
 */
Memory::String overloadTarget(const TypePtr &type, const Memory::String &op)
{
    if (!type) return Memory::String();
    for (const auto &item : type->operators)
        if (item.first == op) return item.second;
    return Memory::String();
}

/**
 * @brief 将表达式节点降级为指令并返回其结果操作数。
 * @param node 待处理的语法节点。
 * @param emitter 接收降级指令与诊断的发射器。
 * @retval 生成或读取的结果文本。
 */
Memory::String emitExpr(const NodePtr &node, Emitter &emitter)
{
    SourceScope source(emitter, node);
    if (!node) return "0";
    const Memory::String literal = literalText(node);
    if (!literal.empty()) return literal;

    switch (node->kind)
    {
    case Node::Kind::Sequence:
        if (node->kids.empty()) return "0";
        for (std::size_t index = 0; index + 1u < node->kids.size(); ++index)
            emitStmt(node->kids[index], emitter);
        return emitExpr(node->kids.back(), emitter);
    case Node::Kind::ByteArrayLit:
    {
        // 单独的键空间避免与内部带尾零的 C 字符串共享数据项。
        const Memory::String key = Memory::String("bytes:") + node->text;
        Memory::String name;
        const auto found = emitter.literalNames.find(key);
        if (found != emitter.literalNames.end())
            name = found->second;
        else
        {
            name = "bytes_" + Memory::toString(++emitter.labelIndex);
            emitter.literalNames.emplace(key, name);
        }
        Memory::String body = "[";
        for (std::size_t index = 0; index < node->text.size(); ++index)
        {
            if (index != 0u) body += ", ";
            body += Memory::toString(static_cast<unsigned char>(node->text[index]));
        }
        body += "]";
        emitter.data(name, body, node->declared);
        const Memory::String address = emitter.temp();
        emitter.emit("(addr, " + name + ", _, " + address + ")");
        return address;
    }
    case Node::Kind::StringLit:
    {
        /* 普通字面量跨函数复用数据项；显式具名数据保留调用方指定的符号身份。 */
        Memory::String name = node->typeName;
        if (name.empty())
        {
            const Memory::String key = Memory::String("cstring:") + node->text;
            const auto found = emitter.literalNames.find(key);
            if (found != emitter.literalNames.end())
                name = found->second;
            else
            {
                name = "str_" + Memory::toString(++emitter.labelIndex);
                emitter.literalNames.emplace(key, name);
            }
        }
        emitter.data(name, "\"" + node->text + "\"", Type::stringType());
        // 字符串在数据区里是一串以 `\0` 结尾的字节；表达式的值是它的**基址**
        // （i64 句柄），这样才能放进变量、当实参传出去、再按地址逐字节取。
        const Memory::String address = emitter.temp();
        emitter.emit("(addr, " + name + ", _, " + address + ")");
        return address;
    }
    case Node::Kind::Var:
        if (emitter.env.find(node->text) == emitter.env.end())
            emitter.error("引用了未声明的变量：" + node->text);
        return node->text;

    case Node::Kind::Binary:
    {
        const NodePtr leftNode = node->kids.size() > 0u ? node->kids[0] : nullptr;
        const NodePtr rightNode = node->kids.size() > 1u ? node->kids[1] : nullptr;
        const TypePtr leftType = infer(leftNode, emitter.env);
        const TypePtr rightType = infer(rightNode, emitter.env);
        const Memory::String op = node->text;
        const bool compare = (op == "==" || op == "!=" || op == "<" || op == "<=" || op == ">" || op == ">=");

        // 1. 引用只允许判空，不做指针运算。
        if ((leftType->kind == Kind::Ref || rightType->kind == Kind::Ref) && !(compare && (op == "==" || op == "!=")))
        {
            emitter.error("引用只支持 == / !=（可空判断），不做指针运算：运算符 " + op);
        }

        // 2. 运算符重载：左操作数是复合类型且声明了该运算符 → `a.add(b)`。
        const Memory::String method = overloadTarget(leftType, op);
        if (!method.empty())
        {
            const Memory::String self = emitExpr(leftNode, emitter);
            const Memory::String other = emitExpr(rightNode, emitter);
            const Memory::String target = emitter.temp();
            emitter.emit("(param, " + self + ", _, self)");
            emitter.emit("(param, " + other + ", _, other)");
            emitter.emit("(call, " + leftType->name + "." + method + ", _, " + target + ")");
            return target;
        }

        // 3. 逻辑短路：右侧表达式必须放在条件跳转之后生成，否则函数调用、
        // 越界检查等副作用会在 `false && rhs` / `true || rhs` 中错误发生。
        if (op == "&&" || op == "||")
        {
            const Memory::String left = emitExpr(leftNode, emitter);
            const Memory::String target = emitter.temp();
            const Memory::String done = emitter.label(op == "&&" ? "and_done" : "or_done");
            const bool conjunction = (op == "&&");
            emitter.emit("(:=, " + Memory::String(conjunction ? "0" : "1") + ", _, " + target + ")");
            emitter.emit("(" + Memory::String(conjunction ? "jz" : "jnz") + ", " + left + ", _, " + done + ")");
            const Memory::String right = emitExpr(rightNode, emitter);
            emitter.emit("(!=:i32, " + right + ", 0, " + target + ")");
            emitter.emit("(label, _, _, " + done + ")");
            return target;
        }

        // 4. 其余内建运算。
        // 比较的类型后缀描述操作数，结果仍为布尔值。
        TypePtr valueType = leftType;
        if (leftType->kind == Kind::Float || rightType->kind == Kind::Float)
        {
            const int leftBits = leftType->kind == Kind::Float ? leftType->bits : 0;
            const int rightBits = rightType->kind == Kind::Float ? rightType->bits : 0;
            valueType = Type::make(Kind::Float, std::max(leftBits, rightBits));
        }
        else if ((leftType->kind == Kind::Int || leftType->kind == Kind::Uint) && (rightType->kind == Kind::Int || rightType->kind == Kind::Uint))
        {
            const Kind kind = (leftType->kind == Kind::Uint || rightType->kind == Kind::Uint) ? Kind::Uint : Kind::Int;
            valueType = Type::make(kind, std::max(leftType->bits, rightType->bits));
        }
        // 混合浮点运算显式转换整数来源，保留 uint64 的无符号语义。
        const bool floating = valueType->kind == Kind::Float;
        const Memory::String left = emitExpr(floating && (leftType->kind == Kind::Int || leftType->kind == Kind::Uint) ? Node::cast(leftNode, valueType) : leftNode, emitter);
        const Memory::String right = emitExpr(floating && (rightType->kind == Kind::Int || rightType->kind == Kind::Uint) ? Node::cast(rightNode, valueType) : rightNode, emitter);
        const Memory::String target = emitter.temp();
        emitter.emit("(" + op + ":" + valueType->quadType() + ", " + left + ", " + right + ", " + target + ")");
        return target;
    }

    case Node::Kind::Unary:
    {
        const NodePtr operandNode = node->kids.empty() ? nullptr : node->kids[0];
        const Memory::String operand = emitExpr(operandNode, emitter);
        const Memory::String target = emitter.temp();
        if (node->text == "!") emitter.emit("(==:i32, " + operand + ", 0, " + target + ")");
        // 一元负号走专门的 `neg`（只有一个操作数）：写成 `(-:i32, x, _, t)` 会被当成二元减法，
        // 第二个操作数缺失，两个后端都会报"缺少操作数"。
        else if (node->text == "-")
            emitter.emit("(neg:" + infer(operandNode, emitter.env)->quadType() + ", " + operand + ", _, " + target + ")");
        else
            emitter.emit("(" + node->text + ":" + infer(operandNode, emitter.env)->quadType() + ", " + operand + ", _, " + target + ")");
        return target;
    }

    case Node::Kind::Cast:
    {
        // 强转就是 `cvt`：位宽 / 符号 / 浮点由目标类型决定（栈机与 transfer 都实现它）。
        const NodePtr source = node->kids.empty() ? nullptr : node->kids[0];
        const Memory::String value = emitExpr(source, emitter);
        const TypePtr targetType = node->declared ? node->declared : Type::i32();
        // 同一数值类型的值已经按该类型产生，不再重复转换。
        const TypePtr sourceType = infer(source, emitter.env);
        const bool numeric = sourceType->kind == Type::Kind::Int || sourceType->kind == Type::Kind::Uint || sourceType->kind == Type::Kind::Float || sourceType->kind == Type::Kind::Bool;
        const bool proven = source && ((source->kind == Node::Kind::Var && emitter.env.count(source->text)) || source->kind == Node::Kind::Cast || source->kind == Node::Kind::Binary || source->kind == Node::Kind::Unary || source->kind == Node::Kind::LoadInt);
        if (numeric && proven && sourceType->quadType() == targetType->quadType()) return value;
        const Memory::String target = emitter.temp();
        emitter.emit("(cvt:" + targetType->quadType() + ", " + value + ", " + sourceType->quadType() + ", " + target + ")");
        return target;
    }

    case Node::Kind::Index:
    {
        const NodePtr baseNode = node->kids.size() > 0u ? node->kids[0] : nullptr;
        const Memory::String base = emitExpr(baseNode, emitter);
        const Memory::String position = emitExpr(node->kids.size() > 1u ? node->kids[1] : nullptr,
                                                 emitter);
        const TypePtr container = infer(baseNode, emitter.env);
        const TypePtr element = container->element ? container->element : Type::i32();
        // u8 连续存储，其他标量或句柄保留八字节槽。
        const Memory::String scaled = emitter.temp();
        emitter.emit("(*:i64, " + position + ", " + Memory::toString(scalarStorageBytes(element)) + ", " + scaled + ")");
        const Memory::String address = emitter.temp();
        emitter.emit("(+:i64, " + base + ", " + scaled + ", " + address + ")");
        // rvalue：算出地址后再取一次值（本 AST 里赋值目标只能是变量名，所以下标总是 rvalue）。
        // `load` 带上**元素类型**：数据区的槽位是 8 字节，整型按整数解释、浮点按
        // IEEE-754 双精度解释（两个后端都认这个后缀）。
        const Memory::String value = emitter.temp();
        emitter.emit("(load:" + element->quadType() + ", " + address + ", _, " + value + ")");
        return value;
    }

    case Node::Kind::Member:
    {
        const NodePtr baseNode = node->kids.empty() ? nullptr : node->kids[0];
        const Memory::String base = emitExpr(baseNode, emitter);
        // 结构体按同样的 8 字节槽位扁平化：偏移 = 字段序号 × 8（顺序即声明顺序）。
        const TypePtr baseType = infer(baseNode, emitter.env);
        std::size_t fieldIndex = 0u;
        for (std::size_t index = 0; index < baseType->fields.size(); ++index)
        {
            if (baseType->fields[index].first == node->text)
            {
                fieldIndex = index;
                break;
            }
        }
        const Memory::String target = emitter.temp();
        emitter.emit("(+:i64, " + base + ", " + Memory::toString(fieldIndex * 8u) + ", " + target + ")");
        // 成员访问同样是 rvalue：取该槽位的值。
        const TypePtr fieldType = (fieldIndex < baseType->fields.size()) ? baseType->fields[fieldIndex].second : Type::i32();
        const Memory::String value = emitter.temp();
        emitter.emit("(load:" + fieldType->quadType() + ", " + target + ", _, " + value + ")");
        return value;
    }

    case Node::Kind::Offset:
    {
        // 按自定义步长算地址，**不取值**：结果是"元素句柄"（结构体数组用）
        const Memory::String base = emitExpr(node->kids.size() > 0u ? node->kids[0] : nullptr, emitter);
        const Memory::String position = emitExpr(node->kids.size() > 1u ? node->kids[1] : nullptr,
                                                 emitter);
        const Memory::String scaled = emitter.temp();
        emitter.emit("(*:i64, " + position + ", " + node->text + ", " + scaled + ")");
        const Memory::String address = emitter.temp();
        emitter.emit("(+:i64, " + base + ", " + scaled + ", " + address + ")");
        return address;
    }

    case Node::Kind::ArrayNew:
    {
        const Memory::String name = "arr_" + Memory::toString(++emitter.labelIndex);
        Memory::OutputStringStream body;
        body << "[";
        // 数据条目的元素类型：`arrayNew` 带了类型就用它（浮点数组要写对描述），
        // 老写法（只给 meta 字符串）退回 i32。
        const TypePtr arrayType = node->declared ? node->declared : Type::array(Type::i32(), node->text);
        const TypePtr elementType = arrayType->element;
        for (std::size_t index = 0; index < node->kids.size(); ++index)
        {
            if (index != 0u) body << ", ";
            const Memory::String item = literalTextFor(node->kids[index], elementType);
            body << (item.empty() ? emitExpr(node->kids[index], emitter) : item);
        }
        body << "]";
        emitter.data(name, body.str(), arrayType);
        // 复合值在四元式里是"地址"：先取数据区里该条目的基址，再返回它。
        const Memory::String address = emitter.temp();
        emitter.emit("(addr, " + name + ", _, " + address + ")");
        return address;
    }

    case Node::Kind::Alloc:
    {
        // 堆分配：字节数先求值，`(alloc, 字节数, _, t)` 把地址放进 t，表达式的值就是 t。
        const Memory::String bytes = emitExpr(node->kids.empty() ? nullptr : node->kids[0], emitter);
        const Memory::String address = emitter.temp();
        emitter.emit("(alloc, " + bytes + ", _, " + address + ")");
        return address;
    }

    case Node::Kind::LoadInt:
    {
        // 按地址读 8 字节：默认按整数读（运行时数组的长度就是这么读的）；带了
        // `declared` 就按那个类型读（`loadFrom`：字段地址 + 字段类型）。
        const Memory::String address = emitExpr(node->kids.empty() ? nullptr : node->kids[0], emitter);
        const Memory::String value = emitter.temp();
        const TypePtr type = node->declared ? node->declared : Type::i64();
        emitter.emit("(load:" + type->quadType() + ", " + address + ", _, " + value + ")");
        return value;
    }

    case Node::Kind::CheckIndex:
    {
        // 下标陷阱：表达式值 = 下标本身，所以能直接当 `index()` / `offset()` 的位置参数用。
        const Memory::String index = emitExpr(node->kids.empty() ? nullptr : node->kids[0], emitter);
        const Memory::String limit = emitExpr(node->kids.size() > 1u ? node->kids[1] : nullptr, emitter);
        emitter.emit("(check, " + index + ", " + limit + ", " + node->text + ")");
        return index;
    }

    case Node::Kind::CheckPointer:
    {
        const Memory::String index = emitExpr(node->kids.empty() ? nullptr : node->kids[0], emitter);
        const Memory::String base = emitExpr(node->kids.size() > 1u ? node->kids[1] : nullptr, emitter);
        emitter.emit("(checkptr, " + index + ", " + base + ", " + Memory::toString(node->intValue) + ", " + node->text + ")");
        return index;
    }

    case Node::Kind::StructNew:
    {
        const Memory::String name = "obj_" + Memory::toString(++emitter.labelIndex);
        Memory::OutputStringStream body;
        body << "{";
        /* 字段类型从类型环境里取（结构体类型在 generate 之前就登记进 env 了），
         * 这样浮点字段里的整数写法也能补成小数点。 */
        const TypePtr selfType = [&] {
            const auto found = emitter.env.find(node->text);
            return found == emitter.env.end() ? TypePtr() : found->second;
        }();
        for (std::size_t index = 0; index < node->kids.size(); ++index)
        {
            if (index != 0u) body << ", ";
            const TypePtr fieldType =
                (selfType && index < selfType->fields.size()) ? selfType->fields[index].second : TypePtr();
            const Memory::String item = literalTextFor(node->kids[index], fieldType);
            body << (item.empty() ? emitExpr(node->kids[index], emitter) : item);
        }
        body << "}";
        emitter.data(name, body.str(), selfType ? selfType : Type::structOf(node->text, {}));
        const Memory::String address = emitter.temp();
        emitter.emit("(addr, " + name + ", _, " + address + ")");
        return address;
    }

    case Node::Kind::EnumNew:
    {
        const Memory::String name = "enum_" + Memory::toString(++emitter.labelIndex);
        // 保留类型名：数据条目要用真实的枚举类型（含分支表），二进制里才能写对 tag。
        const auto declared = emitter.env.find(node->typeName);
        const TypePtr type = (declared == emitter.env.end()) ? Type::enumOf(node->typeName, {}) : declared->second;
        Memory::OutputStringStream body;
        body << node->text << "(";
        for (std::size_t index = 0; index < node->kids.size(); ++index)
        {
            if (index != 0u) body << ", ";
            body << emitExpr(node->kids[index], emitter);
        }
        body << ")";
        emitter.data(name, body.str(), type);
        const Memory::String address = emitter.temp();
        emitter.emit("(addr, " + name + ", _, " + address + ")");
        return address;
    }

    case Node::Kind::Call:
    {
        const Memory::String target = emitter.temp();
        // 先把实参**全部求值**，再发 `param`：`param` 是"挂起参数表"，要是在求值
        // 过程中又碰到一次调用（`f(g(x))`），内层 call 会把外层已经挂上的参数一起
        // 消耗掉 —— 先算完再挂就不会串。
        Memory::Vector<Memory::String> values;
        for (const NodePtr &argument : node->kids) values.push_back(emitExpr(argument, emitter));
        // 形参名：显式给了就逐个用（`param` 是按名字绑定的），没给就沿用旧的 `arg`。
        for (std::size_t index = 0; index < values.size(); ++index)
        {
            const Memory::String parameter = (index < node->names.size() && !node->names[index].empty()) ? node->names[index] : "arg";
            emitter.emit("(param, " + values[index] + ", _, " + parameter + ")");
        }
        emitter.emit("(call, " + node->text + ", _, " + target + ")");
        return target;
    }

    case Node::Kind::CallPointer:
    {
        // 函数指针调用：目标（那个地址）与实参都先求值，再挂 param，最后 `callptr`。
        const NodePtr targetNode = node->kids.empty() ? nullptr : node->kids[0];
        const Memory::String pointer = emitExpr(targetNode, emitter);
        Memory::Vector<Memory::String> values;
        for (std::size_t index = 1; index < node->kids.size(); ++index)
            values.push_back(emitExpr(node->kids[index], emitter));
        for (std::size_t index = 0; index < values.size(); ++index)
        {
            const Memory::String parameter = (index < node->names.size() && !node->names[index].empty()) ? node->names[index] : "arg";
            emitter.emit("(param, " + values[index] + ", _, " + parameter + ")");
        }
        const Memory::String result = emitter.temp();
        emitter.emit("(callptr, " + pointer + ", _, " + result + ")");
        return result;
    }

    case Node::Kind::MethodCall:
    {
        const NodePtr object = node->kids.empty() ? nullptr : node->kids[0];
        const Memory::String target = emitter.temp();
        // 同上：接收者与实参先全部求值，再依次挂 `param`。
        const Memory::String self = emitExpr(object, emitter);
        Memory::Vector<Memory::String> values;
        for (std::size_t index = 1; index < node->kids.size(); ++index)
            values.push_back(emitExpr(node->kids[index], emitter));
        emitter.emit("(param, " + self + ", _, self)");
        for (const Memory::String &value : values)
        {
            emitter.emit("(param, " + value + ", _, arg)");
        }
        const Memory::String owner = infer(object, emitter.env)->name;
        emitter.emit("(call, " + owner + "." + node->text + ", _, " + target + ")");
        return target;
    }

    case Node::Kind::Lambda: return emitLambda(node, emitter);
    default: break;
    }
    return "0";
}

/**
 * @brief 算一个左值（下标 / 成员）的**地址**；`a[i] = v`、`p.x = v` 用它。
 *
 * 与右值使用相同的元素宽度：u8 下标按字节，其他标量或句柄按八字节；
 * 成员 = 基址 + 字段序号×8，只是不再补最后那次 `load`。
 */
Memory::String emitAddress(const NodePtr &node, Emitter &emitter)
{
    if (!node) return "0";
    if (node->kind == Node::Kind::Index)
    {
        const Memory::String base = emitExpr(node->kids.size() > 0u ? node->kids[0] : nullptr, emitter);
        const Memory::String position = emitExpr(node->kids.size() > 1u ? node->kids[1] : nullptr,
                                                 emitter);
        const TypePtr container = infer(node->kids.empty() ? nullptr : node->kids[0], emitter.env);
        const TypePtr element = container->element ? container->element : Type::i32();
        const Memory::String scaled = emitter.temp();
        emitter.emit("(*:i64, " + position + ", " + Memory::toString(scalarStorageBytes(element)) + ", " + scaled + ")");
        const Memory::String address = emitter.temp();
        emitter.emit("(+:i64, " + base + ", " + scaled + ", " + address + ")");
        return address;
    }
    if (node->kind == Node::Kind::Offset)
    {
        // 本身就是地址计算，左值用它时直接复用同一条路径
        const Memory::String base = emitExpr(node->kids.size() > 0u ? node->kids[0] : nullptr, emitter);
        const Memory::String position = emitExpr(node->kids.size() > 1u ? node->kids[1] : nullptr,
                                                 emitter);
        const Memory::String scaled = emitter.temp();
        emitter.emit("(*:i64, " + position + ", " + node->text + ", " + scaled + ")");
        const Memory::String address = emitter.temp();
        emitter.emit("(+:i64, " + base + ", " + scaled + ", " + address + ")");
        return address;
    }
    if (node->kind == Node::Kind::Member)
    {
        const NodePtr baseNode = node->kids.empty() ? nullptr : node->kids[0];
        const Memory::String base = emitExpr(baseNode, emitter);
        const TypePtr baseType = infer(baseNode, emitter.env);
        std::size_t fieldIndex = 0u;
        for (std::size_t index = 0; index < baseType->fields.size(); ++index)
        {
            if (baseType->fields[index].first == node->text)
            {
                fieldIndex = index;
                break;
            }
        }
        const Memory::String address = emitter.temp();
        emitter.emit("(+:i64, " + base + ", " + Memory::toString(fieldIndex * 8u) + ", " + address + ")");
        return address;
    }
    emitter.error("赋值目标只能是变量、下标或成员");
    return "0";
}

/**
 * @brief 将语句节点降级为四元式指令。
 * @param node 待处理的语法节点。
 * @param emitter 接收降级指令与诊断的发射器。
 */
void emitStmt(const NodePtr &node, Emitter &emitter)
{
    SourceScope source(emitter, node);
    if (!node) return;
    switch (node->kind)
    {
    case Node::Kind::Block:
        for (const NodePtr &statement : node->kids) emitStmt(statement, emitter);
        return;
    case Node::Kind::ExprStmt:
        (void)emitExpr(node->kids.empty() ? nullptr : node->kids[0], emitter);
        return;
    case Node::Kind::Assign:
        if (node->kids.size() >= 2u)  // 左值是节点（下标 / 成员）：先算地址再写
        {
            const NodePtr target = node->kids[0];
            const Memory::String value = emitExpr(node->kids[1], emitter);
            if (target && target->kind == Node::Kind::Var)
            {
                emitter.emit("(:=, " + value + ", _, " + target->text + ")");
                return;
            }
            // `store` 带上**元素类型**：数据区槽位是 8 字节，浮点要按 IEEE-754 写成
            // 位模式（整数路径会把 1.5 截成 1，读回来就不是浮点了）。
            const Memory::String storedType = infer(target, emitter.env)->quadType();
            emitter.emit("(store:" + storedType + ", " + emitAddress(target, emitter) + ", " + value + ", _)");
            return;
        }
        emitter.emit("(:=, " + emitExpr(node->kids.empty() ? nullptr : node->kids[0], emitter) + ", _, " + node->text + ")");
        return;
    case Node::Kind::Declare:
    {
        // 带类型的局部变量：先登记类型，再发一条 `(decl, T, local, name)`。
        const TypePtr type = node->declared ? node->declared : Type::i32();
        emitter.env[node->text] = type;
        emitter.emit("(decl, " + type->quadType() + ", local, " + node->text + ")");
        return;
    }
    case Node::Kind::If:
    {
        const NodePtr condition = node->kids.empty() ? nullptr : node->kids[0];
        const NodePtr yes = node->kids.size() > 1u ? node->kids[1] : nullptr;
        const NodePtr no = node->kids.size() > 2u ? node->kids[2] : nullptr;
        if ((!yes && no && no->kind == Node::Kind::Jump) || (!no && yes && yes->kind == Node::Kind::Jump))
        {
            const auto target = yes ? yes : no;
            emitter.emit(Memory::String(yes ? "(jnz, " : "(jz, ") + emitExpr(condition, emitter) + ", _, " + target->text + ")");
            return;
        }
        const Memory::String otherwise = emitter.label("else");
        const Memory::String done = emitter.label("endif");
        emitter.emit("(jz, " + emitExpr(node->kids.empty() ? nullptr : node->kids[0], emitter) + ", _, " + otherwise + ")");
        emitStmt(node->kids.size() > 1u ? node->kids[1] : nullptr, emitter);
        emitter.emit("(j, _, _, " + done + ")");
        emitter.emit("(label, _, _, " + otherwise + ")");
        emitStmt(node->kids.size() > 2u ? node->kids[2] : nullptr, emitter);
        emitter.emit("(label, _, _, " + done + ")");
        return;
    }
    case Node::Kind::While:
    {
        const Memory::String loop = emitter.label("while");
        const Memory::String done = emitter.label("endwhile");
        emitter.emit("(label, _, _, " + loop + ")");
        emitter.emit("(jz, " + emitExpr(node->kids.empty() ? nullptr : node->kids[0], emitter) + ", _, " + done + ")");
        emitStmt(node->kids.size() > 1u ? node->kids[1] : nullptr, emitter);
        emitter.emit("(j, _, _, " + loop + ")");
        emitter.emit("(label, _, _, " + done + ")");
        return;
    }
    case Node::Kind::DoWhile:
    {
        // do body while (cond)：先做一遍 body，再在末尾判条件往回跳
        const Memory::String loop = emitter.label("do");
        emitter.emit("(label, _, _, " + loop + ")");
        emitStmt(node->kids.empty() ? nullptr : node->kids[0], emitter);
        emitter.emit("(jnz, " + emitExpr(node->kids.size() > 1u ? node->kids[1] : nullptr, emitter) + ", _, " + loop + ")");
        return;
    }
    case Node::Kind::Label:
        emitter.emit("(label, _, _, " + node->text + ")");
        return;
    case Node::Kind::Jump:
        emitter.emit("(j, _, _, " + node->text + ")");
        return;
    case Node::Kind::Return:
        emitter.emit("(ret, " + emitExpr(node->kids.empty() ? nullptr : node->kids[0], emitter) + ", _, _)");
        return;
    case Node::Kind::Write:
        emitter.emit("(write, " + emitExpr(node->kids.empty() ? nullptr : node->kids[0], emitter) + ", _, _)");
        return;
    case Node::Kind::Free:
        emitter.emit("(free, " + emitExpr(node->kids.empty() ? nullptr : node->kids[0], emitter) + ", _, _)");
        return;
    case Node::Kind::StoreAt:
    {
        // 地址自己算好的时候写值：`(store:T, 地址, 值, _)`（浮点按位写，整数按整数写）。
        //
        // **值的表达式先求**，地址后求：表达式临时量在本 IR 里是普通变量名（没有
        // "栈帧局部"这回事），递归时同一段代码的临时量会被内层覆盖 —— 先算地址、
        // 再调用（值里含递归调用）的话，回来时那个地址已经被内层改掉了，就会写到
        // 别处去（`node.left = rebuild(...)` 这种写法踩过）。
        const Memory::String value = emitExpr(node->kids.size() > 1u ? node->kids[1] : nullptr, emitter);
        const Memory::String address = emitExpr(node->kids.empty() ? nullptr : node->kids[0], emitter);
        emitter.emit("(store:" + node->text + ", " + address + ", " + value + ", _)");
        return;
    }
    case Node::Kind::Drop:
        // 软释放：地址不是活跃堆块（0 / 数据区 / 已经还过）就什么都不做。
        emitter.emit("(drop, " + emitExpr(node->kids.empty() ? nullptr : node->kids[0], emitter) + ", _, _)");
        return;
    case Node::Kind::Read:
        // `(read, _, _, 变量)`：按顺序从输入取一项，按变量声明的类型收进去。
        emitter.emit("(read, _, _, " + node->text + ")");
        return;
    case Node::Kind::Print:
    {
        // 文本里的换行 / 引号 / 反斜杠按四元式的转义规则写出来（transfer 的 read() 认这些）。
        Memory::String escaped;
        for (const char ch : node->text)
        {
            if (ch == '\n')
                escaped += "\\n";
            else if (ch == '\t')
                escaped += "\\t";
            else if (ch == '"')
                escaped += "\\\"";
            else if (ch == '\\')
                escaped += "\\\\";
            else
                escaped.push_back(ch);
        }
        emitter.emit("(print, \"" + escaped + "\", _, _)");
        return;
    }
    case Node::Kind::Putc:
    {
        // `(putc, v, _, _)`：把 v 的低 8 位当字符码输出（数值转文本的最小手段）。
        emitter.emit("(putc, " + emitExpr(node->kids.empty() ? nullptr : node->kids[0], emitter) + ", _, _)");
        return;
    }
    default:
        (void)emitExpr(node, emitter);
        return;
    }
}

}  // namespace

// ---- 程序与生成 -----------------------------------------------------------

/**
 * @brief 声明一个常量（值必须是字面量）。
 * @param name 名字。
 * @param type 类型。
 * @param literal 字面量原文（例如 `42`、`"hi"`）。
 */
void Program::addConstant(const Memory::String &name, TypePtr type, const Memory::String &literal)
{
    auto stored = Memory::makeShared<Type>(*type);
    stored->meta = literal;  // 常量值暂存在 meta 里，生成时直接展开
    constants.emplace_back(name, stored);
    variables.emplace_back(name, type);
}

/**
 * @brief 声明一个全局变量。
 * @param name 名字。
 * @param type 类型。
 */
void Program::addVariable(const Memory::String &name, TypePtr type)
{
    variables.emplace_back(name, type);
}

/**
 * @brief 给复合类型加一个方法（结构体 / 枚举上的函数）。
 * @param typeName 复合类型名。
 * @param function 方法（`owner` 会自动填成 `typeName`，形参表里请不要重复写 `self`）。
 */
void Program::addMethod(const Memory::String &typeName, Function function)
{
    function.owner = typeName;
    functions.push_back(std::move(function));
    for (TypePtr &type : types)
    {
        if (type->name == typeName) type->methods.push_back(functions.back().name);
    }
}

/**
 * @brief 声明一个运算符重载：`a OP b` 脱糖成 `a.method(b)`。
 * @param typeName 复合类型名。
 * @param op 运算符（`+ - * / % == != < <= > >= []`）。
 * @param methodName 处理这个运算符的方法名。
 */
void Program::addOperator(const Memory::String &typeName, const Memory::String &op,
                          const Memory::String &methodName)
{
    for (TypePtr &type : types)
    {
        if (type->name == typeName) type->operators.emplace_back(op, methodName);
    }
}

/**
 * @brief 把程序落成四元式 + 数据文件。
 * @param program 程序。
 * @param moduleName 模块名（写进文件头注释）。
 * @return 生成结果。
 */
CodegenResult generate(const Program &program, const Memory::String &moduleName)
{
    Emitter emitter;

    /* 先把用户会占用到的名字收齐，内部临时名（`__temp_t<n>`）据此避让。 */
    for (const auto &item : program.variables) emitter.reserved.insert(item.first);
    for (const auto &item : program.constants) emitter.reserved.insert(item.first);
    for (const Function &function : program.functions)
    {
        emitter.reserved.insert(function.name);
        emitter.reserved.insert(function.quadLabel());
        for (const auto &parameter : function.parameters) emitter.reserved.insert(parameter.first);
        collectReservedNames(function.body, emitter.reserved);
    }

    Memory::OutputStringStream quads;
    quads << "# module: " << moduleName << "\n";
    for (const TypePtr &type : program.types) quads << "# type " << type->describe() << "\n";
    for (const TypePtr &type : program.types) emitter.env[type->name] = type;
    // 函数 / 方法的返回类型也进 env：推断 `f(...)` 与 `obj.m(...)` 的结果类型用。
    for (const Function &function : program.functions)
    {
        emitter.env[function.quadLabel()] = function.result ? function.result : Type::i32();
        emitter.env[function.name] = function.result ? function.result : Type::i32();
    }
    for (const auto &item : program.constants)
    {
        emitter.env[item.first] = item.second;
        quads << "# const " << item.first << " : " << item.second->meta << "\n";
    }
    for (const auto &item : program.variables) emitter.env[item.first] = item.second;

    std::size_t quadIndex = 0;
    auto flush = [&](Memory::Vector<Memory::String> &lines) {
        for (const Memory::String &line : lines)
            if (line.rfind("# chali-source:", 0) == 0)
            {
                const auto split = line.find('\n');
                quads << line.substr(0, split + 1) << quadIndex++ << ": " << line.substr(split + 1) << "\n";
            }
            else
                quads << quadIndex++ << ": " << line << "\n";
        lines.clear();
    };

    // 1. 声明 + 常量初始化。
    for (const auto &item : program.variables)
    {
        emitter.emit("(decl, " + item.second->quadType() + ", global, " + item.first + ")");
    }
    flush(emitter.lines);
    for (const auto &item : program.constants)
    {
        emitter.emit("(:=, " + (item.second->meta.empty() ? Memory::String("0") : item.second->meta) + ", _, " + item.first + ")");
    }
    flush(emitter.lines);

    // 2. 函数与方法：方法标签是 `Type.method`，隐式形参 `self` 由调用方 param 进来。
    //
    // 顺序很关键：虚拟机从第 0 条指令开始执行，所以先把入口 `main` 调到最前面——
    // 先发 `(call, main, _, _)` 引导，再排 main 的函数体，其余函数 / 方法跟在后面。
    // （否则方法体排在前面会被当成程序起点，读没绑定的 `self` 直接报错。）
    Memory::Vector<const Function *> ordered;
    for (const Function &function : program.functions)
    {
        if (function.name == "main")
            ordered.insert(ordered.begin(), &function);
        else
            ordered.push_back(&function);
    }
    const bool hasMain = !ordered.empty() && ordered.front()->name == "main";
    if (hasMain)
    {
        // 引导：调用 main；`main` 返回后必须立刻停机，否则 pc 会落回函数体再跑一遍。
        quads << quadIndex++ << ": (call, main, _, _)\n";
        quads << quadIndex++ << ": (halt, _, _, _)\n";
    }

    for (const Function *entry : ordered)
    {
        const Function &function = *entry;
        for (const auto &parameter : function.parameters)
        {
            emitter.env[parameter.first] = parameter.second;
            emitter.emit("(decl, " + parameter.second->quadType() + ", local, " + parameter.first + ")");
        }
        if (!function.owner.empty())
        {
            emitter.emit("(decl, i64, local, self)");
            const auto owner = emitter.env.find(function.owner);
            emitter.env["self"] = (owner == emitter.env.end()) ? Type::i64() : owner->second;
        }
        emitter.emit("(label, _, _, " + function.quadLabel() + ")");
        emitStmt(function.body, emitter);
        emitter.emit("(ret, 0, _, _)");
        flush(emitter.lines);
    }

    quads << "(halt, _, _, _)\n";
    emitter.result.quads = quads.str();
    emitter.result.data = "# simple_ast data: " + moduleName + "\n";
    for (const Memory::String &line : emitter.dataLines) emitter.result.data += line + "\n";
    return emitter.result;
}

}  // namespace simple_ast
