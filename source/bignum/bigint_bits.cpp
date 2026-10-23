/**
 * @file bigint_bits.cpp
 * @brief 非负整数的位运算与位修改。
 */

#if !defined(NO_CHALI_BIG_INT)
#include "bigint.hpp"
#include <algorithm>
#include <limits>
#include <stdexcept>

namespace bigint
{
// ---------------------------------------------------------------------------
//  位运算 / 位操作 / 判断
// ---------------------------------------------------------------------------

/**
 * @brief 按位与（两个操作数都非负；负数抛 `std::domain_error`）。
 * @param left 左操作数。
 * @param right 右操作数。
 * @retval 计算得到的大数值；复合赋值运算返回当前对象。
 */
BigInt operator&(const BigInt &left, const BigInt &right)
{
    if (left.sign_ < 0 || right.sign_ < 0) throw std::domain_error("bigint: 位运算只支持非负整数");
    BigInt out;
    const std::size_t count = std::min(left.limbs_.size(), right.limbs_.size());
    out.limbs_.assign(count, 0u);
    for (std::size_t index = 0u; index < count; ++index)
    {
        out.limbs_[index] = left.limbs_[index] & right.limbs_[index];
    }
    out.normalize();
    if (!out.limbs_.empty()) out.sign_ = 1;
    return out;
}

/**
 * @brief 按位或（两个操作数都非负；负数抛 `std::domain_error`）。
 * @param left 左操作数。
 * @param right 右操作数。
 * @retval 计算得到的大数值；复合赋值运算返回当前对象。
 */
BigInt operator|(const BigInt &left, const BigInt &right)
{
    if (left.sign_ < 0 || right.sign_ < 0) throw std::domain_error("bigint: 位运算只支持非负整数");
    BigInt out;
    const std::size_t count = std::max(left.limbs_.size(), right.limbs_.size());
    out.limbs_.assign(count, 0u);
    for (std::size_t index = 0u; index < count; ++index)
    {
        const std::uint32_t a = (index < left.limbs_.size()) ? left.limbs_[index] : 0u;
        const std::uint32_t b = (index < right.limbs_.size()) ? right.limbs_[index] : 0u;
        out.limbs_[index] = a | b;
    }
    out.normalize();
    if (!out.limbs_.empty()) out.sign_ = 1;
    return out;
}

/**
 * @brief 按位异或（两个操作数都非负；负数抛 `std::domain_error`）。
 * @param left 左操作数。
 * @param right 右操作数。
 * @retval 计算得到的大数值；复合赋值运算返回当前对象。
 */
BigInt operator^(const BigInt &left, const BigInt &right)
{
    if (left.sign_ < 0 || right.sign_ < 0) throw std::domain_error("bigint: 位运算只支持非负整数");
    BigInt out;
    const std::size_t count = std::max(left.limbs_.size(), right.limbs_.size());
    out.limbs_.assign(count, 0u);
    for (std::size_t index = 0u; index < count; ++index)
    {
        const std::uint32_t a = (index < left.limbs_.size()) ? left.limbs_[index] : 0u;
        const std::uint32_t b = (index < right.limbs_.size()) ? right.limbs_[index] : 0u;
        out.limbs_[index] = a ^ b;
    }
    out.normalize();
    if (!out.limbs_.empty()) out.sign_ = 1;
    return out;
}

/**
 * @brief 按位取反：补口语义 `~x = -x - 1`（唯一允许负数结果的位运算）。
 * @retval 计算得到的大数值；复合赋值运算返回当前对象。
 */
BigInt BigInt::operator~() const { return -*this - BigInt(1); }

/**
 * @brief 把第 bit 位清 0。
 * @param bit 待写入或查询的二进制位。
 */
void BigInt::clearBit(std::size_t bit)
{
    const std::size_t index = bit / 32u;
    if (index >= limbs_.size()) return;
    limbs_[index] &= ~(1u << (bit % 32u));
    normalize();
}

/**
 * @brief 把第 bit 位取反。
 * @param bit 待写入或查询的二进制位。
 */
void BigInt::flipBit(std::size_t bit)
{
    const std::size_t index = bit / 32u;
    if (index >= limbs_.size())
    {
        setBit(bit); /* 超出位宽的翻转就是置 1 */
        return;
    }
    limbs_[index] ^= (1u << (bit % 32u));
    normalize();
    if (!limbs_.empty() && sign_ == 0) sign_ = 1;
}

}  // namespace bigint

#endif
