#include "std_cas.hpp"
#include "std_native_record.hpp"
#include <exception>
#include <limits>
#include <mutex>
#include <stdexcept>
#if !defined(NO_CHALI_STD) && !defined(NO_CHALI_STD_CAS) && !defined(NO_CHALI_BIG_INT)
#include "cas/allocator.h"
#include "cas/detail/calculus.h"
#include "cas/detail/parser.h"
#include "cas/detail/polynomial.h"
#include "cas/detail/printer.h"
#include "cas/detail/trigonometry.h"
#if !defined(NO_CHALI_BIG_FLOAT)
#include "bigfloat.hpp"
#endif
#endif

namespace std_cas_glue
{
namespace
{
using namespace native_record_glue;
#if !defined(NO_CHALI_STD) && !defined(NO_CHALI_STD_CAS) && !defined(NO_CHALI_BIG_INT)
using namespace Cas::Detail;
/**
 * @brief 持有不可变表达式根节点的独立引用。
 */
struct ExpressionRecord
{ Ptr root; };
/**
 * @brief 串行保护 CAS 全局符号、内存池与节点引用计数。
 * @return 覆盖进程生命周期的递归互斥量。
 */
std::recursive_mutex &casMutex()
{
    static auto *guard = new std::recursive_mutex;
    return *guard;
}
/**
 * @brief 为另一个运行时记录保留不可变根节点。
 * @param source 已校验的表达式记录。
 * @param error 接收复制错误，本操作成功时保持原值。
 * @return 具有独立所有权的记录。
 */
void *cloneExpression(const void *source, ::Memory::String &error)
{
    (void)error;
    std::lock_guard<std::recursive_mutex> lock(casMutex());
    return new ExpressionRecord{ static_cast<const ExpressionRecord *>(source)->root.share() };
}
/**
 * @brief 在 CAS 锁保护下释放根节点引用。
 * @param record 待销毁的表达式记录。
 */
void destroyExpression(void *record)
{
    std::lock_guard<std::recursive_mutex> lock(casMutex());
    delete static_cast<ExpressionRecord *>(record);
}
/**
 * @brief 使用规范文本显示表达式，不改变 AST。
 * @param record 当前存活的表达式记录。
 * @return AST 的规范中缀文本。
 */
::Memory::String textExpression(const void *record)
{
    std::lock_guard<std::recursive_mutex> lock(casMutex());
    const auto text = toText(static_cast<const ExpressionRecord *>(record)->root.get());
    return ::Memory::String(text.data(), text.size());
}
const stackvm::NativeRecordType expressionType{ "std::cas::Expr", cloneExpression, destroyExpression, textExpression };
/**
 * @brief 检查句柄类型并借用表达式根节点。
 * @param state 当前宿主调用状态。
 * @param value 表达式对象句柄。
 * @return 存活的记录；失败时为空并保存诊断。
 */
ExpressionRecord *expression(State &state, const Value &value)
{
    return record<ExpressionRecord>(state, value, expressionType, "std::cas");
}
/**
 * @brief 将 AST 根节点所有权交给共享 VM 的通用记录协议。
 * @param state 当前宿主调用状态。
 * @param root 新持有的 AST 根节点，不得为空。
 * @return 运行时表达式记录句柄。
 */
Value makeExpression(State &state, Ptr root)
{
    if (!root) throw std::runtime_error("CAS 无法计算给定表达式");
    auto *value = new ExpressionRecord{ std::move(root) };
    const auto handle = state.host.makeNativeRecord(expressionType, value);
    if (!handle) delete value;
    return Value::fromInt(handle);
}
/**
 * @brief 从运行时复制完整 UTF-8 文本，不借用临时字符串内存。
 * @param state 当前宿主调用状态。
 * @param value String 对象参数。
 * @return 独立拥有的宿主字符串。
 */
std::string readText(State &state, const Value &value)
{
    ::Memory::String text;
    if (!nativeReadCString(state, static_cast<std::size_t>(value.asInt()), &text))
        throw std::invalid_argument("CAS 无法读取字符串参数");
    return std::string(text.data(), text.size());
}
/**
 * @brief 验证微积分自变量确实是单个符号。
 * @param text 自变量的源文本。
 * @return AST 符号表中的编号。
 */
std::uint32_t variableId(const std::string &text)
{
    auto parsed = parseExpression(text);
    if (!parsed.ok || !parsed.expression || parsed.expression->kind() != NodeKind::Symbol)
        throw std::invalid_argument("CAS 自变量必须是单个符号");
    return parsed.expression->symbolId();
}
/**
 * @brief 创建精确的零表达式，供默认初始化使用。
 * @param state 当前宿主调用状态及诊断输出。
 * @param arguments 本入口无参数。
 * @return 值为零的新表达式记录句柄。
 */
Value casNew(State &state, const ::Memory::Vector<Value> &arguments)
{
    (void)arguments;
    Cas::Allocator::initialize();
    return makeExpression(state, Ptr(Node::makeInteger(BigInt(0))));
}
/**
 * @brief 解析字符串为规范化不可变 AST，保留原始解析错误位置。
 * @param state 当前宿主调用状态及诊断输出。
 * @param arguments 待解析的 UTF-8 String。
 * @return 成功解析并拥有 AST 的记录句柄。
 */
Value casParse(State &state, const ::Memory::Vector<Value> &arguments)
{
    const auto text = readText(state, arguments[0]);
    auto parsed = parseExpression(text);
    if (!parsed.ok) throw std::invalid_argument("CAS 表达式解析失败（字节 " + std::to_string(parsed.position) + "）：" + parsed.message);
    return makeExpression(state, std::move(parsed.expression));
}
/**
 * @brief 输出 AST 的规范文本或 LaTeX。
 * @param state 当前宿主调用状态及诊断输出。
 * @param arguments 表达式记录和是否输出 LaTeX 的标志。
 * @return 规范文本或 LaTeX 文本的 VM 字符串句柄。
 */
Value casText(State &state, const ::Memory::Vector<Value> &arguments)
{
    const auto *value = expression(state, arguments[0]);
    if (!value) return Value::fromInt(0);
    const auto text = arguments[1].asInt() ? toLatex(value->root.get()) : toText(value->root.get());
    return Value::fromInt(nativeAllocString(state, ::Memory::String(text.data(), text.size())));
}
/**
 * @brief 查询根节点种类，不执行额外化简。
 * @param state 当前宿主调用状态及诊断输出。
 * @param arguments 待查询的表达式记录。
 * @return 对应语言 NodeKind 的整数编码。
 */
Value casKind(State &state, const ::Memory::Vector<Value> &arguments)
{
    const auto *value = expression(state, arguments[0]);
    return Value::fromInt(value ? static_cast<std::int64_t>(value->root->kind()) : 0);
}
/**
 * @brief 读取符号、函数或命名常数的名称。
 * @param state 当前宿主调用状态及诊断输出。
 * @param arguments 待查询名称的表达式记录。
 * @return 符号或函数名称的 VM 字符串句柄。
 */
Value casName(State &state, const ::Memory::Vector<Value> &arguments)
{
    const auto *value = expression(state, arguments[0]);
    if (!value) return Value::fromInt(0);
    std::string text = value->root->name() ? value->root->name() : "";
    if (value->root->kind() == NodeKind::Constant) text = toText(value->root.get());
    return Value::fromInt(nativeAllocString(state, ::Memory::String(text.data(), text.size())));
}
/**
 * @brief 查询根节点直接子节点数量。
 * @param state 当前宿主调用状态及诊断输出。
 * @param arguments 待统计直接子节点的表达式记录。
 * @return 根节点直接子节点的数量。
 */
Value casSize(State &state, const ::Memory::Vector<Value> &arguments)
{
    const auto *value = expression(state, arguments[0]);
    return Value::fromInt(value ? static_cast<std::int64_t>(value->root->size()) : 0);
}
/**
 * @brief 返回独立持有的子 AST，父表达式释放后仍可使用。
 * @param state 当前宿主调用状态及诊断输出。
 * @param arguments 表达式记录和从零开始的子节点下标。
 * @return 独立保留指定子树的记录句柄。
 */
Value casChild(State &state, const ::Memory::Vector<Value> &arguments)
{
    const auto *value = expression(state, arguments[0]);
    if (!value) return Value::fromInt(0);
    const auto index = arguments[1].asInt();
    if (index < 0 || static_cast<std::uint64_t>(index) >= value->root->size()) throw std::out_of_range("CAS 子节点下标越界");
    return makeExpression(state, Ptr::borrow(value->root->operand(static_cast<std::size_t>(index))));
}
/**
 * @brief 返回整数或有理数的精确分子、分母文本。
 * @param state 当前宿主调用状态及诊断输出。
 * @param arguments 数值表达式记录和是否查询分母的标志。
 * @return 分子或分母十进制文本的 VM 字符串句柄。
 */
Value casNumberPart(State &state, const ::Memory::Vector<Value> &arguments)
{
    const auto *value = expression(state, arguments[0]);
    if (!value) return Value::fromInt(0);
    if (!value->root->isNumber()) throw std::domain_error("CAS 只有数值节点可以读取分子和分母");
    const auto text = arguments[1].asInt() ? value->root->denominator().toString() : value->root->numerator().toString();
    return Value::fromInt(nativeAllocString(state, ::Memory::String(text.data(), text.size())));
}
/**
 * @brief 直接组合并化简两个 AST 的四则运算或整数/符号幂。
 * @param state 当前宿主调用状态及诊断输出。
 * @param arguments 左表达式、右表达式及运算编号。
 * @return 四则运算或幂计算得到的新 AST 记录句柄。
 */
Value casBinary(State &state, const ::Memory::Vector<Value> &arguments)
{
    const auto *left = expression(state, arguments[0]);
    const auto *right = expression(state, arguments[1]);
    if (!left || !right) return Value::fromInt(0);
    Ptr result;
    switch (arguments[2].asInt())
    {
    case 0: result = makeAdd(left->root.share(), right->root.share()); break;
    case 1: result = makeAdd(left->root.share(), makeNegate(right->root.share())); break;
    case 2: result = makeMul(left->root.share(), right->root.share()); break;
    case 3:
        if (right->root->isZero()) throw std::domain_error("CAS 除数不能为零");
        result = makeDivide(left->root.share(), right->root.share());
        break;
    case 4: result = makePow(left->root.share(), right->root.share()); break;
    default: throw std::invalid_argument("CAS 二元运算编号无效");
    }
    return makeExpression(state, std::move(result));
}
/**
 * @brief 比较两棵规范 AST 的结构和值是否相等。
 * @param state 当前宿主调用状态及诊断输出。
 * @param arguments 待比较的左右表达式记录。
 * @return 规范 AST 结构和值相等时为真。
 */
Value casEqual(State &state, const ::Memory::Vector<Value> &arguments)
{
    const auto *left = expression(state, arguments[0]);
    const auto *right = expression(state, arguments[1]);
    return Value::fromInt(left && right && left->root->equals(right->root.get()));
}
/**
 * @brief 执行取负、化简、展开、因式分解、通分、求导或不定积分。
 * @param state 当前宿主调用状态及诊断输出。
 * @param arguments 表达式记录、变换编号、自变量名称和导数阶数。
 * @return 完成指定一元符号变换的新 AST 记录句柄。
 */
Value casUnary(State &state, const ::Memory::Vector<Value> &arguments)
{
    const auto *value = expression(state, arguments[0]);
    if (!value) return Value::fromInt(0);
    const auto operation = arguments[1].asInt();
    Ptr result;
    switch (operation)
    {
    case 0: result = makeNegate(value->root.share()); break;
    case 1: result = simplify(value->root); break;
    case 2: result = expand(value->root); break;
    case 3: result = factor(value->root); break;
    case 4: result = togetherDeep(value->root); break;
    case 5:
    {
        const auto order = arguments[3].asInt();
        if (order < 0 || order > 1000) throw std::out_of_range("CAS 导数阶数必须在 0..1000 之间");
        result = differentiateN(value->root, variableId(readText(state, arguments[2])), static_cast<int>(order));
        break;
    }
    case 6:
    {
        const auto id = variableId(readText(state, arguments[2]));
        result = integrate(value->root, id);
        if (!result)
        {
            Ptr symbol(Node::makeSymbolById(id));
            result = Ptr(Node::makeFunction("integrate", { value->root.get(), symbol.get() }));
        }
        break;
    }
    default: throw std::invalid_argument("CAS 一元运算编号无效");
    }
    return makeExpression(state, std::move(result));
}
/**
 * @brief 在 AST 上执行替换、极限、Taylor 展开或定积分。
 * @param state 当前宿主调用状态及诊断输出。
 * @param arguments 原表达式、替换点、上界、自变量名称、变换编号与展开阶数。
 * @return 替换、极限、展开或定积分得到的新 AST 记录句柄。
 */
Value casTransform(State &state, const ::Memory::Vector<Value> &arguments)
{
    const auto *value = expression(state, arguments[0]);
    const auto *point = expression(state, arguments[1]);
    const auto *upper = expression(state, arguments[2]);
    if (!value || !point || !upper) return Value::fromInt(0);
    const auto id = variableId(readText(state, arguments[3]));
    Ptr result;
    switch (arguments[4].asInt())
    {
    case 0: result = substitute(value->root, id, point->root); break;
    case 1: result = limit(value->root, id, point->root); break;
    case 2:
    {
        const auto order = arguments[5].asInt();
        if (order < 0 || order > 1000) throw std::out_of_range("CAS Taylor 阶数必须在 0..1000 之间");
        result = taylorExpand(value->root, id, point->root, static_cast<int>(order));
        break;
    }
    case 3: result = definiteIntegral(value->root, id, point->root, upper->root); break;
    default: throw std::invalid_argument("CAS AST 变换编号无效");
    }
    return makeExpression(state, std::move(result));
}
/**
 * @brief 将精确有理数通过共享 BigFloat 后端转换为指定精度小数。
 * @param state 当前宿主调用状态及诊断输出。
 * @param arguments 精确数值表达式与请求的十进制有效位数。
 * @return 指定位数十进制结果的 VM 字符串句柄。
 */
Value casDecimal(State &state, const ::Memory::Vector<Value> &arguments)
{
    const auto *value = expression(state, arguments[0]);
    if (!value) return Value::fromInt(0);
    if (!value->root->isNumber()) throw std::domain_error("CAS 小数转换要求精确数值节点，请先替换自由变量");
    const auto digits = arguments[1].asInt();
    if (digits < 1 || digits > 100000) throw std::out_of_range("CAS 小数有效位数必须在 1..100000 之间");
#if !defined(NO_CHALI_BIG_FLOAT)
    const auto numerator = bigfloat::BigFloat::fromInt(value->root->numerator().backend());
    const auto denominator = bigfloat::BigFloat::fromInt(value->root->denominator().backend());
    const auto text = numerator.divide(denominator, static_cast<int>(digits)).toString(0);
    return Value::fromInt(nativeAllocString(state, text));
#else
    throw std::runtime_error("CAS 小数转换需要启用 BigDecimal 后端");
#endif
}
/**
 * @brief 将数学函数组合为保留精确常量的符号 AST。
 * @param state 当前宿主调用及诊断状态。
 * @param arguments 表达式、函数名称与可选的第二表达式。
 * @return 新建并化简的符号表达式记录。
 */
Value casMath(State &state, const ::Memory::Vector<Value> &arguments)
{
    const auto *value = expression(state, arguments[0]);
    const auto *other = expression(state, arguments[2]);
    if (!value || !other) return Value::fromInt(0);
    auto name = readText(state, arguments[1]);
    if (name == "ln") name = "log";
    if (name == "sqrt") return makeExpression(state, makePow(value->root.share(), makeNumber(BigInt(1), BigInt(2))));
    if (name == "log10")
    {
        auto numerator = Ptr(Node::makeFunction("log", { value->root.get() }));
        auto ten = Ptr(Node::makeInteger(BigInt(10)));
        auto denominator = Ptr(Node::makeFunction("log", { ten.get() }));
        return makeExpression(state, simplify(makeDivide(std::move(numerator), std::move(denominator))));
    }
    if (value->root->isNumber())
    {
        const auto numerator = value->root->numerator();
        const auto denominator = value->root->denominator();
        auto quotient = numerator / denominator;
        const auto remainder = numerator % denominator;
        if (name == "floor") return makeExpression(state, Ptr(Node::makeInteger(quotient)));
        if (name == "ceil") return makeExpression(state, Ptr(Node::makeInteger(quotient + BigInt(remainder.isZero() ? 0 : 1))));
        if (name == "trunc") return makeExpression(state, Ptr(Node::makeInteger(quotient + BigInt(numerator.isNegative() && !remainder.isZero() ? 1 : 0))));
        if (name == "round")
        {
            auto rounded = (numerator.abs() * BigInt(2) + denominator) / (denominator * BigInt(2));
            if (numerator.isNegative()) rounded = -rounded;
            return makeExpression(state, Ptr(Node::makeInteger(rounded)));
        }
        if (name == "fmod" && other->root->isNumber())
        {
            if (other->root->isZero()) throw std::domain_error("CAS 余数除数不能为零");
            const auto n = numerator * other->root->denominator();
            const auto d = denominator * other->root->numerator();
            auto q = n / d;
            if (n.isNegative() != d.isNegative() && !(n % d).isZero()) q = q + BigInt(1);
            auto product = makeMul(Ptr(Node::makeInteger(q)), other->root.share());
            return makeExpression(state, makeAdd(value->root.share(), makeNegate(std::move(product))));
        }
    }
    auto result = Ptr(Node::makeFunction(name, { value->root.get() }));
    if (name == "fmod") result = Ptr(Node::makeFunction(name, { value->root.get(), other->root.get() }));
    return makeExpression(state, trigSimplify(simplify(result)));
}
#else
/**
 * @brief 为显式裁剪 CAS 的宿主保留可诊断的入口。
 * @param state 当前调用诊断状态。
 * @param arguments 按原接口传入的参数，本构建不计算。
 * @return 零占位值并报告裁剪错误。
 */
Value casDisabled(State &state, const ::Memory::Vector<Value> &arguments)
{
    (void)arguments;
    state.error = "这个构建未编入 std::cas（需要标准库及大整数后端）";
    return Value::fromInt(0);
}
#endif
}  // namespace
/**
 * @brief 注册串行保护、严格检查参数数量并传播原始错误的 CAS 操作。
 * @param registry 目标宿主原生注册表。
 * @param error 注册失败时接收诊断。
 * @return 所有入口注册成功时为 true。
 */
bool bind(stackvm::NativeRegistry &registry, ::Memory::String *error)
{
    using Operation = Value (*)(State &, const ::Memory::Vector<Value> &);
    struct Binding
    {
        const char *name;
        std::size_t arity;
        Operation operation;
    };
    const Binding bindings[] = {
#if !defined(NO_CHALI_STD) && !defined(NO_CHALI_STD_CAS) && !defined(NO_CHALI_BIG_INT)
        { "__std_cas_math", 3, casMath },
#else
        { "__std_cas_math", 3, casDisabled },
#endif
#if !defined(NO_CHALI_STD) && !defined(NO_CHALI_STD_CAS) && !defined(NO_CHALI_BIG_INT)
        { "__std_cas_new", 0, casNew },
#else
        { "__std_cas_new", 0, casDisabled },
#endif
#if !defined(NO_CHALI_STD) && !defined(NO_CHALI_STD_CAS) && !defined(NO_CHALI_BIG_INT)
        { "__std_cas_parse", 1, casParse },
#else
        { "__std_cas_parse", 1, casDisabled },
#endif
#if !defined(NO_CHALI_STD) && !defined(NO_CHALI_STD_CAS) && !defined(NO_CHALI_BIG_INT)
        { "__std_cas_text", 2, casText },
#else
        { "__std_cas_text", 2, casDisabled },
#endif
#if !defined(NO_CHALI_STD) && !defined(NO_CHALI_STD_CAS) && !defined(NO_CHALI_BIG_INT)
        { "__std_cas_kind", 1, casKind },
#else
        { "__std_cas_kind", 1, casDisabled },
#endif
#if !defined(NO_CHALI_STD) && !defined(NO_CHALI_STD_CAS) && !defined(NO_CHALI_BIG_INT)
        { "__std_cas_name", 1, casName },
#else
        { "__std_cas_name", 1, casDisabled },
#endif
#if !defined(NO_CHALI_STD) && !defined(NO_CHALI_STD_CAS) && !defined(NO_CHALI_BIG_INT)
        { "__std_cas_size", 1, casSize },
#else
        { "__std_cas_size", 1, casDisabled },
#endif
#if !defined(NO_CHALI_STD) && !defined(NO_CHALI_STD_CAS) && !defined(NO_CHALI_BIG_INT)
        { "__std_cas_child", 2, casChild },
#else
        { "__std_cas_child", 2, casDisabled },
#endif
#if !defined(NO_CHALI_STD) && !defined(NO_CHALI_STD_CAS) && !defined(NO_CHALI_BIG_INT)
        { "__std_cas_number_part", 2, casNumberPart },
#else
        { "__std_cas_number_part", 2, casDisabled },
#endif
#if !defined(NO_CHALI_STD) && !defined(NO_CHALI_STD_CAS) && !defined(NO_CHALI_BIG_INT)
        { "__std_cas_binary", 3, casBinary },
#else
        { "__std_cas_binary", 3, casDisabled },
#endif
#if !defined(NO_CHALI_STD) && !defined(NO_CHALI_STD_CAS) && !defined(NO_CHALI_BIG_INT)
        { "__std_cas_equal", 2, casEqual },
#else
        { "__std_cas_equal", 2, casDisabled },
#endif
#if !defined(NO_CHALI_STD) && !defined(NO_CHALI_STD_CAS) && !defined(NO_CHALI_BIG_INT)
        { "__std_cas_unary", 4, casUnary },
#else
        { "__std_cas_unary", 4, casDisabled },
#endif
#if !defined(NO_CHALI_STD) && !defined(NO_CHALI_STD_CAS) && !defined(NO_CHALI_BIG_INT)
        { "__std_cas_transform", 6, casTransform },
#else
        { "__std_cas_transform", 6, casDisabled },
#endif
#if !defined(NO_CHALI_STD) && !defined(NO_CHALI_STD_CAS) && !defined(NO_CHALI_BIG_INT)
        { "__std_cas_decimal", 2, casDecimal },
#else
        { "__std_cas_decimal", 2, casDisabled },
#endif
    };
    bool ok = true;
    for (const auto &binding : bindings)
    {
        auto handler = [operation = binding.operation](stackvm::NativeCallContext &context, const ::Memory::Vector<Value> &arguments) {
#if !defined(NO_CHALI_STD) && !defined(NO_CHALI_STD_CAS) && !defined(NO_CHALI_BIG_INT)
            std::lock_guard<std::recursive_mutex> lock(casMutex());
#endif
            State state{context, {}};
            try {
#if !defined(NO_CHALI_STD) && !defined(NO_CHALI_STD_CAS) && !defined(NO_CHALI_BIG_INT)
                // 不可变子树可跨 VM/线程保留，数值缓冲必须由进程级 arena 持有。
                ::Memory::AllocationScope allocationScope(::Memory::defaultRuntimeAllocator());
#endif
                auto result = operation(state, arguments);
                if (!state.error.empty()) context.fail(state.error);
                return result;
            } catch (const std::exception &exception) {
                context.fail(::Memory::String("CAS：") + exception.what());
                return Value::fromInt(0);
            } };
        ok = registry.add(binding.name, binding.arity, handler, error) && ok;
        ok = registry.add(::Memory::String("std::cas::") + binding.name, binding.arity, handler, error) && ok;
    }
    ok = native_record_glue::bind(registry, "__std_cas_copy", 1, native_record_glue::copy, error) && ok;
    ok = native_record_glue::bind(registry, "__std_cas_drop", 1, native_record_glue::drop, error) && ok;
    ok = native_record_glue::bind(registry, "std::cas::__std_cas_copy", 1, native_record_glue::copy, error) && ok;
    ok = native_record_glue::bind(registry, "std::cas::__std_cas_drop", 1, native_record_glue::drop, error) && ok;
    return ok;
}
}  // namespace std_cas_glue
