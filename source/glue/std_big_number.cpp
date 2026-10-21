#include "std_big_number.hpp"
#ifndef STACKVM_WITH_BIGINT
#define STACKVM_WITH_BIGINT 1
#endif
#ifndef STACKVM_WITH_BIGFLOAT
#define STACKVM_WITH_BIGFLOAT STACKVM_WITH_BIGINT
#endif
#if STACKVM_WITH_BIGFLOAT && !STACKVM_WITH_BIGINT
#error "numeric glue requires the integer backend for decimal arithmetic"
#endif
#include "std_native_record.hpp"
#if STACKVM_WITH_BIGINT
#include "bigint.hpp"
#endif
#if STACKVM_WITH_BIGFLOAT
#include "bigfloat.hpp"
#include "bigfloat_math.hpp"
#endif
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <exception>
#include <stdexcept>

/**
 * @brief 大数的宿主记录、复制协议与操作实现，VM 不持有其数值布局。
 */
namespace big_number_glue
{
using namespace native_record_glue;
#if STACKVM_WITH_BIGINT
/**
 * @brief 胶水独占的大数记录，复制构造产生独立数值。
 */
struct BigIntRecord
{
    bigint::BigInt number;
};
/**
 * @brief 复制一个大数宿主记录。
 * @param source 原记录，调用期间保持存活。
 * @param error 接收复制失败原因，本实现正常复制时保持为空。
 * @return 新分配且独立拥有的记录。
 */
inline void *cloneBigIntRecord(const void *source, Memory::String &error)
{
    (void)error;
    return new BigIntRecord(*static_cast<const BigIntRecord *>(source));
}
/**
 * @brief 销毁胶水独占的大数记录。
 * @param record 本描述符创建的记录。
 */
inline void destroyBigIntRecord(void *record) { delete static_cast<BigIntRecord *>(record); }
/**
 * @brief 按宿主数值语义生成默认文本。
 * @param record 已校验的大数记录。
 * @return 十进制文本。
 */
inline Memory::String textBigIntRecord(const void *record)
{
    return static_cast<const BigIntRecord *>(record)->number.toString();
}
inline const stackvm::NativeRecordType bigIntType{ "BigInteger", cloneBigIntRecord, destroyBigIntRecord, textBigIntRecord };
/**
 * @brief 从运行时通用壳借用大数宿主记录。
 * @param ctx 当前胶水调用状态。
 * @param handle 对象句柄参数。
 * @param who 操作的诊断名称。
 * @return 类型匹配且存活的记录，失败返回空。
 */
inline BigIntRecord *bigIntRecord(State &ctx, const Value &handle, const char *who)
{
    return record<BigIntRecord>(ctx, handle, bigIntType, who);
}
#endif
#if STACKVM_WITH_BIGFLOAT
/**
 * @brief 胶水独占的大数记录，复制构造产生独立数值。
 */
struct BigFloatRecord
{
    bigfloat::BigFloat decimal;
    int decimalPrecision = 0;
};
/**
 * @brief 复制一个大数宿主记录。
 * @param source 原记录，调用期间保持存活。
 * @param error 接收复制失败原因，本实现正常复制时保持为空。
 * @return 新分配且独立拥有的记录。
 */
inline void *cloneBigFloatRecord(const void *source, Memory::String &error)
{
    (void)error;
    return new BigFloatRecord(*static_cast<const BigFloatRecord *>(source));
}
/**
 * @brief 销毁胶水独占的大数记录。
 * @param record 本描述符创建的记录。
 */
inline void destroyBigFloatRecord(void *record) { delete static_cast<BigFloatRecord *>(record); }
/**
 * @brief 按宿主数值语义生成默认文本。
 * @param record 已校验的大数记录。
 * @return 十进制文本。
 */
inline Memory::String textBigFloatRecord(const void *record)
{
    return static_cast<const BigFloatRecord *>(record)->decimal.toString(0);
}
inline const stackvm::NativeRecordType bigFloatType{ "BigDecimal", cloneBigFloatRecord, destroyBigFloatRecord, textBigFloatRecord };
/**
 * @brief 从运行时通用壳借用大数宿主记录。
 * @param ctx 当前胶水调用状态。
 * @param handle 对象句柄参数。
 * @param who 操作的诊断名称。
 * @return 类型匹配且存活的记录，失败返回空。
 */
inline BigFloatRecord *bigFloatRecord(State &ctx, const Value &handle, const char *who)
{
    return record<BigFloatRecord>(ctx, handle, bigFloatType, who);
}
#endif
#if !STACKVM_WITH_BIGINT
/**
 * @brief 为未编入大整数胶水的构建返回原始裁剪诊断。
 * @param ctx 本次调用状态。
 * @return 零占位值。
 */
inline Value bigIntDisabled(State &ctx)
{
    ctx.error = "这个构建没有把大整数编进来（附加功能）：用 STACKVM_WITH_BIGINT=1 重建宿主胶水";
    return Value::fromInt(0);
}
#endif
#if STACKVM_WITH_BIGINT

/**
 * @brief 收下一个算好的大整数（并返回它的句柄）。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param value 待转换、保存或参与计算的值。
 * @retval 封装为原生值的运行时结果。
 */
inline Value nativeNewBigInt(State &ctx, bigint::BigInt value)
{
    auto *record = new BigIntRecord{ std::move(value) };
    const auto handle = ctx.host.makeNativeRecord(bigIntType, record);
    if (!handle) delete record;
    return Value::fromInt(handle);
}

/* ---- 大整数（bint）---- */

/**
 * @brief 执行大整数从整数构造并封装运行时结果。
 *
 * `__std_bint_from_int(值) -> bint`。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
inline Value bintFromInt(State &ctx, const Memory::Vector<Value> &args)
{
    return nativeNewBigInt(ctx, bigint::BigInt(args[0].isFloat() ? static_cast<std::int64_t>(args[0].floating) : args[0].integer));
}

/**
 * @brief 从无符号整数完整构造大数，保留 uint64 的高位。
 * @param ctx 原生调用上下文。
 * @param args 包含无符号整数位模式的参数列表。
 * @retval 新建的大数对象句柄。
 */
inline Value bintFromUInt(State &ctx, const Memory::Vector<Value> &args)
{
    return nativeNewBigInt(ctx, bigint::BigInt::fromU64(static_cast<std::uint64_t>(args[0].integer)));
}

/**
 * @brief 执行大整数从文本构造并封装运行时结果。
 *
 * `__std_bint_from_str(十进制文本地址) -> bint`。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
inline Value bintFromStr(State &ctx, const Memory::Vector<Value> &args)
{
    Memory::String text;
    if (!nativeReadCString(ctx, static_cast<std::size_t>(nativeInt(args[0])), &text))
    {
        ctx.error = "构造大整数时读不到文本（地址 " + Memory::toString(nativeInt(args[0])) + "）";
        return Value::fromInt(0);
    }
    bool ok = false;
    const bigint::BigInt value = bigint::BigInt::fromString(text, &ok);
    if (!ok)
    {
        ctx.error = "不是合法的大整数文本：\"" + text + "\"（只认可选的 +/- 号加十进制数字）";
        return Value::fromInt(0);
    }
    return nativeNewBigInt(ctx, value);
}

/**
 * @brief 执行大整数从浮点数构造并封装运行时结果。
 *
 * `__std_bint_from_flt(值) -> bint`（按向零截断）。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
inline Value bintFromFloat(State &ctx, const Memory::Vector<Value> &args)
{
    const double value = args[0].isFloat() ? args[0].floating : static_cast<double>(args[0].integer);
    return nativeNewBigInt(ctx, bigint::BigInt(static_cast<std::int64_t>(value)));
}

/**
 * @brief 执行大整数算术运算并封装运行时结果。
 *
 * 大整数的 `+ - * / %`：取两个句柄 → 算 → 把结果装进**新的**对象。
 * @param op `+` / `-` / `*` / `/` / `%`。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
inline Value bintArith(State &ctx, const Memory::Vector<Value> &args, char op)
{
    auto *left = bigIntRecord(ctx, args[0], "大整数运算");
    if (left == nullptr) return Value::fromInt(0);
    auto *right = bigIntRecord(ctx, args[1], "大整数运算");
    if (right == nullptr) return Value::fromInt(0);
    if ((op == '/' || op == '%') && right->number.isZero())
    {
        ctx.error = Memory::String("大整数 ") + op + " 0：除数是零";
        return Value::fromInt(0);
    }
    switch (op)
    {
    case '+': return nativeNewBigInt(ctx, left->number + right->number);
    case '-': return nativeNewBigInt(ctx, left->number - right->number);
    case '*': return nativeNewBigInt(ctx, left->number * right->number);
    case '/': return nativeNewBigInt(ctx, left->number / right->number);
    default: return nativeNewBigInt(ctx, left->number % right->number);
    }
}

/**
 * @brief 执行大整数加法并封装运行时结果。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
inline Value bintAdd(State &ctx, const Memory::Vector<Value> &args) { return bintArith(ctx, args, '+'); }
/**
 * @brief 执行大整数减法并封装运行时结果。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
inline Value bintSub(State &ctx, const Memory::Vector<Value> &args) { return bintArith(ctx, args, '-'); }
/**
 * @brief 执行大整数乘法并封装运行时结果。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
inline Value bintMul(State &ctx, const Memory::Vector<Value> &args) { return bintArith(ctx, args, '*'); }
/**
 * @brief 执行大整数除法并封装运行时结果。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
inline Value bintDiv(State &ctx, const Memory::Vector<Value> &args) { return bintArith(ctx, args, '/'); }
/**
 * @brief 执行大整数取余并封装运行时结果。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
inline Value bintMod(State &ctx, const Memory::Vector<Value> &args) { return bintArith(ctx, args, '%'); }

/**
 * @brief 执行大整数取反并封装运行时结果。
 *
 * `__std_bint_neg(值) -> bint`：取负。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
inline Value bintNeg(State &ctx, const Memory::Vector<Value> &args)
{
    auto *value = bigIntRecord(ctx, args[0], "大整数取负");
    if (value == nullptr) return Value::fromInt(0);
    return nativeNewBigInt(ctx, -value->number);
}

/**
 * @brief 执行大整数比较并封装运行时结果。
 *
 * `__std_bint_cmp(左, 右) -> -1 / 0 / 1`。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
inline Value bintCmp(State &ctx, const Memory::Vector<Value> &args)
{
    auto *left = bigIntRecord(ctx, args[0], "大整数比较");
    if (left == nullptr) return Value::fromInt(0);
    auto *right = bigIntRecord(ctx, args[1], "大整数比较");
    if (right == nullptr) return Value::fromInt(0);
    if (left->number < right->number) return Value::fromInt(-1);
    if (right->number < left->number) return Value::fromInt(1);
    return Value::fromInt(0);
}

/**
 * @brief 执行大整数转换为文本并封装运行时结果。
 *
 * `__std_bint_str(值) -> 新串地址`：转成十进制文本（`.str()`）。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
inline Value bintToStr(State &ctx, const Memory::Vector<Value> &args)
{
    auto *value = bigIntRecord(ctx, args[0], "大整数转字符串");
    if (value == nullptr) return Value::fromInt(0);
    return Value::fromInt(static_cast<std::int64_t>(nativeAllocString(ctx, value->number.toString())));
}

/**
 * @brief 大整数 → 64 位整数：取**低 64 位**（二进制补码截断，和 C 的整数转换一个脾气）。
 *
 * 不能借 `toDouble()` 绕（`double` 只有 53 位有效位，`(int)9223372036854775807` 会
 * 因为四舍五入溢出成 0）；这里按 2^32 进制两位两位地取余数拼出来。
 * @param value 大整数。
 * @return 值。
 */
inline std::int64_t bigIntToInt64(const bigint::BigInt &value)
{
    const bigint::BigInt base(4294967296LL); /* 2^32 */
    bigint::BigInt current = value.abs();
    std::uint64_t low = 0u;
    for (int index = 0; index < 2; ++index)
    {
        bigint::BigInt quotient;
        bigint::BigInt remainder;
        if (!current.divmod(base, &quotient, &remainder)) break;
        low |= static_cast<std::uint64_t>(remainder.toDouble()) << (32 * index);
        current = quotient;
    }
    const std::int64_t bits = static_cast<std::int64_t>(low);
    return (value.sign() < 0) ? -bits : bits;
}

/**
 * @brief 执行大整数转换为整数并封装运行时结果。
 *
 * `__std_bint_to_int(值) -> 整数`（超出 64 位取低 64 位）。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
inline Value bintToInt(State &ctx, const Memory::Vector<Value> &args)
{
    auto *value = bigIntRecord(ctx, args[0], "大整数转 int");
    if (value == nullptr) return Value::fromInt(0);
    return Value::fromInt(bigIntToInt64(value->number));
}

/**
 * @brief 执行大整数转换为浮点数并封装运行时结果。
 *
 * `__std_bint_to_flt(值) -> 浮点`。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
inline Value bintToFloat(State &ctx, const Memory::Vector<Value> &args)
{
    auto *value = bigIntRecord(ctx, args[0], "大整数转 double");
    if (value == nullptr) return Value::fromFloat(0.0);
    return Value::fromFloat(value->number.toDouble());
}

/**
 * @brief 跑一段会抛异常的大数运算，把异常翻译成 VM 错误。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param what 运算名称（拼进错误信息）。
 * @param operation 待执行的运算（返回结果槽）。
 * @retval 封装为原生值的运行时结果。
 */
template <typename Operation>
inline Value nativeGuardedBig(State &ctx, const char *what, Operation operation)
{
    try
    {
        return operation();
    } catch (const std::exception &error)
    {
        ctx.error = Memory::String(what) + "：" + error.what();
        return Value::fromInt(0);
    }
}

/* ---- 大整数扩展方法：幂 / 数论 / 素性 / 位运算 / 进制 / 开方 ---- */

/**
 * @brief `__std_bint_pow(值, 指数) -> bint`（`.pow(e)`，指数为非负 int64）。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
inline Value bintPow(State &ctx, const Memory::Vector<Value> &args)
{
    auto *value = bigIntRecord(ctx, args[0], "大整数幂运算");
    if (value == nullptr) return Value::fromInt(0);
    const std::int64_t exponent = nativeInt(args[1]);
    if (exponent < 0)
    {
        ctx.error = "BigInteger.pow 的指数不能是负数";
        return Value::fromInt(0);
    }
    return nativeGuardedBig(ctx, "大整数幂运算", [&] {
        return nativeNewBigInt(ctx, value->number.pow(static_cast<std::uint64_t>(exponent)));
    });
}

/**
 * @brief `__std_bint_powmod(值, 指数, 模) -> bint`（`.powmod(e, m)`，指数是大整数）。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
inline Value bintPowMod(State &ctx, const Memory::Vector<Value> &args)
{
    return nativeGuardedBig(ctx, "大整数模幂", [&] {
        auto *value = bigIntRecord(ctx, args[0], "大整数模幂");
        if (value == nullptr) return Value::fromInt(0);
        auto *exponent = bigIntRecord(ctx, args[1], "大整数模幂");
        if (exponent == nullptr) return Value::fromInt(0);
        auto *modulus = bigIntRecord(ctx, args[2], "大整数模幂");
        if (modulus == nullptr) return Value::fromInt(0);
        return nativeNewBigInt(ctx, value->number.powmod(exponent->number, modulus->number));
    });
}

/**
 * @brief `__std_bint_gcd(左, 右) -> bint`（`.gcd(other)`）。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
inline Value bintGcd(State &ctx, const Memory::Vector<Value> &args)
{
    auto *left = bigIntRecord(ctx, args[0], "大整数 gcd");
    if (left == nullptr) return Value::fromInt(0);
    auto *right = bigIntRecord(ctx, args[1], "大整数 gcd");
    if (right == nullptr) return Value::fromInt(0);
    return nativeNewBigInt(ctx, bigint::BigInt::gcd(left->number, right->number));
}

/**
 * @brief `__std_bint_lcm(左, 右) -> bint`（`.lcm(other)`）。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
inline Value bintLcm(State &ctx, const Memory::Vector<Value> &args)
{
    auto *left = bigIntRecord(ctx, args[0], "大整数 lcm");
    if (left == nullptr) return Value::fromInt(0);
    auto *right = bigIntRecord(ctx, args[1], "大整数 lcm");
    if (right == nullptr) return Value::fromInt(0);
    return nativeNewBigInt(ctx, bigint::BigInt::lcm(left->number, right->number));
}

/**
 * @brief `__std_bint_mod_nonneg(值, 模) -> bint`（`.mod(m)`，结果落在 `[0, |m|)`）。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
inline Value bintModNonNeg(State &ctx, const Memory::Vector<Value> &args)
{
    return nativeGuardedBig(ctx, "大整数非负取模", [&] {
        auto *value = bigIntRecord(ctx, args[0], "大整数非负取模");
        if (value == nullptr) return Value::fromInt(0);
        auto *modulus = bigIntRecord(ctx, args[1], "大整数非负取模");
        if (modulus == nullptr) return Value::fromInt(0);
        return nativeNewBigInt(ctx, value->number.modNonNegative(modulus->number));
    });
}

/**
 * @brief `__std_bint_mod_inverse(值, 模) -> bint`（`.modInverse(m)`，不互素报错）。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
inline Value bintModInverse(State &ctx, const Memory::Vector<Value> &args)
{
    return nativeGuardedBig(ctx, "大整数模逆元", [&] {
        auto *value = bigIntRecord(ctx, args[0], "大整数模逆元");
        if (value == nullptr) return Value::fromInt(0);
        auto *modulus = bigIntRecord(ctx, args[1], "大整数模逆元");
        if (modulus == nullptr) return Value::fromInt(0);
        return nativeNewBigInt(ctx, bigint::BigInt::modInverse(value->number, modulus->number));
    });
}

/**
 * @brief `__std_bint_is_probable_prime(值, 轮数) -> bool`（`.isProbablePrime(rounds)`）。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
inline Value bintIsProbablePrime(State &ctx, const Memory::Vector<Value> &args)
{
    auto *value = bigIntRecord(ctx, args[0], "大整数素性测试");
    if (value == nullptr) return Value::fromInt(0);
    const std::int64_t rounds = nativeInt(args[1]);
    if (rounds < 0)
    {
        ctx.error = "素性测试的轮数不能是负数";
        return Value::fromInt(0);
    }
    return Value::fromInt(value->number.isProbablePrime(static_cast<std::size_t>(rounds)) ? 1 : 0);
}

/**
 * @brief `__std_bint_next_probable_prime(值) -> bint`（`.nextProbablePrime()`，25 轮）。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
inline Value bintNextProbablePrime(State &ctx, const Memory::Vector<Value> &args)
{
    auto *value = bigIntRecord(ctx, args[0], "找下一个素数");
    if (value == nullptr) return Value::fromInt(0);
    return nativeGuardedBig(ctx, "找下一个素数", [&] {
        return nativeNewBigInt(ctx, value->number.nextProbablePrime());
    });
}

/**
 * @brief `__std_bint_bit_length(值) -> 整数`（`.bitLength()`）。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
inline Value bintBitLength(State &ctx, const Memory::Vector<Value> &args)
{
    auto *value = bigIntRecord(ctx, args[0], "大整数位长");
    if (value == nullptr) return Value::fromInt(0);
    return Value::fromInt(static_cast<std::int64_t>(value->number.bitLength()));
}

/**
 * @brief 大整数按位运算（`&` / `|` / `^`，只支持非负数）。
 * @param op 运算字符。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
inline Value bintBitwise(State &ctx, const Memory::Vector<Value> &args, char op)
{
    return nativeGuardedBig(ctx, "大整数位运算", [&] {
        auto *left = bigIntRecord(ctx, args[0], "大整数位运算");
        if (left == nullptr) return Value::fromInt(0);
        auto *right = bigIntRecord(ctx, args[1], "大整数位运算");
        if (right == nullptr) return Value::fromInt(0);
        switch (op)
        {
        case '&': return nativeNewBigInt(ctx, left->number & right->number);
        case '|': return nativeNewBigInt(ctx, left->number | right->number);
        default: return nativeNewBigInt(ctx, left->number ^ right->number);
        }
    });
}

/**
 * @brief `__std_bint_bit_and(左, 右) -> bint`（`.bitAnd(other)`）。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
inline Value bintBitAnd(State &ctx, const Memory::Vector<Value> &args) { return bintBitwise(ctx, args, '&'); }
/**
 * @brief `__std_bint_bit_or(左, 右) -> bint`（`.bitOr(other)`）。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
inline Value bintBitOr(State &ctx, const Memory::Vector<Value> &args) { return bintBitwise(ctx, args, '|'); }
/**
 * @brief `__std_bint_bit_xor(左, 右) -> bint`（`.bitXor(other)`）。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
inline Value bintBitXor(State &ctx, const Memory::Vector<Value> &args) { return bintBitwise(ctx, args, '^'); }

/**
 * @brief `__std_bint_str_radix(值, 进制) -> 新串地址`（`.strRadix(base)`，2..36 进制）。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
inline Value bintToStrRadix(State &ctx, const Memory::Vector<Value> &args)
{
    auto *value = bigIntRecord(ctx, args[0], "大整数转进制文本");
    if (value == nullptr) return Value::fromInt(0);
    const std::int64_t base = nativeInt(args[1]);
    if (base < 2 || base > 36)
    {
        ctx.error = "进制必须在 2..36 之间，现在是 " + Memory::toString(base);
        return Value::fromInt(0);
    }
    return nativeGuardedBig(ctx, "大整数转进制文本", [&] {
        return Value::fromInt(static_cast<std::int64_t>(
            nativeAllocString(ctx, value->number.toString(static_cast<unsigned int>(base)))));
    });
}

/**
 * @brief `__std_bint_iroot(值, k) -> bint`（`.iroot(k)`，整数 k 次方根向下取整）。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
inline Value bintIRoot(State &ctx, const Memory::Vector<Value> &args)
{
    auto *value = bigIntRecord(ctx, args[0], "大整数开方");
    if (value == nullptr) return Value::fromInt(0);
    const std::int64_t k = nativeInt(args[1]);
    if (k <= 0)
    {
        ctx.error = "根次必须是正整数";
        return Value::fromInt(0);
    }
    return nativeGuardedBig(ctx, "大整数开方", [&] {
        return nativeNewBigInt(ctx, bigint::integerRoot(value->number, static_cast<std::uint64_t>(k)));
    });
}

/**
 * @brief `__std_read_bint() -> bint`：把下一项输入按十进制读成一个大整数（`input_bstr`）。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @retval 封装为原生值的运行时结果。
 */
inline Value readBigInt(State &ctx, const Memory::Vector<Value> &)
{
    Memory::String text;
    if (!ctx.host.takeInputText(text))
    {
        ctx.error = "输入不够（第 " + Memory::toString(ctx.host.inputPosition() + 1u) + " 项缺失）";
        return Value::fromInt(0);
    }
    bool ok = false;
    const bigint::BigInt value = bigint::BigInt::fromString(text, &ok);
    if (!ok)
    {
        ctx.error = "输入项不是合法的大整数：\"" + text + "\"";
        return Value::fromInt(0);
    }
    return nativeNewBigInt(ctx, value);
}

#else /* !STACKVM_WITH_BIGINT：大整数没编进来，这几个名字都指向"一句人话" */

/* 名字照样在表里（调用点不用改），但一调就报"这个构建没有把大整数编进来" ——
 * 比"函数入口不存在"清楚得多。 */
#define STACKVM_BINT_STUB(name) \
    Value name(State &cx, const Memory::Vector<Value> &) { return bigIntDisabled(cx); }

STACKVM_BINT_STUB(bintFromInt)
STACKVM_BINT_STUB(bintFromUInt)
STACKVM_BINT_STUB(bintFromStr)
STACKVM_BINT_STUB(bintFromFloat)
STACKVM_BINT_STUB(bintAdd)
STACKVM_BINT_STUB(bintSub)
STACKVM_BINT_STUB(bintMul)
STACKVM_BINT_STUB(bintDiv)
STACKVM_BINT_STUB(bintMod)
STACKVM_BINT_STUB(bintNeg)
STACKVM_BINT_STUB(bintCmp)
STACKVM_BINT_STUB(bintToStr)
STACKVM_BINT_STUB(bintToInt)
STACKVM_BINT_STUB(bintToFloat)
STACKVM_BINT_STUB(bintPow)
STACKVM_BINT_STUB(bintPowMod)
STACKVM_BINT_STUB(bintGcd)
STACKVM_BINT_STUB(bintLcm)
STACKVM_BINT_STUB(bintModNonNeg)
STACKVM_BINT_STUB(bintModInverse)
STACKVM_BINT_STUB(bintIsProbablePrime)
STACKVM_BINT_STUB(bintNextProbablePrime)
STACKVM_BINT_STUB(bintBitLength)
STACKVM_BINT_STUB(bintBitAnd)
STACKVM_BINT_STUB(bintBitOr)
STACKVM_BINT_STUB(bintBitXor)
STACKVM_BINT_STUB(bintToStrRadix)
STACKVM_BINT_STUB(bintIRoot)
STACKVM_BINT_STUB(readBigInt)

#undef STACKVM_BINT_STUB

#endif /* STACKVM_WITH_BIGINT */

#if STACKVM_WITH_BIGFLOAT

/**
 * @brief 在对象存储中创建大浮点数对象。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param value 待转换、保存或参与计算的值。
 * @param precision 大浮点数的计算精度。
 * @retval 封装为原生值的运行时结果。
 */
inline Value nativeNewBigFloat(State &ctx, bigfloat::BigFloat value, int precision = 0)
{
    auto *record = new BigFloatRecord{ std::move(value), precision };
    const auto handle = ctx.host.makeNativeRecord(bigFloatType, record);
    if (!handle) delete record;
    return Value::fromInt(handle);
}

/**
 * @brief 执行大浮点数从整数构造并封装运行时结果。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
inline Value bfloatFromInt(State &ctx, const Memory::Vector<Value> &args)
{
    return nativeNewBigFloat(ctx, bigfloat::BigFloat::fromInt(
                                      bigint::BigInt(args[0].isFloat() ? static_cast<std::int64_t>(args[0].floating) : args[0].integer)));
}

/**
 * @brief 从无符号整数完整构造大数，保留 uint64 的高位。
 * @param ctx 原生调用上下文。
 * @param args 包含无符号整数位模式的参数列表。
 * @retval 新建的大数对象句柄。
 */
inline Value bfloatFromUInt(State &ctx, const Memory::Vector<Value> &args)
{
    return nativeNewBigFloat(ctx, bigfloat::BigFloat::fromInt(
                                      bigint::BigInt::fromU64(static_cast<std::uint64_t>(args[0].integer))));
}

/**
 * @brief 执行大浮点数从浮点数构造并封装运行时结果。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
inline Value bfloatFromFloat(State &ctx, const Memory::Vector<Value> &args)
{
    return nativeNewBigFloat(ctx, bigfloat::BigFloat::fromDouble(
                                      args[0].isFloat() ? args[0].floating : static_cast<double>(args[0].integer)));
}

/**
 * @brief 执行大浮点数从文本构造并封装运行时结果。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
inline Value bfloatFromStr(State &ctx, const Memory::Vector<Value> &args)
{
    Memory::String text;
    if (!nativeReadCString(ctx, static_cast<std::size_t>(nativeInt(args[0])), &text))
    {
        ctx.error = "构造大浮点数时读不到文本";
        return Value::fromInt(0);
    }
    bool ok = false;
    const bigfloat::BigFloat value = bigfloat::BigFloat::fromString(text, &ok);
    if (!ok)
    {
        ctx.error = "不是合法的大浮点数文本：\"" + text + "\"";
        return Value::fromInt(0);
    }
    return nativeNewBigFloat(ctx, value);
}

/**
 * @brief 执行大浮点数从大整数构造并封装运行时结果。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
inline Value bfloatFromBigInt(State &ctx, const Memory::Vector<Value> &args)
{
    auto *value = bigIntRecord(ctx, args[0], "大整数转大浮点数");
    if (value == nullptr) return Value::fromInt(0);
    return nativeNewBigFloat(ctx, bigfloat::BigFloat::fromInt(value->number));
}

/**
 * @brief 校验大浮点数操作数并执行指定算术运算。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @param op 运算符文本或类别。
 * @retval 封装为原生值的运行时结果。
 */
inline Value bfloatArith(State &ctx, const Memory::Vector<Value> &args, char op)
{
    auto *left = bigFloatRecord(ctx, args[0], "大浮点数运算");
    if (left == nullptr) return Value::fromInt(0);
    auto *right = bigFloatRecord(ctx, args[1], "大浮点数运算");
    if (right == nullptr) return Value::fromInt(0);
    if (op == '/' && right->decimal.isZero())
    {
        ctx.error = "大浮点数除数是零";
        return Value::fromInt(0);
    }
    /* 结果的精度取两个操作数精度的**最大值**（而不是只看左操作数）。 */
    const int precision = std::max(left->decimalPrecision, right->decimalPrecision);
    if (op == '+')
    {
        auto result = left->decimal + right->decimal;
        return nativeNewBigFloat(ctx, precision ? result.round(precision) : result, precision);
    }
    if (op == '-')
    {
        auto result = left->decimal - right->decimal;
        return nativeNewBigFloat(ctx, precision ? result.round(precision) : result, precision);
    }
    if (op == '*')
    {
        auto result = left->decimal * right->decimal;
        return nativeNewBigFloat(ctx, precision ? result.round(precision) : result, precision);
    }
    const int digits = precision != 0 ? precision : 34;
    return nativeNewBigFloat(ctx, left->decimal.divide(right->decimal, digits), precision);
}

/**
 * @brief 执行大浮点数加法并封装运行时结果。
 * @param c 原生调用上下文。
 * @param a 第一个操作数或节点编号。
 * @retval 封装为原生值的运行时结果。
 */
inline Value bfloatAdd(State &c, const Memory::Vector<Value> &a) { return bfloatArith(c, a, '+'); }
/**
 * @brief 执行大浮点数减法并封装运行时结果。
 * @param c 原生调用上下文。
 * @param a 第一个操作数或节点编号。
 * @retval 封装为原生值的运行时结果。
 */
inline Value bfloatSub(State &c, const Memory::Vector<Value> &a) { return bfloatArith(c, a, '-'); }
/**
 * @brief 执行大浮点数乘法并封装运行时结果。
 * @param c 原生调用上下文。
 * @param a 第一个操作数或节点编号。
 * @retval 封装为原生值的运行时结果。
 */
inline Value bfloatMul(State &c, const Memory::Vector<Value> &a) { return bfloatArith(c, a, '*'); }
/**
 * @brief 执行大浮点数除法并封装运行时结果。
 * @param c 原生调用上下文。
 * @param a 第一个操作数或节点编号。
 * @retval 封装为原生值的运行时结果。
 */
inline Value bfloatDiv(State &c, const Memory::Vector<Value> &a) { return bfloatArith(c, a, '/'); }

/**
 * @brief 执行大浮点数取反并封装运行时结果。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
inline Value bfloatNeg(State &ctx, const Memory::Vector<Value> &args)
{
    auto *value = bigFloatRecord(ctx, args[0], "大浮点数取负");
    return value ? nativeNewBigFloat(ctx, -value->decimal, value->decimalPrecision) : Value::fromInt(0);
}

/**
 * @brief 执行大浮点数比较并封装运行时结果。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
inline Value bfloatCmp(State &ctx, const Memory::Vector<Value> &args)
{
    auto *left = bigFloatRecord(ctx, args[0], "大浮点数比较");
    auto *right = bigFloatRecord(ctx, args[1], "大浮点数比较");
    return (left && right) ? Value::fromInt(left->decimal.compare(right->decimal)) : Value::fromInt(0);
}

/**
 * @brief 执行大浮点数转换为文本并封装运行时结果。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
inline Value bfloatToStr(State &ctx, const Memory::Vector<Value> &args)
{
    auto *value = bigFloatRecord(ctx, args[0], "大浮点数转字符串");
    if (value == nullptr) return Value::fromInt(0);
    return Value::fromInt(static_cast<std::int64_t>(nativeAllocString(ctx, value->decimal.toString(0))));
}

/**
 * @brief 执行大浮点数转换为整数并封装运行时结果。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
inline Value bfloatToInt(State &ctx, const Memory::Vector<Value> &args)
{
    auto *value = bigFloatRecord(ctx, args[0], "大浮点数转 int");
    if (value == nullptr) return Value::fromInt(0);
    bigint::BigInt result = value->decimal.mantissa();
    const int exponent = value->decimal.exponent10();
    bigint::BigInt scale(1);
    for (int i = 0; i < std::abs(exponent); ++i) scale *= bigint::BigInt(10);
    result = exponent >= 0 ? result * scale : result / scale;
    return Value::fromInt(bigIntToInt64(result));
}

/**
 * @brief 执行大浮点数转换为浮点数并封装运行时结果。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
inline Value bfloatToFloat(State &ctx, const Memory::Vector<Value> &args)
{
    auto *value = bigFloatRecord(ctx, args[0], "大浮点数转 double");
    if (value == nullptr) return Value::fromFloat(0.0);
    return Value::fromFloat(std::strtod(value->decimal.toString(17).c_str(), nullptr));
}

/**
 * @brief 设置大浮点数对象的有效精度。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
inline Value bfloatSetValuePrecision(State &ctx, const Memory::Vector<Value> &args)
{
    auto *value = bigFloatRecord(ctx, args[0], "BigDecimal.setPrecision");
    if (value == nullptr) return Value::fromInt(0);
    const std::int64_t digits = nativeInt(args[1]);
    if (digits < 1 || digits > 100000)
    {
        ctx.error = "BigDecimal 有效位数必须在 1..100000 之间，现在是 " + Memory::toString(digits);
        return Value::fromInt(0);
    }
    const int previous = value->decimalPrecision != 0 ? value->decimalPrecision :
                                                        34;
    value->decimal = value->decimal.round(static_cast<int>(digits));
    value->decimalPrecision = static_cast<int>(digits);
    return Value::fromInt(previous);
}

/**
 * @brief 提取序列化所需的大浮点数有效精度状态。
 * @param ctx 原生调用上下文。
 * @param args 包含待查询的大浮点数记录。
 * @return 显式有效位数；零表示沿用默认精度。
 */
Value bfloatSerializationPrecision(State &ctx, const Memory::Vector<Value> &args)
{
    const auto *value = bigFloatRecord(ctx, args[0], "BigDecimal 序列化精度");
    return Value::fromInt(value ? value->decimalPrecision : 0);
}

/* ---- 大浮点数扩展方法：幂 / 绝对值 / 取整 / 开方 ---- */

/**
 * @brief `__std_bfloat_pow(值, 指数) -> bfloat`（`.pow(e)`，精确整数次幂）。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
inline Value bfloatPow(State &ctx, const Memory::Vector<Value> &args)
{
    auto *value = bigFloatRecord(ctx, args[0], "大浮点数幂运算");
    if (value == nullptr) return Value::fromInt(0);
    const std::int64_t exponent = nativeInt(args[1]);
    if (exponent < 0)
    {
        ctx.error = "BigDecimal.pow 的指数不能是负数（负次幂请用除法自己算）";
        return Value::fromInt(0);
    }
    return nativeGuardedBig(ctx, "大浮点数幂运算", [&] {
        return nativeNewBigFloat(ctx, value->decimal.pow(static_cast<std::uint64_t>(exponent)),
                                 value->decimalPrecision);
    });
}

/**
 * @brief `__std_bfloat_abs(值) -> bfloat`（`.abs()`）。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
inline Value bfloatAbs(State &ctx, const Memory::Vector<Value> &args)
{
    auto *value = bigFloatRecord(ctx, args[0], "大浮点数绝对值");
    if (value == nullptr) return Value::fromInt(0);
    return nativeNewBigFloat(ctx, value->decimal.abs(), value->decimalPrecision);
}

/**
 * @brief `__std_bfloat_is_integer(值) -> bool`（`.isInteger()`）。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
inline Value bfloatIsInteger(State &ctx, const Memory::Vector<Value> &args)
{
    auto *value = bigFloatRecord(ctx, args[0], "大浮点数整数判断");
    if (value == nullptr) return Value::fromInt(0);
    return Value::fromInt(value->decimal.isInteger() ? 1 : 0);
}

/**
 * @brief 大浮点数取整家族（floor / ceil / trunc，结果是大整数）。
 * @param mode 取整方式：'f' 向负无穷 / 'c' 向正无穷 / 't' 向零。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
inline Value bfloatRoundToBigInt(State &ctx, const Memory::Vector<Value> &args, char mode)
{
    auto *value = bigFloatRecord(ctx, args[0], "大浮点数取整");
    if (value == nullptr) return Value::fromInt(0);
    switch (mode)
    {
    case 'f': return nativeNewBigInt(ctx, value->decimal.floor());
    case 'c': return nativeNewBigInt(ctx, value->decimal.ceil());
    default: return nativeNewBigInt(ctx, value->decimal.trunc());
    }
}

/**
 * @brief `__std_bfloat_floor(值) -> bint`（`.floor()`）。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
inline Value bfloatFloor(State &ctx, const Memory::Vector<Value> &args) { return bfloatRoundToBigInt(ctx, args, 'f'); }
/**
 * @brief `__std_bfloat_ceil(值) -> bint`（`.ceil()`）。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
inline Value bfloatCeil(State &ctx, const Memory::Vector<Value> &args) { return bfloatRoundToBigInt(ctx, args, 'c'); }
/**
 * @brief `__std_bfloat_trunc(值) -> bint`（`.trunc()`）。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
inline Value bfloatTrunc(State &ctx, const Memory::Vector<Value> &args) { return bfloatRoundToBigInt(ctx, args, 't'); }

/**
 * @brief `__std_bfloat_sqrt(值, 有效位数) -> bfloat`（`.sqrt(digits)`）。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
inline Value bfloatSqrt(State &ctx, const Memory::Vector<Value> &args)
{
    auto *value = bigFloatRecord(ctx, args[0], "大浮点数开平方");
    if (value == nullptr) return Value::fromInt(0);
    const std::int64_t digits = nativeInt(args[1]);
    if (digits < 1 || digits > 100000)
    {
        ctx.error = "BigDecimal.sqrt 的有效位数必须在 1..100000 之间，现在是 " + Memory::toString(digits);
        return Value::fromInt(0);
    }
    return nativeGuardedBig(ctx, "大浮点数开平方", [&] {
        return nativeNewBigFloat(ctx, value->decimal.sqrt(static_cast<int>(digits)), value->decimalPrecision);
    });
}

/**
 * @brief `__std_bfloat_root(值, k, 有效位数) -> bfloat`（`.root(k, digits)`，k 次方根）。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @param args 按接口约定顺序传入的实参列表。
 * @retval 封装为原生值的运行时结果。
 */
inline Value bfloatRoot(State &ctx, const Memory::Vector<Value> &args)
{
    auto *value = bigFloatRecord(ctx, args[0], "大浮点数开 k 次方");
    if (value == nullptr) return Value::fromInt(0);
    const std::int64_t k = nativeInt(args[1]);
    const std::int64_t digits = nativeInt(args[2]);
    if (k <= 0)
    {
        ctx.error = "BigDecimal.root 的根次必须是正整数";
        return Value::fromInt(0);
    }
    if (digits < 1 || digits > 100000)
    {
        ctx.error = "BigDecimal.root 的有效位数必须在 1..100000 之间，现在是 " + Memory::toString(digits);
        return Value::fromInt(0);
    }
    return nativeGuardedBig(ctx, "大浮点数开 k 次方", [&] {
        return nativeNewBigFloat(ctx, value->decimal.root(static_cast<std::uint64_t>(k), static_cast<int>(digits)),
                                 value->decimalPrecision);
    });
}

/**
 * @brief 从对象句柄读取大浮点数。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @retval 封装为原生值的运行时结果。
 */
inline Value readBigFloat(State &ctx, const Memory::Vector<Value> &)
{
    Memory::String text;
    if (!ctx.host.takeInputText(text))
    {
        ctx.error = "输入不够，取不到大浮点数";
        return Value::fromInt(0);
    }
    bool ok = false;
    const bigfloat::BigFloat value = bigfloat::BigFloat::fromString(text, &ok);
    if (!ok)
    {
        ctx.error = "输入项不是合法的大浮点数：\"" + text + "\"";
        return Value::fromInt(0);
    }
    return nativeNewBigFloat(ctx, value);
}

/**
 * @brief 使用共享十进制数学后端计算泛型 std::math 的大浮点分支。
 * @param ctx 当前宿主状态，保留定义域或精度错误。
 * @param args 依次包含左数值、运算编号、右数值；一元调用复用左数值作为右参数。
 * @return 新建的 BigDecimal，保持实际计算有效位数。
 */
Value bfloatMath(State &ctx, const Memory::Vector<Value> &args)
{
    auto *left = bigFloatRecord(ctx, args[0], "高精度数学");
    auto *right = bigFloatRecord(ctx, args[2], "高精度数学");
    if (!left || !right) return Value::fromInt(0);
    const int precision = std::max(left->decimalPrecision, right->decimalPrecision);
    const int digits = precision ? precision : 34;
    try
    {
        bigfloat::BigFloat result;
        bool exact = false;
        switch (args[1].asInt())
        {
        case 0: result = bigfloat::sin(left->decimal, digits); break;
        case 1: result = bigfloat::cos(left->decimal, digits); break;
        case 2: result = bigfloat::tan(left->decimal, digits); break;
        case 3: result = bigfloat::asin(left->decimal, digits); break;
        case 4: result = bigfloat::acos(left->decimal, digits); break;
        case 5: result = bigfloat::atan(left->decimal, digits); break;
        case 6: result = bigfloat::exp(left->decimal, digits); break;
        case 7: result = bigfloat::ln(left->decimal, digits); break;
        case 8: result = bigfloat::log10(left->decimal, digits); break;
        case 9: result = left->decimal.sqrt(digits); break;
        case 10:
            result = bigfloat::BigFloat::fromInt(left->decimal.floor());
            exact = true;
            break;
        case 11:
            result = bigfloat::BigFloat::fromInt(left->decimal.ceil());
            exact = true;
            break;
        case 12:
            result = bigfloat::BigFloat::fromInt(left->decimal.sign() < 0 ? (left->decimal - bigfloat::BigFloat("0.5")).ceil() : (left->decimal + bigfloat::BigFloat("0.5")).floor());
            exact = true;
            break;
        case 13:
            result = bigfloat::BigFloat::fromInt(left->decimal.trunc());
            exact = true;
            break;
        case 14: result = bigfloat::pow(left->decimal, right->decimal, digits); break;
        case 15:
            result = bigfloat::fmod(left->decimal, right->decimal);
            exact = true;
            break;
        default: throw std::invalid_argument("高精度数学运算编号无效");
        }
        return nativeNewBigFloat(ctx, std::move(result), exact ? precision : digits);
    } catch (const std::exception &error)
    {
        ctx.error = Memory::String("std::math 高精度计算：") + error.what();
        return Value::fromInt(0);
    }
}

/**
 * @brief 构造指定有效位数的高精度圆周率。
 * @param ctx 当前宿主调用状态。
 * @param args 包含 1..100000 范围的有效位数。
 * @return 保留对应计算精度的 BigDecimal 记录。
 */
Value bfloatPi(State &ctx, const Memory::Vector<Value> &args)
{
    const auto digits = args[0].asInt();
    if (digits < 1 || digits > 100000)
    {
        ctx.error = "圆周率有效位数必须在 1..100000 之间";
        return Value::fromInt(0);
    }
    return nativeNewBigFloat(ctx, bigfloat::pi(static_cast<int>(digits)), static_cast<int>(digits));
}

#else

/**
 * @brief 报告当前构建未启用大浮点数功能。
 * @param ctx 原生调用上下文，提供内存访问与错误报告。
 * @retval 封装为原生值的运行时结果。
 */
inline Value bigFloatDisabled(State &ctx)
{
    ctx.error = "这个构建没有把大浮点数编进来：用 STACKVM_WITH_BIGFLOAT=1 重建虚拟机";
    return Value::fromInt(0);
}
#define STACKVM_BFLOAT_STUB(name) \
    Value name(State &c, const Memory::Vector<Value> &) { return bigFloatDisabled(c); }
STACKVM_BFLOAT_STUB(bfloatFromInt)
STACKVM_BFLOAT_STUB(bfloatFromUInt)
STACKVM_BFLOAT_STUB(bfloatFromFloat)
STACKVM_BFLOAT_STUB(bfloatFromStr)
STACKVM_BFLOAT_STUB(bfloatFromBigInt)
STACKVM_BFLOAT_STUB(bfloatAdd)
STACKVM_BFLOAT_STUB(bfloatSub)
STACKVM_BFLOAT_STUB(bfloatMul)
STACKVM_BFLOAT_STUB(bfloatDiv)
STACKVM_BFLOAT_STUB(bfloatNeg)
STACKVM_BFLOAT_STUB(bfloatCmp)
STACKVM_BFLOAT_STUB(bfloatToStr)
STACKVM_BFLOAT_STUB(bfloatToInt)
STACKVM_BFLOAT_STUB(bfloatToFloat)
STACKVM_BFLOAT_STUB(bfloatSetValuePrecision)
STACKVM_BFLOAT_STUB(bfloatSerializationPrecision)
STACKVM_BFLOAT_STUB(bfloatPow)
STACKVM_BFLOAT_STUB(bfloatAbs)
STACKVM_BFLOAT_STUB(bfloatIsInteger)
STACKVM_BFLOAT_STUB(bfloatFloor)
STACKVM_BFLOAT_STUB(bfloatCeil)
STACKVM_BFLOAT_STUB(bfloatTrunc)
STACKVM_BFLOAT_STUB(bfloatSqrt)
STACKVM_BFLOAT_STUB(bfloatRoot)
STACKVM_BFLOAT_STUB(readBigFloat)
STACKVM_BFLOAT_STUB(bfloatMath)
STACKVM_BFLOAT_STUB(bfloatPi)
#undef STACKVM_BFLOAT_STUB

#endif

/**
 * @brief 将全部大数操作注册为普通胶水函数。
 * @param registry 目标宿主注册表。
 * @param error 接收绑定失败原因。
 * @return 全部注册成功时为 true。
 */
bool bind(stackvm::NativeRegistry &registry, Memory::String *error)
{
    bool ok = true;
    ok = native_record_glue::bind(registry, "__std_bint_from_int", 1, bintFromInt, error) && ok;
    ok = native_record_glue::bind(registry, "__std_bint_from_uint", 1, bintFromUInt, error) && ok;
    ok = native_record_glue::bind(registry, "__std_bint_from_str", 1, bintFromStr, error) && ok;
    ok = native_record_glue::bind(registry, "__std_bint_from_flt", 1, bintFromFloat, error) && ok;
    ok = native_record_glue::bind(registry, "__std_bint_add", 2, bintAdd, error) && ok;
    ok = native_record_glue::bind(registry, "__std_bint_sub", 2, bintSub, error) && ok;
    ok = native_record_glue::bind(registry, "__std_bint_mul", 2, bintMul, error) && ok;
    ok = native_record_glue::bind(registry, "__std_bint_div", 2, bintDiv, error) && ok;
    ok = native_record_glue::bind(registry, "__std_bint_mod", 2, bintMod, error) && ok;
    ok = native_record_glue::bind(registry, "__std_bint_neg", 1, bintNeg, error) && ok;
    ok = native_record_glue::bind(registry, "__std_bint_cmp", 2, bintCmp, error) && ok;
    ok = native_record_glue::bind(registry, "__std_bint_str", 1, bintToStr, error) && ok;
    ok = native_record_glue::bind(registry, "__std_bint_to_int", 1, bintToInt, error) && ok;
    ok = native_record_glue::bind(registry, "__std_bint_to_flt", 1, bintToFloat, error) && ok;
    ok = native_record_glue::bind(registry, "__std_bint_pow", 2, bintPow, error) && ok;
    ok = native_record_glue::bind(registry, "__std_bint_powmod", 3, bintPowMod, error) && ok;
    ok = native_record_glue::bind(registry, "__std_bint_gcd", 2, bintGcd, error) && ok;
    ok = native_record_glue::bind(registry, "__std_bint_lcm", 2, bintLcm, error) && ok;
    ok = native_record_glue::bind(registry, "__std_bint_mod_nonneg", 2, bintModNonNeg, error) && ok;
    ok = native_record_glue::bind(registry, "__std_bint_mod_inverse", 2, bintModInverse, error) && ok;
    ok = native_record_glue::bind(registry, "__std_bint_is_probable_prime", 2, bintIsProbablePrime, error) && ok;
    ok = native_record_glue::bind(registry, "__std_bint_next_probable_prime", 1, bintNextProbablePrime, error) && ok;
    ok = native_record_glue::bind(registry, "__std_bint_bit_length", 1, bintBitLength, error) && ok;
    ok = native_record_glue::bind(registry, "__std_bint_bit_and", 2, bintBitAnd, error) && ok;
    ok = native_record_glue::bind(registry, "__std_bint_bit_or", 2, bintBitOr, error) && ok;
    ok = native_record_glue::bind(registry, "__std_bint_bit_xor", 2, bintBitXor, error) && ok;
    ok = native_record_glue::bind(registry, "__std_bint_str_radix", 2, bintToStrRadix, error) && ok;
    ok = native_record_glue::bind(registry, "__std_bint_iroot", 2, bintIRoot, error) && ok;
    ok = native_record_glue::bind(registry, "__std_read_bint", 0, readBigInt, error) && ok;
    ok = native_record_glue::bind(registry, "__std_input_bint", 0, readBigInt, error) && ok;
    ok = native_record_glue::bind(registry, "__std_bfloat_from_int", 1, bfloatFromInt, error) && ok;
    ok = native_record_glue::bind(registry, "__std_bfloat_from_uint", 1, bfloatFromUInt, error) && ok;
    ok = native_record_glue::bind(registry, "__std_bfloat_from_flt", 1, bfloatFromFloat, error) && ok;
    ok = native_record_glue::bind(registry, "__std_bfloat_from_str", 1, bfloatFromStr, error) && ok;
    ok = native_record_glue::bind(registry, "__std_bfloat_from_bint", 1, bfloatFromBigInt, error) && ok;
    ok = native_record_glue::bind(registry, "__std_bfloat_add", 2, bfloatAdd, error) && ok;
    ok = native_record_glue::bind(registry, "__std_bfloat_sub", 2, bfloatSub, error) && ok;
    ok = native_record_glue::bind(registry, "__std_bfloat_mul", 2, bfloatMul, error) && ok;
    ok = native_record_glue::bind(registry, "__std_bfloat_div", 2, bfloatDiv, error) && ok;
    ok = native_record_glue::bind(registry, "__std_bfloat_neg", 1, bfloatNeg, error) && ok;
    ok = native_record_glue::bind(registry, "__std_bfloat_cmp", 2, bfloatCmp, error) && ok;
    ok = native_record_glue::bind(registry, "__std_bfloat_str", 1, bfloatToStr, error) && ok;
    ok = native_record_glue::bind(registry, "__std_bfloat_math", 3, bfloatMath, error) && ok;
    ok = native_record_glue::bind(registry, "std::math::__std_bfloat_math", 3, bfloatMath, error) && ok;
    ok = native_record_glue::bind(registry, "__std_bfloat_pi", 1, bfloatPi, error) && ok;
    ok = native_record_glue::bind(registry, "std::math::__std_bfloat_pi", 1, bfloatPi, error) && ok;
    ok = native_record_glue::bind(registry, "__std_bfloat_to_int", 1, bfloatToInt, error) && ok;
    ok = native_record_glue::bind(registry, "__std_bfloat_pow", 2, bfloatPow, error) && ok;
    ok = native_record_glue::bind(registry, "__std_bfloat_abs", 1, bfloatAbs, error) && ok;
    ok = native_record_glue::bind(registry, "__std_bfloat_is_integer", 1, bfloatIsInteger, error) && ok;
    ok = native_record_glue::bind(registry, "__std_bfloat_floor", 1, bfloatFloor, error) && ok;
    ok = native_record_glue::bind(registry, "__std_bfloat_ceil", 1, bfloatCeil, error) && ok;
    ok = native_record_glue::bind(registry, "__std_bfloat_trunc", 1, bfloatTrunc, error) && ok;
    ok = native_record_glue::bind(registry, "__std_bfloat_sqrt", 2, bfloatSqrt, error) && ok;
    ok = native_record_glue::bind(registry, "__std_bfloat_root", 3, bfloatRoot, error) && ok;
    ok = native_record_glue::bind(registry, "__std_bfloat_to_flt", 1, bfloatToFloat, error) && ok;
    ok = native_record_glue::bind(registry, "__std_bfloat_set_value_precision", 2, bfloatSetValuePrecision, error) && ok;
    ok = native_record_glue::bind(registry, "std::reflect::__serdeBigPrecision", 1, bfloatSerializationPrecision, error) && ok;
    ok = native_record_glue::bind(registry, "__std_read_bfloat", 0, readBigFloat, error) && ok;
    ok = native_record_glue::bind(registry, "__std_input_bfloat", 0, readBigFloat, error) && ok;
    ok = native_record_glue::bind(registry, "__std_bint_copy", 1, native_record_glue::copy, error) && ok;
    ok = native_record_glue::bind(registry, "__std_bint_drop", 1, native_record_glue::drop, error) && ok;
    ok = native_record_glue::bind(registry, "__std_bfloat_copy", 1, native_record_glue::copy, error) && ok;
    ok = native_record_glue::bind(registry, "__std_bfloat_drop", 1, native_record_glue::drop, error) && ok;
    return ok;
}
}  // namespace big_number_glue
