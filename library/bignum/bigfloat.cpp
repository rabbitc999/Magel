#if !defined(NO_CHALI_BIG_INT) && !defined(NO_CHALI_BIG_FLOAT)
/**
 * @file bigfloat.cpp
 * @brief `BigFloat` 的实现：尾数 `BigInt` × 10^exp10，解析 / 四则 / 比较 / 舍入。
 *
 * 策略与参考实现一致：所有运算都在整数上做，**多算几位再舍入**到目标有效位数，
 * 所以 `0.1 + 0.2` 精确等于 `0.3`（十进制浮点，不是二进制浮点）。
 * 大规模乘法直接吃到 `BigInt` 的 NTT 内核（O(n log n)）。
 */

#include "bigfloat.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <stdexcept>

namespace bigfloat
{
namespace
{
/**
 * @brief 默认有效位数（`operator/` 用；与 128 位浮点的十进制约 34 位同量级）。
 */
constexpr int kDefaultDivisionDigits = 34;

/**
 * @brief 十进制位数（0 算 1 位）。
 */
int decimalDigits(const bigint::BigInt &value)
{
    return static_cast<int>(value.decimalDigits());
}

/**
 * @brief `value / 10^shift`，四舍五入（远离零）。
 * @param shift 移掉的十进制位数（≤ 0 时原样返回）。
 */
bigint::BigInt divideRounded(const bigint::BigInt &value, int shift)
{
    if (shift <= 0) return value;
    const bigint::BigInt divisor = bigint::pow10(static_cast<std::uint64_t>(shift));
    bigint::BigInt quotient;
    bigint::BigInt remainder;
    value.divmod(divisor, &quotient, &remainder);
    if (remainder.abs() * bigint::BigInt(2) >= divisor)
    {
        quotient += bigint::BigInt(value.sign() < 0 ? -1 : 1);
    }
    return quotient;
}

/**
 * @brief 字符是不是数字。
 */
bool isDigit(char ch) { return ch >= '0' && ch <= '9'; }

/**
 * @brief 把一个"已经舍入好"的值写成十进制文本。
 * @param mantissa 尾数（符号在里面）。
 * @param exp10 十进制指数。
 * @param plain `true` = 恒定普通记法；`false` = 按 `%g` 口径在指数过大 / 过小时切科学记数法。
 * @param precision 显示口径的有效位数（`%g` 里那个 P：指数 `X` 满足 `X < -4 || X >= P` 时用科学记数法）。
 */
Memory::String formatDecimal(const bigint::BigInt &mantissa, int exp10, bool plain, int precision)
{
    if (mantissa.isZero()) return "0";
    const bool negative = mantissa.sign() < 0;
    const Memory::String digits = mantissa.abs().toString();
    const int digitCount = static_cast<int>(digits.size());
    const int pointPos = digitCount + exp10; /* 小数点位置：0 = 最左 */
    const int exponent = pointPos - 1;       /* 科学记数法里的指数：值 = d.ddd × 10^exponent */

    Memory::String text;
    if (negative) text.push_back('-');
    if (!plain && (exponent < -4 || exponent >= precision))
    {
        text.push_back(digits[0]);
        if (digitCount > 1)
        {
            text.push_back('.');
            text += digits.substr(1u);
        }
        text.push_back('e');
        if (exponent >= 0) text.push_back('+');
        text += Memory::toString(exponent);
        return text;
    }
    if (pointPos <= 0)
    {
        text += "0.";
        text.append(static_cast<std::size_t>(-pointPos), '0');
        text += digits;
        return text;
    }
    if (pointPos >= digitCount)
    {
        text += digits;
        text.append(static_cast<std::size_t>(pointPos - digitCount), '0');
        return text;
    }
    text += digits.substr(0u, static_cast<std::size_t>(pointPos));
    text.push_back('.');
    text += digits.substr(static_cast<std::size_t>(pointPos));
    return text;
}
}  // namespace

// ---------------------------------------------------------------------------
//  解析 / 规范化
// ---------------------------------------------------------------------------

/**
 * @brief 从十进制文本构造：`123.456` / `-0.5` / `.5` / `1e100` / `2.5E-3` 都行。
 * @param text 文本。
 * @throws std::invalid_argument 文本不是合法十进制小数时。
 */
BigFloat::BigFloat(const Memory::String &text)
{
    *this = fromString(text, nullptr);
}

/**
 * @brief 从 64 位有符号整数**隐式**构造。
 * @param value 值。
 */
BigFloat::BigFloat(std::int64_t value)
{
    *this = fromInt(bigint::BigInt(value));
}

/**
 * @brief 从 double **隐式**构造（语义同 `fromDouble`：最短往返写法）。
 * @param value 值。
 */
BigFloat::BigFloat(double value)
{
    *this = fromDouble(value);
}

/**
 * @brief 显式转 64 位有符号整数（向零截断；大数按位截断）。
 * @retval 取原数值向零截断后的 64 位有符号整数。
 */
BigFloat::operator std::int64_t() const
{
    const bigint::BigInt truncated = trunc();
    if (truncated.sign() < 0) return -static_cast<std::int64_t>(truncated.abs());
    return static_cast<std::int64_t>(truncated);
}

/**
 * @brief 显式转双精度浮点（可能丢精度；按 17 位有效数字往返）。
 * @retval 保存转换结果的双精度浮点值。
 */
BigFloat::operator double() const
{
    if (isZero()) return 0.0;
    return std::strtod(toString(17).c_str(), nullptr);
}

/**
 * @brief 从十进制文本构造（不抛异常版）。
 * @param text 文本。
 * @param ok 非空时写入是否解析成功。
 * @retval 计算得到的大数值；复合赋值运算返回当前对象。
 */
BigFloat BigFloat::fromString(const Memory::String &text, bool *ok)
{
    auto report = [&](bool success) {
        if (ok != nullptr) *ok = success;
    };

    std::size_t index = 0u;
    while (index < text.size() && (text[index] == ' ' || text[index] == '\t')) ++index;
    int sign = 1;
    if (index < text.size() && (text[index] == '+' || text[index] == '-'))
    {
        if (text[index] == '-') sign = -1;
        ++index;
    }

    Memory::String integerPart;
    while (index < text.size() && isDigit(text[index])) integerPart.push_back(text[index++]);
    Memory::String fractionPart;
    if (index < text.size() && text[index] == '.')
    {
        ++index;
        while (index < text.size() && isDigit(text[index])) fractionPart.push_back(text[index++]);
    }
    if (integerPart.empty() && fractionPart.empty())
    {
        report(false);
        return BigFloat();
    }
    long long exponent = 0;
    if (index < text.size() && (text[index] == 'e' || text[index] == 'E'))
    {
        ++index;
        int exponentSign = 1;
        if (index < text.size() && (text[index] == '+' || text[index] == '-'))
        {
            if (text[index] == '-') exponentSign = -1;
            ++index;
        }
        if (index >= text.size() || !isDigit(text[index]))
        {
            report(false);
            return BigFloat();
        }
        while (index < text.size() && isDigit(text[index]))
        {
            exponent = exponent * 10 + (text[index++] - '0');
            if (exponent > 1000000) /* 指数大得离谱就当语法错，别把内存吃光 */
            {
                report(false);
                return BigFloat();
            }
        }
        exponent *= exponentSign;
    }
    while (index < text.size() && (text[index] == ' ' || text[index] == '\t')) ++index;
    if (index != text.size())
    {
        report(false);
        return BigFloat();
    }

    BigFloat out;
    out.mant_ = bigint::BigInt(integerPart + fractionPart); /* 允许前导 0 */
    if (!out.mant_.isZero()) out.mant_ = sign < 0 ? -out.mant_ : out.mant_;
    const long long exponentValue =
        exponent - static_cast<long long>(fractionPart.size());
    if (exponentValue < std::numeric_limits<int>::min() || exponentValue > std::numeric_limits<int>::max())
    {
        report(false);
        return BigFloat();
    }
    out.exp10_ = static_cast<int>(exponentValue);
    out.normalize();
    report(true);
    return out;
}

/**
 * @brief 从整数构造（值就是那个整数）。
 * @param value 待转换、保存或参与计算的值。
 * @retval 计算得到的大数值；复合赋值运算返回当前对象。
 */
BigFloat BigFloat::fromInt(const bigint::BigInt &value)
{
    BigFloat out;
    out.mant_ = value;
    out.normalize();
    return out;
}

/**
 * @brief 从 `double` 构造：取**能往返同一个 double 的最短十进制写法**。
 *
 * 参考实现的 `EigenMath_BigFloatFromDouble` 直接 `%.17g`，于是 `0.1` 会变成
 * `0.10000000000000001`；这里改成从 1 位起逐位试 `%.*g`、用 `strtod` 读回来
 * 比对，第一个能往返的位数就是结果 —— 所以 `fromDouble(0.1).toString() == "0.1"`，
 * 而 `fromDouble(0.1) + fromDouble(0.2)` 精确等于 `"0.3"`。
 * 非有限值（NaN / ±inf）按 0 处理（与参考实现一致）。
 * 注意：内部用 `snprintf` / `strtod`，跟 `printf` 一样**认当前 locale 的小数点**
 * （缺省的 `"C"` locale 下就是 `.`）；要跨 locale 稳定就自己先解析好文本。
 * @param value 值。
 * @param digits 有效位数**上限**（缺省 17 = double 的往返精度；只在 1..17 内生效）。
 * @retval 计算得到的大数值；复合赋值运算返回当前对象。
 */
BigFloat BigFloat::fromDouble(double value, int digits)
{
    if (!std::isfinite(value)) return BigFloat(); /* NaN / ±inf 按 0 处理 */
    const int limit = (digits > 0 && digits < 17) ? digits : 17;
    char buffer[64];
    bool ok = false;
    BigFloat out;
    /* 从 1 位有效数字往上试：第一个"读回来还是同一个 double"的写法就是最短写法。
     * `%.17g` 一定能往返，所以循环最坏也就 17 轮。 */
    for (int precision = 1; precision <= limit; ++precision)
    {
        std::snprintf(buffer, sizeof(buffer), "%.*g", precision, value);
        out = fromString(buffer, &ok);
        if (!ok) return BigFloat(); /* 理论上不会发生；兜底成 0 */
        if (std::strtod(buffer, nullptr) == value) break;
    }
    return out;
}

/**
 * @brief 规范化数值内部表示并移除冗余零位。
 */
void BigFloat::normalize()
{
    if (mant_.isZero())
    {
        exp10_ = 0;
        return;
    }
    /* 先按 10^9 一档剥尾随零（比一位一位剥快得多），最后再用 10 收尾 */
    for (;;)
    {
        bigint::BigInt quotient;
        bigint::BigInt remainder;
        mant_.divmod(bigint::pow10(9u), &quotient, &remainder);
        if (!remainder.isZero()) break;
        mant_ = quotient;
        exp10_ += 9;
    }
    for (;;)
    {
        bigint::BigInt quotient;
        bigint::BigInt remainder;
        mant_.divmod(bigint::BigInt(10), &quotient, &remainder);
        if (!remainder.isZero()) break;
        mant_ = quotient;
        exp10_ += 1;
    }
}

// ---------------------------------------------------------------------------
//  舍入 / 输出 / 比较
// ---------------------------------------------------------------------------

/**
 * @brief 舍入到 `digits` 位有效数字（四舍五入、远离零；`digits ≤ 0` 抛异常）。
 * @param digits 有效位数。
 * @retval 计算得到的大数值；复合赋值运算返回当前对象。
 */
BigFloat BigFloat::round(int digits) const
{
    if (digits <= 0) throw std::invalid_argument("bigfloat: 有效位数必须是正数");
    BigFloat out = *this;
    if (out.mant_.isZero()) return out;
    const int have = decimalDigits(out.mant_);
    if (have <= digits) return out;
    const int shift = have - digits;
    out.mant_ = divideRounded(out.mant_, shift);
    out.exp10_ += shift;
    out.normalize();
    return out;
}

/**
 * @brief 十进制文本，保留 `digits` 位有效数字。
 *
 * 指数不大时输出普通记法（`123.456`、`0.00123`），指数太大或太小改用
 * 科学记数法（`1.23e+100`、`4.5e-30`）—— 与 `%.17g` 同一口径。
 * @param digits 有效位数；`≤ 0` 表示**不舍入**（按值本身精确输出）。
 *        缺省 17，与 `%.17g` 同量级（所以 `1e20 + 1` 会显示成 `1e+20`）。
 * @retval 生成或读取的结果文本。
 */
Memory::String BigFloat::toString(int digits) const
{
    /* `digits ≤ 0` = 不舍入，但显示口径仍按缺省的 17 位（跟 `%.17g` 一致） */
    const int precision = (digits > 0) ? digits : 17;
    if (digits <= 0) return formatDecimal(mant_, exp10_, false, precision);
    const BigFloat value = round(digits);
    return formatDecimal(value.mant_, value.exp10_, false, precision);
}

/**
 * @brief 十进制文本，**恒定用普通记法**（不切科学记数法）。
 *
 * 对应参考实现的 `EigenMath_BigFloatToDecimalString`（"供 infix / LaTeX 输出用"）：
 * `BigFloat("1e20").toPlainString()` → `"100000000000000000000"`，
 * `(BigFloat("1e20") + BigFloat("1")).toPlainString()` → `"100000000000000000001"`。
 * @param digits 有效位数；`≤ 0`（缺省）表示不舍入、按值本身精确输出。
 * @retval 生成或读取的结果文本。
 */
Memory::String BigFloat::toPlainString(int digits) const
{
    if (digits <= 0) return formatDecimal(mant_, exp10_, true, 17); /* 不舍入 */
    const BigFloat value = round(digits);
    return formatDecimal(value.mant_, value.exp10_, true, digits);
}

/**
 * @brief 比较当前数值与另一数值的大小。
 * @param other 参与当前运算或比较的另一对象。
 * @retval 当前值小于、等于、大于另一值时分别为负数、0、正数。
 */
int BigFloat::compare(const BigFloat &other) const
{
    if (mant_.sign() != other.mant_.sign())
    {
        return (mant_.sign() < other.mant_.sign()) ? -1 : 1;
    }
    if (mant_.isZero()) return 0;
    /* 同号：比 尾数 × 10^exp10 的绝对值 —— 对齐到更小的指数，再比整数 */
    const int exponent = std::min(exp10_, other.exp10_);
    const bigint::BigInt left =
        mant_.abs() * bigint::pow10(static_cast<std::uint64_t>(exp10_ - exponent));
    const bigint::BigInt right =
        other.mant_.abs() * bigint::pow10(static_cast<std::uint64_t>(other.exp10_ - exponent));
    const int magnitude = left.compare(right);
    return (mant_.sign() >= 0) ? magnitude : -magnitude;
}

// ---------------------------------------------------------------------------
//  四则
// ---------------------------------------------------------------------------

/**
 * @brief 计算两个数值的和。
 * @param left 左操作数。
 * @param right 右操作数。
 * @retval 计算得到的大数值；复合赋值运算返回当前对象。
 */
BigFloat operator+(const BigFloat &left, const BigFloat &right)
{
    if (left.isZero()) return right;
    if (right.isZero()) return left;
    const int exponent = std::min(left.exp10_, right.exp10_);
    BigFloat out;
    out.mant_ = left.mant_ * bigint::pow10(static_cast<std::uint64_t>(left.exp10_ - exponent)) + right.mant_ * bigint::pow10(static_cast<std::uint64_t>(right.exp10_ - exponent));
    out.exp10_ = exponent;
    out.normalize();
    return out;
}

/**
 * @brief 计算数值之差或当前数值的相反数。
 * @param left 左操作数。
 * @param right 右操作数。
 * @retval 计算得到的大数值；复合赋值运算返回当前对象。
 */
BigFloat operator-(const BigFloat &left, const BigFloat &right) { return left + (-right); }

/**
 * @brief 一元负号。
 * @retval 计算得到的大数值；复合赋值运算返回当前对象。
 */
BigFloat BigFloat::operator-() const
{
    BigFloat out = *this;
    out.mant_ = -out.mant_;
    return out;
}

/**
 * @brief 计算两个数值的乘积。
 * @param left 左操作数。
 * @param right 右操作数。
 * @retval 两个数值相乘得到的积。
 */
BigFloat operator*(const BigFloat &left, const BigFloat &right)
{
    BigFloat out;
    out.mant_ = left.mant_ * right.mant_;
    out.exp10_ = left.exp10_ + right.exp10_;
    out.normalize();
    return out;
}

/**
 * @brief 除法并指定有效位数（`operator/` 用 34 位）。
 * @param divisor 除数。
 * @param digits 商保留的有效位数。
 * @retval 计算得到的大数值；复合赋值运算返回当前对象。
 */
BigFloat BigFloat::divide(const BigFloat &divisor, int digits) const
{
    if (divisor.isZero()) throw std::domain_error("bigfloat: 除以零");
    if (digits <= 0) throw std::invalid_argument("bigfloat: 有效位数必须是正数");
    if (isZero()) return BigFloat();
    /* 把被除数放大，让商有 digits + 2 位有效数字，再舍入回 digits 位 */
    const int numeratorDigits = decimalDigits(mant_);
    const int denominatorDigits = decimalDigits(divisor.mant_);
    const int shift = digits + 2 + denominatorDigits - numeratorDigits;
    bigint::BigInt numerator = mant_;
    int exponent = exp10_ - divisor.exp10_;
    if (shift > 0)
    {
        numerator *= bigint::pow10(static_cast<std::uint64_t>(shift));
        exponent -= shift;
    }
    bigint::BigInt quotient;
    bigint::BigInt remainder;
    numerator.divmod(divisor.mant_, &quotient, &remainder);
    BigFloat out;
    out.mant_ = quotient;
    out.exp10_ = exponent;
    out.normalize();
    return out.round(digits);
}

/**
 * @brief 计算两个数值相除的商。
 * @param left 左操作数。
 * @param right 右操作数。
 * @retval 计算得到的大数值；复合赋值运算返回当前对象。
 */
BigFloat operator/(const BigFloat &left, const BigFloat &right)
{
    return left.divide(right, kDefaultDivisionDigits);
}

/**
 * @brief 将右操作数累加到当前值。
 * @param other 参与当前运算或比较的另一对象。
 * @retval 更新后的当前数值对象引用。
 */
BigFloat &BigFloat::operator+=(const BigFloat &other)
{
    *this = *this + other;
    return *this;
}
/**
 * @brief 从当前值减去右操作数。
 * @param other 参与当前运算或比较的另一对象。
 * @retval 更新后的当前数值对象引用。
 */
BigFloat &BigFloat::operator-=(const BigFloat &other)
{
    *this = *this - other;
    return *this;
}
/**
 * @brief 将当前值乘以右操作数。
 * @param other 参与当前运算或比较的另一对象。
 * @retval 更新后的当前数值对象引用。
 */
BigFloat &BigFloat::operator*=(const BigFloat &other)
{
    *this = *this * other;
    return *this;
}
/**
 * @brief 将当前值除以右操作数。
 * @param other 参与当前运算或比较的另一对象。
 * @retval 更新后的当前数值对象引用。
 */
BigFloat &BigFloat::operator/=(const BigFloat &other)
{
    *this = *this / other;
    return *this;
}

/**
 * @brief 判断两个值是否相等。
 * @param left 左操作数。
 * @param right 右操作数。
 * @retval 两个值相等时为 true，否则为 false。
 */
bool operator==(const BigFloat &left, const BigFloat &right)
{
    return left.compare(right) == 0;
}
/**
 * @brief 判断两个值是否不相等。
 * @param left 左操作数。
 * @param right 右操作数。
 * @retval 两个值不相等时为 true，否则为 false。
 */
bool operator!=(const BigFloat &left, const BigFloat &right)
{
    return left.compare(right) != 0;
}
/**
 * @brief 判断左值是否小于右值。
 * @param left 左操作数。
 * @param right 右操作数。
 * @retval 左值小于右值时为 true，否则为 false。
 */
bool operator<(const BigFloat &left, const BigFloat &right)
{
    return left.compare(right) < 0;
}
/**
 * @brief 判断左值是否小于或等于右值。
 * @param left 左操作数。
 * @param right 右操作数。
 * @retval 左值小于或等于右值时为 true，否则为 false。
 */
bool operator<=(const BigFloat &left, const BigFloat &right)
{
    return left.compare(right) <= 0;
}
/**
 * @brief 判断左值是否大于右值。
 * @param left 左操作数。
 * @param right 右操作数。
 * @retval 左值大于右值时为 true，否则为 false。
 */
bool operator>(const BigFloat &left, const BigFloat &right)
{
    return left.compare(right) > 0;
}
/**
 * @brief 判断左值是否大于或等于右值。
 * @param left 左操作数。
 * @param right 右操作数。
 * @retval 左值大于或等于右值时为 true，否则为 false。
 */
bool operator>=(const BigFloat &left, const BigFloat &right)
{
    return left.compare(right) >= 0;
}

}  // namespace bigfloat

#endif
