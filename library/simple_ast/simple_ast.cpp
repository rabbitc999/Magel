/**
 * @file simple_ast.cpp
 * @brief simple_ast.hpp 的实现：类型打印、AST 构造、四元式 / `.data` 生成。
 */

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

// ---- .data 二进制导出的小工具 --------------------------------------------

/**
 * @brief 从 meta 里取 `key=数字`；没有就返回 fallback。
 * @param meta 类型布局的附加元数据。
 * @param key 查找或写入使用的键。
 * @param fallback 无法取得目标值时使用的默认值。
 * @retval 元数据中指定键的整数值；缺失时为 fallback。
 */
int metaInt(const Memory::String &meta, const Memory::String &key, int fallback)
{
    const std::size_t at = meta.find(key + "=");
    if (at == Memory::String::npos) return fallback;
    return std::atoi(meta.c_str() + at + key.size() + 1u);
}

/**
 * @brief 追加小端整数（8 字节）。
 * @param out 接收处理结果的输出对象。
 * @param value 待转换、保存或参与计算的值。
 */
void putInt(Memory::String &out, std::int64_t value)
{
    const std::uint64_t bits = static_cast<std::uint64_t>(value);
    for (int index = 0; index < 8; ++index) out.push_back(static_cast<char>((bits >> (index * 8)) & 0xFFu));
}

/**
 * @brief 追加小端 32 位无符号（4 字节）。
 * @param out 接收处理结果的输出对象。
 * @param value 待转换、保存或参与计算的值。
 */
void putU32(Memory::String &out, std::uint32_t value)
{
    for (int index = 0; index < 4; ++index) out.push_back(static_cast<char>((value >> (index * 8)) & 0xFFu));
}

/**
 * @brief 追加小端双精度（8 字节）。
 * @param out 接收处理结果的输出对象。
 * @param value 待转换、保存或参与计算的值。
 */
void putDouble(Memory::String &out, double value)
{
    std::uint64_t bits = 0;
    static_assert(sizeof(bits) == sizeof(value), "double 必须是 8 字节");
    std::memcpy(&bits, &value, sizeof(bits));
    for (int index = 0; index < 8; ++index) out.push_back(static_cast<char>((bits >> (index * 8)) & 0xFFu));
}

/**
 * @brief 文本是不是带引号的字符串。
 * @param text 待处理的文本。
 * @retval 满足检查条件或操作完成时为 true，检查失败或无法完成时为 false。
 */
bool isQuoted(const Memory::String &text)
{
    return text.size() >= 2u && text.front() == '"' && text.back() == '"';
}

/**
 * @brief 切顶层逗号（去括号，尊重引号）。
 * @param body 待处理的函数体或循环体。
 * @retval 生成或读取的结果文本。
 */
Memory::Vector<Memory::String> splitTop(const Memory::String &body)
{
    // 先把最外层的 [] / {} / () 剥掉：它们让内容停在 depth=1，
    // 而下面的分隔判断只在 depth==0 生效——不剥就会把 `[10, 20, 30, 40]`
    // 当成一个元素（这正是数组在 .data 里只写 4 字节的原因）。
    Memory::String text = body;
    if (text.size() >= 2u)
    {
        const char front = text.front();
        const char back = text.back();
        const bool wrapped = (front == '[' && back == ']') || (front == '{' && back == '}') || (front == '(' && back == ')');
        if (wrapped) text = text.substr(1u, text.size() - 2u);
    }

    Memory::Vector<Memory::String> parts;
    Memory::String current;
    int depth = 0;
    bool quoted = false;
    for (const char ch : text)
    {
        if (ch == '"') quoted = !quoted;
        if (!quoted)
        {
            if (ch == '[' || ch == '{' || ch == '(')
            {
                ++depth;
                continue;
            }
            if (ch == ']' || ch == '}' || ch == ')')
            {
                --depth;
                continue;
            }
            if (ch == ',' && depth == 0)
            {
                parts.push_back(current);
                current.clear();
                continue;
            }
        }
        current += ch;
    }
    if (!current.empty()) parts.push_back(current);
    return parts;
}

/**
 * @brief 解析数字文本。
 * @param text 待处理的文本。
 * @param isFloat 是否按浮点数表示读写数据。
 * @param intValue 整数形式的数值。
 * @param doubleValue 浮点形式的数值。
 * @retval 满足检查条件或操作完成时为 true，检查失败或无法完成时为 false。
 */
bool numberText(const Memory::String &text, bool &isFloat, std::int64_t &intValue, double &doubleValue)
{
    if (text.empty()) return false;
    char *end = nullptr;
    doubleValue = std::strtod(text.c_str(), &end);
    if (end == nullptr || *end != '\0') return false;
    isFloat = (text.find_first_of(".eE") != Memory::String::npos);
    intValue = static_cast<std::int64_t>(doubleValue);
    return true;
}

/**
 * 生成期状态。
 */
struct Emitter
{
    CodegenResult result;
    TypeEnv env;
    Memory::Vector<Memory::String> dataLines;
    Memory::Vector<Memory::String> lines;
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
            marker << "# magel-source: " << sourceLine << " " << std::quoted(sourceFile) << "\n";
            lines.push_back(marker.str() + text);
            hasSourceMarker = true;
        }
        else if (hasSourceMarker)
        {
            lines.push_back("# magel-source: 0 \"\"\n" + text);
            hasSourceMarker = false;
        }
        else
            lines.push_back(text);
    }
    /**
     * @brief 分配一个新的临时变量名称。
     * @retval 生成或读取的结果文本。
     */
    Memory::String temp() { return "t" + Memory::toString(++tempIndex); }
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
    case Node::Kind::StringLit:
    {
        /* 具名的（`namedStr`）沿用给的名字 → 同名只有一份数据项；普通的每次新开一个。 */
        const Memory::String name = node->typeName.empty() ? ("str_" + Memory::toString(++emitter.labelIndex)) : node->typeName;
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
        // 降级成"基址 + 偏移"：每个元素一个 8 字节槽位 → 先 位置×8，再加基址。
        const Memory::String scaled = emitter.temp();
        emitter.emit("(*:i64, " + position + ", 8, " + scaled + ")");
        const Memory::String address = emitter.temp();
        emitter.emit("(+:i64, " + base + ", " + scaled + ", " + address + ")");
        // rvalue：算出地址后再取一次值（本 AST 里赋值目标只能是变量名，所以下标总是 rvalue）。
        // `load` 带上**元素类型**：数据区的槽位是 8 字节，整型按整数解释、浮点按
        // IEEE-754 双精度解释（两个后端都认这个后缀）。
        const TypePtr container = infer(baseNode, emitter.env);
        const TypePtr element = container->element ? container->element : Type::i32();
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
 * 与 `emitExpr` 里 rvalue 的地址计算逐字一致：下标 = 基址 + 位置×8，
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
        const Memory::String scaled = emitter.temp();
        emitter.emit("(*:i64, " + position + ", 8, " + scaled + ")");
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

// ---- 类型 -----------------------------------------------------------------

/**
 * @brief 根据类型类别与位宽创建类型描述。
 * @param kind 待处理对象的类别。
 * @param bits 数值类型的位宽。
 * @retval 对应的类型描述对象。
 */
TypePtr Type::make(Kind kind, int bits)
{
    auto type = Memory::makeShared<Type>();
    type->kind = kind;
    type->bits = bits;
    return type;
}

/**
 * @brief 构造有符号整数类型，位宽为 8。
 * @retval 对应的类型描述对象。
 */
TypePtr Type::i8() { return make(Kind::Int, 8); }
/**
 * @brief 构造有符号整数类型，位宽为 16。
 * @retval 对应的类型描述对象。
 */
TypePtr Type::i16() { return make(Kind::Int, 16); }
/**
 * @brief 构造有符号整数类型，位宽为 32。
 * @retval 对应的类型描述对象。
 */
TypePtr Type::i32() { return make(Kind::Int, 32); }
/**
 * @brief 构造有符号整数类型，位宽为 64。
 * @retval 对应的类型描述对象。
 */
TypePtr Type::i64() { return make(Kind::Int, 64); }
/**
 * @brief 构造无符号整数类型，位宽为 8。
 * @retval 对应的类型描述对象。
 */
TypePtr Type::u8() { return make(Kind::Uint, 8); }
/**
 * @brief 构造无符号整数类型，位宽为 16。
 * @retval 对应的类型描述对象。
 */
TypePtr Type::u16() { return make(Kind::Uint, 16); }
/**
 * @brief 构造无符号整数类型，位宽为 32。
 * @retval 对应的类型描述对象。
 */
TypePtr Type::u32() { return make(Kind::Uint, 32); }
/**
 * @brief 构造无符号整数类型，位宽为 64。
 * @retval 对应的类型描述对象。
 */
TypePtr Type::u64() { return make(Kind::Uint, 64); }
/**
 * @brief 构造浮点类型，位宽为 32。
 * @retval 对应的类型描述对象。
 */
TypePtr Type::f32() { return make(Kind::Float, 32); }
/**
 * @brief 构造浮点类型，位宽为 64。
 * @retval 对应的类型描述对象。
 */
TypePtr Type::f64() { return make(Kind::Float, 64); }
/**
 * @brief 构造布尔类型描述。
 * @retval 对应的类型描述对象。
 */
TypePtr Type::boolean() { return make(Kind::Bool, 1); }
/**
 * @brief 构造字符串类型描述。
 * @retval 对应的类型描述对象。
 */
TypePtr Type::stringType() { return make(Kind::String, 0); }

/**
 * @brief 根据元素类型及布局信息构造数组类型。
 * @param element 数组或容器的元素类型。
 * @param meta 类型布局的附加元数据。
 * @retval 对应的类型描述对象。
 */
TypePtr Type::array(TypePtr element, Memory::String meta)
{
    auto type = make(Kind::Array);
    type->element = std::move(element);
    type->meta = std::move(meta);
    return type;
}

/**
 * @brief 根据目标类型构造引用类型。
 * @param target 待访问或修改的目标。
 * @retval 对应的类型描述对象。
 */
TypePtr Type::ref(TypePtr target)
{
    auto type = make(Kind::Ref);
    type->element = std::move(target);
    type->meta = "nullable";
    return type;
}

/**
 * @brief 根据字段列表构造命名结构体类型。
 * @param name 名称文本。
 * @param fields 结构体字段定义或字段初始化值列表。
 * @retval 对应的类型描述对象。
 */
TypePtr Type::structOf(Memory::String name, Memory::Vector<std::pair<Memory::String, TypePtr>> fields)
{
    auto type = make(Kind::Struct);
    type->name = std::move(name);
    type->fields = std::move(fields);
    type->meta = "value";
    return type;
}

/**
 * @brief 根据变体列表构造命名枚举类型。
 * @param name 名称文本。
 * @param variants 枚举变体声明列表。
 * @retval 对应的类型描述对象。
 */
TypePtr Type::enumOf(Memory::String name, Memory::Vector<std::pair<Memory::String, TypePtr>> variants)
{
    auto type = make(Kind::Enum);
    type->name = std::move(name);
    type->variants = std::move(variants);
    type->meta = "tagged";
    return type;
}

/**
 * @brief 构造泛型类型参数的类型描述。
 * @param name 名称文本。
 * @retval 对应的类型描述对象。
 */
TypePtr Type::generic(Memory::String name)
{
    auto type = make(Kind::Generic);
    type->name = std::move(name);
    type->meta = "generic";
    return type;
}

/**
 * @brief 取得类型对应的四元式类型名称。
 * @retval 生成或读取的结果文本。
 */
Memory::String Type::quadType() const
{
    switch (kind)
    {
    case Kind::Bool: return "i8";
    case Kind::Int: return "i" + Memory::toString(bits);
    case Kind::Uint: return "u" + Memory::toString(bits);
    case Kind::Float: return "f" + Memory::toString(bits);
    default: return "i64";  // 字符串 / 数组 / 引用 / 结构体 / 枚举 / 泛型：i64 句柄
    }
}

/**
 * @brief 生成当前类型的可读描述。
 * @retval 生成或读取的结果文本。
 */
Memory::String Type::describe() const
{
    Memory::String text;
    switch (kind)
    {
    case Kind::Bool: text = "bool"; break;
    case Kind::Int: text = "i" + Memory::toString(bits); break;
    case Kind::Uint: text = "u" + Memory::toString(bits); break;
    case Kind::Float: text = "f" + Memory::toString(bits); break;
    case Kind::String: text = "string"; break;
    case Kind::Generic: text = "generic<" + name + ">"; break;
    case Kind::Array: text = "array<" + (element ? element->describe() : "?") + ">"; break;
    case Kind::Ref: text = "ref<" + (element ? element->describe() : "?") + (meta == "nonnull" ? ">" : ">?"); break;
    case Kind::Struct:
        text = "struct " + name + " {";
        for (std::size_t index = 0; index < fields.size(); ++index)
        {
            if (index != 0u) text += ", ";
            text += fields[index].first + ": " + fields[index].second->describe();
        }
        if (!methods.empty())
        {
            text += " | methods:";
            for (const Memory::String &method : methods) text += " " + method;
        }
        if (!operators.empty())
        {
            text += " | operators:";
            for (const auto &item : operators) text += " " + item.first + "→" + item.second;
        }
        text += "}";
        break;
    case Kind::Enum:
        text = "enum " + name + " {";
        for (std::size_t index = 0; index < variants.size(); ++index)
        {
            if (index != 0u) text += ", ";
            text += variants[index].first;
            if (variants[index].second) text += "(" + variants[index].second->describe() + ")";
        }
        text += "}";
        break;
    default: text = "void"; break;
    }
    if (!meta.empty()) text += "{" + meta + "}";
    return text;
}

// ---- 节点 -----------------------------------------------------------------

/**
 * @brief 构造整数常量对应的低层语法节点。
 * @param value 待转换、保存或参与计算的值。
 * @retval 生成的低层语法节点。
 */
NodePtr Node::i(std::int64_t value)
{
    auto node = Memory::makeShared<Node>();
    node->kind = Kind::IntLit;
    node->intValue = value;
    return node;
}

/**
 * @brief 构造浮点常量对应的低层语法节点。
 * @param value 待转换、保存或参与计算的值。
 * @retval 生成的低层语法节点。
 */
NodePtr Node::f(double value)
{
    auto node = Memory::makeShared<Node>();
    node->kind = Kind::FloatLit;
    node->floatValue = value;
    return node;
}

/**
 * @brief 构造字符串常量对应的低层语法节点。
 * @param text 待处理的文本。
 * @retval 生成的低层语法节点。
 */
NodePtr Node::str(Memory::String text)
{
    auto node = Memory::makeShared<Node>();
    node->kind = Kind::StringLit;
    node->text = std::move(text);
    return node;
}

/**
 * @brief 构造布尔常量对应的低层语法节点。
 * @param value 待转换、保存或参与计算的值。
 * @retval 生成的低层语法节点。
 */
NodePtr Node::boolean(bool value)
{
    auto node = Memory::makeShared<Node>();
    node->kind = Kind::BoolLit;
    node->intValue = value ? 1 : 0;
    return node;
}

/**
 * @brief 构造空值对应的低层语法节点。
 * @retval 生成的低层语法节点。
 */
NodePtr Node::null()
{
    auto node = Memory::makeShared<Node>();
    node->kind = Kind::NullLit;
    return node;
}

/**
 * @brief 构造变量引用对应的低层语法节点。
 * @param name 名称文本。
 * @retval 生成的低层语法节点。
 */
NodePtr Node::var(Memory::String name)
{
    auto node = Memory::makeShared<Node>();
    node->kind = Kind::Var;
    node->text = std::move(name);
    return node;
}

/**
 * @brief 构造二元运算对应的低层语法节点。
 * @param op 运算符文本或类别。
 * @param left 左操作数。
 * @param right 右操作数。
 * @retval 生成的低层语法节点。
 */
NodePtr Node::binary(Memory::String op, NodePtr left, NodePtr right)
{
    auto node = Memory::makeShared<Node>();
    node->kind = Kind::Binary;
    node->text = std::move(op);
    node->kids = { std::move(left), std::move(right) };
    return node;
}

/**
 * @brief 构造顺序求值对应的低层语法节点。
 * @param statements 待处理的语句列表。
 * @param value 待转换、保存或参与计算的值。
 * @retval 生成的低层语法节点。
 */
NodePtr Node::sequence(Memory::Vector<NodePtr> statements, NodePtr value)
{
    NodePtr node = Memory::makeShared<Node>();
    node->kind = Kind::Sequence;
    node->kids = std::move(statements);
    node->kids.push_back(std::move(value));
    return node;
}

/**
 * @brief 构造一元运算对应的低层语法节点。
 * @param op 运算符文本或类别。
 * @param operand 一元运算的操作数。
 * @retval 生成的低层语法节点。
 */
NodePtr Node::unary(Memory::String op, NodePtr operand)
{
    auto node = Memory::makeShared<Node>();
    node->kind = Kind::Unary;
    node->text = std::move(op);
    node->kids = { std::move(operand) };
    return node;
}

/**
 * @brief 构造索引访问对应的低层语法节点。
 * @param target 待访问或修改的目标。
 * @param position 目标位置或源码位置。
 * @retval 生成的低层语法节点。
 */
NodePtr Node::index(NodePtr target, NodePtr position)
{
    auto node = Memory::makeShared<Node>();
    node->kind = Kind::Index;
    node->kids = { std::move(target), std::move(position) };
    return node;
}

/**
 * @brief 按**自定义步长**算元素地址（不取值）：`base + position × stride`。
 *
 * `index` 固定按 8 字节槽位取值，适合标量数组；元素是结构体时每个元素占
 * `字段数 × 8` 字节、而表达式要的是**句柄**（地址）而不是取出来的值，
 * 这时用 `offset`（结构体数组 `arr[i].f` 就靠它）。
 * @param base 起始地址、基值或目标表达式。
 * @param position 目标位置或源码位置。
 * @param stride 相邻元素之间的字节步长。
 * @retval 生成的低层语法节点。
 */
NodePtr Node::offset(NodePtr base, NodePtr position, std::int64_t stride)
{
    auto node = Memory::makeShared<Node>();
    node->kind = Kind::Offset;
    node->text = Memory::toString(stride);
    node->kids = { std::move(base), std::move(position) };
    return node;
}

/**
 * @brief 构造成员访问对应的低层语法节点。
 * @param target 待访问或修改的目标。
 * @param field 待访问的字段名称或描述。
 * @retval 生成的低层语法节点。
 */
NodePtr Node::member(NodePtr target, Memory::String field)
{
    auto node = Memory::makeShared<Node>();
    node->kind = Kind::Member;
    node->text = std::move(field);
    node->kids = { std::move(target) };
    return node;
}

/**
 * @brief 构造函数调用对应的低层语法节点。
 * @param callee 被调用函数的名称或表达式。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 生成的低层语法节点。
 */
NodePtr Node::call(Memory::String callee, Memory::Vector<NodePtr> args)
{
    auto node = Memory::makeShared<Node>();
    node->kind = Kind::Call;
    node->text = std::move(callee);
    node->kids = std::move(args);
    return node;
}

/**
 * @brief 带形参名的调用：第 i 个实参绑定到 `parameterNames[i]`。
 *
 * 四元式里 `(param, 实参, _, 形参名)` 是**按名字**绑定的（见 stackvm 的 `Op::Call`），
 * 所以多参数调用必须把 callee 的形参名逐个写出来；只写 `arg` 会让所有实参挤在同一个槽。
 * 方法调用也可以走这里：形参名写 `self` + 各实参名，callee 写 `Type.方法名`。
 * @param callee 被调用函数的名称或表达式。
 * @param parameterNames 形参名称列表。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 生成的低层语法节点。
 */
NodePtr Node::call(Memory::String callee, Memory::Vector<Memory::String> parameterNames,
                   Memory::Vector<NodePtr> args)
{
    NodePtr node = call(std::move(callee), std::move(args));
    node->names = std::move(parameterNames);
    return node;
}

/**
 * @brief 构造方法调用对应的低层语法节点。
 * @param object 待访问的对象。
 * @param method 压缩方法编号或目标方法名称。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 生成的低层语法节点。
 */
NodePtr Node::methodCall(NodePtr object, Memory::String method, Memory::Vector<NodePtr> args)
{
    auto node = Memory::makeShared<Node>();
    node->kind = Kind::MethodCall;
    node->text = std::move(method);
    node->kids.push_back(std::move(object));
    for (NodePtr &argument : args) node->kids.push_back(std::move(argument));
    return node;
}

/**
 * @brief 构造闭包函数对应的低层语法节点。
 * @param body 待处理的函数体或循环体。
 * @retval 生成的低层语法节点。
 */
NodePtr Node::lambda(NodePtr body)
{
    auto node = Memory::makeShared<Node>();
    node->kind = Kind::Lambda;
    if (body) node->kids = { std::move(body) };
    return node;
}

/**
 * @brief 构造数组构造对应的低层语法节点。
 * @param type 类型描述。
 * @param items 待保存的元素集合。
 * @retval 生成的低层语法节点。
 */
NodePtr Node::arrayNew(TypePtr type, Memory::Vector<NodePtr> items)
{
    auto node = Memory::makeShared<Node>();
    node->kind = Kind::ArrayNew;
    node->text = type ? type->meta : Memory::String();
    node->declared = type;  // 元素类型进 .data 的描述（浮点数组也要写对）
    node->kids = std::move(items);
    return node;
}

/**
 * @brief 构造结构体构造对应的低层语法节点。
 * @param typeName 目标类型的名称。
 * @param fields 结构体字段定义或字段初始化值列表。
 * @retval 生成的低层语法节点。
 */
NodePtr Node::structNew(Memory::String typeName, Memory::Vector<NodePtr> fields)
{
    auto node = Memory::makeShared<Node>();
    node->kind = Kind::StructNew;
    node->text = std::move(typeName);
    node->kids = std::move(fields);
    return node;
}

/**
 * @brief 构造枚举变体构造对应的低层语法节点。
 * @param typeName 目标类型的名称。
 * @param variant 枚举变体名称或定义。
 * @param payload 枚举载荷或协议消息正文。
 * @retval 生成的低层语法节点。
 */
NodePtr Node::enumNew(Memory::String typeName, Memory::String variant, Memory::Vector<NodePtr> payload)
{
    auto node = Memory::makeShared<Node>();
    node->kind = Kind::EnumNew;
    node->text = std::move(variant);
    node->typeName = std::move(typeName);  // 保留类型名（二进制 tag / 类型推断都要用）
    node->kids = std::move(payload);
    return node;
}

/**
 * @brief 堆分配：`(alloc, 字节数, _, t)`，表达式的值是拿到的地址（`i64`）。
 *
 * 与 `arrayNew` / `structNew` 的区别：那两个把对象放进**数据区**（每个声明点一块、
 * 静态布局），这里要的是**运行期才知道大小**的对象，走虚拟机的堆；什么时候还回去
 * 由程序自己 `freeMemory`（栈机那边是 `(free, 地址, _, _)`）。返回 0 表示没分配到。
 * @param bytes 内存区域或数据的字节数。
 * @retval 生成的低层语法节点。
 */
NodePtr Node::alloc(NodePtr bytes)
{
    auto node = Memory::makeShared<Node>();
    node->kind = Kind::Alloc;
    if (bytes) node->kids = { std::move(bytes) };
    return node;
}

/**
 * @brief 按地址读一个**指定类型**的值：`(load:T, 地址, _, t)`。
 *
 * 与 `loadInt` 的区别是"这 8 个字节怎么解释"由 `type` 决定（`f64` 按 IEEE-754
 * 读，`i64` 按整数读）。字段访问用它：字段地址是自己按槽位算出来的，不能走
 * `member()` 那种"按名字在基类型里查下标"的路（基类型是内层结构体时会查错）。
 * @param address 地址（i64 表达式）。
 * @param type 值的类型。
 * @return 表达式节点。
 */
NodePtr Node::loadFrom(NodePtr address, TypePtr type)
{
    auto node = Memory::makeShared<Node>();
    node->kind = Kind::LoadInt;
    node->declared = type ? std::move(type) : Type::i64();
    if (address) node->kids = { std::move(address) };
    return node;
}

/**
 * @brief 往一个**算好的地址**写值：`(store:T, 地址, 值, _)`，当语句用。
 *
 * `assign(左值, 值)` 只认"变量 / 下标 / 成员"三种左值；地址已经自己算好（比如
 * 结构体字段的槽位偏移）时用这一条。
 * @param address 地址。
 * @param value 值。
 * @param type 值的类型（决定浮点按位写还是整数写）。
 * @return 语句节点。
 */
NodePtr Node::storeAt(NodePtr address, NodePtr value, TypePtr type)
{
    auto node = Memory::makeShared<Node>();
    node->kind = Kind::StoreAt;
    node->text = (type ? type : Type::i64())->quadType();  // 写的时候按这个类型写
    if (address) node->kids.push_back(std::move(address));
    if (value) node->kids.push_back(std::move(value));
    return node;
}

/**
 * @brief **软释放**：`(drop, 地址, _, _)`，当语句用。
 *
 * 与 `freeMemory` 的区别是"还不了就当没这回事"：地址是 0（空指针）、指向数据区
 * （静态字面量）、或者那块堆内存**已经还过**了，都不算错。编译器在语句块出口插的
 * 自动释放走这一条（见 magel 的"生命周期"），显式 `delete` 仍用 `freeMemory`。
 * @param pointer 待释放或校验的指针。
 * @retval 生成的低层语法节点。
 */
NodePtr Node::release(NodePtr pointer)
{
    auto node = Memory::makeShared<Node>();
    node->kind = Kind::Drop;
    node->kids.push_back(std::move(pointer));
    return node;
}

/**
 * @brief 构造内存释放对应的低层语法节点。
 * @param pointer 待释放或校验的指针。
 * @retval 生成的低层语法节点。
 */
NodePtr Node::freeMemory(NodePtr pointer)
{
    auto node = Memory::makeShared<Node>();
    node->kind = Kind::Free;
    if (pointer) node->kids = { std::move(pointer) };
    return node;
}

/**
 * @brief 间接调用（函数指针）：`(param …)` + `(callptr, 函数指针, _, 目标)`。
 *
 * `target` 的值是"数据区里存着函数标签的那串字节"的地址（0 = 空指针）。
 * 栈机侧实参**按位置**绑定：位置表由组装时从"函数标签前面那段 decl"扫出来，
 * 所以这里的 `parameterNames` 只是占位（走 `(param, 值, _, 名字)` 的格式而已）。
 * @param target 待访问或修改的目标。
 * @param parameterNames 形参名称列表。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 生成的低层语法节点。
 */
NodePtr Node::callPointer(NodePtr target, Memory::Vector<Memory::String> parameterNames,
                          Memory::Vector<NodePtr> args)
{
    auto node = Memory::makeShared<Node>();
    node->kind = Kind::CallPointer;
    if (target) node->kids.push_back(std::move(target));
    for (NodePtr &argument : args) node->kids.push_back(std::move(argument));
    node->names = std::move(parameterNames);
    return node;
}

/**
 * @brief **具名**字符串数据项：同名只登记一次，表达式的值是它的地址。
 *
 * `str()` 每次发射都新开一个数据项（同一个内容出现两次就两份）；要"同一个东西
 * 地址始终一致"就得用这个（编译器给函数指针发的"函数标签"就是它 —— 于是
 * `p == 函数名` 能按地址比）。
 * @param name 数据项名字（要唯一地对应一份内容）。
 * @param text 内容（不含引号）。
 * @retval 生成的低层语法节点。
 */
NodePtr Node::namedStr(Memory::String name, Memory::String text)
{
    auto node = Memory::makeShared<Node>();
    node->kind = Kind::StringLit;
    node->text = std::move(text);
    node->typeName = std::move(name);  // 数据项名字（普通字符串字面量这里为空）
    return node;
}

/**
 * @brief 按地址读 8 个字节，当**整数**用（`(load:i64, 地址, _, t)`）。
 *
 * `index()` 的取值宽度跟着**基址的元素类型**走（浮点元素就按 IEEE-754 解释），
 * 而"运行时数组的长度"这类值必须按整数读 —— 所以单独给一个原语。
 * @param address 运行时内存地址。
 * @retval 生成的低层语法节点。
 */
NodePtr Node::loadInt(NodePtr address)
{
    auto node = Memory::makeShared<Node>();
    node->kind = Kind::LoadInt;
    if (address) node->kids = { std::move(address) };
    return node;
}

/**
 * @brief 下标陷阱：`(check, 下标, 上限, 标签)`，越界就终止运行。
 *
 * 表达式的值就是**下标本身**，所以可以直接塞进 `index()` / `offset()` 的位置参数。
 * @param index 待访问的元素索引。
 * @param limit 索引、步数或资源使用的上限。
 * @param label 诊断或跳转使用的标签。
 * @retval 生成的低层语法节点。
 */
NodePtr Node::checkIndex(NodePtr index, NodePtr limit, Memory::String label)
{
    auto node = Memory::makeShared<Node>();
    node->kind = Kind::CheckIndex;
    node->text = std::move(label);
    if (index) node->kids.push_back(std::move(index));
    node->kids.push_back(std::move(limit));
    return node;
}

/**
 * @brief 按区块容量查下标：`(checkptr, 下标, 基址, 步长, 标签)`。
 *
 * 校验"第 index 个元素（`基址 + index × 步长`，共 `步长` 字节）"落在基址所在的
 * **那一块**里（堆块按分配大小、数据区按符号 `size=`）；基址不在任何一块里也报错。
 * @param index 待访问的元素索引。
 * @param base 起始地址、基值或目标表达式。
 * @param stride 相邻元素之间的字节步长。
 * @param label 诊断或跳转使用的标签。
 * @retval 生成的低层语法节点。
 */
NodePtr Node::checkPointer(NodePtr index, NodePtr base, std::int64_t stride, Memory::String label)
{
    auto node = Memory::makeShared<Node>();
    node->kind = Kind::CheckPointer;
    node->text = std::move(label);
    node->intValue = stride;
    if (index) node->kids.push_back(std::move(index));
    node->kids.push_back(std::move(base));
    return node;
}

/**
 * @brief 构造赋值对应的低层语法节点。
 * @param target 待访问或修改的目标。
 * @param value 待转换、保存或参与计算的值。
 * @retval 生成的低层语法节点。
 */
NodePtr Node::assign(Memory::String target, NodePtr value)
{
    auto node = Memory::makeShared<Node>();
    node->kind = Kind::Assign;
    node->text = std::move(target);
    node->kids = { std::move(value) };
    return node;
}

/**
 * @brief 构造赋值对应的低层语法节点。
 * @param target 待访问或修改的目标。
 * @param value 待转换、保存或参与计算的值。
 * @retval 生成的低层语法节点。
 */
NodePtr Node::assign(NodePtr target, NodePtr value)
{
    auto node = Memory::makeShared<Node>();
    node->kind = Kind::Assign;
    node->kids = { std::move(target), std::move(value) };
    return node;
}

/**
 * @brief 构造变量声明对应的低层语法节点。
 * @param name 名称文本。
 * @param type 类型描述。
 * @retval 生成的低层语法节点。
 */
NodePtr Node::declare(Memory::String name, TypePtr type)
{
    auto node = Memory::makeShared<Node>();
    node->kind = Kind::Declare;
    node->text = std::move(name);
    node->declared = std::move(type);
    return node;
}

/**
 * @brief 构造类型转换对应的低层语法节点。
 * @param value 待转换、保存或参与计算的值。
 * @param type 类型描述。
 * @retval 生成的低层语法节点。
 */
NodePtr Node::cast(NodePtr value, TypePtr type)
{
    auto node = Memory::makeShared<Node>();
    node->kind = Kind::Cast;
    node->declared = std::move(type);
    node->kids = { std::move(value) };
    return node;
}

/**
 * @brief 构造条件分支对应的低层语法节点。
 * @param condition 控制分支或循环的条件。
 * @param thenBranch 条件成立时执行的分支。
 * @param elseBranch 条件不成立时执行的分支。
 * @retval 生成的低层语法节点。
 */
NodePtr Node::ifThenElse(NodePtr condition, NodePtr thenBranch, NodePtr elseBranch)
{
    auto node = Memory::makeShared<Node>();
    node->kind = Kind::If;
    node->kids = { std::move(condition), std::move(thenBranch), std::move(elseBranch) };
    return node;
}

/**
 * @brief 构造前置条件循环对应的低层语法节点。
 * @param condition 控制分支或循环的条件。
 * @param body 待处理的函数体或循环体。
 * @retval 生成的低层语法节点。
 */
NodePtr Node::whileLoop(NodePtr condition, NodePtr body)
{
    auto node = Memory::makeShared<Node>();
    node->kind = Kind::While;
    node->kids = { std::move(condition), std::move(body) };
    return node;
}

/**
 * @brief 构造后置条件循环对应的低层语法节点。
 * @param body 待处理的函数体或循环体。
 * @param condition 控制分支或循环的条件。
 * @retval 生成的低层语法节点。
 */
NodePtr Node::doWhileLoop(NodePtr body, NodePtr condition)
{
    auto node = Memory::makeShared<Node>();
    node->kind = Kind::DoWhile;
    node->kids = { std::move(body), std::move(condition) };
    return node;
}

/**
 * @brief 构造跳转标签对应的低层语法节点。
 * @param name 名称文本。
 * @retval 生成的低层语法节点。
 */
NodePtr Node::label(Memory::String name)
{
    auto node = Memory::makeShared<Node>();
    node->kind = Kind::Label;
    node->text = std::move(name);
    return node;
}

/**
 * @brief 构造无条件跳转对应的低层语法节点。
 * @param target 待访问或修改的目标。
 * @retval 生成的低层语法节点。
 */
NodePtr Node::jump(Memory::String target)
{
    auto node = Memory::makeShared<Node>();
    node->kind = Kind::Jump;
    node->text = std::move(target);
    return node;
}

/**
 * @brief 构造函数返回对应的低层语法节点。
 * @param value 待转换、保存或参与计算的值。
 * @retval 生成的低层语法节点。
 */
NodePtr Node::ret(NodePtr value)
{
    auto node = Memory::makeShared<Node>();
    node->kind = Kind::Return;
    if (value) node->kids = { std::move(value) };
    return node;
}

/**
 * @brief 构造表达式语句对应的低层语法节点。
 * @param expression 待解析或求值的表达式文本。
 * @retval 生成的低层语法节点。
 */
NodePtr Node::exprStmt(NodePtr expression)
{
    auto node = Memory::makeShared<Node>();
    node->kind = Kind::ExprStmt;
    node->kids = { std::move(expression) };
    return node;
}

/**
 * @brief 构造值输出对应的低层语法节点。
 * @param value 待转换、保存或参与计算的值。
 * @retval 生成的低层语法节点。
 */
NodePtr Node::write(NodePtr value)
{
    auto node = Memory::makeShared<Node>();
    node->kind = Kind::Write;
    if (value) node->kids = { std::move(value) };
    return node;
}

/**
 * @brief 构造语句块对应的低层语法节点。
 * @param statements 待处理的语句列表。
 * @retval 生成的低层语法节点。
 */
NodePtr Node::block(Memory::Vector<NodePtr> statements)
{
    auto node = Memory::makeShared<Node>();
    node->kind = Kind::Block;
    node->kids = std::move(statements);
    return node;
}

/**
 * @brief 构造文本输出对应的低层语法节点。
 * @param text 待处理的文本。
 * @retval 生成的低层语法节点。
 */
NodePtr Node::print(Memory::String text)
{
    auto node = Memory::makeShared<Node>();
    node->kind = Kind::Print;
    node->text = std::move(text);
    return node;
}

/**
 * @brief 构造字符输出对应的低层语法节点。
 * @param value 待转换、保存或参与计算的值。
 * @retval 生成的低层语法节点。
 */
NodePtr Node::putc(NodePtr value)
{
    auto node = Memory::makeShared<Node>();
    node->kind = Kind::Putc;
    node->kids = { std::move(value) };
    return node;
}

/**
 * @brief 从输入读一个值写进变量（生成 `(read, _, _, 变量)`）。
 *
 * 输入项由调用方通过 `Machine::run(program, input)` 按顺序提供（每项一个
 * 数字字面量），类型按变量声明的类型收（整数 / 浮点都行）。
 * @param variable 变量名（要先 `Node::declare` 或当形参声明过）。
 * @retval 生成的低层语法节点。
 */
NodePtr Node::read(Memory::String variable)
{
    auto node = Memory::makeShared<Node>();
    node->kind = Kind::Read;
    node->text = std::move(variable);
    return node;
}

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
            if (line.rfind("# magel-source:", 0) == 0)
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

namespace
{

/**
 * @brief 把数据条目铺成二进制；可选输出布局表。
 * @param entries 条目。
 * @param layout 非空时写入 `名字 offset=… size=… type=…`。
 * @return 二进制内容。
 */
Memory::String buildDataBinary(const Memory::Vector<CodegenResult::Entry> &entries,
                               Memory::Vector<Memory::String> *layout)
{
    // 1. 句柄表：数组元素里出现字符串 / 复合对象时写的是"条目序号"。
    Memory::Map<Memory::String, std::uint32_t> handles;
    for (std::size_t index = 0; index < entries.size(); ++index)
    {
        handles[entries[index].name] = static_cast<std::uint32_t>(index);
    }

    Memory::String out;
    // **地址 0 留给"空指针"**：数据区从偏移 8 开始排，这样任何真实对象的地址都不为 0，
    // `p == null` / 函数指针判空才有意义（堆那边本来也是从数据区之后开始长的）。
    out.assign(8u, '\0');
    for (const CodegenResult::Entry &entry : entries)
    {
        // 2. 对齐（`align=` 缺省 8 字节）。
        const int align = metaInt(entry.type->meta, "align", 8);
        while ((out.size() % static_cast<std::size_t>(align > 0 ? align : 1)) != 0u) out.push_back('\0');
        const std::size_t start = out.size();

        /**
         * 写一个值：字符串 → 字节 + `\0`；数字 → 8 字节；已知标签 → 4 字节句柄。
         */
        const Memory::Function<void(const Memory::String &)> writeValue = [&](const Memory::String &token) {
            Memory::String text = token;
            while (!text.empty() && (text.front() == ' ')) text.erase(text.begin());
            while (!text.empty() && (text.back() == ' ')) text.pop_back();
            if (isQuoted(text))
            {
                out += text.substr(1u, text.size() - 2u);
                out.push_back('\0');
                return;
            }
            bool isFloat = false;
            std::int64_t intValue = 0;
            double doubleValue = 0.0;
            if (numberText(text, isFloat, intValue, doubleValue))
            {
                if (isFloat)
                    putDouble(out, doubleValue);
                else
                    putInt(out, intValue);
                return;
            }
            const auto found = handles.find(text);
            putU32(out, found == handles.end() ? 0u : found->second);
        };

        // 3. 按类型写内容。
        if (entry.type->kind == Type::Kind::String)
        {
            writeValue(entry.body);
        }
        else if (entry.type->kind == Type::Kind::Array || entry.type->kind == Type::Kind::Struct)
        {
            for (const Memory::String &item : splitTop(entry.body)) writeValue(item);
        }
        else if (entry.type->kind == Type::Kind::Enum)
        {
            // 枚举：4 字节 tag（分支序号，按类型的 variants 顺序）+ 载荷。
            const std::size_t open = entry.body.find('(');
            const Memory::String variant =
                (open == Memory::String::npos) ? entry.body : entry.body.substr(0u, open);
            std::uint32_t tag = 0u;
            for (std::size_t index = 0; index < entry.type->variants.size(); ++index)
            {
                if (entry.type->variants[index].first == variant) tag = static_cast<std::uint32_t>(index);
            }
            putU32(out, tag);
            if (open != Memory::String::npos)
            {
                const std::size_t close = entry.body.rfind(')');
                const Memory::String payload = entry.body.substr(open + 1u, close - open - 1u);
                for (const Memory::String &item : splitTop(payload)) writeValue(item);
            }
        }
        else
        {
            writeValue(entry.body);
        }

        if (layout != nullptr)
        {
            // 带上 stride=8：数据区里每个槽位固定 8 字节，和四元式里 `(*:i64, index, 8, t)`
            // 用的步长一致；stackvm 的 DataArea 会把它读进来备查。
            layout->push_back(entry.name + " offset=" + Memory::toString(start) + " size=" + Memory::toString(out.size() - start) + " stride=8 type=" + entry.type->describe());
        }
    }
    // 5. 尾部补 8 个零字节：数据区是按地址按 8 字节整块读的（`load`），
    //    最后一个条目末尾也要有"可读满 8 字节"的余量，逐字节扫字符串才不会越界。
    while ((out.size() % 8u) != 0u) out.push_back('\0');
    out.append(8u, '\0');
    return out;
}

}  // namespace

/**
 * @brief 导出 `.data` 的**二进制**形态（小端）。
 *
 * 布局规则（简单、够嵌入式用）：
 *
 *   - 每个条目按 `align=` 对齐（缺省 8 字节），不足补 0；
 *   - 字符串：UTF-8 字节 + `\0`；
 *   - 数值：8 字节有符号 / 浮点（按条目类型：f* 用 IEEE-754 双精度）；
 *   - 数组 / 结构体：元素依次排布；
 *   - 枚举：4 字节 tag（分支序号）+ 载荷；
 *   - 引用 / 字符串 / 复合对象出现在数组里时写 4 字节"条目序号"当句柄。
 * @return 二进制内容。
 */
Memory::String CodegenResult::exportDataBinary() const
{
    return buildDataBinary(entries, nullptr);
}

/**
 * @brief 每个条目在二进制里的偏移与大小（便于查表 / 测试）。
 * @return 一行一条：`名字 offset=… size=… type=…`。
 */
Memory::Vector<Memory::String> CodegenResult::exportDataLayout() const
{
    Memory::Vector<Memory::String> layout;
    (void)buildDataBinary(entries, &layout);
    return layout;
}

/**
 * @brief 导出配套的 C 头文件（偏移表 + 字节数组）。
 * @param moduleName 模块名（写进注释与宏前缀）。
 * @return 头文件文本。
 */
Memory::String CodegenResult::exportDataHeader(const Memory::String &moduleName) const
{
    const Memory::String binary = buildDataBinary(entries, nullptr);
    Memory::OutputStringStream header;
    header << "/* " << moduleName << " 的 .data 二进制（小端）。由 simple_ast 生成。 */\n";
    header << "#ifndef SIMPLE_AST_DATA_" << moduleName << "_H\n";
    header << "#define SIMPLE_AST_DATA_" << moduleName << "_H\n\n";
    header << "#include <stdint.h>\n\n";
    for (const Memory::String &line : exportDataLayout())
    {
        const std::size_t space = line.find(' ');
        const Memory::String name = line.substr(0u, space);
        const std::size_t offsetAt = line.find("offset=");
        header << "#define DATA_" << name << "_OFFSET "
               << line.substr(offsetAt + 7u, line.find(' ', offsetAt) - offsetAt - 7u) << "u\n";
    }
    header << "\nstatic const unsigned char " << moduleName << "_data[] = {";
    for (std::size_t index = 0; index < binary.size(); ++index)
    {
        if (index % 12u == 0u) header << "\n    ";
        header << static_cast<int>(static_cast<unsigned char>(binary[index])) << ",";
    }
    header << "\n};\n\n#endif\n";
    return header.str();
}

}  // namespace simple_ast
