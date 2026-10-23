/**
 * @file bigint_division.cpp
 * @brief 整数幅值的分块长除法与 Burnikel-Ziegler 递归。
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
constexpr std::uint64_t kLimbBase = 1ull << 32;
}

/**
 * @brief 取一段小端 limb。
 * @param value 待转换、保存或参与计算的值。
 * @param offset 相对于起点的字节偏移。
 * @param count 元素数量或操作次数。
 * @retval 计算得到的大数值；复合赋值运算返回当前对象。
 */
BigInt BigInt::limbSlice(const BigInt &value, std::size_t offset, std::size_t count)
{
    BigInt out;
    if (offset >= value.limbs_.size() || count == 0u) return out;
    const std::size_t end = std::min(value.limbs_.size(), offset + count);
    out.limbs_.assign(value.limbs_.begin() + static_cast<std::ptrdiff_t>(offset),
                      value.limbs_.begin() + static_cast<std::ptrdiff_t>(end));
    out.normalize();
    if (!out.limbs_.empty()) out.sign_ = 1;
    return out;
}

/**
 * @brief `high * base^lowWidth + low`。
 * @param high 高位数值块。
 * @param low 低位数值块。
 * @param lowWidth 低位数值块占用的分块数量。
 * @retval 计算得到的大数值；复合赋值运算返回当前对象。
 */
BigInt BigInt::combineBlocks(const BigInt &high, const BigInt &low, std::size_t lowWidth)
{
    BigInt out;
    out.limbs_.assign(lowWidth + high.limbs_.size(), 0u);
    const std::size_t lowCount = std::min(lowWidth, low.limbs_.size());
    std::copy(low.limbs_.begin(), low.limbs_.begin() + static_cast<std::ptrdiff_t>(lowCount),
              out.limbs_.begin());
    std::copy(high.limbs_.begin(), high.limbs_.end(),
              out.limbs_.begin() + static_cast<std::ptrdiff_t>(lowWidth));
    out.normalize();
    if (!out.limbs_.empty()) out.sign_ = 1;
    return out;
}

/**
 * @brief 归一化后按 32 位块估商、校正的 Knuth 长除法基线。
 * @param left 左操作数。
 * @param right 右操作数。
 * @param quotient 接收整除运算得到的商。
 * @param remainder 接收整除运算得到的余数。
 */
void BigInt::divmodBase(const BigInt &left, const BigInt &right,
                        BigInt *quotient, BigInt *remainder)
{
    BigInt quotientValue;
    BigInt remainderValue = left.abs();
    if (compareMagnitude(left, right) < 0)
    {
        if (quotient != nullptr) *quotient = quotientValue;
        if (remainder != nullptr) *remainder = remainderValue;
        return;
    }
    if (right.limbs_.size() == 1u)
    {
        const std::uint32_t rest = remainderValue.divSmall(right.limbs_[0]);
        if (quotient != nullptr) *quotient = std::move(remainderValue);
        if (remainder != nullptr) *remainder = BigInt::fromU64(rest);
        return;
    }

    // Knuth D：将除数最高位归一化，每次估计并校正一个 32 位商块。
    unsigned int shift = 0u;
    for (std::uint32_t top = right.limbs_.back(); (top & 0x80000000u) == 0u; top <<= 1u)
        ++shift;
    BigInt normalizedDivisor = right.abs();
    normalizedDivisor.shiftLeft(shift);
    remainderValue.shiftLeft(shift);
    const std::size_t size = normalizedDivisor.limbs_.size();
    const std::size_t count = left.limbs_.size() - size + 1u;
    remainderValue.limbs_.resize(left.limbs_.size() + 1u, 0u);
    quotientValue.limbs_.assign(count, 0u);
    const auto &divisor = normalizedDivisor.limbs_;
    auto &work = remainderValue.limbs_;
    for (std::size_t cursor = count; cursor-- > 0u;)
    {
        const std::uint64_t numerator = (static_cast<std::uint64_t>(work[cursor + size]) << 32u)
                                        | work[cursor + size - 1u];
        std::uint64_t estimate = numerator / divisor[size - 1u];
        std::uint64_t rest = numerator % divisor[size - 1u];
        while (estimate >= kLimbBase || (rest < kLimbBase && estimate * divisor[size - 2u] > (rest << 32u) + work[cursor + size - 2u]))
        {
            --estimate;
            rest += divisor[size - 1u];
            if (rest >= kLimbBase) break;
        }
        std::uint64_t borrow = 0u;
        for (std::size_t index = 0u; index < size; ++index)
        {
            const std::uint64_t product = estimate * divisor[index] + borrow;
            const std::uint32_t low = static_cast<std::uint32_t>(product);
            borrow = (product >> 32u) + (work[cursor + index] < low ? 1u : 0u);
            work[cursor + index] -= low;
        }
        const bool negative = work[cursor + size] < borrow;
        work[cursor + size] -= static_cast<std::uint32_t>(borrow);
        if (negative)
        {
            --estimate;
            std::uint64_t carry = 0u;
            for (std::size_t index = 0u; index < size; ++index)
            {
                const std::uint64_t sum = static_cast<std::uint64_t>(work[cursor + index])
                                          + divisor[index] + carry;
                work[cursor + index] = static_cast<std::uint32_t>(sum);
                carry = sum >> 32u;
            }
            work[cursor + size] += static_cast<std::uint32_t>(carry);
        }
        quotientValue.limbs_[cursor] = static_cast<std::uint32_t>(estimate);
    }
    quotientValue.sign_ = 1;
    quotientValue.normalize();
    work.resize(size);
    remainderValue.normalize();
    remainderValue.shiftRight(shift);
    if (quotient != nullptr) *quotient = std::move(quotientValue);
    if (remainder != nullptr) *remainder = std::move(remainderValue);
}

/**
 * @brief Burnikel-Ziegler 的 3n/2n 子问题。
 * @param dividend 除法运算的被除数。
 * @param divisor 除法运算的除数。
 * @param blockSize 每个递归计算块包含的分块数量。
 * @param quotient 接收整除运算得到的商。
 * @param remainder 接收整除运算得到的余数。
 */
void BigInt::divThreeByTwo(const BigInt &dividend, const BigInt &divisor,
                           std::size_t blockSize, BigInt *quotient, BigInt *remainder)
{
    const BigInt a3 = limbSlice(dividend, 0u, blockSize);
    const BigInt a2 = limbSlice(dividend, blockSize, blockSize);
    const BigInt a1 = limbSlice(dividend, blockSize * 2u, blockSize);
    const BigInt b2 = limbSlice(divisor, 0u, blockSize);
    const BigInt b1 = limbSlice(divisor, blockSize, blockSize);

    BigInt quotientValue;
    BigInt remainderHigh;
    if (compareMagnitude(a1, b1) < 0)
    {
        divTwoByOne(combineBlocks(a1, a2, blockSize), b1, blockSize,
                    &quotientValue, &remainderHigh);
    }
    else
    {
        quotientValue.limbs_.assign(blockSize, std::numeric_limits<std::uint32_t>::max());
        quotientValue.sign_ = 1;
        remainderHigh = combineBlocks(subMagnitude(a1, b1), a2, blockSize) + b1;
    }

    BigInt remainderValue = combineBlocks(remainderHigh, a3, blockSize) - mulMagnitude(quotientValue, b2);
    while (remainderValue.sign_ < 0)
    {
        remainderValue += divisor;
        quotientValue -= BigInt(1);
    }
    if (quotient != nullptr) *quotient = quotientValue;
    if (remainder != nullptr) *remainder = remainderValue;
}

/**
 * @brief Burnikel-Ziegler 的 2n/1n 子问题。
 * @param dividend 除法运算的被除数。
 * @param divisor 除法运算的除数。
 * @param blockSize 每个递归计算块包含的分块数量。
 * @param quotient 接收整除运算得到的商。
 * @param remainder 接收整除运算得到的余数。
 */
void BigInt::divTwoByOne(const BigInt &dividend, const BigInt &divisor,
                         std::size_t blockSize, BigInt *quotient, BigInt *remainder)
{
    if (compareMagnitude(dividend, divisor) < 0)
    {
        if (quotient != nullptr) *quotient = BigInt();
        if (remainder != nullptr) *remainder = dividend;
        return;
    }
    if (blockSize <= divisionThreshold() || (blockSize & 1u) != 0u)
    {
        divmodBase(dividend, divisor, quotient, remainder);
        return;
    }

    const std::size_t half = blockSize / 2u;
    const BigInt low = limbSlice(dividend, 0u, half);
    const BigInt upper = limbSlice(dividend, half, blockSize + half);
    BigInt quotientHigh;
    BigInt remainderHigh;
    divThreeByTwo(upper, divisor, half, &quotientHigh, &remainderHigh);
    BigInt quotientLow;
    BigInt remainderValue;
    divThreeByTwo(combineBlocks(remainderHigh, low, half), divisor, half,
                  &quotientLow, &remainderValue);
    if (quotient != nullptr) *quotient = combineBlocks(quotientHigh, quotientLow, half);
    if (remainder != nullptr) *remainder = remainderValue;
}

/**
 * @brief 幅值除取整（Burnikel-Ziegler 分治，小规模使用 Knuth 分块长除法）。
 * @param left 左操作数。
 * @param right 右操作数。
 * @param quotient 接收整除运算得到的商。
 * @param remainder 接收整除运算得到的余数。
 */
void BigInt::divmodMagnitude(const BigInt &left, const BigInt &right,
                             BigInt *quotient, BigInt *remainder)
{
    const BigInt dividend = left.abs();
    const BigInt divisor = right.abs();
    if (compareMagnitude(dividend, divisor) < 0)
    {
        if (quotient != nullptr) *quotient = BigInt();
        if (remainder != nullptr) *remainder = dividend;
        return;
    }
    if (divisor.limbs_.size() <= divisionThreshold() || dividend.limbs_.size() - divisor.limbs_.size() <= 1u)
    {
        divmodBase(dividend, divisor, quotient, remainder);
        return;
    }

    std::size_t blockSize = 1u;
    while (blockSize < divisor.limbs_.size()) blockSize <<= 1u;
    const std::size_t padding = blockSize - divisor.limbs_.size();
    std::size_t topShift = 0u;
    for (std::uint32_t top = divisor.limbs_.back(); (top & 0x80000000u) == 0u; top <<= 1u)
        ++topShift;
    const std::size_t normalizationBits = padding * 32u + topShift;
    BigInt normalizedDividend = dividend;
    BigInt normalizedDivisor = divisor;
    normalizedDividend.shiftLeft(normalizationBits);
    normalizedDivisor.shiftLeft(normalizationBits);

    BigInt quotientValue;
    BigInt remainderValue;
    const std::size_t blockCount =
        (normalizedDividend.limbs_.size() + blockSize - 1u) / blockSize;
    for (std::size_t block = blockCount; block-- > 0u;)
    {
        const BigInt chunk = limbSlice(normalizedDividend, block * blockSize, blockSize);
        const BigInt window = combineBlocks(remainderValue, chunk, blockSize);
        BigInt quotientBlock;
        divTwoByOne(window, normalizedDivisor, blockSize, &quotientBlock, &remainderValue);
        quotientValue = combineBlocks(quotientValue, quotientBlock, blockSize);
    }
    remainderValue.shiftRight(normalizationBits);
    quotientValue.normalize();
    remainderValue.normalize();
    if (!quotientValue.limbs_.empty()) quotientValue.sign_ = 1;
    if (!remainderValue.limbs_.empty()) remainderValue.sign_ = 1;
    if (quotient != nullptr) *quotient = quotientValue;
    if (remainder != nullptr) *remainder = remainderValue;
}

}  // namespace bigint

#endif
