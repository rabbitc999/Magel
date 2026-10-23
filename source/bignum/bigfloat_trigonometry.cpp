#if !defined(NO_CHALI_BIG_INT) && !defined(NO_CHALI_BIG_FLOAT)
#include "bigfloat_math.hpp"
#include "bigfloat_math_detail.hpp"
#include <algorithm>
#include <stdexcept>
#include <utility>

namespace bigfloat
{
namespace
{
/**
 * @brief 为三角参数约化和接近特殊角时的消去保留额外精度。
 * @param value 原始弧度值。
 * @param digits 需要的结果有效位数。
 * @return 包含参数数量级和原始尾数信息的工作位数。
 */
int trigPrecision(const BigFloat &value, int digits)
{
    const auto magnitude = std::max<std::int64_t>(0, mathDetail::decimalOrder(value));
    return mathDetail::workPrecision(digits, magnitude + static_cast<std::int64_t>(value.mantissa().decimalDigits()));
}
/**
 * @brief 同时计算正弦和余弦，先模圆周率约化，再缩小角度并复用倍角。
 * @param value 有限弧度角。
 * @param work 包含消去保护位的工作精度。
 * @return 工作精度下的正弦与余弦。
 */
std::pair<BigFloat, BigFloat> sinCosWorking(const BigFloat &value, int work)
{
    if (value.isZero()) return { BigFloat(), BigFloat(1) };
    const auto circle = pi(work);
    const auto period = circle * BigFloat(2);
    const auto quotient = value.divide(period, work).trunc();
    BigFloat reduced = value - BigFloat::fromInt(quotient) * period;
    if (reduced > circle) reduced -= period;
    if (reduced < -circle) reduced += period;
    int reductions = 0;
    while (reduced.abs() > BigFloat("0.125"))
    {
        reduced = reduced.divide(BigFloat(2), work);
        ++reductions;
    }
    const auto square = (reduced * reduced).round(work);
    BigFloat sine = reduced, cosine(1), sineTerm = reduced, cosineTerm(1);
    bool converged = false;
    for (int index = 1; index <= work * 2 + 64; ++index)
    {
        const auto twice = static_cast<std::int64_t>(index) * 2;
        sineTerm = (-sineTerm * square).divide(BigFloat(twice * (twice + 1)), work);
        cosineTerm = (-cosineTerm * square).divide(BigFloat((twice - 1) * twice), work);
        const auto nextSine = (sine + sineTerm).round(work);
        const auto nextCosine = (cosine + cosineTerm).round(work);
        if (nextSine == sine && nextCosine == cosine)
        {
            converged = true;
            break;
        }
        sine = nextSine;
        cosine = nextCosine;
    }
    if (!converged) throw std::runtime_error("高精度三角级数未收敛");
    while (reductions-- > 0)
    {
        const auto nextSine = (BigFloat(2) * sine * cosine).round(work);
        cosine = (cosine * cosine - sine * sine).round(work);
        sine = nextSine;
    }
    return { sine, cosine };
}
/**
 * @brief 使用倒数和半角约化后的交错级数计算反正切。
 * @param value 任意有限实数。
 * @param work 已验证的工作有效位数。
 * @return 工作精度下的反正切主值。
 */
BigFloat atanWorking(const BigFloat &value, int work)
{
    if (value.isZero()) return BigFloat();
    const bool inverse = value.abs() > BigFloat(1);
    BigFloat reduced = inverse ? BigFloat(1).divide(value.abs(), work) : value.abs();
    int reductions = 0;
    while (reduced > BigFloat("0.1"))
    {
        reduced = reduced.divide(BigFloat(1) + (BigFloat(1) + reduced * reduced).sqrt(work), work);
        ++reductions;
    }
    const auto square = (reduced * reduced).round(work);
    BigFloat term = reduced, sum = reduced;
    bool converged = false;
    for (int index = 1; index <= work * 2 + 64; ++index)
    {
        term = (-term * square).round(work);
        const auto next = (sum + term.divide(BigFloat(static_cast<std::int64_t>(index) * 2 + 1), work)).round(work);
        if (next == sum)
        {
            converged = true;
            break;
        }
        sum = next;
    }
    if (!converged) throw std::runtime_error("高精度反正切级数未收敛");
    sum = (sum * BigFloat::fromInt(bigint::BigInt(2).pow(static_cast<std::uint64_t>(reductions)))).round(work);
    if (inverse) sum = pi(work).divide(BigFloat(2), work) - sum;
    return value.sign() < 0 ? -sum : sum;
}
}  // namespace
/**
 * @brief 计算带消去保护位的高精度正弦。
 * @param value 弧度角。
 * @param digits 结果有效位数。
 * @return 舍入后的正弦值。
 */
BigFloat sin(const BigFloat &value, int digits)
{
    return sinCosWorking(value, trigPrecision(value, digits)).first.round(digits);
}
/**
 * @brief 计算带消去保护位的高精度余弦。
 * @param value 弧度角。
 * @param digits 结果有效位数。
 * @return 舍入后的余弦值。
 */
BigFloat cos(const BigFloat &value, int digits)
{
    return sinCosWorking(value, trigPrecision(value, digits)).second.round(digits);
}
/**
 * @brief 使用同一次角度约化计算正切的分子分母。
 * @param value 弧度角。
 * @param digits 结果有效位数。
 * @return 舍入后的正切；检测到零余弦时抛出定义域异常。
 */
BigFloat tan(const BigFloat &value, int digits)
{
    const int work = trigPrecision(value, digits);
    const auto pair = sinCosWorking(value, work);
    if (pair.second.isZero()) throw std::domain_error("正切在当前输入处未定义");
    return pair.first.divide(pair.second, work).round(digits);
}
/**
 * @brief 计算任意有限实数的反正切主值。
 * @param value 待求反正切的实数。
 * @param digits 结果有效位数。
 * @return 舍入后的弧度角。
 */
BigFloat atan(const BigFloat &value, int digits)
{
    return atanWorking(value, mathDetail::workPrecision(digits)).round(digits);
}
/**
 * @brief 通过平方根与反正切计算反正弦，保留接近端点的消去信息。
 * @param value 位于 [-1,1] 的实数。
 * @param digits 结果有效位数。
 * @return 舍入后的反正弦主值。
 */
BigFloat asin(const BigFloat &value, int digits)
{
    const int work = mathDetail::workPrecision(digits, static_cast<std::int64_t>(value.mantissa().decimalDigits()));
    if (value.abs() > BigFloat(1)) throw std::domain_error("反正弦要求输入位于 [-1,1]");
    if (value.abs() == BigFloat(1))
    {
        auto result = pi(work).divide(BigFloat(2), work).round(digits);
        return value.sign() < 0 ? -result : result;
    }
    const auto denominator = (BigFloat(1) - value * value).sqrt(work);
    return atanWorking(value.divide(denominator, work), work).round(digits);
}
/**
 * @brief 使用半角恒等式计算反余弦，避免一附近的主值消去。
 * @param value 位于 [-1,1] 的实数。
 * @param digits 结果有效位数。
 * @return 舍入后的反余弦主值。
 */
BigFloat acos(const BigFloat &value, int digits)
{
    const int work = mathDetail::workPrecision(digits, static_cast<std::int64_t>(value.mantissa().decimalDigits()));
    if (value.abs() > BigFloat(1)) throw std::domain_error("反余弦要求输入位于 [-1,1]");
    if (value == BigFloat(1)) return BigFloat();
    if (value == BigFloat(-1)) return pi(work).round(digits);
    const auto ratio = (BigFloat(1) - value).divide(BigFloat(1) + value, work).sqrt(work);
    return (BigFloat(2) * atanWorking(ratio, work)).round(digits);
}
}  // namespace bigfloat
#endif
