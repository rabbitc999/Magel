/**
 * @file bigfloat_roots.cpp
 * @brief 十进制浮点的幂、方根与整数取整。
 */
#if !defined(NO_CHALI_BIG_FLOAT) && !defined(NO_CHALI_BIG_INT)
#include "bigfloat.hpp"
#include <algorithm>
#include <limits>
#include <stdexcept>
namespace bigfloat
{
// ---------------------------------------------------------------------------
//  平方根
// ---------------------------------------------------------------------------

/**
 * @brief 平方根，结果保留 `digits` 位有效数字。
 * @param digits 有效位数。
 * @throws std::domain_error 负数开方时。
 * @retval 计算得到的大数值；复合赋值运算返回当前对象。
 */
BigFloat BigFloat::sqrt(int digits) const
{
    if (digits <= 0) throw std::invalid_argument("bigfloat: 有效位数必须是正数");
    if (mant_.sign() < 0) throw std::domain_error("bigfloat: 负数不能开平方");
    if (isZero()) return BigFloat();
    /* 值 = m × 10^e：先把 e 凑成偶数（不够就 m × 10、e − 1），
     * 再把 m 放大到 2×digits 位，做整数平方根，最后舍入。 */
    bigint::BigInt m = mant_.abs();
    long long exponent = exp10_;
    if ((exponent % 2) != 0)
    {
        m *= bigint::BigInt(10);
        exponent -= 1;
    }
    /* 多算 2 位再舍回（尾数误差 < 1 个最低位，2 位余量足够躲开四舍五入的边界） */
    const long long want = 2ll * digits + 2ll;
    const long long have = static_cast<long long>(m.decimalDigits());
    if (have < want)
    {
        long long extra = want - have;
        /* 放大的位数要跟指数**同奇偶**，这样 `exponent / 2` 才是精确的一半
         * —— 否则 sqrt(4) 会算出 6.324…×10⁻⁹ 这种结果。 */
        if (((extra + exponent) % 2) != 0) extra += 1;
        m *= bigint::pow10(static_cast<std::uint64_t>(extra));
        exponent -= extra;
    }
    BigFloat out;
    out.mant_ = bigint::integerSqrt(m);
    out.exp10_ = static_cast<int>(exponent / 2);
    out.normalize();
    return out.round(digits);
}

// ---------------------------------------------------------------------------
//  整数次幂 / 绝对值 / 取整 / k 次方根
// ---------------------------------------------------------------------------

/**
 * @brief 精确整数次幂：`*this ^ exponent`。
 * @param exponent 指数（无符号；`0` 次幂为 1）。
 * @retval 计算得到的大数值；复合赋值运算返回当前对象。
 */
BigFloat BigFloat::pow(std::uint64_t exponent) const
{
    if (exponent == 0u) return BigFloat::fromInt(bigint::BigInt(1));
    if (exp10_ != 0)
    {
        const std::uint64_t magnitude = exp10_ < 0 ? -static_cast<std::int64_t>(exp10_) : exp10_;
        const std::uint64_t limit = exp10_ < 0 ? -static_cast<std::int64_t>(std::numeric_limits<int>::min() / 2) : std::numeric_limits<int>::max() / 2;
        if (exponent > limit / magnitude) throw std::overflow_error("bigfloat: 幂运算的十进制指数溢出");
    }
    const int product = exp10_ == 0 ? 0 : exp10_ * static_cast<int>(exponent);
    BigFloat out;
    out.mant_ = mant_.pow(exponent);
    out.exp10_ = static_cast<int>(product);
    out.normalize();
    return out;
}

/**
 * @brief 绝对值。
 * @retval 计算得到的大数值；复合赋值运算返回当前对象。
 */
BigFloat BigFloat::abs() const
{
    BigFloat out = *this;
    out.mant_ = out.mant_.abs();
    return out;
}

/**
 * @brief 是不是整数（`5.0`、`1e20` 是；`0.5`、`1e-3` 不是；0 是）。
 * @retval 满足检查条件或操作完成时为 true，检查失败或无法完成时为 false。
 */
bool BigFloat::isInteger() const
{
    if (mant_.isZero() || exp10_ >= 0) return true;
    /* 指数为负：值是整数当且仅当尾数能被 10^(-exp) 整除 */
    const bigint::BigInt divisor = bigint::pow10(static_cast<std::uint64_t>(-exp10_));
    bigint::BigInt remainder;
    mant_.divmod(divisor, nullptr, &remainder);
    return remainder.isZero();
}

/**
 * @brief 向零截断。
 * @retval 计算得到的大数值；复合赋值运算返回当前对象。
 */
bigint::BigInt BigFloat::trunc() const
{
    if (mant_.isZero()) return bigint::BigInt();
    if (exp10_ >= 0) return mant_ * bigint::pow10(static_cast<std::uint64_t>(exp10_));
    bigint::BigInt quotient;
    mant_.divmod(bigint::pow10(static_cast<std::uint64_t>(-exp10_)), &quotient, nullptr);
    return quotient; /* divmod 的商已带被除数符号（向零截断） */
}

/**
 * @brief 向负无穷取整。
 * @retval 计算得到的大数值；复合赋值运算返回当前对象。
 */
bigint::BigInt BigFloat::floor() const
{
    if (mant_.isZero() || exp10_ >= 0) return trunc();
    bigint::BigInt quotient;
    bigint::BigInt remainder;
    mant_.divmod(bigint::pow10(static_cast<std::uint64_t>(-exp10_)), &quotient, &remainder);
    if (mant_.sign() < 0 && !remainder.isZero()) --quotient;
    return quotient;
}

/**
 * @brief 向正无穷取整。
 * @retval 计算得到的大数值；复合赋值运算返回当前对象。
 */
bigint::BigInt BigFloat::ceil() const
{
    if (mant_.isZero() || exp10_ >= 0) return trunc();
    bigint::BigInt quotient;
    bigint::BigInt remainder;
    mant_.divmod(bigint::pow10(static_cast<std::uint64_t>(-exp10_)), &quotient, &remainder);
    if (mant_.sign() > 0 && !remainder.isZero()) ++quotient;
    return quotient;
}

/**
 * @brief `k` 次方根，结果保留 `digits` 位有效数字（牛顿迭代）。
 * @param k 根次（≥ 1）。
 * @param digits 有效位数。
 * @retval 计算得到的大数值；复合赋值运算返回当前对象。
 */
BigFloat BigFloat::root(std::uint64_t k, int digits) const
{
    if (k == 0u) throw std::invalid_argument("bigfloat: 根次必须为正");
    if (digits > (std::numeric_limits<int>::max() - 6) / 2)
        throw std::overflow_error("bigfloat: 方根精度超出内部计算范围");
    if (digits <= 0) throw std::invalid_argument("bigfloat: 有效位数必须是正数");
    if (mant_.sign() < 0) throw std::domain_error("bigfloat: 负数不能开 k 次方");
    if (isZero()) return BigFloat();
    if (k == 1u) return round(digits);
    if (k == 2u) return sqrt(digits);

    /* 初值：尾数取整数 k 次方根、指数按 e / k 缩放 —— 量级差不超过一个 10 的
     * 因子，牛顿迭代足够把它拉回来。 */
    BigFloat guess;
    guess.mant_ = bigint::integerRoot(mant_.abs(), k);
    if (guess.mant_.isZero()) guess.mant_ = bigint::BigInt(1);
    guess.exp10_ = k > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()) ? 0 : static_cast<int>(static_cast<std::int64_t>(exp10_) / static_cast<std::int64_t>(k));
    guess.normalize();

    const BigFloat target = abs();
    const BigFloat kBig = BigFloat::fromInt(bigint::BigInt::fromU64(k));
    const BigFloat km1 = BigFloat::fromInt(bigint::BigInt::fromU64(k - 1u));
    const int targetDigits = 2 * digits + 4;
    /* 牛顿迭代 x ← ((k−1)·x + v / x^(k−1)) / k。先用低精度迭代到不动点
     * （初值与真根可能差一个数量级，"每轮翻倍"要等误差进入二次收敛区才成立），
     * 之后再按精度逐级翻倍，每级一次迭代即可。 */
    BigFloat x = guess;
    BigFloat previous;
    for (int warmup = 0; warmup < 8; ++warmup)
    {
        previous = x;
        const BigFloat quotient = target.divide(x.pow(k - 1u), 10);
        x = (km1 * x + quotient).divide(kBig, 10);
        if (x == previous) break;
    }
    int precision = 8; /* 预热后至少有 8 位正确数字 */
    while (precision < targetDigits)
    {
        precision += std::min(precision, targetDigits - precision);
        const BigFloat quotient = target.divide(x.pow(k - 1u), precision + 2);
        x = (km1 * x + quotient).divide(kBig, precision + 2);
    }
    return x.round(digits);
}

}  // namespace bigfloat
#endif
