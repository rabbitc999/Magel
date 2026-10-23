/**
 * @file test_bigint.cpp
 * @brief `bigint` 单元测试：构造 / 互转、比较、四则、幂、gcd、同余幂与边界。
 */

#include "bigint.hpp"

#include <iostream>
#include <string>

namespace
{
int checks = 0;
int failures = 0;
#define CHECK(condition)                                                         \
    do                                                                           \
    {                                                                            \
        ++checks;                                                                \
        if (!(condition))                                                        \
        {                                                                        \
            ++failures;                                                          \
            std::cerr << "  失败：" #condition "（第 " << __LINE__ << " 行）\n"; \
        }                                                                        \
    } while (false)

using bigint::BigInt;

/**
 * @brief 构造 / 十进制互转 / 截断。
 */
void testConstruct()
{
    CHECK(BigInt().toString() == Memory::String("0"));
    CHECK(BigInt(0).isZero());
    CHECK(BigInt(42).toString() == Memory::String("42"));
    CHECK(BigInt(-42).toString() == Memory::String("-42"));
    CHECK(BigInt(4294967296ll).toString() == Memory::String("4294967296"));                          // 2^32
    CHECK(BigInt(-9223372036854775807ll - 1).toString() == Memory::String("-9223372036854775808"));  // INT64_MIN

    CHECK(BigInt("0").isZero());
    CHECK(BigInt("+123").toString() == Memory::String("123"));
    CHECK(BigInt("-000123").toString() == Memory::String("-123"));
    CHECK(BigInt("1606938044258990275541962092341162602522202993782792835301376").toString() == Memory::String("1606938044258990275541962092341162602522202993782792835301376"));  // 2^200

    bool ok = false;
    CHECK(BigInt::fromString("12a", &ok).isZero());
    CHECK(!ok);
    CHECK(BigInt::fromString("", &ok).isZero());
    CHECK(!ok);
    CHECK(BigInt::fromString("-", &ok).isZero());
    CHECK(!ok);
    CHECK(BigInt::fromString("9999999999999999999999999999", &ok).toString() == Memory::String("9999999999999999999999999999"));
    CHECK(ok);

    CHECK(BigInt("1606938044258990275541962092341162602522202993782792835301376").toU64() == 0u);
    CHECK(BigInt::fromU64(18446744073709551615ull).toString() == Memory::String("18446744073709551615"));
    CHECK(BigInt::fromU64(18446744073709551615ull).toU64() == 18446744073709551615ull);
    CHECK(BigInt::fromU64(0u).isZero());
    CHECK(BigInt::fromU64(4294967296ull).toString() == Memory::String("4294967296"));
    CHECK(BigInt(-1).toU64() == 1u);  // 只看幅值
    CHECK(BigInt(12345).toDouble() == 12345.0);
    CHECK(BigInt(-12345).toDouble() == -12345.0);
    CHECK(BigInt("1606938044258990275541962092341162602522202993782792835301376").decimalDigits() == 61u);
    CHECK(BigInt(0).decimalDigits() == 1u);
    CHECK(BigInt(1).limbCount() == 1u);
    CHECK(BigInt(0).limbCount() == 0u);
}

/**
 * @brief 比较与符号。
 */
void testCompare()
{
    CHECK(BigInt(-5).compare(BigInt(3)) == -1);
    CHECK(BigInt(5).compare(BigInt(-3)) == 1);
    CHECK(BigInt(-5).compare(BigInt(-3)) == -1);
    CHECK(BigInt(5).compare(BigInt(5)) == 0);
    CHECK(BigInt(0).compare(BigInt(0)) == 0);
    CHECK(BigInt(0) < BigInt(1));
    CHECK(BigInt(-1) < BigInt(0));
    CHECK(BigInt("100000000000000000000000") > BigInt("99999999999999999999999"));
    CHECK(BigInt(7) != BigInt(8));
    CHECK(BigInt(7) >= BigInt(7));
    CHECK(BigInt(-7) <= BigInt(-7));
    CHECK(BigInt(-9).abs() == BigInt(9));
    CHECK(BigInt(9).sign() == 1);
    CHECK(BigInt(-9).sign() == -1);
    CHECK(BigInt(0).sign() == 0);
}

/**
 * @brief 加减乘（含进位 / 借位 / 符号交叉）。
 */
void testArithmetic()
{
    CHECK((BigInt(2) + BigInt(3)).toString() == Memory::String("5"));
    CHECK((BigInt(4294967295ll) + BigInt(1)).toString() == Memory::String("4294967296"));
    CHECK((BigInt(-5) + BigInt(3)).toString() == Memory::String("-2"));
    CHECK((BigInt(5) + BigInt(-3)).toString() == Memory::String("2"));
    CHECK((BigInt(-5) + BigInt(5)).isZero());
    CHECK((BigInt(0) + BigInt(-7)).toString() == Memory::String("-7"));

    CHECK((BigInt(2) - BigInt(3)).toString() == Memory::String("-1"));
    CHECK((BigInt(-2) - BigInt(-3)).toString() == Memory::String("1"));
    CHECK((BigInt(4294967296ll) - BigInt(1)).toString() == Memory::String("4294967295"));
    CHECK((BigInt(3) - BigInt(3)).isZero());

    CHECK((BigInt(6) * BigInt(7)).toString() == Memory::String("42"));
    CHECK((BigInt(-6) * BigInt(7)).toString() == Memory::String("-42"));
    CHECK((BigInt(-6) * BigInt(-7)).toString() == Memory::String("42"));
    CHECK((BigInt(0) * BigInt(999)).isZero());
    CHECK((BigInt(65536) * BigInt(65536)).toString() == Memory::String("4294967296"));
    CHECK((BigInt("12345678901234567890") * BigInt("98765432109876543210")).toString() == Memory::String("1219326311370217952237463801111263526900"));
    CHECK((BigInt("99999999999999999999") * BigInt("99999999999999999999")).toString() == Memory::String("9999999999999999999800000000000000000001"));

    BigInt value(10);
    CHECK((value += BigInt(5)).toString() == Memory::String("15"));
    CHECK((value -= BigInt(20)).toString() == Memory::String("-5"));
    CHECK((value *= BigInt(-2)).toString() == Memory::String("10"));
    CHECK((++value).toString() == Memory::String("11"));
    CHECK((value++).toString() == Memory::String("11"));
    CHECK(value.toString() == Memory::String("12"));
    CHECK((--value).toString() == Memory::String("11"));
    CHECK((value--).toString() == Memory::String("11"));
    CHECK(value.toString() == Memory::String("10"));
}

/**
 * @brief 除取整（向零截断）、余数符号、除零。
 */
void testDivision()
{
    const std::size_t defaultDivision = BigInt::divisionThreshold();
    BigInt quotient;
    BigInt remainder;
    CHECK(BigInt(7).divmod(BigInt(2), &quotient, &remainder));
    CHECK(quotient.toString() == Memory::String("3"));
    CHECK(remainder.toString() == Memory::String("1"));
    CHECK((BigInt(-7) / BigInt(2)).toString() == Memory::String("-3"));  // 向零截断
    CHECK((BigInt(-7) % BigInt(2)).toString() == Memory::String("-1"));  // 余数跟被除数
    CHECK((BigInt(7) / BigInt(-2)).toString() == Memory::String("-3"));
    CHECK((BigInt(7) % BigInt(-2)).toString() == Memory::String("1"));
    CHECK((BigInt(0) / BigInt(5)).isZero());
    CHECK((BigInt(3) / BigInt(1000)).isZero());
    CHECK((BigInt(3) % BigInt(1000)).toString() == Memory::String("3"));

    CHECK(BigInt(12345).divmod(BigInt(), &quotient, &remainder) == false);  // 除零
    bool threw = false;
    try
    {
        (void)(BigInt(1) / BigInt(0));
    } catch (const std::domain_error &) { threw = true; }
    CHECK(threw);
    threw = false;
    try
    {
        (void)(BigInt(1) % BigInt(0));
    } catch (const std::domain_error &) { threw = true; }
    CHECK(threw);

    const BigInt big("1606938044258990275541962092341162602522202993782792835301376");  // 2^200
    const BigInt divisor("12345678901234567890");
    CHECK((big / divisor).toString() == Memory::String("130161982756436057245270848995428623856352"));
    CHECK((big % divisor).toString() == Memory::String("6540610052683564096"));
    CHECK((big / divisor) * divisor + (big % divisor) == big);  // 恒等式
    CHECK((big / big).toString() == Memory::String("1"));
    CHECK((big % big).isZero());
    CHECK((big / BigInt("999999999999999999999999999999")) * BigInt("999999999999999999999999999999") + (big % BigInt("999999999999999999999999999999")) == big);

    /* 强制 Burnikel-Ziegler 与长除法分别计算同一组非 2 次幂长度数据。 */
    const BigInt bzDivisor = BigInt(2).pow(2050u) + BigInt(123456789);
    const BigInt expectedQuotient = BigInt(3).pow(800u) + BigInt(77);
    const BigInt expectedRemainder = BigInt(2).pow(1024u) + BigInt(99);
    const BigInt bzDividend = bzDivisor * expectedQuotient + expectedRemainder;
    BigInt::setDivisionThreshold(32u);
    BigInt bzQuotient;
    BigInt bzRemainder;
    CHECK(bzDividend.divmod(bzDivisor, &bzQuotient, &bzRemainder));
    BigInt::setDivisionThreshold(1000000u);
    BigInt baseQuotient;
    BigInt baseRemainder;
    CHECK(bzDividend.divmod(bzDivisor, &baseQuotient, &baseRemainder));
    CHECK(bzQuotient == baseQuotient && bzQuotient == expectedQuotient);
    CHECK(bzRemainder == baseRemainder && bzRemainder == expectedRemainder);
    CHECK((-bzDividend) / bzDivisor == -expectedQuotient);
    CHECK((-bzDividend) % bzDivisor == -expectedRemainder);
    BigInt::setDivisionThreshold(defaultDivision);
    CHECK(BigInt::divisionThreshold() == defaultDivision);
}

/**
 * 幂、gcd / lcm、同余幂。
 */
/**
 * @brief 乘法内核：Karatsuba 与 NTT 必须逐位一致，并验证保护阈值不会改变结果。
 */
void testNtt()
{
    const std::size_t defaultMultiplication = BigInt::multiplicationThreshold();
    /* 造几个"够大"的数：2^1000、10^100、随机形状的 limb 序列 */
    const BigInt big = BigInt(2).pow(1000);
    const BigInt ten = BigInt(10).pow(100);
    BigInt randomish("1234567890123456789012345678901234567890123456789012345678901234567890"
                     "9876543210987654321098765432109876543210987654321098765432109876543210");

    /* 依次强制 NTT、Karatsuba 和扩大的 Karatsuba 保护区，逐位比对。 */
    BigInt::setMultiplicationThreshold(1u);
    const BigInt ntt1 = big * ten;
    const BigInt ntt2 = big * randomish;
    const BigInt ntt3 = randomish * randomish;
    const BigInt ntt4 = BigInt(2).pow(4096) * BigInt(2).pow(4096);
    BigInt::setMultiplicationThreshold(1000000u);
    BigInt::setKaratsubaThreshold(1u);
    const BigInt karatsuba1 = big * ten;
    const BigInt karatsuba2 = big * randomish;
    const BigInt karatsuba3 = randomish * randomish;
    const BigInt karatsuba4 = BigInt(2).pow(4096) * BigInt(2).pow(4096);
    BigInt::setKaratsubaThreshold(1000000u);
    const BigInt protectedKaratsuba1 = big * ten;
    const BigInt protectedKaratsuba2 = big * randomish;
    const BigInt protectedKaratsuba3 = randomish * randomish;
    const BigInt protectedKaratsuba4 = BigInt(2).pow(4096) * BigInt(2).pow(4096);
    CHECK(ntt1 == karatsuba1 && karatsuba1 == protectedKaratsuba1);
    CHECK(ntt2 == karatsuba2 && karatsuba2 == protectedKaratsuba2);
    CHECK(ntt3 == karatsuba3 && karatsuba3 == protectedKaratsuba3);
    CHECK(ntt4 == karatsuba4 && karatsuba4 == protectedKaratsuba4);

    /* NTT 路径也要和"人工已知值"一致 */
    BigInt::setMultiplicationThreshold(1u);
    CHECK(ntt4 == BigInt(2).pow(8192));
    CHECK(ntt1 == BigInt(2).pow(1000) * BigInt(10).pow(100));
    CHECK((ntt3 / randomish) == randomish);
    CHECK((ntt3 % randomish).isZero());

    /* 大数除法 / 取余在 NTT 乘法下依然自洽：q × b + r == a */
    const BigInt divisor = BigInt(3).pow(200);
    const BigInt quotient = ntt2 / divisor;
    const BigInt remainder = ntt2 % divisor;
    CHECK(quotient * divisor + remainder == ntt2);
    CHECK(remainder < divisor);

    /* 换回默认阈值，后面的用例不受影响 */
    BigInt::setMultiplicationThreshold(defaultMultiplication);
    BigInt::setKaratsubaThreshold(96u);
    CHECK(BigInt::multiplicationThreshold() == defaultMultiplication);
    CHECK(BigInt::karatsubaThreshold() == 96u);
}

/**
 * @brief 验证大整数幂运算与最大公约数运算。
 */
void testPowerAndGcd()
{
    CHECK(BigInt(2).pow(10).toString() == Memory::String("1024"));
    CHECK(BigInt(2).pow(0).toString() == Memory::String("1"));
    CHECK(BigInt(0).pow(0).toString() == Memory::String("1"));
    CHECK(BigInt(0).pow(5).isZero());
    CHECK(BigInt(-3).pow(3).toString() == Memory::String("-27"));
    CHECK(BigInt(2).pow(200).toString() == Memory::String("1606938044258990275541962092341162602522202993782792835301376"));
    CHECK(BigInt(3).pow(40).toString() == Memory::String("12157665459056928801"));

    const BigInt left = BigInt(2).pow(100) * BigInt(3);
    const BigInt right = BigInt(3).pow(50) * BigInt(5);
    CHECK(BigInt::gcd(left, right).toString() == Memory::String("3"));
    CHECK(BigInt::gcd(BigInt(0), BigInt(0)).isZero());
    CHECK(BigInt::gcd(BigInt(0), BigInt(-12)).toString() == Memory::String("12"));
    CHECK(BigInt::gcd(BigInt(-18), BigInt(24)).toString() == Memory::String("6"));
    CHECK(BigInt::lcm(BigInt(4), BigInt(6)).toString() == Memory::String("12"));
    CHECK(BigInt::lcm(BigInt(0), BigInt(7)).isZero());
    CHECK(BigInt::lcm(left, right).toString() == Memory::String("4550219075001074886663792637671283162463576301628293120"));

    CHECK(BigInt(7).powmod(300, BigInt(1000000007)).toString() == Memory::String("678144625"));
    CHECK(BigInt(2).powmod(10, BigInt(1000)).toString() == Memory::String("24"));
    CHECK(BigInt(-2).powmod(3, BigInt(5)).toString() == Memory::String("2"));  // (−8) mod 5
    CHECK(BigInt(123).powmod(0, BigInt(97)).toString() == Memory::String("1"));
}

/**
 * @brief 位运算 / 位操作 / 判断 / 最值。
 */
void testBits()
{
    const BigInt twelve(12);
    CHECK(twelve.bitLength() == 4u);
    CHECK(!twelve.bitAt(0u));
    CHECK(twelve.bitAt(2u));
    CHECK(!twelve.bitAt(4u));
    CHECK(BigInt(0).bitLength() == 0u);
    CHECK(BigInt(-12).bitLength() == 4u); /* 幅值的位长 */

    BigInt value(12);
    value.setBit(0u);
    CHECK(value == BigInt(13));
    value.clearBit(3u);
    CHECK(value == BigInt(5));
    value.flipBit(0u);
    CHECK(value == BigInt(4));
    value.flipBit(10u);
    CHECK(value == BigInt(1028));
    BigInt zero;
    zero.setBit(5u);
    CHECK(zero == BigInt(32)); /* 对 0 置位后应成为 +32，而非留在"零表示" */
    BigInt shifted(1);
    shifted.shiftLeft(10u);
    CHECK(shifted == BigInt(1024));
    shifted.shiftRight(11u);
    CHECK(shifted.isZero());
    shifted.setBit(3u);
    shifted.shiftRight(100u);
    CHECK(shifted.isZero());

    CHECK((BigInt(12) & BigInt(10)) == BigInt(8));
    CHECK((BigInt(12) | BigInt(10)) == BigInt(14));
    CHECK((BigInt(12) ^ BigInt(10)) == BigInt(6));
    const BigInt bigA = BigInt(2).pow(100) + BigInt(123);
    const BigInt bigB = BigInt(2).pow(100) + BigInt(456);
    CHECK((bigA & bigB) == BigInt(2).pow(100) + BigInt(123 & 456));
    CHECK((bigA | bigB) == bigA + bigB - (bigA & bigB)); /* a | b = a + b − (a & b) */
    CHECK((bigA ^ bigB) == bigA + bigB - BigInt(2) * (bigA & bigB));

    bool threw = false;
    try
    {
        (void)(BigInt(-1) & BigInt(1));
    } catch (const std::domain_error &) { threw = true; }
    CHECK(threw);
    threw = false;
    try
    {
        (void)(BigInt(1) | BigInt(-1));
    } catch (const std::domain_error &) { threw = true; }
    CHECK(threw);
    threw = false;
    try
    {
        (void)(BigInt(-1) ^ BigInt(-1));
    } catch (const std::domain_error &) { threw = true; }
    CHECK(threw);
    CHECK((~BigInt(5)) == BigInt(-6));
    CHECK((~BigInt(0)) == BigInt(-1));
    CHECK((~BigInt(-3)) == BigInt(2));

    CHECK(BigInt(4).isEven());
    CHECK(!BigInt(4).isOdd());
    CHECK(BigInt(-3).isOdd());
    CHECK(!BigInt(-3).isEven());
    CHECK(BigInt(0).isEven());
    CHECK(!BigInt(0).isOdd());
    CHECK(BigInt(-7).isNegative());
    CHECK(!BigInt(-7).isPositive());
    CHECK(BigInt(7).isPositive());
    CHECK(!BigInt(7).isNegative());
    CHECK(!BigInt(0).isPositive());
    CHECK(!BigInt(0).isNegative());
    CHECK(BigInt::min(BigInt(-5), BigInt(3)) == BigInt(-5));
    CHECK(BigInt::max(BigInt(-5), BigInt(3)) == BigInt(3));
    CHECK(BigInt::min(BigInt(9), BigInt(9)) == BigInt(9));
}

/**
 * @brief 数论：非负取模 / 大指数快速幂 / 扩展欧几里得 / 模逆元。
 */
void testNumberTheory()
{
    CHECK(BigInt(-7).modNonNegative(BigInt(3)) == BigInt(2));
    CHECK(BigInt(7).modNonNegative(BigInt(-3)) == BigInt(1)); /* 模取绝对值 */
    CHECK(BigInt(0).modNonNegative(BigInt(5)).isZero());
    bool threw = false;
    try
    {
        (void)BigInt(1).modNonNegative(BigInt(0));
    } catch (const std::domain_error &) { threw = true; }
    CHECK(threw);

    const bigint::ExtendedGcd eg = BigInt::extendedGcd(BigInt(240), BigInt(46));
    CHECK(eg.gcd == BigInt(2));
    CHECK(eg.x == BigInt(-9));
    CHECK(eg.y == BigInt(47));
    CHECK(BigInt(240) * eg.x + BigInt(46) * eg.y == eg.gcd);
    const bigint::ExtendedGcd eg2 = BigInt::extendedGcd(BigInt(-240), BigInt(46));
    CHECK(eg2.gcd == BigInt(2));
    CHECK(BigInt(-240) * eg2.x + BigInt(46) * eg2.y == eg2.gcd);

    CHECK(BigInt::modInverse(BigInt(3), BigInt(11)) == BigInt(4));
    CHECK((BigInt(3) * BigInt::modInverse(BigInt(3), BigInt(11))).modNonNegative(BigInt(11)) == BigInt(1));
    CHECK(BigInt::modInverse(BigInt(-3), BigInt(11)) == BigInt(7)); /* −3 ≡ 8，8 × 7 ≡ 1 (mod 11) */
    threw = false;
    try
    {
        (void)BigInt::modInverse(BigInt(3), BigInt(12));
    } catch (const std::domain_error &) { threw = true; }
    CHECK(threw);
    threw = false;
    try
    {
        (void)BigInt::modInverse(BigInt(1), BigInt(0));
    } catch (const std::domain_error &) { threw = true; }
    CHECK(threw);

    /* 大指数快速幂：与 64 位指数版一致、指数可加性、费马小定理。 */
    const BigInt modulus("1000000007");
    CHECK(BigInt(7).powmod(BigInt(300), modulus) == BigInt(7).powmod(300u, modulus));
    const BigInt e1 = BigInt(2).pow(100) + BigInt(5);
    const BigInt e2 = BigInt(3).pow(20);
    const BigInt lhs = BigInt(3).powmod(e1 + e2, modulus);
    const BigInt rhs = (BigInt(3).powmod(e1, modulus) * BigInt(3).powmod(e2, modulus)).modNonNegative(modulus);
    CHECK(lhs == rhs);
    CHECK(BigInt(3).powmod(modulus - BigInt(1), modulus) == BigInt(1));
    threw = false;
    try
    {
        (void)BigInt(2).powmod(BigInt(-1), modulus);
    } catch (const std::invalid_argument &) { threw = true; }
    CHECK(threw);
    threw = false;
    try
    {
        (void)BigInt(2).powmod(BigInt(3), BigInt(0));
    } catch (const std::domain_error &) { threw = true; }
    CHECK(threw);
}

/**
 * @brief 概率素性测试与下一个素数。
 */
void testPrimes()
{
    CHECK(!BigInt(-7).isProbablePrime());
    CHECK(!BigInt(0).isProbablePrime());
    CHECK(!BigInt(1).isProbablePrime());
    CHECK(BigInt(2).isProbablePrime());
    CHECK(BigInt(3).isProbablePrime());
    CHECK(!BigInt(4).isProbablePrime());
    CHECK(!BigInt(561).isProbablePrime());
    CHECK(!BigInt(41041).isProbablePrime());                   /* Carmichael 数：7 × 11 × 13 × 41 */
    CHECK(BigInt(2147483647ll).isProbablePrime());             /* 2^31 − 1 */
    CHECK(BigInt("2305843009213693951").isProbablePrime());    /* 2^61 − 1 */
    CHECK(!BigInt("147573952589676412927").isProbablePrime()); /* 2^67 − 1（Mersenne 合数） */
    CHECK(BigInt(1).nextProbablePrime() == BigInt(2));
    CHECK(BigInt(10).nextProbablePrime() == BigInt(11));
    CHECK(BigInt(2147483647ll).nextProbablePrime() == BigInt("2147483659"));
    CHECK(BigInt(2147483647ll).nextProbablePrime() > BigInt(2147483647ll));
}

/**
 * @brief 2..36 进制互转与 int64 隐式 / 显式转换。
 */
void testBaseConvert()
{
    CHECK(BigInt(255).toString(2) == Memory::String("11111111"));
    CHECK(BigInt(255).toString(16) == Memory::String("ff"));
    CHECK(BigInt(255).toString(36) == Memory::String("73"));
    CHECK(BigInt(-255).toString(16) == Memory::String("-ff"));
    CHECK(BigInt(0).toString(8) == Memory::String("0"));
    bool threw = false;
    try
    {
        (void)BigInt(1).toString(1);
    } catch (const std::invalid_argument &) { threw = true; }
    CHECK(threw);
    threw = false;
    try
    {
        (void)BigInt(1).toString(37);
    } catch (const std::invalid_argument &) { threw = true; }
    CHECK(threw);

    CHECK(BigInt::fromString("ff", 16) == BigInt(255));
    CHECK(BigInt::fromString("-FF", 16) == BigInt(-255));
    CHECK(BigInt::fromString("zz", 36) == BigInt(1295));
    CHECK(BigInt::fromString("1010", 2) == BigInt(10));
    bool ok = true;
    CHECK(BigInt::fromString("2", 2, &ok).isZero());
    CHECK(!ok);
    CHECK(BigInt::fromString("12g", 16, &ok).isZero());
    CHECK(!ok);
    CHECK(BigInt::fromString("", 10, &ok).isZero());
    CHECK(!ok);
    CHECK(BigInt::fromString("  7f  ", 16, &ok) == BigInt(127));
    CHECK(ok);
    threw = false;
    try
    {
        (void)BigInt::fromString("1", 1);
    } catch (const std::invalid_argument &) { threw = true; }
    CHECK(threw);

    const BigInt big = BigInt(2).pow(300) + BigInt(987654321);
    for (unsigned int base = 2u; base <= 36u; ++base)
    {
        CHECK(BigInt::fromString(big.toString(base), base) == big);
    }

    /* int64 → BigInt 隐式；BigInt → int64 / double 显式。 */
    CHECK(BigInt(123) == 123);
    CHECK(-5 < BigInt(0));
    BigInt fromInt64 = 9876543210ll;
    CHECK(fromInt64.toString() == Memory::String("9876543210"));
    CHECK(static_cast<std::int64_t>(BigInt(12345)) == 12345);
    CHECK(static_cast<double>(BigInt(-42)) == -42.0);
}

/**
 * @brief 整数 k 次方根。
 */
void testIntegerRoot()
{
    CHECK(bigint::integerRoot(BigInt(1000), 3) == BigInt(10));
    CHECK(bigint::integerRoot(BigInt(999), 3) == BigInt(9));
    CHECK(bigint::integerRoot(BigInt(16), 2) == BigInt(4));
    CHECK(bigint::integerRoot(BigInt(1), 5) == BigInt(1));
    CHECK(bigint::integerRoot(BigInt(0), 7).isZero());
    CHECK(bigint::integerRoot(BigInt(10).pow(30), 3) == BigInt(10).pow(10));
    CHECK(bigint::integerRoot(BigInt(2).pow(200), 4) == BigInt(2).pow(50));
    CHECK(bigint::integerRoot(BigInt(2).pow(1000), 10) == BigInt(2).pow(100));
    bool threw = false;
    try
    {
        (void)bigint::integerRoot(BigInt(-8), 3);
    } catch (const std::domain_error &) { threw = true; }
    CHECK(threw);
    threw = false;
    try
    {
        (void)bigint::integerRoot(BigInt(8), 0);
    } catch (const std::invalid_argument &) { threw = true; }
    CHECK(threw);
}

}  // namespace

/**
 * @brief 运行本文件的回归测试并报告执行结果。
 * @retval 进程退出状态，0 表示成功，非零表示失败。
 */
int main()
{
    testConstruct();
    testCompare();
    testArithmetic();
    testDivision();
    testNtt();
    testPowerAndGcd();
    testBits();
    testNumberTheory();
    testPrimes();
    testBaseConvert();
    testIntegerRoot();
    if (failures == 0)
    {
        std::cout << "bigint-tests：全部通过（" << checks << " 项断言）\n";
        return 0;
    }
    std::cout << "bigint-tests：" << failures << " / " << checks << " 项断言失败\n";
    return 1;
}
