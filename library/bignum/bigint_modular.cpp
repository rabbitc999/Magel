/**
 * @file bigint_modular.cpp
 * @brief 模运算、扩展欧几里得与概率素性检验。
 */

#if !defined(NO_CHALI_BIG_INT)
#include "bigint.hpp"
#include <algorithm>
#include <limits>
#include <stdexcept>

namespace bigint
{
// ---------------------------------------------------------------------------
//  数论：非负取模 / 快速幂（大指数）/ 扩展欧几里得 / 模逆元 / 素性
// ---------------------------------------------------------------------------

/**
 * @brief 非负取模：结果落在 `[0, |modulus|)`（模为零抛异常）。
 * @param modulus 模数。
 * @retval 计算得到的大数值；复合赋值运算返回当前对象。
 */
BigInt BigInt::modNonNegative(const BigInt &modulus) const
{
    if (modulus.isZero()) throw std::domain_error("bigint: 模数为零");
    BigInt remainder;
    divmod(modulus, nullptr, &remainder);
    if (!remainder.isZero() && remainder.sign_ < 0) remainder += modulus.abs();
    return remainder;
}

/**
 * @brief 同余幂（**指数也是大整数**的快速幂）：`base ^ exponent mod modulus`。
 * @param exponent 指数（大整数，负指数抛 `std::invalid_argument`）。
 * @param modulus 模数。
 * @retval 计算得到的大数值；复合赋值运算返回当前对象。
 */
BigInt BigInt::powmod(const BigInt &exponent, const BigInt &modulus) const
{
    if (modulus.isZero()) throw std::domain_error("bigint: 模数为零");
    if (exponent.sign_ < 0) throw std::invalid_argument("bigint: 负指数没有模幂（请先求模逆元）");
    const BigInt m = modulus.abs();
    BigInt result(1);
    result %= m;
    BigInt base = modNonNegative(m);
    BigInt remaining = exponent;
    while (!remaining.isZero())
    {
        if (remaining.isOdd()) result = (result * base) % m;
        remaining.shiftRight(1u);
        if (!remaining.isZero()) base = (base * base) % m;
    }
    return result;
}

/**
 * @brief 扩展欧几里得：同时给出 `gcd(a, b)` 与裴蜀系数 `x`、`y`。
 * @param a 第一个数。
 * @param b 第二个数。
 * @retval `a × x + b × y = gcd(a, b)` 的三元组。
 */
ExtendedGcd BigInt::extendedGcd(const BigInt &a, const BigInt &b)
{
    /* 在绝对值上跑迭代版扩展欧几里得，最后按原符号把系数翻回去。 */
    BigInt oldR = a.abs();
    BigInt r = b.abs();
    BigInt oldS(1);
    BigInt s;
    BigInt oldT;
    BigInt t(1);
    while (!r.isZero())
    {
        BigInt quotient;
        BigInt remainder;
        oldR.divmod(r, &quotient, &remainder);
        oldR = r;
        r = remainder;
        const BigInt nextS = oldS - quotient * s;
        oldS = s;
        s = nextS;
        const BigInt nextT = oldT - quotient * t;
        oldT = t;
        t = nextT;
    }
    ExtendedGcd out;
    out.gcd = oldR;
    out.x = oldS;
    out.y = oldT;
    if (a.sign_ < 0) out.x = -out.x;
    if (b.sign_ < 0) out.y = -out.y;
    return out;
}

/**
 * @brief 模逆元：`value × inverse ≡ 1 (mod modulus)`。
 * @param value 待求逆的值。
 * @param modulus 模数。
 * @retval 落在 `[0, |modulus|)` 的逆元。
 */
BigInt BigInt::modInverse(const BigInt &value, const BigInt &modulus)
{
    if (modulus.isZero()) throw std::domain_error("bigint: 模数为零");
    const ExtendedGcd result = extendedGcd(value, modulus);
    if (result.gcd != BigInt(1))
    {
        throw std::domain_error("bigint: 模逆元不存在（两数不互素）");
    }
    return result.x.modNonNegative(modulus);
}

/**
 * @brief 概率素性测试（Miller-Rabin，固定种子的确定性底数 + 小素数试除）。
 * @param rounds Miller-Rabin 轮数。
 * @retval 大概率是素数时为 true，一定是合数时为 false。
 */
bool BigInt::isProbablePrime(std::size_t rounds) const
{
    static const std::uint32_t kSmallPrimes[] = { 2u, 3u, 5u, 7u, 11u, 13u, 17u, 19u, 23u, 29u, 31u, 37u };
    if (sign_ < 0 || compare(BigInt(2)) < 0) return false;
    for (const std::uint32_t prime : kSmallPrimes)
    {
        const BigInt remainder = *this % BigInt(prime);
        if (remainder.isZero()) return *this == BigInt(prime);
    }
    /* 没被 ≤ 37 的素数整除、且自身小于 41²，此时一定是素数 */
    if (compare(BigInt(41) * BigInt(41)) < 0) return true;

    /* n − 1 = d × 2^s（d 奇） */
    BigInt d = *this - BigInt(1);
    std::size_t s = 0u;
    while (d.isEven())
    {
        d.shiftRight(1u);
        ++s;
    }
    /* xorshift64：固定种子，同一输入每轮底数完全确定（可复现）。 */
    std::uint64_t state = 88172645463325252ull;
    const BigInt minusOne = *this - BigInt(1);
    const BigInt minusThree = *this - BigInt(3);
    for (std::size_t round = 0u; round < rounds; ++round)
    {
        state ^= state << 13u;
        state ^= state >> 7u;
        state ^= state << 17u;
        const BigInt base = BigInt::fromU64(state).modNonNegative(minusThree) + BigInt(2);
        BigInt x = base.powmod(d, *this);
        if (x == BigInt(1) || x == minusOne) continue;
        bool witness = true;
        for (std::size_t step = 1u; step < s; ++step)
        {
            x = (x * x) % *this;
            if (x == minusOne)
            {
                witness = false;
                break;
            }
        }
        if (witness) return false;
    }
    return true;
}

/**
 * @brief 严格大于当前值的下一个（概率）素数。
 * @param rounds Miller-Rabin 轮数。
 * @retval 计算得到的大数值；复合赋值运算返回当前对象。
 */
BigInt BigInt::nextProbablePrime(std::size_t rounds) const
{
    if (sign_ < 0 || compare(BigInt(2)) < 0) return BigInt(2);
    BigInt candidate = *this + BigInt(1);
    if (candidate.isEven()) ++candidate;
    while (!candidate.isProbablePrime(rounds)) candidate += BigInt(2);
    return candidate;
}

}  // namespace bigint

#endif
