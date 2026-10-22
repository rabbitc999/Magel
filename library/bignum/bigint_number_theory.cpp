/**
 * @file bigint_number_theory.cpp
 * @brief 整数幂与公约数。
 */

#if !defined(NO_CHALI_BIG_INT)
#include "bigint.hpp"
#include <algorithm>
#include <limits>
#include <stdexcept>

namespace bigint
{
// ---------------------------------------------------------------------------
//  幂 / 公约数 / 同余
// ---------------------------------------------------------------------------

/**
 * @brief 同号幂：`*this ^ exponent`（二进制快速幂，`0^0 = 1`）。
 * @param exponent 指数（无符号）。
 * @retval 计算得到的大数值；复合赋值运算返回当前对象。
 */
BigInt BigInt::pow(std::uint64_t exponent) const
{
    BigInt result(1);
    BigInt base = *this;
    std::uint64_t remaining = exponent;
    while (remaining != 0u)
    {
        if ((remaining & 1u) != 0u) result *= base;
        remaining >>= 1u;
        if (remaining != 0u) base *= base;
    }
    return result;
}

/**
 * @brief 最大公约数（欧几里得，取绝对值；`gcd(0, 0) = 0`）。
 * @param left 左操作数。
 * @param right 右操作数。
 * @retval 计算得到的大数值；复合赋值运算返回当前对象。
 */
BigInt BigInt::gcd(const BigInt &left, const BigInt &right)
{
    BigInt a = left.abs();
    BigInt b = right.abs();
    while (!b.isZero())
    {
        BigInt remainder;
        a.divmod(b, nullptr, &remainder);
        a = b;
        b = remainder;
    }
    return a;
}

/**
 * @brief 最小公倍数（`lcm(0, x) = 0`）。
 * @param left 左操作数。
 * @param right 右操作数。
 * @retval 计算得到的大数值；复合赋值运算返回当前对象。
 */
BigInt BigInt::lcm(const BigInt &left, const BigInt &right)
{
    if (left.isZero() || right.isZero()) return BigInt();
    const BigInt divisor = gcd(left, right);
    BigInt out = (left.abs() / divisor) * right.abs();
    return out;
}

/**
 * @brief 同余幂：`base ^ exponent mod modulus`（快速幂，逐步取模；模为零抛异常）。
 * @param exponent 指数（无符号）。
 * @param modulus 模数。
 * @retval 计算得到的大数值；复合赋值运算返回当前对象。
 */
BigInt BigInt::powmod(std::uint64_t exponent, const BigInt &modulus) const
{
    if (modulus.isZero()) throw std::domain_error("bigint: 模数为零");
    BigInt result(1);
    result %= modulus;
    BigInt base = *this % modulus;
    if (!base.isZero() && base.sign_ < 0) base += modulus.abs();
    std::uint64_t remaining = exponent;
    while (remaining != 0u)
    {
        if ((remaining & 1u) != 0u) result = (result * base) % modulus;
        remaining >>= 1u;
        if (remaining != 0u) base = (base * base) % modulus;
    }
    if (!result.isZero() && result.sign_ < 0) result += modulus.abs();
    return result;
}

}  // namespace bigint

#endif
