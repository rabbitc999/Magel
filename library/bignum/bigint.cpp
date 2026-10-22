#if !defined(NO_CHALI_BIG_INT)
/**
 * @file bigint.cpp
 * @brief 大整数构造、规范化、基础幅值运算、有符号算术与整数方根。
 *
 * 参考 `eigenmath-plus/eigenmath_bigint.c` 的整数部分：同样是"符号 + 小端 limb +
 * 有效长度 + 每次运算后规范化"，只是把手工 `malloc` 换成 `std::vector`。
 * 乘法按补齐规模在 Karatsuba 与 NTT 之间切换，递归工作区一次分配。
 */

#include "bigint.hpp"

#include <algorithm>
#include <limits>
#include <stdexcept>

namespace bigint
{
namespace
{
constexpr std::uint64_t kLimbBase = 1ull << 32;
/**
 * @brief 检查十进制输入字符是否为 ASCII 数字。
 * @param ch 待检查的输入字符。
 * @return 字符位于 0 到 9 时返回 true。
 */
bool isDigit(char ch) { return ch >= '0' && ch <= '9'; }
}  // namespace

// ---------------------------------------------------------------------------
//  构造 / 规范化
// ---------------------------------------------------------------------------

/**
 * @brief 从 64 位有符号整数构造（`INT64_MIN` 也正确）。
 * @param value 值。
 */
BigInt::BigInt(std::int64_t value)
{
    if (value == 0) return;
    sign_ = (value < 0) ? -1 : 1;
    // INT64_MIN 取负会溢出，所以先按无符号取幅值
    std::uint64_t magnitude = (value < 0) ? (~static_cast<std::uint64_t>(value) + 1ull) : static_cast<std::uint64_t>(value);
    while (magnitude != 0)
    {
        limbs_.push_back(static_cast<std::uint32_t>(magnitude & 0xFFFFFFFFull));
        magnitude >>= 32u;
    }
}

/**
 * @brief 从十进制字符串构造（可带 `+` / `-` 号，允许前导 0）。
 * @param decimal 十进制文本。
 * @throws std::invalid_argument 文本不是合法十进制整数时。
 */
BigInt::BigInt(const Memory::String &decimal)
{
    *this = parseDecimal(decimal, nullptr);
}

/**
 * @brief 从 64 位**无符号**整数构造（`uint64_t` 上界也不会被当成 -1）。
 *
 * 与参考实现的 `EigenMath_BigIntFromU64` 对应：`int64_t` 构造只吃有符号范围，
 * 大于 `INT64_MAX` 的无符号值请显式用这个。
 * @param value 值。
 * @retval 计算得到的大数值；复合赋值运算返回当前对象。
 */
BigInt BigInt::fromU64(std::uint64_t value)
{
    BigInt out;
    while (value != 0u)
    {
        out.limbs_.push_back(static_cast<std::uint32_t>(value & 0xFFFFFFFFull));
        value >>= 32u;
    }
    if (!out.limbs_.empty()) out.sign_ = 1;
    return out;
}

/**
 * @brief 从十进制字符串构造（不抛异常版）。
 * @param decimal 十进制文本。
 * @param ok 非空时写入是否解析成功。
 * @return 解析成功时的值；失败返回 0。
 */
BigInt BigInt::fromString(const Memory::String &decimal, bool *ok)
{
    bool parsed = false;
    BigInt value = parseDecimal(decimal, &parsed);
    if (ok != nullptr) *ok = parsed;
    return parsed ? value : BigInt();
}

/**
 * @brief 把十进制文本按 10^9 一段一段地吃进来（`ok` 见 fromString）。
 * @param decimal 十进制数值文本。
 * @param ok 非空时接收操作是否成功的标志。
 * @retval 计算得到的大数值；复合赋值运算返回当前对象。
 */
BigInt BigInt::parseDecimal(const Memory::String &decimal, bool *ok)
{
    auto report = [&](bool success) {
        if (ok != nullptr) *ok = success;
    };

    std::size_t index = 0u;
    while (index < decimal.size() && (decimal[index] == ' ' || decimal[index] == '\t')) ++index;
    int sign = 1;
    if ((index < decimal.size()) && (decimal[index] == '+' || decimal[index] == '-'))
    {
        if (decimal[index] == '-') sign = -1;
        ++index;
    }
    if (index >= decimal.size()) /* 只有符号 / 空串 */
    {
        report(false);
        return BigInt();
    }
    for (std::size_t cursor = index; cursor < decimal.size(); ++cursor)
    {
        if (!isDigit(decimal[cursor]))
        {
            report(false);
            return BigInt();
        }
    }

    /* 从高位往低位：每 9 位凑一段，`value = value × 10^9 + 段`。 */
    BigInt value;
    std::size_t cursor = index;
    while (cursor < decimal.size())
    {
        const std::size_t take = std::min<std::size_t>(9u, decimal.size() - cursor);
        std::uint32_t chunk = 0u;
        std::uint32_t scale = 1u;
        for (std::size_t offset = 0u; offset < take; ++offset)
        {
            chunk = chunk * 10u + static_cast<std::uint32_t>(decimal[cursor + offset] - '0');
            scale *= 10u;
        }
        value.mulSmallAdd(scale, chunk);
        cursor += take;
    }
    if (!value.isZero()) value.sign_ = sign;
    report(true);
    return value;
}

/**
 * @brief 规范化数值内部表示并移除冗余零位。
 */
void BigInt::normalize()
{
    while (!limbs_.empty() && limbs_.back() == 0u) limbs_.pop_back();
    if (limbs_.empty()) sign_ = 0;
}

/**
 * @brief 绝对值。
 * @retval 计算得到的大数值；复合赋值运算返回当前对象。
 */
BigInt BigInt::abs() const
{
    BigInt value = *this;
    if (!value.isZero()) value.sign_ = 1;
    return value;
}

// ---------------------------------------------------------------------------
//  位操作 / 小整数除法（除法与十进制输出用）
// ---------------------------------------------------------------------------

/**
 * @brief 二进制位长度（0 返回 0）。
 * @retval 数值的有效二进制位数，零值为 0。
 */
std::size_t BigInt::bitLength() const
{
    if (limbs_.empty()) return 0u;
    std::size_t bits = (limbs_.size() - 1u) * 32u;
    std::uint32_t top = limbs_.back();
    while (top != 0u)
    {
        ++bits;
        top >>= 1u;
    }
    return bits;
}

/**
 * @brief 取第 bit 位是不是 1。
 * @param bit 待写入或查询的二进制位。
 * @retval 满足检查条件或操作完成时为 true，检查失败或无法完成时为 false。
 */
bool BigInt::bitAt(std::size_t bit) const
{
    const std::size_t index = bit / 32u;
    if (index >= limbs_.size()) return false;
    return ((limbs_[index] >> (bit % 32u)) & 1u) != 0u;
}

/**
 * @brief 把第 bit 位置 1。
 * @param bit 待写入或查询的二进制位。
 */
void BigInt::setBit(std::size_t bit)
{
    const std::size_t index = bit / 32u;
    if (index >= limbs_.size()) limbs_.resize(index + 1u, 0u);
    limbs_[index] |= (1u << (bit % 32u));
    if (sign_ == 0) sign_ = 1; /* 对 0 置位后应变成 +1，而不是留在"零表示" */
}

/**
 * @brief 左移 bits 位（就地）。
 * @param bits 数值类型的位宽。
 */
void BigInt::shiftLeft(std::size_t bits)
{
    if (limbs_.empty() || bits == 0u) return;
    const std::size_t whole = bits / 32u;
    const std::size_t part = bits % 32u;
    Memory::Vector<std::uint32_t> out(limbs_.size() + whole + 1u, 0u);
    for (std::size_t index = 0u; index < limbs_.size(); ++index)
    {
        const std::uint64_t value = static_cast<std::uint64_t>(limbs_[index]) << part;
        out[index + whole] |= static_cast<std::uint32_t>(value & 0xFFFFFFFFull);
        out[index + whole + 1u] |= static_cast<std::uint32_t>(value >> 32u);
    }
    limbs_.swap(out);
    normalize();
}

/**
 * @brief 右移 bits 位（就地）。
 * @param bits 数值类型的位宽。
 */
void BigInt::shiftRight(std::size_t bits)
{
    if (limbs_.empty() || bits == 0u) return;
    const std::size_t whole = bits / 32u;
    const std::size_t part = bits % 32u;
    if (whole >= limbs_.size())
    {
        limbs_.clear();
        sign_ = 0;
        return;
    }
    Memory::Vector<std::uint32_t> out(limbs_.size() - whole, 0u);
    for (std::size_t index = 0u; index + whole < limbs_.size(); ++index)
    {
        std::uint64_t value = limbs_[index + whole] >> part;
        if (part != 0u && index + whole + 1u < limbs_.size())
        {
            value |= static_cast<std::uint64_t>(limbs_[index + whole + 1u]) << (32u - part);
        }
        out[index] = static_cast<std::uint32_t>(value & 0xFFFFFFFFull);
    }
    limbs_.swap(out);
    normalize();
}

/**
 * @brief 幅值 = 幅值 × small + add（small < 2^32）。
 * @param small 小整数乘数或除数。
 * @param add 乘法后需要累加的小整数。
 */
void BigInt::mulSmallAdd(std::uint32_t small, std::uint32_t add)
{
    std::uint64_t carry = add;
    for (std::size_t index = 0u; index < limbs_.size(); ++index)
    {
        const std::uint64_t value =
            static_cast<std::uint64_t>(limbs_[index]) * small + carry;
        limbs_[index] = static_cast<std::uint32_t>(value & 0xFFFFFFFFull);
        carry = value >> 32u;
    }
    while (carry != 0u)
    {
        limbs_.push_back(static_cast<std::uint32_t>(carry & 0xFFFFFFFFull));
        carry >>= 32u;
    }
    normalize();
    if (!limbs_.empty() && sign_ == 0) sign_ = 1;
}

/**
 * @brief 幅值 / small 并返回余数（small ≠ 0）。
 * @param small 小整数乘数或除数。
 * @retval 除法运算得到的余数。
 */
std::uint32_t BigInt::divSmall(std::uint32_t small)
{
    std::uint64_t remainder = 0u;
    for (std::size_t index = limbs_.size(); index-- > 0u;)
    {
        const std::uint64_t value = (remainder << 32u) | limbs_[index];
        limbs_[index] = static_cast<std::uint32_t>(value / small);
        remainder = value % small;
    }
    normalize();
    return static_cast<std::uint32_t>(remainder);
}

// ---------------------------------------------------------------------------
//  幅值运算
// ---------------------------------------------------------------------------

/**
 * @brief 幅值比较：|a| 与 |b|。
 * @param left 左操作数。
 * @param right 右操作数。
 * @retval 左侧幅值小于、等于、大于右侧幅值时分别为负数、0、正数。
 */
int BigInt::compareMagnitude(const BigInt &left, const BigInt &right)
{
    if (left.limbs_.size() != right.limbs_.size())
    {
        return (left.limbs_.size() < right.limbs_.size()) ? -1 : 1;
    }
    for (std::size_t index = left.limbs_.size(); index-- > 0u;)
    {
        if (left.limbs_[index] != right.limbs_[index])
        {
            return (left.limbs_[index] < right.limbs_[index]) ? -1 : 1;
        }
    }
    return 0;
}

/**
 * @brief 幅值相加（都按非负处理）。
 * @param left 左操作数。
 * @param right 右操作数。
 * @retval 计算得到的大数值；复合赋值运算返回当前对象。
 */
BigInt BigInt::addMagnitude(const BigInt &left, const BigInt &right)
{
    BigInt out;
    const std::size_t count = std::max(left.limbs_.size(), right.limbs_.size());
    out.limbs_.assign(count, 0u);
    std::uint64_t carry = 0u;
    for (std::size_t index = 0u; index < count; ++index)
    {
        const std::uint64_t a = (index < left.limbs_.size()) ? left.limbs_[index] : 0u;
        const std::uint64_t b = (index < right.limbs_.size()) ? right.limbs_[index] : 0u;
        const std::uint64_t value = a + b + carry;
        out.limbs_[index] = static_cast<std::uint32_t>(value & 0xFFFFFFFFull);
        carry = value >> 32u;
    }
    if (carry != 0u) out.limbs_.push_back(static_cast<std::uint32_t>(carry));
    out.normalize();
    if (!out.limbs_.empty()) out.sign_ = 1;
    return out;
}

/**
 * @brief 幅值相减：要求 |left| ≥ |right|。
 * @param left 左操作数。
 * @param right 右操作数。
 * @retval 计算得到的大数值；复合赋值运算返回当前对象。
 */
BigInt BigInt::subMagnitude(const BigInt &left, const BigInt &right)
{
    BigInt out;
    out.limbs_.assign(left.limbs_.size(), 0u);
    std::int64_t borrow = 0; /* 0 或 -1 */
    for (std::size_t index = 0u; index < left.limbs_.size(); ++index)
    {
        const std::int64_t a = static_cast<std::int64_t>(left.limbs_[index]);
        const std::int64_t b = (index < right.limbs_.size()) ? static_cast<std::int64_t>(right.limbs_[index]) : 0;
        std::int64_t value = a - b + borrow;
        if (value < 0)
        {
            value += static_cast<std::int64_t>(kLimbBase);
            borrow = -1;
        }
        else
        {
            borrow = 0;
        }
        out.limbs_[index] = static_cast<std::uint32_t>(value);
    }
    out.normalize();
    if (!out.limbs_.empty()) out.sign_ = 1;
    return out;
}

/**
 * @brief 整数平方根（向下取整）：二分 / 牛顿迭代，只用大整数的加减乘除。
 *
 * 大浮点的 `sqrt` 靠它（先放大到 2×digits 位有效数字再开方）。
 * @param value 非负整数；为负时抛 `std::domain_error`。
 * @retval 计算得到的大数值；复合赋值运算返回当前对象。
 */
BigInt integerSqrt(const BigInt &value)
{
    if (value.sign() < 0) throw std::domain_error("bigint: 负数没有整数平方根");
    if (value.isZero()) return BigInt();
    /* 二分：先找一个 high 使 high² > value，再不断折半逼近 */
    BigInt low(0);
    BigInt high(1);
    while (high * high <= value) high *= BigInt(2);
    while (high - low > BigInt(1))
    {
        const BigInt middle = (low + high) / BigInt(2);
        if (middle * middle <= value)
            low = middle;
        else
            high = middle;
    }
    return low;
}

/**
 * @brief 10 的 `exponent` 次幂（大浮点对齐指数时大量使用；幂用 NTT 乘法）。
 * @param exponent 幂运算的指数。
 * @retval 计算得到的大数值；复合赋值运算返回当前对象。
 */
BigInt pow10(std::uint64_t exponent) { return BigInt(10).pow(exponent); }

/**
 * @brief 整数 `k` 次方根（向下取整）：二分，只用大整数的加减乘除与幂。
 * @param value 非负整数。
 * @param k 根次（≥ 1）。
 * @retval 计算得到的大数值；复合赋值运算返回当前对象。
 */
BigInt integerRoot(const BigInt &value, std::uint64_t k)
{
    if (k == 0u) throw std::invalid_argument("bigint: 根次必须为正");
    if (value.sign() < 0) throw std::domain_error("bigint: 负数没有整数 k 次方根");
    if (value.isZero() || k == 1u) return value;
    /* 二分：high = 2^(bitLength / k + 1)，保证 high^k > value */
    BigInt low(0);
    BigInt high(1);
    high.shiftLeft(value.bitLength() / k + 1u);
    while (high - low > BigInt(1))
    {
        const BigInt middle = (low + high) / BigInt(2);
        if (middle.pow(k) <= value)
            low = middle;
        else
            high = middle;
    }
    return low;
}

// ---------------------------------------------------------------------------
//  比较 / 算术
// ---------------------------------------------------------------------------

/**
 * @brief 比较当前数值与另一数值的大小。
 * @param other 参与当前运算或比较的另一对象。
 * @retval 当前值小于、等于、大于另一值时分别为负数、0、正数。
 */
int BigInt::compare(const BigInt &other) const
{
    if (sign_ != other.sign_) return (sign_ < other.sign_) ? -1 : 1;
    const int magnitude = compareMagnitude(*this, other);
    return (sign_ >= 0) ? magnitude : -magnitude;
}

/**
 * @brief 计算两个数值的和。
 * @param left 左操作数。
 * @param right 右操作数。
 * @retval 计算得到的大数值；复合赋值运算返回当前对象。
 */
BigInt operator+(const BigInt &left, const BigInt &right)
{
    if (left.isZero()) return right;
    if (right.isZero()) return left;
    if (left.sign_ == right.sign_)
    {
        BigInt out = BigInt::addMagnitude(left, right);
        out.sign_ = left.sign_;
        return out;
    }
    const int magnitude = BigInt::compareMagnitude(left, right);
    if (magnitude == 0) return BigInt();
    if (magnitude > 0)
    {
        BigInt out = BigInt::subMagnitude(left, right);
        out.sign_ = left.sign_;
        return out;
    }
    BigInt out = BigInt::subMagnitude(right, left);
    out.sign_ = right.sign_;
    return out;
}

/**
 * @brief 计算数值之差或当前数值的相反数。
 * @param left 左操作数。
 * @param right 右操作数。
 * @retval 计算得到的大数值；复合赋值运算返回当前对象。
 */
BigInt operator-(const BigInt &left, const BigInt &right) { return left + (-right); }

/**
 * @brief 一元负号。
 * @retval 计算得到的大数值；复合赋值运算返回当前对象。
 */
BigInt BigInt::operator-() const
{
    BigInt out = *this;
    out.sign_ = -out.sign_;
    return out;
}

/**
 * @brief 计算两个数值的乘积。
 * @param left 左操作数。
 * @param right 右操作数。
 * @retval 两个数值相乘得到的积。
 */
BigInt operator*(const BigInt &left, const BigInt &right)
{
    if (left.isZero() || right.isZero()) return BigInt();
    BigInt out = BigInt::mulMagnitude(left, right);
    out.sign_ = left.sign_ * right.sign_;
    return out;
}

/**
 * @brief 除取整：`this / divisor` 与余数（向零截断，`this = q × divisor + r`）。
 * @param divisor 除数。
 * @param quotient 非空时写入商。
 * @param remainder 非空时写入余数。
 * @return 除数为零返回 false（此时不写输出）。
 */
bool BigInt::divmod(const BigInt &divisor, BigInt *quotient, BigInt *remainder) const
{
    if (divisor.isZero()) return false;
    if (isZero())
    {
        if (quotient != nullptr) *quotient = BigInt();
        if (remainder != nullptr) *remainder = BigInt();
        return true;
    }
    BigInt q;
    BigInt r;
    BigInt::divmodMagnitude(*this, divisor, &q, &r);
    /* 向零截断：商的符号是两边符号的积，余数符号跟被除数 */
    if (!q.isZero())
    {
        q.sign_ = sign_ * divisor.sign_;
        /* |a| ≥ |b| 时商的幅值正确；否则上面已经是 0 */
    }
    if (!r.isZero()) r.sign_ = sign_;
    if (quotient != nullptr) *quotient = q;
    if (remainder != nullptr) *remainder = r;
    return true;
}

/**
 * @brief 计算两个数值相除的商。
 * @param left 左操作数。
 * @param right 右操作数。
 * @retval 计算得到的大数值；复合赋值运算返回当前对象。
 */
BigInt operator/(const BigInt &left, const BigInt &right)
{
    BigInt quotient;
    if (!left.divmod(right, &quotient, nullptr))
    {
        throw std::domain_error("bigint: 除以零");
    }
    return quotient;
}

/**
 * @brief 计算两个整数相除的余数。
 * @param left 左操作数。
 * @param right 右操作数。
 * @retval 计算得到的大数值；复合赋值运算返回当前对象。
 */
BigInt operator%(const BigInt &left, const BigInt &right)
{
    BigInt remainder;
    if (!left.divmod(right, nullptr, &remainder))
    {
        throw std::domain_error("bigint: 除以零");
    }
    return remainder;
}

/**
 * @brief 将右操作数累加到当前值。
 * @param other 参与当前运算或比较的另一对象。
 * @retval 更新后的当前数值对象引用。
 */
BigInt &BigInt::operator+=(const BigInt &other)
{
    *this = *this + other;
    return *this;
}
/**
 * @brief 从当前值减去右操作数。
 * @param other 参与当前运算或比较的另一对象。
 * @retval 更新后的当前数值对象引用。
 */
BigInt &BigInt::operator-=(const BigInt &other)
{
    *this = *this - other;
    return *this;
}
/**
 * @brief 将当前值乘以右操作数。
 * @param other 参与当前运算或比较的另一对象。
 * @retval 更新后的当前数值对象引用。
 */
BigInt &BigInt::operator*=(const BigInt &other)
{
    *this = *this * other;
    return *this;
}
/**
 * @brief 将当前值除以右操作数。
 * @param other 参与当前运算或比较的另一对象。
 * @retval 更新后的当前数值对象引用。
 */
BigInt &BigInt::operator/=(const BigInt &other)
{
    *this = *this / other;
    return *this;
}
/**
 * @brief 将当前值更新为除以右操作数后的余数。
 * @param other 参与当前运算或比较的另一对象。
 * @retval 更新后的当前数值对象引用。
 */
BigInt &BigInt::operator%=(const BigInt &other)
{
    *this = *this % other;
    return *this;
}

/**
 * @brief 推进迭代器或将当前数值增加一。
 * @retval 前置形式返回更新后的对象引用，后置形式返回修改前的数值副本。
 */
BigInt &BigInt::operator++()
{
    *this += BigInt(1);
    return *this;
}
/**
 * @brief 推进迭代器或将当前数值增加一。
 * @retval 前置形式返回更新后的对象引用，后置形式返回修改前的数值副本。
 */
BigInt BigInt::operator++(int)
{
    BigInt before = *this;
    ++(*this);
    return before;
}
/**
 * @brief 将当前数值减小一。
 * @retval 前置形式返回更新后的对象引用，后置形式返回修改前的数值副本。
 */
BigInt &BigInt::operator--()
{
    *this -= BigInt(1);
    return *this;
}
/**
 * @brief 将当前数值减小一。
 * @retval 前置形式返回更新后的对象引用，后置形式返回修改前的数值副本。
 */
BigInt BigInt::operator--(int)
{
    BigInt before = *this;
    --(*this);
    return before;
}

/**
 * @brief 判断两个值是否相等。
 * @param left 左操作数。
 * @param right 右操作数。
 * @retval 两个值相等时为 true，否则为 false。
 */
bool operator==(const BigInt &left, const BigInt &right) { return left.compare(right) == 0; }
/**
 * @brief 判断两个值是否不相等。
 * @param left 左操作数。
 * @param right 右操作数。
 * @retval 两个值不相等时为 true，否则为 false。
 */
bool operator!=(const BigInt &left, const BigInt &right) { return left.compare(right) != 0; }
/**
 * @brief 判断左值是否小于右值。
 * @param left 左操作数。
 * @param right 右操作数。
 * @retval 左值小于右值时为 true，否则为 false。
 */
bool operator<(const BigInt &left, const BigInt &right) { return left.compare(right) < 0; }
/**
 * @brief 判断左值是否小于或等于右值。
 * @param left 左操作数。
 * @param right 右操作数。
 * @retval 左值小于或等于右值时为 true，否则为 false。
 */
bool operator<=(const BigInt &left, const BigInt &right) { return left.compare(right) <= 0; }
/**
 * @brief 判断左值是否大于右值。
 * @param left 左操作数。
 * @param right 右操作数。
 * @retval 左值大于右值时为 true，否则为 false。
 */
bool operator>(const BigInt &left, const BigInt &right) { return left.compare(right) > 0; }
/**
 * @brief 判断左值是否大于或等于右值。
 * @param left 左操作数。
 * @param right 右操作数。
 * @retval 左值大于或等于右值时为 true，否则为 false。
 */
bool operator>=(const BigInt &left, const BigInt &right) { return left.compare(right) >= 0; }

}  // namespace bigint

#endif
