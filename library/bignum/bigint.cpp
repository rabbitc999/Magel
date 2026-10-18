#if !defined(NO_MAGEL_BIG_INT)
/**
 * @file bigint.cpp
 * @brief `BigInt` 的实现：基 2^32 的 limb 数组 + 三级乘法内核 + Burnikel-Ziegler 除法。
 *
 * 参考 `eigenmath-plus/eigenmath_bigint.c` 的整数部分：同样是"符号 + 小端 limb +
 * 有效长度 + 每次运算后规范化"，只是把手工 `malloc` 换成 `std::vector`。
 * 乘法按规模在学校算法、Karatsuba 与 NTT 之间切换（阈值与实测数字见 README）。
 */

#include "bigint.hpp"

#include <algorithm>
#include <limits>
#include <stdexcept>

namespace bigint
{
namespace
{
constexpr std::uint64_t kLimbBase = 1ull << 32;       ///< 一个 limb 的基数。
constexpr std::uint32_t kDecimalChunk = 1000000000u;  ///< 十进制串一段 9 位。

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
/* 三级乘法实测阈值（MinGW g++ 11.2 -O2）：学校算法 / Karatsuba 约 96 limb，
 * Karatsuba / NTT 的首个平衡交叉区约为两边合计 48,000 limb。详见 README。 */
constexpr std::size_t kDefaultThreshold = 48000u;
constexpr std::size_t kDefaultKaratsubaThreshold = 96u;
constexpr std::size_t kDefaultDivisionThreshold = 56u;

std::size_t g_multiplicationThreshold = kDefaultThreshold;
std::size_t g_karatsubaThreshold = kDefaultKaratsubaThreshold;
std::size_t g_divisionThreshold = kDefaultDivisionThreshold;

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
 */
template <std::uint32_t Modulus>
void ntt(Memory::Vector<std::uint32_t> &values, bool invert)
{
    constexpr std::uint64_t modulus = Modulus;
    const std::size_t count = values.size();
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
        for (std::size_t start = 0u; start < count; start += length)
        {
            std::uint64_t weight = 1u;
            const std::size_t half = length / 2u;
            for (std::size_t offset = 0u; offset < half; ++offset)
            {
                const std::uint32_t even = values[start + offset];
                const std::uint32_t odd = static_cast<std::uint32_t>(
                    (values[start + offset + half] * weight) % modulus);
                /* 加减各是"两个 < p 的数"，用条件减代替取模（省掉除法） */
                std::uint32_t sum = even + odd;
                if (sum >= modulus) sum -= static_cast<std::uint32_t>(modulus);
                std::uint32_t difference = even + static_cast<std::uint32_t>(modulus) - odd;
                if (difference >= modulus) difference -= static_cast<std::uint32_t>(modulus);
                values[start + offset] = sum;
                values[start + offset + half] = difference;
                weight = (weight * root) % modulus;
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
    for (std::size_t bit = 0u; bit < bitCount; ++bit)
    {
        if (((limbs[bit / 32u] >> (bit % 32u)) & 1u) == 0u) continue;
        digits[bit / kDigitBits] |= (1u << (bit % kDigitBits));
    }
    return digits;
}

/**
 * @brief 一次 NTT 卷积，模 `Modulus`（结果长度 = a.size() + b.size() - 1）。
 */
template <std::uint32_t Modulus>
Memory::Vector<std::uint32_t> convolution(const Memory::Vector<std::uint32_t> &left,
                                          const Memory::Vector<std::uint32_t> &right)
{
    constexpr std::uint64_t modulus = Modulus;
    std::size_t count = 1u;
    while (count < left.size() + right.size()) count <<= 1u;
    Memory::Vector<std::uint32_t> a(count, 0u);
    Memory::Vector<std::uint32_t> b(count, 0u);
    /* 15 位数字一定 < 2^15 < 两个素数，所以只要夹一下就够了（不用除法） */
    for (std::size_t index = 0u; index < left.size(); ++index)
    {
        a[index] = (left[index] >= modulus) ? left[index] - static_cast<std::uint32_t>(modulus) : left[index];
    }
    for (std::size_t index = 0u; index < right.size(); ++index)
    {
        b[index] = (right[index] >= modulus) ? right[index] - static_cast<std::uint32_t>(modulus) : right[index];
    }
    ntt<Modulus>(a, false);
    ntt<Modulus>(b, false);
    for (std::size_t index = 0u; index < count; ++index)
    {
        a[index] = static_cast<std::uint32_t>(
            (static_cast<std::uint64_t>(a[index]) * b[index]) % modulus);
    }
    ntt<Modulus>(a, true);
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
 * @brief 计算两个无符号分块数组的和。
 * @param left 左操作数。
 * @param right 右操作数。
 * @retval 按处理顺序保存的结果元素集合。
 */
Limbs addLimbs(const Limbs &left, const Limbs &right)
{
    Limbs out(std::max(left.size(), right.size()) + 1u, 0u);
    std::uint64_t carry = 0u;
    for (std::size_t index = 0u; index + 1u < out.size(); ++index)
    {
        const std::uint64_t a = (index < left.size()) ? left[index] : 0u;
        const std::uint64_t b = (index < right.size()) ? right[index] : 0u;
        const std::uint64_t value = a + b + carry;
        out[index] = static_cast<std::uint32_t>(value);
        carry = value >> 32u;
    }
    out.back() = static_cast<std::uint32_t>(carry);
    trimLimbs(out);
    return out;
}

/**
 * @brief limb 幅值相减；调用方保证 `left >= right`。
 */
Limbs subLimbs(const Limbs &left, const Limbs &right)
{
    Limbs out(left.size(), 0u);
    std::uint64_t borrow = 0u;
    for (std::size_t index = 0u; index < left.size(); ++index)
    {
        const std::uint64_t a = left[index];
        const std::uint64_t b = (index < right.size()) ? right[index] : 0u;
        const std::uint64_t subtrahend = b + borrow;
        if (a < subtrahend)
        {
            out[index] = static_cast<std::uint32_t>(kLimbBase + a - subtrahend);
            borrow = 1u;
        }
        else
        {
            out[index] = static_cast<std::uint32_t>(a - subtrahend);
            borrow = 0u;
        }
    }
    trimLimbs(out);
    return out;
}

/**
 * @brief 使用逐项相乘算法计算分块数组的乘积。
 * @param left 左操作数。
 * @param right 右操作数。
 * @retval 按处理顺序保存的结果元素集合。
 */
Limbs schoolbookMultiply(const Limbs &left, const Limbs &right)
{
    if (left.empty() || right.empty()) return Limbs();
    Limbs out(left.size() + right.size() + 1u, 0u);
    for (std::size_t index = 0u; index < left.size(); ++index)
    {
        std::uint64_t carry = 0u;
        const std::uint64_t a = left[index];
        for (std::size_t inner = 0u; inner < right.size(); ++inner)
        {
            const std::uint64_t value = a * right[inner] + out[index + inner] + carry;
            out[index + inner] = static_cast<std::uint32_t>(value);
            carry = value >> 32u;
        }
        std::size_t cursor = index + right.size();
        while (carry != 0u)
        {
            const std::uint64_t value = out[cursor] + carry;
            out[cursor] = static_cast<std::uint32_t>(value);
            carry = value >> 32u;
            ++cursor;
        }
    }
    trimLimbs(out);
    return out;
}

/**
 * @brief 将分块数组按指定块偏移累加到目标中。
 * @param target 待访问或修改的目标。
 * @param value 待转换、保存或参与计算的值。
 * @param shift 分块数组或数值的移位量。
 */
void addShifted(Limbs &target, const Limbs &value, std::size_t shift)
{
    if (value.empty()) return;
    if (target.size() < shift + value.size() + 1u) target.resize(shift + value.size() + 1u, 0u);
    std::uint64_t carry = 0u;
    std::size_t index = 0u;
    for (; index < value.size(); ++index)
    {
        const std::uint64_t sum = target[shift + index] + static_cast<std::uint64_t>(value[index]) + carry;
        target[shift + index] = static_cast<std::uint32_t>(sum);
        carry = sum >> 32u;
    }
    std::size_t cursor = shift + index;
    while (carry != 0u)
    {
        const std::uint64_t sum = target[cursor] + carry;
        target[cursor] = static_cast<std::uint32_t>(sum);
        carry = sum >> 32u;
        ++cursor;
        if (cursor == target.size() && carry != 0u) target.push_back(0u);
    }
}

/**
 * @brief 使用 Karatsuba 分治算法计算分块数组的乘积。
 * @param left 左操作数。
 * @param right 右操作数。
 * @param threshold 切换计算算法的规模阈值。
 * @retval 按处理顺序保存的结果元素集合。
 */
Limbs karatsubaMultiply(const Limbs &left, const Limbs &right, std::size_t threshold)
{
    if (left.empty() || right.empty()) return Limbs();
    if (std::min(left.size(), right.size()) <= threshold)
        return schoolbookMultiply(left, right);

    const std::size_t half = std::max(left.size(), right.size()) / 2u;
    const auto split = [half](const Limbs &value, bool high) {
        const std::size_t begin = high ? std::min(half, value.size()) : 0u;
        const std::size_t end = high ? value.size() : std::min(half, value.size());
        return Limbs(value.begin() + static_cast<std::ptrdiff_t>(begin),
                     value.begin() + static_cast<std::ptrdiff_t>(end));
    };
    const Limbs leftLow = split(left, false);
    const Limbs leftHigh = split(left, true);
    const Limbs rightLow = split(right, false);
    const Limbs rightHigh = split(right, true);

    const Limbs lowProduct = karatsubaMultiply(leftLow, rightLow, threshold);
    const Limbs highProduct = karatsubaMultiply(leftHigh, rightHigh, threshold);
    Limbs middleProduct = karatsubaMultiply(addLimbs(leftLow, leftHigh),
                                            addLimbs(rightLow, rightHigh), threshold);
    middleProduct = subLimbs(middleProduct, lowProduct);
    middleProduct = subLimbs(middleProduct, highProduct);

    Limbs out(left.size() + right.size() + 1u, 0u);
    addShifted(out, lowProduct, 0u);
    addShifted(out, middleProduct, half);
    addShifted(out, highProduct, half * 2u);
    trimLimbs(out);
    return out;
}

/**
 * @brief 十进制字符是不是数字。
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
 * @brief 幅值乘法（学校算法，O(n×m)）。
 * @param left 左操作数。
 * @param right 右操作数。
 * @retval 计算得到的大数值；复合赋值运算返回当前对象。
 */
BigInt BigInt::mulMagnitude(const BigInt &left, const BigInt &right)
{
    BigInt out;
    if (left.limbs_.empty() || right.limbs_.empty()) return out; /* 0 */
    /* 规模够大就换 NTT（O(n log n)）：15 位数字 → 双素数卷积 → CRT → 打包回 limb。 */
    if (left.limbs_.size() + right.limbs_.size() >= g_multiplicationThreshold)
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
    if (std::min(left.limbs_.size(), right.limbs_.size()) > g_karatsubaThreshold)
        out.limbs_ = karatsubaMultiply(left.limbs_, right.limbs_, g_karatsubaThreshold);
    else
        out.limbs_ = schoolbookMultiply(left.limbs_, right.limbs_);
    out.normalize();
    if (!out.limbs_.empty()) out.sign_ = 1;
    return out;
}

/**
 * @brief NTT 切换阈值：两边 limb 个数之和达到它就改用 NTT。
 *
 * 默认 48000；测试和调参可以直接改（`1` = 永远走 NTT）。
 * @param limbs 大整数的低位在前分块数组或块数阈值。
 */
void BigInt::setMultiplicationThreshold(std::size_t limbs) { g_multiplicationThreshold = limbs; }
/**
 * @brief 当前 NTT 阈值。
 * @retval 切换 NTT 乘法的分块数量阈值。
 */
std::size_t BigInt::multiplicationThreshold() { return g_multiplicationThreshold; }
/**
 * @brief Karatsuba 基线阈值：较短操作数超过该 limb 数时使用 Karatsuba。
 *
 * 默认 96；设置为很大可强制使用学校算法，0 会按最小安全值 1 处理。
 * @param limbs 大整数的低位在前分块数组或块数阈值。
 */
void BigInt::setKaratsubaThreshold(std::size_t limbs)
{
    g_karatsubaThreshold = std::max<std::size_t>(1u, limbs);
}
/**
 * @brief 当前 Karatsuba 基线阈值。
 * @retval 切换 Karatsuba 乘法的分块数量阈值。
 */
std::size_t BigInt::karatsubaThreshold() { return g_karatsubaThreshold; }
/**
 * @brief Burnikel-Ziegler 除法的递归基线；除数不超过该 limb 数时使用长除法。
 *
 * 默认 56、最小 32；设置为很大可强制使用长除法。
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
 * @brief 长除法递归基线。
 * @param left 左操作数。
 * @param right 右操作数。
 * @param quotient 接收整除运算得到的商。
 * @param remainder 接收整除运算得到的余数。
 */
void BigInt::divmodBase(const BigInt &left, const BigInt &right,
                        BigInt *quotient, BigInt *remainder)
{
    BigInt quotientValue; /* 0 */
    BigInt remainderValue = left.abs();
    const BigInt divisor = right.abs();
    if (compareMagnitude(remainderValue, divisor) >= 0)
    {
        const std::size_t shift = remainderValue.bitLength() - divisor.bitLength();
        BigInt shifted = divisor;
        shifted.shiftLeft(shift);
        for (std::size_t step = shift + 1u; step-- > 0u;)
        {
            if (compareMagnitude(remainderValue, shifted) >= 0)
            {
                remainderValue = subMagnitude(remainderValue, shifted);
                quotientValue.setBit(step);
            }
            shifted.shiftRight(1u);
        }
        quotientValue.normalize();
        if (!quotientValue.limbs_.empty()) quotientValue.sign_ = 1;
    }
    if (quotient != nullptr) *quotient = quotientValue;
    if (remainder != nullptr) *remainder = remainderValue;
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
    if (blockSize <= g_divisionThreshold || (blockSize & 1u) != 0u)
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
 * @brief 幅值除取整（默认 Burnikel-Ziegler，小规模回退长除法）。
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
    if (divisor.limbs_.size() <= g_divisionThreshold)
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
ExtendedGcd BigInt::extendedGcd(const BigInt &a, const BigInt &b){
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
    static const std::uint32_t kSmallPrimes[] = {2u, 3u, 5u, 7u, 11u, 13u, 17u, 19u, 23u, 29u, 31u, 37u};
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
        if (ch >= '0' && ch <= '9') digit = static_cast<unsigned int>(ch - '0');
        else if (ch >= 'a' && ch <= 'z') digit = static_cast<unsigned int>(ch - 'a') + 10u;
        else if (ch >= 'A' && ch <= 'Z') digit = static_cast<unsigned int>(ch - 'A') + 10u;
        else digit = base; /* 非法字符 */
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
