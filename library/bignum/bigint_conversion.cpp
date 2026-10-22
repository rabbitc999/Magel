/**
 * @file bigint_conversion.cpp
 * @brief 整数的文本与机器数值转换。
 */

#if !defined(NO_CHALI_BIG_INT)
#include "bigint.hpp"
#include <algorithm>
#include <limits>
#include <stdexcept>

namespace bigint
{
namespace
{
constexpr std::uint32_t kDecimalChunk = 1000000000u;
}

// ---------------------------------------------------------------------------
//  整数 k 次方根
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
//  十进制互转 / 截断
// ---------------------------------------------------------------------------

/**
 * @brief 十进制字符串（负数带 `-`；0 输出 `"0"`）。
 * @retval 生成或读取的结果文本。
 */
Memory::String BigInt::toString() const
{
    if (isZero()) return "0";
    BigInt magnitude = abs();
    Memory::Vector<std::uint32_t> chunks;
    while (!magnitude.isZero())
    {
        chunks.push_back(magnitude.divSmall(kDecimalChunk));
    }
    Memory::String text;
    if (sign_ < 0) text.push_back('-');
    text += Memory::toString(chunks.back());
    for (std::size_t index = chunks.size() - 1u; index-- > 0u;)
    {
        Memory::String part = Memory::toString(chunks[index]);
        text.append(9u - part.size(), '0');
        text += part;
    }
    return text;
}

/**
 * @brief 十进制位数（0 算 1 位）。
 * @retval 数值的十进制位数，零值为 1。
 */
std::size_t BigInt::decimalDigits() const
{
    if (isZero()) return 1u;
    BigInt magnitude = abs();
    std::size_t digits = 0u;
    while (!magnitude.isZero())
    {
        const std::uint32_t remainder = magnitude.divSmall(kDecimalChunk);
        digits += (magnitude.isZero()) ? Memory::toString(remainder).size() : 9u;
    }
    return digits;
}

/**
 * @brief `base`（2..36）进制字符串：数字用 `0-9a-z`，负数带 `-`。
 * @param base 进制；不在 2..36 抛 `std::invalid_argument`。
 * @retval 生成或读取的结果文本。
 */
Memory::String BigInt::toString(unsigned int base) const
{
    if (base < 2u || base > 36u) throw std::invalid_argument("bigint: 进制必须在 2..36");
    if (isZero()) return "0";
    static const char *kAlphabet = "0123456789abcdefghijklmnopqrstuvwxyz";
    BigInt magnitude = abs();
    Memory::String reversed;
    while (!magnitude.isZero())
    {
        reversed.push_back(kAlphabet[magnitude.divSmall(base)]);
    }
    if (sign_ < 0) reversed.push_back('-');
    Memory::String text;
    for (std::size_t index = reversed.size(); index-- > 0u;) text.push_back(reversed[index]);
    return text;
}

/**
 * @brief 从 `base`（2..36）进制字符串构造（不抛异常版）。
 * @param text 文本。
 * @param base 进制；不在 2..36 抛 `std::invalid_argument`。
 * @param ok 非空时写入是否解析成功。
 * @retval 计算得到的大数值；复合赋值运算返回当前对象。
 */
BigInt BigInt::fromString(const Memory::String &text, unsigned int base, bool *ok)
{
    auto report = [&](bool success) {
        if (ok != nullptr) *ok = success;
    };
    if (base < 2u || base > 36u) throw std::invalid_argument("bigint: 进制必须在 2..36");

    std::size_t index = 0u;
    while (index < text.size() && (text[index] == ' ' || text[index] == '\t')) ++index;
    int sign = 1;
    if (index < text.size() && (text[index] == '+' || text[index] == '-'))
    {
        if (text[index] == '-') sign = -1;
        ++index;
    }
    if (index >= text.size())
    {
        report(false);
        return BigInt();
    }
    BigInt value;
    bool any = false;
    for (; index < text.size(); ++index)
    {
        const char ch = text[index];
        if (ch == ' ' || ch == '\t') break; /* 交给后面的尾部空白检查 */
        unsigned int digit;
        if (ch >= '0' && ch <= '9')
            digit = static_cast<unsigned int>(ch - '0');
        else if (ch >= 'a' && ch <= 'z')
            digit = static_cast<unsigned int>(ch - 'a') + 10u;
        else if (ch >= 'A' && ch <= 'Z')
            digit = static_cast<unsigned int>(ch - 'A') + 10u;
        else
            digit = base; /* 非法字符 */
        if (digit >= base)
        {
            report(false);
            return BigInt();
        }
        value.mulSmallAdd(base, digit);
        any = true;
    }
    while (index < text.size() && (text[index] == ' ' || text[index] == '\t')) ++index;
    if (!any || index != text.size())
    {
        report(false);
        return BigInt();
    }
    if (!value.isZero()) value.sign_ = sign;
    report(true);
    return value;
}

/**
 * @brief 截断成 64 位无符号整数（只取低 64 位，符号位丢弃）。
 * @retval 取原数值低 64 位得到的无符号整数。
 */
std::uint64_t BigInt::toU64() const
{
    std::uint64_t value = 0u;
    for (std::size_t index = std::min<std::size_t>(limbs_.size(), 2u); index-- > 0u;)
    {
        value = (value << 32u) | limbs_[index];
    }
    return value;
}

/**
 * @brief 截断成 64 位有符号整数（按位截断，与 C 的强转一致）。
 * @retval 取原数值低 64 位得到的有符号整数。
 */
std::int64_t BigInt::toI64() const { return static_cast<std::int64_t>(toU64()); }

/**
 * @brief 显式转 64 位有符号整数（按位截断，与 `toI64` 一致）。
 * @retval 取原数值低 64 位得到的有符号整数。
 */
BigInt::operator std::int64_t() const { return toI64(); }

/**
 * @brief 显式转双精度浮点（可能丢精度；符号跟着走）。
 * @retval 保存转双精度浮点结果的对象。
 */
BigInt::operator double() const { return toDouble(); }

/**
 * @brief 转双精度浮点（可能丢精度；符号跟着走）。
 * @retval 保存转双精度浮点（可能丢精度；符号跟着走）结果的对象。
 */
double BigInt::toDouble() const
{
    double value = 0.0;
    for (std::size_t index = limbs_.size(); index-- > 0u;)
    {
        value = value * 4294967296.0 + static_cast<double>(limbs_[index]);
    }
    return (sign_ < 0) ? -value : value;
}

}  // namespace bigint

#endif
