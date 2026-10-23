#if !defined(NO_CHALI_BIG_INT) && !defined(NO_CHALI_BIG_FLOAT)
#include "bigfloat_math.hpp"
#include "bigfloat_math_detail.hpp"
#include <algorithm>
#include <stdexcept>

namespace bigfloat
{
namespace mathDetail
{
/**
 * @brief 检查精度并保留保护位，限制中间结果的工作规模。
 * @param digits 当前层请求的有效位数，包含嵌套调用的保护位，范围为 1..250000。
 * @param extra 本次消去或参数约化所需的额外位数。
 * @return 加入 32 位保护位的工作精度。
 */
int workPrecision(int digits, std::int64_t extra)
{
    if (digits < 1 || digits > 250000) throw std::out_of_range("高精度数学工作有效位数必须在 1..250000 之间");
    const auto work = static_cast<std::int64_t>(digits) + 32 + extra;
    if (extra < 0 || work > 300032) throw std::out_of_range("高精度数学参数约化超出工作位数预算");
    return static_cast<int>(work);
}
/**
 * @brief 通过尾数位数与十进制指数取得幅值的数量级。
 * @param value 输入十进制数。
 * @return 最高有效十进制位的指数，零返回 0。
 */
std::int64_t decimalOrder(const BigFloat &value)
{
    if (value.isZero()) return 0;
    return static_cast<std::int64_t>(value.mantissa().decimalDigits()) - 1 + value.exponent10();
}
}  // namespace mathDetail
namespace
{
/**
 * @brief 对适中正数使用平方根约化和反双曲正切级数求对数。
 * @param input 正数，接近一的输入保留原始消去精度。
 * @param work 已验证的内部有效位数。
 * @return 内部精度的自然对数。
 */
BigFloat logarithmSeries(const BigFloat &input, int work)
{
    BigFloat value = input;
    int reductions = 0;
    while (value > BigFloat("1.1") || value < BigFloat("0.9"))
    {
        value = value.sqrt(work);
        ++reductions;
    }
    const auto ratio = (value - BigFloat(1)).divide(value + BigFloat(1), work);
    const auto square = (ratio * ratio).round(work);
    BigFloat term = ratio, sum = ratio;
    for (int index = 1; index <= work * 2 + 64; ++index)
    {
        term = (term * square).round(work);
        const auto next = (sum + term.divide(BigFloat(static_cast<std::int64_t>(index) * 2 + 1), work)).round(work);
        if (next == sum)
        {
            const auto scale = BigFloat::fromInt(bigint::BigInt(2).pow(static_cast<std::uint64_t>(reductions) + 1));
            return (sum * scale).round(work);
        }
        sum = next;
    }
    throw std::runtime_error("高精度对数级数未在预算内收敛");
}
}  // namespace
/**
 * @brief 使用 Gauss–Legendre AGM 迭代计算圆周率。
 * @param digits 结果有效位数。
 * @return 舍入后的圆周率。
 */
BigFloat pi(int digits)
{
    const int work = mathDetail::workPrecision(digits);
    BigFloat a(1), b = BigFloat(1).divide(BigFloat(2).sqrt(work), work);
    BigFloat t("0.25"), scale(1);
    for (int iteration = 0; iteration < 64; ++iteration)
    {
        const auto nextA = (a + b).divide(BigFloat(2), work);
        const auto nextB = (a * b).round(work).sqrt(work);
        const auto delta = a - nextA;
        t = (t - scale * (delta * delta).round(work)).round(work);
        if (nextA == a && nextB == b)
        {
            const auto sum = nextA + nextB;
            return (sum * sum).divide(BigFloat(4) * t, work).round(digits);
        }
        a = nextA;
        b = nextB;
        scale *= BigFloat(2);
    }
    throw std::runtime_error("高精度圆周率迭代未收敛");
}
/**
 * @brief 将指数约化到小区间并以 Taylor 级数和重复平方计算指数。
 * @param value 实数指数。
 * @param digits 结果有效位数。
 * @return 舍入后的自然指数。
 */
BigFloat exp(const BigFloat &value, int digits)
{
    int work = mathDetail::workPrecision(digits, 16);
    if (value.isZero()) return BigFloat(1);
    if (value.abs() > BigFloat("4944763830")) throw std::overflow_error("自然指数结果超出十进制指数范围");
    BigFloat reduced = value.abs();
    int reductions = 0;
    while (reduced > BigFloat("0.125"))
    {
        reduced = reduced.divide(BigFloat(2), work);
        ++reductions;
    }
    BigFloat term(1), sum(1);
    bool converged = false;
    for (int index = 1; index <= work * 2 + 64; ++index)
    {
        term = (term * reduced).divide(BigFloat(index), work);
        const auto next = (sum + term).round(work);
        if (next == sum)
        {
            converged = true;
            break;
        }
        sum = next;
    }
    if (!converged) throw std::runtime_error("高精度指数级数未收敛");
    while (reductions-- > 0) sum = (sum * sum).round(work);
    if (value.sign() < 0) sum = BigFloat(1).divide(sum, work);
    return sum.round(digits);
}
/**
 * @brief 按十进制数量级分离输入，并使用对数级数求精确工作精度结果。
 * @param value 严格为正的实数。
 * @param digits 结果有效位数。
 * @return 舍入后的自然对数。
 */
BigFloat ln(const BigFloat &value, int digits)
{
    const int work = mathDetail::workPrecision(digits);
    if (value.sign() <= 0) throw std::domain_error("自然对数要求正数");
    if (value >= BigFloat("0.5") && value <= BigFloat(2)) return logarithmSeries(value, work).round(digits);
    auto text = value.mantissa().toString();
    const auto shift = static_cast<std::int64_t>(text.size()) - 1;
    text += "e";
    text += Memory::toString(-shift);
    const BigFloat coefficient(text);
    const auto order = mathDetail::decimalOrder(value);
    auto result = logarithmSeries(coefficient, work);
    if (order != 0) result = (result + BigFloat(order) * logarithmSeries(BigFloat(10), work)).round(work);
    return result.round(digits);
}
/**
 * @brief 计算常用对数，十的整数幂直接返回精确指数。
 * @param value 正实数。
 * @param digits 结果有效位数。
 * @return 舍入后的常用对数。
 */
BigFloat log10(const BigFloat &value, int digits)
{
    const int work = mathDetail::workPrecision(digits);
    if (value.sign() <= 0) throw std::domain_error("常用对数要求正数");
    if (value.mantissa() == bigint::BigInt(1)) return BigFloat(value.exponent10()).round(digits);
    return ln(value, digits + 16).divide(logarithmSeries(BigFloat(10), work), work).round(digits);
}
/**
 * @brief 用定精度二进制幂或指数对数计算实数乘方。
 * @param value 底数。
 * @param exponent 实数指数。
 * @param digits 结果有效位数。
 * @return 舍入后的实数幂。
 */
BigFloat pow(const BigFloat &value, const BigFloat &exponent, int digits)
{
    const int work = mathDetail::workPrecision(digits);
    if (exponent.isZero()) return BigFloat(1);
    if (value.isZero())
    {
        if (exponent.sign() < 0) throw std::domain_error("零不能取负次幂");
        return BigFloat();
    }
    if (exponent.isInteger())
    {
        const auto integer = exponent.trunc();
        if (integer.bitLength() > 64u) throw std::overflow_error("整数幂指数超出 64 位范围");
        auto remaining = integer.toU64();
        BigFloat result(1), base = value.round(work);
        while (remaining != 0u)
        {
            if (remaining & 1u) result = (result * base).round(work);
            remaining >>= 1u;
            if (remaining) base = (base * base).round(work);
        }
        if (exponent.sign() < 0) result = BigFloat(1).divide(result, work);
        return result.round(digits);
    }
    if (value.sign() < 0) throw std::domain_error("负底数的非整数次幂不属于实数");
    return exp((exponent * ln(value, digits + 8)).round(work), digits);
}
/**
 * @brief 将十进制指数对齐到整数模运算，避免有限精度除法误判商。
 * @param value 被除数。
 * @param divisor 非零除数。
 * @return 精确余数，符号跟随被除数。
 */
BigFloat fmod(const BigFloat &value, const BigFloat &divisor)
{
    if (divisor.isZero()) throw std::domain_error("十进制余数的除数不能为零");
    if (value.isZero() || value.abs() < divisor.abs()) return value;
    const auto shift = static_cast<std::int64_t>(value.exponent10()) - divisor.exponent10();
    bigint::BigInt remainder;
    int exponent;
    if (shift >= 0)
    {
        const auto scale = bigint::BigInt(10).powmod(static_cast<std::uint64_t>(shift), divisor.mantissa().abs());
        remainder = (value.mantissa() * scale) % divisor.mantissa();
        exponent = divisor.exponent10();
    }
    else
    {
        remainder = value.mantissa() % (divisor.mantissa() * bigint::pow10(static_cast<std::uint64_t>(-shift)));
        exponent = value.exponent10();
    }
    return BigFloat(remainder.toString() + "e" + Memory::toString(exponent));
}
}  // namespace bigfloat
#endif
