/**
 * @file bigint_multiplication.cpp
 * @brief Karatsuba 与 NTT 乘法内核及算法阈值配置。
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

/* ---- NTT 内核（参考实现里"大规模乘法走 NTT"那部分的 C++ 版）----
 *
 * 两个 NTT 友好的素数：p = c × 2^k + 1，都有原根 3：
 *   p1 = 119 × 2^23 + 1 = 998244353      （支持长度到 2^23）
 *   p2 = 479 × 2^21 + 1 = 1004535809     （支持长度到 2^21）
 * 每个系数的真值 < p1 × p2 ≈ 2^60，而 15 位数字乘法的系数上界是
 * 15+15+log2(n) 位（n ≤ 2^20 时约 50 位），所以 CRT 重建不会溢出。
 */
constexpr std::uint32_t kNttPrime1 = 998244353u;
constexpr std::uint32_t kNttPrime2 = 1004535809u;
constexpr std::uint32_t kNttRoot = 3u;
constexpr std::size_t kDigitBits = 15u;  ///< NTT 用的"15 位数字"。
/* 按原生乘法宽度区分成本模型；实际分派还考虑补齐长度、长短比例和平方。
 * BIGINT_FORCE_PORTABLE_MULTIPLY 仅供回归验证可移植递归终点，日常构建不定义。 */
#if defined(__SIZEOF_INT128__) && !defined(BIGINT_FORCE_PORTABLE_MULTIPLY)
constexpr std::size_t kDefaultThreshold = 3072u;
constexpr std::uint64_t kKaratsubaCostWeight = 2u;
#else
constexpr std::size_t kDefaultThreshold = 768u;
constexpr std::uint64_t kKaratsubaCostWeight = 6u;
#endif
constexpr std::size_t kDefaultKaratsubaThreshold = 96u;
constexpr std::size_t kDefaultDivisionThreshold = 256u;

std::size_t g_multiplicationThreshold = kDefaultThreshold;
std::size_t g_karatsubaThreshold = kDefaultKaratsubaThreshold;
std::size_t g_divisionThreshold = kDefaultDivisionThreshold;

/**
 * @brief 结合实际补齐长度估计 Karatsuba 与 NTT 的成本，限制变换长度。
 * @param leftSize 左操作数的分块数。
 * @param rightSize 右操作数的分块数。
 * @param square 两个幅值是否相同，相同时 NTT 可以复用正变换。
 * @return 当前阈值及变换长度允许并且估计更快时返回 true。
 */
bool preferNtt(std::size_t leftSize, std::size_t rightSize, bool square)
{
    const std::size_t shorter = std::min(leftSize, rightSize);
    const std::size_t longer = std::max(leftSize, rightSize);
    if (shorter <= 1u) return false;
    const std::size_t digits = (leftSize * 32u + 14u) / 15u + (rightSize * 32u + 14u) / 15u;
    if (digits > (1u << 21u)) return false;
    if (g_multiplicationThreshold <= 1u) return true;
    if (leftSize + rightSize < g_multiplicationThreshold || shorter <= g_karatsubaThreshold) return false;
    std::size_t width = 1u;
    std::uint64_t karatsubaWork = 1u;
    while (width < shorter)
    {
        width <<= 1u;
        karatsubaWork *= 3u;
    }
    karatsubaWork *= (longer + width - 1u) / width;
    std::size_t transform = 1u;
    std::size_t levels = 0u;
    while (transform < digits)
    {
        transform <<= 1u;
        ++levels;
    }
    const std::uint64_t nttWork = static_cast<std::uint64_t>(transform) * levels;
    return karatsubaWork * kKaratsubaCostWeight > nttWork * (square ? 3u : 4u);
}

/**
 * @brief (base ^ exponent) mod Modulus（快速幂，uint32 版）。
 *
 * `Modulus` 走模板参数（编译期常量）：蝶形里的 `%` 于是能编成"乘高位 + 移位"
 * 的定长除法替代，而不是很慢的 64 位除法指令 —— 这是 NTT 快慢的关键。
 */
template <std::uint32_t Modulus>
std::uint32_t powModU32(std::uint32_t base, std::uint64_t exponent)
{
    constexpr std::uint64_t modulus = Modulus;
    std::uint64_t result = 1u;
    std::uint64_t value = base % modulus;
    while (exponent != 0u)
    {
        if ((exponent & 1u) != 0u) result = (result * value) % modulus;
        value = (value * value) % modulus;
        exponent >>= 1u;
    }
    return static_cast<std::uint32_t>(result);
}

/**
 * @brief 原地 NTT（invert = true 时做逆变换并乘上 n^{-1}）。
 * @param values 原地变换的系数数组，长度为当前素数支持的 2 的幂。
 * @param invert 为 true 时执行逆变换，否则执行正变换。
 */
template <std::uint32_t Modulus>
void ntt(Memory::Vector<std::uint32_t> &values, bool invert)
{
    constexpr std::uint64_t modulus = Modulus;
    const std::size_t count = values.size();
    Memory::Vector<std::uint32_t> weights(count / 2u, 1u);
    for (std::size_t index = 1u, reverse = 0u; index < count; ++index)
    {
        std::size_t bit = count >> 1u;
        for (; (reverse & bit) != 0u; bit >>= 1u) reverse ^= bit;
        reverse ^= bit;
        if (index < reverse) std::swap(values[index], values[reverse]);
    }
    for (std::size_t length = 2u; length <= count; length <<= 1u)
    {
        std::uint32_t root = powModU32<Modulus>(kNttRoot, (modulus - 1u) / length);
        if (invert) root = powModU32<Modulus>(root, modulus - 2u); /* 逆元 */
        const std::size_t half = length / 2u;
        for (std::size_t offset = 1u; offset < half; ++offset)
            weights[offset] = static_cast<std::uint32_t>((static_cast<std::uint64_t>(weights[offset - 1u]) * root) % modulus);
        for (std::size_t start = 0u; start < count; start += length)
        {
            for (std::size_t offset = 0u; offset < half; ++offset)
            {
                const std::uint32_t even = values[start + offset];
                const std::uint32_t odd = static_cast<std::uint32_t>(
                    (static_cast<std::uint64_t>(values[start + offset + half]) * weights[offset]) % modulus);
                /* 加减各是"两个 < p 的数"，用条件减代替取模（省掉除法） */
                std::uint32_t sum = even + odd;
                if (sum >= modulus) sum -= static_cast<std::uint32_t>(modulus);
                std::uint32_t difference = even + static_cast<std::uint32_t>(modulus) - odd;
                if (difference >= modulus) difference -= static_cast<std::uint32_t>(modulus);
                values[start + offset] = sum;
                values[start + offset + half] = difference;
            }
        }
    }
    if (invert)
    {
        const std::uint32_t inverse =
            powModU32<Modulus>(static_cast<std::uint32_t>(count), modulus - 2u);
        for (std::uint32_t &value : values)
        {
            value = static_cast<std::uint32_t>(
                (static_cast<std::uint64_t>(value) * inverse) % modulus);
        }
    }
}

/**
 * @brief 把幅值切成 15 位一段的数字（小端）。
 * @param limbs 高位不含零块的 32 位小端幅值。
 * @return 低位在前、每项不超过 15 位的系数数组。
 */
Memory::Vector<std::uint32_t> toBase15(const Memory::Vector<std::uint32_t> &limbs)
{
    Memory::Vector<std::uint32_t> digits;
    if (limbs.empty()) return digits;
    const std::size_t totalBits = (limbs.size() - 1u) * 32u;
    std::size_t topBits = 0u;
    for (std::uint32_t top = limbs.back(); top != 0u; top >>= 1u) ++topBits;
    const std::size_t bitCount = totalBits + topBits;
    const std::size_t digitCount = (bitCount + kDigitBits - 1u) / kDigitBits;
    digits.assign(digitCount, 0u);
    for (std::size_t index = 0u; index < digitCount; ++index)
    {
        const std::size_t bit = index * kDigitBits;
        const std::size_t limb = bit / 32u;
        const unsigned int offset = static_cast<unsigned int>(bit % 32u);
        std::uint64_t window = limbs[limb];
        if (limb + 1u < limbs.size())
            window |= static_cast<std::uint64_t>(limbs[limb + 1u]) << 32u;
        digits[index] = static_cast<std::uint32_t>((window >> offset) & 0x7fffu);
    }
    return digits;
}

/**
 * @brief 一次 NTT 卷积，模 `Modulus`（结果长度 = a.size() + b.size() - 1）。
 * @param left 非空的左侧 15 位系数数组。
 * @param right 非空的右侧 15 位系数数组；与左侧相等时复用正变换。
 * @return 按 Modulus 取模的有效卷积系数，不保留变换补齐区。
 */
template <std::uint32_t Modulus>
Memory::Vector<std::uint32_t> convolution(const Memory::Vector<std::uint32_t> &left,
                                          const Memory::Vector<std::uint32_t> &right)
{
    constexpr std::uint64_t modulus = Modulus;
    std::size_t count = 1u;
    const std::size_t used = left.size() + right.size() - 1u;
    while (count < used) count <<= 1u;
    const bool square = left == right;
    Memory::Vector<std::uint32_t> a(count, 0u);
    Memory::Vector<std::uint32_t> b(square ? 0u : count, 0u);
    /* 15 位数字一定 < 2^15 < 两个素数，所以只要夹一下就够了（不用除法） */
    for (std::size_t index = 0u; index < left.size(); ++index)
    {
        a[index] = (left[index] >= modulus) ? left[index] - static_cast<std::uint32_t>(modulus) : left[index];
    }
    for (std::size_t index = 0u; !square && index < right.size(); ++index)
    {
        b[index] = (right[index] >= modulus) ? right[index] - static_cast<std::uint32_t>(modulus) : right[index];
    }
    ntt<Modulus>(a, false);
    if (!square) ntt<Modulus>(b, false);
    for (std::size_t index = 0u; index < count; ++index)
    {
        a[index] = static_cast<std::uint32_t>(
            (static_cast<std::uint64_t>(a[index]) * (square ? a[index] : b[index])) % modulus);
    }
    ntt<Modulus>(a, true);
    a.resize(used);
    return a;
}

/**
 * @brief 把 15 位数字（小端）打包回 BigInt 的 32 位 limb。
 *
 * 用一个 64 位小缓冲按 15 位往里塞、够 32 位就吐出一个 limb —— 线性一趟，
 * 不做"每个数字左移 15i 再相加"那种 O(n²) 的笨办法。
 */
Memory::Vector<std::uint32_t> packBase15(const Memory::Vector<std::uint64_t> &digits)
{
    /* 真正的位长度：从最高那个非零的 15 位数字数出来 */
    std::size_t bitCount = 0u;
    for (std::size_t index = digits.size(); index-- > 0u;)
    {
        if (digits[index] == 0u) continue;
        bitCount = index * kDigitBits;
        for (std::uint64_t top = digits[index]; top != 0u; top >>= 1u) ++bitCount;
        break;
    }
    if (bitCount == 0u) return Memory::Vector<std::uint32_t>(); /* 0 */
    Memory::Vector<std::uint32_t> limbs((bitCount + 31u) / 32u, 0u);
    std::size_t limbIndex = 0u;
    std::uint64_t buffer = 0u;
    std::size_t buffered = 0u;
    for (const std::uint64_t digit : digits)
    {
        buffer |= (digit & 0x7FFFull) << buffered;
        buffered += kDigitBits;
        while (buffered >= 32u)
        {
            if (limbIndex < limbs.size()) limbs[limbIndex] = static_cast<std::uint32_t>(buffer);
            ++limbIndex;
            buffer >>= 32u;
            buffered -= 32u;
        }
    }
    if (buffered != 0u && limbIndex < limbs.size())
    {
        limbs[limbIndex] = static_cast<std::uint32_t>(buffer);
    }
    return limbs;
}

using Limbs = Memory::Vector<std::uint32_t>;

/**
 * @brief 移除大整数分块数组的高位零块。
 * @param limbs 大整数的低位在前分块数组或块数阈值。
 */
void trimLimbs(Limbs &limbs)
{
    while (!limbs.empty() && limbs.back() == 0u) limbs.pop_back();
}

/**
 * @brief 比较等宽分块数组并将绝对差写入独立缓冲区。
 * @param left 左侧分块数组。
 * @param right 右侧分块数组。
 * @param count 两侧数组的分块数。
 * @param difference 接收绝对差的 count 个分块。
 * @return 左值小于右值时返回 true，否则返回 false。
 */
bool subtractBlocks(const std::uint32_t *left, const std::uint32_t *right,
                    std::size_t count, std::uint32_t *difference)
{
    bool negative = false;
    for (std::size_t index = count; index-- > 0u;)
    {
        if (left[index] == right[index]) continue;
        negative = left[index] < right[index];
        break;
    }
    if (negative) std::swap(left, right);
    std::uint64_t borrow = 0u;
    for (std::size_t index = 0u; index < count; ++index)
    {
        const std::uint64_t subtrahend = static_cast<std::uint64_t>(right[index]) + borrow;
        difference[index] = static_cast<std::uint32_t>(left[index] - subtrahend);
        borrow = left[index] < subtrahend ? 1u : 0u;
    }
    return negative;
}

/**
 * @brief 在预分配缓冲区内执行等宽 Karatsuba，在原生整数宽度处终止递归。
 * @param left 左侧 count 个低位在前的分块。
 * @param right 右侧 count 个低位在前的分块。
 * @param count 分块数，必须是 2 的幂。
 * @param output 接收 2 * count 个乘积分块，不与输入重叠。
 * @param scratch 当前递归分支共用的工作区，至少 6 * count + 64 个分块。
 * @note 支持原生 128 位乘积时，两块输入直接执行一次 64 位原生乘法。
 */
void karatsubaBlocks(const std::uint32_t *left, const std::uint32_t *right,
                     std::size_t count, std::uint32_t *output, std::uint32_t *scratch)
{
#if defined(__SIZEOF_INT128__) && !defined(BIGINT_FORCE_PORTABLE_MULTIPLY)
    if (count == 4u)
    {
        __extension__ using DoubleWord = unsigned __int128;
        const std::uint64_t aLow = static_cast<std::uint64_t>(left[0]) | (static_cast<std::uint64_t>(left[1]) << 32u);
        const std::uint64_t aHigh = static_cast<std::uint64_t>(left[2]) | (static_cast<std::uint64_t>(left[3]) << 32u);
        const std::uint64_t bLow = static_cast<std::uint64_t>(right[0]) | (static_cast<std::uint64_t>(right[1]) << 32u);
        const std::uint64_t bHigh = static_cast<std::uint64_t>(right[2]) | (static_cast<std::uint64_t>(right[3]) << 32u);
        const DoubleWord low = static_cast<DoubleWord>(aLow) * bLow;
        const DoubleWord high = static_cast<DoubleWord>(aHigh) * bHigh;
        const DoubleWord difference = static_cast<DoubleWord>(aHigh < aLow ? aLow - aHigh : aHigh - aLow) * (bHigh < bLow ? bLow - bHigh : bHigh - bLow);
        DoubleWord middle = low + high;
        std::uint64_t top = middle < low ? 1u : 0u;
        const DoubleWord previous = middle;
        if ((aHigh < aLow) != (bHigh < bLow))
        {
            middle += difference;
            top += middle < previous ? 1u : 0u;
        }
        else
        {
            middle -= difference;
            top -= previous < difference ? 1u : 0u;
        }
        const DoubleWord second = (low >> 64u) + static_cast<std::uint64_t>(middle);
        const DoubleWord third = static_cast<std::uint64_t>(high) + (middle >> 64u) + (second >> 64u);
        const std::uint64_t words[] = { static_cast<std::uint64_t>(low), static_cast<std::uint64_t>(second),
                                        static_cast<std::uint64_t>(third), static_cast<std::uint64_t>((high >> 64u) + top + (third >> 64u)) };
        for (std::size_t index = 0u; index < 4u; ++index)
        {
            output[index * 2u] = static_cast<std::uint32_t>(words[index]);
            output[index * 2u + 1u] = static_cast<std::uint32_t>(words[index] >> 32u);
        }
        return;
    }
    if (count == 2u)
    {
        __extension__ using DoubleWord = unsigned __int128;
        const std::uint64_t a = static_cast<std::uint64_t>(left[0]) | (static_cast<std::uint64_t>(left[1]) << 32u);
        const std::uint64_t b = static_cast<std::uint64_t>(right[0]) | (static_cast<std::uint64_t>(right[1]) << 32u);
        const DoubleWord product = static_cast<DoubleWord>(a) * b;
        for (std::size_t index = 0u; index < 4u; ++index)
            output[index] = static_cast<std::uint32_t>(product >> (index * 32u));
        return;
    }
#endif
    if (count == 1u)
    {
        const std::uint64_t product = static_cast<std::uint64_t>(*left) * *right;
        output[0] = static_cast<std::uint32_t>(product);
        output[1] = static_cast<std::uint32_t>(product >> 32u);
        return;
    }
    const std::size_t half = count / 2u;
    karatsubaBlocks(left, right, half, output, scratch);
    karatsubaBlocks(left + half, right + half, half, output + count, scratch);
    auto *leftDifference = scratch;
    auto *rightDifference = scratch + half;
    auto *differenceProduct = scratch + count;
    auto *middle = scratch + count * 2u;
    auto *childScratch = middle + count + 1u;
    const bool leftNegative = subtractBlocks(left + half, left, half, leftDifference);
    const bool rightNegative = subtractBlocks(right + half, right, half, rightDifference);
    karatsubaBlocks(leftDifference, rightDifference, half, differenceProduct, childScratch);
    std::uint64_t carry = 0u;
    for (std::size_t index = 0u; index < count; ++index)
    {
        const std::uint64_t sum = static_cast<std::uint64_t>(output[index]) + output[count + index] + carry;
        middle[index] = static_cast<std::uint32_t>(sum);
        carry = sum >> 32u;
    }
    middle[count] = static_cast<std::uint32_t>(carry);
    carry = 0u;
    for (std::size_t index = 0u; index < count; ++index)
    {
        if (leftNegative != rightNegative)
        {
            const std::uint64_t sum = static_cast<std::uint64_t>(middle[index]) + differenceProduct[index] + carry;
            middle[index] = static_cast<std::uint32_t>(sum);
            carry = sum >> 32u;
        }
        else
        {
            const std::uint64_t subtrahend = static_cast<std::uint64_t>(differenceProduct[index]) + carry;
            carry = middle[index] < subtrahend ? 1u : 0u;
            middle[index] -= static_cast<std::uint32_t>(subtrahend);
        }
    }
    if (leftNegative != rightNegative)
        middle[count] += static_cast<std::uint32_t>(carry);
    else
        middle[count] -= static_cast<std::uint32_t>(carry);
    carry = 0u;
    for (std::size_t index = 0u; index <= count; ++index)
    {
        const std::uint64_t sum = static_cast<std::uint64_t>(output[half + index]) + middle[index] + carry;
        output[half + index] = static_cast<std::uint32_t>(sum);
        carry = sum >> 32u;
    }
    for (std::size_t index = half + count + 1u; carry != 0u && index < count * 2u; ++index)
    {
        const std::uint64_t sum = static_cast<std::uint64_t>(output[index]) + carry;
        output[index] = static_cast<std::uint32_t>(sum);
        carry = sum >> 32u;
    }
}

/**
 * @brief 分配一次工作区并使用 Karatsuba 计算幅值乘积。
 * @param left 左操作数的分块。
 * @param right 右操作数的分块。
 * @return 去掉高位零块的乘积。
 */
Limbs karatsubaMultiply(const Limbs &left, const Limbs &right)
{
    if (left.empty() || right.empty()) return Limbs();
    if (std::min(left.size(), right.size()) == 1u)
    {
        const Limbs &wide = left.size() > right.size() ? left : right;
        const std::uint32_t scalar = left.size() > right.size() ? right[0] : left[0];
        Limbs output(wide.size() + 1u, 0u);
        std::uint64_t carry = 0u;
        for (std::size_t index = 0u; index < wide.size(); ++index)
        {
            const std::uint64_t product = static_cast<std::uint64_t>(wide[index]) * scalar + carry;
            output[index] = static_cast<std::uint32_t>(product);
            carry = product >> 32u;
        }
        output.back() = static_cast<std::uint32_t>(carry);
        trimLimbs(output);
        return output;
    }
    const Limbs &narrow = left.size() < right.size() ? left : right;
    const Limbs &wide = left.size() < right.size() ? right : left;
    if (wide.size() <= 4u)
    {
        std::uint32_t a[4] = {}, b[4] = {}, scratch[88] = {};
        std::copy(left.begin(), left.end(), a);
        std::copy(right.begin(), right.end(), b);
        const std::size_t width = wide.size() <= 2u ? 2u : 4u;
        Limbs output(width * 2u, 0u);
        karatsubaBlocks(a, b, width, output.data(), scratch);
        trimLimbs(output);
        return output;
    }
    std::size_t width = 1u;
    while (width < narrow.size()) width <<= 1u;
    Limbs paddedLeft(width, 0u), paddedRight(width, 0u);
    std::copy(narrow.begin(), narrow.end(), paddedLeft.begin());
    Limbs product(width * 2u, 0u);
    Limbs output(left.size() + right.size() + 1u, 0u);
    Limbs scratch(width * 6u + 64u, 0u);
    for (std::size_t offset = 0u; offset < wide.size(); offset += width)
    {
        std::fill(paddedRight.begin(), paddedRight.end(), 0u);
        const std::size_t count = std::min(width, wide.size() - offset);
        std::copy_n(wide.begin() + static_cast<std::ptrdiff_t>(offset), count, paddedRight.begin());
        karatsubaBlocks(paddedLeft.data(), paddedRight.data(), width, product.data(), scratch.data());
        std::uint64_t carry = 0u;
        std::size_t index = 0u;
        const std::size_t used = narrow.size() + count;
        for (; index < used; ++index)
        {
            const std::uint64_t sum = static_cast<std::uint64_t>(output[offset + index]) + product[index] + carry;
            output[offset + index] = static_cast<std::uint32_t>(sum);
            carry = sum >> 32u;
        }
        while (carry != 0u)
        {
            const std::uint64_t sum = static_cast<std::uint64_t>(output[offset + index]) + carry;
            output[offset + index++] = static_cast<std::uint32_t>(sum);
            carry = sum >> 32u;
        }
    }
    trimLimbs(output);
    return output;
}

}  // namespace

/**
 * @brief 按补齐成本选择 Karatsuba 或 NTT 计算幅值乘积。
 * @param left 左操作数。
 * @param right 右操作数。
 * @retval 计算得到的大数值；复合赋值运算返回当前对象。
 */
BigInt BigInt::mulMagnitude(const BigInt &left, const BigInt &right)
{
    BigInt out;
    if (left.limbs_.empty() || right.limbs_.empty()) return out; /* 0 */
    /* 规模够大就换 NTT（O(n log n)）：15 位数字 → 双素数卷积 → CRT → 打包回 limb。 */
    const bool square = left.limbs_ == right.limbs_;
    if (preferNtt(left.limbs_.size(), right.limbs_.size(), square))
    {
        const Memory::Vector<std::uint32_t> a = toBase15(left.limbs_);
        const Memory::Vector<std::uint32_t> b = toBase15(right.limbs_);
        const Memory::Vector<std::uint32_t> residue1 = convolution<kNttPrime1>(a, b);
        const Memory::Vector<std::uint32_t> residue2 = convolution<kNttPrime2>(a, b);
        const std::uint64_t prime1 = kNttPrime1;
        const std::uint64_t prime2 = kNttPrime2;
        /* CRT：x ≡ r1 (mod p1)、x ≡ r2 (mod p2) → x = r1 + p1 × t，
         * t = (r2 − r1) × p1^{-1} (mod p2)。 */
        const std::uint64_t inverse =
            powModU32<kNttPrime2>(static_cast<std::uint32_t>(prime1 % prime2), prime2 - 2u);
        const std::size_t count = std::max(residue1.size(), residue2.size());
        Memory::Vector<std::uint64_t> coefficients(count, 0u);
        std::uint64_t carry = 0u;
        for (std::size_t index = 0u; index < count; ++index)
        {
            const std::uint64_t value1 = (index < residue1.size()) ? residue1[index] : 0u;
            const std::uint64_t value2 = (index < residue2.size()) ? residue2[index] : 0u;
            const std::uint64_t difference = (value2 + prime2 - (value1 % prime2)) % prime2;
            const std::uint64_t factor = (difference * inverse) % prime2;
            const std::uint64_t exact = value1 + prime1 * factor; /* < p1 × p2 ≈ 2^60 */
            const std::uint64_t current = exact + carry;
            coefficients[index] = current & 0x7FFFull; /* 低 15 位 */
            carry = current >> kDigitBits;
        }
        while (carry != 0u)
        {
            coefficients.push_back(carry & 0x7FFFull);
            carry >>= kDigitBits;
        }
        out.limbs_ = packBase15(coefficients);
        out.normalize();
        if (!out.limbs_.empty()) out.sign_ = 1;
        return out;
    }
    out.limbs_ = karatsubaMultiply(left.limbs_, right.limbs_);
    out.normalize();
    if (!out.limbs_.empty()) out.sign_ = 1;
    return out;
}

/**
 * @brief NTT 候选下限：两边 limb 数之和达到它后再比较补齐成本。
 *
 * 支持原生 128 位乘积时默认 3072，否则为 768；`1` 强制可支持的多块输入走 NTT。
 * @param limbs 大整数的低位在前分块数组或块数阈值。
 */
void BigInt::setMultiplicationThreshold(std::size_t limbs) { g_multiplicationThreshold = limbs; }
/**
 * @brief 当前 NTT 阈值。
 * @retval 切换 NTT 乘法的分块数量阈值。
 */
std::size_t BigInt::multiplicationThreshold() { return g_multiplicationThreshold; }
/**
 * @brief Karatsuba 保护阈值：较短操作数不超过该 limb 数时保留 Karatsuba。
 *
 * 默认 96；不再控制学校算法基线，0 按 1 处理；强制 NTT 模式优先。
 * @param limbs 大整数的低位在前分块数组或块数阈值。
 */
void BigInt::setKaratsubaThreshold(std::size_t limbs)
{
    g_karatsubaThreshold = std::max<std::size_t>(1u, limbs);
}
/**
 * @brief 当前 Karatsuba 保护阈值。
 * @return 自动分派保留 Karatsuba 的短侧块数上限。
 */
std::size_t BigInt::karatsubaThreshold() { return g_karatsubaThreshold; }
/**
 * @brief Burnikel-Ziegler 除法的递归基线；除数不超过该 limb 数时使用长除法。
 *
 * 默认 256、最小 32；设置为很大可强制使用长除法。
 * @param limbs 大整数的低位在前分块数组或块数阈值。
 */
void BigInt::setDivisionThreshold(std::size_t limbs)
{
    g_divisionThreshold = std::max<std::size_t>(32u, limbs);
}
/**
 * @brief 当前 Burnikel-Ziegler 递归基线。
 * @retval 切换递归除法的分块数量阈值。
 */
std::size_t BigInt::divisionThreshold() { return g_divisionThreshold; }

}  // namespace bigint
#endif
